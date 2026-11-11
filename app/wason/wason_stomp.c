//#############################################################################
// FILE:   app/wason/wason_stomp.c
// TITLE:  Wason Ware microgame STOMP!  (Super Mario)
//
//   Public: ww_MrInit, ww_MrUpdate, ww_MrRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// STOMP!  (Super Mario)  L/R run, button or UP jumps. Stomp 2 Goombas by
//         landing on their heads.
// ============================================================================
#define MR_GY   204

static int16_t mr_x, mr_y, mr_vy, mr_px, mr_py, mr_stomp, mr_sp;
static int16_t mr_gx[2], mr_gd[2], mr_pgx[2];
static bool    mr_ga[2], mr_dr[2];

void ww_MrInit(uint16_t spd)
{
    mr_x = mr_px = 24;
    mr_y = mr_py = MR_GY - 18;
    mr_vy = 0;
    mr_stomp = 0;
    mr_sp = 1 + ww_Min16((int16_t)spd / 4, 1);
    mr_gx[0] = mr_pgx[0] = 250;  mr_gd[0] = -1;  mr_ga[0] = true;
    mr_gx[1] = mr_pgx[1] = 140;  mr_gd[1] = -1;  mr_ga[1] = true;
    mr_dr[0] = mr_dr[1] = false;
}

int16_t ww_MrUpdate(void)
{
    uint16_t i;

    mr_x = ww_Clamp16(mr_x + ww_HeldX() * 3, 0, 304);
    if(mr_y >= MR_GY - 18 && (g_joy.btnPressed || g_dpad.uPressed))
    {
        mr_vy = -11;
        Buzzer_PlaySfx(&sfx_jump);
    }
    mr_vy++;
    mr_y += mr_vy;
    if(mr_y >= MR_GY - 18) { mr_y = MR_GY - 18; mr_vy = 0; }

    for(i = 0; i < 2; i++)
    {
        if(!mr_ga[i]) continue;
        mr_gx[i] += mr_gd[i] * mr_sp;
        if(mr_gx[i] < 10)  mr_gd[i] =  1;
        if(mr_gx[i] > 290) mr_gd[i] = -1;

        if(ww_Hit(mr_x + 1, mr_y, 12, 18, mr_gx[i], MR_GY - 14, 16, 14))
        {
            if(mr_vy > 0 && (mr_y + 18 - mr_vy) <= (MR_GY - 14 + 3))
            {
                mr_ga[i] = false;
                mr_vy = -8;
                Buzzer_PlaySfx(&sfx_coin);
                if(++mr_stomp >= 2) return MG_WIN;
            }
            else return MG_LOSE;
        }
    }
    return MG_PLAY;
}

void ww_MrRender(void)
{
    uint16_t i;

    if(ww_first)
    {
        ww_Box(0, MR_GY, FIELD_W, FIELD_H - MR_GY, 0xC84C0CUL);
        ww_Box(0, MR_GY, FIELD_W, 3, 0xFFB070UL);
        ww_Box(40, 40, 50, 14, COLOR_WHITE);   ww_Box(50, 30, 30, 10, COLOR_WHITE);
        ww_Box(230, 60, 50, 14, COLOR_WHITE);  ww_Box(240, 50, 30, 10, COLOR_WHITE);
        ww_Box(150, 80, 20, 20, COLOR_GOLD);   ww_Box(155, 85, 10, 10, 0xB8860BUL);
    }

    ww_Box(mr_px, mr_py, 14, 18, MR_BG);
    for(i = 0; i < 2; i++)
    {
        if(mr_dr[i]) ww_Box(mr_pgx[i], MR_GY - 14, 16, 14, MR_BG);
        if(mr_ga[i])
        {
            ww_Box(mr_gx[i] + 1, MR_GY - 14, 14, 8, COLOR_BROWN);
            ww_Box(mr_gx[i] + 3, MR_GY - 12, 3, 3, COLOR_WHITE);
            ww_Box(mr_gx[i] + 10, MR_GY - 12, 3, 3, COLOR_WHITE);
            ww_Box(mr_gx[i] + 2, MR_GY - 6, 12, 6, 0x222222UL);
            mr_pgx[i] = mr_gx[i];
            mr_dr[i]  = true;
        }
        else mr_dr[i] = false;
    }
    ww_Box(mr_x + 1, mr_y, 12, 5, COLOR_RED);                      // cap
    ww_Box(mr_x + 2, mr_y + 5, 10, 5, 0xFFCC99UL);                 // face
    ww_Box(mr_x + 1, mr_y + 10, 12, 5, COLOR_BLUE);                // overalls
    ww_Box(mr_x, mr_y + 15, 14, 3, COLOR_BROWN);                   // boots
    mr_px = mr_x; mr_py = mr_y;
}
