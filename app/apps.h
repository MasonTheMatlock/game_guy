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
//   Holding the joystick button (g_joy.btnLong) opens the PAUSE MENU (settings.c)
//   on top of any running app (not the launcher). It offers Volume and Brightness
//   plus the settings page of THAT APP ONLY (its App.settings table, below).
//   Every app's page is also reachable from the launcher: Settings > Apps > <app>.
//   Apps do not implement the hold.
//
// HOW TO ADD A NEW APP (each app lives in its own folder, app/<name>/)
//   1. Create app/myapp/myapp.h:  #include "../apps.h"  and  extern const App g_myappApp;
//   2. Split the code by job, e.g.  myapp_logic.c  myapp_draw.c  myapp_sound.c
//      myapp_art.c  and the file that holds the descriptor, myapp.c. Share
//      private declarations through one myapp_priv.h.
//   3. Optional: give the app its own settings page (see AppSettings below) and
//      point .settings at it. No settings = no entry in the pause menu.
//   4. Add   #include "myapp/myapp.h"   at the bottom of this file and
//      &g_myappApp,   to g_apps[] in app/apps.c. The menu builds itself from it.
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
#define SCREEN_H      LCD_VERTICAL_MAX       // LCD is 320 x 240, landscape
#define SCREEN_W     LCD_HORIZONTAL_MAX

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

//*****************************************************************************
// Per-app settings. An app describes its own settings as DATA; settings.c draws
// and runs them. Pause menu > <App> settings only ever shows the running app's.
//   SET_INFO    label + current value, nothing to press
//   SET_CHOICE  label + value; stick right / tap steps to the next value
//   SET_COLOR   opens a swatch picker (palette, preview text): use for
//               "appearance" options such as a background or theme colour
//   SET_ACTION  runs a function; optional confirm screen first; the text it
//               returns ("line 1|line 2|line 3") is shown as the result
//*****************************************************************************
typedef enum { SET_INFO = 0, SET_CHOICE, SET_COLOR, SET_ACTION } SettingKind;

typedef struct
{
    const char  *label;
    SettingKind  kind;

    const char *(*text)(void);          // INFO / CHOICE: current value as text
    void        (*step)(int16_t dir);   // CHOICE: move to the next value (wrap)

    uint16_t     nColors;               // COLOR: palette size (max 12)
    uint32_t   (*color)(uint16_t i);    //        0xRRGGBB of entry i
    const char *(*colorName)(uint16_t i);
    uint16_t   (*get)(void);            //        current entry
    void       (*set)(uint16_t i);      //        choose entry i (RAM; settings flushes on exit)
    const char  *preview;               //        text shown inside the preview box

    const char *(*run)(void);           // ACTION: do it, return the result text (or NULL)
    const char  *confirm;               //         "a|b|c" = ask first (NULL = run at once)
    const char  *confirmButton;         //         text of the confirm button
    bool         danger;                //         confirm button is red
    bool       (*enabled)(void);        // optional: NULL = always; false = shown dimmed
} SettingItem;

typedef struct
{
    const char        *title;           // row label in the pause menu, e.g. "Menu Settings"
    const SettingItem *items;
    uint16_t           count;           // 1..5
} AppSettings;

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
    void      (*redraw)(void);  // optional: repaint the screen from current state
                                // after the Settings overlay (keeps game state);
                                // NULL = fall back to init() (restarts the app)
    const AppSettings *settings;// optional: this app's own settings page (or NULL)
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

// The app the pause menu was opened over (set by apps.c, read by settings.c).
extern const App       *g_pausedApp;



// soung bug fix???
void Sfx_PlayWait(const Song *sfx);     // short effect that survives the next Buzzer_Stop()

//*****************************************************************************
// Per-app headers. Keep these LAST: each one includes apps.h for the types
// above, which is safe because the guard is already set by this point.
//*****************************************************************************
#include "menu/menu.h"
#include "tetris/tetris.h"
#include "raycaster/raycaster.h"
#include "settings.h"
#include "test/test.h"
#include "songs.h"              // Buzzer_PlaySong(&song_tetris, 1), sfx_coin, ...
#include "images.h"
#include "wason/wason.h"

#endif // __APPS_H__
