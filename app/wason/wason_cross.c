//#############################################################################
// FILE:   app/wason/wason_cross.c
// TITLE:  Wason Ware microgame CROSS!  (Frogger)
//
//   Public: ww_FrInit, ww_FrUpdate, ww_FrRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// CROSS!  (Frogger)  hop to the far bank. Stick/D-pad edges = one hop.
// ============================================================================
#define FR_RH   32
#define FR_GRASS 0x1E6B1EUL

static int16_t fr_x, fr_row, fr_dxx, fr_drow;
static int16_t fr_cx[10], fr_cpx[10], fr_cv[10];

static uint32_t FrRowBg(int16_t row) { return (row == 0 || row == 6) ? FR_GRASS : FR_BG; }

static const uint32_t kFrCar[5] = { COLOR_RED, COLOR_BLUE, COLOR_YELLOW, COLOR_WHITE, COLOR_ORANGE };

void ww_FrInit(uint16_t spd)
{
    int16_t i, lane;

    fr_x = fr_dxx = 149;
    fr_row = fr_drow = 6;

    for(i = 0; i < 10; i++)
    {
        lane = 1 + i / 2;
        fr_cx[i]  = fr_cpx[i] = (((lane * 61) + (i & 1) * 180) % 360) - 44;
        fr_cv[i]  = ((lane & 1) ? 1 : -1) * (2 + (lane % 3) + ww_Min16((int16_t)spd / 2, 2));
    }
}

int16_t ww_FrUpdate(void)
{
    int16_t i, lane, fy;

    if(ww_EdgeU() && fr_row > 0)  { fr_row--; Buzzer_PlaySfx(&sfx_blip); }
    if(ww_EdgeD() && fr_row < 6)  { fr_row++; Buzzer_PlaySfx(&sfx_blip); }
    if(ww_EdgeL())                  fr_x = ww_Max16(fr_x - 32, 4);
    if(ww_EdgeR())                  fr_x = ww_Min16(fr_x + 32, 294);

    for(i = 0; i < 10; i++)
    {
        fr_cx[i] += fr_cv[i];
        if(fr_cv[i] > 0 && fr_cx[i] >= 316) fr_cx[i] -= 360;
        if(fr_cv[i] < 0 && fr_cx[i] < -44)  fr_cx[i] += 360;
    }

    if(fr_row == 0) return MG_WIN;

    fy = fr_row * FR_RH + 5;
    for(i = 0; i < 10; i++)
    {
        lane = 1 + i / 2;
        if(lane == fr_row && ww_Hit(fr_x, fy, 22, 22, fr_cx[i], lane * FR_RH + 5, 44, 22))
            return MG_LOSE;
    }
    return MG_PLAY;
}

void ww_FrRender(void)
{
    int16_t i, lane;

    if(ww_first)
    {
        ww_Box(0, 0, FIELD_W, FR_RH, FR_GRASS);
        ww_Box(0, FR_RH, FIELD_W, FR_RH * 5, FR_BG);
        ww_Box(0, FR_RH * 6, FIELD_W, FR_RH, FR_GRASS);
    }

    for(i = 0; i < 10; i++)
    {
        lane = 1 + i / 2;
        ww_SlideX(lane * FR_RH + 5, 22, fr_cpx[i], fr_cx[i], 44, kFrCar[lane - 1], FR_BG);
        fr_cpx[i] = fr_cx[i];
    }

    if(ww_first || fr_x != fr_dxx || fr_row != fr_drow)
    {
        ww_Box(fr_dxx, fr_drow * FR_RH + 5, 22, 22, FrRowBg(fr_drow));
        ww_Box(fr_x, fr_row * FR_RH + 5, 22, 22, COLOR_LIME);
        ww_Box(fr_x + 3,  fr_row * FR_RH + 7, 5, 5, COLOR_WHITE);
        ww_Box(fr_x + 14, fr_row * FR_RH + 7, 5, 5, COLOR_WHITE);
        ww_Box(fr_x + 5,  fr_row * FR_RH + 9, 2, 2, COLOR_BLACK);
        ww_Box(fr_x + 15, fr_row * FR_RH + 9, 2, 2, COLOR_BLACK);
        fr_dxx = fr_x; fr_drow = fr_row;
    }
}
