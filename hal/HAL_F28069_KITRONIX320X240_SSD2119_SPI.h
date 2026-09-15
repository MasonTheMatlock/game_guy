//*****************************************************************************
//
// HAL_F28069_KITRONIX320X240_SSD2119_SPI.h - Prototypes and pin/peripheral
//     configuration for driving the Kitronix320x240x16 SSD2119 BoosterPack
//     from a TMS320F28069 (F2806x) over the SPI-A peripheral, 8-bit,
//     4-wire (SIMO / CLK / CS / D-C), with GPIO reset and backlight control.
//
// Ported from TI's HAL_MSP_EXP430F5529LP_KITRONIX320X240_SSD2119_SPI HAL.
// The upper display driver (kitronix320x240x16_ssd2119_spi.c) is unchanged;
// it only calls the HAL_LCD_* functions implemented here, so this file (and
// its .c) is the only thing that needs to be written per-MCU.
//
//                       TMS320F28069                 BOOSTXL-K350QVG-S1
//                      -----------------              ------------
//              GPIO16 |     SPISIMOA    |----------> |LCD_SDI     |
//              GPIO18 |     SPICLKA     |----------> |LCD_SCL     |
//              GPIO19 |     GPIO (CS)   |----------> |LCD_SCS     |
//              GPIO17 |     GPIO (D/C)  |----------> |LCD_SDC     |
//              GPIO44 |     GPIO (RST)  |----------> |LCD_RESET   |
//              GPIO3  |     GPIO (BKLT) |----------> |LCD_PWM     |
//                      -----------------              ------------
//
// NOTE: SPISOMIA (GPIO17 alt. function) and SPISTEA (GPIO19 alt. function)
// are intentionally NOT used. D/C and CS are driven as plain GPIO outputs
// so the driver can toggle D/C between command/data bytes and hold CS
// asserted across multi-byte transfers, which the SSD2119 requires.
//
//*****************************************************************************

#ifndef __HAL_F28069_KITRONIX320X240_SSD2119_SPI_H__
#define __HAL_F28069_KITRONIX320X240_SSD2119_SPI_H__

#include "F2806x_Device.h"

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
//
// User configuration for the LCD HAL
//
//*****************************************************************************

// SYSCLKOUT of the F28069 as configured by InitSysCtrl() (Hz). Update this
// if your project uses a different PLL configuration than the default
// C2000Ware F2806x template (90 MHz).
#define HAL_LCD_SYSCLKOUT_FREQUENCY   90000000UL

// LSPCLK = SYSCLKOUT / 4 by default on F2806x (LOSPCP default = 0x2).
#define HAL_LCD_LSPCLK_FREQUENCY      (HAL_LCD_SYSCLKOUT_FREQUENCY / 4UL)

// SPI-A bit rate divider. Baud = LSPCLK / (SPIBRR + 1) for SPIBRR = 3..127.
// SPIBRR = 3 -> ~5.6 MHz with a 90 MHz SYSCLKOUT/22.5 MHz LSPCLK. This is a
// conservative starting point to bring the panel up; raise it once the
// rectangle test is confirmed working (SSD2119 will run considerably faster).
#define HAL_LCD_SPIBRR                3

// GPIO pin assignments (per the board wiring given by the user)
#define LCD_SIMO_GPIO       16      // SPISIMOA (mux = 1)
#define LCD_CLK_GPIO        18      // SPICLKA  (mux = 1)
#define LCD_SCS_GPIO        19      // GPIO output, chip select (active low)
#define LCD_SDC_GPIO        17      // GPIO output, data/command select
#define LCD_RESET_GPIO      44      // GPIO output, panel reset (active low)
#define LCD_PWM_GPIO        3       // GPIO output, backlight enable

//*****************************************************************************
//
// Prototypes for the globals exported by this HAL.
//
//*****************************************************************************
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
