//#############################################################################
// FILE:   app/wason/wason_flip.c
// TITLE:  Wason Ware microgame FLIP!  (Pokemon Pinball)
//
//   Public: ww_PbInit, ww_PbUpdate, ww_PbRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// FLIP!  (Pokemon Pinball)  hold L / R (or the stick / button) to raise a
//        flipper. Pikachu and the Voltorbs kick the ball, the Digletts pop up
//        and down. All that matters: don't drain the ball until time's up.
//        200x200 window; only what changed is redrawn.
// ============================================================================
#define PB_W    200
#define PB_H    200
#define PB_PX   40                          // flipper pivot x (left); right is mirrored
#define PB_PY   160
#define PB_TX   88                          // flipper tip x
#define PB_N    5

static const int16_t kPbX[PB_N] = {  52, 148, 100,  26, 174 };      // 0,1 Voltorb  2 Pikachu  3,4 Diglett
static const int16_t kPbY[PB_N] = {  96,  96,  48,  72,  72 };
static const int16_t kPbR[PB_N] = {  12,  12,  16,   9,   9 };
static const int16_t kPbT[PB_N] = {   0,   0,   1,   2,   2 };

static int16_t pb_x4, pb_y4, pb_vx, pb_vy, pb_px, pb_py;
static int16_t pb_flash[PB_N], pb_kick[2], pb_dt[PB_N];
static bool    pb_lf, pb_rf, pb_dlf, pb_drf, pb_up[PB_N], pb_dsUp[PB_N], pb_dsFl[PB_N];
static bool    pb_nearLow;

static int16_t PbFloorL(int16_t x, bool up)                     // floor height on the left half
{
    if(x < PB_PX) return 130 + ((x - 6) * 30) / (PB_PX - 6);                        // outlane guide
    return PB_PY + ((x - PB_PX) * ((up ? 146 : 176) - PB_PY)) / (PB_TX - PB_PX);    // flipper
}

void ww_PbInit(uint16_t spd)
{
    int16_t i;

    pb_x4 = (80 + ww_Rnd(40)) * 4;
    pb_y4 = 100 * 4;
    pb_vx = ww_Rnd(17) - 8;
    pb_vy = 0;
    pb_px = pb_x4 / 4; pb_py = pb_y4 / 4;
    pb_kick[0] = pb_kick[1] = 0;
    pb_lf = pb_rf = pb_dlf = pb_drf = false;
    for(i = 0; i < PB_N; i++)
    {
        pb_flash[i] = 0;
        pb_up[i] = (i == 3);  pb_dt[i] = 20 + i * 12;
        pb_dsUp[i] = !pb_up[i];  pb_dsFl[i] = true;
    }
}

int16_t ww_PbUpdate(void)
{
    int16_t i, x, y, dx, dy, fy, sl, rr;
    bool    right;

    pb_lf = g_dpad.l || g_joy.dx < -50 || g_joy.btn;
    pb_rf = g_dpad.r || g_joy.dx >  50 || g_joy.btn;
    if(pb_kick[0] > 0) pb_kick[0]--;
    if(pb_kick[1] > 0) pb_kick[1]--;

    pb_vy = ww_Min16(pb_vy + 1, 26);
    pb_vx = ww_Clamp16(pb_vx - pb_vx / 24, -28, 28);
    pb_x4 += pb_vx;  pb_y4 += pb_vy;
    x = pb_x4 / 4;  y = pb_y4 / 4;

    if(x < 10)        { pb_x4 = 40;              pb_vx =  ww_Abs16(pb_vx); }
    if(x > PB_W - 10) { pb_x4 = (PB_W - 10) * 4; pb_vx = -ww_Abs16(pb_vx); }
    if(y < 10)        { pb_y4 = 40;              pb_vy =  ww_Abs16(pb_vy); }
    x = pb_x4 / 4;  y = pb_y4 / 4;

    for(i = 0; i < PB_N; i++)                                   // Pokemon bumpers
    {
        if(pb_flash[i] > 0) pb_flash[i]--;
        if(kPbT[i] == 2)                                        // Diglett: pops up and down
        {
            if(--pb_dt[i] <= 0) { pb_up[i] = !pb_up[i]; pb_dt[i] = pb_up[i] ? 45 : 50; }
            if(!pb_up[i]) continue;
        }
        dx = x - kPbX[i];  dy = y - kPbY[i];  rr = kPbR[i] + 4;
        if(dx * dx + dy * dy < rr * rr)
        {
            pb_vx = ww_Clamp16(dx * 2 + (dx > 0 ? 6 : -6), -28, 28);
            pb_vy = ww_Clamp16(dy * 2 + (dy > 0 ? 6 : -6), -28, 28);
            pb_flash[i] = 5;
            if(kPbT[i] == 2) { pb_up[i] = false; pb_dt[i] = 60; }          // Diglett ducks after a hit
            Buzzer_PlaySfx(&sfx_coin);
        }
    }

    right = (x > PB_W / 2);                                     // slanted floors + the draining gap
    if(y + 4 >= 125)
    {
        int16_t xm = right ? (PB_W - 1 - x) : x;
        if(xm >= 6 && xm < PB_TX)
        {
            bool up = right ? pb_rf : pb_lf;
            fy = PbFloorL(xm, up);
            if(y + 4 >= fy && pb_vy >= 0)
            {
                bool onFlip = (xm >= PB_PX);
                int16_t side = right ? 1 : 0;
                y = fy - 4;  pb_y4 = y * 4;
                if(onFlip && up && pb_kick[side] == 0)                 // flip!
                {
                    pb_vy = -26;
                    pb_vx = (6 + (xm - PB_PX) / 5) * (right ? -1 : 1);
                    pb_kick[side] = 8;
                    Buzzer_PlaySfx(&sfx_blip);
                }
                else
                {
                    sl = onFlip ? 1 : 3;
                    pb_vy = (pb_vy > 8) ? -(pb_vy / 3) : 0;
                    pb_vx += (right ? -sl : sl);
                }
            }
        }
    }
    pb_nearLow = (y >= 112);
    if(y >= PB_H - 4) return MG_LOSE;                           // drained
    return MG_PLAY;
}

static void PbDrawFlip(int16_t side, bool up, uint32_t c)
{
    int16_t i, tip = up ? 146 : 176, x, yc;

    for(i = 0; i < 4; i++)
    {
        x  = PB_PX + i * 12;
        yc = PB_PY + ((tip - PB_PY) * (i * 12 + 6)) / (PB_TX - PB_PX);
        ww_Box(side ? PB_W - x - 12 : x, yc - 3, 12, 7, c);
    }
}

static void PbDrawGuides(void)
{
    int16_t i, x, y;

    for(i = 0; i < 6; i++)
    {
        x = 6 + i * ((PB_PX - 6) / 6);
        y = 130 + (i * 30) / 6;
        ww_Box(x, y + 2, (PB_PX - 6) / 6 + 1, 6, COLOR_GRAY);
        ww_Box(PB_W - x - (PB_PX - 6) / 6 - 1, y + 2, (PB_PX - 6) / 6 + 1, 6, COLOR_GRAY);
    }
}

static void PbDrawObj(int16_t i)
{
    bool fl = (pb_flash[i] > 0);

    if(kPbT[i] == 0)       ww_DrawArt(kPbX[i] - 12, kPbY[i] - 12, fl ? ww_kArtBallHit : ww_kArtBall, 12, 12, 2, PB_BG, true);
    else if(kPbT[i] == 1)  ww_DrawArt(kPbX[i] - 16, kPbY[i] - 16, ww_kArtPikachu, 16, 16, 2, fl ? COLOR_WHITE : PB_BG, true);
    else if(pb_up[i])      ww_DrawArt(kPbX[i] - 14, kPbY[i] - 12, ww_kArtDiglett, 14, 12, 2, fl ? COLOR_WHITE : PB_BG, true);
    else
    {
        ww_Box(kPbX[i] - 14, kPbY[i] - 12, 28, 24, PB_BG);
        ww_Box(kPbX[i] - 12, kPbY[i] + 6, 24, 5, 0x05050FUL);               // the empty hole
    }
    pb_dsUp[i] = pb_up[i];  pb_dsFl[i] = fl;
}

void ww_PbRender(void)
{
    int16_t i, x = pb_x4 / 4, y = pb_y4 / 4;

    if(ww_first)
    {
        ww_Box(0, 0, 6, PB_H, COLOR_GRAY);  ww_Box(PB_W - 6, 0, 6, PB_H, COLOR_GRAY);
        ww_Box(0, 0, PB_W, 6, COLOR_GRAY);
        PbDrawGuides();
        for(i = 0; i < PB_N; i++) PbDrawObj(i);
        pb_dlf = pb_drf = false;
        PbDrawFlip(0, false, COLOR_YELLOW);  PbDrawFlip(1, false, COLOR_YELLOW);
    }

    ww_Box(pb_px - 5, pb_py - 5, 10, 10, PB_BG);                   // old ball
    for(i = 0; i < PB_N; i++)
        if(pb_dsUp[i] != pb_up[i] || pb_dsFl[i] != (pb_flash[i] > 0)) PbDrawObj(i);
    if(pb_lf != pb_dlf)
    {
        PbDrawFlip(0, pb_dlf, PB_BG);  PbDrawFlip(0, pb_lf, COLOR_YELLOW);  pb_dlf = pb_lf;
    }
    if(pb_rf != pb_drf)
    {
        PbDrawFlip(1, pb_drf, PB_BG);  PbDrawFlip(1, pb_rf, COLOR_YELLOW);  pb_drf = pb_rf;
    }
    if(pb_nearLow || (pb_py >= 108))                            // ball touched the lower furniture: repair it
    {
        PbDrawGuides();
        PbDrawFlip(0, pb_dlf, COLOR_YELLOW);  PbDrawFlip(1, pb_drf, COLOR_YELLOW);
    }
    ww_Box(x - 3, y - 4, 6, 8, COLOR_WHITE);                       // ball
    ww_Box(x - 4, y - 3, 8, 6, COLOR_WHITE);
    ww_Box(x - 4, y - 1, 8, 2, COLOR_RED);
    pb_px = x; pb_py = y;
}
