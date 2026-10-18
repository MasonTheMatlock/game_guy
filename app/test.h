//#############################################################################
// FILE:   app/test.h
// TITLE:  Test app - public descriptor and layout / feel constants
//#############################################################################

#ifndef __TEST_H__
#define __TEST_H__

#include "apps.h"

extern const App g_testApp;                 // app/test.c

//*****************************************************************************
// Timing / feel
//*****************************************************************************
#define TEST_FRAME_US       16000UL     // pause after each frame (~60 fps)
#define TEST_INVERT_Y       0           // g_joy.dy is +DOWN, same as the screen's +y.
                                        // Set to 1 if your stick is wired backwards.
#define TEST_EDGE_ON        60          // |stick| >= 60  -> counts as a "flick"
#define TEST_EDGE_OFF       30          // |stick| <  30  -> stick has returned to center
#define TEST_PAD_LIT_FRAMES 6           // a pad button stays lit this many frames after
                                        // an edge (~100 ms), so a tap is visible
#define TEST_CHORD_SLACK    2           // R+D within this many frames = "back to menu"
#define TEST_PRED_GAIN      2           // prediction = pos + GAIN * (pos - lastPos)

//*****************************************************************************
// D-pad buttons (U L R D). The app only sees one-frame press EDGES (xPressed), so each
// button is lit for TEST_PAD_LIT_FRAMES after its edge. If your dpad driver
// has a "held" flag, point these at it and set TEST_PAD_LIT_FRAMES to 1.
//*****************************************************************************
#define TEST_PAD_U          (g_dpad.uPressed)
#define TEST_PAD_L          (g_dpad.lPressed)
#define TEST_PAD_R          (g_dpad.rPressed)
#define TEST_PAD_D          (g_dpad.dPressed)

//*****************************************************************************
// Input-test layout (pixels). Screen is 320 x 240.
//*****************************************************************************
#define TEST_BOX_CX         80          // stick box center
#define TEST_BOX_CY         125
#define TEST_BOX_HALF       55          // box is 110 x 110
#define TEST_DOT_R          5           // current position (filled square, half-size)
#define TEST_RING_R         7           // predicted position (hollow square, half-size)
#define TEST_ERASE_R        9           // erase square half-size around either marker

#define TEST_PAD_CX         240         // button diamond center
#define TEST_PAD_CY         125
#define TEST_PAD_OFS        42          // center-to-button distance
#define TEST_PAD_HALF       17          // button is 34 x 34

#define TEST_STK_Y          203         // stick-button indicator, centered on PAD_CX
#define TEST_READOUT_Y      197         // "X:.. Y:.." text under the stick box

//*****************************************************************************
// Sound-test layout
//*****************************************************************************
#define TEST_ROW_Y0         80          // first row's text center
#define TEST_ROW_DY         34          // spacing between rows
#define TEST_BAR_X0         61          // volume bars: 5 bars, 30 wide, 12 gap
#define TEST_BAR_W          30
#define TEST_BAR_GAP        12
#define TEST_BAR_BOTTOM     198
#define TEST_BAR_H0         8           // shortest bar; each next one is +4 taller

#endif // __TEST_H__
