//#############################################################################
// FILE:   app/wason/wason_slice.c
// TITLE:  Wason Ware microgame SLICE!  (Fruit Ninja)
//
//   Public: ww_FnInit, ww_FnUpdate, ww_FnRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// SLICE!  (Fruit Ninja)  swipe the cursor through 4 fruits. Avoid the bomb!
//         Sprites are opaque art: draw the new frame first, then erase only the
//         strips the fruit left behind, so nothing flickers.
// ============================================================================
#define FN_N    2

static const char *const *const kFnArt[4] = { ww_kArtOrange, ww_kArtApple, ww_kArtLime, ww_kArtBomb };

static int16_t fn_x[FN_N], fn_y4[FN_N], fn_vx[FN_N], fn_vy4[FN_N], fn_dx[FN_N], fn_dy[FN_N], fn_k[FN_N];
static bool    fn_on[FN_N], fn_dr[FN_N];
static int16_t fn_cx, fn_cy, fn_pcx, fn_pcy, fn_cd, fn_cnt, fn_shown, fn_rate;

void ww_FnInit(uint16_t spd)
{
    uint16_t i;

    for(i = 0; i < FN_N; i++) { fn_on[i] = false; fn_dr[i] = false; }
    fn_cx = fn_pcx = 160;
    fn_cy = fn_pcy = 115;
    fn_cd = 10;
    fn_cnt = 0; fn_shown = -1;
    fn_rate = ww_Max16(16, 30 - 2 * (int16_t)spd);
}

int16_t ww_FnUpdate(void)
{
    uint16_t i;
    int16_t  mx, my;

    mx = g_joy.dx / 9;
    my = g_joy.dy / 9;
    if(g_dpad.l) mx = -8;
    if(g_dpad.r) mx =  8;
    if(g_dpad.u) my = -8;
    if(g_dpad.d) my =  8;
    fn_cx = ww_Clamp16(fn_cx + mx, 8, FIELD_W - 9);
    fn_cy = ww_Clamp16(fn_cy + my, 8, FIELD_H - 9);

    if(--fn_cd <= 0)
    {
        for(i = 0; i < FN_N; i++)
            if(!fn_on[i])
            {
                fn_on[i]   = true;
                fn_k[i]    = (ww_Rnd(6) == 0) ? 3 : ww_Rnd(3);
                fn_x[i]    = 50 + ww_Rnd(220);
                fn_y4[i]   = (FIELD_H + 20) * 4;
                fn_vy4[i]  = -(46 + ww_Rnd(14));
                fn_vx[i]   = (fn_x[i] < 160) ? 1 + ww_Rnd(3) : -(1 + ww_Rnd(3));
                break;
            }
        fn_cd = fn_rate + ww_Rnd(14);
    }

    for(i = 0; i < FN_N; i++)
    {
        if(!fn_on[i]) continue;
        fn_x[i]   += fn_vx[i];
        fn_vy4[i] += 2;
        fn_y4[i]  += fn_vy4[i];
        if(fn_vy4[i] > 0 && fn_y4[i] > (FIELD_H + 26) * 4) { fn_on[i] = false; continue; }

        if(ww_Abs16(fn_x[i] - fn_cx) <= 20 && ww_Abs16(fn_y4[i] / 4 - fn_cy) <= 20)
        {
            fn_on[i] = false;
            if(fn_k[i] == 3) { Buzzer_PlaySfx(&sfx_explosion); return MG_LOSE; }
            Buzzer_PlaySfx(&sfx_laser);
            if(++fn_cnt >= 4) return MG_WIN;
        }
    }
    return MG_PLAY;
}

void ww_FnRender(void)
{
    uint16_t i;
    int16_t  y;

    for(i = 0; i < FN_N; i++)
    {
        if(fn_on[i])
        {
            y = fn_y4[i] / 4 - 16;
            if(fn_dr[i])
            {
                ww_DrawArt(fn_x[i] - 16, y, kFnArt[fn_k[i]], 16, 16, 2, FN_BG, true);          // new first...
                ww_EraseOutside(fn_dx[i] - 16, fn_dy[i], 32, 32, fn_x[i] - 16, y, 32, 32, FN_BG); // ...then the leftovers
            }
            else ww_DrawArt(fn_x[i] - 16, y, kFnArt[fn_k[i]], 16, 16, 2, FN_BG, true);
            fn_dx[i] = fn_x[i]; fn_dy[i] = y; fn_dr[i] = true;
        }
        else if(fn_dr[i]) { ww_Box(fn_dx[i] - 16, fn_dy[i], 32, 32, FN_BG); fn_dr[i] = false; }
    }

    if(ww_first || fn_cx != fn_pcx || fn_cy != fn_pcy)
    {
        ww_Box(fn_cx - 7, fn_cy - 7, 14, 14, COLOR_WHITE);
        ww_Box(fn_cx - 4, fn_cy - 4, 8, 8, COLOR_CHARCOAL);
        if(!ww_first) ww_EraseOutside(fn_pcx - 7, fn_pcy - 7, 14, 14, fn_cx - 7, fn_cy - 7, 14, 14, FN_BG);
        fn_pcx = fn_cx; fn_pcy = fn_cy;
    }

    if(fn_shown != fn_cnt)
    {
        for(i = 0; i < 4; i++)
            ww_Box(10 + i * 22, 8, 16, 16, (int16_t)i < fn_cnt ? COLOR_GOLD : COLOR_DARK_GRAY);
        fn_shown = fn_cnt;
    }
}
