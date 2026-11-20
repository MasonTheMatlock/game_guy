//#############################################################################
// FILE:   app/wason/wason_blast.c
// TITLE:  Wason Ware microgame BLAST!  (Metroid)
//
//   Public: ww_MtInit, ww_MtUpdate, ww_MtRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// BLAST!  (Metroid)  Aim the arm cannon with the STICK, fire with BLUE.
//         Freeze 5 of the creatures before one reaches Samus.
//         Metroids take 2 hits; Reo bats are fast and wavy.
// ============================================================================
#define MT_N    4
#define MT_CX   40                          // cannon muzzle origin
#define MT_CY   104

static int16_t mt_ax, mt_ay, mt_dvx, mt_dvy, mt_kills, mt_cd, mt_spd;
static int16_t mt_ex[MT_N], mt_ey[MT_N], mt_pex[MT_N], mt_pey[MT_N], mt_et[MT_N], mt_hp[MT_N], mt_fl[MT_N];
static int16_t mt_bx[5], mt_by[5], mt_bvx[5], mt_bvy[5], mt_pbx[5], mt_pby[5];
static bool    mt_eon[MT_N], mt_edr[MT_N], mt_bon[5], mt_bdr[5], mt_aimDirty;

static void MtAim(void)                                         // stick -> unit-ish vector (|max| = 8)
{
    int16_t s = ww_Max16(ww_Abs16(g_joy.dx), ww_Abs16(g_joy.dy));

    if(s >= 25)
    {
        mt_ax = (int16_t)(((int32_t)g_joy.dx * 8) / s);
        mt_ay = (int16_t)(((int32_t)g_joy.dy * 8) / s);
        if(mt_ax < 2 && mt_ax > -2 && mt_ay < 2 && mt_ay > -2) mt_ax = 8;
    }
}

void ww_MtInit(uint16_t spd)
{
    int16_t i;

    mt_ax = 8; mt_ay = 0; mt_dvx = mt_dvy = 99;
    mt_kills = 0;
    mt_cd = 15;
    mt_spd = 1 + ww_Min16((int16_t)spd / 3, 1);
    mt_aimDirty = true;
    for(i = 0; i < MT_N; i++) { mt_eon[i] = mt_edr[i] = false; mt_fl[i] = 0; }
    for(i = 0; i < 5; i++) { mt_bon[i] = mt_bdr[i] = false; }
}

int16_t ww_MtUpdate(void)
{
    int16_t i, j, w, h;

    MtAim();
    if(mt_ax != mt_dvx || mt_ay != mt_dvy) mt_aimDirty = true;

    if(Ui_Select())
        for(i = 0; i < 5; i++)
            if(!mt_bon[i])
            {
                mt_bon[i] = true;  mt_bx[i] = mt_pbx[i] = MT_CX + mt_ax * 2;  mt_by[i] = mt_pby[i] = MT_CY + mt_ay * 2;
                mt_bvx[i] = mt_ax;  mt_bvy[i] = mt_ay;
                Buzzer_PlaySfx(&sfx_laser);
                break;
            }

    if(--mt_cd <= 0)
    {
        for(i = 0; i < MT_N; i++)
            if(!mt_eon[i])
            {
                mt_eon[i] = true;  mt_et[i] = (ww_Rnd(3) == 0) ? 1 : 0;
                mt_ex[i] = mt_pex[i] = 324;  mt_ey[i] = mt_pey[i] = 20 + ww_Rnd(170);
                mt_hp[i] = mt_et[i] ? 1 : 2;  mt_fl[i] = 0;
                break;
            }
        mt_cd = ww_Max16(22, 44 + ww_Rnd(24) - 8 * mt_spd);
    }

    for(i = 0; i < MT_N; i++)
    {
        if(!mt_eon[i]) continue;
        if(mt_fl[i] > 0) mt_fl[i]--;
        mt_ex[i] -= mt_et[i] ? mt_spd + 2 : mt_spd + 1;
        if(((ww_frame + i * 7) & 3) == 0)                         // drift toward Samus, bats weave
        {
            if(mt_ey[i] < 100) mt_ey[i]++; else if(mt_ey[i] > 110) mt_ey[i]--;
            if(mt_et[i]) mt_ey[i] += ((ww_frame >> 3) & 1) ? 3 : -3;
        }
        w = mt_et[i] ? 12 : 16;  h = mt_et[i] ? 10 : 14;
        if(ww_Hit(mt_ex[i], mt_ey[i], w * 2, h * 2, 8, 88, 30, 44)) return MG_LOSE;
    }

    for(j = 0; j < 5; j++)
    {
        if(!mt_bon[j]) continue;
        mt_bx[j] += mt_bvx[j];  mt_by[j] += mt_bvy[j];
        if(mt_bx[j] < 0 || mt_bx[j] > 330 || mt_by[j] < -6 || mt_by[j] > 226) { mt_bon[j] = false; continue; }
        for(i = 0; i < MT_N; i++)
        {
            if(!mt_eon[i]) continue;
            w = mt_et[i] ? 12 : 16;  h = mt_et[i] ? 10 : 14;
            if(ww_Hit(mt_bx[j] - 3, mt_by[j] - 3, 7, 7, mt_ex[i], mt_ey[i], w * 2, h * 2))
            {
                mt_bon[j] = false;  mt_fl[i] = 3;
                if(--mt_hp[i] <= 0)
                {
                    mt_eon[i] = false;
                    Buzzer_PlaySfx(&sfx_explosion);
                    if(++mt_kills >= 5) return MG_WIN;
                }
                else Buzzer_PlaySfx(&sfx_blip);
                break;
            }
        }
    }
    return MG_PLAY;
}

static void MtBarrel(int16_t vx, int16_t vy, bool on)
{
    int16_t k;
    for(k = 1; k <= 4; k++)
        ww_Box(MT_CX + vx * k - 2, MT_CY + vy * k - 2, 5, 5, on ? COLOR_ORANGE : MT_BG);
    ww_Box(MT_CX + vx * 8 - 1, MT_CY + vy * 8 - 1, 3, 3, on ? COLOR_YELLOW : MT_BG);          // sight
}

void ww_MtRender(void)
{
    int16_t i;

    if(ww_first)
    {
        ww_DrawArt(8, 88, ww_kArtSamus, 14, 22, 2, MT_BG, true);
        mt_dvx = mt_dvy = 99;  mt_aimDirty = true;
        for(i = 0; i < MT_N; i++) mt_edr[i] = false;
    }
    if(mt_aimDirty)
    {
        if(mt_dvx != 99) MtBarrel(mt_dvx, mt_dvy, false);
        MtBarrel(mt_ax, mt_ay, true);
        mt_dvx = mt_ax;  mt_dvy = mt_ay;
        mt_aimDirty = false;
    }

    for(i = 0; i < MT_N; i++)
    {
        if(mt_eon[i])
        {
            if(mt_edr[i]) ww_MoveArt(mt_pex[i], mt_pey[i], mt_ex[i], mt_ey[i], mt_et[i] ? ww_kArtReo : ww_kArtMetroid,
                                  mt_et[i] ? 12 : 16, mt_et[i] ? 10 : 14, 2, MT_BG);
            else          ww_DrawArt(mt_ex[i], mt_ey[i], mt_et[i] ? ww_kArtReo : ww_kArtMetroid,
                                  mt_et[i] ? 12 : 16, mt_et[i] ? 10 : 14, 2, MT_BG, true);
            if(mt_fl[i] > 0) ww_Box(mt_ex[i] + 8, mt_ey[i] + 6, 6, 6, COLOR_WHITE);
            mt_pex[i] = mt_ex[i];  mt_pey[i] = mt_ey[i];  mt_edr[i] = true;
        }
        else if(mt_edr[i]) { ww_Box(mt_pex[i], mt_pey[i], 32, 28, MT_BG); mt_edr[i] = false; }
    }
    for(i = 0; i < 5; i++)
    {
        if(mt_bdr[i]) { ww_Box(mt_pbx[i] - 3, mt_pby[i] - 3, 7, 7, MT_BG); mt_bdr[i] = false; }
        if(mt_bon[i])
        {
            ww_Box(mt_bx[i] - 2, mt_by[i] - 2, 5, 5, COLOR_CYAN);
            mt_pbx[i] = mt_bx[i];  mt_pby[i] = mt_by[i];  mt_bdr[i] = true;
        }
    }
}
