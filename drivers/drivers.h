#ifndef __DRIVERS_H__
#define __DRIVERS_H__

#include <stdint.h>
#include <stdbool.h>
#include "F2806x_Device.h"
#include "F2806x_Examples.h"

// Joystick Filtering Parameters
#define JOY_DEADZONE  750

// Global Hardware Calibration Trackers (Shared across files)
extern volatile uint16_t center_x;
extern volatile uint16_t center_y;

// Joystick Prototypes
void Init_Joystick_ADC(void);
void Init_Switch_GPIO(void);
void Read_Joystick(uint16_t *x_val, uint16_t *y_val, uint16_t *sw_val);
void Calibrate_Joystick(void);

#endif // __DRIVERS_H__
