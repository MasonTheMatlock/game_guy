//#############################################################################
// FILE:   app/raycaster.c
// TITLE:  Raycaster - PLACEHOLDER (callback app contract)
//
// STATUS
//   This file only proves the app plumbing works: the menu can launch it, it
//   draws a screen, and a long press returns to the menu. The real game gets
//   built up inside the TODO functions below.
//
// PLANNED CONTROLS
//   stick left/right : turn (analog: pushing farther turns faster)
//   stick up/down    : walk forward / backward
//   button tap       : fire
//   HOLD stick button: quit to the menu (free, handled by main.c)
//
// WHAT A RAYCASTER IS (the idea to build toward)
//   A "3D" view drawn from a flat 2D grid map, the way Wolfenstein 3D did it.
//   For each vertical column of pixels on the screen:
//     1. shoot a ray from the player across the map at that column's angle,
//     2. step the ray from grid line to grid line until it enters a wall cell
//        (the "DDA" algorithm),
//     3. the distance travelled gives the wall height: far = short, near = tall,
//     4. draw that column as ceiling color, wall color, then floor color.
//   Use the PERPENDICULAR distance (not the straight-line distance) or walls
//   look curved, like a fisheye lens.
//
// APP CONTRACT
//   init()   once on launch: reset state, draw the screen from scratch.
//   update() once per frame: logic only, no drawing. Return APP_QUIT to leave.
//   render() once per frame after update(): drawing only.
//   main.c already calls Joy_Update() / Dpad_Update() before update() and
//   paces the frames, so there is no while(1), Joy_Update() or DELAY_US() here.
//   One public symbol: g_raycasterApp.
//#############################################################################

#include "raycaster.h"   // g_raycasterApp, RC_FRAME_US

//*****************************************************************************
// Game state
//   TODO: the map, player position (px, py), heading, etc. go here.
//   Keep them 'static' so they stay private to this file, like the other apps.
//*****************************************************************************

//*****************************************************************************
// init: runs once when the app starts. Reset game state and draw anything
// that never changes. Today that is just the placeholder screen.
//*****************************************************************************
static void Raycaster_Init(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    // Same title style as the other screens: centered text over a divider line.
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Place Holder",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);

    Graphics_drawStringCentered(&g_sContext, (int16_t *)"COMING SOON",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 110, OPAQUE_TEXT);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 214, OPAQUE_TEXT);

    // TODO: reset the player to the start of the map, generate the maze, etc.
}

//*****************************************************************************
// update: once per frame, BEFORE render. Change the game state: read g_joy,
// move the player, run enemies. Do not draw anything here.
//*****************************************************************************
static AppStatus Raycaster_Update(void)
{
    // TODO: turn with g_joy.dx, walk with g_joy.dy, fire on g_joy.btnPressed.
    //       (dx and dy are -100..100; see the Joy struct in drivers/drivers.h)
    //       Return APP_QUIT here for an app-initiated exit (e.g. D-pad C).

    return APP_CONTINUE;
}

//*****************************************************************************
// render: once per frame, AFTER update. Draw the current state of the game.
//*****************************************************************************
static void Raycaster_Render(void)
{
    // TODO: cast the rays and draw the 3D view.
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//   Already declared in apps.h (extern const App g_raycasterApp) and listed in
//   g_apps[] in main.c.
//*****************************************************************************
const App g_raycasterApp =
{
    "RAYCASTER",                        // name (first letter = menu tile icon)
    "Stick: move  Btn: fire",           // hint shown under the menu
    COLOR_RED,                          // menu tile color
    RC_FRAME_US,                        // frame period
    Raycaster_Init, Raycaster_Update, Raycaster_Render,
    NULL,                               // no exit() cleanup needed
    NULL                                // not a legacy app
};
