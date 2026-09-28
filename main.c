//*****************************************************************************
//
// main_rectangle_test.c - Bring-up test for the TMS320F28069 +
//     BOOSTXL-K350QVG-S1 (Kitronix 320x240 SSD2119) port with Collision Counter.
//
//*****************************************************************************

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "grlib.h"
#include <stdio.h>  // Required for sprintf

#define SQUARE_SIZE   20

// Graphic library context
Graphics_Context g_sContext;

void main(void)
{
#ifdef _RELEASE
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart,
           (Uint32)&RamfuncsLoadSize);
#endif

    // Initialize system
    InitSysCtrl();
    InitGpio();

    DINT;
    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    InitPieVectTable();

    // Initialize LCD Hardware and Bind Graphics Context
    Kitronix320x240x16_SSD2119Init();
    Graphics_initContext(&g_sContext, &g_sKitronix320x240x16_SSD2119);
    
    // Draw Static Game Header Board Layout
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"TETRIS",
                                AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);

    Graphics_drawLine(&g_sContext, 10, 30, 309, 30);
    
    // Counter variables
    uint16_t collision_count = 0;
    char score_buffer[20]; // Buffer to store the formatted text string

    // Setup bounding variables for rectangles
    uint16_t x1 = 20;
    uint16_t y1 = 40; 

    uint16_t x2 = 250;
    uint16_t y2 = 180;

    int16_t block_speed = 4;

    int16_t dx1 = block_speed;
    int16_t dy1 = block_speed;

    int16_t dx2 = -block_speed;
    int16_t dy2 = -block_speed;

    Graphics_Rectangle box1;
    Graphics_Rectangle box2;

    // Draw initial text display
    sprintf(score_buffer, "Collisions: %u", collision_count);
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawString(&g_sContext, (int16_t *)score_buffer, AUTO_STRING_LENGTH, 15, 215, OPAQUE_TEXT);

    // Draw initial squares
    box1.xMin = x1; box1.xMax = x1 + SQUARE_SIZE - 1;
    box1.yMin = y1; box1.yMax = y1 + SQUARE_SIZE - 1;
    Graphics_setForegroundColor(&g_sContext, COLOR_GREEN);
    Graphics_fillRectangle(&g_sContext, &box1);

    box2.xMin = x2; box2.xMax = x2 + SQUARE_SIZE - 1;
    box2.yMin = y2; box2.yMax = y2 + SQUARE_SIZE - 1;
    Graphics_setForegroundColor(&g_sContext, COLOR_PURPLE);
    Graphics_fillRectangle(&g_sContext, &box2);

    // Main animation loop
    while(1)
    {
        // 1. Erase old squares by filling them with the background color
        Graphics_setForegroundColor(&g_sContext, COLOR_BLACK);
        
        box1.xMin = x1; box1.xMax = x1 + SQUARE_SIZE - 1;
        box1.yMin = y1; box1.yMax = y1 + SQUARE_SIZE - 1;
        Graphics_fillRectangle(&g_sContext, &box1);

        box2.xMin = x2; box2.xMax = x2 + SQUARE_SIZE - 1;
        box2.yMin = y2; box2.yMax = y2 + SQUARE_SIZE - 1;
        Graphics_fillRectangle(&g_sContext, &box2);

        // 2. Compute motion step increments
        x1 += dx1;
        y1 += dy1;

        x2 += dx2;
        y2 += dy2;

        // 3. Green square wall collisions (Keeping inside Y bounds 35 to 200 to clear counter text)
        if(x1 <= 0 || x1 >= 320 - SQUARE_SIZE)
        {
            dx1 = -dx1;
        }
        if(y1 <= 35 || y1 >= 210 - SQUARE_SIZE)
        {
            dy1 = -dy1;
        }

        // 4. Purple square wall collisions
        if(x2 <= 0 || x2 >= 320 - SQUARE_SIZE)
        {
            dx2 = -dx2;
        }
        if(y2 <= 35 || y2 >= 210 - SQUARE_SIZE)
        {
            dy2 = -dy2;
        }

        // 5. Square-to-square collision vector swaps
        if((x1 < x2 + SQUARE_SIZE) &&
           (x1 + SQUARE_SIZE > x2) &&
           (y1 < y2 + SQUARE_SIZE) &&
           (y1 + SQUARE_SIZE > y2))
        {
            dx1 = -dx1; dy1 = -dy1;
            dx2 = -dx2; dy2 = -dy2;

            // Increment count on true intersection impact
            collision_count++;

            // Update the live text overlay string array
            sprintf(score_buffer, "Collisions: %u ", collision_count);
            Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
            Graphics_drawString(&g_sContext, (int16_t *)score_buffer, AUTO_STRING_LENGTH, 15, 215, OPAQUE_TEXT);
        }

        // 6. Draw new Green square
        box1.xMin = x1; box1.xMax = x1 + SQUARE_SIZE - 1;
        box1.yMin = y1; box1.yMax = y1 + SQUARE_SIZE - 1;
        Graphics_setForegroundColor(&g_sContext, COLOR_GREEN);
        Graphics_fillRectangle(&g_sContext, &box1);

        // 7. Draw new Purple square
        box2.xMin = x2; box2.xMax = x2 + SQUARE_SIZE - 1;
        box2.yMin = y2; box2.yMax = y2 + SQUARE_SIZE - 1;
        Graphics_setForegroundColor(&g_sContext, COLOR_PURPLE);
        Graphics_fillRectangle(&g_sContext, &box2);

        // Animation delay framework
        DELAY_US(10000);
    }
}
