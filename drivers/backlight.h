//#############################################################################
// FILE:   drivers/backlight.h
// TITLE:  LCD backlight brightness (hardware PWM from an ePWM output)
//
//   The backlight pin must be an ePWM pin. On the LAUNCHXL-F28069M:
//       J4-37 = GPIO3 = ePWM2B        (the HAL's original backlight pin)
//       J4-40 = GPIO0 = ePWM1A
//   Set BACKLIGHT_PIN below to match where the wire actually goes.
//   (J4-34 is GPIO13, which is NOT an ePWM pin: TZ2 / SPISOMIB only.)
//
//   RESOURCES THIS DRIVER OWNS
//     J4-37 build: GPIO3 + ePWM2.      J4-40 build: GPIO0 + ePWM1.
//
//   No interrupt: the ePWM peripheral makes the PWM, the CPU only writes the
//   compare register when the level changes.
//
//   Usage:
//     Backlight_Init();                 // main(), next to Buzzer_Init()
//     Backlight_SetLevel(3);            // app layer
//#############################################################################

#ifndef __BACKLIGHT_H__
#define __BACKLIGHT_H__

#include <stdint.h>

//*****************************************************************************
// Where is the backlight wire? Pick ONE.
//*****************************************************************************
#define BACKLIGHT_PIN_J4_37     0       // GPIO3 / ePWM2B
#define BACKLIGHT_PIN_J4_40     1       // GPIO0 / ePWM1A

#define BACKLIGHT_PIN           BACKLIGHT_PIN_J4_40

// Number of brightness steps. Level 0 is dim (never fully off, so the screen
// can't be lost); the top level is full brightness.
#define BACKLIGHT_NUM_LEVELS    8

// Call once from main(), after InitGpio() and the LCD init. Muxes the pin to
// the ePWM and starts it. No interrupt is involved.
void     Backlight_Init(void);

void     Backlight_SetLevel(uint16_t level);    // 0..BACKLIGHT_NUM_LEVELS-1
uint16_t Backlight_GetLevel(void);

#endif // __BACKLIGHT_H__
