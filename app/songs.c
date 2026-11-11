//#############################################################################
// FILE:   app/songs.c
// TITLE:  Song and sound-effect data
//
//   Sounds that EVERY app can use: UI ticks, pause / resume, coin, generic arcade
//   effects and the boot jingles. Sounds that belong to one app live in that
//   app's folder:  app/tetris/tetris_sound.c   app/raycaster/rc_sound.c
//#############################################################################

#include "songs.h"

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

static const Note n_bootDmg[] = { {N_B5,2},{N_E6,10} };

static const Note n_bootGbc[] = {
    {N_REST, 4},  // 0.0–0.4s: startup pause
    {N_E5,   2},  // 0.4–0.6s
    {N_G5,   2},  // 0.6–0.8s
    {N_C6,   3},  // 0.8–1.1s: bright rising chime
    {N_G5,   2},  // 1.1–1.3s
    {N_C6,   4},  // 1.3–1.7s: main startup note
    {N_REST, 2},  // 1.7–1.9s: short breath
    {N_G5,   2},  // 1.9–2.1s
    {N_C6,   2},  // 2.1–2.3s
    {N_E6,   4},  // 2.3–2.7s: high sparkle
    {N_C6,   2},  // 2.7–2.9s
    {N_G5,   3},  // 2.9–3.2s
    {N_C6,   3},  // 3.2–3.5s
    {N_E6,   4},  // 3.5–3.9s: final flourish
    {N_G5,   2},  // 3.9–4.1s
    {N_C6,   2},  // 4.1–4.3s
    {N_E6,   7},  // 4.3–5.0s: final sustained note
};


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
SFX(sfx_boot_gbc, n_bootGbc, 72, 25, "Boot GBC");
SFX(sfx_boot_dmg, n_bootDmg, 45, 0, "Boot DMG");
