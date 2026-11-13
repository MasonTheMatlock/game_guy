//#############################################################################
// FILE:   app/wason/wason_chomp.c
// TITLE:  Wason Ware microgame CHOMP!  (Pac-Man)
//
//   Public: ww_PmInit, ww_PmUpdate, ww_PmRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// CHOMP!  (Pac-Man)  a real maze. Steer (it keeps going, and turns when it can).
//         Eat 12 dots or a power pellet then catch the ghost.  Don't get caught.
//         210x182 window, 14 px cells; only changed cells are redrawn.
// ============================================================================
#define PM_C    14
#define PM_GW   15
#define PM_GH   13

static const char *const kPmMap[PM_GH] =
{
    "###############",
    "#o.....#.....o#",
    "#.###.#.#.###.#",
    "#.............#",
    "#.###.#.#.###.#",
    "#.....#.#.....#",
    "#####.....#####",
    "#.....#.#.....#",
    "#.###.#.#.###.#",
    "#.............#",
    "#.###.#.#.###.#",
    "#o...........o#",
    "###############"
};

static uint16_t pm_d[PM_GH][PM_GW];                             // 0 none, 1 dot, 2 power pellet
static int16_t  pm_x, pm_y, pm_dx, pm_dy, pm_ndx, pm_ndy, pm_ox, pm_oy, pm_pc, pm_eaten, pm_fr, pm_gm, pm_gc;
static int16_t  pm_g[2][6];                                     // x, y, dx, dy, ox, oy
static int16_t  pm_ng;
static bool     pm_moved;

static bool PmWall(int16_t x, int16_t y)
{
    return x < 0 || x >= PM_GW || y < 0 || y >= PM_GH || kPmMap[y][x] == '#';
}

void ww_PmInit(uint16_t spd)
{
    int16_t x, y;

    for(y = 0; y < PM_GH; y++)
        for(x = 0; x < PM_GW; x++)
            pm_d[y][x] = (kPmMap[y][x] == '.') ? 1 : ((kPmMap[y][x] == 'o') ? 2 : 0);
    pm_x = pm_ox = 1;  pm_y = pm_oy = 9;
    pm_dx = 1; pm_dy = 0; pm_ndx = 1; pm_ndy = 0;
    pm_pc = 5;  pm_eaten = 0;  pm_fr = 0;
    pm_gm = 9 - ww_Min16((int16_t)spd / 3, 2);
    pm_gc = pm_gm;
    pm_ng = (spd >= 3) ? 2 : 1;
    pm_g[0][0] = 13; pm_g[0][1] = 3; pm_g[0][2] = -1; pm_g[0][3] = 0; pm_g[0][4] = 13; pm_g[0][5] = 3;
    pm_g[1][0] = 7;  pm_g[1][1] = 3; pm_g[1][2] = 1;  pm_g[1][3] = 0; pm_g[1][4] = 7;  pm_g[1][5] = 3;
    pm_moved = false;
}

static void PmGhostStep(int16_t gi)
{
    static const int16_t dxs[4] = { 0, -1, 0, 1 };
    static const int16_t dys[4] = { -1, 0, 1, 0 };
    int16_t k, bk = -1, bd = 32000, d, tx, ty, nx, ny, *g = pm_g[gi];

    tx = pm_x;  ty = pm_y;
    if(gi == 1) { tx += pm_dx * 3;  ty += pm_dy * 3; }          // the pink one aims ahead of you
    for(k = 0; k < 4; k++)
    {
        nx = g[0] + dxs[k];  ny = g[1] + dys[k];
        if(PmWall(nx, ny)) continue;
        if(dxs[k] == -g[2] && dys[k] == -g[3] && (g[2] || g[3])) continue;     // no U-turns
        d = (pm_fr > 0) ? ww_Rnd(100) : ((nx - tx) * (nx - tx) + (ny - ty) * (ny - ty));
        if(d < bd) { bd = d; bk = k; }
    }
    if(bk < 0)                                                  // dead end: turn around
    { g[2] = -g[2]; g[3] = -g[3]; bk = -1; }
    else { g[2] = dxs[bk]; g[3] = dys[bk]; }
    g[4] = g[0]; g[5] = g[1];
    g[0] += g[2]; g[1] += g[3];
}

int16_t ww_PmUpdate(void)
{
    int16_t hx = ww_HeldX(), hy = ww_HeldY(), gi;

    if(hx != 0)      { pm_ndx = hx; pm_ndy = 0; }
    else if(hy != 0) { pm_ndx = 0;  pm_ndy = hy; }

    pm_moved = false;
    if(pm_fr > 0) pm_fr--;

    if(--pm_pc <= 0)
    {
        pm_pc = 5;
        if(!PmWall(pm_x + pm_ndx, pm_y + pm_ndy)) { pm_dx = pm_ndx; pm_dy = pm_ndy; }
        if(!PmWall(pm_x + pm_dx, pm_y + pm_dy))
        {
            pm_ox = pm_x; pm_oy = pm_y;
            pm_x += pm_dx; pm_y += pm_dy;
            pm_moved = true;
            if(pm_d[pm_y][pm_x] == 1) { pm_d[pm_y][pm_x] = 0; pm_eaten++; Buzzer_PlaySfx(&sfx_blip); }
            else if(pm_d[pm_y][pm_x] == 2) { pm_d[pm_y][pm_x] = 0; pm_fr = 90; Buzzer_PlaySfx(&sfx_powerup); }
            if(pm_eaten >= 50) return MG_WIN;
        }
    }
    if(--pm_gc <= 0)
    {
        pm_gc = (pm_fr > 0) ? pm_gm + 4 : pm_gm;
        for(gi = 0; gi < pm_ng; gi++) PmGhostStep(gi);
        pm_moved = true;
    }
    for(gi = 0; gi < pm_ng; gi++)
        if(pm_g[gi][0] == pm_x && pm_g[gi][1] == pm_y)
            return (pm_fr > 0) ? MG_WIN : MG_LOSE;
    return MG_PLAY;
}

static void PmCell(int16_t x, int16_t y)
{
    int16_t px = x * PM_C, py = y * PM_C;

    if(kPmMap[y][x] == '#')
    {
        ww_Box(px, py, PM_C, PM_C, COLOR_NAVY);
        ww_Box(px + 2, py + 2, PM_C - 4, PM_C - 4, COLOR_BLUE);
        return;
    }
    ww_Box(px, py, PM_C, PM_C, PM_BG);
    if(pm_d[y][x] == 1)      ww_Box(px + 6, py + 6, 3, 3, COLOR_WHITE);
    else if(pm_d[y][x] == 2) { ww_Box(px + 4, py + 4, 7, 7, COLOR_WHITE); }
}

void ww_PmRender(void)
{
    int16_t x, y, gi, cx, cy;

    if(ww_first)
    {
        for(y = 0; y < PM_GH; y++) for(x = 0; x < PM_GW; x++) PmCell(x, y);
    }
    else if(pm_moved)
    {
        PmCell(pm_ox, pm_oy);
        for(gi = 0; gi < pm_ng; gi++) PmCell(pm_g[gi][4], pm_g[gi][5]);
    }
    else return;

    cx = pm_x * PM_C + 7;  cy = pm_y * PM_C + 7;                // Pac-Man: octagon + mouth
    ww_Box(cx - 4, cy - 6, 8, 12, COLOR_YELLOW);
    ww_Box(cx - 6, cy - 4, 12, 8, COLOR_YELLOW);
    if(ww_frame & 4)
        ww_Box(cx + (pm_dx > 0 ? 0 : (pm_dx < 0 ? -6 : -2)), cy + (pm_dy > 0 ? 0 : (pm_dy < 0 ? -6 : -2)),
            pm_dx ? 6 : 4, pm_dy ? 6 : 4, PM_BG);

    for(gi = 0; gi < pm_ng; gi++)
    {
        cx = pm_g[gi][0] * PM_C + 1;  cy = pm_g[gi][1] * PM_C + 1;
        ww_Box(cx + 1, cy, 10, 3, (pm_fr > 0) ? COLOR_BLUE : (gi ? COLOR_PINK : COLOR_RED));
        ww_Box(cx, cy + 3, 12, 9, (pm_fr > 0) ? COLOR_BLUE : (gi ? COLOR_PINK : COLOR_RED));
        ww_Box(cx + 2, cy + 4, 3, 3, COLOR_WHITE);  ww_Box(cx + 7, cy + 4, 3, 3, COLOR_WHITE);
        ww_Box(cx + 3, cy + 5, 1, 2, COLOR_BLACK);  ww_Box(cx + 8, cy + 5, 1, 2, COLOR_BLACK);
    }
}
