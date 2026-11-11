//#############################################################################
// FILE:   app/wason/wason_launch.c
// TITLE:  Wason Ware microgame LAUNCH!  (Angry Birds)
//
//   Public: ww_BdInit, ww_BdUpdate, ww_BdRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// LAUNCH!  (Angry Birds)  pull the STICK back like a slingshot: the direction
//          is the angle, how far you pull is the power. A dotted preview shows
//          the shot. Press D-pad DOWN (or click the stick) to let go.
// ============================================================================
#define BD_GY       200
#define BD_SX       44
#define BD_SY       168
#define BD_DOTS     8

static int16_t bd_ax, bd_ay, bd_pig, bd_dx, bd_dy, bd_bx, bd_by;
static int16_t bd_dotx[BD_DOTS], bd_doty[BD_DOTS];
static bool    bd_fly, bd_aimDirty, bd_pigAlive, bd_dotsDr;
static int32_t bd_x, bd_y, bd_vx, bd_vy;

void ww_BdInit(uint16_t spd)
{
    bd_ax = bd_ay = 0;
    bd_pig = 190 + ww_Rnd(90);
    bd_fly = false;
    bd_aimDirty = true;
    bd_pigAlive = true;
    bd_dotsDr = false;
    bd_dx = bd_bx = BD_SX;
    bd_dy = bd_by = BD_SY;
}

int16_t ww_BdUpdate(void)
{
    if(!bd_fly)
    {
        if(g_joy.dx != bd_ax || g_joy.dy != bd_ay) { bd_ax = g_joy.dx; bd_ay = g_joy.dy; bd_aimDirty = true; }

        if((g_dpad.dPressed || g_joy.btnPressed) && (ww_Abs16(bd_ax) + ww_Abs16(bd_ay)) >= 15)
        {
            bd_vx = -(int32_t)bd_ax * 9;                        // pull left/down -> fly right/up
            bd_vy = -(int32_t)bd_ay * 9;
            bd_x  = (int32_t)(BD_SX + bd_ax / 4) * 100;
            bd_y  = (int32_t)(BD_SY + bd_ay / 4) * 100;
            bd_fly = true;
            Buzzer_PlaySfx(&sfx_jump);
        }
        return MG_PLAY;
    }

    bd_x  += bd_vx;
    bd_y  += bd_vy;
    bd_vy += 24;
    bd_dx  = (int16_t)(bd_x / 100);
    bd_dy  = (int16_t)(bd_y / 100);

    if(ww_Hit(bd_dx - 6, bd_dy - 6, 12, 12, bd_pig, BD_GY - 24, 24, 24))
    {
        bd_pigAlive = false;
        Buzzer_PlaySfx(&sfx_explosion);
        return MG_WIN;
    }
    if(bd_dy >= BD_GY - 6 || bd_dx > FIELD_W || bd_dx < 0) return MG_LOSE;
    return MG_PLAY;
}

static void BdDrawPig(void)
{
    ww_Box(bd_pig, BD_GY - 24, 24, 24, COLOR_LIME);
    ww_Box(bd_pig + 4,  BD_GY - 19, 6, 6, COLOR_WHITE);
    ww_Box(bd_pig + 14, BD_GY - 19, 6, 6, COLOR_WHITE);
    ww_Box(bd_pig + 6,  BD_GY - 17, 2, 2, COLOR_BLACK);
    ww_Box(bd_pig + 16, BD_GY - 17, 2, 2, COLOR_BLACK);
    ww_Box(bd_pig + 7,  BD_GY - 9, 10, 6, 0x7FBF00UL);
}

static void BdDrawBird(int16_t x, int16_t y)                    // 12x12, plain rects so moves are exact
{
    ww_Box(x - 6, y - 6, 12, 12, COLOR_RED);
    ww_Box(x + 1, y - 3, 4, 4, COLOR_WHITE);
    ww_Box(x + 3, y - 2, 2, 2, COLOR_BLACK);
    ww_Box(x + 5, y + 1, 3, 3, COLOR_YELLOW);
    ww_Box(x - 1, y - 5, 6, 2, COLOR_BLACK);
}

void ww_BdRender(void)
{
    int16_t i, t, nx, ny;

    if(ww_first)
    {
        ww_Box(0, BD_GY, FIELD_W, FIELD_H - BD_GY, 0x2E7D32UL);
        ww_Box(BD_SX - 4, BD_SY + 14, 6, BD_GY - BD_SY - 14, COLOR_BROWN);          // slingshot post
        bd_dotsDr = false;
        bd_aimDirty = true;
        if(bd_pigAlive) BdDrawPig();
    }

    if(!bd_fly)
    {
        if(bd_aimDirty)
        {
            if(bd_dotsDr)                                       // erase old preview dots
                for(i = 0; i < BD_DOTS; i++) ww_Box(bd_dotx[i], bd_doty[i], 3, 3, BD_BG);
            nx = BD_SX + bd_ax / 4;  ny = BD_SY + bd_ay / 4;
            BdDrawBird(nx, ny);
            if(!ww_first) ww_EraseOutside(bd_bx - 6, bd_by - 6, 12, 12, nx - 6, ny - 6, 12, 12, BD_BG);
            bd_bx = nx; bd_by = ny;

            for(i = 0; i < BD_DOTS; i++)                        // new preview: where the bird will be
            {
                t = (i + 1) * 5;
                bd_dotx[i] = nx + (int16_t)(((int32_t)(-bd_ax) * 9 * t) / 100);
                bd_doty[i] = ny + (int16_t)(((int32_t)(-bd_ay) * 9 * t) / 100 + (int32_t)12 * t * (t + 1) / 100);
                if(bd_doty[i] > BD_GY - 4 || bd_dotx[i] < 0 || bd_dotx[i] > FIELD_W - 3) bd_doty[i] = -10;
                else ww_Box(bd_dotx[i], bd_doty[i], 3, 3, COLOR_WHITE);
            }
            bd_dotsDr = true;
            bd_aimDirty = false;
        }
    }
    else
    {
        if(bd_dotsDr)
        {
            for(i = 0; i < BD_DOTS; i++) ww_Box(bd_dotx[i], bd_doty[i], 3, 3, BD_BG);
            bd_dotsDr = false;
        }
        if(bd_pigAlive) BdDrawPig();
        nx = bd_dx;  ny = ww_Min16(bd_dy, BD_GY - 8);
        BdDrawBird(nx, ny);
        ww_EraseOutside(bd_bx - 6, bd_by - 6, 12, 12, nx - 6, ny - 6, 12, 12, BD_BG);
        bd_bx = nx;  bd_by = ny;
    }

    if(!bd_pigAlive) ww_Box(bd_pig, BD_GY - 24, 24, 24, BD_BG);
}
