//#############################################################################
// FILE:   app/wason/wason_flap.c
// TITLE:  Wason Ware microgame FLAP!  (Flappy Bird)
//
//   Public: ww_FbInit, ww_FbUpdate, ww_FbRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// FLAP!  (Flappy Bird)  any button flaps. Fly through the pipes.  Survive.
// ============================================================================
#define FB_X    80
#define FB_W    36
#define FB_GAP  80
#define FB_FLOOR 216

static int16_t fb_y16, fb_vy, fb_py, fb_v;
static int16_t fb_x[2], fb_px[2], fb_gy[2];

void ww_FbInit(uint16_t spd)
{
    fb_y16 = 100 * 16;
    fb_vy  = 0;
    fb_py  = 100;
    fb_v   = 3 + ww_Min16((int16_t)spd / 2, 2);
    fb_x[0] = fb_px[0] = 300;  fb_gy[0] = 60 + ww_Rnd(80);
    fb_x[1] = fb_px[1] = 470;  fb_gy[1] = 60 + ww_Rnd(80);
}

int16_t ww_FbUpdate(void)
{
    uint16_t i;
    int16_t  y;

    if(ww_AnyPress()) { fb_vy = -110; Buzzer_PlaySfx(&sfx_blip); }
    fb_vy = ww_Min16(fb_vy + 9, 150);
    fb_y16 += fb_vy;
    y = fb_y16 / 16;
    if(y < 0 || y > FB_FLOOR - 14) return MG_LOSE;

    for(i = 0; i < 2; i++)
    {
        fb_x[i] -= fb_v;
        if(fb_x[i] < -FB_W) { fb_x[i] = fb_x[1 - i] + 170; fb_gy[i] = 55 + ww_Rnd(90); fb_px[i] = fb_x[i]; }
        if(ww_Hit(FB_X, y, 16, 14, fb_x[i], 0, FB_W, FB_FLOOR) &&
           (y < fb_gy[i] || y + 14 > fb_gy[i] + FB_GAP))
            return MG_LOSE;
    }
    return MG_PLAY;
}

void ww_FbRender(void)
{
    uint16_t i;
    int16_t  y = fb_y16 / 16;

    if(ww_first) ww_Box(0, FB_FLOOR, FIELD_W, FIELD_H - FB_FLOOR, 0xDED895UL);

    for(i = 0; i < 2; i++)
    {
        ww_SlideX(0, fb_gy[i], fb_px[i], fb_x[i], FB_W, COLOR_GREEN, FB_BG);
        ww_SlideX(fb_gy[i] + FB_GAP, FB_FLOOR - fb_gy[i] - FB_GAP,
               fb_px[i], fb_x[i], FB_W, COLOR_GREEN, FB_BG);
        fb_px[i] = fb_x[i];
    }

    ww_Box(FB_X - 1, fb_py - 1, 20, 16, FB_BG);
    ww_Box(FB_X, y, 16, 14, COLOR_YELLOW);
    ww_Box(FB_X + 10, y + 3, 4, 4, COLOR_WHITE);
    ww_Box(FB_X + 12, y + 4, 2, 2, COLOR_BLACK);
    ww_Box(FB_X + 14, y + 8, 4, 4, COLOR_ORANGE);
    fb_py = y;
}
