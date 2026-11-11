//#############################################################################
// FILE:   app/raycaster/rc_sound.c
// TITLE:  RAYCASTER sounds: the "Corridor Run" theme, staff shots, footsteps
//
//   "Corridor Run": original A minor dungeon-crawler chiptune. One buzzer plays
//   one note at a time, so the chords (Am - F - G - E) are faked with fast
//   arpeggios. Layout (loops): INTRO x2, LEAD, BRIDGE.
//#############################################################################

#include "rc_sound.h"

#define SFX(sym, notes, unitMs, gap, label)                                   \
    const Song sym = { notes, sizeof(notes)/sizeof(notes[0]), unitMs, gap, label }

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


// ---- Staff shots (one per staff, played by raycaster Fire) -------------
static const Note n_staffRay[]   = { {N_G5,1},{N_C6,1},{N_E6,2} };                       // bright rising zip
static const Note n_staffPrism[] = { {N_C5,1},{N_E5,1},{N_G5,1},{N_C6,1},{N_G5,1},{N_E6,1} }; // shimmering split
static const Note n_staffLance[] = { {N_E5,1},{N_C5,1},{N_A4,1},{N_F4,2},{N_E4,3} };     // heavy falling thrust
static const Note n_staffStorm[] = { {N_E6,1},{N_B5,1},{N_E6,1},{N_G5,1},{N_E6,1} };     // short crackle (fires fast)
static const Note n_staffSun[]   = { {N_E4,3},{N_G4,2},{N_C5,1},{N_E5,1},{N_G5,1},{N_C6,2} }; // boom + flare

SFX(sfx_staff_ray,   n_staffRay,   10, 0, "Ray Wand");
SFX(sfx_staff_prism, n_staffPrism, 10, 0, "Prism Staff");
SFX(sfx_staff_lance, n_staffLance, 16, 0, "Lance");
SFX(sfx_staff_storm, n_staffStorm,  5, 0, "Storm Rod");
SFX(sfx_staff_sun,   n_staffSun,   14, 0, "Sunfire");

// ---- Footsteps (grass): very short low rustle, two variants for left/right foot
static const Note n_stepA[] = { {N_E4,1},{N_F4,1},{N_E4,1} };
static const Note n_stepB[] = { {N_F4,1},{N_E4,1},{N_G4,1} };
SFX(sfx_step_a, n_stepA, 8, 0, "Step A");
SFX(sfx_step_b, n_stepB, 8, 0, "Step B");
