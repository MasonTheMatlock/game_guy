//###########################################################################
// FILE:   main.c
// TITLE:  Calibrated Joystick Moving Square - White Flash on J2-11 (GPIO55)
//###########################################################################

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "grlib/grlib.h"
#include <stdio.h>
#include "drivers/drivers.h"

#define SQUARE_SIZE   20



// Graphic library context
Graphics_Context g_sContext;



// Function Prototypes
void Init_Joystick_ADC(void);
void Init_Switch_GPIO(void);
void Read_Joystick(uint16_t *x_val, uint16_t *y_val, uint16_t *sw_val);
void Calibrate_Joystick(void);

void main(void)
{
#ifdef _RELEASE
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart,
           (Uint32)&RamfuncsLoadSize);
#endif

    // Step 1. Initialize system control and default GPIOs
    InitSysCtrl();
    InitGpio();

    DINT;
    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    InitPieVectTable();

    Init_Switch_GPIO();  // Setup J2-11 / GPIO55
    Init_Joystick_ADC(); // Setup J7-63 (ADCINB7) and J7-64 (ADCINB4)

    Calibrate_Joystick();

    
    Kitronix320x240x16_SSD2119Init();
    Graphics_initContext(&g_sContext, &g_sKitronix320x240x16_SSD2119);
    
    // Draw Static Dashboard Header Layout
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCm20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"SQUARE FLASH TEST",
                                AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);

    Graphics_drawLine(&g_sContext, 10, 30, 309, 30);
    
    // Coordinate tracking variables (Centered initially)
    int16_t x1 = (LCD_HORIZONTAL_MAX - SQUARE_SIZE) / 2;
    int16_t y1 = (LCD_VERTICAL_MAX - SQUARE_SIZE) / 2; 
    int16_t prev_x = x1;
    int16_t prev_y = y1;

    // Track historical switch state to check for color changes while stationary
    uint16_t prev_sw = 1; 

    Graphics_Rectangle box;

    uint16_t raw_x = 2048;
    uint16_t raw_y = 2048;
    uint16_t raw_sw = 1;

    // Draw initial active box frame in Green
    box.xMin = x1; box.xMax = x1 + SQUARE_SIZE - 1;
    box.yMin = y1; box.yMax = y1 + SQUARE_SIZE - 1;
    Graphics_setForegroundColor(&g_sContext, COLOR_GREEN);
    Graphics_fillRectangle(&g_sContext, &box);

    // Main animation loop
    while(1)
    {
        // 1. Read joystick hardware position and bank B switch vectors
        Read_Joystick(&raw_x, &raw_y, &raw_sw);

        // 2. Compute motion step increments from analog deviations
        if (raw_x < (center_x - JOY_DEADZONE)) {
            x1 -= 3; // Move Left
        } else if (raw_x > (center_x + JOY_DEADZONE)) {
            x1 += 3; // Move Right
        }

        // Corrected Y-axis inversion mapping
        if (raw_y < (center_y - JOY_DEADZONE)) {
            y1 -= 3; // Move Up
        } else if (raw_y > (center_y + JOY_DEADZONE)) {
            y1 += 3; // Move Down
        }

        // 3. Keep within physical constraint safety margins (Clears header board line)
        if (x1 < 0) x1 = 0;
        if (x1 > (LCD_HORIZONTAL_MAX - SQUARE_SIZE)) x1 = LCD_HORIZONTAL_MAX - SQUARE_SIZE;
        if (y1 < 35) y1 = 35;
        if (y1 > (LCD_VERTICAL_MAX - SQUARE_SIZE)) y1 = LCD_VERTICAL_MAX - SQUARE_SIZE;

        // Determine current targeted fill color parameter based on button state (Active Low)
        uint32_t active_color = (raw_sw == 0) ? COLOR_WHITE : COLOR_GREEN;

        // 4. Perform visual screen updates if positioning OR button state modified
        if ((x1 != prev_x) || (y1 != prev_y))
        {
            // Erase the old square trace frame with a black rectangle override
            Graphics_setForegroundColor(&g_sContext, COLOR_BLACK);
            box.xMin = prev_x; box.xMax = prev_x + SQUARE_SIZE - 1;
            box.yMin = prev_y; box.yMax = prev_y + SQUARE_SIZE - 1;
            Graphics_fillRectangle(&g_sContext, &box);

            // Draw new square positioning frame
            Graphics_setForegroundColor(&g_sContext, active_color);
            box.xMin = x1; box.xMax = x1 + SQUARE_SIZE - 1;
            box.yMin = y1; box.yMax = y1 + SQUARE_SIZE - 1;
            Graphics_fillRectangle(&g_sContext, &box);

            // Update cached tracking states
            prev_x = x1;
            prev_y = y1;
            prev_sw = raw_sw;
        }
        else if (raw_sw != prev_sw)
        {
            // Force color shift redraw immediately when holding button still
            Graphics_setForegroundColor(&g_sContext, active_color);
            box.xMin = x1; box.xMax = x1 + SQUARE_SIZE - 1;
            box.yMin = y1; box.yMax = y1 + SQUARE_SIZE - 1;
            Graphics_fillRectangle(&g_sContext, &box);

            prev_sw = raw_sw;
        }

        // Loop Throttling TBD what value is best
        DELAY_US(16666); // ~60Hz tracking updates
    }
}

