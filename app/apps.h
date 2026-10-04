//#############################################################################
// FILE:   app/apps.h
// TITLE:  The "app contract" - what main.c and every game agree on
//
// WHY THIS FILE EXISTS
//   The project is split into small pieces that each do one job:
//
//       main.c            boots the hardware, then runs the menu <-> app loop
//       app/main_menu.c   draws the menu and returns which app was picked
//       app/tetris.c      the Tetris game
//       app/raycaster.c   the raycaster game (placeholder for now)
//
//   These files need to know *just enough* about each other to cooperate.
//   That shared knowledge lives here, in one place, instead of being copied
//   into every .c file. If a file only needs to talk to the others, it
//   includes this header and nothing else from the project's own code.
//
// THE APP CONTRACT
//   Every app is one .c file that exposes ONE public function:
//
//       void Xxx_Run(void);
//
//   While Xxx_Run() executes it owns the screen and the joystick. It must:
//     1. draw its own screen from scratch when it starts (never assume what
//        was on the LCD before - it was probably the menu),
//     2. call Joy_Update() once per frame to read the joystick,
//     3. return when the player HOLDS the button (g_joy.btnLong).
//   When Xxx_Run() returns, main() takes over again and shows the menu.
//
// HOW TO ADD A NEW APP (the whole checklist)
//   1. Create app/myapp.c with a  void Myapp_Run(void)  that follows the
//      contract above.
//   2. Declare  void Myapp_Run(void);  in the "App entry points" list below.
//   3. Add one line to the g_apps[] table in main.c.
//   The menu builds itself from that table, so it needs no changes.
//#############################################################################

#ifndef __APPS_H__
#define __APPS_H__

#include <stdint.h>
#include <stdbool.h>
#include "F2806x_Device.h"      // device registers
#include "F2806x_Examples.h"    // DELAY_US(), CPU_RATE
#include "grlib.h"              // Graphics_* drawing functions
#include "drivers/drivers.h"    // joystick: g_joy, Joy_Update(), TimerNow()
#include "hal/kitronix320x240x16_ssd2119_spi.h"

//*****************************************************************************
// Screen geometry
//*****************************************************************************
#define SCREEN_W      LCD_VERTICAL_MAX       // LCD is 320 x 240, landscape
#define SCREEN_H      LCD_HORIZONTAL_MAX



//*****************************************************************************
// Shared services (defined once in main.c, used by every app)
//*****************************************************************************

// The graphics context: grlib's "handle" to the LCD. Every Graphics_xxx()
// call needs it. main.c creates and initializes it; apps only use it.
extern Graphics_Context g_sContext;

// graphics font sans seriff 20 pt bold.
extern const Graphics_Font g_sFontCmss20b;


// Fill the rectangle (x0,y0)-(x1,y1), both corners INCLUSIVE, with a color.
// Does nothing if the rectangle is empty (x1 < x0 or y1 < y0), so callers
// don't have to special-case zero-sized shapes.
void Graphics_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color);

// Small pseudo-random number generator (linear congruential).
// Rand_Seed() picks the starting point; seed it from something that differs
// on every run (e.g. TimerNow()) or the game will play out identically.
void     Rand_Seed(uint32_t seed);
uint16_t Rand16(void);          // returns 0..65535

//*****************************************************************************
// App registry - one entry per app, built in main.c, read by the menu
//*****************************************************************************
typedef struct
{
    const char *name;           // text on the menu button
    const char *hint;           // control hint shown while it is highlighted
    uint32_t    color;          // menu button color (0xRRGGBB)
    void      (*run)(void);     // the app's entry point (see contract above)
} AppInfo;

extern const AppInfo  g_apps[];     // the table itself (defined in main.c)
extern const uint16_t g_appCount;   // number of entries in g_apps[]


int16_t Menu_Run(void);

// app/tetris.c
void Tetris_Run(void);

// app/raycaster.c
void Raycaster_Run(void);



#endif // __APPS_H__
