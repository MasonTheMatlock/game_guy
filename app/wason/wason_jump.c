//#############################################################################
// FILE:   app/wason/wason_jump.c
// TITLE:  Wason Ware microgame JUMP!  (Chrome Dino)
//
//   Public: ww_DnInit, ww_DnUpdate, ww_DnRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// JUMP!  (Chrome Dino)  any button jumps over the cacti.  Survive.
// ============================================================================
#define DN_X    40
#define DN_GY   206
#define DN_INK  0x535353UL

static int16_t dn_y, dn_vy, dn_py, dn_v, dn_cd;
static int16_t dn_cx[2], dn_cpx[2], dn_ch[2];
static bool    dn_on[2];

void ww_DnInit(uint16_t spd)
{
    dn_y = dn_py = DN_GY - 26;
    dn_vy = 0;
    dn_v  = 5 + ww_Min16((int16_t)spd / 2, 2);
    dn_cd = 20;
    dn_on[0] = dn_on[1] = false;
}

int16_t ww_DnUpdate(void)
{
    uint16_t i;

    if(dn_y >= DN_GY - 26 && ww_AnyPress()) { dn_vy = -12; Buzzer_PlaySfx(&sfx_jump); }
    dn_vy++;
    dn_y += dn_vy;
    if(dn_y >= DN_GY - 26) { dn_y = DN_GY - 26; dn_vy = 0; }

    if(--dn_cd <= 0)
    {
        for(i = 0; i < 2; i++)
            if(!dn_on[i])
            {
                dn_on[i]  = true;
                dn_cx[i]  = dn_cpx[i] = FIELD_W;
                dn_ch[i]  = 24 + 8 * ww_Rnd(3);
                break;
            }
        dn_cd = (170 + ww_Rnd(100)) / dn_v;
    }

    for(i = 0; i < 2; i++)
    {
        if(!dn_on[i]) continue;
        dn_cx[i] -= dn_v;
        if(dn_cx[i] < -16) { dn_on[i] = false; continue; }
        if(ww_Hit(DN_X + 4, dn_y + 3, 12, 22, dn_cx[i] + 1, DN_GY - dn_ch[i], 12, dn_ch[i]))
            return MG_LOSE;
    }
    return MG_PLAY;
}

void ww_DnRender(void)
{
    uint16_t i;

    if(ww_first) ww_Box(0, DN_GY, FIELD_W, 2, DN_INK);

    for(i = 0; i < 2; i++)
        if(dn_on[i])
        {
            ww_SlideX(DN_GY - dn_ch[i], dn_ch[i], dn_cpx[i], dn_cx[i], 14, DN_INK, DN_BG);
            dn_cpx[i] = dn_cx[i];
        }

    ww_Box(DN_X, dn_py, 22, 26, DN_BG);
    ww_Box(DN_X, dn_y + 6, 14, 14, DN_INK);                // body
    ww_Box(DN_X + 8, dn_y, 12, 10, DN_INK);                // head
    ww_Box(DN_X + 14, dn_y + 2, 3, 3, DN_BG);              // eye
    ww_Box(DN_X + 4, dn_y + 20, 4, (ww_frame & 4) ? 6 : 3, DN_INK);     // legs
    ww_Box(DN_X + 11, dn_y + 20, 4, (ww_frame & 4) ? 3 : 6, DN_INK);
    dn_py = dn_y;
}
