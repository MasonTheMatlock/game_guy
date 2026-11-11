//#############################################################################
// FILE:   app/wason/wason_shoot.c
// TITLE:  Wason Ware microgame SHOOT!  (Galaga)
//
//   Public: ww_GlInit, ww_GlUpdate, ww_GlRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// SHOOT!  (Galaga)  move and fire. Bugs sway in formation and dive at you.
//         Shoot 6.
// ============================================================================
#define GL_SY   208

static int16_t gl_ex[10], gl_ey[10], gl_px[10], gl_py[10], gl_dv[10];
static int16_t gl_sx, gl_psx, gl_cd, gl_kills, gl_spd;
static int16_t gl_bx[2], gl_by[2], gl_pbx[2], gl_pby[2];
static bool    gl_al[10], gl_dr[10], gl_bon[2], gl_bdr[2];

static int16_t Tri(int16_t t, int16_t period, int16_t amp)      // triangle wave -amp..amp
{
    int16_t p = t % period, half = period / 2;
    int16_t v = (p < half) ? p : period - p;
    return (int16_t)(((int32_t)v * 2 * amp) / half) - amp;
}

void ww_GlInit(uint16_t spd)
{
    int16_t i;

    for(i = 0; i < 10; i++) { gl_al[i] = true; gl_dv[i] = 0; gl_dr[i] = false; }
    gl_bon[0] = gl_bon[1] = false;
    gl_bdr[0] = gl_bdr[1] = false;
    gl_sx = gl_psx = 147;
    gl_cd = 40;
    gl_kills = 0;
    gl_spd = 4 + ww_Min16((int16_t)spd / 2, 2);
}

int16_t ww_GlUpdate(void)
{
    int16_t i, j, sway = (Tri(ww_frame, 90, 22) >> 1) << 1;

    gl_sx = ww_Clamp16(gl_sx + ww_HeldX() * 5, 0, FIELD_W - 26);

    if(g_joy.btnPressed || g_dpad.uPressed)
        for(i = 0; i < 2; i++)
            if(!gl_bon[i])
            {
                gl_bon[i] = true; gl_bx[i] = gl_sx + 12; gl_by[i] = GL_SY - 6;
                Buzzer_PlaySfx(&sfx_laser);
                break;
            }

    if(--gl_cd <= 0)
    {
        for(j = 0; j < 6; j++)
        {
            i = ww_Rnd(10);
            if(gl_al[i] && !gl_dv[i]) { gl_dv[i] = 1; break; }
        }
        gl_cd = ww_Max16(28, 60 - 6 * (gl_spd - 4)) + ww_Rnd(20);
    }

    for(i = 0; i < 10; i++)
    {
        if(!gl_al[i]) continue;
        if(!gl_dv[i])
        {
            gl_ex[i] = 62 + (i % 5) * 44 + sway;
            gl_ey[i] = 36 + (i / 5) * 26;
        }
        else
        {
            if(gl_dv[i] == 1) { gl_dv[i] = 2; }
            gl_ey[i] += gl_spd;
            gl_ex[i] += ww_Sgn16(gl_sx + 13 - gl_ex[i]) * 2;
            if(gl_ey[i] > 236) gl_dv[i] = 0;                     // back to formation
            if(gl_dv[i] && ww_Hit(gl_ex[i], gl_ey[i], 18, 12, gl_sx, GL_SY, 26, 12)) return MG_LOSE;
        }
    }

    for(j = 0; j < 2; j++)
    {
        if(!gl_bon[j]) continue;
        gl_by[j] -= 9;
        if(gl_by[j] < -8) { gl_bon[j] = false; continue; }
        for(i = 0; i < 10; i++)
            if(gl_al[i] && ww_Hit(gl_bx[j], gl_by[j], 3, 8, gl_ex[i] - 3, gl_ey[i], 24, 12))
            {
                gl_al[i] = false;
                gl_bon[j] = false;
                Buzzer_PlaySfx(&sfx_explosion);
                if(++gl_kills >= 6) return MG_WIN;
                break;
            }
    }
    return MG_PLAY;
}

void ww_GlRender(void)
{
    int16_t i;

    for(i = 0; i < 10; i++)
    {
        if(!gl_al[i])
        {
            if(gl_dr[i]) { ww_Box(gl_px[i] - 4, gl_py[i], 26, 12, GL_BG); gl_dr[i] = false; }
            continue;
        }
        if(gl_dr[i] && gl_px[i] == gl_ex[i] && gl_py[i] == gl_ey[i]) continue;        // hasn't moved
        if(gl_dr[i]) ww_Box(gl_px[i] - 4, gl_py[i], 26, 12, GL_BG);                       // erase+draw back to back
        ww_Box(gl_ex[i], gl_ey[i] + 2, 18, 8, (i < 5) ? COLOR_YELLOW : COLOR_CYAN);
        ww_Box(gl_ex[i] - 4, gl_ey[i], 5, 10, COLOR_RED);
        ww_Box(gl_ex[i] + 17, gl_ey[i], 5, 10, COLOR_RED);
        ww_Box(gl_ex[i] + 4, gl_ey[i] + 4, 3, 3, COLOR_BLACK);
        ww_Box(gl_ex[i] + 11, gl_ey[i] + 4, 3, 3, COLOR_BLACK);
        gl_px[i] = gl_ex[i]; gl_py[i] = gl_ey[i]; gl_dr[i] = true;
    }
    for(i = 0; i < 2; i++)
    {
        if(gl_bdr[i]) { ww_Box(gl_pbx[i], gl_pby[i], 3, 8, GL_BG); gl_bdr[i] = false; }
        if(gl_bon[i])
        {
            ww_Box(gl_bx[i], gl_by[i], 3, 8, COLOR_WHITE);
            gl_pbx[i] = gl_bx[i]; gl_pby[i] = gl_by[i]; gl_bdr[i] = true;
        }
    }
    if(ww_first || gl_sx != gl_psx)
    {
        ww_Box(gl_psx, GL_SY - 7, 26, 20, GL_BG);
        ww_Box(gl_sx, GL_SY, 26, 12, COLOR_WHITE);
        ww_Box(gl_sx + 11, GL_SY - 6, 4, 6, COLOR_WHITE);
        ww_Box(gl_sx, GL_SY + 4, 4, 8, COLOR_RED);  ww_Box(gl_sx + 22, GL_SY + 4, 4, 8, COLOR_RED);
        gl_psx = gl_sx;
    }
}
