//#############################################################################
// FILE:   drivers/memory.h
// TITLE:  Save driver - persistent save data / settings in on-chip flash
//
// HOW IT WORKS
//   Flash sector H (0x3D8000) is NOT used by the linker, so this driver owns
//   it (sector G used to be a second save sector; it is program flash now).
//   The sector is an append-only LOG of save records: a save is APPENDED to
//   the first blank slot, and the sector is erased only when it is full
//   (~200 saves), then the log restarts. That is the wear leveling: ~227x fewer erases than erasing on
//   every save. A record only counts once its CRC checks out, so if power
//   dies mid-save the previous record is still the newest valid one.
//
//   Everything lives in a RAM copy. Memory_Read()/Memory_Write() only touch RAM
//   (fast, call them whenever). Memory_Flush() is the ONLY call that touches
//   flash, and it BLOCKS (sector erase = hundreds of ms or more, interrupts
//   are off the whole time). Call it on a user action (leaving a screen,
//   game over, closing Settings), NEVER inside a per-frame update().
//
// SLOTS
//   Each app/feature owns one slot, a fixed number of 16-bit words. NOTE: on
//   the C28x a "char" is 16 bits, so sizeof(x) is already in WORDS.
//
//   TO ADD A SLOT: append ONE line to MEM_SLOTS below.
//     - Only ever APPEND. Never reorder, delete, or resize an existing slot,
//       or old saves will be read at the wrong offsets.
//     - Use SPARE-style slots now if you want room to grow later.
//
// USAGE
//   main():   Memory_Init();   // after Buzzer_Init / Backlight_Init
//
//   Load:     MemTetris t;
//             if(!Memory_Read(MEM_SLOT_TETRIS, &t, sizeof t))
//                 t.highScore = 0;                  // never saved yet
//
//   Store:    t.highScore = score;
//             Memory_Write(MEM_SLOT_TETRIS, &t, sizeof t);   // RAM only
//             Memory_Flush();                                 // blocks, writes flash
//#############################################################################

#ifndef __MEM_H__
#define __MEM_H__

#include <stdint.h>
#include <stdbool.h>

//*****************************************************************************
// Slot table:  X(NAME, size in 16-bit words).  APPEND ONLY.
//*****************************************************************************
#define MEM_SLOTS(X)           \
    X(SETTINGS,   2)            \
    X(TETRIS,     4)            \
    X(RAYCASTER,  4)            \
    X(SPARE,     16)            \
    X(TETRIS_SCORES, 10)        \
    X(TETRIS_NAMES, 25)         \
    X(MENU,          2)         \
    X(WASONWARE,     4)             X(RC_SCORES,    30)
typedef enum
{
#define X(name, words) MEM_SLOT_##name,
    MEM_SLOTS(X)
#undef X
    MEM_SLOT_COUNT                     // must stay <= 32
} MemSlot;

//*****************************************************************************
// Slot payload types (sizeof must equal the words listed above).
// Add your own app's struct here, or keep it private in your app and just
// pass sizeof(your struct) as long as it matches the table.
//*****************************************************************************
typedef struct
{
    uint16_t volume;                    // Buzzer_GetVolume() index
    uint16_t brightness;                // Backlight_GetLevel() index
} MemSettings;                         // 2 words

typedef struct
{
    uint32_t highScore;
    uint16_t bestLines;
    uint16_t reserved;
} MemTetris;                           // 4 words

#define TETRIS_NUM_SCORES   5
typedef struct
{
    uint32_t score[TETRIS_NUM_SCORES];  // best first, 0 = empty
} MemTetrisScores;                     // 10 words (uint32_t = 2 words)

#define TETRIS_NAME_LEN     5
typedef struct
{
    uint16_t c[TETRIS_NUM_SCORES][TETRIS_NAME_LEN];   // ASCII, one char per word
} MemTetrisNames;                      // 25 words

typedef struct
{
    uint32_t bestScore;
    uint16_t bestLevel;
    uint16_t reserved;
} MemRaycaster;                        // 4 words

// Raycaster high scores: top 5 waves survived, best first, 5-char names.
// wave 0 = empty entry (the game shows its default "WASON / wave 15" there).
#define RC_NUM_SCORES   5
#define RC_NAME_LEN     5
typedef struct
{
    uint16_t wave[RC_NUM_SCORES];
    uint16_t c[RC_NUM_SCORES][RC_NAME_LEN];   // ASCII, one char per word
} MemRcScores;                         // 30 words

typedef struct
{
    uint16_t bgIndex;                   // main menu background: palette index
    uint16_t reserved;
} MemMenu;                             // 2 words

typedef struct
{
    uint32_t best;                      // Wason Ware best run (microgames won)
    uint16_t reserved[2];
} MemWason;                            // 4 words

//*****************************************************************************
// API
//*****************************************************************************

// Load the newest valid save from flash into RAM AND apply the saved volume
// and brightness. Returns true if a save was found; false = first boot /
// corrupt, every slot reads as "never saved".
bool Memory_Init(void);

// Copy slot -> dst. `words` must equal the slot's size. Returns false (dst
// untouched) if the slot was never written or the size is wrong.
bool Memory_Read(MemSlot slot, void *dst, uint16_t words);

// Copy src -> slot in RAM. Marks the save dirty only if the data changed.
// Returns false if the size is wrong.
bool Memory_Write(MemSlot slot, const void *src, uint16_t words);

// True if RAM has changes not yet written to flash.
bool Memory_IsDirty(void);

// Why the last Memory_Flush()/write failed (MEM_ERR_*), and the raw Flash API
// status code. Handy to watch in the debugger or print on screen.
#define MEM_ERR_NONE       0
#define MEM_ERR_ERASE      1
#define MEM_ERR_PROGRAM    2
#define MEM_ERR_VERIFY     3
#define MEM_ERR_SETUP      4   // Flash API scale factor / callback did not stick
uint16_t Memory_GetLastError(void);
uint16_t Memory_GetLastApiStatus(void);
bool     Memory_FlashSetupOk(void);       // false = Flash API scale/callback write did not stick

// Write the RAM copy to flash if dirty. BLOCKS; interrupts off; buzzer is
// stopped. Returns true if nothing to do or the save succeeded and verified.
bool Memory_Flush(void);

// Read the live volume/brightness, store them, and flush if they changed.
// Use as Settings' exit callback:  ..., Memory_SyncSettings, NULL, NULL
void Memory_SyncSettings(void);

// Factory reset: forget everything (RAM and flash).
bool Memory_EraseAll(void);

// Factory reset WITHOUT erasing: put every slot back to its factory values and
// APPEND one new record to the log (the old records just become history), so
// it costs the same flash wear as any other save. Sector erase only happens
// if the active sector happens to be full, exactly like Memory_Flush().
//   volume level 5 (index 4), brightness level 8 (index 7), menu color 0,
//   Tetris scores 0 with blank names, every other slot zeroed.
// Also applies the volume/brightness to the hardware now. BLOCKS (flash).
// Returns true if it reached flash and verified.
bool Memory_FactoryReset(void);

#endif // __MEM_H__
