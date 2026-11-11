//#############################################################################
// FILE:   app/wason/wason_run.c
// TITLE:  Wason Ware microgame RUN!  (Sonic)
//
//   Public: ww_SoInit, ww_SoUpdate, ww_SoRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// RUN!  (Sonic the Hedgehog)  Green Hill Zone. Any button = spin jump, DOWN =
//       roll (flattens Motobugs). Grab 5 rings, stomp Motobugs from above,
//       dodge the spikes, bounce off the springs for the high rings.
//       Sprites are transparent over a painted scenery (hills, clouds, palms).
// ============================================================================
#define SO_GY   126                         // ground top (window-local)
#define SO_X    60
#define SO_N    4

typedef struct { int16_t x, y, w, h; uint32_t c; } SoRect;
static const SoRect kSoBg[] =
{
    { 30, 16, 46, 10, 0xFFFFFFUL }, { 40, 10, 28, 8, 0xFFFFFFUL },          // clouds
    { 190, 24, 52, 10, 0xFFFFFFUL }, { 200, 18, 32, 8, 0xFFFFFFUL }, { 280, 12, 36, 8, 0xFFFFFFUL },
    { 10, 84, 100, 42, 0x2E9E6BUL }, { 24, 76, 72, 8, 0x2E9E6BUL },         // far hills
    { 150, 70, 130, 56, 0x2E9E6BUL }, { 168, 60, 94, 10, 0x2E9E6BUL }, { 290, 92, 40, 34, 0x2E9E6BUL },
    { 60, 100, 80, 26, 0x3CC06AUL }, { 74, 94, 52, 6, 0x3CC06AUL },         // near hills
    { 230, 96, 90, 30, 0x3CC06AUL }, { 244, 90, 62, 6, 0x3CC06AUL },
};
static const int16_t kSoDecX[5] = { 14, 100, 168, 246, 296 };                // static palms / sunflowers
static const int16_t kSoDecT[5] = {  0,   1,   0,   1,   0 };

static int16_t so_y, so_vy, so_py, so_v, so_cd, so_rings, so_ph, so_pend, so_chain, so_chr, so_roll, so_dRings;
static int16_t so_pw1, so_ph1, so_pt1;
static int16_t so_x[SO_N], so_px[SO_N], so_ty[SO_N], so_t[SO_N], so_h[SO_N], so_w[SO_N];
static bool    so_on[SO_N], so_dr[SO_N];

static const char *const *SoArt(int16_t t)
{
    if(t == 0) return ww_kArtRing;
    if(t == 1) return ww_kArtMotobug;
    if(t == 2) return ww_kArtSpikes;
    return ww_kArtSpring;
}

static void SoDecor(int16_t i)
{
    if(kSoDecT[i] == 0) ww_DrawArt(kSoDecX[i], SO_GY - 30, ww_kArtPalm, 20, 30, 1, SO_BG, false);
    else                ww_DrawArt(kSoDecX[i], SO_GY - 20, ww_kArtSunflower, 12, 20, 1, SO_BG, false);
}

// Repaint the scenery inside a window-local rectangle.
static void SoPaint(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t i, ix0, iy0, ix1, iy1, dw, dh;

    ww_Box(x, y, w, h, SO_BG);
    for(i = 0; i < (int16_t)(sizeof(kSoBg) / sizeof(kSoBg[0])); i++)
    {
        ix0 = ww_Max16(x, kSoBg[i].x);  iy0 = ww_Max16(y, kSoBg[i].y);
        ix1 = ww_Min16(x + w, kSoBg[i].x + kSoBg[i].w);  iy1 = ww_Min16(y + h, kSoBg[i].y + kSoBg[i].h);
        if(ix0 < ix1 && iy0 < iy1) ww_Box(ix0, iy0, ix1 - ix0, iy1 - iy0, kSoBg[i].c);
    }
    for(i = 0; i < 5; i++)
    {
        dw = kSoDecT[i] ? 12 : 20;  dh = kSoDecT[i] ? 20 : 30;
        if(ww_Hit(x, y, w, h, kSoDecX[i], SO_GY - dh, dw, dh)) SoDecor(i);
    }
}

void ww_SoInit(uint16_t spd)
{
    uint16_t i;

    so_y = so_py = SO_GY - 20;
    so_vy = 0;
    so_v  = 5 + ww_Min16((int16_t)spd / 2, 3);
    so_cd = 12;
    so_rings = 0;  so_dRings = -1;
    so_ph = 0;  so_pend = 0;  so_chain = 0;  so_chr = 0;  so_roll = 0;
    so_pw1 = 16;  so_ph1 = 20;  so_pt1 = SO_GY - 20;
    for(i = 0; i < SO_N; i++) { so_on[i] = false; so_dr[i] = false; }
}

int16_t ww_SoUpdate(void)
{
    uint16_t i, j;
    int16_t  r, t = 0, hh, ww, hy, hh2;
    bool     air;

    air = (so_y < SO_GY - 20);
    if(!air && so_roll == 0 && ww_EdgeD()) { so_roll = 26; Buzzer_PlaySfx(&sfx_laser); }    // spin roll
    else if(!air && (g_joy.btnPressed || g_dpad.uPressed || g_dpad.lPressed || g_dpad.rPressed))
    { so_vy = -11; so_roll = 0; Buzzer_PlaySfx(&sfx_jump); }
    if(so_roll > 0) so_roll--;
    so_vy++;
    so_y += so_vy;
    if(so_y >= SO_GY - 20) { so_y = SO_GY - 20; so_vy = 0; }
    air = (so_y < SO_GY - 20);
    if((ww_frame & 1) == 0) so_ph = (so_ph + 1) & 3;

    if(--so_cd <= 0)
    {
        for(i = 0; i < SO_N; i++)
            if(!so_on[i])
            {
                if(so_pend > 0)       { t = 0; so_pend--; r = 2; }                 // high rings after a spring
                else if(so_chain > 0) { t = 0; so_chain--; r = so_chr; }           // rest of a ring chain
                else
                {
                    r = ww_Rnd(8);
                    t = (r < 3) ? 0 : ((r < 5) ? 1 : ((r < 7) ? 2 : 3));
                    if(t == 0) { so_chr = ww_Rnd(2); r = so_chr; so_chain = 2; }       // a chain of 3 rings
                    else r = 0;
                }
                hh = (t == 1) ? 12 : 10;
                ww = (t == 0) ? 10 : ((t == 1) ? 14 : ((t == 2) ? 16 : 14));
                so_on[i] = true; so_t[i] = t; so_h[i] = hh; so_w[i] = ww;
                so_ty[i] = (t == 0) ? ((r == 2) ? SO_GY - 84 : ((r == 1) ? SO_GY - 50 : SO_GY - 24)) : SO_GY - hh;
                so_x[i] = so_px[i] = 322;
                if(t == 3) so_pend = 2;
                break;
            }
        so_cd = (so_pend > 0 || so_chain > 0) ? 5 : ((t == 0 || t == 3) ? 14 + ww_Rnd(8) : 26 + ww_Rnd(10));
    }

    hy = (so_roll > 0 && !air) ? so_y + 6 : so_y + 2;
    hh2 = (so_roll > 0 && !air) ? 12 : (air ? 14 : 16);
    for(i = 0; i < SO_N; i++)
    {
        if(!so_on[i]) continue;
        so_x[i] -= so_v;
        if(so_x[i] < -20) { so_on[i] = false; continue; }
        if(!ww_Hit(SO_X + 2, hy, 12, hh2, so_x[i], so_ty[i], so_w[i], so_h[i])) continue;

        if(so_t[i] == 0)
        {
            so_on[i] = false;
            Buzzer_PlaySfx(&sfx_coin);
            if(++so_rings >= 5) return MG_WIN;
        }
        else if(so_t[i] == 1)                                   // Motobug: roll through it or land on it
        {
            if((so_roll > 0 && !air) || (so_vy > 0 && so_y + 20 - so_vy <= so_ty[i] + 4))
            {
                so_on[i] = false;  if(air) so_vy = -8;
                Buzzer_PlaySfx(&sfx_explosion);
                for(j = 0; j < SO_N; j++) if(so_on[j] && so_t[j] == 0 && so_x[j] > 200) { so_on[j] = false; }
                if(++so_rings >= 5) return MG_WIN;
            }
            else return MG_LOSE;
        }
        else if(so_t[i] == 2) return MG_LOSE;                   // spikes
        else if(so_vy >= 0)                                     // spring
        {
            so_vy = -15;  so_on[i] = false;
            Buzzer_PlaySfx(&sfx_jump);
        }
    }
    return MG_PLAY;
}

void ww_SoRender(void)
{
    uint16_t i, k;
    int16_t  top, w, h, rx, ry, rw, rh, t;
    bool     air = (so_y < SO_GY - 20), ball = air || so_roll > 0, redo;
    const char *const *art;

    if(ww_first)
    {
        ww_Box(0, SO_GY, 320, 6, 0x58D03CUL);
        ww_Box(0, SO_GY + 6, 320, 28, 0xC68E4AUL);
        for(i = 0; i < 16; i++) { ww_Box(i * 20, SO_GY + 6, 10, 14, 0x8B5A2BUL); ww_Box(i * 20 + 10, SO_GY + 20, 10, 14, 0x8B5A2BUL); }
        SoPaint(0, 0, 320, SO_GY);
        so_pt1 = SO_GY - 20;  so_pw1 = 16;  so_ph1 = 20;
        so_dRings = -1;
    }

    for(i = 0; i < SO_N; i++)                                   // items: repaint old spot, draw new (back to back)
    {
        if(so_on[i])
        {
            if(so_x[i] != so_px[i] || !so_dr[i])
            {
                if(so_dr[i]) SoPaint(so_px[i], so_ty[i], so_w[i], so_h[i]);
                ww_DrawArt(so_x[i], so_ty[i], SoArt(so_t[i]), so_w[i], so_h[i], 1, SO_BG, false);
                so_px[i] = so_x[i];  so_dr[i] = true;
            }
        }
        else if(so_dr[i]) { SoPaint(so_px[i], so_ty[i], so_w[i], so_h[i]); so_dr[i] = false; }
    }

    if(ball) { art = ww_kArtSpin; w = 16; h = 16; top = so_y + (air ? 2 : 4); }
    else
    {
        w = 16; h = 20; top = so_y;
        t = (so_v >= 7 && (so_ph & 2)) ? 2 : (so_ph & 1);        // peel-out blur once he's really moving
        art = (t == 2) ? ww_kArtSonic2 : (t ? ww_kArtSonic1 : ww_kArtSonic0);
    }
    redo = ww_first || top != so_pt1 || h != so_ph1 || (!ball);   // running legs animate every frame
    for(i = 0; i < SO_N && !redo; i++)
        if(so_dr[i] && ww_Hit(SO_X - 4, top - 2, 24, h + 4, so_px[i], so_ty[i], so_w[i], so_h[i])) redo = true;
    if(redo)
    {
        ry = ww_Min16(top, so_pt1);  rh = ww_Max16(top + h, so_pt1 + so_ph1) - ry;  rx = SO_X;  rw = 16;
        SoPaint(rx, ry, rw, rh);
        for(i = 0; i < SO_N; i++)                               // items that were under his box come back
            if(so_dr[i] && ww_Hit(rx, ry, rw, rh, so_px[i], so_ty[i], so_w[i], so_h[i]))
                ww_DrawArt(so_px[i], so_ty[i], SoArt(so_t[i]), so_w[i], so_h[i], 1, SO_BG, false);
        ww_DrawArt(SO_X, top, art, w, h, 1, SO_BG, false);
        so_pt1 = top;  so_ph1 = h;
    }

    if(so_dRings != so_rings)                                   // HUD: rings collected
    {
        for(k = 0; k < 5; k++)
        {
            ww_Box(4 + k * 14, 2, 12, 12, SO_BG);
            if((int16_t)k < so_rings) ww_DrawArt(5 + k * 14, 3, ww_kArtRing, 10, 10, 1, SO_BG, false);
        }
        so_dRings = so_rings;
    }
}
