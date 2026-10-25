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
#define TETRIS_CLEAR_FRAMES       20        // full rows flash this long before they vanish
                                            //   (~0.3 s at 16 ms/frame)
#define TETRIS_CLEAR_FLASH_FRAMES 4         // frames per white / normal phase
                                            //   (20 / 4 = white, normal, white, normal, white)
#define TETRIS_BANNER_FRAMES      75        // how long TETRIS! / NEW #1! stays up (~1.2 s)

// ---- Title screen / high scores ------------------------------------------
// Start / High Scores screen (drawn by tetris.c). The top-5 list itself is
// saved in flash through drivers/save.h (SAVE_SLOT_TETRIS_SCORES).
#define TETRIS_TITLE_ROW_X0       70        // menu rectangles, left/right edge
#define TETRIS_TITLE_ROW_X1       249
#define TETRIS_TITLE_ROW_Y0       90        // top of the first rectangle
#define TETRIS_TITLE_ROW_H        34
#define TETRIS_TITLE_ROW_GAP      6
#define TETRIS_SCORE_Y0           58        // first line of the high score list
#define TETRIS_SCORE_PITCH        32
#define TETRIS_STICK_ON           60        // stick flick thresholds
#define TETRIS_STICK_OFF          30

// ---- D-pad mapping --------------------------------------------------------
// The D-pad works alongside the joystick. To remap a button, point these at
// a different one of g_dpad.u / .l / .r / .d (and uPressed / lPressed / ...).
#define TETRIS_KEY_LEFT_HELD      (g_dpad.l)
#define TETRIS_KEY_RIGHT_HELD     (g_dpad.r)
#define TETRIS_KEY_DOWN_HELD      (g_dpad.d)
#define TETRIS_KEY_ROTATE_TAP     (g_dpad.uPressed)

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
