//#############################################################################
// FILE:   app/wason/wason_plant.c
// TITLE:  Wason Ware microgame PLANT!  (Plants vs Zombies)
//
//   Public: ww_PzInit, ww_PzUpdate, ww_PzRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// PLANT!  (Plants vs Zombies)  the STICK (up/down) picks a lane, BLUE plants a
//         Peashooter at the house end of it. Zombies EAT plants, so keep
//         replanting. Stop 5 before one gets in the house.
// ============================================================================
#define PZ_LH   56                          // lane height
#define PZ_HX   26                          // house strip width
#define PZ_N    4

static int16_t pz_lane, pz_dlane, pz_kills, pz_cd, pz_spd, pz_spw, pz_cool;
static int16_t pz_zx[PZ_N], pz_zl[PZ_N], pz_zhp[PZ_N], pz_zpx[PZ_N], pz_zfl[PZ_N];
static bool    pz_zon[PZ_N], pz_zdr[PZ_N];
static bool    pz_plant[3], pz_pdr[3], pz_erase[3];
static int16_t pz_fire[3], pz_php[3];
static int16_t pz_pxp[8], pz_pl[8], pz_ppx[8];
static bool    pz_pon[8], pz_pdr2[8];

static uint32_t PzBg(int16_t lane) { return (lane & 1) ? PZ_BG1 : PZ_BG0; }

void ww_PzInit(uint16_t spd)
{
    int16_t i;

    pz_lane = 1; pz_dlane = -1; pz_kills = 0;
    pz_cd = 12;  pz_cool = 0;
    pz_spd = 4 + ww_Min16((int16_t)spd / 3, 2);
    pz_spw = ww_Max16(20, 38 - 3 * (int16_t)spd);
    for(i = 0; i < PZ_N; i++) { pz_zon[i] = pz_zdr[i] = false; pz_zfl[i] = 0; }
    for(i = 0; i < 3; i++) { pz_plant[i] = pz_pdr[i] = pz_erase[i] = false; pz_fire[i] = 20; pz_php[i] = 0; }
    for(i = 0; i < 8; i++) { pz_pon[i] = pz_pdr2[i] = false; }
}

int16_t ww_PzUpdate(void)
{
    int16_t i, j;
    bool    seen;

    if(pz_cool > 0) pz_cool--;
    if(ww_EdgeU() && pz_lane > 0) pz_lane--;
    if(ww_EdgeD() && pz_lane < 2) pz_lane++;
    if(Ui_Select() && !pz_plant[pz_lane] && pz_cool == 0)
    {
        pz_plant[pz_lane] = true;  pz_php[pz_lane] = 70;  pz_fire[pz_lane] = 12;
        pz_cool = 22;                                           // seed packet recharge
        Buzzer_PlaySfx(&sfx_coin);
    }

    if(--pz_cd <= 0)
    {
        for(i = 0; i < PZ_N; i++)
            if(!pz_zon[i])
            {
                pz_zon[i] = true; pz_zl[i] = ww_Rnd(3); pz_zx[i] = pz_zpx[i] = 292;
                pz_zhp[i] = (ww_Rnd(3) == 0) ? 7 : 4;                // conehead / regular
                break;
            }
        pz_cd = pz_spw + ww_Rnd(18);
    }

    for(i = 0; i < PZ_N; i++)                                   // zombies shamble every 3rd frame, or chew
    {
        if(!pz_zon[i]) continue;
        if(pz_zfl[i] > 0) pz_zfl[i]--;
        if(pz_plant[pz_zl[i]] && pz_zx[i] <= PZ_HX + 38)        // chewing the plant in this lane
        {
            if(--pz_php[pz_zl[i]] <= 0) { pz_plant[pz_zl[i]] = false; pz_pdr[pz_zl[i]] = false; pz_erase[pz_zl[i]] = true;
                                          Buzzer_PlaySfx(&sfx_thud); }
            continue;
        }
        if((ww_frame % 3) == 0) pz_zx[i] -= pz_spd;
        if(pz_zx[i] <= PZ_HX) return MG_LOSE;
    }

    for(i = 0; i < 3; i++)                                      // plants shoot when a zombie is in the lane
    {
        if(!pz_plant[i]) continue;
        if(--pz_fire[i] > 0) continue;
        pz_fire[i] = 14;
        seen = false;
        for(j = 0; j < PZ_N; j++) if(pz_zon[j] && pz_zl[j] == i) seen = true;
        if(seen)
        {
            for(j = 0; j < 8; j++)
                if(!pz_pon[j]) { pz_pon[j] = true; pz_pl[j] = i; pz_pxp[j] = pz_ppx[j] = PZ_HX + 34; break; }
            Buzzer_PlaySfx(&sfx_laser);
        }
    }
    for(j = 0; j < 8; j++)                                      // peas
    {
        if(!pz_pon[j]) continue;
        pz_pxp[j] += 7;
        if(pz_pxp[j] > 320) { pz_pon[j] = false; continue; }
        for(i = 0; i < PZ_N; i++)
            if(pz_zon[i] && pz_zl[i] == pz_pl[j] && pz_pxp[j] + 6 >= pz_zx[i] && pz_pxp[j] <= pz_zx[i] + 24)
            {
                pz_pon[j] = false;
                pz_zfl[i] = 3;
                if(--pz_zhp[i] <= 0)
                {
                    pz_zon[i] = false;
                    Buzzer_PlaySfx(&sfx_explosion);
                    if(++pz_kills >= 5) return MG_WIN;
                }
                break;
            }
    }
    return MG_PLAY;
}

void ww_PzRender(void)
{
    int16_t i, ly;

    if(ww_first)
    {
        for(i = 0; i < 3; i++) ww_Box(PZ_HX, i * PZ_LH, 320 - PZ_HX, PZ_LH, PzBg(i));
        ww_Box(0, 0, PZ_HX, 3 * PZ_LH, 0x8D6E63UL);
        for(i = 0; i < 3; i++) ww_Box(4, i * PZ_LH + 14, 18, 28, 0x5D4037UL);       // doors
        pz_dlane = -1;
        for(i = 0; i < 3; i++) pz_pdr[i] = false;
    }

    if(ww_first || pz_lane != pz_dlane)                          // lane marker on the house wall
    {
        if(pz_dlane >= 0) ww_Box(14, pz_dlane * PZ_LH + 4, 10, 8, 0x8D6E63UL);
        ww_Box(14, pz_lane * PZ_LH + 4, 10, 8, pz_cool > 0 ? COLOR_GRAY : COLOR_YELLOW);
        pz_dlane = pz_lane;
    }

    for(i = 0; i < 3; i++)
    {
        if(pz_erase[i]) { ww_Box(PZ_HX, i * PZ_LH + 6, 40, 44, PzBg(i)); pz_erase[i] = false; }
        if(pz_plant[i] && !pz_pdr[i])
        {
            ww_DrawArt(PZ_HX + 2, i * PZ_LH + 12, ww_kArtPeashooter, 16, 16, 2, PzBg(i), true);
            pz_pdr[i] = true;
        }
    }

    for(i = 0; i < PZ_N; i++)
    {
        ly = pz_zl[i] * PZ_LH + 6;
        if(pz_zon[i])
        {
            if(pz_zx[i] != pz_zpx[i] || !pz_zdr[i])
            {
                if(pz_zdr[i]) ww_MoveArt(pz_zpx[i], ly, pz_zx[i], ly, ww_kArtZombie, 12, 22, 2, PzBg(pz_zl[i]));
                else          ww_DrawArt(pz_zx[i], ly, ww_kArtZombie, 12, 22, 2, PzBg(pz_zl[i]), true);
                pz_zpx[i] = pz_zx[i];  pz_zdr[i] = true;
            }
        }
        else if(pz_zdr[i]) { ww_Box(pz_zpx[i], ly, 24, 44, PzBg(pz_zl[i])); pz_zdr[i] = false; }
    }

    for(i = 0; i < 8; i++)
    {
        ly = pz_pl[i] * PZ_LH + 17;
        if(pz_pon[i])
        {
            if(pz_pdr2[i]) ww_MoveArt(pz_ppx[i], ly, pz_pxp[i], ly, ww_kArtPea, 6, 6, 1, PzBg(pz_pl[i]));
            else           ww_DrawArt(pz_pxp[i], ly, ww_kArtPea, 6, 6, 1, PzBg(pz_pl[i]), true);
            pz_ppx[i] = pz_pxp[i];  pz_pdr2[i] = true;
        }
        else if(pz_pdr2[i]) { ww_Box(pz_ppx[i], ly, 6, 6, PzBg(pz_pl[i])); pz_pdr2[i] = false; }
    }
}
