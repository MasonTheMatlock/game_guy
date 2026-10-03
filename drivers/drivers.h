#ifndef __DRIVERS_H__
#define __DRIVERS_H__

#include <stdint.h>
#include <stdbool.h>
#include "F2806x_Device.h"
#include "F2806x_Examples.h"

//*****************************************************************************
// Joystick tuning (raw ADC counts are 0..4095, centered around ~2048)
//*****************************************************************************
#define JOY_SAMPLES       4       // ADC samples averaged per read (noise filter)
#define JOY_DEADZONE      400     // analog deadzone around center
#define JOY_FULLSCALE     1800    // deflection that counts as 100% (stick rarely hits the rails)
#define JOY_DIR_ENGAGE    1000    // digital direction turns ON beyond this...
#define JOY_DIR_RELEASE   600     // ...and OFF below this (hysteresis, no flicker)

#define BTN_DEBOUNCE_MS   25
#define LONG_PRESS_MS     1200


#define TICKS_PER_MS      ((uint32_t)(1.0e6L / CPU_RATE))

//*****************************************************************************
// Calibrated stick center (a variable, NOT a #define, so calibration can set it)
//*****************************************************************************
extern uint16_t center_x;
extern uint16_t center_y;

//*****************************************************************************
// Processed joystick state, refreshed by Joy_Update()
//*****************************************************************************
typedef struct
{
    int16_t dx, dy;          // analog deflection -100..100, 0 inside deadzone (+x right, +y down)
    int16_t dirX, dirY;      // digital direction -1 / 0 / +1 (with hysteresis)
    int16_t edgeX, edgeY;    // direction newly engaged on this poll (-1 / 0 / +1)
    int16_t btn;             // 1 while the button is held
    int16_t btnPressed;      // 1 on the poll where a press was detected
    int16_t btnLong;         // 1 once, when a long press is reached
} Joy;

extern Joy g_joy;

//*****************************************************************************
// API
//*****************************************************************************

// One call sets up the switch GPIO, ADC and timer, then calibrates the stick.
// Returns 1 if calibration looked sane, 0 if it fell back to 2048/2048
// (stick was held off-center, or the wiring is wrong).
int16_t Joystick_Init(void);

// Call once per frame. Fills g_joy.
void Joy_Update(void);

// Block until the button is released, then clear pending edges
void Joy_WaitRelease(void);

// Low-level pieces (Joystick_Init calls these for you)
void    Init_Joystick_ADC(void);
void    Init_Switch_GPIO(void);
void    Timer_Init(void);
int16_t Calibrate_Joystick(void);

// Raw read: averaged ADC values, and the raw switch pin (0 = pressed)
void Read_Joystick(uint16_t *x_val, uint16_t *y_val, uint16_t *sw_val);

// Timing helpers (timer counts down, unsigned math handles wrap)
uint32_t TimerNow(void);
uint32_t MsSince(uint32_t startTick);

#endif
