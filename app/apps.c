//#############################################################################
// FILE:   app/apps.c
// TITLE:  The app layer's implementation of apps.h
//
//   1. Shared services    g_sContext, Graphics_FillRect, Rand_Seed/Rand16
//   2. App table          g_apps[] - what shows up in the menu
//   3. App runner         App_Launch(), App_Loop(): the ONE frame loop
//
//   App_Loop() -> RunApp(&g_menuApp)    menu calls App_Launch(chosen) and quits
//              -> RunApp(chosen)        app quits (hold button, or APP_QUIT)
//              -> RunApp(&g_menuApp)    ...
//#############################################################################

#include "apps.h"

//*****************************************************************************
// 1. Shared services
//*****************************************************************************
Graphics_Context g_sContext;

static uint32_t s_seed = 987654321UL;

void Rand_Seed(uint32_t seed)
{
    s_seed = seed;
}

// LCG ("Numerical Recipes"). Low bits are weak, so return the HIGH 16 bits.
uint16_t Rand16(void)
{
    s_seed = s_seed * 1664525UL + 1013904223UL;
    return (uint16_t)(s_seed >> 16);
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
// 2. App table
//    Order here = order in the menu. One pointer per app.
//*****************************************************************************
const App *const g_apps[] =
{
    &g_tetrisApp,           // app/tetris.c
    &g_raycasterApp,        // app/raycaster.c
    &g_settingsApp,         // app/settings.c
    &g_testApp,             // app/test.c
};


const uint16_t g_appCount = sizeof(g_apps) / sizeof(g_apps[0]);

//*****************************************************************************
// 3. App runner
//*****************************************************************************
static const App *s_next = NULL;            // set by App_Launch()

void App_Launch(const App *app)
{
    s_next = app;
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

// Runs one app until it quits.
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

// Menu -> app -> menu -> ... forever. Never returns.
void App_Loop(void)
{
    const App *app = &g_menuApp;

    while(1)
    {
        RunApp(app);
        WaitInputsReleased();

        // The menu sets s_next via App_Launch(). When an app ends on its own,
        // s_next is NULL and we fall back to the menu.
        app    = s_next ? s_next : &g_menuApp;
        s_next = NULL;
    }
}
