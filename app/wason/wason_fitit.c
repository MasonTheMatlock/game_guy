//#############################################################################
// FILE:   app/wason/wason_fitit.c
// TITLE:  Wason Ware microgame FIT IT!  (Tetris)
//
//   Public: ww_TtInit, ww_TtUpdate, ww_TtRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// FIT IT!  (Tetris)  slide the I-piece over the gap and drop it in.
// ============================================================================
#define TT_X0   80
#define TT_C    16

static int16_t tt_col, tt_row, tt_gap, tt_fd, tt_tick, tt_pcol, tt_prow;

static void TtCell(int16_t c, int16_t r, uint32_t color)
{
    ww_Box(TT_X0 + c * TT_C, r * TT_C, TT_C - 1, TT_C - 1, color);
}

void ww_TtInit(uint16_t spd)
{
    tt_gap  = ww_Rnd(7);
    tt_col  = (tt_gap + 2 + ww_Rnd(4)) % 7;
    tt_row  = 0;
    tt_fd   = ww_Max16(3, 8 - (int16_t)spd / 2);
    tt_tick = 0;
    tt_pcol = tt_col;
    tt_prow = -1;
}

int16_t ww_TtUpdate(void)
{
    if(ww_EdgeL() && tt_col > 0) { tt_col--; Buzzer_PlaySfx(&sfx_blip); }
    if(ww_EdgeR() && tt_col < 6) { tt_col++; Buzzer_PlaySfx(&sfx_blip); }

    if(++tt_tick >= ((ww_HeldY() > 0 || Ui_SelectHeld()) ? 1 : tt_fd))
    {
        tt_tick = 0;
        if(tt_row + 1 == 13)
        {
            if(tt_col == tt_gap)
            {
                tt_row = 13;
                Buzzer_PlaySfx(&sfx_tetris_say);
                return MG_WIN;
            }
            Buzzer_PlaySfx(&sfx_thud);
            return MG_LOSE;                         // landed on the stack, misaligned
        }
        tt_row++;
    }
    return MG_PLAY;
}

void ww_TtRender(void)
{
    int16_t c;

    if(ww_first)
    {
        ww_Box(TT_X0 - 2, 0, 2, 224, COLOR_GRAY);
        ww_Box(TT_X0 + 10 * TT_C, 0, 2, 224, COLOR_GRAY);
        for(c = 0; c < 10; c++)
            TtCell(c, 13, (c >= tt_gap && c < tt_gap + 4) ? 0x1C1C44UL : COLOR_SILVER);
        tt_prow = -1;
    }

    if(ww_first || tt_col != tt_pcol || tt_row != tt_prow)
    {
        if(tt_prow >= 0 && tt_prow < 13) ww_Box(TT_X0 + tt_pcol * TT_C, tt_prow * TT_C, 4 * TT_C, TT_C, TT_BG);
        for(c = 0; c < 4; c++) TtCell(tt_col + c, tt_row, COLOR_CYAN);
        tt_pcol = tt_col; tt_prow = tt_row;
    }
}
