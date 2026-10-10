//#############################################################################
// FILE:   app/test.h
// TITLE:  Test app - public descriptor and layout / feel constants
//#############################################################################

#ifndef __TEST_H__
#define __TEST_H__

#include "apps.h"

extern const App g_testApp;                 // app/test.c

//*****************************************************************************
// Layout (pixels). Screen is 320 x 240.
//*****************************************************************************
#define TEST_PLAY_X0     10             // play area (inclusive)
#define TEST_PLAY_Y0     38
#define TEST_PLAY_X1     (SCREEN_W - 11)
#define TEST_PLAY_Y1     190

#define TEST_SQUARE      20             // square side length

#define TEST_COUNTER_Y   204            // "frames: N" text, centered
#define TEST_HINT_Y      228            // bottom hint, centered

//*****************************************************************************
// Feel
//*****************************************************************************
#define TEST_FRAME_US    16000UL        // pause after each frame (~60 fps)
#define TEST_STICK_DIV   20             // stick (-100..100) / 20 = -5..5 px/frame
#define TEST_INVERT_Y    0              // g_joy.dy is +DOWN (see joystick.h), same as
                                        // the screen's +y, so no inversion needed.
                                        // Set to 1 if your stick is wired backwards.
#define TEST_COUNTER_EVERY 10           // redraw the counter every N frames

#endif // __TEST_H__
