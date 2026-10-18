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

// Play a SHORT effect and wait for it, so the next screen's Buzzer_Stop()
// doesn't cut it off. Capped at ~1 s so a driver hiccup can't hang the app.
void Sfx_PlayWait(const Song *sfx)
{
    uint16_t guard = 1000;

    Buzzer_PlaySfx(sfx);
    while(Buzzer_IsPlaying() && guard--)
        DELAY_US(1000);
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
    } while(g_dpad.u || g_dpad.l || g_dpad.r || g_dpad.d);
}

static void RunApp(const App *app);

// Runs Settings on top of `app`. Returns true to resume the app, false to
// leave it for the main menu.
static bool PauseMenu(const App *app)
{
    WaitInputsReleased();               // the long press that got us here
    Sfx_PlayWait(&sfx_pause);               // <-- new
    g_settingsOverlay = true;
    g_settingsToMenu  = false;
    RunApp(&g_settingsApp);
    g_settingsOverlay = false;

    WaitInputsReleased();
    if(g_settingsToMenu) return false;

    Sfx_PlayWait(&sfx_resume);              // <-- new
    Buzzer_Stop();
    if(app->redraw) app->redraw();      // keep state, repaint screen
    else            app->init();        // fallback: restarts the app
    return true;
}

// Runs one app until it quits.
static void RunApp(const App *app)
{
    Buzzer_Stop();                          // start silent; init() starts its own music

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

        // Hold the button: Settings overlay (any app except the menu).
        // Inside Settings itself, a hold just closes it.
        if(app != &g_menuApp && g_joy.btnLong)
        {
            if(app == &g_settingsApp) break;
            if(PauseMenu(app)) continue;    // resumed
            break;                          // "Main Menu": end this app
        }

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
