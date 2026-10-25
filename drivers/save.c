//#############################################################################
// FILE:   drivers/save.c
// TITLE:  Save driver implementation. See save.h for the big picture.
//
// REQUIREMENTS
//   * TI Flash API for F2806x (Flash2806x_API_Library.h + the .lib) on the
//     include path and linked (you already have this).
//   * Flash sectors G/H must stay out of the linker SECTIONS (they are).
//
// WHY THIS FILE CHANGED (Release/flash-boot bugs)
//   In a Release build ALL code runs from flash. While the flash is erasing
//   or programming, the CPU cannot fetch instructions from it, so every
//   instruction that executes during the operation must live in RAM or ROM.
//   The old code ran FlashOp() and the watchdog callback from flash. That
//   works in the Debug (all-RAM) build, and in Release it "sometimes works",
//   which looks like random lost saves. Now FlashProgramOp() and
//   FlashCallback() are placed in ".TI.ramfunc" (copied to RAM by the
//   memcpy at the top of main()) and the Flash API itself is in boot ROM.
//
// WEAR LEVELING  (log-structured, append-only)
//   Each 16K-word sector is a LOG of save records. A save APPENDS one record
//   (72 words) to the first blank slot. A sector is erased only when it is
//   completely full (~227 saves), and then the OTHER sector takes over.
//   Result: ~227x fewer erases than "erase on every save" and ~100x fewer
//   than the old ping-pong. Flash is rated ~50,000 erase cycles per sector,
//   so that is roughly 20 MILLION saves instead of ~100 thousand.
//   Records are never modified after being programmed (each word is
//   programmed once between erases), so it is also power-fail safe: a torn
//   record just fails its CRC and the previous record is used.
//
// RECORD LAYOUT (16-bit words, 8-word aligned; same header as before, so old
//   saves are still readable)
//   [0] magic  [1] layout version  [2..3] sequence (newer = bigger)
//   [4..5] slot-present mask  [6] payload words  [7] CRC16 of [0..6]+payload
//   [8..]  payload (all slots back to back), padded to a multiple of 8 words
//#############################################################################

#include "save.h"
#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "Flash2806x_API_Library.h"
#include "buzzer.h"                     // Buzzer_Stop(), volume
#include "backlight.h"                  // brightness

//*****************************************************************************
// Config
//*****************************************************************************
#define SECTOR_A_ADDR       0x3D8000UL  // flash sector H
#define SECTOR_B_ADDR       0x3DC000UL  // flash sector G
#define SECTOR_A_MASK       SECTORH
#define SECTOR_B_MASK       SECTORG
#define SECTOR_WORDS        0x4000U     // 16K words per sector

#define SAVE_MAGIC          0xA55AU
#define SAVE_LAYOUT_VERSION 1U          // bump ONLY if the header format changes
#define SAVE_PAYLOAD_MAX    4096U       // sanity limit when reading flash

#define H_MAGIC             0
#define H_VERSION           1
#define H_SEQ_LO            2
#define H_SEQ_HI            3
#define H_MASK_LO           4
#define H_MASK_HI           5
#define H_PAYLOAD           6
#define H_CRC               7
#define HDR_WORDS           8

// Total payload words = sum of every slot size. Computed inside an enum so
// X is expanded RIGHT NOW (a macro would expand later, after #undef X).
#define X(name, words)      + (words)
enum { SAVE_PAYLOAD_WORDS = (0 SAVE_SLOTS(X)) };
#undef X

enum { IMG_WORDS = ((HDR_WORDS + SAVE_PAYLOAD_WORDS + 7) & ~7) };

// Where a sector lives. (Host unit tests swap this for a RAM array.)
#ifdef SAVE_HOST_TEST
extern volatile uint16_t *Save_TestFlashBase(int16_t which);
#define FLASH_BASE(w)       Save_TestFlashBase(w)
#else
#define FLASH_BASE(w)       ((volatile uint16_t *)((w) ? SECTOR_B_ADDR : SECTOR_A_ADDR))
#endif
#define SECTOR_MASK(w)      ((w) ? SECTOR_B_MASK : SECTOR_A_MASK)

//*****************************************************************************
// State (all RAM)
//*****************************************************************************
static const uint16_t kSlotWords[SAVE_SLOT_COUNT] =
{
#define X(name, words) (words),
    SAVE_SLOTS(X)
#undef X
};

static uint16_t s_img[IMG_WORDS];       // header + payload, exactly what goes to flash
static uint32_t s_mask;                 // bit n = slot n has been written
static uint32_t s_seq;                  // sequence of the newest record in flash
static int16_t  s_active = -1;          // sector holding the newest record: -1 none, 0 = A, 1 = B
static uint16_t s_next;                 // word offset of the next blank slot in s_active
static bool     s_dirty;
static uint16_t s_err;                  // SAVE_ERR_*
static uint16_t s_apiStatus;            // raw Flash API status of the last failure

#define PAYLOAD             (s_img + HDR_WORDS)

//*****************************************************************************
// Helpers
//*****************************************************************************

// Bitwise CRC (no lookup table on purpose: tables are const data in flash,
// and nothing may read flash while it is being erased/programmed).
static uint16_t Crc16(const volatile uint16_t *p, uint16_t n, uint16_t crc)
{
    uint16_t i;

    while(n--)
    {
        crc ^= *p++;
        for(i = 0; i < 16; i++)
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
    }
    return crc;
}

static uint16_t SlotOffset(uint16_t slot)
{
    uint16_t off = 0;
    uint16_t i;

    for(i = 0; i < slot; i++)
        off += kSlotWords[i];
    return off;
}

// Record length in flash for a given payload size (8-word aligned).
static uint16_t Stride(uint16_t payloadWords)
{
    return (uint16_t)((HDR_WORDS + payloadWords + 7U) & ~7U);
}

//*****************************************************************************
// Log scanning
//*****************************************************************************

// Header looks like a record that fits in the sector? (CRC not checked.)
static bool Plausible(const volatile uint16_t *p, uint16_t pos, uint16_t *n)
{
    if(p[H_MAGIC] != SAVE_MAGIC || p[H_VERSION] != SAVE_LAYOUT_VERSION)
        return false;

    *n = p[H_PAYLOAD];
    if(*n == 0 || *n > SAVE_PAYLOAD_MAX)
        return false;

    return ((uint32_t)pos + Stride(*n)) <= SECTOR_WORDS;
}

// Is the whole slot at `pos` erased (0xFFFF)?
static bool SlotBlank(const volatile uint16_t *base, uint16_t pos)
{
    uint16_t i;

    for(i = 0; i < IMG_WORDS; i++)
        if(base[pos + i] != 0xFFFFU)
            return false;
    return true;
}

// Walk the log from the start. Returns true and sets *lastPos to the LAST
// plausible record that starts before `limit`. *endPos is always set to the
// first fully blank slot (== SECTOR_WORDS if the sector is full). A torn
// or garbage slot is stepped over, so records appended after one are found.
static bool WalkSector(int16_t which, uint16_t limit,
                       uint16_t *lastPos, uint16_t *endPos)
{
    const volatile uint16_t *base = FLASH_BASE(which);
    uint16_t pos   = 0;
    uint16_t n;
    bool     found = false;

    while((uint32_t)pos + IMG_WORDS <= SECTOR_WORDS)
    {
        if(Plausible(base + pos, pos, &n))
        {
            if(pos < limit)
            {
                *lastPos = pos;
                found    = true;
            }
            pos += Stride(n);
        }
        else if(SlotBlank(base, pos))
        {
            *endPos = pos;
            return found;
        }
        else
        {
            pos += IMG_WORDS;           // torn / garbage slot: skip it
        }
    }
    *endPos = SECTOR_WORDS;             // full
    return found;
}

// Newest record in this sector whose CRC is good.
static bool NewestInSector(int16_t which, uint16_t *pos, uint32_t *seq,
                           uint32_t *mask, uint16_t *pw)
{
    const volatile uint16_t *base = FLASH_BASE(which);
    uint16_t limit = SECTOR_WORDS;
    uint16_t last, end, n, crc;

    while(WalkSector(which, limit, &last, &end))
    {
        const volatile uint16_t *p = base + last;

        n   = p[H_PAYLOAD];
        crc = Crc16(p, H_CRC, 0xFFFFU);             // words 0..6
        crc = Crc16(p + HDR_WORDS, n, crc);
        if(crc == p[H_CRC])
        {
            *pos  = last;
            *seq  = ((uint32_t)p[H_SEQ_HI] << 16) | p[H_SEQ_LO];
            *mask = ((uint32_t)p[H_MASK_HI] << 16) | p[H_MASK_LO];
            *pw   = n;
            return true;
        }
        limit = last;                   // torn record: look at the one before
    }
    return false;
}

//*****************************************************************************
// Flash operations. EVERYTHING in this section that runs while the flash is
// busy must be in RAM: no calls into flash code, no const (flash) data.
//*****************************************************************************
#ifndef SAVE_HOST_TEST
#pragma CODE_SECTION(FlashCallback,  ".TI.ramfunc");
#pragma CODE_SECTION(FlashProgramOp, ".TI.ramfunc");
#pragma FUNC_CANNOT_INLINE(FlashCallback);
#pragma FUNC_CANNOT_INLINE(FlashProgramOp);
#endif

// Called by the Flash API during long operations: keep the watchdog happy.
static void FlashCallback(void)
{
    EALLOW;
    SysCtrlRegs.WDKEY = 0x55;
    SysCtrlRegs.WDKEY = 0xAA;
    EDIS;
}

// Optionally erase a sector, then optionally program `words` words.
// Returns 0 ok, 1 erase failed, 2 program failed. Interrupts off inside.
static uint16_t FlashProgramOp(uint16_t sectorMask, bool erase,
                               uint16_t *dst, uint16_t *src, uint16_t words,
                               uint16_t *apiStatus)
{
    FLASH_ST fst;
    uint16_t st;
    uint16_t r = 0;
    uint16_t s;

    st = __disable_interrupts();

    if(erase)
    {
        s = Flash_Erase(sectorMask, &fst);
        if(s != STATUS_SUCCESS)
        {
            *apiStatus = s;
            r = 1;
        }
    }
    if(r == 0 && words != 0)
    {
        s = Flash_Program(dst, src, words, &fst);
        if(s != STATUS_SUCCESS)
        {
            *apiStatus = s;
            r = 2;
        }
    }

    __restore_interrupts(st);
    return r;
}

// Erase (optional) + program the RAM image into sector `which` at `pos`,
// then read it back. Returns true only if it verified.
static bool WriteImage(int16_t which, uint16_t pos, bool erase)
{
    volatile uint16_t *dst = FLASH_BASE(which) + pos;
    uint16_t i;
    uint16_t r;

    s_err       = SAVE_ERR_NONE;
    s_apiStatus = 0;

    Buzzer_Stop();                      // no sound ISR / note mid-operation

    Flash_CPUScaleFactor = SCALE_FACTOR;
    Flash_CallbackPtr    = FlashCallback;

    r = FlashProgramOp(SECTOR_MASK(which), erase, (uint16_t *)dst, s_img,
                       IMG_WORDS, &s_apiStatus);
    if(r != 0)
    {
        s_err = (r == 1) ? SAVE_ERR_ERASE : SAVE_ERR_PROGRAM;
        return false;
    }

    for(i = 0; i < IMG_WORDS; i++)
        if(dst[i] != s_img[i])
        {
            s_err = SAVE_ERR_VERIFY;
            return false;
        }
    return true;
}

//*****************************************************************************
// Public API
//*****************************************************************************
static bool LoadFromFlash(void)
{
    uint32_t seq[2], mask[2];
    uint16_t pos[2], pw[2], n, end, dummy;
    bool     good[2];
    int16_t  best = -1;
    uint16_t i;

    for(i = 0; i < IMG_WORDS; i++)
        s_img[i] = 0xFFFFU;
    s_mask   = 0;
    s_seq    = 0;
    s_active = -1;
    s_next   = 0;
    s_dirty  = false;
    s_err    = SAVE_ERR_NONE;

    good[0] = NewestInSector(0, &pos[0], &seq[0], &mask[0], &pw[0]);
    good[1] = NewestInSector(1, &pos[1], &seq[1], &mask[1], &pw[1]);

    if(good[0] && good[1])
        best = ((int32_t)(seq[1] - seq[0]) > 0) ? 1 : 0;    // wrap-safe compare
    else if(good[0])
        best = 0;
    else if(good[1])
        best = 1;

    if(best < 0)
    {
        for(i = 0; i < SAVE_PAYLOAD_WORDS; i++)
            PAYLOAD[i] = 0;
        return false;
    }

    // Copy what the flash copy has. If this firmware added slots at the end
    // since it was saved, the stored payload is shorter: the extra slots are
    // simply "never written" (their mask bits are 0).
    n = (pw[best] < SAVE_PAYLOAD_WORDS) ? pw[best] : SAVE_PAYLOAD_WORDS;
    {
        const volatile uint16_t *src = FLASH_BASE(best) + pos[best] + HDR_WORDS;

        for(i = 0; i < SAVE_PAYLOAD_WORDS; i++)
            PAYLOAD[i] = (i < n) ? src[i] : 0;
    }

    s_mask   = mask[best] & ((SAVE_SLOT_COUNT >= 32) ? 0xFFFFFFFFUL
                                                      : ((1UL << SAVE_SLOT_COUNT) - 1UL));
    s_seq    = seq[best];
    s_active = best;

    WalkSector(best, 0, &dummy, &end);  // only want the append position
    s_next   = end;
    return true;
}

bool Save_Init(void)
{
    bool found = LoadFromFlash();
    SaveSettings s;

    if(Save_Read(SAVE_SLOT_SETTINGS, &s,
                 sizeof(s) / sizeof(uint16_t)))
    {
        if(s.volume < BUZZER_NUM_VOLUMES)
            Buzzer_SetVolume(s.volume);

        if(s.brightness < BACKLIGHT_NUM_LEVELS)
            Backlight_SetLevel(s.brightness);
    }

    return found;
}

bool Save_Read(SaveSlot slot, void *dst, uint16_t words)
{
    uint16_t *d = (uint16_t *)dst;
    const uint16_t *s;
    uint16_t i;

    if((uint16_t)slot >= SAVE_SLOT_COUNT || words != kSlotWords[slot])
        return false;
    if(!(s_mask & (1UL << slot)))
        return false;

    s = PAYLOAD + SlotOffset(slot);
    for(i = 0; i < words; i++)
        d[i] = s[i];
    return true;
}

bool Save_Write(SaveSlot slot, const void *src, uint16_t words)
{
    const uint16_t *s = (const uint16_t *)src;
    uint16_t *d;
    bool     changed;
    uint16_t i;

    if((uint16_t)slot >= SAVE_SLOT_COUNT || words != kSlotWords[slot])
        return false;

    changed = !(s_mask & (1UL << slot));
    d = PAYLOAD + SlotOffset(slot);
    for(i = 0; i < words; i++)
    {
        if(d[i] != s[i])
        {
            d[i]    = s[i];
            changed = true;
        }
    }

    if(changed)
    {
        s_mask |= (1UL << slot);
        s_dirty = true;                 // identical rewrites cost no flash wear
    }
    return true;
}

bool Save_IsDirty(void)
{
    return s_dirty;
}

uint16_t Save_GetLastError(void)
{
    return s_err;
}

uint16_t Save_GetLastApiStatus(void)
{
    return s_apiStatus;
}

bool Save_Flush(void)
{
    uint32_t seq;
    uint16_t crc;
    int16_t  sector;
    uint16_t pos;
    bool     erase;
    bool     ok;

    if(!s_dirty)
        return true;

    seq = s_seq + 1;

    s_img[H_MAGIC]   = SAVE_MAGIC;
    s_img[H_VERSION] = SAVE_LAYOUT_VERSION;
    s_img[H_SEQ_LO]  = (uint16_t)(seq & 0xFFFFU);
    s_img[H_SEQ_HI]  = (uint16_t)(seq >> 16);
    s_img[H_MASK_LO] = (uint16_t)(s_mask & 0xFFFFU);
    s_img[H_MASK_HI] = (uint16_t)(s_mask >> 16);
    s_img[H_PAYLOAD] = SAVE_PAYLOAD_WORDS;

    crc = Crc16(s_img, H_CRC, 0xFFFFU);
    crc = Crc16(PAYLOAD, SAVE_PAYLOAD_WORDS, crc);
    s_img[H_CRC] = crc;

    // Pad words after the payload stay 0xFFFF (set in LoadFromFlash).

    // 1) Normal case: append to the next blank slot of the active sector.
    if(s_active >= 0 && (uint32_t)s_next + IMG_WORDS <= SECTOR_WORDS)
    {
        sector = s_active;
        pos    = s_next;
        erase  = false;
        ok     = WriteImage(sector, pos, erase);
        if(ok)
            goto committed;

        // Failed: that slot may be half-programmed. Never reuse it.
        s_next = (uint16_t)(s_next + IMG_WORDS);
    }

    // 2) Active sector full (or first save, or the append failed): erase the
    //    OTHER sector (it only holds older, superseded records) and start the
    //    log there. The newest copy stays safe in the active sector until the
    //    new record is written.
    sector = (s_active == 0) ? 1 : 0;
    pos    = 0;
    erase  = true;
    ok     = WriteImage(sector, pos, erase);

    if(!ok && s_active < 0)
    {
        // Nothing valid anywhere yet: the other sector is a fair fallback.
        sector = 1;
        ok     = WriteImage(sector, pos, erase);
    }
    if(!ok)
        return false;                   // stays dirty; the next flush retries

committed:
    s_seq    = seq;
    s_active = sector;
    s_next   = (uint16_t)(pos + IMG_WORDS);
    s_dirty  = false;
    return true;
}

bool Save_EraseAll(void)
{
    uint16_t i;
    uint16_t st = 0;

    s_mask = 0;
    for(i = 0; i < SAVE_PAYLOAD_WORDS; i++)
        PAYLOAD[i] = 0;
    s_active = -1;
    s_next   = 0;
    s_seq    = 0;
    s_dirty  = false;

    Buzzer_Stop();
    Flash_CPUScaleFactor = SCALE_FACTOR;
    Flash_CallbackPtr    = FlashCallback;

    if(FlashProgramOp(SECTOR_A_MASK, true, 0, 0, 0, &st) != 0) return false;
    if(FlashProgramOp(SECTOR_B_MASK, true, 0, 0, 0, &st) != 0) return false;
    return true;
}

// Capture the live volume/brightness and write flash if they changed.
// Signature matches App.exit, so it can be plugged straight into Settings.
void Save_SyncSettings(void)
{
    SaveSettings s;

    s.volume     = Buzzer_GetVolume();
    s.brightness = Backlight_GetLevel();

    Save_Write(SAVE_SLOT_SETTINGS, &s,
               sizeof(s) / sizeof(uint16_t));

    Save_Flush();
}
