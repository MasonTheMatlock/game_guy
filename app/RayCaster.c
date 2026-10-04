//#############################################################################
// FILE:   app/raycaster.c
// TITLE:  Raycaster - PLACEHOLDER
//
// STATUS
//   This file only proves the app plumbing works: the menu can launch it, it
//   draws a screen, it reads the joystick, and a long press returns to the
//   menu. The real game gets built up inside the TODO functions below.
//
// PLANNED CONTROLS
//   stick left/right : turn (analog: pushing farther turns faster)
//   stick up/down    : walk forward / backward
//   button tap       : fire
//   hold button      : quit to the menu   (already works - see Raycaster_Run)
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
//#############################################################################

#include "apps.h"

//*****************************************************************************
// Constants
//*****************************************************************************
#define RC_FRAME_US    16000UL          // pause per loop pass (~60 frames/sec
                                        //   before drawing time is added)

//*****************************************************************************
// Game state
//   TODO: the map, player position (px, py), heading, etc. go here.
//   Keep them 'static' so they stay private to this file, like the other apps.
//*****************************************************************************

//*****************************************************************************
// Init / Update / Render
//   Almost every game loop splits into these three jobs. Keeping them separate
//   means you can change how the game LOOKS without risking how it BEHAVES.
//*****************************************************************************

// Called once each time the app starts. Reset game state and draw anything
// that never changes. Today that is just the placeholder screen.
static void Raycaster_Init(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    // Same title style as the other screens: centered text over a divider line.
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Place Holder",
                                AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, 309, 30);

    Graphics_drawStringCentered(&g_sContext, (int16_t *)"COMING SOON",
                                AUTO_STRING_LENGTH, 159, 110, OPAQUE_TEXT);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, 159, 214, OPAQUE_TEXT);

    // TODO: reset the player to the start of the map, generate the maze, etc.
}

// Called once per frame BEFORE drawing. Change the game state: read g_joy,
// move the player, run enemies. Do not draw anything here.
static void Raycaster_Update(void)
{
    // TODO: turn with g_joy.dx, walk with g_joy.dy, fire on g_joy.btnPressed.
    //       (dx and dy are -100..100; see the Joy struct in drivers/drivers.h)
}

// Called once per frame AFTER Update. Draw the current state of the game.

static void Raycaster_Render(void)
{
    // TODO: cast the rays and draw the 3D view.
}


void Raycaster_Run(void)
{
    Raycaster_Init();

    while(1)
    {
        Joy_Update();                               // refresh g_joy once per frame
        if(g_joy.btnLong) return;                   // hold button = back to menu

        Raycaster_Update();
        Raycaster_Render();

        DELAY_US(RC_FRAME_US);
    }
}
