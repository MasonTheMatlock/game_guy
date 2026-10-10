#ifndef __DPAD_H__
#define __DPAD_H__

#include <stdint.h>
#include "joystick.h"   // BTN_DEBOUNCE_MS, LONG_PRESS_MS, TimerNow(), MsSince()
                        // (the D-pad debounce uses the joystick's timer)

//*****************************************************************************
// D-pad buttons A, B, C, D
//                A
//              B   C
//                D
//   A = GPIO25   B = GPIO52   C = GPIO53   D = GPIO56
// (the GPIO numbers live in dpad.c, next to the register setup)
//*****************************************************************************

// 1 = pressing a button pulls its pin LOW (internal pull-up is enabled).
// Set to 0 if your buttons drive the pin HIGH when pressed.
#define DPAD_ACTIVE_LOW   1

// How long a D-pad button must be held to count as a long press.
// Defaults to the joystick's value; change it here to tune the D-pad alone.
#define DPAD_LONG_PRESS_MS   LONG_PRESS_MS

typedef struct
{
    int16_t a, b, c, d;                  // 1 while the button is held (debounced)
    int16_t aPressed, bPressed,          // 1 only on the poll where a new press
            cPressed, dPressed;          //   was detected (one-shot)
    int16_t aLong, bLong,                // 1 only on the poll where the button
            cLong, dLong;                //   has been held DPAD_LONG_PRESS_MS
                                         //   (one-shot, once per press)
} Dpad;

extern Dpad g_dpad;

// Call after Joystick_Init() (it starts the timer the debounce relies on).
void    Dpad_Init(void);

// Call once per frame, right next to Joy_Update(). Fills g_dpad.
void    Dpad_Update(void);

// 1 if any of A/B/C/D was newly pressed on the last Dpad_Update()
int16_t Dpad_AnyPressed(void);

// 1 if any of A/B/C/D reached a long press on the last Dpad_Update()
int16_t Dpad_AnyLong(void);

#endif // __DPAD_H__
