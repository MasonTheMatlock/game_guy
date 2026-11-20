//#############################################################################
// FILE:   app/wason/wason_helpers.c
// TITLE:  Wason Ware shared helpers: math, drawing, input, text, pixel art
//
//   Everything here is public to the other wason_*.c files (declared in
//   wason_priv.h) and carries a ww_ prefix.
//#############################################################################
#include "wason_priv.h"

bool     ww_first;                          // true on the first render of a screen
uint16_t ww_frame;                          // frames since the microgame started

int16_t ww_Min16(int16_t a, int16_t b) { return (a < b) ? a : b; }
int16_t ww_Max16(int16_t a, int16_t b) { return (a > b) ? a : b; }
int16_t ww_Abs16(int16_t a)            { return (a < 0) ? -a : a; }
int16_t ww_Sgn16(int16_t a)            { return (a > 0) - (a < 0); }

int16_t ww_Clamp16(int16_t v, int16_t lo, int16_t hi)
{
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

int16_t ww_Rnd(int16_t n)               // 0..n-1
{
    return (int16_t)(Rand16() % (uint16_t)n);
}

// Render window. While a microgame renders, (0,0) is the window's top-left
// corner and everything is clipped to it, so a game that only needs a small
// area (and a small area is far cheaper to repaint over this SPI link) just
// declares one in kGames[]. Outside render the window is the whole playfield.
int16_t ww_wx = 0, ww_wy = 0, ww_ww = FIELD_W, ww_wh = FIELD_H;

// Filled rectangle, window-relative and clipped (never touches the fuse bar).
void ww_Box(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c)
{
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;

    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x1 > ww_ww - 1) x1 = ww_ww - 1;
    if(y1 > ww_wh - 1) y1 = ww_wh - 1;
    if(x1 < x || y1 < y) return;
    Graphics_FillRect(ww_wx + x, ww_wy + y, ww_wx + x1, ww_wy + y1, c);
}

bool ww_Hit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return (ax < bx + bw) && (bx < ax + aw) && (ay < by + bh) && (by < ay + ah);
}

// Filled "circle" (octagon from 3 rectangles - grlib's circle.c is not in this
// project, so Graphics_fillCircle would not link). Skipped if any part would
// leave the playfield.
void ww_Disc(int16_t x, int16_t y, int16_t r, uint32_t c)
{
    int16_t a = (r * 3) / 4;
    int16_t b = r / 2;

    if(x - r < 0 || y - r < 0 || x + r >= ww_ww || y + r >= ww_wh) return;
    ww_Box(x - a, y - a, 2 * a + 1, 2 * a + 1, c);
    ww_Box(x - r, y - b, 2 * r + 1, 2 * b + 1, c);
    ww_Box(x - b, y - r, 2 * b + 1, 2 * r + 1, c);
}

// Move a w x h block horizontally from x0 to x1 (same y) touching only the
// pixels that change. Falls back to erase-all + draw-all for big jumps and
// for the first frame of a screen.
void ww_SlideX(int16_t y, int16_t h, int16_t x0, int16_t x1, int16_t w,
                   uint32_t c, uint32_t bg)
{
    int16_t d = x1 - x0;

    if(ww_first || d >= w || d <= -w)
    {
        ww_Box(x0, y, w, h, bg);
        ww_Box(x1, y, w, h, c);
    }
    else if(d > 0)
    {
        ww_Box(x0, y, d, h, bg);
        ww_Box(x0 + w, y, d, h, c);
    }
    else if(d < 0)
    {
        ww_Box(x1 + w, y, -d, h, bg);
        ww_Box(x1, y, -d, h, c);
    }
}

void ww_SlideY(int16_t x, int16_t w, int16_t y0, int16_t y1, int16_t h,
                   uint32_t c, uint32_t bg)
{
    int16_t d = y1 - y0;

    if(ww_first || d >= h || d <= -h)
    {
        ww_Box(x, y0, w, h, bg);
        ww_Box(x, y1, w, h, c);
    }
    else if(d > 0)
    {
        ww_Box(x, y0, w, d, bg);
        ww_Box(x, y0 + h, w, d, c);
    }
    else if(d < 0)
    {
        ww_Box(x, y1 + h, w, -d, bg);
        ww_Box(x, y1, w, -d, c);
    }
}

// ---- input ------------------------------------------------------------------
// MOVE is the stick (ui.h). The colored buttons only move things when
// UI_BUTTONS_ALSO_MOVE is 1.
int16_t ww_HeldX(void) { return Ui_MoveX(); }               // -1 = left, +1 = right
int16_t ww_HeldY(void) { return Ui_MoveY(); }               // -1 = up,   +1 = down

#if UI_BUTTONS_ALSO_MOVE
bool ww_EdgeL(void) { return Ui_NavX() < 0 || g_dpad.lPressed; }
bool ww_EdgeR(void) { return Ui_NavX() > 0 || g_dpad.rPressed; }
bool ww_EdgeU(void) { return Ui_NavY() < 0 || g_dpad.uPressed; }
bool ww_EdgeD(void) { return Ui_NavY() > 0 || g_dpad.dPressed; }
#else
bool ww_EdgeL(void) { return Ui_NavX() < 0; }               // stick tilted left THIS frame
bool ww_EdgeR(void) { return Ui_NavX() > 0; }
bool ww_EdgeU(void) { return Ui_NavY() < 0; }
bool ww_EdgeD(void) { return Ui_NavY() > 0; }
#endif

bool ww_AnyPress(void) { return Ui_AnyPress(); }

// Analog stick Y as a paddle speed (the buttons only help if UI_BUTTONS_ALSO_MOVE).
int16_t ww_PaddleSpeed(int16_t maxSpeed)
{
#if UI_BUTTONS_ALSO_MOVE
    if(g_dpad.u) return -maxSpeed;
    if(g_dpad.d) return  maxSpeed;
#endif
    return (int16_t)(((int32_t)g_joy.dy * maxSpeed) / 100);
}

// ---- text -------------------------------------------------------------------
void ww_Text(const char *s, int16_t x, int16_t y, uint32_t c)
{
    Graphics_setForegroundColor(&g_sContext, c);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH,
                                ww_wx + x, ww_wy + y, TRANSPARENT_TEXT);
}

void ww_BoldText(const char *s, int16_t x, int16_t y, uint32_t c)
{
    ww_Text(s, x,     y, c);
    ww_Text(s, x + 1, y, c);
    ww_Text(s, x,     y + 1, c);
}

// "label" + decimal value -> buf
void ww_Label(char *buf, const char *label, uint32_t v)
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
void ww_Arrow(int16_t cx, int16_t cy, int16_t dir, int16_t u, uint32_t c)
{
    int16_t r, w, sw = (3 * u) / 2;

    for(r = 0; r < 3; r++)
    {
        w = (2 * r + 1) * u;
        if(dir == 0)      ww_Box(cx - w / 2, cy - 3 * u + r * u, w, u, c);
        else if(dir == 1) ww_Box(cx - w / 2, cy + 3 * u - (r + 1) * u, w, u, c);
        else if(dir == 2) ww_Box(cx - 3 * u + r * u, cy - w / 2, u, w, c);
        else              ww_Box(cx + 3 * u - (r + 1) * u, cy - w / 2, u, w, c);
    }
    if(dir == 0)      ww_Box(cx - sw / 2, cy, sw, 3 * u, c);
    else if(dir == 1) ww_Box(cx - sw / 2, cy - 3 * u, sw, 3 * u, c);
    else if(dir == 2) ww_Box(cx, cy - sw / 2, 3 * u, sw, c);
    else              ww_Box(cx - 3 * u, cy - sw / 2, 3 * u, sw, c);
}

// Erase "old rectangle minus new rectangle" (up to 4 bands). Draw the new
// sprite FIRST, then call this: the sprite is never missing from the screen,
// and only the pixels it actually vacated are repainted.
void ww_EraseOutside(int16_t ox, int16_t oy, int16_t ow, int16_t oh,
                         int16_t nx, int16_t ny, int16_t nw, int16_t nh, uint32_t bg)
{
    int16_t ix0 = ww_Max16(ox, nx), iy0 = ww_Max16(oy, ny);
    int16_t ix1 = ww_Min16(ox + ow, nx + nw), iy1 = ww_Min16(oy + oh, ny + nh);

    if(ix0 >= ix1 || iy0 >= iy1) { ww_Box(ox, oy, ow, oh, bg); return; }
    if(iy0 > oy)           ww_Box(ox, oy, ow, iy0 - oy, bg);                       // band above
    if(iy1 < oy + oh)      ww_Box(ox, iy1, ow, oy + oh - iy1, bg);                 // band below
    if(ix0 > ox)           ww_Box(ox, iy0, ix0 - ox, iy1 - iy0, bg);               // band left
    if(ix1 < ox + ow)      ww_Box(ix1, iy0, ox + ow - ix1, iy1 - iy0, bg);         // band right
}

// ---- pixel art ----------------------------------------------------------------
// A sprite is an array of strings, one char per pixel ('.' = background). Runs of
// the same char (and identical rows) are merged into single rectangles, which is
// what keeps an art sprite cheap on this display link.
uint32_t ww_ArtColor(char ch, uint32_t bg)
{
    switch(ch)
    {
        case 'k': return 0x101018UL;  case 'w': return 0xFAFAFAUL;  case 'r': return 0xDC2828UL;
        case 'o': return 0xF58C28UL;  case 'y': return 0xFADC5AUL;  case 'g': return 0x46AA46UL;
        case 'd': return 0x1E6432UL;  case 'b': return 0x326EDCUL;  case 'c': return 0x78C8F0UL;
        case 'n': return 0x825028UL;  case 'p': return 0xFAC8A0UL;  case 's': return 0xBEC3CDUL;
        case 'm': return 0xC83CA0UL;  case 't': return 0x46AAA0UL;  case 'l': return 0x96DC50UL;
        case 'e': return 0x50555FUL;  case 'z': return 0x82A06EUL;  case 'u': return 0x3C468CUL;
        case 'a': return 0xFAFA78UL;  case 'h': return 0xC89646UL;  case 'f': return 0xEBAA5AUL;
        case 'v': return 0x6E32A0UL;  case 'x': return 0x3C1E64UL;  case 'j': return 0x8C1A1AUL;
        case 'i': return 0x46A0E6UL;
        default:  return bg;
    }
}

bool ww_SameRow(const char *a, const char *b, int16_t w)
{
    int16_t i;
    for(i = 0; i < w; i++) if(a[i] != b[i]) return false;
    return true;
}

// Draw a w x h sprite scaled by sc. opaque=true also paints the '.' pixels with
// bg, so the sprite fully replaces whatever was under it (no separate erase).
void ww_DrawArt(int16_t x, int16_t y, const char *const *rows, int16_t w, int16_t h,
                    int16_t sc, uint32_t bg, bool opaque)
{
    int16_t r, c, n, run;

    for(r = 0; r < h; r += n)
    {
        for(n = 1; r + n < h && ww_SameRow(rows[r], rows[r + n], w); n++) { }
        for(c = 0; c < w; c += run)
        {
            for(run = 1; c + run < w && rows[r][c + run] == rows[r][c]; run++) { }
            if(rows[r][c] == '.' && !opaque) continue;
            ww_Box(x + c * sc, y + r * sc, run * sc, n * sc, ww_ArtColor(rows[r][c], bg));
        }
    }
}

// Move an opaque sprite: paint it at the new spot, then erase just what it left.
void ww_MoveArt(int16_t ox, int16_t oy, int16_t nx, int16_t ny, const char *const *rows,
                    int16_t w, int16_t h, int16_t sc, uint32_t bg)
{
    ww_DrawArt(nx, ny, rows, w, h, sc, bg, true);
    ww_EraseOutside(ox, oy, w * sc, h * sc, nx, ny, w * sc, h * sc, bg);
}
