//*****************************************************************************
//
// HAL_F28069_KITRONIX320X240_SSD2119_SPI.h - Prototypes and pin/peripheral
//     configuration for driving the Kitronix320x240x16 SSD2119 BoosterPack
//     (BOOSTXL-K350QVG-S1) plugged directly into a LAUNCHXL-F28069M, over
//     SPI-A, 8-bit, 4-wire (SIMO / CLK / CS / D-C), with GPIO reset and
//     backlight control.
//
// Ported from TI's HAL_MSP_EXP430F5529LP_KITRONIX320X240_SSD2119_SPI HAL.
// The upper display driver (kitronix320x240x16_ssd2119_spi.c) is unchanged.
//
//   BoosterPack pin   LCD signal    LaunchPad GPIO        Mode
//   ---------------   ----------    ------------------    -------------------
//   J1.7              LCD_SCL       GPIO18 (SPICLKA)      peripheral, mux 1
//   J2.15             LCD_SDI       GPIO16 (SPISIMOA)     peripheral, mux 1
//   J1.8              LCD_SDC       GPIO22                GPIO out (D/C)
//   J2.13             LCD_SCS       GPIO50                GPIO out (CS)
//   J4.32             LCD_RST       LCD_RESET_GPIO below  GPIO out (see note)
//   J4.40             LED on/off    GPIO0                 GPIO out (backlight)
//
// NOTE (reset): on the LAUNCHXL-F28069M, J4.32 is the "DAC1" net, a filtered
// PWM output (1k + 1nF RC), not a bare GPIO. TI forum posts say the DAC
// filters sit on ePWM7/8, i.e. GPIO40..43. DAC1 = GPIO40 is a best guess:
// verify it on the LaunchPad schematic (or with a meter on J4.32 while you
// toggle GPIO40..43) and change LCD_RESET_GPIO if needed. The RC filter is
// fine for a 1 ms reset pulse. Must be in the range 32..47 for the .c file.
//
//*****************************************************************************

#ifndef __HAL_F28069_KITRONIX320X240_SSD2119_SPI_H__
#define __HAL_F28069_KITRONIX320X240_SSD2119_SPI_H__

#include "F2806x_Device.h"

#ifdef __cplusplus
extern "C" {
#endif

// SYSCLKOUT of the F28069 as configured by InitSysCtrl() (Hz).
#define HAL_LCD_SYSCLKOUT_FREQUENCY   90000000UL

// LSPCLK = SYSCLKOUT / 4 by default on F2806x.
#define HAL_LCD_LSPCLK_FREQUENCY      (HAL_LCD_SYSCLKOUT_FREQUENCY / 4UL)

// SPI-A bit rate divider. Baud = LSPCLK / (SPIBRR + 1) for SPIBRR = 3..127.
#define HAL_LCD_SPIBRR                3

// GPIO pin assignments (direct-plug BoosterPack wiring)
#define LCD_CLK_GPIO        18      // J1.7   SPICLKA  (mux = 1)
#define LCD_SIMO_GPIO       16      // J2.15  SPISIMOA (mux = 1)
#define LCD_SDC_GPIO        22      // J1.8   GPIO output, data/command
#define LCD_SCS_GPIO        50      // J2.13  GPIO output, chip select (active low)
#define LCD_RESET_GPIO      40      // J4.32  (DAC1 net) GPIO output, reset (active low) -- VERIFY
#define LCD_PWM_GPIO        0       // J4.40  GPIO output, backlight on/off

extern void InitLCDInterfaceSPI(void);
extern void HAL_LCD_initLCD(void);
extern void HAL_LCD_writeCommand(Uint16 command);
extern void HAL_LCD_writeData(Uint16 data);
extern void HAL_LCD_delay30ms(void);
extern void HAL_LCD_selectLCD(void);
extern void HAL_LCD_deselectLCD(void);

#ifdef __cplusplus
}
#endif

#endif // __HAL_F28069_KITRONIX320X240_SSD2119_SPI_H__
