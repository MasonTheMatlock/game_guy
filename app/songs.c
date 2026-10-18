//#############################################################################
// FILE:   app/songs.c
// TITLE:  Song and sound-effect data
//
//   "Corridor Run": original A minor dungeon-crawler chiptune. One buzzer plays
//   one note at a time, so the chords (Am - F - G - E) are faked with fast
//   arpeggios. Layout (loops): INTRO x2, LEAD, BRIDGE.
//
//   "Korobeiniki": the traditional Russian folk melody used as the Tetris
//   theme. Layout (loops): A, A, B, A, short pause.
//#############################################################################

#include "songs.h"

//*****************************************************************************
// Corridor Run
//*****************************************************************************
#define INTRO_RIFF \
    /* Am */ {N_A4,D_E},{N_C5,D_E},{N_E5,D_E},{N_A5,D_E},{N_E5,D_E},{N_C5,D_E},{N_E5,D_E},{N_C5,D_E}, \
    /* F  */ {N_F4,D_E},{N_A4,D_E},{N_C5,D_E},{N_F5,D_E},{N_C5,D_E},{N_A4,D_E},{N_C5,D_E},{N_A4,D_E}, \
    /* G  */ {N_G4,D_E},{N_B4,D_E},{N_D5,D_E},{N_G5,D_E},{N_D5,D_E},{N_B4,D_E},{N_D5,D_E},{N_B4,D_E}, \
    /* E  */ {N_E4,D_E},{N_G4S,D_E},{N_B4,D_E},{N_E5,D_E},{N_B4,D_E},{N_G4S,D_E},{N_B4,D_E},{N_G4S,D_E}

#define LEAD_MELODY \
    /* Bar 1 (Am) */ {N_E5,D_Q},{N_E5,D_E},{N_D5,D_E},{N_C5,D_Q},{N_A4,D_Q}, \
    /* Bar 2 (F)  */ {N_C5,D_Q},{N_A4,D_E},{N_C5,D_E},{N_F5,D_H}, \
    /* Bar 3 (G)  */ {N_D5,D_Q},{N_D5,D_E},{N_E5,D_E},{N_G5,D_Q},{N_D5,D_Q}, \
    /* Bar 4 (E)  */ {N_B4,D_Q},{N_G4S,D_Q},{N_E5,D_H}, \
    /* Bar 5 (Am) */ {N_A5,D_Q},{N_G5,D_E},{N_E5,D_E},{N_C5,D_Q},{N_E5,D_Q}, \
    /* Bar 6 (F)  */ {N_F5,D_Q},{N_E5,D_E},{N_C5,D_E},{N_A4,D_Q},{N_C5,D_Q}, \
    /* Bar 7 (G)  */ {N_B4,D_Q},{N_D5,D_Q},{N_G5,D_Q},{N_F5,D_E},{N_D5,D_E}, \
    /* Bar 8 (E)  */ {N_E5,D_Q},{N_G4S,D_Q},{N_A4,D_H}

#define BRIDGE_RUN \
    /* Dm */ {N_D5,D_E},{N_F5,D_E},{N_A5,D_E},{N_F5,D_E},{N_D5,D_E},{N_F5,D_E},{N_A5,D_E},{N_C6,D_E}, \
    /* Am */ {N_C6,D_E},{N_A5,D_E},{N_E5,D_E},{N_A5,D_E},{N_C6,D_E},{N_A5,D_E},{N_E5,D_E},{N_A5,D_E}, \
    /* E  */ {N_E5,D_E},{N_G5S,D_E},{N_B5,D_E},{N_E6,D_E},{N_B5,D_E},{N_G5S,D_E},{N_E5,D_E},{N_G5S,D_E}, \
    /* E  */ {N_B5,D_Q},{N_G5S,D_Q},{N_E5,D_Q},{N_REST,D_Q}

static const Note s_corridorRunNotes[] =
{
    INTRO_RIFF, INTRO_RIFF,
    LEAD_MELODY,
    BRIDGE_RUN
};

const Song song_corridor_run =
{
    s_corridorRunNotes,
    sizeof(s_corridorRunNotes) / sizeof(s_corridorRunNotes[0]),
    150,                // eighthMs: 150 ms per eighth (~200 BPM)
    25,                 // gapMs: staccato
    "Corridor Run"
};

//*****************************************************************************
// Korobeiniki (traditional Russian folk melody)
//*****************************************************************************
#define TETRIS_A \
    /* Bar 1 */ {N_E5,D_Q},{N_B4,D_E},{N_C5,D_E},{N_D5,D_Q},{N_C5,D_E},{N_B4,D_E}, \
    /* Bar 2 */ {N_A4,D_Q},{N_A4,D_E},{N_C5,D_E},{N_E5,D_Q},{N_D5,D_E},{N_C5,D_E}, \
    /* Bar 3 */ {N_B4,D_QD},{N_C5,D_E},{N_D5,D_Q},{N_E5,D_Q}, \
    /* Bar 4 */ {N_C5,D_Q},{N_A4,D_Q},{N_A4,D_H}, \
    /* Bar 5 */ {N_REST,D_E},{N_D5,D_QD},{N_F5,D_E},{N_A5,D_Q},{N_G5,D_E},{N_F5,D_E}, \
    /* Bar 6 */ {N_E5,D_QD},{N_C5,D_E},{N_E5,D_Q},{N_D5,D_E},{N_C5,D_E}, \
    /* Bar 7 */ {N_B4,D_Q},{N_B4,D_E},{N_C5,D_E},{N_D5,D_Q},{N_E5,D_Q}, \
    /* Bar 8 */ {N_C5,D_Q},{N_A4,D_Q},{N_A4,D_H}

#define TETRIS_B \
    /* Bar 9  */ {N_E5,D_H},{N_C5,D_H}, \
    /* Bar 10 */ {N_D5,D_H},{N_B4,D_H}, \
    /* Bar 11 */ {N_C5,D_H},{N_A4,D_H}, \
    /* Bar 12 */ {N_G4S,D_QD+D_QD},{N_B4,D_Q}, \
    /* Bar 13 */ {N_E5,D_H},{N_C5,D_H}, \
    /* Bar 14 */ {N_D5,D_H},{N_B4,D_H}, \
    /* Bar 15 */ {N_C5,D_H},{N_E5,D_H}, \
    /* Bar 16 */ {N_A5,D_QD+D_QD},{N_G5S,D_Q}

static const Note s_tetrisNotes[] =
{
    TETRIS_A, TETRIS_A,
    TETRIS_B,
    TETRIS_A,
    {N_REST,D_H}
};

const Song song_tetris =
{
    s_tetrisNotes,
    sizeof(s_tetrisNotes) / sizeof(s_tetrisNotes[0]),
    200,                // eighthMs: 200 ms per eighth (150 BPM)
    40,                 // gapMs: keeps repeated notes distinct
    "Tetris"
};

//*****************************************************************************
// Coin collect: two quick rising notes (60 ms, then 240 ms)
//
/******************************************************************************
Legacy SFX
static const Note s_coinNotes[] ={{N_B5, 2},{N_E6, 8}};

const Song sfx_coin =
{
    s_coinNotes,
    sizeof(s_coinNotes) / sizeof(s_coinNotes[0]),
    30,                 // eighthMs: 30 ms per unit
    0,                  // gapMs: none
    "Coin"
};
*/

//*****************************************************************************
// Sound effects. Duration numbers are multiples of the "unit" ms value.
//*****************************************************************************
#define SFX(sym, notes, unitMs, gap, label)                                   \
    const Song sym = { notes, sizeof(notes)/sizeof(notes[0]), unitMs, gap, label }

// ---- UI ----------------------------------------------------------------

static const Note n_coinNotes[] ={{N_B5, 2},{N_E6, 8}};

static const Note n_menuMove[]   = { {N_C6,2} };                                // 30 ms tick
static const Note n_menuBump[]   = { {N_E4,3} };                                // low, dull
static const Note n_menuSelect[] = { {N_G5,2},{N_C6,2},{N_E6,4} };              // rising
static const Note n_menuBack[]   = { {N_E6,2},{N_C6,2},{N_G5,4} };              // falling
static const Note n_pause[]      = { {N_E5,2},{N_B4,2},{N_E4,3} };
static const Note n_resume[]     = { {N_E4,2},{N_B4,2},{N_E5,3} };


// ---- Games -------------------------------------------------------------
static const Note n_blip[]    = { {N_A5,1} };
static const Note n_thud[]    = { {N_G4,2},{N_E4,2} };
static const Note n_jump[]    = { {N_C5,1},{N_E5,1},{N_G5,1},{N_C6,1},{N_E6,2} };
static const Note n_laser[]   = { {N_E6,1},{N_C6,1},{N_A5,1},{N_F5,1},{N_D5,1},{N_B4,1},{N_G4,1} };
static const Note n_powerup[] = { {N_E5,1},{N_G5,1},{N_B5,1},{N_E6,1},
                                  {N_G5,1},{N_B5,1},{N_E6,1},{N_E6,3} };
static const Note n_hit[]     = { {N_A4,1},{N_E4,1},{N_A4,1},{N_E4,1},{N_F4,2} };
static const Note n_explode[] = { {N_G4,1},{N_E4,1},{N_F4,1},{N_E4,1},{N_G4,1},{N_E4,1},
                                  {N_F4,1},{N_E4,1},{N_F4,2},{N_E4,2},{N_E4,4} };   // rattle
static const Note n_levelUp[] = { {N_G4,1},{N_C5,1},{N_E5,1},{N_G5,2},
                                  {N_E5,1},{N_G5,1},{N_C6,4} };
static const Note n_gameOver[]= { {N_C5,2},{N_B4,2},{N_A4,2},{N_G4S,6} };

static const Note n_tetrisSay[] = {{N_D5,2},{N_A5,1},{N_G5,1},{N_E5,1},{N_C5,3}};
static const Note n_bootDmg[] = { {N_B5,2},{N_E6,10} };
static const Note n_bootGbc[] = {{N_C5,1},{N_E5,1},{N_G5,1},{N_C6,1},{N_E6,1},{N_REST,2}, {N_E6,3},{N_G5S,0 + 1},{N_B5,1},{N_E6,10}};

SFX(sfx_coin,   n_coinNotes,   30, 0, "Coin");

SFX(sfx_menu_move,   n_menuMove,   15, 0, "Menu Move");
SFX(sfx_menu_bump,   n_menuBump,   20, 0, "Menu Bump");
SFX(sfx_menu_select, n_menuSelect, 20, 0, "Menu Select");
SFX(sfx_menu_back,   n_menuBack,   20, 0, "Menu Back");
SFX(sfx_pause,       n_pause,      25, 0, "Pause");
SFX(sfx_resume,      n_resume,     25, 0, "Resume");

SFX(sfx_blip,      n_blip,     20,  0,  "Blip");
SFX(sfx_thud,      n_thud,     25,  0,  "Thud");
SFX(sfx_jump,      n_jump,     14,  0,  "Jump");
SFX(sfx_laser,     n_laser,     8,  0,  "Laser");
SFX(sfx_powerup,   n_powerup,  45,  0,  "Power Up");
SFX(sfx_hit,       n_hit,      30,  0,  "Hit");
SFX(sfx_explosion, n_explode,  16,  0,  "Explosion");
SFX(sfx_level_up,  n_levelUp,  80, 15,  "Level Up");
SFX(sfx_game_over, n_gameOver, 110, 20, "Game Over");
SFX(sfx_tetris_say, n_tetrisSay, 40, 8, "Tetris Say");
SFX(sfx_boot_gbc, n_bootGbc, 35, 0, "Boot GBC");
SFX(sfx_boot_dmg, n_bootDmg, 45, 0, "Boot DMG");





//*****************************************************************************
// Song list: the Settings app cycles through these (D-pad C).
//*****************************************************************************
const Song *const songList[] =
{
    &song_corridor_run,
    &song_tetris,
};

const uint16_t numSongs = sizeof(songList) / sizeof(songList[0]);
