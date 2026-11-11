//#############################################################################
// FILE:   app/wason/wason_callit.c
// TITLE:  Wason Ware microgame CALL IT!  (Helldivers 2)
//
//   Public: ww_HdInit, ww_HdUpdate, ww_HdRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// CALL IT!  (Helldivers 2)  enter the stratagem code on the D-pad. A wrong
//           direction resets the code.
// ============================================================================

static int16_t hd_seq[7], hd_n, hd_k, hd_shown;

void ww_HdInit(uint16_t spd)
{
    int16_t i;

    hd_n = 4 + ww_Min16((int16_t)spd / 2, 2);
    for(i = 0; i < hd_n; i++) hd_seq[i] = ww_Rnd(4);
    hd_k = 0;
    hd_shown = -1;
}

int16_t ww_HdUpdate(void)
{
    int16_t p = -1;

    if(ww_EdgeU())      p = 0;
    else if(ww_EdgeD()) p = 1;
    else if(ww_EdgeL()) p = 2;
    else if(ww_EdgeR()) p = 3;
    if(p < 0) return MG_PLAY;

    if(p == hd_seq[hd_k])
    {
        Buzzer_PlaySfx(&sfx_blip);
        if(++hd_k >= hd_n) return MG_WIN;
    }
    else
    {
        hd_k = 0;
        Buzzer_PlaySfx(&sfx_hit);
    }
    return MG_PLAY;
}

static void HdArrow(int16_t i)
{
    int16_t x = 160 - (hd_n - 1) * 22 + i * 44;

    ww_Box(x - 14, 98, 28, 28, HD_BG);
    ww_Arrow(x, 110, hd_seq[i], 4, i < hd_k ? COLOR_GOLD : COLOR_WHITE);
}

void ww_HdRender(void)
{
    int16_t i;

    if(ww_first)
    {
        ww_Text("STRATAGEM", 160, 40, COLOR_GOLD);
        ww_Text("ENTER THE CODE", 160, 180, COLOR_GRAY);
        hd_shown = -1;
    }
    if(hd_shown != hd_k)
    {
        if(hd_shown < 0 || hd_shown > hd_k) { for(i = 0; i < hd_n; i++) HdArrow(i); }   // start / reset: all
        else                                { for(i = hd_shown; i < hd_k; i++) HdArrow(i); }  // else just the new ones
        hd_shown = hd_k;
    }
}
