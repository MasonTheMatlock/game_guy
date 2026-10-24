//#############################################################################
// FILE:   drivers/save.h
// TITLE:  Save driver - persistent save data / settings in on-chip flash
//
// HOW IT WORKS
//   Flash sectors H (0x3D8000) and G (0x3DC000) are NOT used by the linker
//   (see 28069_RAM_lnk.cmd), so this driver owns them. They are used as two
//   alternating copies ("ping-pong"): a save is written to the sector that is
//   NOT the current good one, and only counts once its CRC checks out. If
//   power dies mid-save, the previous save is still intact.
//
//   Everything lives in a RAM copy. Save_Read()/Save_Write() only touch RAM
//   (fast, call them whenever). Save_Flush() is the ONLY call that touches
//   flash, and it BLOCKS (sector erase = hundreds of ms or more, interrupts
//   are off the whole time). Call it on a user action (leaving a screen,
//   game over, closing Settings), NEVER inside a per-frame update().
//
// SLOTS
//   Each app/feature owns one slot, a fixed number of 16-bit words. NOTE: on
//   the C28x a "char" is 16 bits, so sizeof(x) is already in WORDS.
//
//   TO ADD A SLOT: append ONE line to SAVE_SLOTS below.
//     - Only ever APPEND. Never reorder, delete, or resize an existing slot,
//       or old saves will be read at the wrong offsets.
//     - Use SPARE-style slots now if you want room to grow later.
//
// USAGE
//   main():   Save_Init();   // after Buzzer_Init / Backlight_Init
//
//   Load:     SaveTetris t;
//             if(!Save_Read(SAVE_SLOT_TETRIS, &t, sizeof t))
//                 t.highScore = 0;                  // never saved yet
//
//   Store:    t.highScore = score;
//             Save_Write(SAVE_SLOT_TETRIS, &t, sizeof t);   // RAM only
//             Save_Flush();                                 // blocks, writes flash
//#############################################################################

#ifndef __SAVE_H__
#define __SAVE_H__

#include <stdint.h>
#include <stdbool.h>

//*****************************************************************************
// Slot table:  X(NAME, size in 16-bit words).  APPEND ONLY.
//*****************************************************************************
#define SAVE_SLOTS(X)           \
    X(SETTINGS,   2)            \
    X(TETRIS,     4)            \
    X(RAYCASTER,  4)            \
    X(SPARE,     16)            \
    X(TETRIS_SCORES, 10)

typedef enum
{
#define X(name, words) SAVE_SLOT_##name,
    SAVE_SLOTS(X)
#undef X
    SAVE_SLOT_COUNT                     // must stay <= 32
} SaveSlot;

//*****************************************************************************
// Slot payload types (sizeof must equal the words listed above).
// Add your own app's struct here, or keep it private in your app and just
// pass sizeof(your struct) as long as it matches the table.
//*****************************************************************************
typedef struct
{
    uint16_t volume;                    // Buzzer_GetVolume() index
    uint16_t brightness;                // Backlight_GetLevel() index
} SaveSettings;                         // 2 words

typedef struct
{
    uint32_t highScore;
    uint16_t bestLines;
    uint16_t reserved;
} SaveTetris;                           // 4 words

#define TETRIS_NUM_SCORES   5
typedef struct
{
    uint32_t score[TETRIS_NUM_SCORES];  // best first, 0 = empty
} SaveTetrisScores;                     // 10 words (uint32_t = 2 words)

typedef struct
{
    uint32_t bestScore;
    uint16_t bestLevel;
    uint16_t reserved;
} SaveRaycaster;                        // 4 words

//*****************************************************************************
// API
//*****************************************************************************

// Load the newest valid save from flash into RAM AND apply the saved volume
// and brightness. Returns true if a save was found; false = first boot /
// corrupt, every slot reads as "never saved".
bool Save_Init(void);

// Copy slot -> dst. `words` must equal the slot's size. Returns false (dst
// untouched) if the slot was never written or the size is wrong.
bool Save_Read(SaveSlot slot, void *dst, uint16_t words);

// Copy src -> slot in RAM. Marks the save dirty only if the data changed.
// Returns false if the size is wrong.
bool Save_Write(SaveSlot slot, const void *src, uint16_t words);

// True if RAM has changes not yet written to flash.
bool Save_IsDirty(void);

// Write the RAM copy to flash if dirty. BLOCKS; interrupts off; buzzer is
// stopped. Returns true if nothing to do or the save succeeded and verified.
bool Save_Flush(void);

// Read the live volume/brightness, store them, and flush if they changed.
// Use as Settings' exit callback:  ..., Save_SyncSettings, NULL, NULL
void Save_SyncSettings(void);

// Factory reset: forget everything (RAM and flash).
bool Save_EraseAll(void);

#endif // __SAVE_H__
