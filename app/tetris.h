//#############################################################################
// FILE:   app/tetris.h
// TITLE:  Tetris - public descriptor, tuning constants, shared types
//
// Everything here is visible to any file that includes apps.h, so constants
// carry a TETRIS_ prefix. Data tables (shapes, colors, scoring) and all game
// state stay private (static) in tetris.c.
//#############################################################################

#ifndef __TETRIS_H__
#define __TETRIS_H__

#include "apps.h"

extern const App g_tetrisApp;               // app/tetris.c

// ---- Board geometry -------------------------------------------------------
// The playfield is TETRIS_BOARD_W x TETRIS_BOARD_H cells, each TETRIS_CELL x
// TETRIS_CELL pixels: 10 x 20 cells * 10 px = 100 x 200 px, drawn at
// (TETRIS_BOARD_X, TETRIS_BOARD_Y). That leaves ~100 px free on each side
// for the score panel and next piece.
#define TETRIS_BOARD_W            10
#define TETRIS_BOARD_H            20
#define TETRIS_CELL               10        // pixels per cell
#define TETRIS_BOARD_X            110       // left edge of the playfield (pixels)
#define TETRIS_BOARD_Y            36        // top edge (below the title bar)

// ---- Timing ---------------------------------------------------------------
#define TETRIS_FRAME_US           16000UL   // pause after each frame (~60 fps)
#define TETRIS_DAS_FRAMES         9         // frames you must hold left/right
                                            //   before it starts repeating
#define TETRIS_ARR_FRAMES         3         // frames between repeats
#define TETRIS_GAME_OVER_HOLD_FRAMES 90     // ignore input after game over
                                            //   (~1.5 s at 16 ms/frame)

// ---- D-pad mapping --------------------------------------------------------
// The D-pad works alongside the joystick. To remap a button, point these at
// a different one of g_dpad.a / .b / .c / .d (and aPressed / bPressed / ...).
#define TETRIS_KEY_LEFT_HELD      (g_dpad.a)
#define TETRIS_KEY_RIGHT_HELD     (g_dpad.b)
#define TETRIS_KEY_DOWN_HELD      (g_dpad.c)
#define TETRIS_KEY_ROTATE_TAP     (g_dpad.dPressed)

// ---- Types ----------------------------------------------------------------
// The falling piece. (x, y) is where its 4x4 box sits on the board; the box
// can hang off the edge (x < 0) because the piece's own cells may still be
// inside the board.
typedef struct
{
    int16_t type;       // 0..6, index into the shape table (I, J, L, O, S, T, Z)
    int16_t rot;        // 0..3
    int16_t x;          // column of the 4x4 box's left edge
    int16_t y;          // row of the 4x4 box's top edge
} TetrisPiece;

#endif // __TETRIS_H__
