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
//*****************************************************************************
static const Note s_coinNotes[] =
{
    {N_B5, 2},
    {N_E6, 8}
};

const Song sfx_coin =
{
    s_coinNotes,
    sizeof(s_coinNotes) / sizeof(s_coinNotes[0]),
    30,                 // eighthMs: 30 ms per unit
    0,                  // gapMs: none
    "Coin"
};

//*****************************************************************************
// Song list: the Settings app cycles through these (D-pad C).
//*****************************************************************************
const Song *const songList[] =
{
    &song_corridor_run,
    &song_tetris,
};

const uint16_t numSongs = sizeof(songList) / sizeof(songList[0]);
