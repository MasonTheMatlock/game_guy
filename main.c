//#############################################################################
// FILE:   main.c
// TITLE:  game_guy - boot, shared services, and the ONE app loop
//
//   Board : TMS320F28069 (C2000 Piccolo)
//   Screen: BOOSTXL-K350QVG-S1, 320x240 Kitronix SSD2119 LCD over SPI
//   Input : analog joystick (VRx = ADCINB7, VRy = ADCINB4, button = GPIO55)
//           4-button D-pad  (A = GPIO25, B = GPIO52, C = GPIO53, D = GPIO56)
//
// main.c is responsible for:
//   1. Booting the hardware.
//   2. Shared services (Graphics_FillRect, Rand16).
//   3. The app table g_apps[].
//   4. RunApp(): the only frame loop in the program. Every app (including
//      the menu) is just init/update/render callbacks that this loop drives.
//
//   main -> RunApp(&g_menuApp)    menu calls App_Launch(chosen) and quits
//        -> RunApp(chosen)        app quits (hold button, or APP_QUIT)
//        -> RunApp(&g_menuApp)    ...
//#############################################################################

#include <string.h>                         // memcpy (flash -> RAM, Release)
#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "grlib/grlib.h"
#include "drivers/drivers.h"
#include "app/apps.h"

Graphics_Context g_sContext;

//*****************************************************************************
// Shared services
//*****************************************************************************
static uint32_t g_seed = 987654321UL;

void Rand_Seed(uint32_t seed)
{
    g_seed = seed;
}

// LCG ("Numerical Recipes"). Low bits are weak, so return the HIGH 16 bits.
uint16_t Rand16(void)
{
    g_seed = g_seed * 1664525UL + 1013904223UL;
    return (uint16_t)(g_seed >> 16);
}

void Graphics_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color)
{
    Graphics_Rectangle r;

    if(y1 < y0 || x1 < x0) return;

    r.xMin = x0;
    r.yMin = y0;
    r.xMax = x1;
    r.yMax = y1;
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_Graphics_FillRectangle(&g_sContext, &r);
}

//*****************************************************************************
// App table
//
// LEGACY apps (blocking Xxx_Run loop) are wrapped here with .run set and the
// callbacks NULL. When you convert an app, move its descriptor into its own
// .c file as   const App g_xxxApp = {...};   and delete the line below.
//
//                 name         hint                        color                  frameUs init  upd   rend  exit  run
//*****************************************************************************
//static const App s_tetris    = { "TETRIS",    "Btn: rotate  Up: drop",  COLOR_TETRIS_T_PURPLE, 0, NULL, NULL, NULL, NULL, Tetris_Run    };
//static const App s_raycaster = { "RAYCASTER", "Stick: walk  Btn: fire", COLOR_GREEN,           0, NULL, NULL, NULL, NULL, Raycaster_Run };
//static const App s_settings  = { "SETTINGS",  "Stick: move Btn: select", COLOR_GRAY,           0, NULL, NULL, NULL, NULL, Raycaster_Run }; // placeholder


// Order here = order in the menu. One pointer per app.
const App *const g_apps[] =
{
    &g_tetrisApp,              // legacy wrapper (until tetris.c is converted)
    &g_raycasterApp,        // app/raycaster.c
    //&g_settings,         // app/settings.c
    &g_testApp,             // app/test.c
};

const uint16_t g_appCount = sizeof(g_apps) / sizeof(g_apps[0]);

//*****************************************************************************
// App runner
//*****************************************************************************
static const App *g_next = NULL;            // set by App_Launch()

void App_Launch(const App *app)
{
    g_next = app;
}

// Wait until the joystick button AND all four D-pad buttons are released, so
// the press that ended one screen is not read as the next screen's input.
static void WaitInputsReleased(void)
{
    Joy_WaitRelease();

    do
    {
        Dpad_Update();
        DELAY_US(5000);
    } while(g_dpad.a || g_dpad.b || g_dpad.c || g_dpad.d);
}

// Runs one app until it quits. This is the only frame loop in the program.
static void RunApp(const App *app)
{
    // Legacy app: it owns its own loop, so just call it.
    if(app->run)
    {
        app->run();
        return;
    }

    app->init();

    while(1)
    {
        Joy_Update();                       // input is read ONCE per frame,
        Dpad_Update();                      // here, never inside apps

        // Universal "hold button to quit". The menu has nothing to quit to.
        if(app != &g_menuApp && g_joy.btnLong) break;

        if(app->update() == APP_QUIT) break;

        app->render();
        DELAY_US(app->frameUs);
    }

    if(app->exit) app->exit();
}

//*****************************************************************************
// Boot helpers
//*****************************************************************************
#define WARNING_HOLD_MS   2000

// Joystick_Init() returns 0 if calibration looked wrong. Say so on screen
// instead of failing silently.
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
    // Declarations first (C89-safe), before any statements.
    int16_t     joystickOk;
    const App  *app;

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

    // Step 5. Menu -> app -> menu -> ...
    app = &g_menuApp;

    while(1)
    {
        RunApp(app);
        WaitInputsReleased();

        // The menu sets g_next via App_Launch(). When an app ends on its own,
        // g_next is NULL and we fall back to the menu.
        app    = g_next ? g_next : &g_menuApp;
        g_next = NULL;
    }
}
