//#############################################################################
// FILE:   app/wason/wason_dash.c
// TITLE:  Wason Ware microgame DASH!  (Geometry Dash)
//
//   Public: ww_GdInit, ww_GdUpdate, ww_GdRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// DASH!  (Geometry Dash)  hold BLUE to jump. Dodge spikes, land on blocks.
//        Survive.  Cube and spikes are drawn "new first, then erase the
//        leftovers" so nothing blinks.
// ============================================================================
#define GD_GY   206
#define GD_X    70

static int16_t gd_y, gd_vy, gd_py, gd_v, gd_cd;
static int16_t gd_ox[3], gd_opx[3], gd_ot[3];                   // 0 spike, 1 double spike, 2 block
static bool    gd_on[3], gd_dr[3], gd_gnd;

static int16_t GdW(int16_t t) { return (t == 0) ? 26 : ((t == 1) ? 52 : 30); }
static int16_t GdH(int16_t t) { return 24; }

void ww_GdInit(uint16_t spd)
{
    uint16_t i;

    gd_y = gd_py = GD_GY - 20;
    gd_vy = 0;
    gd_gnd = true;
    gd_v  = 6 + ww_Min16((int16_t)spd / 2, 2);
    gd_cd = 25;
    for(i = 0; i < 3; i++) { gd_on[i] = false; gd_dr[i] = false; }
}

int16_t ww_GdUpdate(void)
{
    uint16_t i;
    int16_t  prevB, bottom, sup, top;

    if(gd_gnd && Ui_SelectHeld()) { gd_vy = -11; Buzzer_PlaySfx(&sfx_jump); }

    if(--gd_cd <= 0)
    {
        for(i = 0; i < 3; i++)
            if(!gd_on[i]) { gd_on[i] = true; gd_ot[i] = ww_Rnd(3); gd_ox[i] = gd_opx[i] = FIELD_W; break; }
        gd_cd = (140 + ww_Rnd(90)) / gd_v;
    }
    for(i = 0; i < 3; i++)
        if(gd_on[i])
        {
            gd_ox[i] -= gd_v;
            if(gd_ox[i] < -60) gd_on[i] = false;
        }

    prevB = gd_y + 20;
    gd_vy = ww_Min16(gd_vy + 1, 12);
    gd_y += gd_vy;
    bottom = gd_y + 20;
    sup = GD_GY;

    for(i = 0; i < 3; i++)
    {
        if(!gd_on[i]) continue;
        if(gd_ot[i] == 2)
        {
            if(gd_ox[i] < GD_X + 18 && GD_X + 2 < gd_ox[i] + 30)
            {
                top = GD_GY - 24;
                if(prevB <= top && bottom >= top) sup = top;
                else if(bottom > top + 2 && prevB > top) return MG_LOSE;     // hit the side
            }
        }
        else if(ww_Hit(GD_X + 3, gd_y + 4, 14, 14, gd_ox[i] + 7, GD_GY - 16, GdW(gd_ot[i]) - 14, 14))
            return MG_LOSE;
    }

    if(bottom >= sup) { gd_y = sup - 20; gd_vy = 0; gd_gnd = true; }
    else gd_gnd = false;
    return MG_PLAY;
}

static void GdDrawCube(int16_t y)
{
    ww_Box(GD_X, y, 20, 20, COLOR_YELLOW);
    ww_Box(GD_X + 4, y + 5, 4, 4, COLOR_BLACK);
    ww_Box(GD_X + 12, y + 5, 4, 4, COLOR_BLACK);
    ww_Box(GD_X + 4, y + 13, 12, 3, COLOR_BLACK);
}

void ww_GdRender(void)
{
    uint16_t i;
    int16_t  y = GD_GY - 24;

    if(ww_first)
    {
        ww_Box(0, GD_GY, FIELD_W, FIELD_H - GD_GY, 0x6A1B9AUL);
        ww_Box(0, GD_GY, FIELD_W, 3, 0xE040FBUL);
        gd_py = gd_y;
        GdDrawCube(gd_y);
    }

    for(i = 0; i < 3; i++)
    {
        if(gd_on[i])
        {
            if(gd_ot[i] == 2)
            {
                ww_SlideX(y, 24, gd_opx[i], gd_ox[i], 30, COLOR_CYAN, GD_BG);
                if(ww_first || gd_ox[i] != gd_opx[i]) ww_Box(gd_ox[i], y, 30, 3, 0xB2EBF2UL);   // lit top edge
            }
            else
            {
                if(gd_ox[i] != gd_opx[i] || !gd_dr[i] || ww_first)
                {
                    ww_DrawArt(gd_ox[i], y, ww_kArtGdSpike, 13, 12, 2, GD_BG, true);
                    if(gd_ot[i] == 1) ww_DrawArt(gd_ox[i] + 26, y, ww_kArtGdSpike, 13, 12, 2, GD_BG, true);
                    if(gd_dr[i] && !ww_first)
                        ww_EraseOutside(gd_opx[i], y, GdW(gd_ot[i]), 24, gd_ox[i], y, GdW(gd_ot[i]), 24, GD_BG);
                }
            }
            gd_opx[i] = gd_ox[i];
            gd_dr[i]  = true;
        }
        else if(gd_dr[i]) { ww_Box(gd_opx[i], y, GdW(gd_ot[i]), 24, GD_BG); gd_dr[i] = false; }
    }

    if(gd_y != gd_py)                                           // cube: new first, then the strip it left
    {
        GdDrawCube(gd_y);
        ww_EraseOutside(GD_X, gd_py, 20, 20, GD_X, gd_y, 20, 20, GD_BG);
        gd_py = gd_y;
    }
}
