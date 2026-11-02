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

enum { PH_TITLE, PH_INTRO, PH_PLAY, PH_RESULT, PH_OVER, PH_PICK };
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

// Filled "circle" (octagon from 3 rectangles - grlib's circle.c is not in this
// project, so Graphics_fillCircle would not link). Skipped if any part would
// leave the playfield.
static void Disc(int16_t x, int16_t y, int16_t r, uint32_t c)
{
    int16_t a = (r * 3) / 4;
    int16_t b = r / 2;

    if(x - r < 0 || y - r < 0 || x + r >= FIELD_W || y + r >= FIELD_H) return;
    Graphics_FillRect(x - a, y - a, x + a, y + a, c);
    Graphics_FillRect(x - r, y - b, x + r, y + b, c);
    Graphics_FillRect(x - b, y - r, x + b, y + r, c);
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

// Arrow of unit size u (u=4 -> 24 px long). dir: 0 up, 1 down, 2 left, 3 right.
static void Arrow(int16_t cx, int16_t cy, int16_t dir, int16_t u, uint32_t c)
{
    int16_t r, w, sw = (3 * u) / 2;

    for(r = 0; r < 3; r++)
    {
        w = (2 * r + 1) * u;
        if(dir == 0)      Box(cx - w / 2, cy - 3 * u + r * u, w, u, c);
        else if(dir == 1) Box(cx - w / 2, cy + 3 * u - (r + 1) * u, w, u, c);
        else if(dir == 2) Box(cx - 3 * u + r * u, cy - w / 2, u, w, c);
        else              Box(cx + 3 * u - (r + 1) * u, cy - w / 2, u, w, c);
    }
    if(dir == 0)      Box(cx - sw / 2, cy, sw, 3 * u, c);
    else if(dir == 1) Box(cx - sw / 2, cy - 3 * u, sw, 3 * u, c);
    else if(dir == 2) Box(cx, cy - sw / 2, 3 * u, sw, c);
    else              Box(cx - 3 * u, cy - sw / 2, 3 * u, sw, c);
}

//*****************************************************************************
// 2. MICROGAMES
//*****************************************************************************

// ============================================================================
// CLIMB!  (Donkey Kong)  jump the rolling barrels, then climb the ladder.
//         L/R run, button or UP jumps, UP at the ladder climbs.
// ============================================================================
#define DK_GY   204
#define DK_PY   188                         // player top when standing
#define DK_TOP  108                         // player top on the upper girder
#define DK_LX   240                         // ladder left edge
#define DK_BG   0x101030UL

static int16_t dk_x, dk_y, dk_vy, dk_px, dk_py, dk_v, dk_cd;
static int16_t dk_bx[3], dk_bpx[3];
static bool    dk_on[3], dk_climb;

static void DkLadder(void)
{
    int16_t k;

    Box(DK_LX, 124, 3, 80, COLOR_CYAN);
    Box(DK_LX + 9, 124, 3, 80, COLOR_CYAN);
    for(k = 0; k < 8; k++) Box(DK_LX, 130 + k * 10, 12, 2, COLOR_CYAN);
}

static void DkInit(uint16_t spd)
{
    uint16_t i;

    dk_x = dk_px = 20;
    dk_y = dk_py = DK_PY;
    dk_vy = 0;
    dk_climb = false;
    dk_v  = 3 + Min16((int16_t)spd / 3, 2);
    dk_cd = 20;
    for(i = 0; i < 3; i++) dk_on[i] = false;
}

static int16_t DkUpdate(void)
{
    uint16_t i;
    bool     onLadder;

    if(dk_climb)
    {
        if(HeldX() != 0) dk_climb = false;
        else if(HeldY() < 0)
        {
            dk_y -= 2;
            if(dk_y <= DK_TOP) { dk_y = DK_TOP; return MG_WIN; }
        }
        else if(HeldY() > 0)
        {
            dk_y += 2;
            if(dk_y >= DK_PY) { dk_y = DK_PY; dk_climb = false; }
        }
    }
    if(!dk_climb)
    {
        dk_x = Clamp16(dk_x + HeldX() * 3, 0, 304);
        onLadder = (Abs16(dk_x - DK_LX) <= 8);
        if(onLadder && HeldY() < 0)
        {
            dk_climb = true; dk_x = DK_LX; dk_vy = 0;
        }
        else
        {
            if(dk_y >= DK_PY && dk_vy == 0 && (g_joy.btnPressed || g_dpad.uPressed))
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
        dk_cd = Max16(24, 40 + Rnd(30) - 4 * dk_v);
    }
    for(i = 0; i < 3; i++)
    {
        if(!dk_on[i]) continue;
        dk_bx[i] -= dk_v + 1;
        if(dk_bx[i] < -14) { dk_on[i] = false; continue; }
        if(Hit(dk_x + 1, dk_y + 1, 10, 14, dk_bx[i], DK_GY - 12, 12, 12)) return MG_LOSE;
    }
    return MG_PLAY;
}

static void DkRender(void)
{
    uint16_t i;
    bool     nearLadder = (dk_x + 12 > DK_LX - 4 && dk_x < DK_LX + 16) ||
                          (dk_px + 12 > DK_LX - 4 && dk_px < DK_LX + 16);

    if(s_first)
    {
        Box(0, DK_GY, FIELD_W, 8, COLOR_RED);
        Box(190, 124, 130, 8, COLOR_RED);
        Box(272, 82, 40, 42, COLOR_BROWN);                      // Donkey Kong
        Box(278, 90, 10, 8, COLOR_WHITE);  Box(296, 90, 10, 8, COLOR_WHITE);
        Box(282, 92, 3, 4, COLOR_BLACK);   Box(299, 92, 3, 4, COLOR_BLACK);
        Box(280, 106, 24, 10, 0xC8A070UL);
        DkLadder();
    }

    for(i = 0; i < 3; i++)
    {
        if(dk_on[i])
        {
            SlideX(DK_GY - 12, 12, dk_bpx[i], dk_bx[i], 12, COLOR_ORANGE, DK_BG);
            dk_bpx[i] = dk_bx[i];
        }
    }

    Box(dk_px, dk_py, 12, 16, DK_BG);
    if(nearLadder) DkLadder();
    Box(dk_x, dk_y, 12, 6, COLOR_RED);                          // cap
    Box(dk_x + 1, dk_y + 6, 10, 4, 0xFFCC99UL);                 // face
    Box(dk_x + 1, dk_y + 10, 10, 6, COLOR_BLUE);                // overalls
    dk_px = dk_x; dk_py = dk_y;
}

// ============================================================================
// BATTLE!  (Pokemon)  pick the super-effective move: L = WATER, UP = FIRE,
//          R = GRASS. Water > Fire > Grass > Water. Land 3 hits.
// ============================================================================
#define PK_BG   0x203040UL

static const uint32_t kPkCol[3]    = { COLOR_ORANGE, COLOR_BLUE, COLOR_GREEN };
static const int16_t  kPkCounter[3] = { 1, 2, 0 };          // fire<-water, water<-grass, grass<-fire
static const char    *kPkName[3]   = { "FIRE", "WATER", "GRASS" };

static int16_t pk_t, pk_hits, pk_shown, pk_drawn;

static void PkInit(uint16_t spd)
{
    pk_t = Rnd(3);
    pk_hits = 0;
    pk_shown = pk_drawn = -1;
}

static int16_t PkUpdate(void)
{
    int16_t p = -1;

    if(EdgeU())      p = 0;
    else if(EdgeL()) p = 1;
    else if(EdgeR()) p = 2;
    if(p < 0) return MG_PLAY;

    if(p != kPkCounter[pk_t]) return MG_LOSE;
    Buzzer_PlaySfx(&sfx_explosion);
    if(++pk_hits >= 3) return MG_WIN;
    pk_t = (pk_t + 1 + Rnd(2)) % 3;
    return MG_PLAY;
}

static void PkRender(void)
{
    if(s_first)
    {
        Box(20, 170, 80, 50, COLOR_BLUE);
        Box(120, 170, 80, 50, COLOR_RED);
        Box(220, 170, 80, 50, COLOR_GREEN);
        Text("< WATER", 60, 195, COLOR_WHITE);
        Text("^ FIRE", 160, 195, COLOR_WHITE);
        Text("GRASS >", 260, 195, COLOR_WHITE);
        pk_shown = pk_drawn = -1;
    }

    if(pk_drawn != pk_t)
    {
        Box(60, 36, 200, 124, PK_BG);
        Disc(160, 100, 32, kPkCol[pk_t]);
        Box(145, 92, 10, 10, COLOR_WHITE);  Box(165, 92, 10, 10, COLOR_WHITE);
        Box(149, 96, 4, 4, COLOR_BLACK);    Box(169, 96, 4, 4, COLOR_BLACK);
        Box(150, 112, 20, 4, COLOR_BLACK);
        if(pk_t == 0)      { Box(152, 56, 16, 14, COLOR_YELLOW); Box(157, 46, 6, 10, COLOR_RED); }
        else if(pk_t == 1) { Box(154, 50, 12, 20, COLOR_SKY_BLUE); }
        else               { Box(146, 52, 28, 10, COLOR_LIME); Box(156, 44, 8, 10, COLOR_LIME); }
        Text(kPkName[pk_t], 160, 148, COLOR_WHITE);
        pk_drawn = pk_t;
    }

    if(pk_shown != pk_hits)
    {
        Box(80, 12, 160, 10, COLOR_DARK_GRAY);
        Box(80, 12, (160 * (3 - pk_hits)) / 3, 10, COLOR_LIME);
        pk_shown = pk_hits;
    }
}

// ============================================================================
// FLIP!  (Pokemon Pinball)  L / R (or stick, or button) work the flippers.
//        Hit the bumpers 5 times, don't drain.
// ============================================================================
#define PB_BG   0x14143CUL
#define PB_FY   196

static const int16_t kPbBx[3] = { 100, 220, 160 };
static const int16_t kPbBy[3] = {  70,  70, 120 };

static int16_t pb_x4, pb_y4, pb_vx, pb_vy, pb_px, pb_py, pb_hits;
static int16_t pb_flash[3];
static bool    pb_lf, pb_rf;

static void PbInit(uint16_t spd)
{
    pb_x4 = (130 + Rnd(60)) * 4;
    pb_y4 = 40 * 4;
    pb_vx = Rnd(25) - 12;
    pb_vy = 0;
    pb_px = pb_x4 / 4; pb_py = pb_y4 / 4;
    pb_hits = 0;
    pb_flash[0] = pb_flash[1] = pb_flash[2] = 0;
    pb_lf = pb_rf = false;
}

static int16_t PbUpdate(void)
{
    int16_t i, x, y, dx, dy;

    pb_lf = g_dpad.l || g_joy.dx < -50 || g_joy.btn;
    pb_rf = g_dpad.r || g_joy.dx >  50 || g_joy.btn;

    pb_vy = Min16(pb_vy + 1, 40);
    pb_x4 += pb_vx;
    pb_y4 += pb_vy;
    x = pb_x4 / 4;  y = pb_y4 / 4;

    if(x < 25)  { pb_x4 = 100;  pb_vx =  Abs16(pb_vx); }
    if(x > 295) { pb_x4 = 1180; pb_vx = -Abs16(pb_vx); }
    if(y < 12)  { pb_y4 = 48;   pb_vy =  Abs16(pb_vy); }
    x = pb_x4 / 4;  y = pb_y4 / 4;

    for(i = 0; i < 3; i++)
    {
        if(pb_flash[i] > 0) pb_flash[i]--;
        dx = x - kPbBx[i];  dy = y - kPbBy[i];
        if(dx * dx + dy * dy < 361)
        {
            pb_vx = Clamp16(dx * 3, -36, 36);
            pb_vy = Clamp16(dy * 3, -36, 36);
            if(dx == 0 && dy == 0) pb_vy = -24;
            pb_flash[i] = 6;
            Buzzer_PlaySfx(&sfx_coin);
            if(++pb_hits >= 5) return MG_WIN;
        }
    }

    if(pb_vy > 0 && y >= PB_FY - 12 && y <= PB_FY + 6)
    {
        if(x >= 70 && x < 150)
        {
            if(pb_lf) { pb_vy = -36; pb_vx = 6 + (x - 70) / 6; Buzzer_PlaySfx(&sfx_blip); }
            else      { pb_vy = -Max16(pb_vy / 3, 4); pb_vx = 8; }
        }
        else if(x >= 170 && x < 250)
        {
            if(pb_rf) { pb_vy = -36; pb_vx = -(6 + (250 - x) / 6); Buzzer_PlaySfx(&sfx_blip); }
            else      { pb_vy = -Max16(pb_vy / 3, 4); pb_vx = -8; }
        }
    }
    if(y >= 200 && x < 70)  { pb_y4 = 800; pb_vy = -Max16(Abs16(pb_vy) / 2, 4); pb_vx =  10; }
    if(y >= 200 && x > 250) { pb_y4 = 800; pb_vy = -Max16(Abs16(pb_vy) / 2, 4); pb_vx = -10; }

    if(y >= 224) return MG_LOSE;
    return MG_PLAY;
}

static void PbRender(void)
{
    int16_t i, x = pb_x4 / 4, y = pb_y4 / 4;

    if(s_first)
    {
        Box(14, 0, 6, FIELD_H, COLOR_GRAY);
        Box(300, 0, 6, FIELD_H, COLOR_GRAY);
    }

    Box(pb_px - 6, pb_py - 6, 12, 12, PB_BG);

    for(i = 0; i < 3; i++)
    {
        Disc(kPbBx[i], kPbBy[i], 14, pb_flash[i] ? COLOR_WHITE : COLOR_RED);
        Box(kPbBx[i] - 14, kPbBy[i] - 1, 29, 3, COLOR_BLACK);
        Box(kPbBx[i] - 4, kPbBy[i] - 4, 9, 9, COLOR_WHITE);
    }

    Box(70, PB_FY - 16, 80, 30, PB_BG);
    Box(170, PB_FY - 16, 80, 30, PB_BG);
    Box(70,  pb_lf ? PB_FY - 8 : PB_FY + 2, 80, 6, COLOR_YELLOW);
    Box(170, pb_rf ? PB_FY - 8 : PB_FY + 2, 80, 6, COLOR_YELLOW);

    Disc(x, y, 5, COLOR_WHITE);
    Box(x - 5, y - 1, 11, 2, COLOR_RED);
    pb_px = x; pb_py = y;
}

// ============================================================================
// RUN!  (Sonic)  auto-run: any button jumps. Grab 5 rings, dodge the spikes.
// ============================================================================
#define SO_GY   200
#define SO_X    60
#define SO_BG   0x3A7BD5UL

static int16_t so_y, so_vy, so_py, so_v, so_cd, so_rings;
static int16_t so_x[3], so_px[3], so_top[3], so_t[3];
static bool    so_on[3], so_dr[3];

static void SoInit(uint16_t spd)
{
    uint16_t i;

    so_y = so_py = SO_GY - 18;
    so_vy = 0;
    so_v  = 6 + Min16((int16_t)spd / 2, 3);
    so_cd = 10;
    so_rings = 0;
    for(i = 0; i < 3; i++) { so_on[i] = false; so_dr[i] = false; }
}

static int16_t SoUpdate(void)
{
    uint16_t i;
    int16_t  r;

    if(so_y >= SO_GY - 18 && AnyPress()) { so_vy = -12; Buzzer_PlaySfx(&sfx_jump); }
    so_vy++;
    so_y += so_vy;
    if(so_y >= SO_GY - 18) { so_y = SO_GY - 18; so_vy = 0; }

    if(--so_cd <= 0)
    {
        for(i = 0; i < 3; i++)
            if(!so_on[i])
            {
                r = Rnd(6);
                so_on[i] = true;
                so_t[i]  = (r < 2) ? 0 : ((r < 4) ? 1 : 2);
                so_top[i] = (so_t[i] == 0) ? 178 : ((so_t[i] == 1) ? 120 : SO_GY - 14);
                so_x[i] = so_px[i] = FIELD_W;
                break;
            }
        so_cd = 14 + Rnd(12);
    }

    for(i = 0; i < 3; i++)
    {
        if(!so_on[i]) continue;
        so_x[i] -= so_v;
        if(so_x[i] < -16) { so_on[i] = false; continue; }

        if(so_t[i] == 2)
        {
            if(Hit(SO_X + 2, so_y + 2, 14, 16, so_x[i] + 2, so_top[i], 10, 14)) return MG_LOSE;
        }
        else if(Hit(SO_X, so_y, 18, 18, so_x[i], so_top[i], 14, 14))
        {
            so_on[i] = false;
            Buzzer_PlaySfx(&sfx_coin);
            if(++so_rings >= 5) return MG_WIN;
        }
    }
    return MG_PLAY;
}

static void SoRender(void)
{
    uint16_t i;

    if(s_first)
    {
        Box(0, SO_GY, FIELD_W, FIELD_H - SO_GY, 0xB5651DUL);
        Box(0, SO_GY, FIELD_W, 4, COLOR_GREEN);
    }

    for(i = 0; i < 3; i++)
    {
        if(so_dr[i]) Box(so_px[i], so_top[i], 14, 14, SO_BG);
        if(so_on[i])
        {
            if(so_t[i] == 2)
            {
                Box(so_x[i] + 5, so_top[i],      4, 5, COLOR_SILVER);
                Box(so_x[i] + 3, so_top[i] + 5,  8, 5, COLOR_SILVER);
                Box(so_x[i],     so_top[i] + 10, 14, 4, COLOR_SILVER);
            }
            else
            {
                Disc(so_x[i] + 7, so_top[i] + 7, 6, COLOR_GOLD);
                Box(so_x[i] + 5, so_top[i] + 5, 4, 4, SO_BG);
            }
            so_px[i] = so_x[i];
            so_dr[i] = true;
        }
        else so_dr[i] = false;
    }

    Box(SO_X - 5, so_py - 1, 25, 21, SO_BG);
    if(so_y < SO_GY - 18)                                       // spin ball
    {
        Disc(SO_X + 9, so_y + 9, 9, COLOR_BLUE);
        Box(SO_X + 5, so_y + 7, 9, 4, COLOR_WHITE);
    }
    else
    {
        Box(SO_X, so_y, 16, 14, COLOR_BLUE);
        Box(SO_X - 4, so_y + 3, 6, 5, COLOR_BLUE);              // quills
        Box(SO_X + 9, so_y + 3, 5, 5, COLOR_WHITE);             // eye
        Box(SO_X + 2, so_y + 14, 14, 4, COLOR_RED);             // shoes
    }
    so_py = so_y;
}

// ============================================================================
// BLAST!  (Metroid)  UP/DOWN switch lanes, button (or L/R) fires.
//         Freeze 3 Metroids (2 hits each) before they reach Samus.
// ============================================================================
#define MT_BG   0x14001EUL

static const int16_t kMtL[3] = { 70, 130, 190 };

static int16_t mt_lane, mt_dlane, mt_kills, mt_cd, mt_v;
static int16_t mt_ex[2], mt_el[2], mt_hp[2], mt_pex[2], mt_fl[2];
static int16_t mt_bx[2], mt_bl[2], mt_pbx[2];
static bool    mt_eon[2], mt_edr[2], mt_bon[2], mt_bdr[2];

static void MtInit(uint16_t spd)
{
    uint16_t i;

    mt_lane = 1; mt_dlane = -1;
    mt_kills = 0;
    mt_cd = 20;
    mt_v = 1 + (spd >= 3 ? 1 : 0);
    for(i = 0; i < 2; i++) { mt_eon[i] = mt_edr[i] = mt_bon[i] = mt_bdr[i] = false; mt_fl[i] = 0; }
}

static int16_t MtUpdate(void)
{
    uint16_t i, j;

    if(EdgeU() && mt_lane > 0) mt_lane--;
    if(EdgeD() && mt_lane < 2) mt_lane++;

    if(g_joy.btnPressed || g_dpad.lPressed || g_dpad.rPressed)
        for(i = 0; i < 2; i++)
            if(!mt_bon[i])
            {
                mt_bon[i] = true; mt_bx[i] = mt_pbx[i] = 44; mt_bl[i] = mt_lane;
                Buzzer_PlaySfx(&sfx_laser);
                break;
            }

    if(--mt_cd <= 0)
    {
        for(i = 0; i < 2; i++)
            if(!mt_eon[i])
            {
                mt_eon[i] = true; mt_ex[i] = mt_pex[i] = FIELD_W; mt_el[i] = Rnd(3);
                mt_hp[i] = 2; mt_fl[i] = 0;
                break;
            }
        mt_cd = 50 + Rnd(30);
    }

    for(i = 0; i < 2; i++)
    {
        if(!mt_eon[i]) continue;
        mt_ex[i] -= mt_v;
        if(mt_fl[i] > 0) mt_fl[i]--;
        if(mt_ex[i] < 50) return MG_LOSE;
    }

    for(j = 0; j < 2; j++)
    {
        if(!mt_bon[j]) continue;
        mt_bx[j] += 8;
        if(mt_bx[j] > FIELD_W) { mt_bon[j] = false; continue; }
        for(i = 0; i < 2; i++)
            if(mt_eon[i] && mt_el[i] == mt_bl[j] && mt_bx[j] + 6 >= mt_ex[i] && mt_bx[j] <= mt_ex[i] + 28)
            {
                mt_bon[j] = false;
                mt_fl[i] = 4;
                if(--mt_hp[i] <= 0)
                {
                    mt_eon[i] = false;
                    Buzzer_PlaySfx(&sfx_explosion);
                    if(++mt_kills >= 3) return MG_WIN;
                }
                break;
            }
    }
    return MG_PLAY;
}

static void MtRender(void)
{
    uint16_t i;
    int16_t  cy;

    for(i = 0; i < 2; i++)                                      // erase old
    {
        if(mt_edr[i]) { Box(mt_pex[i] - 1, kMtL[mt_el[i]] - 18, 34, 36, MT_BG); mt_edr[i] = false; }
        if(mt_bdr[i]) { Box(mt_pbx[i], kMtL[mt_bl[i]] - 2, 8, 4, MT_BG); mt_bdr[i] = false; }
    }
    if(s_first || mt_lane != mt_dlane)
    {
        if(mt_dlane >= 0) Box(8, kMtL[mt_dlane] - 20, 36, 40, MT_BG);
        Box(10, kMtL[mt_lane] - 18, 20, 36, COLOR_ORANGE);      // suit
        Box(20, kMtL[mt_lane] - 14, 8, 8, COLOR_LIME);          // visor
        Box(28, kMtL[mt_lane] - 2, 14, 6, COLOR_GOLD);          // arm cannon
        mt_dlane = mt_lane;
    }
    for(i = 0; i < 2; i++)                                      // draw new
    {
        if(mt_eon[i])
        {
            cy = kMtL[mt_el[i]];
            Disc(mt_ex[i] + 14, cy - 4, 12, mt_fl[i] ? COLOR_WHITE : COLOR_GREEN);
            Box(mt_ex[i] + 10, cy - 6, 8, 8, COLOR_RED);
            Box(mt_ex[i] + 2,  cy + 8, 4, 8, COLOR_GREEN);
            Box(mt_ex[i] + 12, cy + 8, 4, 10, COLOR_GREEN);
            Box(mt_ex[i] + 22, cy + 8, 4, 8, COLOR_GREEN);
            mt_pex[i] = mt_ex[i]; mt_edr[i] = true;
        }
        if(mt_bon[i])
        {
            Box(mt_bx[i], kMtL[mt_bl[i]] - 2, 6, 4, COLOR_CYAN);
            mt_pbx[i] = mt_bx[i]; mt_bdr[i] = true;
        }
    }
}

// ============================================================================
// RACE!  (Hydro Thunder)  steer the boat with the stick (it drifts!).
//        Dodge rocks and buoys down the river.  Survive.
// ============================================================================
#define HY_BG   0x1976D2UL
#define HY_Y    170

static int16_t hy_x8, hy_vx, hy_px, hy_v, hy_cd;
static int16_t hy_ox[4], hy_oy[4], hy_opy[4], hy_ot[4];
static bool    hy_on[4], hy_dr[4];

static int16_t HyW(int16_t t) { return t ? 14 : 24; }
static int16_t HyH(int16_t t) { return t ? 20 : 24; }

static void HyInit(uint16_t spd)
{
    uint16_t i;

    hy_x8 = 151 * 8;  hy_vx = 0;  hy_px = 151;
    hy_v  = 5 + Min16((int16_t)spd / 2, 3);
    hy_cd = 10;
    for(i = 0; i < 4; i++) { hy_on[i] = false; hy_dr[i] = false; }
}

static int16_t HyUpdate(void)
{
    uint16_t i;
    int16_t  st = g_joy.dx, x;

    if(g_dpad.l) st = -100;
    if(g_dpad.r) st =  100;
    hy_vx += st / 12;
    hy_vx -= hy_vx / 8;
    hy_vx  = Clamp16(hy_vx, -40, 40);
    hy_x8 += hy_vx;
    if(hy_x8 < 40 * 8)          { hy_x8 = 40 * 8;          hy_vx = 0; }
    if(hy_x8 > (280 - 18) * 8)  { hy_x8 = (280 - 18) * 8;  hy_vx = 0; }
    x = hy_x8 / 8;

    if(--hy_cd <= 0)
    {
        for(i = 0; i < 4; i++)
            if(!hy_on[i])
            {
                hy_on[i] = true;
                hy_ot[i] = Rnd(2);
                hy_ox[i] = 42 + Rnd(236 - HyW(hy_ot[i]));
                hy_oy[i] = hy_opy[i] = -HyH(hy_ot[i]);
                break;
            }
        hy_cd = Max16(9, 22 + Rnd(10) - hy_v);
    }

    for(i = 0; i < 4; i++)
    {
        if(!hy_on[i]) continue;
        hy_oy[i] += hy_v;
        if(hy_oy[i] >= FIELD_H) { hy_on[i] = false; continue; }
        if(Hit(x + 1, HY_Y + 2, 16, 28, hy_ox[i], hy_oy[i], HyW(hy_ot[i]), HyH(hy_ot[i])))
            return MG_LOSE;
    }
    return MG_PLAY;
}

static void HyRender(void)
{
    uint16_t i;
    int16_t  x = hy_x8 / 8;

    if(s_first)
    {
        Box(0, 0, 36, FIELD_H, COLOR_GREEN);
        Box(284, 0, 36, FIELD_H, COLOR_GREEN);
        Box(36, 0, 2, FIELD_H, COLOR_WHITE);
        Box(282, 0, 2, FIELD_H, COLOR_WHITE);
    }

    for(i = 0; i < 4; i++)
    {
        if(hy_on[i])
        {
            SlideY(hy_ox[i], HyW(hy_ot[i]), hy_opy[i], hy_oy[i], HyH(hy_ot[i]),
                   hy_ot[i] ? COLOR_RED : COLOR_GRAY, HY_BG);
            hy_opy[i] = hy_oy[i];
            hy_dr[i]  = true;
        }
        else if(hy_dr[i])
        {
            Box(hy_ox[i], hy_opy[i], HyW(hy_ot[i]), HyH(hy_ot[i]), HY_BG);
            hy_dr[i] = false;
        }
    }

    Box(hy_px, HY_Y, 18, 30, HY_BG);
    Box(x + 4, HY_Y, 10, 6, COLOR_RED);                         // bow
    Box(x, HY_Y + 6, 18, 24, COLOR_WHITE);                      // hull
    Box(x + 5, HY_Y + 11, 8, 8, COLOR_BLUE);                    // cockpit
    hy_px = x;
}

// ============================================================================
// TURN!  (Temple Run, isometric)  run the winding path; press L or R as you
//        reach each corner (the white arrow tells you which). Miss it and you fall.
// ============================================================================
#define TP_SEGS 5
#define TP_N    (TP_SEGS * 3 + 1)
#define TP_BG   0x0F2F1AUL

static const int16_t kHx[4] = { 1, 0, -1,  0 };                 // grid headings: +x +y -x -y
static const int16_t kHy[4] = { 0, 1,  0, -1 };                 // = SE SW NW NE on screen

static int16_t tp_gx[TP_N], tp_gy[TP_N], tp_hd[TP_SEGS], tp_cd[TP_SEGS];
static int16_t tp_p, tp_tf, tp_ci, tp_rx, tp_ry, tp_prx, tp_pry, tp_pi;

static int16_t TpSx(int16_t i) { return 160 + (tp_gx[i] - tp_gy[i]) * 16; }
static int16_t TpSy(int16_t i) { return 24 + (tp_gx[i] + tp_gy[i]) * 8; }

static bool TpBuild(bool fixed)
{
    int16_t seg, step, j, idx, h, gx = 11, gy = 11, s, d;

    h = 2 + Rnd(2);
    tp_gx[0] = gx; tp_gy[0] = gy;
    for(seg = 0; seg < TP_SEGS; seg++)
    {
        if(fixed)         h = (seg & 1) ? 2 : 3;
        else if(seg > 0)  h = (h + (Rnd(2) ? 1 : 3)) & 3;
        tp_hd[seg] = h;
        for(step = 0; step < 3; step++)
        {
            gx += kHx[h];  gy += kHy[h];
            idx = seg * 3 + step + 1;
            s = gx + gy;  d = gx - gy;
            if(s < 1 || s > 22 || d < -8 || d > 8) return false;
            for(j = 0; j < idx; j++)
                if(tp_gx[j] == gx && tp_gy[j] == gy) return false;
            tp_gx[idx] = gx; tp_gy[idx] = gy;
        }
    }
    return true;
}

static void TpInit(uint16_t spd)
{
    int16_t tries, k, ax, ay, bx, by, a, b;

    for(tries = 0; tries < 80 && !TpBuild(false); tries++) { }
    if(tries >= 80) TpBuild(true);

    for(k = 0; k < TP_SEGS - 1; k++)                            // turn side at each corner
    {
        a = tp_hd[k];  b = tp_hd[k + 1];
        ax = (kHx[a] - kHy[a]) * 16;  ay = (kHx[a] + kHy[a]) * 8;
        bx = (kHx[b] - kHy[b]) * 16;  by = (kHx[b] + kHy[b]) * 8;
        tp_cd[k] = ((ax * by - ay * bx) > 0) ? 1 : -1;          // +1 = right, -1 = left
    }
    tp_tf = 15 - Min16((int16_t)spd, 4);
    tp_p = 0; tp_ci = 0; tp_pi = 0;
    tp_rx = tp_prx = TpSx(0);
    tp_ry = tp_pry = TpSy(0);
}

static int16_t TpUpdate(void)
{
    int16_t i, f, tc, want;

    tp_p++;
    if(tp_ci < TP_SEGS - 1)
    {
        tc = (tp_ci + 1) * 3 * tp_tf;
        if(tp_p > tc + 4) return MG_LOSE;                       // missed the turn: fall
        if(tp_p >= tc - 12 && (EdgeL() || EdgeR()))
        {
            want = EdgeL() ? -1 : 1;
            if(want != tp_cd[tp_ci]) return MG_LOSE;
            tp_ci++;
            Buzzer_PlaySfx(&sfx_blip);
        }
    }
    if(tp_p >= (TP_N - 1) * tp_tf) return MG_WIN;

    i = tp_p / tp_tf;  f = tp_p % tp_tf;
    tp_pi = i;
    tp_rx = TpSx(i) + ((TpSx(i + 1) - TpSx(i)) * f) / tp_tf;
    tp_ry = TpSy(i) + ((TpSy(i + 1) - TpSy(i)) * f) / tp_tf;
    return MG_PLAY;
}

static void TpTile(int16_t i)
{
    int16_t  cx = TpSx(i), cy = TpSy(i);
    uint32_t c = (i % 3 == 0 && i > 0 && i < TP_N - 1) ? 0xE0B040UL : ((i & 1) ? 0xC9A66BUL : 0xB08D57UL);

    Box(cx - 6,  cy - 8, 12, 4, c);
    Box(cx - 14, cy - 4, 28, 4, c);
    Box(cx - 14, cy,     28, 4, c);
    Box(cx - 6,  cy + 4, 12, 4, c);
    if(i % 3 == 0 && i > 0 && i < TP_N - 1)
        Arrow(cx, cy, (tp_cd[i / 3 - 1] < 0) ? 2 : 3, 2, COLOR_WHITE);
}

static void TpRender(void)
{
    int16_t i;

    if(s_first)
    {
        for(i = 0; i < TP_N; i++) TpTile(i);
        tp_prx = tp_rx; tp_pry = tp_ry;
    }
    else
    {
        Box(tp_prx - 5, tp_pry - 22, 10, 24, TP_BG);
        for(i = Max16(tp_pi - 1, 0); i <= Min16(tp_pi + 2, TP_N - 1); i++) TpTile(i);
    }
    Box(tp_rx - 4, tp_ry - 14, 8, 12, COLOR_ORANGE);
    Box(tp_rx - 3, tp_ry - 20, 6, 6, 0xFFCC99UL);
    tp_prx = tp_rx; tp_pry = tp_ry;
}

// ============================================================================
// CALL IT!  (Helldivers 2)  enter the stratagem code on the D-pad. A wrong
//           direction resets the code.
// ============================================================================
#define HD_BG   0x0B1220UL

static int16_t hd_seq[7], hd_n, hd_k, hd_shown;

static void HdInit(uint16_t spd)
{
    int16_t i;

    hd_n = 4 + Min16((int16_t)spd / 2, 2);
    for(i = 0; i < hd_n; i++) hd_seq[i] = Rnd(4);
    hd_k = 0;
    hd_shown = -1;
}

static int16_t HdUpdate(void)
{
    int16_t p = -1;

    if(EdgeU())      p = 0;
    else if(EdgeD()) p = 1;
    else if(EdgeL()) p = 2;
    else if(EdgeR()) p = 3;
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

static void HdRender(void)
{
    int16_t i, x;

    if(s_first)
    {
        Text("STRATAGEM", 160, 40, COLOR_GOLD);
        Text("ENTER THE CODE", 160, 180, COLOR_GRAY);
        hd_shown = -1;
    }
    if(hd_shown != hd_k)
    {
        for(i = 0; i < hd_n; i++)
        {
            x = 160 - (hd_n - 1) * 22 + i * 44;
            Box(x - 18, 92, 36, 36, HD_BG);
            Arrow(x, 110, hd_seq[i], 4, i < hd_k ? COLOR_GOLD : COLOR_WHITE);
        }
        hd_shown = hd_k;
    }
}

// ============================================================================
// BREAK!  (Brick Breaker)  stick/D-pad moves the paddle. Smash 5 bricks.
// ============================================================================
#define BK_BG   0x101828UL

static int16_t bk_px, bk_ppx, bk_x, bk_y, bk_vx, bk_vy, bk_pbx, bk_pby, bk_cnt;
static bool    bk_a[24], bk_dr[24];

static void BkBrick(int16_t i)
{
    static const uint32_t col[3] = { COLOR_RED, COLOR_ORANGE, COLOR_YELLOW };
    Box(16 + (i % 8) * 36, 30 + (i / 8) * 14, 34, 12, col[i / 8]);
}

static void BkInit(uint16_t spd)
{
    int16_t i;

    for(i = 0; i < 24; i++) { bk_a[i] = true; bk_dr[i] = false; }
    bk_px = bk_ppx = 136;
    bk_x = bk_pbx = 156;  bk_y = bk_pby = 150;
    bk_vx = Rnd(2) ? 3 : -3;
    bk_vy = -(5 + Min16((int16_t)spd / 2, 2));
    bk_cnt = 0;
}

static int16_t BkUpdate(void)
{
    int16_t i, bx, by, oy = bk_y;

    bk_px = Clamp16(bk_px + (g_dpad.l ? -8 : (g_dpad.r ? 8 : (g_joy.dx * 8) / 100)), 0, 272);

    bk_x += bk_vx;  bk_y += bk_vy;
    if(bk_x < 0)   { bk_x = 0;   bk_vx =  Abs16(bk_vx); }
    if(bk_x > 312) { bk_x = 312; bk_vx = -Abs16(bk_vx); }
    if(bk_y < 0)   { bk_y = 0;   bk_vy =  Abs16(bk_vy); }

    if(bk_vy > 0 && bk_y + 8 >= 210 && bk_y + 8 <= 222 && bk_x + 8 > bk_px && bk_x < bk_px + 48)
    {
        bk_y = 202;
        bk_vy = -Abs16(bk_vy);
        bk_vx = Clamp16(bk_vx + ((bk_x + 4) - (bk_px + 24)) / 8, -6, 6);
        if(bk_vx == 0) bk_vx = 1;
        Buzzer_PlaySfx(&sfx_blip);
    }

    for(i = 0; i < 24; i++)
    {
        if(!bk_a[i]) continue;
        bx = 16 + (i % 8) * 36;  by = 30 + (i / 8) * 14;
        if(Hit(bk_x, bk_y, 8, 8, bx, by, 34, 12))
        {
            bk_a[i] = false;
            if(oy + 8 <= by || oy >= by + 12) bk_vy = -bk_vy; else bk_vx = -bk_vx;
            Buzzer_PlaySfx(&sfx_coin);
            if(++bk_cnt >= 5) return MG_WIN;
            break;
        }
    }
    if(bk_y >= FIELD_H) return MG_LOSE;
    return MG_PLAY;
}

static void BkRender(void)
{
    int16_t i;

    for(i = 0; i < 24; i++)
    {
        if(bk_a[i] && (s_first || !bk_dr[i])) { BkBrick(i); bk_dr[i] = true; }
        else if(!bk_a[i] && bk_dr[i])
        {
            Box(16 + (i % 8) * 36, 30 + (i / 8) * 14, 34, 12, BK_BG);
            bk_dr[i] = false;
        }
    }

    Box(bk_pbx, bk_pby, 8, 8, BK_BG);                           // erase ball, repair bricks it touched
    for(i = 0; i < 24; i++)
        if(bk_a[i] && Hit(bk_pbx, bk_pby, 8, 8, 16 + (i % 8) * 36, 30 + (i / 8) * 14, 34, 12)) BkBrick(i);

    if(s_first || bk_px != bk_ppx)
    {
        Box(bk_ppx, 210, 48, 8, BK_BG);
        Box(bk_px, 210, 48, 8, COLOR_CYAN);
        bk_ppx = bk_px;
    }
    Box(bk_x, bk_y, 8, 8, COLOR_WHITE);
    bk_pbx = bk_x; bk_pby = bk_y;
}

// ============================================================================
// STOMP!  (Super Mario)  L/R run, button or UP jumps. Stomp 2 Goombas by
//         landing on their heads.
// ============================================================================
#define MR_GY   204
#define MR_BG   0x5C94FCUL

static int16_t mr_x, mr_y, mr_vy, mr_px, mr_py, mr_stomp, mr_sp;
static int16_t mr_gx[2], mr_gd[2], mr_pgx[2];
static bool    mr_ga[2], mr_dr[2];

static void MrInit(uint16_t spd)
{
    mr_x = mr_px = 24;
    mr_y = mr_py = MR_GY - 18;
    mr_vy = 0;
    mr_stomp = 0;
    mr_sp = 1 + Min16((int16_t)spd / 4, 1);
    mr_gx[0] = mr_pgx[0] = 250;  mr_gd[0] = -1;  mr_ga[0] = true;
    mr_gx[1] = mr_pgx[1] = 140;  mr_gd[1] = -1;  mr_ga[1] = true;
    mr_dr[0] = mr_dr[1] = false;
}

static int16_t MrUpdate(void)
{
    uint16_t i;

    mr_x = Clamp16(mr_x + HeldX() * 3, 0, 304);
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

        if(Hit(mr_x + 1, mr_y, 12, 18, mr_gx[i], MR_GY - 14, 16, 14))
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

static void MrRender(void)
{
    uint16_t i;

    if(s_first)
    {
        Box(0, MR_GY, FIELD_W, FIELD_H - MR_GY, 0xC84C0CUL);
        Box(0, MR_GY, FIELD_W, 3, 0xFFB070UL);
        Box(40, 40, 50, 14, COLOR_WHITE);   Box(50, 30, 30, 10, COLOR_WHITE);
        Box(230, 60, 50, 14, COLOR_WHITE);  Box(240, 50, 30, 10, COLOR_WHITE);
        Box(150, 80, 20, 20, COLOR_GOLD);   Box(155, 85, 10, 10, 0xB8860BUL);
    }

    Box(mr_px, mr_py, 14, 18, MR_BG);
    for(i = 0; i < 2; i++)
    {
        if(mr_dr[i]) Box(mr_pgx[i], MR_GY - 14, 16, 14, MR_BG);
        if(mr_ga[i])
        {
            Box(mr_gx[i] + 1, MR_GY - 14, 14, 8, COLOR_BROWN);
            Box(mr_gx[i] + 3, MR_GY - 12, 3, 3, COLOR_WHITE);
            Box(mr_gx[i] + 10, MR_GY - 12, 3, 3, COLOR_WHITE);
            Box(mr_gx[i] + 2, MR_GY - 6, 12, 6, 0x222222UL);
            mr_pgx[i] = mr_gx[i];
            mr_dr[i]  = true;
        }
        else mr_dr[i] = false;
    }
    Box(mr_x + 1, mr_y, 12, 5, COLOR_RED);                      // cap
    Box(mr_x + 2, mr_y + 5, 10, 5, 0xFFCC99UL);                 // face
    Box(mr_x + 1, mr_y + 10, 12, 5, COLOR_BLUE);                // overalls
    Box(mr_x, mr_y + 15, 14, 3, COLOR_BROWN);                   // boots
    mr_px = mr_x; mr_py = mr_y;
}

// ============================================================================
// DASH!  (Geometry Dash)  any button jumps. Dodge spikes, land on blocks.
//        Survive.
// ============================================================================
#define GD_GY   206
#define GD_X    70
#define GD_BG   0x1A0B3BUL

static int16_t gd_y, gd_vy, gd_py, gd_v, gd_cd;
static int16_t gd_ox[3], gd_opx[3], gd_ot[3];                   // 0 spike, 1 double spike, 2 block
static bool    gd_on[3], gd_dr[3], gd_gnd;

static int16_t GdW(int16_t t) { return (t == 0) ? 20 : ((t == 1) ? 40 : 30); }
static int16_t GdH(int16_t t) { return (t == 2) ? 24 : 20; }

static void GdSpike(int16_t x)
{
    Box(x + 8, GD_GY - 20, 4, 5, COLOR_RED);
    Box(x + 5, GD_GY - 15, 10, 5, COLOR_RED);
    Box(x + 2, GD_GY - 10, 16, 5, COLOR_RED);
    Box(x,     GD_GY - 5,  20, 5, COLOR_RED);
}

static void GdInit(uint16_t spd)
{
    uint16_t i;

    gd_y = gd_py = GD_GY - 20;
    gd_vy = 0;
    gd_gnd = true;
    gd_v  = 6 + Min16((int16_t)spd / 2, 2);
    gd_cd = 25;
    for(i = 0; i < 3; i++) { gd_on[i] = false; gd_dr[i] = false; }
}

static int16_t GdUpdate(void)
{
    uint16_t i;
    int16_t  prevB, bottom, sup, top;

    if(gd_gnd && (AnyPress() || g_joy.btn || g_dpad.u)) { gd_vy = -11; Buzzer_PlaySfx(&sfx_jump); }

    if(--gd_cd <= 0)
    {
        for(i = 0; i < 3; i++)
            if(!gd_on[i]) { gd_on[i] = true; gd_ot[i] = Rnd(3); gd_ox[i] = gd_opx[i] = FIELD_W; break; }
        gd_cd = (130 + Rnd(90)) / gd_v;
    }
    for(i = 0; i < 3; i++)
        if(gd_on[i])
        {
            gd_ox[i] -= gd_v;
            if(gd_ox[i] < -45) gd_on[i] = false;
        }

    prevB = gd_y + 20;
    gd_vy = Min16(gd_vy + 1, 12);
    gd_y += gd_vy;
    bottom = gd_y + 20;
    sup = GD_GY;

    for(i = 0; i < 3; i++)
    {
        if(!gd_on[i]) continue;
        if(gd_ot[i] == 2)
        {
            if(gd_ox[i] < GD_X + 18 && GD_X + 2 < gd_ox[i] + 30)
            {
                top = GD_GY - 24;
                if(prevB <= top && bottom >= top) sup = top;
                else if(bottom > top + 2 && prevB > top) return MG_LOSE;     // hit the side
            }
        }
        else if(Hit(GD_X + 3, gd_y + 4, 14, 14, gd_ox[i] + 4, GD_GY - 20 + 4, GdW(gd_ot[i]) - 8, 16))
            return MG_LOSE;
    }

    if(bottom >= sup) { gd_y = sup - 20; gd_vy = 0; gd_gnd = true; }
    else gd_gnd = false;
    return MG_PLAY;
}

static void GdRender(void)
{
    uint16_t i;

    if(s_first)
    {
        Box(0, GD_GY, FIELD_W, FIELD_H - GD_GY, 0x6A1B9AUL);
        Box(0, GD_GY, FIELD_W, 3, 0xE040FBUL);
    }

    Box(GD_X, gd_py, 20, 20, GD_BG);
    for(i = 0; i < 3; i++)
    {
        if(gd_dr[i]) Box(gd_opx[i], GD_GY - GdH(gd_ot[i]), GdW(gd_ot[i]), GdH(gd_ot[i]), GD_BG);
        if(gd_on[i])
        {
            if(gd_ot[i] == 2)
            {
                Box(gd_ox[i], GD_GY - 24, 30, 24, COLOR_CYAN);
                Box(gd_ox[i] + 3, GD_GY - 21, 24, 18, 0x003366UL);
            }
            else
            {
                GdSpike(gd_ox[i]);
                if(gd_ot[i] == 1) GdSpike(gd_ox[i] + 20);
            }
            gd_opx[i] = gd_ox[i];
            gd_dr[i]  = true;
        }
        else gd_dr[i] = false;
    }

    if(gd_gnd)
    {
        Box(GD_X, gd_y, 20, 20, COLOR_YELLOW);
        Box(GD_X + 4, gd_y + 5, 4, 4, COLOR_BLACK);
        Box(GD_X + 12, gd_y + 5, 4, 4, COLOR_BLACK);
        Box(GD_X + 4, gd_y + 13, 12, 3, COLOR_BLACK);
    }
    else                                                        // spinning cube
    {
        Box(GD_X + 5, gd_y, 10, 20, COLOR_YELLOW);
        Box(GD_X, gd_y + 5, 20, 10, COLOR_YELLOW);
    }
    gd_py = gd_y;
}

// ============================================================================
// BEAT!  (Melatonin)  rhythm: press any button when each note hits the ring,
//        on the click. Hit 5 notes; 3 misses and you wake up.
// ============================================================================
#define ML_BG   0x2B1B4DUL
#define ML_Z    70
#define ML_N    7

static const int16_t kMlT[ML_N] = { 10, 38, 59, 94, 115, 150, 178 };

static int16_t ml_res[ML_N], ml_px[ML_N], ml_hits, ml_miss, ml_flash, ml_v;

static int16_t MlX(int16_t i)
{
    if(s_frame < kMlT[i]) return 340;
    return 330 - (int16_t)(((int32_t)(s_frame - kMlT[i])) * ml_v);
}

static void MlInit(uint16_t spd)
{
    int16_t i;

    ml_v = 5 + Min16((int16_t)spd / 3, 2);
    ml_hits = ml_miss = ml_flash = 0;
    for(i = 0; i < ML_N; i++) { ml_res[i] = 0; ml_px[i] = 340; }
}

static int16_t MlUpdate(void)
{
    int16_t i, x, best = -1, bd = 99;

    if(ml_flash > 0) ml_flash--;

    for(i = 0; i < ML_N; i++)
    {
        x = MlX(i);
        if(ml_res[i] == 0 && x < ML_Z - 18)
        {
            ml_res[i] = 2;
            Buzzer_PlaySfx(&sfx_thud);
            if(++ml_miss >= 3) return MG_LOSE;
        }
        if(x <= ML_Z && x + ml_v > ML_Z) Buzzer_PlaySfx(&sfx_blip);     // the click
    }

    if(AnyPress())
    {
        for(i = 0; i < ML_N; i++)
        {
            x = MlX(i);
            if(ml_res[i] == 0 && Abs16(x - ML_Z) <= 18 && Abs16(x - ML_Z) < bd)
            { bd = Abs16(x - ML_Z); best = i; }
        }
        if(best >= 0)
        {
            ml_res[best] = 1;
            ml_flash = 6;
            Buzzer_PlaySfx(&sfx_coin);
            if(++ml_hits >= 5) return MG_WIN;
        }
    }
    return MG_PLAY;
}

static void MlRender(void)
{
    int16_t  i, x;
    uint32_t zc = ml_flash ? COLOR_LIME : COLOR_WHITE;

    if(s_first) Text("TAP ON THE BEAT", 160, 200, COLOR_GRAY);

    for(i = 0; i < ML_N; i++)
        if(ml_px[i] > -20 && ml_px[i] < 340) Box(ml_px[i] - 12, 98, 24, 36, ML_BG);

    Box(0, 112, FIELD_W, 6, COLOR_GRAY);
    Box(ML_Z - 20, 94, 40, 3, zc);   Box(ML_Z - 20, 133, 40, 3, zc);
    Box(ML_Z - 20, 94, 3, 42, zc);   Box(ML_Z + 17, 94, 3, 42, zc);

    for(i = 0; i < ML_N; i++)
    {
        x = MlX(i);
        if(ml_res[i] == 0 && x < 330) Disc(x, 115, 10, COLOR_PINK);
        ml_px[i] = x;
    }
}

// ============================================================================
// SORT!  (Machine Party recycler)  L = PAPER, UP = CAN, R = GLASS.
//        Sort 5 items. Wrong bin and the machine jams.
// ============================================================================
#define RC_BG   0x203038UL

static int16_t rc_t, rc_n, rc_shown;

static void RcInit(uint16_t spd)
{
    rc_t = Rnd(3);
    rc_n = 0;
    rc_shown = -1;
}

static int16_t RcUpdate(void)
{
    int16_t p = -1;

    if(EdgeL())      p = 0;
    else if(EdgeU()) p = 1;
    else if(EdgeR()) p = 2;
    if(p < 0) return MG_PLAY;

    if(p != rc_t) { Buzzer_PlaySfx(&sfx_hit); return MG_LOSE; }
    Buzzer_PlaySfx(&sfx_coin);
    if(++rc_n >= 5) return MG_WIN;
    rc_t = Rnd(3);
    return MG_PLAY;
}

static void RcRender(void)
{
    int16_t i;

    if(s_first)
    {
        Box(20, 160, 80, 56, COLOR_BLUE);
        Box(120, 160, 80, 56, COLOR_SILVER);
        Box(220, 160, 80, 56, COLOR_GREEN);
        Text("< PAPER", 60, 188, COLOR_WHITE);
        Text("^ CAN", 160, 188, COLOR_BLACK);
        Text("GLASS >", 260, 188, COLOR_WHITE);
        Box(60, 134, 200, 8, COLOR_GRAY);                       // conveyor belt
        rc_shown = -1;
    }
    if(rc_shown != rc_n)
    {
        Box(100, 40, 120, 90, RC_BG);
        for(i = 0; i < 5; i++)
            Box(100 + i * 24, 12, 16, 16, i < rc_n ? COLOR_LIME : COLOR_DARK_GRAY);
        rc_shown = rc_n;
    }
    if(rc_shown >= 0)
    {
        Box(130, 56, 60, 74, RC_BG);
        if(rc_t == 0)
        {
            Box(148, 76, 24, 32, COLOR_WHITE);
            Box(152, 84, 16, 2, COLOR_GRAY);  Box(152, 90, 16, 2, COLOR_GRAY);  Box(152, 96, 16, 2, COLOR_GRAY);
        }
        else if(rc_t == 1)
        {
            Box(150, 76, 20, 34, COLOR_SILVER);
            Box(150, 76, 20, 4, COLOR_GRAY);
            Box(150, 88, 20, 8, COLOR_RED);
        }
        else
        {
            Box(150, 90, 20, 28, COLOR_GREEN);
            Box(156, 76, 8, 16, COLOR_GREEN);
            Box(155, 72, 10, 4, COLOR_GOLD);
        }
    }
}

// ============================================================================
// SHOOT!  (Galaga)  move and fire. Bugs sway in formation and dive at you.
//         Shoot 6.
// ============================================================================
#define GL_BG   COLOR_BLACK
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

static void GlInit(uint16_t spd)
{
    int16_t i;

    for(i = 0; i < 10; i++) { gl_al[i] = true; gl_dv[i] = 0; gl_dr[i] = false; }
    gl_bon[0] = gl_bon[1] = false;
    gl_bdr[0] = gl_bdr[1] = false;
    gl_sx = gl_psx = 147;
    gl_cd = 40;
    gl_kills = 0;
    gl_spd = 4 + Min16((int16_t)spd / 2, 2);
}

static int16_t GlUpdate(void)
{
    int16_t i, j, sway = Tri(s_frame, 90, 22);

    gl_sx = Clamp16(gl_sx + HeldX() * 5, 0, FIELD_W - 26);

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
            i = Rnd(10);
            if(gl_al[i] && !gl_dv[i]) { gl_dv[i] = 1; break; }
        }
        gl_cd = Max16(28, 60 - 6 * (gl_spd - 4)) + Rnd(20);
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
            gl_ex[i] += Sgn16(gl_sx + 13 - gl_ex[i]) * 2;
            if(gl_ey[i] > 236) gl_dv[i] = 0;                     // back to formation
            if(gl_dv[i] && Hit(gl_ex[i], gl_ey[i], 18, 12, gl_sx, GL_SY, 26, 12)) return MG_LOSE;
        }
    }

    for(j = 0; j < 2; j++)
    {
        if(!gl_bon[j]) continue;
        gl_by[j] -= 9;
        if(gl_by[j] < -8) { gl_bon[j] = false; continue; }
        for(i = 0; i < 10; i++)
            if(gl_al[i] && Hit(gl_bx[j], gl_by[j], 3, 8, gl_ex[i] - 3, gl_ey[i], 24, 12))
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

static void GlRender(void)
{
    int16_t i;

    for(i = 0; i < 10; i++)                                     // erase everything old first
        if(gl_dr[i]) { Box(gl_px[i] - 4, gl_py[i], 26, 12, GL_BG); gl_dr[i] = false; }
    for(i = 0; i < 2; i++)
        if(gl_bdr[i]) { Box(gl_pbx[i], gl_pby[i], 3, 8, GL_BG); gl_bdr[i] = false; }

    for(i = 0; i < 10; i++)
        if(gl_al[i])
        {
            Box(gl_ex[i], gl_ey[i] + 2, 18, 8, (i < 5) ? COLOR_YELLOW : COLOR_CYAN);
            Box(gl_ex[i] - 4, gl_ey[i], 5, 10, COLOR_RED);
            Box(gl_ex[i] + 17, gl_ey[i], 5, 10, COLOR_RED);
            Box(gl_ex[i] + 4, gl_ey[i] + 4, 3, 3, COLOR_BLACK);
            Box(gl_ex[i] + 11, gl_ey[i] + 4, 3, 3, COLOR_BLACK);
            gl_px[i] = gl_ex[i]; gl_py[i] = gl_ey[i]; gl_dr[i] = true;
        }
    for(i = 0; i < 2; i++)
        if(gl_bon[i])
        {
            Box(gl_bx[i], gl_by[i], 3, 8, COLOR_WHITE);
            gl_pbx[i] = gl_bx[i]; gl_pby[i] = gl_by[i]; gl_bdr[i] = true;
        }

    Box(gl_psx, GL_SY - 7, 26, 20, GL_BG);
    Box(gl_sx, GL_SY, 26, 12, COLOR_WHITE);
    Box(gl_sx + 11, GL_SY - 6, 4, 6, COLOR_WHITE);
    Box(gl_sx, GL_SY + 4, 4, 8, COLOR_RED);  Box(gl_sx + 22, GL_SY + 4, 4, 8, COLOR_RED);
    gl_psx = gl_sx;
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

//*****************************************************************************
// 3. GAME TABLE  <- add a microgame: one line here
//      cmd, inspiration tag, frames, background, winOnTimeout, init, update, render
//*****************************************************************************
static const MicroGame kGames[] =
{
    { "CLIMB!",    "Donkey Kong",     260, DK_BG, 0, DkInit, DkUpdate, DkRender },
    { "BATTLE!",   "Pokemon",         200, PK_BG, 0, PkInit, PkUpdate, PkRender },
    { "FLIP!",     "Pokemon Pinball", 340, PB_BG, 0, PbInit, PbUpdate, PbRender },
    { "RUN!",      "Sonic",           260, SO_BG, 0, SoInit, SoUpdate, SoRender },
    { "BLAST!",    "Metroid",         320, MT_BG, 0, MtInit, MtUpdate, MtRender },
    { "SLICE!",    "Fruit Ninja",     280, FN_BG, 0, FnInit, FnUpdate, FnRender },
    { "RACE!",     "Hydro Thunder",   190, HY_BG, 1, HyInit, HyUpdate, HyRender },
    { "TURN!",     "Temple Run",      280, TP_BG, 0, TpInit, TpUpdate, TpRender },
    { "CALL IT!",  "Helldivers 2",    170, HD_BG, 0, HdInit, HdUpdate, HdRender },
    { "BREAK!",    "Brick Breaker",   280, BK_BG, 0, BkInit, BkUpdate, BkRender },
    { "FIT IT!",   "Tetris",          240, TT_BG, 0, TtInit, TtUpdate, TtRender },
    { "CHOMP!",    "Pac-Man",         330, CH_BG, 0, ChInit, ChUpdate, ChRender },
    { "CROSS!",    "Frogger",         280, FR_BG, 0, FrInit, FrUpdate, FrRender },
    { "FLAP!",     "Flappy Bird",     200, FB_BG, 1, FbInit, FbUpdate, FbRender },
    { "JUMP!",     "Chrome Dino",     180, DN_BG, 1, DnInit, DnUpdate, DnRender },
    { "STOMP!",    "Super Mario",     260, MR_BG, 0, MrInit, MrUpdate, MrRender },
    { "DASH!",     "Geometry Dash",   200, GD_BG, 1, GdInit, GdUpdate, GdRender },
    { "LAUNCH!",   "Angry Birds",     260, BD_BG, 0, BdInit, BdUpdate, BdRender },
    { "BEAT!",     "Melatonin",       300, ML_BG, 0, MlInit, MlUpdate, MlRender },
    { "SORT!",     "Recycler",        220, RC_BG, 0, RcInit, RcUpdate, RcRender },
    { "RETURN!",   "Pong",            300, PG_BG, 0, PgInit, PgUpdate, PgRender },
    { "SHOOT!",    "Galaga",          320, GL_BG, 0, GlInit, GlUpdate, GlRender },
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
static bool     s_practice;                         // practice mode: no lives, no score
static int16_t  s_menu, s_sel, s_pLevel;            // title cursor, picker cursor, practice speed
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
        Graphics_FillRect(20 + i * 36, 20, 40 + i * 36, 34, kBanner[i % 6]);
    BoldText("WASON WARE", FIELD_W / 2 + 2, 72, COLOR_MAGENTA);
    BoldText("WASON WARE", FIELD_W / 2,     70, COLOR_YELLOW);
    Text("22 microgames. 4 lives. Go!", FIELD_W / 2, 105, COLOR_WHITE);

    Label(buf, "Best: ", s_best);
    Text(buf, FIELD_W / 2, 132, COLOR_GOLD);

    if(s_menu == 0) Graphics_FillRect(80, 152, 239, 177, COLOR_YELLOW);
    else            Graphics_FillRect(80, 182, 239, 207, COLOR_YELLOW);
    Text("PLAY",     FIELD_W / 2, 165, s_menu == 0 ? COLOR_BLACK : COLOR_WHITE);
    Text("PRACTICE", FIELD_W / 2, 195, s_menu == 1 ? COLOR_BLACK : COLOR_WHITE);
    Text("U/D choose   button: go   R: back", FIELD_W / 2, 228, COLOR_GRAY);
}

// ---- practice: pick any microgame, in pages ---------------------------------
//   8 rows per page. Moving inside a page only repaints the two rows whose
//   highlight changed; the whole page is repainted only when the cursor crosses
//   to another page (or after Settings). Row 0 is "< BACK".
#define PICK_ROWS   8
#define PICK_Y0     40
#define PICK_RH     22

static int16_t s_pickOld;                           // row to un-highlight on a cheap update
static uint16_t s_pickPart;                         // 1 = rows changed, 2 = speed changed

static int16_t PickRows(void) { return (int16_t)NUM_GAMES + 1; }
static int16_t PickPages(void) { return (PickRows() + PICK_ROWS - 1) / PICK_ROWS; }

static void DrawPickRow(int16_t i)
{
    int16_t y = PICK_Y0 + (i % PICK_ROWS) * PICK_RH;

    if(i < 0 || i >= PickRows()) return;
    Graphics_FillRect(20, y - 10, 299, y + 10, (i == s_sel) ? kBanner[i % 6] : COLOR_BLACK);
    Text(i == 0 ? "< BACK" : kGames[i - 1].inspo, FIELD_W / 2, y,
         (i == s_sel) ? COLOR_BLACK : COLOR_WHITE);
}

static void DrawPickSpeed(void)
{
    char buf[24];

    Graphics_FillRect(0, 206, FIELD_W - 1, 226, COLOR_BLACK);
    Label(buf, "Speed: ", (uint32_t)s_pLevel);
    Text(buf, FIELD_W / 2, 218, COLOR_CYAN);
}

static void DrawPick(void)                          // whole page
{
    char    pg[8];
    int16_t page = s_sel / PICK_ROWS, i;

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);
    BoldText("PRACTICE", FIELD_W / 2, 16, COLOR_GOLD);

    pg[0] = (char)('1' + page);  pg[1] = '/';  pg[2] = (char)('0' + PickPages());  pg[3] = 0;
    Text(pg, 292, 16, COLOR_GRAY);

    for(i = page * PICK_ROWS; i < (page + 1) * PICK_ROWS; i++) DrawPickRow(i);

    DrawPickSpeed();
    Text("U/D pick   L/R speed   button: go", FIELD_W / 2, 234, COLOR_GRAY);
}

static void DrawIntro(void)
{
    char    buf[24];
    int16_t i;
    const MicroGame *g = &kGames[s_cur];

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);

    if(!s_practice)
        for(i = 0; i < WW_LIVES; i++)
            Graphics_FillRect(110 + i * 24, 12, 125 + i * 24, 27,
                              (uint16_t)i < s_lives ? COLOR_RED : COLOR_DARK_GRAY);

    Banner(g->cmd, kBanner[s_cur % 6], COLOR_BLACK);
    Text(g->inspo, FIELD_W / 2, 165, COLOR_GRAY);
    if(s_practice)
    {
        BoldText("PRACTICE", FIELD_W / 2, 40, COLOR_GOLD);
        Label(buf, "Speed: ", s_level);
    }
    else
    {
        Label(buf, "Score: ", s_score);
        if(s_speedUp) BoldText("SPEED UP!", FIELD_W / 2, 55, COLOR_ORANGE);
    }
    Text(buf, FIELD_W / 2, 205, COLOR_WHITE);
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
    s_practice = false;
    s_lives = WW_LIVES; s_score = 0; s_played = 0; s_level = 0;
    s_speedUp = 0; s_newBest = false;
    s_bag[NUM_GAMES - 1] = 0xFFFF; s_bagPos = NUM_GAMES;
    Rand_Seed(TimerNow() + 12345UL);
    PickGame();
    Buzzer_PlaySfx(&sfx_menu_select);
    GoPhase(PH_INTRO);
}

static void StartPractice(uint16_t game)
{
    s_practice = true;
    s_cur      = game;
    s_level    = (uint16_t)s_pLevel;
    s_speedUp  = 0;
    Rand_Seed(TimerNow() + 777UL);
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
    if(won) { if(!s_practice) s_score++; Buzzer_PlaySfx(&sfx_powerup); }
    else    { if(!s_practice) s_lives--; Buzzer_PlaySfx(&sfx_hit); }
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
    if(s_practice) { GoPhase(PH_PICK); return; }     // back to the list, cursor stays on the game

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
    s_practice = false;
    s_menu = 0; s_sel = 1; s_pLevel = 0;
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
            if(EdgeU() || EdgeD()) { s_menu ^= 1; s_dirty = true; Buzzer_PlaySfx(&sfx_blip); }
            if(g_joy.btnPressed)
            {
                if(s_menu == 0) StartRun();
                else { Buzzer_PlaySfx(&sfx_menu_select); s_pickPart = 0; GoPhase(PH_PICK); }
            }
            break;

        case PH_PICK:
            if((EdgeU() && s_sel > 0) || (EdgeD() && s_sel < (int16_t)NUM_GAMES))
            {
                s_pickOld = s_sel;
                s_sel += EdgeU() ? -1 : 1;
                if(s_sel / PICK_ROWS != s_pickOld / PICK_ROWS) s_dirty = true;   // new page: full repaint
                else s_pickPart |= 1;                                            // same page: 2 rows only
                Buzzer_PlaySfx(&sfx_blip);
            }
            if(EdgeL() && s_pLevel > 0) { s_pLevel--; s_pickPart |= 2; }
            if(EdgeR() && s_pLevel < 6) { s_pLevel++; s_pickPart |= 2; }
            if(g_joy.btnPressed)
            {
                if(s_sel == 0) GoPhase(PH_TITLE);
                else           StartPractice((uint16_t)(s_sel - 1));
            }
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

        case PH_PICK:
            if(s_dirty) { DrawPick(); s_dirty = false; s_pickPart = 0; }
            else if(s_pickPart)
            {
                Graphics_setFont(&g_sContext, &g_sFontCmss20b);
                if(s_pickPart & 1) { DrawPickRow(s_pickOld); DrawPickRow(s_sel); }
                if(s_pickPart & 2) DrawPickSpeed();
                s_pickPart = 0;
            }
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
    "22 microgames, 4 lives",               // hint shown under the menu
    COLOR_MAGENTA,                          // menu tile color
    WW_FRAME_US,                            // frame period
    WW_Init, WW_Update, WW_Render,
    WW_Exit,
    NULL,                                   // not a legacy app
    WW_Redraw
};
