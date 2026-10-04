//#############################################################################
// FILE:   app/button_test.c
// TITLE:  Button Diagnostics App - Graphical Square Visualization
//
// CONTROLS
//   any dpad button : lights up corresponding directional square
//   joystick button : lights up center joystick square
//   hold joy button : quit to main menu
//#############################################################################

#include "apps.h"

//*****************************************************************************
// 1. VISUAL LAYOUT CONSTANTS
//********************************################################*************
#define APP_LOOP_US        16000UL      // Loop frame rate (~60Hz execution pacing)

#define SQUARE_SIZE        40           // Dimensions of each block (40x40 pixels)
#define CENTER_X           159          // Screen center X (320 / 2)
#define CENTER_Y           125          // Screen center Y (roughly middle of play area)
#define OFFSET             50           // Pixel gap between center and outer boxes

// Component colors for active states
#define COLOR_ACTIVE_JOY   COLOR_WHITE
#define COLOR_ACTIVE_A     COLOR_RED
#define COLOR_ACTIVE_B     COLOR_TETRIS_S_GREEN
#define COLOR_ACTIVE_C     COLOR_TETRIS_I_CYAN
#define COLOR_ACTIVE_D     COLOR_TETRIS_O_YELLOW

//*****************************************************************************
// 2. LOCAL CACHE STATE (Updated to prevent stack corruption overflows)
//*****************************************************************************
// Define a dedicated array length specifically for this application's visual squares
#define TEST_APP_SQUARES   4

static uint16_t s_lastBtnState[TEST_APP_SQUARES];
static uint16_t s_lastJoyBtnState;

static void ResetTestCache(void)
{
    uint16_t i;
    for(i = 0; i < TEST_APP_SQUARES; i++)
    {
        s_lastBtnState[i] = 0xFFFF;     // Safely resets indices 0, 1, 2, and 3
    }
    s_lastJoyBtnState = 0xFFFF;
}


//*****************************************************************************
// 3. DRAWING & RENDERING ENGINE
//*****************************************************************************

// Draws the top layout architecture text elements
static void DrawTestLayout(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    // Main Header Layout
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"INPUT CONTROLLER TEST",
                                AUTO_STRING_LENGTH, CENTER_X, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 35, 309, 35);

    // Navigation Context Footer Hint
    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold joystick btn = exit",
                                AUTO_STRING_LENGTH, CENTER_X, 222, TRANSPARENT_TEXT);

    ResetTestCache();
}

// Computes bounding parameters and updates a square box only on state changes
static void RenderButtonSquare(int16_t cx, int16_t cy, uint16_t currentState, uint16_t* cachedState, uint32_t activeColor)
{
    if (currentState != *cachedState)
    {
        Graphics_Rectangle rect;
        rect.xMin = cx - (SQUARE_SIZE / 2);
        rect.xMax = cx + (SQUARE_SIZE / 2) - 1;
        rect.yMin = cy - (SQUARE_SIZE / 2);
        rect.yMax = cy + (SQUARE_SIZE / 2) - 1;

        if (currentState)
        {
            // Fill with active theme color when held down
            Graphics_FillRect(rect.xMin, rect.yMin, rect.xMax, rect.yMax, activeColor);
        }
        else
        {
            // Erase fill area back to true background layer
            Graphics_FillRect(rect.xMin, rect.yMin, rect.xMax, rect.yMax, COLOR_BLACK);
            
            // Draw a dark clean frame showing unengaged placeholder footprint
            Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
            Graphics_drawRectangle(&g_sContext, &rect);
        }
        
        *cachedState = currentState; // Commit change verification to local cache
    }
}

//*****************************************************************************
// 4. PUBLIC ENTRY POINT
//*****************************************************************************
void Button_Run(void)
{
    Buttons_Init();
    DrawTestLayout();

    while(1)
    {
        // ---- 1. HARDWARE VECTOR REGISTRATION ----
        Joy_Update();       // Scan primary thumbstick axes + onboard selector switch
        Buttons_Update();   // Scan external breadboard button array lines

        // Return condition map
        if (g_joy.btnLong)
        {
            return;
        }

        // ---- 2. GRAPHICAL MATRIX UPDATES ----
        // Center: Analog Joystick click element
        RenderButtonSquare(CENTER_X, CENTER_Y, g_joy.btn, &s_lastJoyBtnState, COLOR_ACTIVE_JOY);

        // D-Pad Configuration Ring (A=Up, B=Right, C=Down, D=Left)
        RenderButtonSquare(CENTER_X,          CENTER_Y - OFFSET, g_buttons[btnA].state, &s_lastBtnState[btnA], COLOR_ACTIVE_A);   // Top
        RenderButtonSquare(CENTER_X + OFFSET, CENTER_Y,          g_buttons[btnB].state, &s_lastBtnState[btnB], COLOR_ACTIVE_B);   // Right
        RenderButtonSquare(CENTER_X,          CENTER_Y + OFFSET, g_buttons[btnC].state, &s_lastBtnState[btnC], COLOR_ACTIVE_C);   // Bottom
        RenderButtonSquare(CENTER_X - OFFSET, CENTER_Y,          g_buttons[btnD].state, &s_lastBtnState[btnD], COLOR_ACTIVE_D);   // Left

        // ---- 3. RUNTIME CLOCK TIMING REGULATION ----
        DELAY_US(APP_LOOP_US);
    }
}
