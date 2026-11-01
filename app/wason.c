//#############################################################################
// FILE:   app/wason.c
// TITLE:  Wason Ware - WarioWare-style microgames (see wasonware.h)
//
//   1. Helpers            Box/Slide drawing, input helpers, text
//   2. Microgames         14 of them, each Init / Update / Render
//   3. Game table         kGames[]  <- add new microgames here
//   4. Framework          title -> intro -> play -> result -> ... -> game over
//   5. App descriptor     g_wasonwareApp
//
//   CONTROLS (all games): stick or D-pad. "Any press" = stick button or any
//   D-pad button. Hold the stick button to open Settings (done by apps.c).
//   Title / game over: D or stick button = start, R = back to the menu.
//
//   C28x notes: char/int are 16 bits, so products that can pass 32767 are
//   cast to int32_t, and only int16_t/uint16_t/uint32_t are used.
//#############################################################################

#include "wason.h"

//*****************************************************************************
// Layout
//*****************************************************************************
#define FIELD_W     320
#define FIELD_H     230                     // playfield; the fuse bar is below
#define BAR_Y0      232
#define BAR_Y1      239

#define NUM_GAMES   ((uint16_t)(sizeof(kGames) / sizeof(kGames[0])))

enum { PH_TITLE, PH_INTRO, PH_PLAY, PH_RESULT, PH_OVER };
enum { MG_PLAY = 0, MG_WIN = 1, MG_LOSE = 2 };

typedef struct
{
    const char *cmd;                        // one-word command, shown big
    const char *inspo;                      // "inspired by" tag, shown small
    uint16_t    frames;                     // time limit (frames)
    uint32_t    bg;                         // background color
    uint16_t    winOnTimeout;               // 1 = "survive" game, 0 = "do it" game
    void      (*init)(uint16_t spd);
    int16_t   (*update)(void);
    void      (*render)(void);
} MicroGame;

static bool     s_first;                    // true on the first render of a screen
static uint16_t s_frame;                    // frames since the microgame started

//*****************************************************************************
// 1. Helpers
//*****************************************************************************
static int16_t Min16(int16_t a, int16_t b) { return (a < b) ? a : b; }
static int16_t Max16(int16_t a, int16_t b) { return (a > b) ? a : b; }
static int16_t Abs16(int16_t a)            { return (a < 0) ? -a : a; }
static int16_t Sgn16(int16_t a)            { return (a > 0) - (a < 0); }

static int16_t Clamp16(int16_t v, int16_t lo, int16_t hi)
{
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static int16_t Rnd(int16_t n)               // 0..n-1
{
    return (int16_t)(Rand16() % (uint16_t)n);
}

// Filled rectangle, clipped to the playfield (never touches the fuse bar).
static void Box(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c)
{
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;

    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x1 > FIELD_W - 1) x1 = FIELD_W - 1;
    if(y1 > FIELD_H - 1) y1 = FIELD_H - 1;
    if(x1 < x || y1 < y) return;
    Graphics_FillRect(x, y, x1, y1, c);
}

static bool Hit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return (ax < bx + bw) && (bx < ax + aw) && (ay < by + bh) && (by < ay + ah);
}

// Filled circle, skipped if any part would leave the playfield.
static void Disc(int16_t x, int16_t y, int16_t r, uint32_t c)
{
    if(x - r < 0 || y - r < 0 || x + r >= FIELD_W || y + r >= FIELD_H) return;
    Graphics_setForegroundColor(&g_sContext, c);
    Graphics_fillCircle(&g_sContext, x, y, r);
}

// Move a w x h block horizontally from x0 to x1 (same y) touching only the
// pixels that change. Falls back to erase-all + draw-all for big jumps and
// for the first frame of a screen.
static void SlideX(int16_t y, int16_t h, int16_t x0, int16_t x1, int16_t w,
                   uint32_t c, uint32_t bg)
{
    int16_t d = x1 - x0;

    if(s_first || d >= w || d <= -w)
    {
        Box(x0, y, w, h, bg);
        Box(x1, y, w, h, c);
    }
    else if(d > 0)
    {
        Box(x0, y, d, h, bg);
        Box(x0 + w, y, d, h, c);
    }
    else if(d < 0)
    {
        Box(x1 + w, y, -d, h, bg);
        Box(x1, y, -d, h, c);
    }
}

static void SlideY(int16_t x, int16_t w, int16_t y0, int16_t y1, int16_t h,
                   uint32_t c, uint32_t bg)
{
    int16_t d = y1 - y0;

    if(s_first || d >= h || d <= -h)
    {
        Box(x, y0, w, h, bg);
        Box(x, y1, w, h, c);
    }
    else if(d > 0)
    {
        Box(x, y0, w, d, bg);
        Box(x, y0 + h, w, d, c);
    }
    else if(d < 0)
    {
        Box(x, y1 + h, w, -d, bg);
        Box(x, y1, w, -d, c);
    }
}

// ---- input ------------------------------------------------------------------
static int16_t HeldX(void)
{
    if(g_dpad.l || g_joy.dx < -50) return -1;
    if(g_dpad.r || g_joy.dx >  50) return  1;
    return 0;
}

static int16_t HeldY(void)                  // -1 = up, +1 = down
{
    if(g_dpad.u || g_joy.dy < -50) return -1;
    if(g_dpad.d || g_joy.dy >  50) return  1;
    return 0;
}

static bool EdgeL(void) { return g_dpad.lPressed || g_joy.edgeX < 0; }
static bool EdgeR(void) { return g_dpad.rPressed || g_joy.edgeX > 0; }
static bool EdgeU(void) { return g_dpad.uPressed || g_joy.edgeY < 0; }
static bool EdgeD(void) { return g_dpad.dPressed || g_joy.edgeY > 0; }

static bool AnyPress(void)
{
    return g_joy.btnPressed || g_dpad.uPressed || g_dpad.dPressed ||
           g_dpad.lPressed  || g_dpad.rPressed;
}

// Analog stick Y as a paddle speed, with the D-pad as a fixed-speed override.
static int16_t PaddleSpeed(int16_t maxSpeed)
{
    if(g_dpad.u) return -maxSpeed;
    if(g_dpad.d) return  maxSpeed;
    return (int16_t)(((int32_t)g_joy.dy * maxSpeed) / 100);
}

// ---- text -------------------------------------------------------------------
static void Text(const char *s, int16_t x, int16_t y, uint32_t c)
{
    Graphics_setForegroundColor(&g_sContext, c);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH,
                                x, y, TRANSPARENT_TEXT);
}

static void BoldText(const char *s, int16_t x, int16_t y, uint32_t c)
{
    Text(s, x,     y, c);
    Text(s, x + 1, y, c);
    Text(s, x,     y + 1, c);
}

// "label" + decimal value -> buf
static void Label(char *buf, const char *label, uint32_t v)
{
    char     tmp[11];
    int16_t  n = 0, i = 0;

    while(*label) buf[i++] = *label++;

    do
    {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while(v);

    while(n) buf[i++] = tmp[--n];
    buf[i] = 0;
}

//*****************************************************************************
// 2. MICROGAMES
//*****************************************************************************

// ============================================================================
// DODGE!  (Subway Surfers)  3 lanes, trains come down, switch lanes.  Survive.
// ============================================================================
#define SF_W    50
#define SF_H    46
#define SF_PY   188
#define SF_BG   0x2A2F38UL

static int16_t sf_lane, sf_drawLane, sf_v, sf_cd;
static int16_t sf_y[3], sf_py[3], sf_l[3];
static bool    sf_on[3], sf_dr[3];

static int16_t SfX(int16_t lane) { return 80 + lane * 80; }

static void SfInit(uint16_t spd)
{
    uint16_t i;

    sf_lane = 1;
    sf_drawLane = -1;
    sf_v  = 5 + Min16((int16_t)spd, 4);
    sf_cd = 8;
    for(i = 0; i < 3; i++) { sf_on[i] = false; sf_dr[i] = false; }
}

static int16_t SfUpdate(void)
{
    uint16_t i;

    if(EdgeL() && sf_lane > 0) { sf_lane--; Buzzer_PlaySfx(&sfx_blip); }
    if(EdgeR() && sf_lane < 2) { sf_lane++; Buzzer_PlaySfx(&sfx_blip); }

    if(--sf_cd <= 0)
    {
        for(i = 0; i < 3; i++)
            if(!sf_on[i])
            {
                sf_on[i] = true;
                sf_l[i]  = Rnd(3);
                sf_y[i]  = -SF_H;
                sf_py[i] = sf_y[i];
                break;
            }
        sf_cd = Max16(14, 30 + Rnd(14) - 2 * sf_v);
    }

    for(i = 0; i < 3; i++)
    {
        if(!sf_on[i]) continue;
        sf_y[i] += sf_v;
        if(sf_y[i] >= FIELD_H) { sf_on[i] = false; continue; }

        if(sf_l[i] == sf_lane &&
           Hit(SfX(sf_l[i]) - 25, sf_y[i], SF_W, SF_H, SfX(sf_lane) - 12, SF_PY, 24, 24))
            return MG_LOSE;
    }
    return MG_PLAY;
}

static void SfRender(void)
{
    uint16_t i;

    if(s_first)
    {
        Box(119, 0, 2, FIELD_H, COLOR_GRAY);
        Box(199, 0, 2, FIELD_H, COLOR_GRAY);
    }

    for(i = 0; i < 3; i++)
    {
        if(sf_on[i])
        {
            SlideY(SfX(sf_l[i]) - 25, SF_W, sf_py[i], sf_y[i], SF_H, COLOR_CRIMSON, SF_BG);
            sf_py[i] = sf_y[i];
            sf_dr[i] = true;
        }
        else if(sf_dr[i])
        {
            Box(SfX(sf_l[i]) - 25, sf_py[i], SF_W, SF_H, SF_BG);
            sf_dr[i] = false;
        }
    }

    if(s_first || sf_lane != sf_drawLane)
    {
        if(sf_drawLane >= 0) Box(SfX(sf_drawLane) - 12, SF_PY, 24, 24, SF_BG);
        Box(SfX(sf_lane) - 12, SF_PY, 24, 24, COLOR_YELLOW);
        Box(SfX(sf_lane) - 6,  SF_PY, 12, 8,  COLOR_WHITE);
        sf_drawLane = sf_lane;
    }
}

// ============================================================================
// FLY!  (Jetpack Joyride)  hold UP / button to thrust, dodge zappers.  Survive.
// ============================================================================
#define JT_X    60
#define JT_BG   0x1B1F3BUL

static int16_t jt_y8, jt_vy, jt_py, jt_cd, jt_v;
static bool    jt_thr;
static int16_t jt_zx[3], jt_zpx[3], jt_zy[3];
static bool    jt_on[3];

static void JtInit(uint16_t spd)
{
    uint16_t i;

    jt_y8 = 100 * 8;
    jt_vy = 0;
    jt_py = 100;
    jt_cd = 14;
    jt_v  = 4 + Min16((int16_t)spd / 2, 4);
    jt_thr = false;
    for(i = 0; i < 3; i++) jt_on[i] = false;
}

static int16_t JtUpdate(void)
{
    uint16_t i;
    int16_t  y;

    jt_thr = g_dpad.u || g_joy.dy < -50 || g_joy.btn;
    jt_vy += jt_thr ? -6 : 3;
    jt_vy  = Clamp16(jt_vy, -48, 40);
    jt_y8 += jt_vy;
    if(jt_y8 < 0)             { jt_y8 = 0;             jt_vy = 0; }
    if(jt_y8 > 200 * 8)       { jt_y8 = 200 * 8;       jt_vy = 0; }
    y = jt_y8 / 8;

    if(--jt_cd <= 0)
    {
        for(i = 0; i < 3; i++)
            if(!jt_on[i])
            {
                jt_on[i]  = true;
                jt_zx[i]  = FIELD_W;
                jt_zpx[i] = FIELD_W;
                jt_zy[i]  = 8 + Rnd(150);
                break;
            }
        jt_cd = Max16(22, 36 + Rnd(24) - 3 * (jt_v - 4));
    }

    for(i = 0; i < 3; i++)
    {
        if(!jt_on[i]) continue;
        jt_zx[i] -= jt_v;
        if(jt_zx[i] < -12) { jt_on[i] = false; continue; }
        if(Hit(JT_X + 1, y + 1, 12, 20, jt_zx[i], jt_zy[i], 10, 56)) return MG_LOSE;
    }
    return MG_PLAY;
}

static void JtRender(void)
{
    uint16_t i;
    int16_t  y = jt_y8 / 8;

    if(s_first) Box(0, 226, FIELD_W, 4, COLOR_GRAY);

    for(i = 0; i < 3; i++)
    {
        if(!jt_on[i]) continue;
        SlideX(jt_zy[i], 56, jt_zpx[i], jt_zx[i], 10, COLOR_YELLOW, JT_BG);
        Box(jt_zx[i] + 4, jt_zy[i], 2, 56, (s_frame & 1) ? COLOR_WHITE : COLOR_CYAN);
        jt_zpx[i] = jt_zx[i];
    }

    Box(JT_X - 6, jt_py, 22, 26, JT_BG);                // erase old
    Box(JT_X, y, 14, 22, COLOR_SKY_BLUE);               // body
    Box(JT_X + 3, y + 3, 8, 6, COLOR_WHITE);            // visor
    Box(JT_X - 4, y + 4, 4, 12, COLOR_SILVER);          // jetpack
    if(jt_thr) Box(JT_X - 4, y + 16, 4, 8, COLOR_ORANGE);
    jt_py = y;
}

// ============================================================================
// LAUNCH!  (Angry Birds)  stick up/down = angle, button = fire at the pig.
//          A power meter swings by itself: time your shot.
// ============================================================================
#define BD_GY       200
#define BD_SX       44
#define BD_SY       170
#define BD_BG       0x87CEEBUL

// angle idx 0..12 = 15..75 degrees, x100
static const int16_t kCos[13] = { 97, 94, 91, 87, 82, 77, 71, 64, 57, 50, 42, 34, 26 };
static const int16_t kSin[13] = { 26, 34, 42, 50, 57, 64, 71, 77, 82, 87, 91, 94, 97 };

static int16_t bd_ang, bd_pw, bd_pwd, bd_pig, bd_dx, bd_dy;
static bool    bd_fly, bd_aimDirty, bd_pigAlive, bd_clear;
static int32_t bd_x, bd_y, bd_vx, bd_vy;

static void BdInit(uint16_t spd)
{
    bd_ang = 6;
    bd_pw  = 0;
    bd_pwd = 3 + Min16((int16_t)spd, 3);
    bd_pig = 190 + Rnd(90);
    bd_fly = false;
    bd_aimDirty = true;
    bd_clear = false;
    bd_pigAlive = true;
    bd_dx = BD_SX;
    bd_dy = BD_SY;
}

static int16_t BdUpdate(void)
{
    int32_t v100;

    if(!bd_fly)
    {
        bd_pw += bd_pwd;
        if(bd_pw >= 99) { bd_pw = 99; bd_pwd = -Abs16(bd_pwd); }
        if(bd_pw <= 0)  { bd_pw = 0;  bd_pwd =  Abs16(bd_pwd); }

        if((s_frame % 3) == 0)
        {
            if(HeldY() < 0 && bd_ang < 12) { bd_ang++; bd_aimDirty = true; }
            if(HeldY() > 0 && bd_ang > 0)  { bd_ang--; bd_aimDirty = true; }
        }

        if(AnyPress())
        {
            v100  = 500 + (int32_t)bd_pw * 4;
            bd_vx = (v100 * kCos[bd_ang]) / 100;
            bd_vy = -(v100 * kSin[bd_ang]) / 100;
            bd_x  = (int32_t)BD_SX * 100;
            bd_y  = (int32_t)BD_SY * 100;
            bd_fly = true;
            bd_clear = true;
            Buzzer_PlaySfx(&sfx_jump);
        }
        return MG_PLAY;
    }

    bd_x  += bd_vx;
    bd_y  += bd_vy;
    bd_vy += 24;
    bd_dx  = (int16_t)(bd_x / 100);
    bd_dy  = (int16_t)(bd_y / 100);

    if(Hit(bd_dx - 6, bd_dy - 6, 12, 12, bd_pig, BD_GY - 24, 24, 24))
    {
        bd_pigAlive = false;
        Buzzer_PlaySfx(&sfx_explosion);
        return MG_WIN;
    }
    if(bd_dy >= BD_GY - 6 || bd_dx > FIELD_W || bd_dx < 0) return MG_LOSE;
    return MG_PLAY;
}

static void BdDrawPig(void)
{
    Box(bd_pig, BD_GY - 24, 24, 24, COLOR_LIME);
    Box(bd_pig + 4,  BD_GY - 19, 6, 6, COLOR_WHITE);
    Box(bd_pig + 14, BD_GY - 19, 6, 6, COLOR_WHITE);
    Box(bd_pig + 6,  BD_GY - 17, 2, 2, COLOR_BLACK);
    Box(bd_pig + 16, BD_GY - 17, 2, 2, COLOR_BLACK);
    Box(bd_pig + 7,  BD_GY - 9, 10, 6, 0x7FBF00UL);
}

static void BdRender(void)
{
    static int16_t lastX = BD_SX, lastY = BD_SY;

    if(s_first)
    {
        Box(0, BD_GY, FIELD_W, FIELD_H - BD_GY, 0x2E7D32UL);
        Box(BD_SX - 4, BD_SY + 8, 6, BD_GY - BD_SY - 8, COLOR_BROWN);       // slingshot
        lastX = BD_SX; lastY = BD_SY;
        bd_aimDirty = true;
    }

    if(!bd_fly)
    {
        // power meter (redrawn every frame while aiming)
        Box(10, 10, bd_pw, 10, COLOR_ORANGE);
        Box(10 + bd_pw, 10, 100 - bd_pw + 1, 10, COLOR_DARK_GRAY);

        if(bd_aimDirty)
        {
            Box(BD_SX - 10, BD_SY - 36, 60, 44, BD_BG);
            Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
            Graphics_drawLine(&g_sContext, BD_SX, BD_SY,
                              BD_SX + (kCos[bd_ang] * 28) / 100,
                              BD_SY - (kSin[bd_ang] * 28) / 100);
            Disc(BD_SX, BD_SY, 6, COLOR_RED);
            bd_aimDirty = false;
        }
    }
    else
    {
        if(bd_clear) { Box(BD_SX - 10, BD_SY - 36, 60, 44, BD_BG); bd_clear = false; }
        Box(lastX - 8, lastY - 8, 16, Min16(16, BD_GY - (lastY - 8)), BD_BG);
        if(bd_pigAlive) BdDrawPig();
        Disc(bd_dx, Min16(bd_dy, BD_GY - 8), 6, COLOR_RED);
        lastX = bd_dx;
        lastY = Min16(bd_dy, BD_GY - 8);
    }

    if(s_first && bd_pigAlive) BdDrawPig();
    if(!bd_pigAlive) Box(bd_pig, BD_GY - 24, 24, 24, BD_BG);
}

// ============================================================================
// EAT!  (Snake)  eat 3 apples without hitting a wall or yourself.
// ============================================================================
#define SN_C    10
#define SN_GW   32
#define SN_GH   23
#define SN_BG   0x101810UL

static int16_t sn_x[24], sn_y[24], sn_len, sn_dx, sn_dy, sn_ndx, sn_ndy;
static int16_t sn_ax, sn_ay, sn_ate, sn_cd, sn_mv, sn_ex, sn_ey;
static bool    sn_moved, sn_erase;

static void SnCell(int16_t cx, int16_t cy, uint32_t c)
{
    Box(cx * SN_C + 1, cy * SN_C + 1, SN_C - 2, SN_C - 2, c);
}

static bool SnOnSnake(int16_t x, int16_t y)
{
    int16_t i;
    for(i = 0; i < sn_len; i++)
        if(sn_x[i] == x && sn_y[i] == y) return true;
    return false;
}

static void SnPlaceApple(void)
{
    int16_t tries, x, y;

    for(tries = 0; tries < 60; tries++)
    {
        x = sn_x[0] + Rnd(13) - 6;
        y = sn_y[0] + Rnd(9) - 4;
        if(x < 0 || x >= SN_GW || y < 0 || y >= SN_GH) continue;
        if(SnOnSnake(x, y)) continue;
        if(Abs16(x - sn_x[0]) + Abs16(y - sn_y[0]) < 3) continue;
        sn_ax = x; sn_ay = y;
        return;
    }
    sn_ax = (sn_x[0] + 6) % SN_GW;  sn_ay = sn_y[0];
}

static void SnInit(uint16_t spd)
{
    sn_len = 3;
    sn_x[0] = 10; sn_y[0] = 11;
    sn_x[1] = 9;  sn_y[1] = 11;
    sn_x[2] = 8;  sn_y[2] = 11;
    sn_dx = sn_ndx = 1;
    sn_dy = sn_ndy = 0;
    sn_ate = 0;
    sn_mv  = 6 - Min16((int16_t)spd / 2, 2);
    sn_cd  = sn_mv;
    sn_moved = sn_erase = false;
    SnPlaceApple();
}

static int16_t SnUpdate(void)
{
    int16_t hx = HeldX(), hy = HeldY();
    int16_t nx, ny, i, lim;
    bool    eat;

    if(hx != 0 && sn_dx == 0)      { sn_ndx = hx; sn_ndy = 0; }
    else if(hy != 0 && sn_dy == 0) { sn_ndx = 0;  sn_ndy = hy; }

    sn_moved = false;
    if(--sn_cd > 0) return MG_PLAY;
    sn_cd = sn_mv;

    sn_dx = sn_ndx; sn_dy = sn_ndy;
    nx = sn_x[0] + sn_dx;
    ny = sn_y[0] + sn_dy;
    if(nx < 0 || nx >= SN_GW || ny < 0 || ny >= SN_GH) return MG_LOSE;

    eat = (nx == sn_ax && ny == sn_ay);
    lim = eat ? sn_len : sn_len - 1;                // the tail moves away unless we grow
    for(i = 0; i < lim; i++)
        if(sn_x[i] == nx && sn_y[i] == ny) return MG_LOSE;

    sn_erase = !eat;
    if(!eat) { sn_ex = sn_x[sn_len - 1]; sn_ey = sn_y[sn_len - 1]; }
    else if(sn_len < 23) sn_len++;

    for(i = sn_len - 1; i > 0; i--) { sn_x[i] = sn_x[i - 1]; sn_y[i] = sn_y[i - 1]; }
    sn_x[0] = nx; sn_y[0] = ny;
    sn_moved = true;

    if(eat)
    {
        Buzzer_PlaySfx(&sfx_coin);
        if(++sn_ate >= 3) return MG_WIN;
        SnPlaceApple();
    }
    return MG_PLAY;
}

static void SnRender(void)
{
    int16_t i;

    if(s_first)
    {
        for(i = sn_len - 1; i > 0; i--) SnCell(sn_x[i], sn_y[i], COLOR_GREEN);
        SnCell(sn_x[0], sn_y[0], COLOR_LIME);
        SnCell(sn_ax, sn_ay, COLOR_RED);
        return;
    }
    if(!sn_moved) return;

    if(sn_erase) Box(sn_ex * SN_C, sn_ey * SN_C, SN_C, SN_C, SN_BG);
    SnCell(sn_x[1], sn_y[1], COLOR_GREEN);
    SnCell(sn_x[0], sn_y[0], COLOR_LIME);
    SnCell(sn_ax, sn_ay, COLOR_RED);
}

// ============================================================================
// OUTLAST!  (Tron)  steer your light cycle; outlast the orange bike.
// ============================================================================
#define TR_C    8
#define TR_GW   40
#define TR_GH   28
#define TR_OY   3
#define TR_BG   0x000818UL

static uint16_t tr_g[TR_GW * TR_GH];                // 0 empty, 1 player, 2 enemy
static int16_t  tr_px, tr_py, tr_dx, tr_dy, tr_ndx, tr_ndy;
static int16_t  tr_ex, tr_ey, tr_edx, tr_edy, tr_cd, tr_step;
static bool     tr_new;

static bool TrFree(int16_t x, int16_t y)
{
    return x >= 0 && x < TR_GW && y >= 0 && y < TR_GH && tr_g[y * TR_GW + x] == 0;
}

static int16_t TrRoom(int16_t x, int16_t y, int16_t dx, int16_t dy)
{
    int16_t n = 0;
    while(n < 10 && TrFree(x + dx * (n + 1), y + dy * (n + 1))) n++;
    return n;
}

static void TrInit(uint16_t spd)
{
    uint16_t i;

    for(i = 0; i < TR_GW * TR_GH; i++) tr_g[i] = 0;
    tr_px = 4;  tr_py = 20; tr_dx = tr_ndx = 1;  tr_dy = tr_ndy = 0;
    tr_ex = 35; tr_ey = 7;  tr_edx = -1;         tr_edy = 0;
    tr_g[tr_py * TR_GW + tr_px] = 1;
    tr_g[tr_ey * TR_GW + tr_ex] = 2;
    tr_step = (spd >= 3) ? 2 : 3;
    tr_cd   = tr_step;
    tr_new  = false;
}

static int16_t TrUpdate(void)
{
    int16_t hx = HeldX(), hy = HeldY();
    int16_t nx, ny, enx, eny, ahead, rl, rr, ldx, ldy, rdx, rdy;
    bool    pc, ec;

    if(hx != 0 && tr_dx == 0)      { tr_ndx = hx; tr_ndy = 0; }
    else if(hy != 0 && tr_dy == 0) { tr_ndx = 0;  tr_ndy = hy; }

    tr_new = false;
    if(--tr_cd > 0) return MG_PLAY;
    tr_cd = tr_step;

    tr_dx = tr_ndx; tr_dy = tr_ndy;
    nx = tr_px + tr_dx;
    ny = tr_py + tr_dy;

    // Enemy: turn when a wall/trail is close, or now and then at random.
    ahead = TrRoom(tr_ex, tr_ey, tr_edx, tr_edy);
    if(ahead < 3 || Rnd(14) == 0)
    {
        ldx = tr_edy;  ldy = -tr_edx;
        rdx = -tr_edy; rdy = tr_edx;
        rl = TrRoom(tr_ex, tr_ey, ldx, ldy);
        rr = TrRoom(tr_ex, tr_ey, rdx, rdy);
        if(rl > 0 || rr > 0)
        {
            if(rr > rl || (rr == rl && Rnd(2))) { ldx = rdx; ldy = rdy; rl = rr; }
            if(ahead < 3 || rl >= 5) { tr_edx = ldx; tr_edy = ldy; }
        }
    }
    enx = tr_ex + tr_edx;
    eny = tr_ey + tr_edy;

    pc = !TrFree(nx, ny) || (nx == enx && ny == eny);
    ec = !TrFree(enx, eny);
    if(pc) return MG_LOSE;                          // a tie counts against you
    if(ec) return MG_WIN;

    tr_px = nx;  tr_py = ny;  tr_g[ny * TR_GW + nx]   = 1;
    tr_ex = enx; tr_ey = eny; tr_g[eny * TR_GW + enx] = 2;
    tr_new = true;
    return MG_PLAY;
}

static void TrCell(int16_t x, int16_t y, uint32_t c)
{
    Box(x * TR_C, y * TR_C + TR_OY, TR_C - 1, TR_C - 1, c);
}

static void TrRender(void)
{
    uint16_t i;

    if(s_first)
    {
        for(i = 0; i < TR_GW * TR_GH; i++)
            if(tr_g[i])
                TrCell(i % TR_GW, i / TR_GW, tr_g[i] == 1 ? COLOR_CYAN : COLOR_ORANGE);
        return;
    }
    if(!tr_new) return;
    TrCell(tr_px, tr_py, COLOR_CYAN);
    TrCell(tr_ex, tr_ey, COLOR_ORANGE);
}

// ============================================================================
// RETURN!  (Pong)  stick/D-pad up-down moves the paddle; return the ball 3 times.
// ============================================================================
#define PG_PH   52
#define PG_BG   0x000000UL

static int16_t pg_py, pg_ppy, pg_x, pg_y, pg_vx, pg_vy, pg_bx, pg_by, pg_hits, pg_shown;

static void PgInit(uint16_t spd)
{
    pg_py = pg_ppy = 90;
    pg_x = 150; pg_y = 100;
    pg_vx = -(7 + Min16((int16_t)spd, 3));            // serve toward the paddle
    pg_vy = (Rnd(2) ? 1 : -1) * (2 + Rnd(3));
    pg_bx = pg_x; pg_by = pg_y;
    pg_hits = 0;
    pg_shown = -1;
}

static int16_t PgUpdate(void)
{
    pg_py = Clamp16(pg_py + PaddleSpeed(6), 0, FIELD_H - PG_PH);

    pg_x += pg_vx;
    pg_y += pg_vy;
    if(pg_y < 0)             { pg_y = 0;             pg_vy = -pg_vy; }
    if(pg_y > FIELD_H - 8)   { pg_y = FIELD_H - 8;   pg_vy = -pg_vy; }
    if(pg_x > 304)           { pg_x = 304;           pg_vx = -Abs16(pg_vx); }

    if(pg_vx < 0 && pg_x <= 16 && pg_x + 8 > 8 &&
       pg_y + 8 > pg_py && pg_y < pg_py + PG_PH)
    {
        pg_x  = 17;
        pg_vx = Abs16(pg_vx);
        pg_vy = Clamp16(pg_vy + ((pg_y + 4) - (pg_py + PG_PH / 2)) / 6, -7, 7);
        if(pg_vy == 0) pg_vy = 1;
        pg_hits++;
        Buzzer_PlaySfx(&sfx_blip);
        if(pg_hits >= 3) return MG_WIN;
    }
    else if(pg_x < -8) return MG_LOSE;
    return MG_PLAY;
}

static void PgRender(void)
{
    int16_t i;

    if(s_first) Box(312, 0, 8, FIELD_H, COLOR_GRAY);

    if(s_first || pg_py != pg_ppy)
    {
        Box(8, pg_ppy, 8, PG_PH, PG_BG);
        Box(8, pg_py,  8, PG_PH, COLOR_WHITE);
        pg_ppy = pg_py;
    }

    Box(pg_bx, pg_by, 8, 8, PG_BG);
    Box(pg_x, pg_y, 8, 8, COLOR_YELLOW);
    pg_bx = pg_x; pg_by = pg_y;

    if(pg_shown != pg_hits)
    {
        for(i = 0; i < 3; i++)
            Box(130 + i * 24, 8, 16, 16, i < pg_hits ? COLOR_LIME : COLOR_DARK_GRAY);
        pg_shown = pg_hits;
    }
}

// ============================================================================
// FLAP!  (Flappy Bird)  any button flaps. Fly through the pipes.  Survive.
// ============================================================================
#define FB_X    80
#define FB_W    36
#define FB_GAP  80
#define FB_FLOOR 216
#define FB_BG   0x4EC0CAUL

static int16_t fb_y16, fb_vy, fb_py, fb_v;
static int16_t fb_x[2], fb_px[2], fb_gy[2];

static void FbInit(uint16_t spd)
{
    fb_y16 = 100 * 16;
    fb_vy  = 0;
    fb_py  = 100;
    fb_v   = 3 + Min16((int16_t)spd / 2, 2);
    fb_x[0] = fb_px[0] = 300;  fb_gy[0] = 60 + Rnd(80);
    fb_x[1] = fb_px[1] = 470;  fb_gy[1] = 60 + Rnd(80);
}

static int16_t FbUpdate(void)
{
    uint16_t i;
    int16_t  y;

    if(AnyPress()) { fb_vy = -110; Buzzer_PlaySfx(&sfx_blip); }
    fb_vy = Min16(fb_vy + 9, 150);
    fb_y16 += fb_vy;
    y = fb_y16 / 16;
    if(y < 0 || y > FB_FLOOR - 14) return MG_LOSE;

    for(i = 0; i < 2; i++)
    {
        fb_x[i] -= fb_v;
        if(fb_x[i] < -FB_W) { fb_x[i] = fb_x[1 - i] + 170; fb_gy[i] = 55 + Rnd(90); fb_px[i] = fb_x[i]; }
        if(Hit(FB_X, y, 16, 14, fb_x[i], 0, FB_W, FB_FLOOR) &&
           (y < fb_gy[i] || y + 14 > fb_gy[i] + FB_GAP))
            return MG_LOSE;
    }
    return MG_PLAY;
}

static void FbRender(void)
{
    uint16_t i;
    int16_t  y = fb_y16 / 16;

    if(s_first) Box(0, FB_FLOOR, FIELD_W, FIELD_H - FB_FLOOR, 0xDED895UL);

    for(i = 0; i < 2; i++)
    {
        SlideX(0, fb_gy[i], fb_px[i], fb_x[i], FB_W, COLOR_GREEN, FB_BG);
        SlideX(fb_gy[i] + FB_GAP, FB_FLOOR - fb_gy[i] - FB_GAP,
               fb_px[i], fb_x[i], FB_W, COLOR_GREEN, FB_BG);
        fb_px[i] = fb_x[i];
    }

    Box(FB_X - 1, fb_py - 1, 20, 16, FB_BG);
    Box(FB_X, y, 16, 14, COLOR_YELLOW);
    Box(FB_X + 10, y + 3, 4, 4, COLOR_WHITE);
    Box(FB_X + 12, y + 4, 2, 2, COLOR_BLACK);
    Box(FB_X + 14, y + 8, 4, 4, COLOR_ORANGE);
    fb_py = y;
}

// ============================================================================
// JUMP!  (Chrome Dino)  any button jumps over the cacti.  Survive.
// ============================================================================
#define DN_X    40
#define DN_GY   206
#define DN_BG   0xF7F7F7UL
#define DN_INK  0x535353UL

static int16_t dn_y, dn_vy, dn_py, dn_v, dn_cd;
static int16_t dn_cx[2], dn_cpx[2], dn_ch[2];
static bool    dn_on[2];

static void DnInit(uint16_t spd)
{
    dn_y = dn_py = DN_GY - 26;
    dn_vy = 0;
    dn_v  = 5 + Min16((int16_t)spd / 2, 2);
    dn_cd = 20;
    dn_on[0] = dn_on[1] = false;
}

static int16_t DnUpdate(void)
{
    uint16_t i;

    if(dn_y >= DN_GY - 26 && AnyPress()) { dn_vy = -12; Buzzer_PlaySfx(&sfx_jump); }
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
                dn_ch[i]  = 24 + 8 * Rnd(3);
                break;
            }
        dn_cd = (170 + Rnd(100)) / dn_v;
    }

    for(i = 0; i < 2; i++)
    {
        if(!dn_on[i]) continue;
        dn_cx[i] -= dn_v;
        if(dn_cx[i] < -16) { dn_on[i] = false; continue; }
        if(Hit(DN_X + 4, dn_y + 3, 12, 22, dn_cx[i] + 1, DN_GY - dn_ch[i], 12, dn_ch[i]))
            return MG_LOSE;
    }
    return MG_PLAY;
}

static void DnRender(void)
{
    uint16_t i;

    if(s_first) Box(0, DN_GY, FIELD_W, 2, DN_INK);

    for(i = 0; i < 2; i++)
        if(dn_on[i])
        {
            SlideX(DN_GY - dn_ch[i], dn_ch[i], dn_cpx[i], dn_cx[i], 14, DN_INK, DN_BG);
            dn_cpx[i] = dn_cx[i];
        }

    Box(DN_X, dn_py, 22, 26, DN_BG);
    Box(DN_X, dn_y + 6, 14, 14, DN_INK);                // body
    Box(DN_X + 8, dn_y, 12, 10, DN_INK);                // head
    Box(DN_X + 14, dn_y + 2, 3, 3, DN_BG);              // eye
    Box(DN_X + 4, dn_y + 20, 4, (s_frame & 4) ? 6 : 3, DN_INK);     // legs
    Box(DN_X + 11, dn_y + 20, 4, (s_frame & 4) ? 3 : 6, DN_INK);
    dn_py = dn_y;
}

// ============================================================================
// CHOMP!  (Pac-Man)  eat 4 pellets, don't touch the ghost.
// ============================================================================
#define CH_C    16
#define CH_GW   20
#define CH_GH   14
#define CH_BG   0x000010UL

static int16_t ch_px, ch_py, ch_dx, ch_dy, ch_gx, ch_gy, ch_pm, ch_gm, ch_pc, ch_gc;
static int16_t ch_ox, ch_oy, ch_ogx, ch_ogy, ch_left;
static int16_t ch_lx[4], ch_ly[4];
static bool    ch_on[4], ch_moved;

static bool ChBlocked(int16_t x, int16_t y)
{
    return x < 0 || x >= CH_GW || y < 0 || y >= CH_GH || ((x % 6) == 3 && (y % 4) == 1);
}

static bool ChPellet(int16_t x, int16_t y)
{
    uint16_t i;
    for(i = 0; i < 4; i++)
        if(ch_on[i] && ch_lx[i] == x && ch_ly[i] == y) return true;
    return false;
}

static void ChInit(uint16_t spd)
{
    uint16_t i;
    int16_t  x, y;

    ch_px = ch_ox = 0;  ch_py = ch_oy = 0;
    ch_dx = 1; ch_dy = 0;
    ch_gx = ch_ogx = CH_GW - 1;  ch_gy = ch_ogy = CH_GH - 1;
    ch_pm = 3;
    ch_gm = 8 - Min16((int16_t)spd, 3);
    ch_pc = ch_pm; ch_gc = ch_gm;
    ch_left = 4;
    ch_moved = false;

    for(i = 0; i < 4; i++)
    {
        do
        {
            x = Rnd(CH_GW); y = Rnd(CH_GH);
        } while(ChBlocked(x, y) || ChPellet(x, y) || Abs16(x - ch_px) + Abs16(y - ch_py) < 6);
        ch_lx[i] = x; ch_ly[i] = y; ch_on[i] = true;
    }
}

static void ChTryGhost(int16_t dx, int16_t dy, bool *moved)
{
    if(*moved || (dx == 0 && dy == 0)) return;
    if(!ChBlocked(ch_gx + dx, ch_gy + dy)) { ch_gx += dx; ch_gy += dy; *moved = true; }
}

static int16_t ChUpdate(void)
{
    int16_t hx = HeldX(), hy = HeldY();
    int16_t ddx, ddy;
    uint16_t i;
    bool    gm = false;

    ch_moved = false;

    if(hx != 0)      { if(!ChBlocked(ch_px + hx, ch_py)) { ch_dx = hx; ch_dy = 0; } }
    else if(hy != 0) { if(!ChBlocked(ch_px, ch_py + hy)) { ch_dx = 0; ch_dy = hy; } }

    if(--ch_pc <= 0)
    {
        ch_pc = ch_pm;
        if(!ChBlocked(ch_px + ch_dx, ch_py + ch_dy))
        {
            ch_ox = ch_px; ch_oy = ch_py;
            ch_px += ch_dx; ch_py += ch_dy;
            ch_moved = true;
        }
        for(i = 0; i < 4; i++)
            if(ch_on[i] && ch_lx[i] == ch_px && ch_ly[i] == ch_py)
            {
                ch_on[i] = false;
                Buzzer_PlaySfx(&sfx_coin);
                if(--ch_left == 0) return MG_WIN;
            }
    }

    if(--ch_gc <= 0)
    {
        ch_gc = ch_gm;
        ddx = Sgn16(ch_px - ch_gx);
        ddy = Sgn16(ch_py - ch_gy);
        ch_ogx = ch_gx; ch_ogy = ch_gy;
        if(Abs16(ch_px - ch_gx) >= Abs16(ch_py - ch_gy))
        { ChTryGhost(ddx, 0, &gm); ChTryGhost(0, ddy, &gm); }
        else
        { ChTryGhost(0, ddy, &gm); ChTryGhost(ddx, 0, &gm); }
        if(gm) ch_moved = true;
    }

    if(ch_px == ch_gx && ch_py == ch_gy) return MG_LOSE;
    return MG_PLAY;
}

static void ChCell(int16_t x, int16_t y)
{
    Box(x * CH_C, y * CH_C, CH_C, CH_C, CH_BG);
    if(ChBlocked(x, y))
    {
        Box(x * CH_C + 1, y * CH_C + 1, CH_C - 2, CH_C - 2, COLOR_NAVY);
        Box(x * CH_C + 3, y * CH_C + 3, CH_C - 6, CH_C - 6, COLOR_BLUE);
    }
    else if(ChPellet(x, y))
        Box(x * CH_C + 6, y * CH_C + 6, 4, 4, COLOR_WHITE);
}

static void ChRender(void)
{
    int16_t x, y, cx, cy;

    if(s_first)
    {
        for(y = 0; y < CH_GH; y++)
            for(x = 0; x < CH_GW; x++)
                if(ChBlocked(x, y) || ChPellet(x, y)) ChCell(x, y);
    }
    else if(ch_moved)
    {
        ChCell(ch_ox, ch_oy);
        ChCell(ch_ogx, ch_ogy);
    }
    else return;

    cx = ch_px * CH_C + 8;  cy = ch_py * CH_C + 8;
    Disc(cx, cy, 6, COLOR_YELLOW);
    if(s_frame & 4)                                     // mouth
        Box(cx + (ch_dx > 0 ? 1 : (ch_dx < 0 ? -7 : -2)),
            cy + (ch_dy > 0 ? 1 : (ch_dy < 0 ? -7 : -2)),
            ch_dx ? 7 : 4, ch_dy ? 7 : 4, CH_BG);

    cx = ch_gx * CH_C + 2;  cy = ch_gy * CH_C + 2;
    Box(cx, cy, 12, 12, COLOR_RED);
    Box(cx + 2, cy + 3, 3, 3, COLOR_WHITE);
    Box(cx + 7, cy + 3, 3, 3, COLOR_WHITE);
}

// ============================================================================
// WHACK!  (Whack-a-Mole)  press the D-pad direction where the mole pops up.
// ============================================================================
#define WH_BG   0x3B6B1FUL

static const int16_t kWhX[4] = { 160,  80, 240, 160 };   // U L R D
static const int16_t kWhY[4] = {  64, 124, 124, 184 };

static int16_t wh_mole, wh_drawn, wh_t, wh_gap, wh_hits, wh_shown, wh_last, wh_dur;

static void WhInit(uint16_t spd)
{
    wh_mole = -1; wh_drawn = -2;
    wh_gap  = 14;
    wh_hits = 0; wh_shown = -1; wh_last = -1;
    wh_dur  = Max16(16, 40 - 3 * (int16_t)spd);
}

static int16_t WhUpdate(void)
{
    int16_t p = -1;

    if(wh_mole < 0)
    {
        if(--wh_gap <= 0)
        {
            do { wh_mole = Rnd(4); } while(wh_mole == wh_last);
            wh_last = wh_mole;
            wh_t    = wh_dur;
        }
        return MG_PLAY;
    }

    if(EdgeU())      p = 0;
    else if(EdgeL()) p = 1;
    else if(EdgeR()) p = 2;
    else if(EdgeD()) p = 3;

    if(p == wh_mole)
    {
        wh_mole = -1;
        wh_gap  = 8;
        Buzzer_PlaySfx(&sfx_coin);
        if(++wh_hits >= 4) return MG_WIN;
    }
    else
    {
        if(p >= 0) wh_t -= 8;                           // wrong button: mole gets bolder
        if(--wh_t <= 0) return MG_LOSE;
    }
    return MG_PLAY;
}

static void WhDrawHole(int16_t i, bool mole)
{
    int16_t cx = kWhX[i], cy = kWhY[i];

    Box(cx - 32, cy - 36, 64, 56, WH_BG);
    Box(cx - 28, cy + 2, 56, 12, COLOR_BLACK);
    Box(cx - 24, cy, 48, 16, COLOR_BLACK);
    if(mole)
    {
        Box(cx - 18, cy - 32, 36, 36, COLOR_BROWN);
        Box(cx - 11, cy - 24, 8, 8, COLOR_WHITE);
        Box(cx + 3,  cy - 24, 8, 8, COLOR_WHITE);
        Box(cx - 8,  cy - 21, 3, 3, COLOR_BLACK);
        Box(cx + 5,  cy - 21, 3, 3, COLOR_BLACK);
        Box(cx - 5,  cy - 12, 10, 6, COLOR_PINK);
        Box(cx - 28, cy + 4, 56, 12, COLOR_BLACK);      // front rim
    }
}

static void WhRender(void)
{
    int16_t i;

    if(s_first)
    {
        for(i = 0; i < 4; i++) WhDrawHole(i, false);
        wh_drawn = -1; wh_shown = -1;
    }

    if(wh_drawn != wh_mole)
    {
        if(wh_drawn >= 0) WhDrawHole(wh_drawn, false);
        if(wh_mole >= 0)  WhDrawHole(wh_mole, true);
        wh_drawn = wh_mole;
    }

    if(wh_shown != wh_hits)
    {
        for(i = 0; i < 4; i++)
            Box(112 + i * 24, 8, 16, 16, i < wh_hits ? COLOR_GOLD : COLOR_DARK_GRAY);
        wh_shown = wh_hits;
    }
}

// ============================================================================
// CROSS!  (Frogger)  hop to the far bank. Stick/D-pad edges = one hop.
// ============================================================================
#define FR_RH   32
#define FR_BG   0x303030UL
#define FR_GRASS 0x1E6B1EUL

static int16_t fr_x, fr_row, fr_dxx, fr_drow;
static int16_t fr_cx[10], fr_cpx[10], fr_cv[10];

static uint32_t FrRowBg(int16_t row) { return (row == 0 || row == 6) ? FR_GRASS : FR_BG; }

static const uint32_t kFrCar[5] = { COLOR_RED, COLOR_BLUE, COLOR_YELLOW, COLOR_WHITE, COLOR_ORANGE };

static void FrInit(uint16_t spd)
{
    int16_t i, lane;

    fr_x = fr_dxx = 149;
    fr_row = fr_drow = 6;

    for(i = 0; i < 10; i++)
    {
        lane = 1 + i / 2;
        fr_cx[i]  = fr_cpx[i] = (((lane * 61) + (i & 1) * 180) % 360) - 44;
        fr_cv[i]  = ((lane & 1) ? 1 : -1) * (2 + (lane % 3) + Min16((int16_t)spd / 2, 2));
    }
}

static int16_t FrUpdate(void)
{
    int16_t i, lane, fy;

    if(EdgeU() && fr_row > 0)  { fr_row--; Buzzer_PlaySfx(&sfx_blip); }
    if(EdgeD() && fr_row < 6)  { fr_row++; Buzzer_PlaySfx(&sfx_blip); }
    if(EdgeL())                  fr_x = Max16(fr_x - 32, 4);
    if(EdgeR())                  fr_x = Min16(fr_x + 32, 294);

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
        if(lane == fr_row && Hit(fr_x, fy, 22, 22, fr_cx[i], lane * FR_RH + 5, 44, 22))
            return MG_LOSE;
    }
    return MG_PLAY;
}

static void FrRender(void)
{
    int16_t i, lane;

    if(s_first)
    {
        Box(0, 0, FIELD_W, FR_RH, FR_GRASS);
        Box(0, FR_RH, FIELD_W, FR_RH * 5, FR_BG);
        Box(0, FR_RH * 6, FIELD_W, FR_RH, FR_GRASS);
    }

    for(i = 0; i < 10; i++)
    {
        lane = 1 + i / 2;
        SlideX(lane * FR_RH + 5, 22, fr_cpx[i], fr_cx[i], 44, kFrCar[lane - 1], FR_BG);
        fr_cpx[i] = fr_cx[i];
    }

    if(s_first || fr_x != fr_dxx || fr_row != fr_drow)
    {
        Box(fr_dxx, fr_drow * FR_RH + 5, 22, 22, FrRowBg(fr_drow));
        Box(fr_x, fr_row * FR_RH + 5, 22, 22, COLOR_LIME);
        Box(fr_x + 3,  fr_row * FR_RH + 7, 5, 5, COLOR_WHITE);
        Box(fr_x + 14, fr_row * FR_RH + 7, 5, 5, COLOR_WHITE);
        Box(fr_x + 5,  fr_row * FR_RH + 9, 2, 2, COLOR_BLACK);
        Box(fr_x + 15, fr_row * FR_RH + 9, 2, 2, COLOR_BLACK);
        fr_dxx = fr_x; fr_drow = fr_row;
    }
}

// ============================================================================
// SLICE!  (Fruit Ninja)  swipe the cursor through 4 fruits. Avoid the bomb!
// ============================================================================
#define FN_N    3
#define FN_BG   0x1A1A2EUL

static int16_t fn_x[FN_N], fn_y4[FN_N], fn_vx[FN_N], fn_vy4[FN_N], fn_dx[FN_N], fn_dy[FN_N];
static bool    fn_on[FN_N], fn_bomb[FN_N], fn_dr[FN_N];
static int16_t fn_cx, fn_cy, fn_pcx, fn_pcy, fn_cd, fn_cnt, fn_shown, fn_rate;

static const uint32_t kFruit[3] = { COLOR_ORANGE, COLOR_RED, COLOR_LIME };

static void FnInit(uint16_t spd)
{
    uint16_t i;

    for(i = 0; i < FN_N; i++) { fn_on[i] = false; fn_dr[i] = false; }
    fn_cx = fn_pcx = 160;
    fn_cy = fn_pcy = 115;
    fn_cd = 10;
    fn_cnt = 0; fn_shown = -1;
    fn_rate = Max16(14, 26 - 2 * (int16_t)spd);
}

static int16_t FnUpdate(void)
{
    uint16_t i;
    int16_t  mx, my;

    mx = g_joy.dx / 9;
    my = g_joy.dy / 9;
    if(g_dpad.l) mx = -8;
    if(g_dpad.r) mx =  8;
    if(g_dpad.u) my = -8;
    if(g_dpad.d) my =  8;
    fn_cx = Clamp16(fn_cx + mx, 8, FIELD_W - 9);
    fn_cy = Clamp16(fn_cy + my, 8, FIELD_H - 9);

    if(--fn_cd <= 0)
    {
        for(i = 0; i < FN_N; i++)
            if(!fn_on[i])
            {
                fn_on[i]   = true;
                fn_bomb[i] = (Rnd(6) == 0);
                fn_x[i]    = 40 + Rnd(240);
                fn_y4[i]   = (FIELD_H + 14) * 4;
                fn_vy4[i]  = -(46 + Rnd(14));
                fn_vx[i]   = (fn_x[i] < 160) ? 1 + Rnd(3) : -(1 + Rnd(3));
                break;
            }
        fn_cd = fn_rate + Rnd(14);
    }

    for(i = 0; i < FN_N; i++)
    {
        if(!fn_on[i]) continue;
        fn_x[i]   += fn_vx[i];
        fn_vy4[i] += 2;
        fn_y4[i]  += fn_vy4[i];
        if(fn_vy4[i] > 0 && fn_y4[i] > (FIELD_H + 20) * 4) { fn_on[i] = false; continue; }

        if(Abs16(fn_x[i] - fn_cx) <= 18 && Abs16(fn_y4[i] / 4 - fn_cy) <= 18)
        {
            fn_on[i] = false;
            if(fn_bomb[i]) { Buzzer_PlaySfx(&sfx_explosion); return MG_LOSE; }
            Buzzer_PlaySfx(&sfx_laser);
            if(++fn_cnt >= 4) return MG_WIN;
        }
    }
    return MG_PLAY;
}

static void FnRender(void)
{
    uint16_t i;
    int16_t  y;

    for(i = 0; i < FN_N; i++)                           // erase everything that moved
        if(fn_dr[i]) { Box(fn_dx[i] - 14, fn_dy[i] - 14, 28, 28, FN_BG); fn_dr[i] = false; }
    Box(fn_pcx - 8, fn_pcy - 8, 16, 16, FN_BG);

    for(i = 0; i < FN_N; i++)
    {
        if(!fn_on[i]) continue;
        y = fn_y4[i] / 4;
        Disc(fn_x[i], y, 12, fn_bomb[i] ? COLOR_DARK_GRAY : kFruit[i % 3]);
        if(fn_bomb[i]) Box(fn_x[i] - 2, y - 15, 4, 4, COLOR_RED);
        else           Box(fn_x[i] - 2, y - 4, 4, 4, COLOR_WHITE);
        fn_dx[i] = fn_x[i]; fn_dy[i] = y; fn_dr[i] = true;
    }

    Box(fn_cx - 7, fn_cy - 7, 14, 14, COLOR_WHITE);
    Box(fn_cx - 4, fn_cy - 4, 8, 8, COLOR_CHARCOAL);
    fn_pcx = fn_cx; fn_pcy = fn_cy;

    if(fn_shown != fn_cnt)
    {
        for(i = 0; i < 4; i++)
            Box(10 + i * 22, 8, 16, 16, (int16_t)i < fn_cnt ? COLOR_GOLD : COLOR_DARK_GRAY);
        fn_shown = fn_cnt;
    }
}

// ============================================================================
// SHOOT!  (Space Invaders)  button fires. Wipe out all 5 before they land.
// ============================================================================
#define IV_N    5
#define IV_W    26
#define IV_H    14
#define IV_SY   205
#define IV_STEP 6                       // px per march step

static int16_t iv_x[IV_N], iv_px[IV_N], iv_y, iv_py, iv_dir, iv_cd, iv_int, iv_left;
static bool    iv_alive[IV_N], iv_dirty;
static int16_t sh_x, sh_px, bl_x, bl_y, bl_px, bl_py;
static bool    bl_on;

static void IvInit(uint16_t spd)
{
    uint16_t i;

    for(i = 0; i < IV_N; i++) { iv_x[i] = iv_px[i] = 30 + (int16_t)i * 46; iv_alive[i] = true; }
    iv_y = iv_py = 28;
    iv_dir = 1;
    iv_int = Max16(5, 8 - (int16_t)spd / 2);
    iv_cd  = iv_int;
    iv_left = IV_N;
    iv_dirty = true;
    sh_x = sh_px = 147;
    bl_on = false; bl_px = bl_py = -20;
}

static int16_t IvUpdate(void)
{
    uint16_t i;
    int16_t  nx;
    bool     edge = false;

    sh_x = Clamp16(sh_x + HeldX() * 5, 0, FIELD_W - 26);

    if(!bl_on && AnyPress())
    {
        bl_on = true; bl_x = sh_x + 12; bl_y = 196;
        Buzzer_PlaySfx(&sfx_laser);
    }
    if(bl_on)
    {
        bl_y -= 9;
        if(bl_y < -8) bl_on = false;
        for(i = 0; bl_on && i < IV_N; i++)
            if(iv_alive[i] && Hit(bl_x, bl_y, 3, 8, iv_x[i], iv_y, IV_W, IV_H))
            {
                iv_alive[i] = false; bl_on = false; iv_dirty = true;
                Buzzer_PlaySfx(&sfx_explosion);
                if(--iv_left == 0) return MG_WIN;
            }
    }

    if(--iv_cd <= 0)
    {
        iv_cd = iv_int;
        for(i = 0; i < IV_N; i++)
        {
            nx = iv_x[i] + IV_STEP * iv_dir;
            if(iv_alive[i] && (nx < 6 || nx > FIELD_W - 6 - IV_W)) edge = true;
        }
        if(edge) { iv_dir = -iv_dir; iv_y += 12; }
        else for(i = 0; i < IV_N; i++) iv_x[i] += IV_STEP * iv_dir;
        iv_dirty = true;
        if(iv_y + IV_H >= 190) return MG_LOSE;
    }
    return MG_PLAY;
}

static void IvRender(void)
{
    uint16_t i;

    if(iv_dirty || s_first)
    {
        for(i = 0; i < IV_N; i++)
        {
            Box(iv_px[i], iv_py, IV_W, IV_H, COLOR_BLACK);
            if(iv_alive[i])
            {
                Box(iv_x[i], iv_y, IV_W, IV_H, COLOR_MAGENTA);
                Box(iv_x[i] + 5,  iv_y + 4, 4, 4, COLOR_BLACK);
                Box(iv_x[i] + 17, iv_y + 4, 4, 4, COLOR_BLACK);
                Box(iv_x[i] + 3,  iv_y + IV_H - 3, 4, 3, COLOR_BLACK);
                Box(iv_x[i] + 19, iv_y + IV_H - 3, 4, 3, COLOR_BLACK);
            }
            iv_px[i] = iv_x[i];
        }
        iv_py = iv_y;
        iv_dirty = false;
    }

    Box(bl_px, bl_py, 3, 8, COLOR_BLACK);
    if(bl_on) Box(bl_x, bl_y, 3, 8, COLOR_YELLOW);
    bl_px = bl_x; bl_py = bl_y;

    if(s_first || sh_x != sh_px)
    {
        Box(sh_px, IV_SY - 7, 26, 19, COLOR_BLACK);
        Box(sh_x, IV_SY, 26, 12, COLOR_LIME);
        Box(sh_x + 11, IV_SY - 6, 4, 6, COLOR_LIME);
        sh_px = sh_x;
    }
}

// ============================================================================
// FIT IT!  (Tetris)  slide the I-piece over the gap and drop it in.
// ============================================================================
#define TT_X0   80
#define TT_C    16
#define TT_BG   0x0A0A20UL

static int16_t tt_col, tt_row, tt_gap, tt_fd, tt_tick, tt_pcol, tt_prow;

static void TtCell(int16_t c, int16_t r, uint32_t color)
{
    Box(TT_X0 + c * TT_C, r * TT_C, TT_C - 1, TT_C - 1, color);
}

static void TtInit(uint16_t spd)
{
    tt_gap  = Rnd(7);
    tt_col  = (tt_gap + 2 + Rnd(4)) % 7;
    tt_row  = 0;
    tt_fd   = Max16(3, 8 - (int16_t)spd / 2);
    tt_tick = 0;
    tt_pcol = tt_col;
    tt_prow = -1;
}

static int16_t TtUpdate(void)
{
    if(EdgeL() && tt_col > 0) { tt_col--; Buzzer_PlaySfx(&sfx_blip); }
    if(EdgeR() && tt_col < 6) { tt_col++; Buzzer_PlaySfx(&sfx_blip); }

    if(++tt_tick >= ((HeldY() > 0) ? 1 : tt_fd))
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

static void TtRender(void)
{
    int16_t c;

    if(s_first)
    {
        Box(TT_X0 - 2, 0, 2, 224, COLOR_GRAY);
        Box(TT_X0 + 10 * TT_C, 0, 2, 224, COLOR_GRAY);
        for(c = 0; c < 10; c++)
            TtCell(c, 13, (c >= tt_gap && c < tt_gap + 4) ? 0x1C1C44UL : COLOR_SILVER);
        tt_prow = -1;
    }

    if(s_first || tt_col != tt_pcol || tt_row != tt_prow)
    {
        if(tt_prow >= 0 && tt_prow < 13) Box(TT_X0 + tt_pcol * TT_C, tt_prow * TT_C, 4 * TT_C, TT_C, TT_BG);
        for(c = 0; c < 4; c++) TtCell(tt_col + c, tt_row, COLOR_CYAN);
        tt_pcol = tt_col; tt_prow = tt_row;
    }
}

//*****************************************************************************
// 3. GAME TABLE  <- add a microgame: one line here
//      cmd, inspiration tag, frames, background, winOnTimeout, init, update, render
//*****************************************************************************
static const MicroGame kGames[] =
{
    { "DODGE!",   "Subway Surfers",  170, SF_BG,   1, SfInit, SfUpdate, SfRender },
    { "FLY!",     "Jetpack Joyride", 170, JT_BG,   1, JtInit, JtUpdate, JtRender },
    { "LAUNCH!",  "Angry Birds",     260, BD_BG,   0, BdInit, BdUpdate, BdRender },
    { "EAT!",     "Snake",           280, SN_BG,   0, SnInit, SnUpdate, SnRender },
    { "OUTLAST!", "Tron",            190, TR_BG,   1, TrInit, TrUpdate, TrRender },
    { "RETURN!",  "Pong",            300, PG_BG,   0, PgInit, PgUpdate, PgRender },
    { "FLAP!",    "Flappy Bird",     200, FB_BG,   1, FbInit, FbUpdate, FbRender },
    { "JUMP!",    "Chrome Dino",     180, DN_BG,   1, DnInit, DnUpdate, DnRender },
    { "CHOMP!",   "Pac-Man",         330, CH_BG,   0, ChInit, ChUpdate, ChRender },
    { "WHACK!",   "Whack-a-Mole",    260, WH_BG,   0, WhInit, WhUpdate, WhRender },
    { "CROSS!",   "Frogger",         280, FR_BG,   0, FrInit, FrUpdate, FrRender },
    { "SLICE!",   "Fruit Ninja",     280, FN_BG,   0, FnInit, FnUpdate, FnRender },
    { "SHOOT!",   "Space Invaders",  340, COLOR_BLACK, 0, IvInit, IvUpdate, IvRender },
    { "FIT IT!",  "Tetris",          240, TT_BG,   0, TtInit, TtUpdate, TtRender },
};

//*****************************************************************************
// 4. FRAMEWORK
//*****************************************************************************
static const uint32_t kBanner[6] =
{ COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE, COLOR_LIME, COLOR_PINK };

static uint16_t s_phase, s_t, s_cur, s_lives, s_score, s_played, s_level;
static uint16_t s_total, s_left, s_bag[32], s_bagPos, s_speedUp;
static uint32_t s_best;
static bool     s_dirty, s_won, s_newBest;
static int16_t  s_barPx;
static bool     s_barRed;

// ---- fuse bar ---------------------------------------------------------------
static void BarFull(void)
{
    s_barRed = ((uint32_t)s_left * 4 < s_total);
    s_barPx  = (int16_t)(((int32_t)FIELD_W * s_left) / s_total);
    Graphics_FillRect(0, BAR_Y0, FIELD_W - 1, BAR_Y1, COLOR_BLACK);
    if(s_barPx > 0)
        Graphics_FillRect(0, BAR_Y0, s_barPx - 1, BAR_Y1, s_barRed ? COLOR_RED : COLOR_LIME);
}

static void BarUpdate(void)
{
    int16_t px = (int16_t)(((int32_t)FIELD_W * s_left) / s_total);

    if(!s_barRed && (uint32_t)s_left * 4 < s_total)
    {
        s_barRed = true;
        if(px > 0) Graphics_FillRect(0, BAR_Y0, px - 1, BAR_Y1, COLOR_RED);
    }
    if(px < s_barPx)
    {
        Graphics_FillRect(px, BAR_Y0, s_barPx - 1, BAR_Y1, COLOR_BLACK);
        s_barPx = px;
    }
}

// ---- screens ----------------------------------------------------------------
static void Banner(const char *s, uint32_t bg, uint32_t fg)
{
    Graphics_FillRect(0, 85, FIELD_W - 1, 145, bg);
    BoldText(s, FIELD_W / 2, 115, fg);
}

static void DrawTitle(void)
{
    char    buf[24];
    int16_t i;

    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);

    for(i = 0; i < 8; i++)
        Graphics_FillRect(20 + i * 36, 30, 40 + i * 36, 44, kBanner[i % 6]);
    BoldText("WASON WARE", FIELD_W / 2 + 2, 92, COLOR_MAGENTA);
    BoldText("WASON WARE", FIELD_W / 2,     90, COLOR_YELLOW);
    Text("14 microgames. 4 lives. Go!", FIELD_W / 2, 125, COLOR_WHITE);

    Label(buf, "Best: ", s_best);
    Text(buf, FIELD_W / 2, 165, COLOR_GOLD);
    Text("D / button: start    R: back", FIELD_W / 2, 220, COLOR_GRAY);
}

static void DrawIntro(void)
{
    char    buf[24];
    int16_t i;
    const MicroGame *g = &kGames[s_cur];

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);

    for(i = 0; i < WW_LIVES; i++)
        Graphics_FillRect(110 + i * 24, 12, 125 + i * 24, 27,
                          (uint16_t)i < s_lives ? COLOR_RED : COLOR_DARK_GRAY);

    Banner(g->cmd, kBanner[s_played % 6], COLOR_BLACK);
    Text(g->inspo, FIELD_W / 2, 165, COLOR_GRAY);
    Label(buf, "Score: ", s_score);
    Text(buf, FIELD_W / 2, 205, COLOR_WHITE);
    if(s_speedUp) BoldText("SPEED UP!", FIELD_W / 2, 55, COLOR_ORANGE);
}

static void DrawOver(void)
{
    char buf[24];

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);
    BoldText("GAME OVER", FIELD_W / 2, 55, COLOR_RED);
    Label(buf, "Score: ", s_score);
    Text(buf, FIELD_W / 2, 105, COLOR_WHITE);
    Label(buf, "Best: ", s_best);
    Text(buf, FIELD_W / 2, 135, COLOR_GOLD);
    if(s_newBest) BoldText("NEW BEST!", FIELD_W / 2, 170, COLOR_LIME);
    Text("button: retry    R: back", FIELD_W / 2, 220, COLOR_GRAY);
}

// ---- flow -------------------------------------------------------------------
static void RefillBag(void)
{
    uint16_t i, j, t, last = s_bag[NUM_GAMES - 1];   // avoid repeating the previous game

    for(i = 0; i < NUM_GAMES; i++) s_bag[i] = i;
    for(i = NUM_GAMES - 1; i > 0; i--)
    {
        j = Rand16() % (i + 1);
        t = s_bag[i]; s_bag[i] = s_bag[j]; s_bag[j] = t;
    }
    if(s_bag[0] == last && s_played > 0)
    { t = s_bag[0]; s_bag[0] = s_bag[1]; s_bag[1] = t; }
    s_bagPos = 0;
}

static void PickGame(void)
{
    if(s_bagPos >= NUM_GAMES) RefillBag();
    s_cur = s_bag[s_bagPos++];
}

static void GoPhase(uint16_t phase)
{
    s_phase = phase;
    s_t     = 0;
    s_dirty = true;
}

static void StartRun(void)
{
    s_lives = WW_LIVES; s_score = 0; s_played = 0; s_level = 0;
    s_speedUp = 0; s_newBest = false;
    s_bag[NUM_GAMES - 1] = 0xFFFF; s_bagPos = NUM_GAMES;
    Rand_Seed(TimerNow() + 12345UL);
    PickGame();
    Buzzer_PlaySfx(&sfx_menu_select);
    GoPhase(PH_INTRO);
}

static void EnterPlay(void)
{
    const MicroGame *g = &kGames[s_cur];

    s_total = g->frames;
    if(!g->winOnTimeout)                            // "do it" games get tighter each level
        s_total -= (uint16_t)(((uint32_t)g->frames * Min16((int16_t)s_level, 5) * 6) / 100);
    s_left  = s_total;
    s_frame = 0;
    g->init(s_level);
    GoPhase(PH_PLAY);
}

static void EndGame(bool won)
{
    s_won = won;
    if(won) { s_score++; Buzzer_PlaySfx(&sfx_powerup); }
    else    { s_lives--; Buzzer_PlaySfx(&sfx_hit); }
    GoPhase(PH_RESULT);
}

static void EnterOver(void)
{
    SaveWason sv;

    s_newBest = (s_score > s_best);
    if(s_newBest)
    {
        s_best = s_score;
        sv.best = s_best;
        Save_Write(SAVE_SLOT_WASONWARE, &sv, sizeof sv);
        Save_Flush();                               // blocks briefly (flash)
    }
    Buzzer_PlaySfx(&sfx_game_over);
    GoPhase(PH_OVER);
}

static void NextGame(void)
{
    s_played++;
    if(s_lives == 0) { EnterOver(); return; }

    s_level   = s_played / WW_PER_LEVEL;
    s_speedUp = (s_played % WW_PER_LEVEL == 0);
    PickGame();
    GoPhase(PH_INTRO);
}

//*****************************************************************************
// App callbacks
//*****************************************************************************
static void WW_Init(void)
{
    SaveWason sv;

    s_best = Save_Read(SAVE_SLOT_WASONWARE, &sv, sizeof sv) ? sv.best : 0;
    Buzzer_Stop();
    GoPhase(PH_TITLE);
}

static AppStatus WW_Update(void)
{
    int16_t r;

    s_t++;

    switch(s_phase)
    {
        case PH_TITLE:
            if(g_dpad.rPressed) return APP_QUIT;
            if(g_joy.btnPressed || g_dpad.dPressed) StartRun();
            break;

        case PH_INTRO:
            if(s_t >= WW_INTRO_FRAMES) EnterPlay();
            break;

        case PH_PLAY:
            s_frame++;
            r = kGames[s_cur].update();
            if(s_left > 0) s_left--;
            if(r == MG_PLAY && s_left == 0)
                r = kGames[s_cur].winOnTimeout ? MG_WIN : MG_LOSE;
            if(r != MG_PLAY) EndGame(r == MG_WIN);
            break;

        case PH_RESULT:
            if(s_t >= WW_RESULT_FRAMES) NextGame();
            break;

        default:                                    // PH_OVER
            if(g_dpad.rPressed) return APP_QUIT;
            if(g_joy.btnPressed || g_dpad.dPressed) StartRun();
            break;
    }
    return APP_CONTINUE;
}

static void WW_Render(void)
{
    const MicroGame *g = &kGames[s_cur];

    switch(s_phase)
    {
        case PH_TITLE:
            if(s_dirty) { DrawTitle(); s_dirty = false; }
            break;

        case PH_INTRO:
            if(s_dirty) { DrawIntro(); s_dirty = false; }
            break;

        case PH_PLAY:
            if(s_dirty)
            {
                Graphics_FillRect(0, 0, FIELD_W - 1, FIELD_H - 1, g->bg);
                BarFull();
                s_first = true;
                s_dirty = false;
            }
            g->render();
            s_first = false;
            BarUpdate();
            break;

        case PH_RESULT:
            if(s_dirty)
            {
                Graphics_setFont(&g_sContext, &g_sFontCmss20b);
                Graphics_FillRect(0, 0, FIELD_W - 1, FIELD_H - 1, g->bg);
                s_first = true;                     // repaint the final frame from state
                g->render();
                s_first = false;
                if(s_won) Banner("NICE!", COLOR_LIME, COLOR_BLACK);
                else      Banner("OOPS!", COLOR_RED,  COLOR_WHITE);
                s_dirty = false;
            }
            break;

        default:
            if(s_dirty) { DrawOver(); s_dirty = false; }
            break;
    }
}

// Back from the Settings overlay: repaint the current screen, keep the state.
static void WW_Redraw(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    s_dirty = true;
}

static void WW_Exit(void)
{
    Buzzer_Stop();
}

//*****************************************************************************
// 5. The app descriptor: the ONLY public symbol in this file.
//*****************************************************************************
const App g_wasonwareApp =
{
    "WASON WARE",                           // name (first letter = menu tile icon)
    "14 microgames, 4 lives",               // hint shown under the menu
    COLOR_MAGENTA,                          // menu tile color
    WW_FRAME_US,                            // frame period
    WW_Init, WW_Update, WW_Render,
    WW_Exit,
    NULL,                                   // not a legacy app
    WW_Redraw
};
