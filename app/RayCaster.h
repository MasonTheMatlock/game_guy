//#############################################################################
// FILE:   app/raycaster.h
// TITLE:  RAYCASTER - public descriptor and tuning constants
//
//   You are Ray Caster, called to clean out a dungeon by casting rays.
//   5 arena rooms, then a boss. Clear each wave, spend gold in the shop,
//   survive. Three hearts, contact damage.
//
// CONTROLS
//   Stick          move (up/down = forward/back, left/right = strafe)
//   D-pad L / R    turn
//   D-pad U        fire the equipped staff (hold = auto-fire)
//   D-pad D        open / close the shop
//   Hold stick btn pause menu (handled by apps.c, nothing to do here)
//
//   In the shop:  stick up/down = row,  D-pad L/R = tab,
//                 D-pad U or stick button = buy / equip,  D-pad D = close
//#############################################################################

#ifndef __RAYCASTER_H__
#define __RAYCASTER_H__

#include "apps.h"

extern const App g_raycasterApp;            // app/raycaster.c

//*****************************************************************************
// Timing. The LCD transfer dominates frame time, so the pause is tiny.
// All speeds below are PER FRAME: if you change the render size and the frame
// rate changes a lot, scale RC_MOVE_SPEED / RC_TURN_SPEED to match.
//*****************************************************************************
#define RC_FRAME_US         2000UL

//*****************************************************************************
// Render size.  The 3D view is RC_COLS strips, each RC_COL_W pixels wide.
//   - RC_COLS * RC_COL_W   should be 320 (the full LCD width)
//   - RC_COL_W 4 / RC_COLS 80 is the default. If the game is too slow, use
//     RC_COL_W 8 / RC_COLS 40 (and RC_FOV_UNITS 160 still works: 4 units/col).
//   - RC_FOV_UNITS is the field of view in 1/1024-turn units (160 = ~56 deg).
//     (RC_COLS * RC_COL_W) / RC_FOV_UNITS must come out as a whole number
//     (it is the pixels-per-angle-unit used to place sprites and beams).
//*****************************************************************************

#define RC_VIEW_X_OFFSET    0
#define RC_VIEW_Y_OFFSET    0

#define RC_VIEW_Y0          (24 + RC_VIEW_Y_OFFSET)
#define RC_VIEW_H           168

#define RC_COLS             80
#define RC_COL_W            4

#define RC_FOV_UNITS        160

//*****************************************************************************
// World
//*****************************************************************************
#define RC_MAP_W            16              // MUST stay 16 (index uses a shift)
#define RC_MAP_H            16
#define RC_NUM_LEVELS       6               // 5 rooms + boss
#define RC_MAX_ENEMIES      12
#define RC_MAX_SPAWNS       8               // 'e' markers per map

//*****************************************************************************
// Player feel (Q10 world units: 1024 = one map tile)
//*****************************************************************************
#define RC_START_HEARTS     3
#define RC_MAX_HEARTS       6
#define RC_INVERT_Y         0               // g_joy.dy is +DOWN; 1 if wired backwards
#define RC_STICK_DEAD       12              // |stick| below this = 0
#define RC_TURN_SPEED       7               // angle units per frame (1024 = full turn)
#define RC_MOVE_SPEED       70              // Q10 units per frame at full stick
#define RC_PLAYER_R         190             // collision radius (Q10)
#define RC_INVULN_FRAMES    90              // after being hit
#define RC_THICK_FRAMES     160             // with the Thick Skin trait

//*****************************************************************************
// Stick "flick" detection in the shop (same idea as test.c)
//*****************************************************************************
#define RC_EDGE_ON          60
#define RC_EDGE_OFF         30

#endif // __RAYCASTER_H__
