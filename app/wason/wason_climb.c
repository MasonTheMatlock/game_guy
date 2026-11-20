//#############################################################################
// FILE:   app/wason/wason_climb.c
// TITLE:  Wason Ware microgame CLIMB!  (Donkey Kong)
//
//   Public: ww_DkInit, ww_DkUpdate, ww_DkRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// CLIMB!  (Donkey Kong)  jump the rolling barrels, then climb the ladder.
//         STICK runs and climbs the ladder, BLUE (or a stick click) jumps.
// ============================================================================
#define DK_GY   204
#define DK_PY   188                         // player top when standing
#define DK_TOP  108                         // player top on the upper girder
#define DK_LX   240                         // ladder left edge

static int16_t dk_x, dk_y, dk_vy, dk_px, dk_py, dk_v, dk_cd;
static int16_t dk_bx[3], dk_bpx[3];
static bool    dk_on[3], dk_climb;

static void DkLadder(void)
{
    int16_t k;

    ww_Box(DK_LX, 124, 3, 80, COLOR_CYAN);
    ww_Box(DK_LX + 9, 124, 3, 80, COLOR_CYAN);
    for(k = 0; k < 8; k++) ww_Box(DK_LX, 130 + k * 10, 12, 2, COLOR_CYAN);
}

void ww_DkInit(uint16_t spd)
{
    uint16_t i;

    dk_x = dk_px = 20;
    dk_y = dk_py = DK_PY;
    dk_vy = 0;
    dk_climb = false;
    dk_v  = 3 + ww_Min16((int16_t)spd / 3, 2);
    dk_cd = 20;
    for(i = 0; i < 3; i++) dk_on[i] = false;
}

int16_t ww_DkUpdate(void)
{
    uint16_t i;
    bool     onLadder;

    if(dk_climb)
    {
        if(ww_HeldX() != 0) dk_climb = false;
        else if(ww_HeldY() < 0)
        {
            dk_y -= 2;
            if(dk_y <= DK_TOP) { dk_y = DK_TOP; return MG_WIN; }
        }
        else if(ww_HeldY() > 0)
        {
            dk_y += 2;
            if(dk_y >= DK_PY) { dk_y = DK_PY; dk_climb = false; }
        }
    }
    if(!dk_climb)
    {
        dk_x = ww_Clamp16(dk_x + ww_HeldX() * 3, 0, 304);
        onLadder = (ww_Abs16(dk_x - DK_LX) <= 8);
        if(onLadder && ww_HeldY() < 0)
        {
            dk_climb = true; dk_x = DK_LX; dk_vy = 0;
        }
        else
        {
            if(dk_y >= DK_PY && dk_vy == 0 && Ui_Jump())
            {
                dk_vy = -10;
                Buzzer_PlaySfx(&sfx_jump);
            }
            dk_vy++;
            dk_y += dk_vy;
            if(dk_y >= DK_PY) { dk_y = DK_PY; dk_vy = 0; }
        }
    }

    if(--dk_cd <= 0)
    {
        for(i = 0; i < 3; i++)
            if(!dk_on[i]) { dk_on[i] = true; dk_bx[i] = dk_bpx[i] = FIELD_W; break; }
        dk_cd = ww_Max16(24, 40 + ww_Rnd(30) - 4 * dk_v);
    }
    for(i = 0; i < 3; i++)
    {
        if(!dk_on[i]) continue;
        dk_bx[i] -= dk_v + 1;
        if(dk_bx[i] < -14) { dk_on[i] = false; continue; }
        if(ww_Hit(dk_x + 1, dk_y + 1, 10, 14, dk_bx[i], DK_GY - 12, 12, 12)) return MG_LOSE;
    }
    return MG_PLAY;
}

void ww_DkRender(void)
{
    uint16_t i;
    bool     nearLadder = (dk_x + 12 > DK_LX - 4 && dk_x < DK_LX + 16) ||
                          (dk_px + 12 > DK_LX - 4 && dk_px < DK_LX + 16);

    if(ww_first)
    {
        ww_Box(0, DK_GY, FIELD_W, 8, COLOR_RED);
        ww_Box(190, 124, 130, 8, COLOR_RED);
        ww_Box(272, 82, 40, 42, COLOR_BROWN);                      // Donkey Kong
        ww_Box(278, 90, 10, 8, COLOR_WHITE);  ww_Box(296, 90, 10, 8, COLOR_WHITE);
        ww_Box(282, 92, 3, 4, COLOR_BLACK);   ww_Box(299, 92, 3, 4, COLOR_BLACK);
        ww_Box(280, 106, 24, 10, 0xC8A070UL);
        DkLadder();
    }

    for(i = 0; i < 3; i++)
    {
        if(dk_on[i])
        {
            ww_SlideX(DK_GY - 12, 12, dk_bpx[i], dk_bx[i], 12, COLOR_ORANGE, DK_BG);
            dk_bpx[i] = dk_bx[i];
        }
    }

    ww_Box(dk_px, dk_py, 12, 16, DK_BG);
    if(nearLadder) DkLadder();
    ww_Box(dk_x, dk_y, 12, 6, COLOR_RED);                          // cap
    ww_Box(dk_x + 1, dk_y + 6, 10, 4, 0xFFCC99UL);                 // face
    ww_Box(dk_x + 1, dk_y + 10, 10, 6, COLOR_BLUE);                // overalls
    dk_px = dk_x; dk_py = dk_y;
}
