//#############################################################################
// FILE:   app/wason/wason_sort.c
// TITLE:  Wason Ware microgame SORT!  (Recycler)
//
//   Public: ww_RcInit, ww_RcUpdate, ww_RcRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// SORT!  (Machine Party recycler)  YELLOW = PAPER, RED = CAN, GREEN = GLASS.
//        Sort 5 items. Wrong bin and the machine jams.
// ============================================================================

static int16_t rc_t, rc_n, rc_shown;

void ww_RcInit(uint16_t spd)
{
    rc_t = ww_Rnd(3);
    rc_n = 0;
    rc_shown = -1;
}

int16_t ww_RcUpdate(void)
{
    int16_t p = -1;

    if(Ui_Pressed(UI_ICON_L))      p = 0;
    else if(Ui_Pressed(UI_ICON_U)) p = 1;
    else if(Ui_Pressed(UI_ICON_R)) p = 2;
    if(p < 0) return MG_PLAY;

    if(p != rc_t) { Buzzer_PlaySfx(&sfx_hit); return MG_LOSE; }
    Buzzer_PlaySfx(&sfx_coin);
    if(++rc_n >= 5) return MG_WIN;
    rc_t = ww_Rnd(3);
    return MG_PLAY;
}

void ww_RcRender(void)
{
    int16_t i;

    if(ww_first)
    {
        ww_Box(20, 160, 80, 56, COLOR_BLUE);
        ww_Box(120, 160, 80, 56, COLOR_SILVER);
        ww_Box(220, 160, 80, 56, COLOR_GREEN);
        ww_Text("< PAPER", 60, 188, COLOR_WHITE);
        ww_Text("^ CAN", 160, 188, COLOR_BLACK);
        ww_Text("GLASS >", 260, 188, COLOR_WHITE);
        ww_Box(60, 134, 200, 8, COLOR_GRAY);                       // conveyor belt
        rc_shown = -1;
    }
    if(rc_shown != rc_n)
    {
        for(i = 0; i < 5; i++)
            ww_Box(100 + i * 24, 12, 16, 16, i < rc_n ? COLOR_LIME : COLOR_DARK_GRAY);
        rc_shown = rc_n;
        ww_Box(130, 56, 60, 74, RC_BG);
        if(rc_t == 0)
        {
            ww_Box(148, 76, 24, 32, COLOR_WHITE);
            ww_Box(152, 84, 16, 2, COLOR_GRAY);  ww_Box(152, 90, 16, 2, COLOR_GRAY);  ww_Box(152, 96, 16, 2, COLOR_GRAY);
        }
        else if(rc_t == 1)
        {
            ww_Box(150, 76, 20, 34, COLOR_SILVER);
            ww_Box(150, 76, 20, 4, COLOR_GRAY);
            ww_Box(150, 88, 20, 8, COLOR_RED);
        }
        else
        {
            ww_Box(150, 90, 20, 28, COLOR_GREEN);
            ww_Box(156, 76, 8, 16, COLOR_GREEN);
            ww_Box(155, 72, 10, 4, COLOR_GOLD);
        }
    }
}
