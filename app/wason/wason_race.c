//#############################################################################
// FILE:   app/wason/wason_race.c
// TITLE:  Wason Ware microgame RACE! + SURF!  (Cruis'n USA / Subway Surfers (shared road view))
//
//   Public: ww_RsInit, ww_RsUpdate, ww_RsRender, ww_SfInit, ww_SfUpdate, ww_SfRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// Shared pseudo-3D "behind the car / behind the runner" view for RACE! and
// ESCAPE!.  240x180 window, the road is drawn once; only small things move.
// Depth is a step k = 0 (horizon) .. 23 (right at the camera); objects advance
// in 1/16 steps (z16) and are redrawn only when k changes.
// ============================================================================
#define PS_W    240
#define PS_H    180
#define PS_HZ   62
#define PS_K    24
#define PS_CX   120

static int16_t ps_y[PS_K], ps_hw[PS_K], ps_t[PS_K];

static void PsInit(void)
{
    int16_t k;

    for(k = 0; k < PS_K; k++)
    {
        ps_t[k]  = (int16_t)(((int32_t)k * k * 64) / ((PS_K - 1) * (PS_K - 1)));
        ps_y[k]  = PS_HZ + (int16_t)(((int32_t)(PS_H - 1 - PS_HZ) * ps_t[k]) / 64);
        ps_hw[k] = 12 + (int16_t)(((int32_t)106 * ps_t[k]) / 64);
    }
}

static int16_t PsLaneX(int16_t lane, int16_t k) { return PS_CX + (lane - 1) * ((ps_hw[k] * 2) / 3); }

static void PsDrawRoad(uint32_t sky, uint32_t side, uint32_t road, uint32_t edge)
{
    int16_t y, hw;

    ww_Box(0, 0, PS_W, PS_HZ, sky);
    ww_Box(0, PS_HZ, PS_W, PS_H - PS_HZ, side);
    for(y = PS_HZ; y < PS_H; y += 3)
    {
        hw = 12 + (106 * (y - PS_HZ)) / (PS_H - 1 - PS_HZ);
        ww_Box(PS_CX - hw - 3, y, 3, 3, edge);
        ww_Box(PS_CX + hw,     y, 3, 3, edge);
        ww_Box(PS_CX - hw,     y, hw * 2, 3, road);
    }
}

// ============================================================================
// RACE!  (Cruis'n USA)  behind-the-car arcade racer. Stick/L/R steers, hold the
//        button for nitro. Weave through traffic until the clock runs out.
// ============================================================================
#define RS_GRASS 0x2E8B2EUL
#define RS_ROAD  0x454B55UL
#define RS_N     7
#define RS_CY    (PS_H - 6)                 // player car bottom

static int16_t rs_px, rs_ppx, rs_cd, rs_sp, rs_ds, rs_dk[10], rs_dl;
static int16_t rs_z[RS_N], rs_l[RS_N], rs_pk[RS_N], rs_c[RS_N];
static bool    rs_on[RS_N], rs_dr[RS_N], rs_nitro;

static const uint32_t kRsCar[4] = { COLOR_YELLOW, COLOR_CYAN, COLOR_WHITE, COLOR_ORANGE };

void ww_RsInit(uint16_t spd)
{
    int16_t i;

    PsInit();
    rs_px = rs_ppx = PS_CX;
    rs_cd = 14;
    rs_sp = 10 + ww_Min16((int16_t)spd, 4);
    rs_ds = 0;
    rs_nitro = false;
    for(i = 0; i < RS_N; i++) { rs_on[i] = rs_dr[i] = false; }
    for(i = 0; i < 10; i++) rs_dk[i] = -1;
}

int16_t ww_RsUpdate(void)
{
    int16_t i, k, dxs, step;

    dxs = (g_joy.dx * 7) / 100;
#if UI_BUTTONS_ALSO_MOVE
    if(g_dpad.l) dxs = -7;
    if(g_dpad.r) dxs =  7;
#endif
    rs_px = ww_Clamp16(rs_px + dxs, 24, PS_W - 24);
    rs_nitro = Ui_SelectHeld();
    step = rs_nitro ? rs_sp + 6 : rs_sp;
    rs_ds = (rs_ds + step) % (PS_K * 16);

    if(--rs_cd <= 0)
    {
        int16_t lane = ww_Rnd(3), mask = 0, q;

        for(q = 0; q < RS_N; q++)                               // lanes with a car just spawned
            if(rs_on[q] && (rs_z[q] >> 4) <= 9) mask |= (1 << rs_l[q]);
        if((mask | (1 << lane)) != 7)                           // always leave a lane open
            for(i = 0; i < RS_N; i++)
                if(!rs_on[i]) { rs_on[i] = true; rs_z[i] = 16; rs_l[i] = lane; rs_c[i] = ww_Rnd(4); rs_pk[i] = -1; break; }
        rs_cd = ww_Max16(8, 15 + ww_Rnd(10) - (rs_sp - 10));
    }
    for(i = 0; i < RS_N; i++)
    {
        if(!rs_on[i]) continue;
        rs_z[i] += step - 7;                                    // traffic drives, but slower than you
        k = rs_z[i] >> 4;
        if(k >= PS_K - 1) { rs_on[i] = false; continue; }
        if(k >= 20 && ww_Abs16(PsLaneX(rs_l[i], k) - rs_px) < 30) return MG_LOSE;
    }
    return MG_PLAY;
}

static void RsCarRect(int16_t k, int16_t lane, int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    *w = 4 + (ps_t[k] * 32) / 64;
    *h = 3 + (ps_t[k] * 22) / 64;
    *x = PsLaneX(lane, k) - *w / 2;
    *y = ps_y[k] - *h;
}

void ww_RsRender(void)
{
    int16_t i, j, k, x, y, w, h, ox, oy, ow, oh, dw, dh;
    bool    moved;

    if(ww_first)
    {
        PsDrawRoad(RS_SKY, RS_GRASS, RS_ROAD, COLOR_WHITE);
        ww_Box(30, 40, 40, 22, 0x4F7F5FUL);   ww_Box(110, 30, 56, 32, 0x4F7F5FUL);  ww_Box(170, 44, 50, 18, 0x4F7F5FUL);
        ww_Box(190, 14, 18, 18, COLOR_YELLOW);
        for(i = 0; i < 10; i++) rs_dk[i] = -1;
        for(i = 0; i < RS_N; i++) rs_pk[i] = -1;
        rs_ppx = -1;
    }

    // lane dashes: 5 per line, redrawn only when their depth step changes
    for(j = 0; j < 5; j++)
    {
        k = ((rs_ds + j * 80) % (PS_K * 16)) >> 4;
        if(rs_dk[j] != k)
        {
            if(rs_dk[j] >= 1)
            {
                dw = 1 + ps_t[rs_dk[j]] / 20;  dh = 2 + ps_t[rs_dk[j]] / 6;
                ww_Box(PS_CX - ps_hw[rs_dk[j]] / 3 - dw / 2, ps_y[rs_dk[j]] - dh, dw, dh, RS_ROAD);
                ww_Box(PS_CX + ps_hw[rs_dk[j]] / 3 - dw / 2, ps_y[rs_dk[j]] - dh, dw, dh, RS_ROAD);
            }
            if(k >= 1)
            {
                dw = 1 + ps_t[k] / 20;  dh = 2 + ps_t[k] / 6;
                ww_Box(PS_CX - ps_hw[k] / 3 - dw / 2, ps_y[k] - dh, dw, dh, COLOR_WHITE);
                ww_Box(PS_CX + ps_hw[k] / 3 - dw / 2, ps_y[k] - dh, dw, dh, COLOR_WHITE);
            }
            rs_dk[j] = k;
        }
    }

    for(i = 0; i < RS_N; i++)                                   // traffic
    {
        if(rs_on[i])
        {
            k = rs_z[i] >> 4;
            if(k != rs_pk[i])
            {
                RsCarRect(k, rs_l[i], &x, &y, &w, &h);
                ww_Box(x, y, w, h, kRsCar[rs_c[i]]);
                if(w >= 10) { ww_Box(x + w / 6, y + h / 5, (w * 2) / 3, h / 4, 0x203040UL);
                              ww_Box(x + 1, y + h - h / 5, w / 5 + 1, h / 5 + 1, COLOR_RED);
                              ww_Box(x + w - w / 5 - 2, y + h - h / 5, w / 5 + 1, h / 5 + 1, COLOR_RED); }
                if(rs_pk[i] >= 0)
                {
                    RsCarRect(rs_pk[i], rs_l[i], &ox, &oy, &ow, &oh);
                    ww_EraseOutside(ox, oy, ow, oh, x, y, w, h, RS_ROAD);
                }
                rs_pk[i] = k;
                rs_dr[i] = true;
            }
        }
        else if(rs_dr[i] && rs_pk[i] >= 0)
        {
            RsCarRect(rs_pk[i], rs_l[i], &ox, &oy, &ow, &oh);
            ww_Box(ox, oy, ow, oh, RS_ROAD);
            rs_dr[i] = false;
        }
    }

    moved = (rs_px != rs_ppx) || rs_nitro;                      // player car (rear view)
    if(!moved)                                                  // a dash/car passing underneath dirties it
        for(j = 0; j < 5; j++) if(rs_dk[j] >= 19) moved = true;
    if(moved)
    {
        ww_Box(rs_px - 20, RS_CY - 26, 40, 5, COLOR_RED);                      // spoiler
        ww_Box(rs_px - 18, RS_CY - 21, 36, 15, 0xE53935UL);                    // body
        ww_Box(rs_px - 12, RS_CY - 20, 24, 7, 0x203040UL);                     // rear glass
        ww_Box(rs_px - 20, RS_CY - 12, 7, 12, COLOR_BLACK);                    // wheels
        ww_Box(rs_px + 13, RS_CY - 12, 7, 12, COLOR_BLACK);
        ww_Box(rs_px - 16, RS_CY - 8, 6, 3, COLOR_ORANGE);                     // tail lights
        ww_Box(rs_px + 10, RS_CY - 8, 6, 3, COLOR_ORANGE);
        if(rs_nitro) { ww_Box(rs_px - 8, RS_CY, 5, 4, COLOR_ORANGE); ww_Box(rs_px + 3, RS_CY, 5, 4, COLOR_ORANGE); }
        if(rs_ppx >= 0 && rs_px != rs_ppx)
            ww_EraseOutside(rs_ppx - 20, RS_CY - 26, 40, 30, rs_px - 20, RS_CY - 26, 40, 30, RS_ROAD);
        else if(!rs_nitro) ww_Box(rs_px - 8, RS_CY, 16, 4, RS_ROAD);
        rs_ppx = rs_px;
    }
}

// ============================================================================
// SURF!  (Subway Surfers)  run the rails. STICK left/right switches lane, BLUE jumps the
//        barriers, STICK down slides under the signs, grab coins, and stay out of
//        the way of the trains. Survive.   (3D view, shared with RACE!)
// ============================================================================
#define SF_SIDE  0x5E6270UL
#define SF_ROAD  0x7A6F66UL
#define SF_N     5

static int16_t sf_lane, sf_x, sf_px, sf_jy, sf_jvy, sf_slide, sf_cd, sf_sp, sf_pjy, sf_pslide, sf_pf;
static int16_t sf_z[SF_N], sf_l[SF_N], sf_ty[SF_N], sf_pk[SF_N], sf_c[SF_N];
static bool    sf_on[SF_N], sf_dr[SF_N];

static const uint32_t kSfTrain[3] = { 0x2E7D32UL, 0xD84315UL, 0x1565C0UL };

static int16_t SfX(int16_t lane) { return PsLaneX(lane, PS_K - 1); }

void ww_SfInit(uint16_t spd)
{
    int16_t i;

    PsInit();
    sf_lane = 1;  sf_x = sf_px = SfX(1);
    sf_jy = sf_jvy = 0;  sf_slide = 0;
    sf_pjy = 0;  sf_pslide = 0;  sf_pf = -1;
    sf_cd = 10;
    sf_sp = 12 + ww_Min16((int16_t)spd, 4);
    for(i = 0; i < SF_N; i++) { sf_on[i] = sf_dr[i] = false; }
}

int16_t ww_SfUpdate(void)
{
    int16_t i, j, k, t, mask;

    if(ww_EdgeL() && sf_lane > 0) { sf_lane--; Buzzer_PlaySfx(&sfx_blip); }
    if(ww_EdgeR() && sf_lane < 2) { sf_lane++; Buzzer_PlaySfx(&sfx_blip); }
    if(sf_jy == 0 && sf_slide == 0 && Ui_Jump()) { sf_jvy = 9; Buzzer_PlaySfx(&sfx_jump); }
    if(sf_jy == 0 && sf_jvy == 0 && ww_EdgeD()) sf_slide = 18;

    if(sf_jvy != 0 || sf_jy > 0)
    {
        sf_jy += sf_jvy;  sf_jvy--;
        if(sf_jy <= 0) { sf_jy = 0; sf_jvy = 0; }
    }
    if(sf_slide > 0) sf_slide--;
    sf_x += ww_Clamp16(SfX(sf_lane) - sf_x, -9, 9);

    if(--sf_cd <= 0)
    {
        for(i = 0; i < SF_N; i++)
            if(!sf_on[i])
            {
                t = ww_Rnd(10);
                t = (t < 3) ? 0 : ((t < 5) ? 1 : ((t < 7) ? 2 : 3));            // train, barrier, sign, coin
                j = ww_Rnd(3);
                mask = 0;                                                      // never wall off all 3 lanes
                for(k = 0; k < SF_N; k++)
                    if(sf_on[k] && sf_ty[k] != 3 && (sf_z[k] >> 4) <= 9) mask |= (1 << sf_l[k]);
                if(t != 3 && (mask | (1 << j)) == 7) t = 3;
                sf_on[i] = true; sf_ty[i] = t; sf_l[i] = j; sf_z[i] = 16; sf_pk[i] = -1; sf_dr[i] = false;
                sf_c[i] = ww_Rnd(3);
                break;
            }
        sf_cd = ww_Max16(9, 18 + ww_Rnd(12) - 2 * (sf_sp - 12));
    }

    for(i = 0; i < SF_N; i++)
    {
        if(!sf_on[i]) continue;
        sf_z[i] += sf_sp;
        k = sf_z[i] >> 4;
        if(k >= PS_K - 1) { sf_on[i] = false; continue; }
        if(k >= 21 && sf_l[i] == sf_lane)
        {
            if(sf_ty[i] == 3)      { sf_on[i] = false; Buzzer_PlaySfx(&sfx_coin); }
            else if(sf_ty[i] == 0) return MG_LOSE;                              // train
            else if(sf_ty[i] == 1 && sf_jy < 12) return MG_LOSE;                // barrier
            else if(sf_ty[i] == 2 && sf_slide == 0) return MG_LOSE;             // overhead sign
        }
    }
    return MG_PLAY;
}

static void SfObjRect(int16_t i, int16_t k, int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    int16_t laneW = (ps_hw[k] * 2) / 3, t = ps_t[k], ty = sf_ty[i];

    *x = PsLaneX(sf_l[i], k);
    if(ty == 0)      { *w = (laneW * 9) / 10; *h = 6 + (t * 48) / 64; *y = ps_y[k] - *h; }
    else if(ty == 1) { *w = (laneW * 3) / 4;  *h = 2 + (t * 14) / 64; *y = ps_y[k] - *h; }
    else if(ty == 2) { *w = laneW;            *h = 2 + (t * 10) / 64; *y = ps_y[k] - (t * 40) / 64 - *h; }
    else             { *w = 2 + (t * 14) / 64; *h = *w;                *y = ps_y[k] - (t * 12) / 64 - *h; }
    *x -= *w / 2;
}

static void SfDrawObj(int16_t i, int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t ty = sf_ty[i];

    if(ty == 0)                                                 // subway train: body, windscreen, headlights
    {
        ww_Box(x, y, w, h, kSfTrain[sf_c[i]]);
        if(w >= 12)
        {
            ww_Box(x + w / 8, y + h / 8, (w * 3) / 4, h / 3, 0x203040UL);
            ww_Box(x + w / 8, y + (h * 3) / 4, w / 6 + 1, h / 8 + 1, COLOR_YELLOW);
            ww_Box(x + w - w / 8 - w / 6 - 1, y + (h * 3) / 4, w / 6 + 1, h / 8 + 1, COLOR_YELLOW);
        }
    }
    else if(ty == 1)                                            // red/white striped barrier
    {
        ww_Box(x, y, w, h, COLOR_RED);
        ww_Box(x + w / 4, y, w / 5 + 1, h, COLOR_WHITE);
        ww_Box(x + (w * 3) / 5, y, w / 5 + 1, h, COLOR_WHITE);
    }
    else if(ty == 2)                                            // overhead sign
    {
        ww_Box(x, y, w, h, 0x1565C0UL);
        if(h >= 6) ww_Box(x + 2, y + h / 4, w - 4, h / 2, COLOR_WHITE);
    }
    else ww_Box(x, y, w, h, 0xFFD700UL);                           // coin
}

// Repaint the railway (ballast, ties, rails) inside a rectangle. Used instead of a
// flat fill whenever something moves off the track, so the rails survive.
static void SfBoxIn(int16_t rx, int16_t ry, int16_t rw, int16_t rh, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c)
{
    int16_t x0 = ww_Max16(rx, x), y0 = ww_Max16(ry, y), x1 = ww_Min16(rx + rw, x + w), y1 = ww_Min16(ry + rh, y + h);

    if(x1 > x0 && y1 > y0) ww_Box(x0, y0, x1 - x0, y1 - y0, c);
}

static void SfPaintTrack(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t yy, hw, i, cx;

    if(y < PS_HZ) SfBoxIn(x, y, w, h, 0, 0, PS_W, PS_HZ, SF_SKY);
    for(yy = PS_HZ; yy < PS_H; yy += 3)                         // same 3 px bands PsDrawRoad() used
    {
        if(yy + 3 <= y || yy >= y + h) continue;
        hw = 12 + (106 * (yy - PS_HZ)) / (PS_H - 1 - PS_HZ);
        SfBoxIn(x, y, w, h, 0,               yy, PS_W,   3, SF_SIDE);
        SfBoxIn(x, y, w, h, PS_CX - hw - 3,  yy, 3,      3, 0xA0A0A8UL);
        SfBoxIn(x, y, w, h, PS_CX + hw,      yy, 3,      3, 0xA0A0A8UL);
        SfBoxIn(x, y, w, h, PS_CX - hw,      yy, hw * 2, 3, SF_ROAD);
    }
    for(yy = PS_HZ + 4; yy < PS_H; yy += 6)                     // ties and rails
    {
        if(yy + 6 <= y || yy >= y + h) continue;
        hw = 12 + (106 * (yy - PS_HZ)) / (PS_H - 1 - PS_HZ);
        SfBoxIn(x, y, w, h, PS_CX - hw, yy, hw * 2, 2, 0x4A4038UL);
        for(i = 0; i < 3; i++)
        {
            cx = PS_CX + (i - 1) * ((hw * 2) / 3);
            SfBoxIn(x, y, w, h, cx - hw / 10 - 1, yy, 2, 6, 0xC8C8D0UL);
            SfBoxIn(x, y, w, h, cx + hw / 10, yy, 2, 6, 0xC8C8D0UL);
        }
    }
}

// Like EraseOutside(), but the vacated pixels get the track repainted.
static void SfEraseOutside(int16_t ox, int16_t oy, int16_t ow, int16_t oh,
                           int16_t nx, int16_t ny, int16_t nw, int16_t nh)
{
    int16_t ix0 = ww_Max16(ox, nx), iy0 = ww_Max16(oy, ny);
    int16_t ix1 = ww_Min16(ox + ow, nx + nw), iy1 = ww_Min16(oy + oh, ny + nh);

    if(ix0 >= ix1 || iy0 >= iy1) { SfPaintTrack(ox, oy, ow, oh); return; }
    if(iy0 > oy)           SfPaintTrack(ox, oy, ow, iy0 - oy);
    if(iy1 < oy + oh)      SfPaintTrack(ox, iy1, ow, oy + oh - iy1);
    if(ix0 > ox)           SfPaintTrack(ox, iy0, ix0 - ox, iy1 - iy0);
    if(ix1 < ox + ow)      SfPaintTrack(ix1, iy0, ox + ow - ix1, iy1 - iy0);
}

void ww_SfRender(void)
{
    int16_t i, k, x, y, w, h, ox, oy, ow, oh, fy, pose, hw, cx;

    if(ww_first)
    {
        PsDrawRoad(SF_SKY, SF_SIDE, SF_ROAD, 0xA0A0A8UL);
        ww_Box(8, 28, 28, 34, 0x7A7A90UL);   ww_Box(40, 38, 22, 24, 0x6A6A80UL);  ww_Box(66, 22, 26, 40, 0x808098UL);   // skyline
        ww_Box(150, 30, 30, 32, 0x7A7A90UL); ww_Box(184, 40, 24, 22, 0x6A6A80UL); ww_Box(212, 26, 22, 36, 0x808098UL);
        for(y = PS_HZ + 4; y < PS_H; y += 6)                                // railway: ties + 2 rails per lane
        {
            hw = 12 + (106 * (y - PS_HZ)) / (PS_H - 1 - PS_HZ);
            ww_Box(PS_CX - hw, y, hw * 2, 2, 0x4A4038UL);
            for(i = 0; i < 3; i++)
            {
                cx = PS_CX + (i - 1) * ((hw * 2) / 3);
                ww_Box(cx - hw / 10 - 1, y, 2, 6, 0xC8C8D0UL);
                ww_Box(cx + hw / 10, y, 2, 6, 0xC8C8D0UL);
            }
        }
        for(i = 0; i < SF_N; i++) sf_pk[i] = -1;
        sf_pf = -1;
    }

    for(i = 0; i < SF_N; i++)                                   // objects: new first, then the leftovers
    {
        if(sf_on[i])
        {
            k = sf_z[i] >> 4;
            if(k != sf_pk[i])
            {
                SfObjRect(i, k, &x, &y, &w, &h);
                SfDrawObj(i, x, y, w, h);
                if(sf_pk[i] >= 0)
                {
                    SfObjRect(i, sf_pk[i], &ox, &oy, &ow, &oh);
                    SfEraseOutside(ox, oy, ow, oh, x, y, w, h);
                }
                sf_pk[i] = k;  sf_dr[i] = true;
            }
        }
        else if(sf_dr[i] && sf_pk[i] >= 0)
        {
            SfObjRect(i, sf_pk[i], &ox, &oy, &ow, &oh);
            SfPaintTrack(ox, oy, ow, oh);
            sf_dr[i] = false;
        }
    }

    fy = PS_H - 4 - sf_jy;
    pose = (ww_frame >> 2) & 1;
    if(ww_first || sf_x != sf_px || sf_jy != sf_pjy || (sf_slide > 0) != (sf_pslide > 0) || pose != sf_pf)
    {
        int16_t oldFy = PS_H - 4 - sf_pjy;
        int16_t rx0 = ww_Min16(sf_px, sf_x) - 16, ry0 = ww_Min16(oldFy, fy) - 46;

        SfPaintTrack(rx0, ry0, ww_Abs16(sf_px - sf_x) + 34, ww_Abs16(oldFy - fy) + 48);    // old spot back to track...
        if(sf_slide > 0)
        {
            ww_Box(sf_x - 16, fy - 24, 32, 8, 0x1565C0UL);          // cap
            ww_Box(sf_x - 14, fy - 16, 28, 16, 0xF57C00UL);         // sliding on his back
            ww_Box(sf_x - 12, fy - 4, 24, 4, 0x3949ABUL);
        }
        else ww_DrawArt(sf_x - 14, fy - 44, pose ? ww_kArtRun1 : ww_kArtRun0, 14, 22, 2, SF_ROAD, false);   // ...then him on top
        sf_px = sf_x;  sf_pjy = sf_jy;  sf_pslide = sf_slide;  sf_pf = pose;
    }
}
