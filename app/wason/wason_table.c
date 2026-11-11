//#############################################################################
// FILE:   app/wason/wason_table.c
// TITLE:  Wason Ware game table: ONE line per microgame
//#############################################################################
#include "wason_priv.h"

//*****************************************************************************
// 3. GAME TABLE  <- add a microgame: one line here
//      cmd, inspiration tag, frames, background, winOnTimeout, init, update, render
//*****************************************************************************
#define WFULL 0, 0, FIELD_W, FIELD_H
const MicroGame ww_games[] =
{
    { "CLIMB!",    "Donkey Kong",     260, DK_BG, WFULL,              0, ww_DkInit, ww_DkUpdate, ww_DkRender },
    { "BATTLE!",   "Pokemon",         220, PK_BG, WFULL,              0, ww_PkInit, ww_PkUpdate, ww_PkRender },
    { "FLIP!",     "Pokemon Pinball", 340, PB_BG, 60, 12, 200, 200,   1, ww_PbInit, ww_PbUpdate, ww_PbRender },
    { "RUN!",      "Sonic",           280, SO_BG, 0, 40, 320, 160,    0, ww_SoInit, ww_SoUpdate, ww_SoRender },
    { "PLANT!",    "Plants vs Zombies", 360, PZ_BG0, 0, 24, 320, 168, 0, ww_PzInit, ww_PzUpdate, ww_PzRender },
    { "BLAST!",    "Metroid",         340, MT_BG, WFULL,              0, ww_MtInit, ww_MtUpdate, ww_MtRender },
    { "SLICE!",    "Fruit Ninja",     280, FN_BG, WFULL,              0, ww_FnInit, ww_FnUpdate, ww_FnRender },
    { "RACE!",     "Cruis'n USA",     190, RS_SKY, 40, 24, 240, 180,  1, ww_RsInit, ww_RsUpdate, ww_RsRender },
    { "SURF!",     "Subway Surfers",  230, SF_SKY, 40, 24, 240, 180,  1, ww_SfInit, ww_SfUpdate, ww_SfRender },
    { "CALL IT!",  "Helldivers 2",    170, HD_BG, WFULL,              0, ww_HdInit, ww_HdUpdate, ww_HdRender },
    { "BREAK!",    "Brick Breaker",   280, BK_BG, WFULL,              0, ww_BkInit, ww_BkUpdate, ww_BkRender },
    { "FIT IT!",   "Tetris",          240, TT_BG, WFULL,              0, ww_TtInit, ww_TtUpdate, ww_TtRender },
    { "CHOMP!",    "Pac-Man",         330, PM_BG, 55, 24, 210, 182,   0, ww_PmInit, ww_PmUpdate, ww_PmRender },
    { "CROSS!",    "Frogger",         280, FR_BG, WFULL,              0, ww_FrInit, ww_FrUpdate, ww_FrRender },
    { "FLAP!",     "Flappy Bird",     200, FB_BG, WFULL,              1, ww_FbInit, ww_FbUpdate, ww_FbRender },
    { "JUMP!",     "Chrome Dino",     180, DN_BG, WFULL,              1, ww_DnInit, ww_DnUpdate, ww_DnRender },
    { "STOMP!",    "Super Mario",     260, MR_BG, WFULL,              0, ww_MrInit, ww_MrUpdate, ww_MrRender },
    { "DASH!",     "Geometry Dash",   200, GD_BG, WFULL,              1, ww_GdInit, ww_GdUpdate, ww_GdRender },
    { "LAUNCH!",   "Angry Birds",     280, BD_BG, WFULL,              0, ww_BdInit, ww_BdUpdate, ww_BdRender },
    { "BEAT!",     "Melatonin",       480, ML_BG, WFULL,              0, ww_MlInit, ww_MlUpdate, ww_MlRender },
    { "SORT!",     "Recycler",        220, RC_BG, WFULL,              0, ww_RcInit, ww_RcUpdate, ww_RcRender },
    { "RETURN!",   "Pong",            300, PG_BG, WFULL,              0, ww_PgInit, ww_PgUpdate, ww_PgRender },
    { "SHOOT!",    "Galaga",          320, GL_BG, WFULL,              0, ww_GlInit, ww_GlUpdate, ww_GlRender },
};


const uint16_t ww_numGames = (uint16_t)(sizeof(ww_games) / sizeof(ww_games[0]));
