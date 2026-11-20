//#############################################################################
// FILE:   app/wason/wason_table.c
// TITLE:  Wason Ware game table: ONE line per microgame
//#############################################################################
#include "wason_priv.h"

//*****************************************************************************
// 3. GAME TABLE  <- add a microgame: one line here
//      cmd, inspiration tag, frames, background, window, winOnTimeout, init, update, render, HOWn(...)
//*****************************************************************************
#define WFULL 0, 0, FIELD_W, FIELD_H

// Micro instructions: C(role, "text") is one control line. Roles come from ui.h:
//   MOVE (stick)  JUMP/SELECT (blue + stick click)  BACK (red)  ACT2 (yellow)  ACT3 (green)
//   or a physical button  U L R D  (red yellow green blue as drawn on the console).
#define C(i, t)              { UI_ICON_##i, t }
#define HOW1(goal, a)        { goal, 1, { a } }
#define HOW2(goal, a, b)     { goal, 2, { a, b } }
#define HOW3(goal, a, b, c)  { goal, 3, { a, b, c } }
const MicroGame ww_games[] =
{
    { "CLIMB!",    "Donkey Kong",     260, DK_BG, WFULL,              0, ww_DkInit, ww_DkUpdate, ww_DkRender,
        HOW2("Jump barrels, reach the top", C(MOVE,"Run + climb"), C(JUMP,"Jump")) },
    { "BATTLE!",   "Pokemon",         220, PK_BG, WFULL,              0, ww_PkInit, ww_PkUpdate, ww_PkRender,
        HOW3("Counter its type, hit 3x", C(U,"Flamethrower"), C(D,"Hydro Pump"), C(R,"Razor Leaf")) },
    { "FLIP!",     "Pokemon Pinball", 340, PB_BG, 60, 12, 200, 200,   1, ww_PbInit, ww_PbUpdate, ww_PbRender,
        HOW2("Don't drain the ball!", C(L,"Left flipper"), C(R,"Right flipper")) },
    { "RUN!",      "Sonic",           280, SO_BG, 0, 40, 320, 160,    0, ww_SoInit, ww_SoUpdate, ww_SoRender,
        HOW2("Grab 5 rings, dodge spikes", C(JUMP,"Jump"), C(ACT2,"Roll (squash bugs)")) },
    { "PLANT!",    "Plants vs Zombies", 360, PZ_BG0, 0, 24, 320, 168, 0, ww_PzInit, ww_PzUpdate, ww_PzRender,
        HOW2("Stop the zombies!", C(MOVE,"Pick a lane"), C(SELECT,"Plant a shooter")) },
    { "BLAST!",    "Metroid",         340, MT_BG, WFULL,              0, ww_MtInit, ww_MtUpdate, ww_MtRender,
        HOW2("Freeze 5 creatures", C(MOVE,"Aim the cannon"), C(SELECT,"Fire")) },
    { "SLICE!",    "Fruit Ninja",     280, FN_BG, WFULL,              0, ww_FnInit, ww_FnUpdate, ww_FnRender,
        HOW1("Slice 4 fruits, no bombs!", C(MOVE,"Swipe the cursor")) },
    { "RACE!",     "Cruis'n USA",     190, RS_SKY, 40, 24, 240, 180,  1, ww_RsInit, ww_RsUpdate, ww_RsRender,
        HOW2("Dodge the traffic", C(MOVE,"Steer"), C(SELECT,"Nitro (hold)")) },
    { "SURF!",     "Subway Surfers",  230, SF_SKY, 40, 24, 240, 180,  1, ww_SfInit, ww_SfUpdate, ww_SfRender,
        HOW2("Dodge trains and barriers", C(MOVE,"Lane / slide down"), C(JUMP,"Jump")) },
    { "CALL IT!",  "Helldivers 2",    170, HD_BG, WFULL,              0, ww_HdInit, ww_HdUpdate, ww_HdRender,
        HOW2("Copy the arrow code", C(MOVE,"Match each arrow"), C(U,"or the 4 buttons")) },
    { "BREAK!",    "Brick Breaker",   280, BK_BG, WFULL,              0, ww_BkInit, ww_BkUpdate, ww_BkRender,
        HOW1("Smash 5 bricks", C(MOVE,"Move the paddle")) },
    { "FIT IT!",   "Tetris",          240, TT_BG, WFULL,              0, ww_TtInit, ww_TtUpdate, ww_TtRender,
        HOW2("Drop the I into the gap", C(MOVE,"Slide left / right"), C(SELECT,"Drop fast (hold)")) },
    { "CHOMP!",    "Pac-Man",         330, PM_BG, 55, 24, 210, 182,   0, ww_PmInit, ww_PmUpdate, ww_PmRender,
        HOW1("Eat dots, dodge the ghost", C(MOVE,"Steer")) },
    { "CROSS!",    "Frogger",         280, FR_BG, WFULL,              0, ww_FrInit, ww_FrUpdate, ww_FrRender,
        HOW1("Hop to the far bank", C(MOVE,"Hop")) },
    { "FLAP!",     "Flappy Bird",     200, FB_BG, WFULL,              1, ww_FbInit, ww_FbUpdate, ww_FbRender,
        HOW1("Fly through the pipes", C(JUMP,"Flap")) },
    { "JUMP!",     "Chrome Dino",     180, DN_BG, WFULL,              1, ww_DnInit, ww_DnUpdate, ww_DnRender,
        HOW1("Hop over the cacti", C(JUMP,"Jump")) },
    { "STOMP!",    "Super Mario",     260, MR_BG, WFULL,              0, ww_MrInit, ww_MrUpdate, ww_MrRender,
        HOW2("Stomp 2 Goombas", C(MOVE,"Run"), C(JUMP,"Jump")) },
    { "DASH!",     "Geometry Dash",   200, GD_BG, WFULL,              1, ww_GdInit, ww_GdUpdate, ww_GdRender,
        HOW1("Dodge spikes, land on blocks", C(JUMP,"Jump (hold)")) },
    { "LAUNCH!",   "Angry Birds",     280, BD_BG, WFULL,              0, ww_BdInit, ww_BdUpdate, ww_BdRender,
        HOW2("Hit the green pig", C(MOVE,"Pull back to aim"), C(SELECT,"Let go")) },
    { "BEAT!",     "Melatonin",       480, ML_BG, WFULL,              0, ww_MlInit, ww_MlUpdate, ww_MlRender,
        HOW1("Hit 6 of 8 notes", C(SELECT,"Tap on the beat")) },
    { "SORT!",     "Recycler",        220, RC_BG, WFULL,              0, ww_RcInit, ww_RcUpdate, ww_RcRender,
        HOW3("Sort 5 items", C(L,"Paper"), C(U,"Can"), C(R,"Glass")) },
    { "RETURN!",   "Pong",            300, PG_BG, WFULL,              0, ww_PgInit, ww_PgUpdate, ww_PgRender,
        HOW1("Return the ball 3 times", C(MOVE,"Move the paddle")) },
    { "SHOOT!",    "Galaga",          320, GL_BG, WFULL,              0, ww_GlInit, ww_GlUpdate, ww_GlRender,
        HOW2("Shoot 6 bugs", C(MOVE,"Move"), C(SELECT,"Fire")) },
};


const uint16_t ww_numGames = (uint16_t)(sizeof(ww_games) / sizeof(ww_games[0]));

#undef C
#undef HOW1
#undef HOW2
#undef HOW3
