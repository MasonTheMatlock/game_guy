//#############################################################################
// FILE:   main.c
// TITLE:  game_guy - hardware boot, then hand off to the app loop
//
//   Board : TMS320F28069 (C2000 Piccolo)
//   Screen: BOOSTXL-K350QVG-S1, 320x240 Kitronix SSD2119 LCD over SPI
//   Input : analog joystick (VRx = ADCINB7, VRy = ADCINB4, button = GPIO55)
//           4-button D-pad  (A = GPIO25, B = GPIO52, C = GPIO53, D = GPIO56)
//
// main.c only boots the hardware. Everything above that lives in app/:
//   app/apps.h / apps.c   the app contract, shared services (g_sContext,
//                         Graphics_FillRect, Rand16), g_apps[], and
//                         App_Loop(): the one frame loop (menu -> app -> menu)
//   app/main_menu.h / .c  the menu
//   app/<name>.h / .c     one pair per app
//#############################################################################

#include <string.h>                         // memcpy (flash -> RAM, Release)
#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "grlib/grlib.h"
<<<<<<< HEAD
#include "drivers/drivers.h"                // joystick + timer driver
#include "app/apps.h"                       // app contract + shared services


// grlib's handle to the LCD. Initialized in main() after the display is up.
Graphics_Context g_sContext;

// Random number generator state. Private to this file: apps can only reach it
//through Rand_Seed() / Rand16()
static uint32_t g_seed = 987654321UL;

void Rand_Seed(uint32_t seed)
{
    g_seed = seed;
}

// Linear congruential generator (the classic "Numerical Recipes" constants).
// The low bits of an LCG are not very random, so we return the HIGH 16 bits.
uint16_t Rand16(void)
{
    g_seed = g_seed * 1664525UL + 1013904223UL;
    return (uint16_t)(g_seed >> 16);
}

void Graphics_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color)
{
    Graphics_Rectangle r;

    if(y1 < y0 || x1 < x0) return;          // empty rectangle: nothing to draw

    r.xMin = x0;
    r.yMin = y0;
    r.xMax = x1;
    r.yMax = y1;
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_Graphics_FillRectangle(&g_sContext, &r);
}

//*****************************************************************************
// App table
//   The menu is generated from this table, and main() launches apps through
//   the .run pointers. To add an app: write its Xxx_Run(), declare it in
//   app/apps.h, then add ONE line here. The menu and the main loop do not
//   change.
//
//   The menu is a scrolling tile carousel, so there is no limit on the number
//   of entries. The tile icon is the first letter of .name.
//*****************************************************************************
const AppInfo g_apps[] =
{
    //  name          control hint               button color   entry point
    { "TETRIS",     "Btn: rotate  Up: drop",     COLOR_TETRIS_T_PURPLE,      Tetris_Run    },
    { "RAYCASTER",  "Stick: walk  Btn: fire",    COLOR_GREEN,      Raycaster_Run },
    { "SETTINGS",  "Stick: move Btn: select",    COLOR_GRAY,      Raycaster_Run },
};
/*
    To DO: control hint struct instead of string.
    *Pasted from Discord:
    enum ControlAction {
    DROP,
    ROTATE,
    WALK,
    FIRE,
    MOVE,
    SELECT
};

typedef struct ControlHint {
    ControlAction btn;
    ControlAction stick;
    ControlAction up;
} ControlHint;

const char* ControlHint_toString(ControlHint ctrlHint)
{
    // Convert to string here
}

    This concept will be beneficial for devolping new app modules, more efficent user inputs and configurations?!
    Also good for design concepts.
*/
// Computed from the table, so it can never get out of sync with it.
const uint16_t g_appCount = sizeof(g_apps) / sizeof(g_apps[0]);
=======
#include "drivers/drivers.h"
#include "app/apps.h"
>>>>>>> header-house-keeping

//*****************************************************************************
// Boot helpers
//*****************************************************************************
#define WARNING_HOLD_MS   2000

// Joystick_Init() returns 0 if calibration looked wrong. Say so on screen
// instead of failing silently.
//Also a good illustration of how to use inputs to output to the screen.
static void ShowCalibrationWarning(void)
{
    uint16_t i;

    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_RED);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"JOYSTICK CALIBRATION",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 90, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"FAILED - CHECK STICK",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 120, OPAQUE_TEXT);

    for(i = 0; i < WARNING_HOLD_MS / 10; i++) DELAY_US(10000);
}

//*****************************************************************************
// main
//*****************************************************************************
void main(void)
{
    int16_t joystickOk;                     

#ifdef _RELEASE
    // Release (flash) build: copy time-critical code to RAM first.
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart,
           (Uint32)&RamfuncsLoadSize);
#endif

    // Step 1. Clocks, PLL, watchdog, GPIO.
    InitSysCtrl();
    InitGpio();

    // Step 2. Interrupts off, default vector table installed.
    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    // Step 3. Joystick before LCD (order proven on this board). Keep the
    // stick untouched at power-up.
    joystickOk = Joystick_Init();
    Dpad_Init();                            // uses the joystick's timer

    // Step 4. LCD + graphics context.
    Kitronix320x240x16_SSD2119Init();
    Graphics_initContext(&g_sContext, &g_sKitronix320x240x16_SSD2119);

    if(!joystickOk) ShowCalibrationWarning();

    // Step 5. Menu -> app -> menu -> ... (never returns)
    App_Loop();
}