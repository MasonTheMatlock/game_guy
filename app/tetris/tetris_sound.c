//#############################################################################
// FILE:   app/tetris/tetris_sound.c
// TITLE:  TETRIS sounds: the Korobeiniki theme and the announcer jingle
//
//   "Korobeiniki": the traditional Russian folk melody used as the Tetris
//   theme. Layout (loops): A, A, B, A, short pause.
//#############################################################################

#include "tetris_sound.h"

#define SFX(sym, notes, unitMs, gap, label)                                   \
    const Song sym = { notes, sizeof(notes)/sizeof(notes[0]), unitMs, gap, label }

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


static const Note n_tetrisSay[] = {{N_D5,2},{N_A5,1},{N_G5,1},{N_E5,1},{N_C5,3}};
SFX(sfx_tetris_say, n_tetrisSay, 40, 8, "Tetris Say");
