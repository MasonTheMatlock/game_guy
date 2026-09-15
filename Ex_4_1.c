//*****************************************************************************
//
// main_rectangle_test.c - Bring-up test for the TMS320F28069 +
//     BOOSTXL-K350QVG-S1 (Kitronix 320x240 SSD2119) port.
//
// This deliberately talks to the SSD2119 registers directly (through the
// HAL_LCD_* primitives) rather than through grlib, so a successful test
// proves the SPI wiring, HAL, and panel init sequence are correct before
// any grlib font/shape assumptions are layered on top. Once you see a
// colored rectangle on a black screen, swap this out for
// Graphics_fillRectangle() via grlib -- Kitronix320x240x16_SSD2119Init()
// and the HAL underneath it are exactly what grlib's display driver calls.
//
//*****************************************************************************

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"

// Colors in the panel's native 5-6-5 RGB format.
#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF

//*****************************************************************************
//
//! Fills the rectangle [x0,x1] x [y0,y1] (inclusive) with a solid color by
//! setting the SSD2119's GRAM address window and streaming pixel data.
//! Coordinates are in the raw SSD2119 X/Y RAM address space (not yet run
//! through the MAPPED_X/MAPPED_Y orientation macros -- that mapping is what
//! Kitronix320x240x16_SSD2119PixelDraw()/RectFill() do in the real driver).
//
//*****************************************************************************
static void TestFillRect(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1,
                          uint16_t color)
{
    uint32_t numPixels = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);
    uint32_t i;

    HAL_LCD_selectLCD();

    //
    // Constrain the auto-increment window to the rectangle.
    //
    HAL_LCD_writeCommand(SSD2119_H_RAM_START_REG);
    HAL_LCD_writeData(x0);
    HAL_LCD_writeCommand(SSD2119_H_RAM_END_REG);
    HAL_LCD_writeData(x1);
    HAL_LCD_writeCommand(SSD2119_V_RAM_POS_REG);
    HAL_LCD_writeData((y1 << 8) | y0);

    //
    // Move the cursor to the top-left of the window.
    //
    HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
    HAL_LCD_writeData(x0);
    HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
    HAL_LCD_writeData(y0);

    //
    // Stream the fill color; the controller auto-increments X (wrapping to
    // the next Y) within the window we just set.
    //
    HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);
    for(i = 0; i < numPixels; i++)
    {
        HAL_LCD_writeData(color);
    }

    //
    // Restore the full-screen GRAM window so later code (e.g. grlib) isn't
    // surprised by a restricted window left over from this test.
    //
    HAL_LCD_writeCommand(SSD2119_H_RAM_START_REG);
    HAL_LCD_writeData(0x0000);
    HAL_LCD_writeCommand(SSD2119_H_RAM_END_REG);
    HAL_LCD_writeData(LCD_HORIZONTAL_MAX - 1);
    HAL_LCD_writeCommand(SSD2119_V_RAM_POS_REG);
    HAL_LCD_writeData((uint16_t)(LCD_VERTICAL_MAX - 1) << 8);

    HAL_LCD_deselectLCD();
}

void main(void)
{
    //
    // Standard F2806x device init (from the C2000Ware common template):
    // watchdog, PLL/SYSCLKOUT, peripheral clocks, all GPIO to a known
    // (input, GPIO) default state, PIE vector table.
    //
    InitSysCtrl();
    InitGpio();
    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    //
    // Bring up the panel. This calls HAL_LCD_initLCD() (which configures
    // GPIO16/18 for SPI-A, GPIO17/19/44/3 as GPIO, and the SPI-A peripheral
    // itself), resets the SSD2119, and clears the screen to black.
    //
    Kitronix320x240x16_SSD2119Init();

    //
    // Draw a red test rectangle roughly in the middle of the 320x240
    // panel, in raw controller coordinates.
    //
    TestFillRect(60, 259, 70, 169, COLOR_RED);

    for(;;)
    {
        //
        // Bring-up test only -- nothing else to do.
        //
    }
}
