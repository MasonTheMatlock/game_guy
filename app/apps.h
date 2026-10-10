//#############################################################################
// FILE:   app/apps.h
// TITLE:  The app layer's one public header: the app contract, the shared
//         services, and the per-app headers.
//
// THE APP CONTRACT (callback style)
//   An app is a const App struct with three callbacks. apps.c owns the loop:
//
//       init()    called once on launch. Draw the whole screen from scratch
//                 (never assume what was on the LCD) and reset your state.
//       update()  called once per frame. Game LOGIC only, no drawing.
//                 Return APP_QUIT to leave, APP_CONTINUE otherwise.
//       render()  called once per frame after update(). Drawing only.
//       exit()    optional (may be NULL). Cleanup when the app ends.
//
//   Before every update(), the app loop has ALREADY called Joy_Update() and
//   Dpad_Update(). DO NOT call them again in your app - a second call eats
//   the "Pressed" edge flags.
//
//   Holding the joystick button (g_joy.btnLong) quits any app automatically.
//   Apps do not have to implement that.
//
// HOW TO ADD A NEW APP
//   1. Create app/myapp.h:  #include "apps.h"  and  extern const App g_myappApp;
//      plus any constants / types you want to share (see tetris.h).
//   2. Create app/myapp.c: #include "myapp.h", static Myapp_Init/Update/Render
//      and ONE public descriptor:   const App g_myappApp = { ... };
//   3. Add   #include "myapp.h"   at the bottom of this file.
//   4. Add   &g_myappApp,   to g_apps[] in app/apps.c.
//   The menu builds itself from g_apps[].
//
// LEGACY APPS (migration aid)
//   An app that still has its own blocking  void Xxx_Run(void)  loop can be
//   wrapped by putting that function in the .run field and leaving the
//   callbacks NULL. the runner just calls it. Once every app is converted, .run
//   can be deleted.
//#############################################################################

#ifndef __APPS_H__
#define __APPS_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>             // NULL
#include "F2806x_Device.h"      // device registers
#include "F2806x_Examples.h"    // DELAY_US(), CPU_RATE
#include "grlib.h"              // Graphics_* drawing functions
#include "drivers/drivers.h"    // joystick + dpad: g_joy, g_dpad, ...
#include "hal/kitronix320x240x16_ssd2119_spi.h"

//*****************************************************************************
// Screen geometry
//*****************************************************************************
#define SCREEN_W      LCD_VERTICAL_MAX       // LCD is 320 x 240, landscape
#define SCREEN_H      LCD_HORIZONTAL_MAX

//*****************************************************************************
// Shared services (defined in app/apps.c, used by every app)
//*****************************************************************************
extern Graphics_Context g_sContext;
extern const Graphics_Font g_sFontCmss20b;

// Fill (x0,y0)-(x1,y1), corners INCLUSIVE. Empty rectangles are ignored.
void Graphics_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color);

void     Rand_Seed(uint32_t seed);
uint16_t Rand16(void);          // returns 0..65535

//*****************************************************************************
// The App descriptor
//*****************************************************************************
typedef enum
{
    APP_CONTINUE = 0,
    APP_QUIT
} AppStatus;

typedef struct
{
    const char *name;           // menu tile label (first letter = tile icon)
    const char *hint;           // control hint shown under the menu
    uint32_t    color;          // menu tile color (0xRRGGBB)
    uint32_t    frameUs;        // pause after each frame (microseconds)

    void      (*init)(void);    // called once at launch
    AppStatus (*update)(void);  // logic, once per frame
    void      (*render)(void);  // drawing, once per frame
    void      (*exit)(void);    // optional, may be NULL

    void      (*run)(void);     // LEGACY blocking entry point, else NULL
} App;

// Ask the app loop to run `app` next, then return APP_QUIT from your update().
// If nothing is requested, it goes back to the menu.
void App_Launch(const App *app);

// The one frame loop (app/apps.c): menu -> app -> menu -> ...
// Called once by main() after boot. Never returns.
void App_Loop(void);

//*****************************************************************************
// App registry
//*****************************************************************************
extern const App *const g_apps[];   // apps shown in the menu (app/apps.c)
extern const uint16_t   g_appCount;

//*****************************************************************************
// Per-app headers. Keep these LAST: each one includes apps.h for the types
// above, which is safe because the guard is already set by this point.
//*****************************************************************************
#include "main_menu.h"
#include "tetris.h"
#include "raycaster.h"
#include "settings.h"
#include "test.h"

#endif // __APPS_H__
