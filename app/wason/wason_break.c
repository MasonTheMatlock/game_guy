//#############################################################################
// FILE:   app/wason/wason_break.c
// TITLE:  Wason Ware microgame BREAK!  (Brick Breaker)
//
//   Public: ww_BkInit, ww_BkUpdate, ww_BkRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// BREAK!  (Brick Breaker)  the STICK moves the paddle. Smash 5 bricks.
// ============================================================================

static int16_t bk_px, bk_ppx, bk_x, bk_y, bk_vx, bk_vy, bk_pbx, bk_pby, bk_cnt;
static bool    bk_a[24], bk_dr[24];

static void BkBrick(int16_t i)
{
    static const uint32_t col[3] = { COLOR_RED, COLOR_ORANGE, COLOR_YELLOW };
    ww_Box(16 + (i % 8) * 36, 30 + (i / 8) * 14, 34, 12, col[i / 8]);
}

void ww_BkInit(uint16_t spd)
{
    int16_t i;

    for(i = 0; i < 24; i++) { bk_a[i] = true; bk_dr[i] = false; }
    bk_px = bk_ppx = 136;
    bk_x = bk_pbx = 156;  bk_y = bk_pby = 150;
    bk_vx = ww_Rnd(2) ? 3 : -3;
    bk_vy = -(5 + ww_Min16((int16_t)spd / 2, 2));
    bk_cnt = 0;
}

int16_t ww_BkUpdate(void)
{
    int16_t i, bx, by, oy = bk_y;

    bk_px = ww_Clamp16(bk_px + (g_joy.dx * 8) / 100, 0, 272);

    bk_x += bk_vx;  bk_y += bk_vy;
    if(bk_x < 0)   { bk_x = 0;   bk_vx =  ww_Abs16(bk_vx); }
    if(bk_x > 312) { bk_x = 312; bk_vx = -ww_Abs16(bk_vx); }
    if(bk_y < 0)   { bk_y = 0;   bk_vy =  ww_Abs16(bk_vy); }

    if(bk_vy > 0 && bk_y + 8 >= 210 && bk_y + 8 <= 222 && bk_x + 8 > bk_px && bk_x < bk_px + 48)
    {
        bk_y = 202;
        bk_vy = -ww_Abs16(bk_vy);
        bk_vx = ww_Clamp16(bk_vx + ((bk_x + 4) - (bk_px + 24)) / 8, -6, 6);
        if(bk_vx == 0) bk_vx = 1;
        Buzzer_PlaySfx(&sfx_blip);
    }

    for(i = 0; i < 24; i++)
    {
        if(!bk_a[i]) continue;
        bx = 16 + (i % 8) * 36;  by = 30 + (i / 8) * 14;
        if(ww_Hit(bk_x, bk_y, 8, 8, bx, by, 34, 12))
        {
            bk_a[i] = false;
            if(oy + 8 <= by || oy >= by + 12) bk_vy = -bk_vy; else bk_vx = -bk_vx;
            Buzzer_PlaySfx(&sfx_coin);
            if(++bk_cnt >= 5) return MG_WIN;
            break;
        }
    }
    if(bk_y >= FIELD_H) return MG_LOSE;
    return MG_PLAY;
}

void ww_BkRender(void)
{
    int16_t i;

    for(i = 0; i < 24; i++)
    {
        if(bk_a[i] && (ww_first || !bk_dr[i])) { BkBrick(i); bk_dr[i] = true; }
        else if(!bk_a[i] && bk_dr[i])
        {
            ww_Box(16 + (i % 8) * 36, 30 + (i / 8) * 14, 34, 12, BK_BG);
            bk_dr[i] = false;
        }
    }

    ww_Box(bk_pbx, bk_pby, 8, 8, BK_BG);                           // erase ball, repair bricks it touched
    for(i = 0; i < 24; i++)
        if(bk_a[i] && ww_Hit(bk_pbx, bk_pby, 8, 8, 16 + (i % 8) * 36, 30 + (i / 8) * 14, 34, 12)) BkBrick(i);

    if(ww_first || bk_px != bk_ppx)
    {
        ww_Box(bk_ppx, 210, 48, 8, BK_BG);
        ww_Box(bk_px, 210, 48, 8, COLOR_CYAN);
        bk_ppx = bk_px;
    }
    ww_Box(bk_x, bk_y, 8, 8, COLOR_WHITE);
    bk_pbx = bk_x; bk_pby = bk_y;
}
