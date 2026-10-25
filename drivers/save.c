//#############################################################################
// FILE:   drivers/save.c
// TITLE:  Save driver implementation. See save.h for the big picture.
//
// REQUIREMENTS
//   * TI Flash API for F2806x (Flash2806x_API_Library.h + the .lib) added to
//     the project include path and linked. (C2000Ware: libraries/flash_api/
//     f2806x, or controlSUITE: Flash2806x_API.)
//   * Flash API code must run from RAM: your .text is already in RAM. Good.
//   * Sectors G/H must stay out of the linker MEMORY map (they are).
//
// FLASH SECTOR LAYOUT (16-bit words)
//   [0] magic   [1] layout version   [2..3] sequence (newer = bigger)
//   [4..5] slot-present mask   [6] payload words   [7] CRC16 of [0..6]+payload
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
static uint32_t s_seq;                  // sequence of the copy currently in flash
static int16_t  s_active = -1;          // -1 none, 0 = sector A, 1 = sector B
static bool     s_dirty;

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

static uint32_t SectorAddr(int16_t which)
{
    return which ? SECTOR_B_ADDR : SECTOR_A_ADDR;
}

// Is there a complete, CRC-correct save at this sector? If so fill the outputs.
static bool CheckSector(uint32_t addr, uint32_t *seq, uint32_t *mask, uint16_t *pw)
{
    const volatile uint16_t *p = (const volatile uint16_t *)addr;
    uint16_t n;
    uint16_t crc;

    if(p[H_MAGIC] != SAVE_MAGIC || p[H_VERSION] != SAVE_LAYOUT_VERSION)
        return false;

    n = p[H_PAYLOAD];
    if(n == 0 || n > SAVE_PAYLOAD_MAX)
        return false;

    crc = Crc16(p, H_CRC, 0xFFFFU);                 // words 0..6
    crc = Crc16(p + HDR_WORDS, n, crc);
    if(crc != p[H_CRC])
        return false;

    *seq  = ((uint32_t)p[H_SEQ_HI] << 16) | p[H_SEQ_LO];
    *mask = ((uint32_t)p[H_MASK_HI] << 16) | p[H_MASK_LO];
    *pw   = n;
    return true;
}

// Called by the Flash API during long operations: keep the watchdog happy.
// Must not touch flash-resident (const) data.
static void FlashCallback(void)
{
    EALLOW;
    SysCtrlRegs.WDKEY = 0x55;
    SysCtrlRegs.WDKEY = 0xAA;
    EDIS;
}

// Erase `sectorMask` (and optionally program s_img at `addr`). Interrupts off.
static bool FlashOp(uint16_t sectorMask, uint32_t addr, bool program)
{
    FLASH_ST fst;
    uint16_t st;
    bool     ok;

    Buzzer_Stop();                      // no sound ISR / note mid-operation

    st = __disable_interrupts();

    Flash_CPUScaleFactor = SCALE_FACTOR;
    Flash_CallbackPtr    = FlashCallback;

    ok = (Flash_Erase(sectorMask, &fst) == STATUS_SUCCESS);
    if(ok && program)
        ok = (Flash_Program((Uint16 *)addr, s_img, IMG_WORDS, &fst) == STATUS_SUCCESS);

    __restore_interrupts(st);
    return ok;
}

//*****************************************************************************
// Public API
//*****************************************************************************
static bool LoadFromFlash(void)
{
    uint32_t seq[2], mask[2];
    uint16_t pw[2], n;
    bool     good[2];
    int16_t  best = -1;
    uint16_t i;

    for(i = 0; i < IMG_WORDS; i++)
        s_img[i] = 0xFFFFU;
    s_mask   = 0;
    s_seq    = 0;
    s_active = -1;
    s_dirty  = false;

    good[0] = CheckSector(SECTOR_A_ADDR, &seq[0], &mask[0], &pw[0]);
    good[1] = CheckSector(SECTOR_B_ADDR, &seq[1], &mask[1], &pw[1]);

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
        const volatile uint16_t *src =
            (const volatile uint16_t *)SectorAddr(best) + HDR_WORDS;

        for(i = 0; i < SAVE_PAYLOAD_WORDS; i++)
            PAYLOAD[i] = (i < n) ? src[i] : 0;
    }

    s_mask   = mask[best] & ((SAVE_SLOT_COUNT >= 32) ? 0xFFFFFFFFUL
                                                      : ((1UL << SAVE_SLOT_COUNT) - 1UL));
    s_seq    = seq[best];
    s_active = best;
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
    bool     changed = !(s_mask & (1UL << slot));
    uint16_t i;

    if((uint16_t)slot >= SAVE_SLOT_COUNT || words != kSlotWords[slot])
        return false;

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

bool Save_Flush(void)
{
    int16_t  target;
    uint32_t addr;
    uint32_t seq;
    uint16_t crc;
    uint16_t i;
    const volatile uint16_t *p;

    if(!s_dirty)
        return true;

    // Write to the sector that is NOT the current good copy.
    target = (s_active == 0) ? 1 : 0;
    addr   = SectorAddr(target);
    seq    = s_seq + 1;

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

    // Pad words after the payload stay 0xFFFF (set in Save_Init / unchanged).
    if(!FlashOp(target ? SECTOR_B_MASK : SECTOR_A_MASK, addr, true))
        return false;                   // old copy is still valid; stays dirty

    // Read back and compare before trusting it.
    p = (const volatile uint16_t *)addr;
    for(i = 0; i < IMG_WORDS; i++)
        if(p[i] != s_img[i])
            return false;

    s_seq    = seq;
    s_active = target;
    s_dirty  = false;
    return true;
}

bool Save_EraseAll(void)
{
    uint16_t i;

    s_mask = 0;
    for(i = 0; i < SAVE_PAYLOAD_WORDS; i++)
        PAYLOAD[i] = 0;
    s_active = -1;
    s_seq    = 0;
    s_dirty  = false;

    return FlashOp(SECTOR_A_MASK | SECTOR_B_MASK, 0, false);
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
