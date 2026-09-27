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



//#define SCREEN_WIDTH  320
//#define SCREEN_HEIGHT 240

#define SQUARE_SIZE   20

//
// Draw a filled rectangle directly to the SSD2119
//
void DrawFilledRectangle(uint16_t x0,
                         uint16_t x1,
                         uint16_t y0,
                         uint16_t y1,
                         uint16_t color)
{
    uint32_t numPixels;
    uint32_t i;

    numPixels = ((uint32_t)(x1 - x0 + 1) *
                 (uint32_t)(y1 - y0 + 1));

    HAL_LCD_selectLCD();

    //
    // Set horizontal drawing region
    //
    HAL_LCD_writeCommand(SSD2119_H_RAM_START_REG);
    HAL_LCD_writeData(x0);

    HAL_LCD_writeCommand(SSD2119_H_RAM_END_REG);
    HAL_LCD_writeData(x1);

    //
    // Set vertical drawing region
    //
    HAL_LCD_writeCommand(SSD2119_V_RAM_POS_REG);
    HAL_LCD_writeData((y1 << 8) | y0);

    //
    // Set starting RAM address
    //
    HAL_LCD_writeCommand(SSD2119_X_RAM_ADDR_REG);
    HAL_LCD_writeData(x0);

    HAL_LCD_writeCommand(SSD2119_Y_RAM_ADDR_REG);
    HAL_LCD_writeData(y0);

    //
    // Start writing pixel data
    //
    HAL_LCD_writeCommand(SSD2119_RAM_DATA_REG);

    for(i = 0; i < numPixels; i++)
    {
        HAL_LCD_writeData(color);
    }

    HAL_LCD_deselectLCD();
}





void main(void)
{
#ifdef _RELEASE
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart,
           (Uint32)&RamfuncsLoadSize);
#endif

    //
    // Initialize system
    //
    InitSysCtrl();
    InitGpio();

    DINT;

    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    InitPieVectTable();

    //
    // Initialize LCD
    //
    Kitronix320x240x16_SSD2119Init();

    //
    // Clear screen
    //
    DrawFilledRectangle(0, 319, 0, 239, COLOR_BLACK);


    //
    // Variables
    //
    uint16_t x1 = 20;
    uint16_t y1 = 20;

    uint16_t x2 = 250;
    uint16_t y2 = 180;

    int16_t block_speed = 4;

    int16_t dx1 = block_speed;
    int16_t dy1 = block_speed;

    int16_t dx2 = -block_speed;
    int16_t dy2 = -block_speed;


    //
    // Draw initial squares
    //
    DrawFilledRectangle(x1,
                        x1 + SQUARE_SIZE - 1,
                        y1,
                        y1 + SQUARE_SIZE - 1,
                        COLOR_GREEN);

    DrawFilledRectangle(x2,
                        x2 + SQUARE_SIZE - 1,
                        y2,
                        y2 + SQUARE_SIZE - 1,
                        COLOR_PURPLE);


    //
    // Main animation loop
    //
    while(1)
    {
        //
        // Erase old green square
        //
        DrawFilledRectangle(x1,
                            x1 + SQUARE_SIZE - 1,
                            y1,
                            y1 + SQUARE_SIZE - 1,
                            COLOR_BLACK);

        //
        // Erase old purple square
        //
        DrawFilledRectangle(x2,
                            x2 + SQUARE_SIZE - 1,
                            y2,
                            y2 + SQUARE_SIZE - 1,
                            COLOR_BLACK);


        //
        // Move squares
        //
        x1 += dx1;
        y1 += dy1;

        x2 += dx2;
        y2 += dy2;


        //
        // Green square wall collisions
        //
        if(x1 == 0 || x1 >= 320 - SQUARE_SIZE)
        {
            dx1 = -dx1;
        }

        if(y1 == 0 || y1 >= 240 - SQUARE_SIZE)
        {
            dy1 = -dy1;
        }


        //
        // Purple square wall collisions
        //
        if(x2 == 0 || x2 >= 320 - SQUARE_SIZE)
        {
            dx2 = -dx2;
        }

        if(y2 == 0 || y2 >= 240 - SQUARE_SIZE)
        {
            dy2 = -dy2;
        }


        //
        // Square-to-square collision
        //
        if((x1 < x2 + SQUARE_SIZE) &&
           (x1 + SQUARE_SIZE > x2) &&
           (y1 < y2 + SQUARE_SIZE) &&
           (y1 + SQUARE_SIZE > y2))
        {
            dx1 = -dx1;
            dy1 = -dy1;

            dx2 = -dx2;
            dy2 = -dy2;
        }


        //
        // Draw new green square
        //
        DrawFilledRectangle(x1,
                            x1 + SQUARE_SIZE - 1,
                            y1,
                            y1 + SQUARE_SIZE - 1,
                            COLOR_GREEN);


        //
        // Draw new purple square
        //
        DrawFilledRectangle(x2,
                            x2 + SQUARE_SIZE - 1,
                            y2,
                            y2 + SQUARE_SIZE - 1,
                            COLOR_PURPLE);


        //
        // Animation delay
        //
        DELAY_US(10000);
    }
}
