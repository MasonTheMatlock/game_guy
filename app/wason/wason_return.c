//#############################################################################
// FILE:   app/wason/wason_return.c
// TITLE:  Wason Ware microgame RETURN!  (Pong)
//
//   Public: ww_PgInit, ww_PgUpdate, ww_PgRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// RETURN!  (Pong)  stick/D-pad up-down moves the paddle; return the ball 3 times.
// ============================================================================
#define PG_PH   52

static int16_t pg_py, pg_ppy, pg_x, pg_y, pg_vx, pg_vy, pg_bx, pg_by, pg_hits, pg_shown;

void ww_PgInit(uint16_t spd)
{
    pg_py = pg_ppy = 90;
    pg_x = 150; pg_y = 100;
    pg_vx = -(7 + ww_Min16((int16_t)spd, 3));            // serve toward the paddle
    pg_vy = (ww_Rnd(2) ? 1 : -1) * (2 + ww_Rnd(3));
    pg_bx = pg_x; pg_by = pg_y;
    pg_hits = 0;
    pg_shown = -1;
}

int16_t ww_PgUpdate(void)
{
    pg_py = ww_Clamp16(pg_py + ww_PaddleSpeed(6), 0, FIELD_H - PG_PH);

    pg_x += pg_vx;
    pg_y += pg_vy;
    if(pg_y < 0)             { pg_y = 0;             pg_vy = -pg_vy; }
    if(pg_y > FIELD_H - 8)   { pg_y = FIELD_H - 8;   pg_vy = -pg_vy; }
    if(pg_x > 304)           { pg_x = 304;           pg_vx = -ww_Abs16(pg_vx); }

    if(pg_vx < 0 && pg_x <= 16 && pg_x + 8 > 8 &&
       pg_y + 8 > pg_py && pg_y < pg_py + PG_PH)
    {
        pg_x  = 17;
        pg_vx = ww_Abs16(pg_vx);
        pg_vy = ww_Clamp16(pg_vy + ((pg_y + 4) - (pg_py + PG_PH / 2)) / 6, -7, 7);
        if(pg_vy == 0) pg_vy = 1;
        pg_hits++;
        Buzzer_PlaySfx(&sfx_blip);
        if(pg_hits >= 3) return MG_WIN;
    }
    else if(pg_x < -8) return MG_LOSE;
    return MG_PLAY;
}

void ww_PgRender(void)
{
    int16_t i;

    if(ww_first) ww_Box(312, 0, 8, FIELD_H, COLOR_GRAY);

    if(ww_first || pg_py != pg_ppy)
    {
        ww_Box(8, pg_ppy, 8, PG_PH, PG_BG);
        ww_Box(8, pg_py,  8, PG_PH, COLOR_WHITE);
        pg_ppy = pg_py;
    }

    ww_Box(pg_bx, pg_by, 8, 8, PG_BG);
    ww_Box(pg_x, pg_y, 8, 8, COLOR_YELLOW);
    pg_bx = pg_x; pg_by = pg_y;

    if(pg_shown != pg_hits)
    {
        for(i = 0; i < 3; i++)
            ww_Box(130 + i * 24, 8, 16, 16, i < pg_hits ? COLOR_LIME : COLOR_DARK_GRAY);
        pg_shown = pg_hits;
    }
}
