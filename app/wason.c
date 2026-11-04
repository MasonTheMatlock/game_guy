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

enum { PH_TITLE, PH_INTRO, PH_PLAY, PH_RESULT, PH_OVER, PH_PICK, PH_TAUNT };
enum { MG_PLAY = 0, MG_WIN = 1, MG_LOSE = 2 };

typedef struct
{
    const char *cmd;                        // one-word command, shown big
    const char *inspo;                      // "inspired by" tag, shown small
    uint16_t    frames;                     // time limit (frames)
    uint32_t    bg;                         // background color
    int16_t     wx, wy, ww, wh;             // render window (screen coords); smaller = cheaper to repaint
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

// Render window. While a microgame renders, (0,0) is the window's top-left
// corner and everything is clipped to it, so a game that only needs a small
// area (and a small area is far cheaper to repaint over this SPI link) just
// declares one in kGames[]. Outside render the window is the whole playfield.
static int16_t g_wx = 0, g_wy = 0, g_ww = FIELD_W, g_wh = FIELD_H;

// Filled rectangle, window-relative and clipped (never touches the fuse bar).
static void Box(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c)
{
    int16_t x1 = x + w - 1;
    int16_t y1 = y + h - 1;

    if(x < 0) x = 0;
    if(y < 0) y = 0;
    if(x1 > g_ww - 1) x1 = g_ww - 1;
    if(y1 > g_wh - 1) y1 = g_wh - 1;
    if(x1 < x || y1 < y) return;
    Graphics_FillRect(g_wx + x, g_wy + y, g_wx + x1, g_wy + y1, c);
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

    if(x - r < 0 || y - r < 0 || x + r >= g_ww || y + r >= g_wh) return;
    Box(x - a, y - a, 2 * a + 1, 2 * a + 1, c);
    Box(x - r, y - b, 2 * r + 1, 2 * b + 1, c);
    Box(x - b, y - r, 2 * b + 1, 2 * r + 1, c);
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
                                g_wx + x, g_wy + y, TRANSPARENT_TEXT);
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

// Erase "old rectangle minus new rectangle" (up to 4 bands). Draw the new
// sprite FIRST, then call this: the sprite is never missing from the screen,
// and only the pixels it actually vacated are repainted.
static void EraseOutside(int16_t ox, int16_t oy, int16_t ow, int16_t oh,
                         int16_t nx, int16_t ny, int16_t nw, int16_t nh, uint32_t bg)
{
    int16_t ix0 = Max16(ox, nx), iy0 = Max16(oy, ny);
    int16_t ix1 = Min16(ox + ow, nx + nw), iy1 = Min16(oy + oh, ny + nh);

    if(ix0 >= ix1 || iy0 >= iy1) { Box(ox, oy, ow, oh, bg); return; }
    if(iy0 > oy)           Box(ox, oy, ow, iy0 - oy, bg);                       // band above
    if(iy1 < oy + oh)      Box(ox, iy1, ow, oy + oh - iy1, bg);                 // band below
    if(ix0 > ox)           Box(ox, iy0, ix0 - ox, iy1 - iy0, bg);               // band left
    if(ix1 < ox + ow)      Box(ix1, iy0, ox + ow - ix1, iy1 - iy0, bg);         // band right
}

// ---- pixel art ----------------------------------------------------------------
// A sprite is an array of strings, one char per pixel ('.' = background). Runs of
// the same char (and identical rows) are merged into single rectangles, which is
// what keeps an art sprite cheap on this display link.
static uint32_t ArtColor(char ch, uint32_t bg)
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

static bool SameRow(const char *a, const char *b, int16_t w)
{
    int16_t i;
    for(i = 0; i < w; i++) if(a[i] != b[i]) return false;
    return true;
}

// Draw a w x h sprite scaled by sc. opaque=true also paints the '.' pixels with
// bg, so the sprite fully replaces whatever was under it (no separate erase).
static void DrawArt(int16_t x, int16_t y, const char *const *rows, int16_t w, int16_t h,
                    int16_t sc, uint32_t bg, bool opaque)
{
    int16_t r, c, n, run;

    for(r = 0; r < h; r += n)
    {
        for(n = 1; r + n < h && SameRow(rows[r], rows[r + n], w); n++) { }
        for(c = 0; c < w; c += run)
        {
            for(run = 1; c + run < w && rows[r][c + run] == rows[r][c]; run++) { }
            if(rows[r][c] == '.' && !opaque) continue;
            Box(x + c * sc, y + r * sc, run * sc, n * sc, ArtColor(rows[r][c], bg));
        }
    }
}

// Move an opaque sprite: paint it at the new spot, then erase just what it left.
static void MoveArt(int16_t ox, int16_t oy, int16_t nx, int16_t ny, const char *const *rows,
                    int16_t w, int16_t h, int16_t sc, uint32_t bg)
{
    DrawArt(nx, ny, rows, w, h, sc, bg, true);
    EraseOutside(ox, oy, w * sc, h * sc, nx, ny, w * sc, h * sc, bg);
}

//*****************************************************************************
// Pixel art (generated). One string per row; '.' = background. See DrawArt().
//*****************************************************************************

static const char *const kArtFire[16] =
{
    ".......kk.......",
    ".....kkookk.....",
    "....kooooook....",
    "...kooooooook...",
    "...kooooooook...",
    "..koowkoookwokk.",
    "...kokoooookkrrk",
    "...kooooooookrrk",
    "....kookkookraar",
    "....kooyyookkaak",
    "...kooyyyyooorrk",
    "...koyyyyyyooook",
    "...koyyyyyyoook.",
    "....koyyyyoook..",
    "....kooyyookk...",
    "....kookkook...."
};

static const char *const kArtWater[16] =
{
    "................",
    ".....kkkkkk.....",
    "....kbbbbbbk....",
    "...kbbbbbbbbk...",
    "...kbbbbbbbbk...",
    "...kbwkbbbkwk...",
    "...kbkbbbbbk.kk.",
    "...kbbbbbbbbkbck",
    "....kbbkkbbkbbbk",
    "...knccccccnbbbk",
    "..knnyyyyyynbbbk",
    "..knnyyyyyynnbk.",
    "..knnyyyyyynnk..",
    "..knbbyyyybbnk..",
    "...kbbnnnnbbk...",
    "...kbbknnkbbk..."
};

static const char *const kArtGrass[16] =
{
    ".....kkdgkk.....",
    "....kdllgldk....",
    "...kdlllllldk...",
    "...kdlllllldk...",
    "...kdlllllldk...",
    "...kddllllddk...",
    "..kttddddddttk..",
    ".kttttttttdtttk.",
    ".kttwtttttddwtk.",
    "ktttrrtttttrrtk.",
    "ktttdtttttttttk.",
    ".kkttttttttttk..",
    "..kttkttttkttk..",
    "..kttkkkkkkttk..",
    "..kttk....kttk..",
    "...kk......kk..."
};

static const char *const kArtSonic0[20] =
{
    "kk..............",
    "bbkkk..kkkk.....",
    "bbbbbkkbbbbk....",
    "kbbbbbbbbwwwk...",
    ".kkkbbbbbwwkbk..",
    "kkbbbbbbbwwkkk..",
    "bbbbbbbbbwwwppk.",
    "bbbbkbbbbbppppk.",
    "kkkbbbbbbbbpppk.",
    ".kbbbbkbbbbkkk..",
    "kbbbbkbbbbbk....",
    "bbbbkkbbbppk....",
    "bbkkkbbbppppk...",
    "kk..kbbbpppppk..",
    ".....kbbppppppk.",
    "....kbbbbbbkppk.",
    "....kbbkbbbbkk..",
    "...kbbbkkkbbbkkk",
    "..krwwrrk.krwwrr",
    "..krrrrrk.krrrrr"
};

static const char *const kArtSonic1[20] =
{
    "kk..............",
    "bbkkk..kkkk.....",
    "bbbbbkkbbbbk....",
    "kbbbbbbbbwwwk...",
    ".kkkbbbbbwwkbk..",
    "kkbbbbbbbwwkkk..",
    "bbbbbbbbbwwwppk.",
    "bbbbkbbbbbppppk.",
    "kkkbbbbbbbbpppk.",
    ".kbbbbkbbbbkkk..",
    "kbbbbkbbbbbk....",
    "bbbbkkbbbppk....",
    "bbkkkbbbppppk...",
    "kk..kbbbpppppk..",
    ".....kbbppppppk.",
    ".....kbbbbbkppk.",
    "....kkkbbbbkkk..",
    "...krrrrbbkk....",
    "...krrrrwwrrk...",
    "....kkkrrrrrk..."
};

static const char *const kArtSonic2[20] =
{
    "kk..............",
    "bbkkk..kkkk.....",
    "bbbbbkkbbbbk....",
    "kbbbbbbbbwwwk...",
    ".kkkbbbbbwwkbk..",
    "kkbbbbbbbwwkkk..",
    "bbbbbbbbbwwwppk.",
    "bbbbkbbbbbppppk.",
    "kkkbbbbbbbbpppk.",
    ".kbbbbkbbbbkkk..",
    "kbbbbkbbbbbk....",
    "bbbbkbbbpppbk...",
    "bbkkkbbbpppbk...",
    "kk..kbbbpppbk...",
    "...kkkrrrrpkk...",
    "..krrrrrrrrrrk..",
    ".krrwwwwwwwwrrk.",
    ".krrwwwwwwwwrrk.",
    "..krrrrrrrrrrk..",
    "...kkkrrrrkkk..."
};

static const char *const kArtSpin[16] =
{
    ".....kkkkkk.....",
    "....kbbbbbbk....",
    "...kbbbbbbbbk...",
    "..kbbbbbbbbbbk..",
    ".kbbbccccccbbbk.",
    "kbbbccccccccbbbk",
    "kbbbccccccccbbbk",
    "kwwwwwwwwwwwwwwk",
    "kwwwwwwwwwwwwwwk",
    "kbbbccccccccbbbk",
    "kbbbccccccccbbbk",
    ".kbbbccccccbbbk.",
    "..kbbbbbbbbbbk..",
    "...kbbbbbbbbk...",
    "....kbbbbbbk....",
    ".....kkkkkk....."
};

static const char *const kArtMotobug[12] =
{
    "..............",
    "....kkkkkk....",
    "...krrrrrrk...",
    "..krrrrrrkrk..",
    ".krrkrrrrrrrk.",
    "krrrrrrrrrrrrk",
    "krrrrrrkrrrrrk",
    "rrrrrrrrrwwrrr",
    "rrrrrrrrrwwkrr",
    "reeerrrrrrrrrr",
    "keeerrrrrreeek",
    ".kkkrrrrrreeek"
};

static const char *const kArtRing[10] =
{
    "..hhhhhh..",
    ".hyyyyyyh.",
    "hyyyyyyyyh",
    "hyyhhhhyyh",
    "hyyh..hyyh",
    "hyyh..hyyh",
    "hyyhhhhyyh",
    "hyyyyyyyyh",
    ".hyyyyyyh.",
    "..hhhhhh.."
};

static const char *const kArtSpikes[10] =
{
    "kssskkssskksssk.",
    "kssskkssskksssk.",
    "kssk.kssk.kssk..",
    "kssk.kssk.kssk..",
    "kssk.kssk.kssk..",
    "kssk.kssk.kssk..",
    "ssskkssskksssk..",
    "sskkksskkksskkkk",
    "ssssssssssssssss",
    "ssssssssssssssss"
};

static const char *const kArtSpring[10] =
{
    "..kkkkkkkkkk..",
    ".kyyyyyyyyyyk.",
    ".kyyyyyyyyyyk.",
    "..kssssssssk..",
    "...kkkkkkkk...",
    ".kksssssssskk.",
    "krrrrrrrrrrrrk",
    "krrrrrrrrrrrrk",
    "krrrrrrrrrrrrk",
    "krrrrrrrrrrrrk"
};

static const char *const kArtSunflower[20] =
{
    ".kyyyyyyyyk.",
    ".kyyyyyyyyk.",
    ".kyynnnnyyk.",
    "kyyynnnnyyyk",
    "kyyynnnnyyyk",
    ".kyynnnnyyk.",
    ".kyyyyyyyyk.",
    ".kyyyyyyyyk.",
    "..kkkyykkk..",
    "....kddk....",
    "kkkkkddk....",
    "dddddddk....",
    "dddddddkkkkk",
    "kkkkkddddddd",
    "....kddddddd",
    "....kddkkkkk",
    "....kddk....",
    "....kddk....",
    "....kddk....",
    "....kddk...."
};

static const char *const kArtPalm[30] =
{
    "....................",
    ".........kkk........",
    "........kgggk.......",
    "...kkk..kgggk.kkkk..",
    "..kgggk.kgggkkggggk.",
    "..kggggkkgggkgggggk.",
    "..kgggggggggggggggk.",
    "...kggggggggggggkk..",
    "kkkkkgggggggggggk...",
    "gggggggggnnnnggkkkkk",
    "gggggggggnnnnggggggg",
    "gggggggggnnnnggggggg",
    "kkkkkkkkknnnnggggggg",
    ".........knnnnkkkkkk",
    ".........knnnk......",
    ".........knnnk......",
    ".........knnnk......",
    ".........knnnk......",
    ".........knnnk......",
    "........knnnnk......",
    "........knnnnk......",
    "........knnnnk......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk.......",
    "........knnnk......."
};

static const char *const kArtOrange[16] =
{
    ".......kkkkkk...",
    "......knnggggk..",
    ".....kknggggggk.",
    "...kkooooggggk..",
    "..kooooooooook..",
    "..kooooooooook..",
    ".kooaaaaooooook.",
    ".kooaaaaooooook.",
    ".kooooooooooook.",
    ".kooooooooooook.",
    ".kooooooooooook.",
    ".kooooooooooook.",
    "..kooooooooook..",
    "..kooooooooook..",
    "...kkooooookk...",
    ".....kkkkkk....."
};

static const char *const kArtApple[16] =
{
    "........kkkkk...",
    ".......knggggk..",
    "....kkkknggggk..",
    "...krrrknrrrk...",
    "..krrrrrrrrrrk..",
    ".krrrrrrrrrrrrk.",
    "krrrrwrrrrrrrrrk",
    "krrrrwrrrrrrrrrk",
    "krrrrrrrrrrrrrrk",
    "krrrrrrrrrrrrrrk",
    "krrrrrrrrrrrrrrk",
    ".krrrrrrrrrrrrk.",
    "..krrrrrrrrrrk..",
    "...krrrrrrrrk...",
    "....kkrrrrkk....",
    "......kkkk......"
};

static const char *const kArtLime[16] =
{
    ".....kkkkkk.....",
    "....kggggggk....",
    "...kgglgllggk...",
    "..kglllgllllgk..",
    ".kglgllglllglgk.",
    "kggllgaagagllggk",
    "kglllagaggalllgk",
    "kglllaaggaalllgk",
    "kggggggggggggggk",
    "kglllagaggalllgk",
    "kggllgaagagllggk",
    ".kglglllgllglgk.",
    "..kgllllglllgk..",
    "...kggllglggk...",
    "....kggggggk....",
    ".....kkkkkk....."
};

static const char *const kArtBomb[16] =
{
    ".......kkooorak.",
    "......knnokkkk..",
    ".....kknnkk.....",
    "...kkeenneekk...",
    "..keeeeeeeeeek..",
    "..keeeeeeeeeek..",
    ".keesssseeeeeek.",
    ".keesssseeeeeek.",
    ".keeeeeeeeeeeek.",
    ".keeeeeeeeeeeek.",
    ".keeeeeeeeeeeek.",
    ".keeeeeeeeeeeek.",
    "..keeeeeeeeeek..",
    "..keeeeeeeeeek..",
    "...kkeeeeeekk...",
    ".....kkkkkk....."
};

static const char *const kArtPeashooter[16] =
{
    ".....kkk........",
    "...kkgllk.......",
    "..kggllllk......",
    ".kggggllggkkkk..",
    ".kggggggggggggk.",
    "kgggwkgggggggddk",
    "kgggkggggggggdk.",
    ".kgggggggggggddk",
    ".kggggggggggggk.",
    "..kggggggkkkkk..",
    "kkkkkddkk.......",
    "dddddddkkkkkk...",
    "dddddddddddddk..",
    "kkkkkddddddddk..",
    "....kddkkkkkk...",
    ".....kk........."
};

static const char *const kArtZombie[22] =
{
    ".knnnkkkk...",
    ".knnnzzzzk..",
    "..kzzzzzzk..",
    ".kzzwwzzzzk.",
    ".kzkzzkzzzk.",
    ".kzzzzzzzzk.",
    "..kzzzzzzk..",
    "...kkkkzzk..",
    "...kkkkkkk..",
    "kkknnrrnnnk.",
    "zzzznrrnnnk.",
    "zzzznrrnnnk.",
    "zzknnrrnnnk.",
    "kkknnrrnnnk.",
    "..knnnnnnnk.",
    "..knnnnnnnk.",
    "...keeekeeek",
    "...keeekeeek",
    "...keeekeeek",
    "...keeekeeek",
    "...kkkk.kkkk",
    "...kkkk.kkkk"
};

static const char *const kArtPea[6] =
{
    ".dlld.",
    "dlllld",
    "llwlll",
    "llllll",
    "dlllld",
    ".dlld."
};

static const char *const kArtRun0[22] =
{
    "..kbbbbbbbbk..",
    "..kbbbbbbbbk..",
    ".kbbbbbbbbbbk.",
    "kbbbbnnnnnnk..",
    ".kknnnnnnnnk..",
    "...kuuuuuuk...",
    "...knnnnnnk...",
    "...kkknnkkk...",
    "..kyyyyyyyyk..",
    ".kooooooooook.",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    ".kkooooooookk.",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kuuukkwwwk..",
    "..kuuuk.kkk...",
    ".kwwwwk......."
};

static const char *const kArtRun1[22] =
{
    "..kbbbbbbbbk..",
    "..kbbbbbbbbk..",
    ".kbbbbbbbbbbk.",
    "kbbbbnnnnnnk..",
    ".kknnnnnnnnk..",
    "...kuuuuuuk...",
    "...knnnnnnk...",
    "...kkknnkkk...",
    "..kyyyyyyyyk..",
    ".kooooooooook.",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    "kppooooooooppk",
    ".kkooooooookk.",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kuuukkuuuk..",
    "..kwwwkkuuuk..",
    "...kkk.kuuuk..",
    ".......kwwwwk."
};

static const char *const kArtBall[12] =
{
    "...kkkkkk...",
    "..krrrrrrk..",
    ".krrrrrrrrk.",
    "krrrrrrrrrrk",
    "krrrrwwrrrrk",
    ".kkkwwwwkkk.",
    ".kkkwwkwkkk.",
    "kwwwwwwwwwwk",
    "kwwwwwwwwwwk",
    ".kwwwwwwwwk.",
    "..kwwwwwwk..",
    "...kkkkkk..."
};

static const char *const kArtBallHit[12] =
{
    "...kkkkkk...",
    "..kaaaaaak..",
    ".kaaaaaaaak.",
    "kaaaaaaaaaak",
    "kaaaawwaaaak",
    ".kkkwwwwkkk.",
    ".kkkwwkwkkk.",
    "kwwwwwwwwwwk",
    "kwwwwwwwwwwk",
    ".kwwwwwwwwk.",
    "..kwwwwwwk..",
    "...kkkkkk..."
};

static const char *const kArtPikachu[16] =
{
    ".kkyk.kkkk.kyykk",
    ".kyykkyyyykkyyk.",
    ".kyyyyyyyyyyyyk.",
    "..kyyyyyyyyyyk..",
    "...kyywyyywyk...",
    "...kyykyyykyk...",
    "..krryyyyyyrrkkk",
    "..krryyykyyrrkyy",
    "..kyyyyyyyyyyyyy",
    "..kyyyyyyyyyyyyk",
    "..kyyyyyyyyyyyyk",
    "..kyyyyyyyyyyyk.",
    "..kyyyyyyyyyyk..",
    "...kyyyyyyyyk...",
    "....kyyyyyyk....",
    "....kyykkyyk...."
};

static const char *const kArtDiglett[12] =
{
    "..............",
    "......kk......",
    "...kkknnkkk...",
    "..knnnnnnnnk..",
    ".knnnnnnnnnnk.",
    "knnnnnnnnnnnnk",
    "knnnknnnnnknnk",
    "knnnnppppnnnnk",
    "knnnnppkpnnnnk",
    "knnnnppppnnnnk",
    "nnnnnnnnnnnnnn",
    "nnnnnnnnnnnnnn"
};

static const char *const kArtGdSpike[12] =
{
    "......w......",
    ".....wrw.....",
    "....wrrrw....",
    "....wrrrw....",
    "...wrrrrrw...",
    "...wrrrrrw...",
    "..wrrrrrrrw..",
    "..wrrrrrrrw..",
    ".wrrrrrrrrrw.",
    ".wrrrrrrrrrw.",
    "wrrrrrrrrrrrw",
    ".wwwwwwwwwww."
};

static const char *const kArtNote[12] =
{
    "...kkkkkk...",
    "..kmmmmmmk..",
    ".kmmmmmmmmk.",
    "kmmmmwwmmmmk",
    "kmmmwwwwmmmk",
    "kmmwwwwwwmmk",
    "kmmwwwwwwmmk",
    "kmmmwwwwmmmk",
    "kmmmmwwmmmmk",
    ".kmmmmmmmmk.",
    "..kmmmmmmk..",
    "...kkkkkk..."
};

static const char *const kArtMetroid[14] =
{
    "...kkllllllkk...",
    "..kllllggllllk..",
    ".kllggggggggllk.",
    ".klgwgrrrrggwlk.",
    "kllgggrrrrgggllk",
    "klggggrrrrgggglk",
    "klggggrrrrgggglk",
    "kllggggggggggllk",
    "kllggllgllggllk.",
    "klllgllgllggllk.",
    "kllllllgllllllk.",
    "kllkkllllllkllk.",
    "klllklllllkkllk.",
    "klllklllllkkllk."
};

static const char *const kArtSamus[22] =
{
    "...kkkkkk.....",
    "..kooooook....",
    "..kooooook....",
    ".kooooggggk...",
    ".kooooggggk...",
    ".kooooggggk...",
    "..kooooook....",
    "..kooooook....",
    "..kkkkkkkkk...",
    ".koooooooook..",
    ".koooooorrrrk.",
    ".koooooorrrrk.",
    ".kyyyooorrrrk.",
    ".kyyyooorrrrk.",
    ".kyyyooooook..",
    ".koooooooook..",
    ".koooooooook..",
    "..koookoook...",
    "..koookoook...",
    "..koookoook...",
    "..koookoook...",
    ".kyyyykyyyyk.."
};

static const char *const kArtReo[10] =
{
    "uukk....kkuu",
    "uuuuk..kuuuu",
    "kuuuukkuuuuk",
    ".kuuuuuuuuk.",
    "..kkururkk..",
    "...kmmmmk...",
    "...kmmmmk...",
    "....kkkk....",
    "............",
    "............"
};

static const char *const kArtWario[24] =
{
    "........kkkkkkkk........",
    "......kkvvvvvvvvkk......",
    "....kkvvvvvvvvvvvvkk....",
    "...kvvvvvvaaaavvvvvvk...",
    "...kvvvvvaaaaaavvvvvk...",
    "..kvvvvvvaaxaxavvvvvvk..",
    "..kvvvvvvaaaxaavvvvvvk..",
    "...kvvvvvppppppvvvvvk...",
    "..kxkkkkppppppppkkkkk...",
    "..kxkkkkkkppppkkkkkkkk..",
    ".kppppkkkkkkkkkkkkppppk.",
    ".kppppwwwkkkkkkkwwppppk.",
    "kpppppwwwkppppwkwwpppppk",
    ".kppppppppffffppppppppk.",
    ".kpppppppffffffpppppppk.",
    "..kppppppffffffppppppk..",
    "...kppkkkkkffkkkkkppk...",
    "...kkkkkkkkkkkkkkkkkk...",
    "..kvkkkkkkkkkkkkkkkkvk..",
    ".kvvvvkppppppppppkvvvvk.",
    "kvvvvvvppppppppppvvvvvvk",
    "kvvvvvvppppppppppvvvvvvk",
    "vvvvvvvppppppppppvvvvvvv",
    "kvvvvvvppppppppppvvvvvvk"
};

static const char *const kArtMouthC[5] =
{
    "pppppppppp",
    "pppppppppp",
    "pkkkkkkkkp",
    "kppppppppk",
    "pppppppppp"
};

static const char *const kArtMouthO[5] =
{
    "ppwwwwwwpp",
    "pjjjjjjjjp",
    "pjjjjjjjjp",
    "pjjjjjjjjp",
    "ppprrrrppp"
};

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
// BATTLE!  (Pokemon)  pick the right attack. L = HYDRO PUMP, UP = FLAMETHROWER,
//          R = RAZOR LEAF.  Water > Fire > Grass > Water.  Land 3 hits.
// ============================================================================
#define PK_BG   0x203040UL

static const char *const *const kPkArt[3] = { kArtFire, kArtWater, kArtGrass };
static const int16_t  kPkCounter[3] = { 1, 2, 0 };          // fire<-water, water<-grass, grass<-fire
static const char    *kPkName[3]   = { "FIRE", "WATER", "GRASS" };

static int16_t pk_t, pk_hits, pk_shown, pk_drawn, pk_mv, pk_ax, pk_sx, pk_smv;
static bool    pk_anim, pk_sDr;

static void PkInit(uint16_t spd)
{
    pk_t = Rnd(3);
    pk_hits = 0;
    pk_shown = pk_drawn = -1;
    pk_anim = pk_sDr = false;
}

static int16_t PkUpdate(void)
{
    int16_t p = -1;

    if(pk_anim)                                                 // the attack is flying
    {
        pk_ax += 10;
        if(pk_ax >= 104)
        {
            pk_anim = false;
            Buzzer_PlaySfx(&sfx_explosion);
            if(++pk_hits >= 3) return MG_WIN;
            pk_t = (pk_t + 1 + Rnd(2)) % 3;
        }
        return MG_PLAY;
    }

    if(EdgeU())      p = 0;                                     // Flamethrower (fire move)
    else if(EdgeL()) p = 1;                                     // Hydro Pump   (water move)
    else if(EdgeR()) p = 2;                                     // Razor Leaf   (grass move)
    if(p < 0) return MG_PLAY;

    if(p != kPkCounter[pk_t]) return MG_LOSE;                   // not very effective...
    pk_anim = true;  pk_mv = p;  pk_ax = 40;
    Buzzer_PlaySfx(&sfx_laser);
    return MG_PLAY;
}

static void PkStream(int16_t x, int16_t mv)                     // 28x14 attack graphic
{
    if(mv == 1)      { Box(x, 88, 28, 10, COLOR_BLUE);   Box(x + 3, 90, 22, 3, COLOR_CYAN); }          // Hydro Pump
    else if(mv == 0) { Box(x, 86, 28, 14, COLOR_ORANGE); Box(x + 4, 89, 20, 8, COLOR_YELLOW);
                       Box(x + 22, 91, 6, 4, COLOR_RED); }                                            // Flamethrower
    else             { Box(x, 88, 8, 5, COLOR_LIME);  Box(x + 10, 92, 8, 5, COLOR_GREEN);
                       Box(x + 20, 87, 8, 5, COLOR_LIME); Box(x + 4, 95, 8, 4, COLOR_GREEN); }        // Razor Leaf
}

static void PkRender(void)
{
    if(s_first)
    {
        Box(6, 168, 100, 56, COLOR_BLUE);
        Box(110, 168, 100, 56, COLOR_RED);
        Box(214, 168, 100, 56, COLOR_GREEN);
        Text("< HYDRO",   56, 186, COLOR_WHITE);  Text("PUMP",    56, 208, COLOR_WHITE);
        Text("^ FLAME",  160, 186, COLOR_WHITE);   Text("THROWER", 160, 208, COLOR_WHITE);
        Text("RAZOR >",  264, 186, COLOR_WHITE);   Text("LEAF",   264, 208, COLOR_WHITE);
        pk_shown = pk_drawn = -1;
        pk_sDr = false;
    }
    if(pk_drawn != pk_t)
    {
        DrawArt(136, 56, kPkArt[pk_t], 16, 16, 3, PK_BG, true);                 // 48x48, opaque
        Box(100, 112, 120, 24, PK_BG);
        Text(kPkName[pk_t], 160, 124, COLOR_WHITE);
        pk_drawn = pk_t;
    }
    if(pk_anim)
    {
        PkStream(pk_ax, pk_mv);                                 // new first, then clear what it left
        if(pk_sDr) EraseOutside(pk_sx, 86, 28, 14, pk_ax, 86, 28, 14, PK_BG);
        pk_sx = pk_ax;  pk_smv = pk_mv;  pk_sDr = true;
    }
    else if(pk_sDr) { Box(pk_sx, 86, 28, 14, PK_BG); pk_sDr = false; }
    if(pk_shown != pk_hits)
    {
        Box(80, 12, 160, 10, COLOR_DARK_GRAY);
        Box(80, 12, (160 * (3 - pk_hits)) / 3, 10, COLOR_LIME);
        pk_shown = pk_hits;
    }
}

// ============================================================================
// FLIP!  (Pokemon Pinball)  hold L / R (or the stick / button) to raise a
//        flipper. Pikachu and the Voltorbs kick the ball, the Digletts pop up
//        and down. All that matters: don't drain the ball until time's up.
//        200x200 window; only what changed is redrawn.
// ============================================================================
#define PB_BG   0x14143CUL
#define PB_W    200
#define PB_H    200
#define PB_PX   40                          // flipper pivot x (left); right is mirrored
#define PB_PY   160
#define PB_TX   88                          // flipper tip x
#define PB_N    5

static const int16_t kPbX[PB_N] = {  52, 148, 100,  26, 174 };      // 0,1 Voltorb  2 Pikachu  3,4 Diglett
static const int16_t kPbY[PB_N] = {  96,  96,  48,  72,  72 };
static const int16_t kPbR[PB_N] = {  12,  12,  16,   9,   9 };
static const int16_t kPbT[PB_N] = {   0,   0,   1,   2,   2 };

static int16_t pb_x4, pb_y4, pb_vx, pb_vy, pb_px, pb_py;
static int16_t pb_flash[PB_N], pb_kick[2], pb_dt[PB_N];
static bool    pb_lf, pb_rf, pb_dlf, pb_drf, pb_up[PB_N], pb_dsUp[PB_N], pb_dsFl[PB_N];
static bool    pb_nearLow;

static int16_t PbFloorL(int16_t x, bool up)                     // floor height on the left half
{
    if(x < PB_PX) return 130 + ((x - 6) * 30) / (PB_PX - 6);                        // outlane guide
    return PB_PY + ((x - PB_PX) * ((up ? 146 : 176) - PB_PY)) / (PB_TX - PB_PX);    // flipper
}

static void PbInit(uint16_t spd)
{
    int16_t i;

    pb_x4 = (80 + Rnd(40)) * 4;
    pb_y4 = 100 * 4;
    pb_vx = Rnd(17) - 8;
    pb_vy = 0;
    pb_px = pb_x4 / 4; pb_py = pb_y4 / 4;
    pb_kick[0] = pb_kick[1] = 0;
    pb_lf = pb_rf = pb_dlf = pb_drf = false;
    for(i = 0; i < PB_N; i++)
    {
        pb_flash[i] = 0;
        pb_up[i] = (i == 3);  pb_dt[i] = 20 + i * 12;
        pb_dsUp[i] = !pb_up[i];  pb_dsFl[i] = true;
    }
}

static int16_t PbUpdate(void)
{
    int16_t i, x, y, dx, dy, fy, sl, rr;
    bool    right;

    pb_lf = g_dpad.l || g_joy.dx < -50 || g_joy.btn;
    pb_rf = g_dpad.r || g_joy.dx >  50 || g_joy.btn;
    if(pb_kick[0] > 0) pb_kick[0]--;
    if(pb_kick[1] > 0) pb_kick[1]--;

    pb_vy = Min16(pb_vy + 1, 26);
    pb_vx = Clamp16(pb_vx - pb_vx / 24, -28, 28);
    pb_x4 += pb_vx;  pb_y4 += pb_vy;
    x = pb_x4 / 4;  y = pb_y4 / 4;

    if(x < 10)        { pb_x4 = 40;              pb_vx =  Abs16(pb_vx); }
    if(x > PB_W - 10) { pb_x4 = (PB_W - 10) * 4; pb_vx = -Abs16(pb_vx); }
    if(y < 10)        { pb_y4 = 40;              pb_vy =  Abs16(pb_vy); }
    x = pb_x4 / 4;  y = pb_y4 / 4;

    for(i = 0; i < PB_N; i++)                                   // Pokemon bumpers
    {
        if(pb_flash[i] > 0) pb_flash[i]--;
        if(kPbT[i] == 2)                                        // Diglett: pops up and down
        {
            if(--pb_dt[i] <= 0) { pb_up[i] = !pb_up[i]; pb_dt[i] = pb_up[i] ? 45 : 50; }
            if(!pb_up[i]) continue;
        }
        dx = x - kPbX[i];  dy = y - kPbY[i];  rr = kPbR[i] + 4;
        if(dx * dx + dy * dy < rr * rr)
        {
            pb_vx = Clamp16(dx * 2 + (dx > 0 ? 6 : -6), -28, 28);
            pb_vy = Clamp16(dy * 2 + (dy > 0 ? 6 : -6), -28, 28);
            pb_flash[i] = 5;
            if(kPbT[i] == 2) { pb_up[i] = false; pb_dt[i] = 60; }          // Diglett ducks after a hit
            Buzzer_PlaySfx(&sfx_coin);
        }
    }

    right = (x > PB_W / 2);                                     // slanted floors + the draining gap
    if(y + 4 >= 125)
    {
        int16_t xm = right ? (PB_W - 1 - x) : x;
        if(xm >= 6 && xm < PB_TX)
        {
            bool up = right ? pb_rf : pb_lf;
            fy = PbFloorL(xm, up);
            if(y + 4 >= fy && pb_vy >= 0)
            {
                bool onFlip = (xm >= PB_PX);
                int16_t side = right ? 1 : 0;
                y = fy - 4;  pb_y4 = y * 4;
                if(onFlip && up && pb_kick[side] == 0)                 // flip!
                {
                    pb_vy = -26;
                    pb_vx = (6 + (xm - PB_PX) / 5) * (right ? -1 : 1);
                    pb_kick[side] = 8;
                    Buzzer_PlaySfx(&sfx_blip);
                }
                else
                {
                    sl = onFlip ? 1 : 3;
                    pb_vy = (pb_vy > 8) ? -(pb_vy / 3) : 0;
                    pb_vx += (right ? -sl : sl);
                }
            }
        }
    }
    pb_nearLow = (y >= 112);
    if(y >= PB_H - 4) return MG_LOSE;                           // drained
    return MG_PLAY;
}

static void PbDrawFlip(int16_t side, bool up, uint32_t c)
{
    int16_t i, tip = up ? 146 : 176, x, yc;

    for(i = 0; i < 4; i++)
    {
        x  = PB_PX + i * 12;
        yc = PB_PY + ((tip - PB_PY) * (i * 12 + 6)) / (PB_TX - PB_PX);
        Box(side ? PB_W - x - 12 : x, yc - 3, 12, 7, c);
    }
}

static void PbDrawGuides(void)
{
    int16_t i, x, y;

    for(i = 0; i < 6; i++)
    {
        x = 6 + i * ((PB_PX - 6) / 6);
        y = 130 + (i * 30) / 6;
        Box(x, y + 2, (PB_PX - 6) / 6 + 1, 6, COLOR_GRAY);
        Box(PB_W - x - (PB_PX - 6) / 6 - 1, y + 2, (PB_PX - 6) / 6 + 1, 6, COLOR_GRAY);
    }
}

static void PbDrawObj(int16_t i)
{
    bool fl = (pb_flash[i] > 0);

    if(kPbT[i] == 0)       DrawArt(kPbX[i] - 12, kPbY[i] - 12, fl ? kArtBallHit : kArtBall, 12, 12, 2, PB_BG, true);
    else if(kPbT[i] == 1)  DrawArt(kPbX[i] - 16, kPbY[i] - 16, kArtPikachu, 16, 16, 2, fl ? COLOR_WHITE : PB_BG, true);
    else if(pb_up[i])      DrawArt(kPbX[i] - 14, kPbY[i] - 12, kArtDiglett, 14, 12, 2, fl ? COLOR_WHITE : PB_BG, true);
    else
    {
        Box(kPbX[i] - 14, kPbY[i] - 12, 28, 24, PB_BG);
        Box(kPbX[i] - 12, kPbY[i] + 6, 24, 5, 0x05050FUL);               // the empty hole
    }
    pb_dsUp[i] = pb_up[i];  pb_dsFl[i] = fl;
}

static void PbRender(void)
{
    int16_t i, x = pb_x4 / 4, y = pb_y4 / 4;

    if(s_first)
    {
        Box(0, 0, 6, PB_H, COLOR_GRAY);  Box(PB_W - 6, 0, 6, PB_H, COLOR_GRAY);
        Box(0, 0, PB_W, 6, COLOR_GRAY);
        PbDrawGuides();
        for(i = 0; i < PB_N; i++) PbDrawObj(i);
        pb_dlf = pb_drf = false;
        PbDrawFlip(0, false, COLOR_YELLOW);  PbDrawFlip(1, false, COLOR_YELLOW);
    }

    Box(pb_px - 5, pb_py - 5, 10, 10, PB_BG);                   // old ball
    for(i = 0; i < PB_N; i++)
        if(pb_dsUp[i] != pb_up[i] || pb_dsFl[i] != (pb_flash[i] > 0)) PbDrawObj(i);
    if(pb_lf != pb_dlf)
    {
        PbDrawFlip(0, pb_dlf, PB_BG);  PbDrawFlip(0, pb_lf, COLOR_YELLOW);  pb_dlf = pb_lf;
    }
    if(pb_rf != pb_drf)
    {
        PbDrawFlip(1, pb_drf, PB_BG);  PbDrawFlip(1, pb_rf, COLOR_YELLOW);  pb_drf = pb_rf;
    }
    if(pb_nearLow || (pb_py >= 108))                            // ball touched the lower furniture: repair it
    {
        PbDrawGuides();
        PbDrawFlip(0, pb_dlf, COLOR_YELLOW);  PbDrawFlip(1, pb_drf, COLOR_YELLOW);
    }
    Box(x - 3, y - 4, 6, 8, COLOR_WHITE);                       // ball
    Box(x - 4, y - 3, 8, 6, COLOR_WHITE);
    Box(x - 4, y - 1, 8, 2, COLOR_RED);
    pb_px = x; pb_py = y;
}

// ============================================================================
// RUN!  (Sonic the Hedgehog)  Green Hill Zone. Any button = spin jump, DOWN =
//       roll (flattens Motobugs). Grab 5 rings, stomp Motobugs from above,
//       dodge the spikes, bounce off the springs for the high rings.
//       Sprites are transparent over a painted scenery (hills, clouds, palms).
// ============================================================================
#define SO_GY   126                         // ground top (window-local)
#define SO_X    60
#define SO_BG   0x4FA5F5UL
#define SO_N    4

typedef struct { int16_t x, y, w, h; uint32_t c; } SoRect;
static const SoRect kSoBg[] =
{
    { 30, 16, 46, 10, 0xFFFFFFUL }, { 40, 10, 28, 8, 0xFFFFFFUL },          // clouds
    { 190, 24, 52, 10, 0xFFFFFFUL }, { 200, 18, 32, 8, 0xFFFFFFUL }, { 280, 12, 36, 8, 0xFFFFFFUL },
    { 10, 84, 100, 42, 0x2E9E6BUL }, { 24, 76, 72, 8, 0x2E9E6BUL },         // far hills
    { 150, 70, 130, 56, 0x2E9E6BUL }, { 168, 60, 94, 10, 0x2E9E6BUL }, { 290, 92, 40, 34, 0x2E9E6BUL },
    { 60, 100, 80, 26, 0x3CC06AUL }, { 74, 94, 52, 6, 0x3CC06AUL },         // near hills
    { 230, 96, 90, 30, 0x3CC06AUL }, { 244, 90, 62, 6, 0x3CC06AUL },
};
static const int16_t kSoDecX[5] = { 14, 100, 168, 246, 296 };                // static palms / sunflowers
static const int16_t kSoDecT[5] = {  0,   1,   0,   1,   0 };

static int16_t so_y, so_vy, so_py, so_v, so_cd, so_rings, so_ph, so_pend, so_chain, so_chr, so_roll, so_dRings;
static int16_t so_pw1, so_ph1, so_pt1;
static int16_t so_x[SO_N], so_px[SO_N], so_ty[SO_N], so_t[SO_N], so_h[SO_N], so_w[SO_N];
static bool    so_on[SO_N], so_dr[SO_N];

static const char *const *SoArt(int16_t t)
{
    if(t == 0) return kArtRing;
    if(t == 1) return kArtMotobug;
    if(t == 2) return kArtSpikes;
    return kArtSpring;
}

static void SoDecor(int16_t i)
{
    if(kSoDecT[i] == 0) DrawArt(kSoDecX[i], SO_GY - 30, kArtPalm, 20, 30, 1, SO_BG, false);
    else                DrawArt(kSoDecX[i], SO_GY - 20, kArtSunflower, 12, 20, 1, SO_BG, false);
}

// Repaint the scenery inside a window-local rectangle.
static void SoPaint(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t i, ix0, iy0, ix1, iy1, dw, dh;

    Box(x, y, w, h, SO_BG);
    for(i = 0; i < (int16_t)(sizeof(kSoBg) / sizeof(kSoBg[0])); i++)
    {
        ix0 = Max16(x, kSoBg[i].x);  iy0 = Max16(y, kSoBg[i].y);
        ix1 = Min16(x + w, kSoBg[i].x + kSoBg[i].w);  iy1 = Min16(y + h, kSoBg[i].y + kSoBg[i].h);
        if(ix0 < ix1 && iy0 < iy1) Box(ix0, iy0, ix1 - ix0, iy1 - iy0, kSoBg[i].c);
    }
    for(i = 0; i < 5; i++)
    {
        dw = kSoDecT[i] ? 12 : 20;  dh = kSoDecT[i] ? 20 : 30;
        if(Hit(x, y, w, h, kSoDecX[i], SO_GY - dh, dw, dh)) SoDecor(i);
    }
}

static void SoInit(uint16_t spd)
{
    uint16_t i;

    so_y = so_py = SO_GY - 20;
    so_vy = 0;
    so_v  = 5 + Min16((int16_t)spd / 2, 3);
    so_cd = 12;
    so_rings = 0;  so_dRings = -1;
    so_ph = 0;  so_pend = 0;  so_chain = 0;  so_chr = 0;  so_roll = 0;
    so_pw1 = 16;  so_ph1 = 20;  so_pt1 = SO_GY - 20;
    for(i = 0; i < SO_N; i++) { so_on[i] = false; so_dr[i] = false; }
}

static int16_t SoUpdate(void)
{
    uint16_t i, j;
    int16_t  r, t = 0, hh, ww, hy, hh2;
    bool     air;

    air = (so_y < SO_GY - 20);
    if(!air && so_roll == 0 && EdgeD()) { so_roll = 26; Buzzer_PlaySfx(&sfx_laser); }    // spin roll
    else if(!air && (g_joy.btnPressed || g_dpad.uPressed || g_dpad.lPressed || g_dpad.rPressed))
    { so_vy = -11; so_roll = 0; Buzzer_PlaySfx(&sfx_jump); }
    if(so_roll > 0) so_roll--;
    so_vy++;
    so_y += so_vy;
    if(so_y >= SO_GY - 20) { so_y = SO_GY - 20; so_vy = 0; }
    air = (so_y < SO_GY - 20);
    if((s_frame & 1) == 0) so_ph = (so_ph + 1) & 3;

    if(--so_cd <= 0)
    {
        for(i = 0; i < SO_N; i++)
            if(!so_on[i])
            {
                if(so_pend > 0)       { t = 0; so_pend--; r = 2; }                 // high rings after a spring
                else if(so_chain > 0) { t = 0; so_chain--; r = so_chr; }           // rest of a ring chain
                else
                {
                    r = Rnd(8);
                    t = (r < 3) ? 0 : ((r < 5) ? 1 : ((r < 7) ? 2 : 3));
                    if(t == 0) { so_chr = Rnd(2); r = so_chr; so_chain = 2; }       // a chain of 3 rings
                    else r = 0;
                }
                hh = (t == 1) ? 12 : 10;
                ww = (t == 0) ? 10 : ((t == 1) ? 14 : ((t == 2) ? 16 : 14));
                so_on[i] = true; so_t[i] = t; so_h[i] = hh; so_w[i] = ww;
                so_ty[i] = (t == 0) ? ((r == 2) ? SO_GY - 84 : ((r == 1) ? SO_GY - 50 : SO_GY - 24)) : SO_GY - hh;
                so_x[i] = so_px[i] = 322;
                if(t == 3) so_pend = 2;
                break;
            }
        so_cd = (so_pend > 0 || so_chain > 0) ? 5 : ((t == 0 || t == 3) ? 14 + Rnd(8) : 26 + Rnd(10));
    }

    hy = (so_roll > 0 && !air) ? so_y + 6 : so_y + 2;
    hh2 = (so_roll > 0 && !air) ? 12 : (air ? 14 : 16);
    for(i = 0; i < SO_N; i++)
    {
        if(!so_on[i]) continue;
        so_x[i] -= so_v;
        if(so_x[i] < -20) { so_on[i] = false; continue; }
        if(!Hit(SO_X + 2, hy, 12, hh2, so_x[i], so_ty[i], so_w[i], so_h[i])) continue;

        if(so_t[i] == 0)
        {
            so_on[i] = false;
            Buzzer_PlaySfx(&sfx_coin);
            if(++so_rings >= 5) return MG_WIN;
        }
        else if(so_t[i] == 1)                                   // Motobug: roll through it or land on it
        {
            if((so_roll > 0 && !air) || (so_vy > 0 && so_y + 20 - so_vy <= so_ty[i] + 4))
            {
                so_on[i] = false;  if(air) so_vy = -8;
                Buzzer_PlaySfx(&sfx_explosion);
                for(j = 0; j < SO_N; j++) if(so_on[j] && so_t[j] == 0 && so_x[j] > 200) { so_on[j] = false; }
                if(++so_rings >= 5) return MG_WIN;
            }
            else return MG_LOSE;
        }
        else if(so_t[i] == 2) return MG_LOSE;                   // spikes
        else if(so_vy >= 0)                                     // spring
        {
            so_vy = -15;  so_on[i] = false;
            Buzzer_PlaySfx(&sfx_jump);
        }
    }
    return MG_PLAY;
}

static void SoRender(void)
{
    uint16_t i, k;
    int16_t  top, w, h, rx, ry, rw, rh, t;
    bool     air = (so_y < SO_GY - 20), ball = air || so_roll > 0, redo;
    const char *const *art;

    if(s_first)
    {
        Box(0, SO_GY, 320, 6, 0x58D03CUL);
        Box(0, SO_GY + 6, 320, 28, 0xC68E4AUL);
        for(i = 0; i < 16; i++) { Box(i * 20, SO_GY + 6, 10, 14, 0x8B5A2BUL); Box(i * 20 + 10, SO_GY + 20, 10, 14, 0x8B5A2BUL); }
        SoPaint(0, 0, 320, SO_GY);
        so_pt1 = SO_GY - 20;  so_pw1 = 16;  so_ph1 = 20;
        so_dRings = -1;
    }

    for(i = 0; i < SO_N; i++)                                   // items: repaint old spot, draw new (back to back)
    {
        if(so_on[i])
        {
            if(so_x[i] != so_px[i] || !so_dr[i])
            {
                if(so_dr[i]) SoPaint(so_px[i], so_ty[i], so_w[i], so_h[i]);
                DrawArt(so_x[i], so_ty[i], SoArt(so_t[i]), so_w[i], so_h[i], 1, SO_BG, false);
                so_px[i] = so_x[i];  so_dr[i] = true;
            }
        }
        else if(so_dr[i]) { SoPaint(so_px[i], so_ty[i], so_w[i], so_h[i]); so_dr[i] = false; }
    }

    if(ball) { art = kArtSpin; w = 16; h = 16; top = so_y + (air ? 2 : 4); }
    else
    {
        w = 16; h = 20; top = so_y;
        t = (so_v >= 7 && (so_ph & 2)) ? 2 : (so_ph & 1);        // peel-out blur once he's really moving
        art = (t == 2) ? kArtSonic2 : (t ? kArtSonic1 : kArtSonic0);
    }
    redo = s_first || top != so_pt1 || h != so_ph1 || (!ball);   // running legs animate every frame
    for(i = 0; i < SO_N && !redo; i++)
        if(so_dr[i] && Hit(SO_X - 4, top - 2, 24, h + 4, so_px[i], so_ty[i], so_w[i], so_h[i])) redo = true;
    if(redo)
    {
        ry = Min16(top, so_pt1);  rh = Max16(top + h, so_pt1 + so_ph1) - ry;  rx = SO_X;  rw = 16;
        SoPaint(rx, ry, rw, rh);
        for(i = 0; i < SO_N; i++)                               // items that were under his box come back
            if(so_dr[i] && Hit(rx, ry, rw, rh, so_px[i], so_ty[i], so_w[i], so_h[i]))
                DrawArt(so_px[i], so_ty[i], SoArt(so_t[i]), so_w[i], so_h[i], 1, SO_BG, false);
        DrawArt(SO_X, top, art, w, h, 1, SO_BG, false);
        so_pt1 = top;  so_ph1 = h;
    }

    if(so_dRings != so_rings)                                   // HUD: rings collected
    {
        for(k = 0; k < 5; k++)
        {
            Box(4 + k * 14, 2, 12, 12, SO_BG);
            if((int16_t)k < so_rings) DrawArt(5 + k * 14, 3, kArtRing, 10, 10, 1, SO_BG, false);
        }
        so_dRings = so_rings;
    }
}

// ============================================================================
// PLANT!  (Plants vs Zombies)  UP/DOWN picks a lane, the button plants a
//         Peashooter at the house end of it. Zombies EAT plants, so keep
//         replanting. Stop 5 before one gets in the house.
// ============================================================================
#define PZ_LH   56                          // lane height
#define PZ_HX   26                          // house strip width
#define PZ_N    4
#define PZ_BG0  0x4CAF50UL
#define PZ_BG1  0x3E9A42UL

static int16_t pz_lane, pz_dlane, pz_kills, pz_cd, pz_spd, pz_spw, pz_cool;
static int16_t pz_zx[PZ_N], pz_zl[PZ_N], pz_zhp[PZ_N], pz_zpx[PZ_N], pz_zfl[PZ_N];
static bool    pz_zon[PZ_N], pz_zdr[PZ_N];
static bool    pz_plant[3], pz_pdr[3], pz_erase[3];
static int16_t pz_fire[3], pz_php[3];
static int16_t pz_pxp[8], pz_pl[8], pz_ppx[8];
static bool    pz_pon[8], pz_pdr2[8];

static uint32_t PzBg(int16_t lane) { return (lane & 1) ? PZ_BG1 : PZ_BG0; }

static void PzInit(uint16_t spd)
{
    int16_t i;

    pz_lane = 1; pz_dlane = -1; pz_kills = 0;
    pz_cd = 12;  pz_cool = 0;
    pz_spd = 4 + Min16((int16_t)spd / 3, 2);
    pz_spw = Max16(20, 38 - 3 * (int16_t)spd);
    for(i = 0; i < PZ_N; i++) { pz_zon[i] = pz_zdr[i] = false; pz_zfl[i] = 0; }
    for(i = 0; i < 3; i++) { pz_plant[i] = pz_pdr[i] = pz_erase[i] = false; pz_fire[i] = 20; pz_php[i] = 0; }
    for(i = 0; i < 8; i++) { pz_pon[i] = pz_pdr2[i] = false; }
}

static int16_t PzUpdate(void)
{
    int16_t i, j;
    bool    seen;

    if(pz_cool > 0) pz_cool--;
    if(EdgeU() && pz_lane > 0) pz_lane--;
    if(EdgeD() && pz_lane < 2) pz_lane++;
    if((g_joy.btnPressed || g_dpad.lPressed || g_dpad.rPressed) && !pz_plant[pz_lane] && pz_cool == 0)
    {
        pz_plant[pz_lane] = true;  pz_php[pz_lane] = 70;  pz_fire[pz_lane] = 12;
        pz_cool = 22;                                           // seed packet recharge
        Buzzer_PlaySfx(&sfx_coin);
    }

    if(--pz_cd <= 0)
    {
        for(i = 0; i < PZ_N; i++)
            if(!pz_zon[i])
            {
                pz_zon[i] = true; pz_zl[i] = Rnd(3); pz_zx[i] = pz_zpx[i] = 292;
                pz_zhp[i] = (Rnd(3) == 0) ? 7 : 4;                // conehead / regular
                break;
            }
        pz_cd = pz_spw + Rnd(18);
    }

    for(i = 0; i < PZ_N; i++)                                   // zombies shamble every 3rd frame, or chew
    {
        if(!pz_zon[i]) continue;
        if(pz_zfl[i] > 0) pz_zfl[i]--;
        if(pz_plant[pz_zl[i]] && pz_zx[i] <= PZ_HX + 38)        // chewing the plant in this lane
        {
            if(--pz_php[pz_zl[i]] <= 0) { pz_plant[pz_zl[i]] = false; pz_pdr[pz_zl[i]] = false; pz_erase[pz_zl[i]] = true;
                                          Buzzer_PlaySfx(&sfx_thud); }
            continue;
        }
        if((s_frame % 3) == 0) pz_zx[i] -= pz_spd;
        if(pz_zx[i] <= PZ_HX) return MG_LOSE;
    }

    for(i = 0; i < 3; i++)                                      // plants shoot when a zombie is in the lane
    {
        if(!pz_plant[i]) continue;
        if(--pz_fire[i] > 0) continue;
        pz_fire[i] = 14;
        seen = false;
        for(j = 0; j < PZ_N; j++) if(pz_zon[j] && pz_zl[j] == i) seen = true;
        if(seen)
        {
            for(j = 0; j < 8; j++)
                if(!pz_pon[j]) { pz_pon[j] = true; pz_pl[j] = i; pz_pxp[j] = pz_ppx[j] = PZ_HX + 34; break; }
            Buzzer_PlaySfx(&sfx_laser);
        }
    }
    for(j = 0; j < 8; j++)                                      // peas
    {
        if(!pz_pon[j]) continue;
        pz_pxp[j] += 7;
        if(pz_pxp[j] > 320) { pz_pon[j] = false; continue; }
        for(i = 0; i < PZ_N; i++)
            if(pz_zon[i] && pz_zl[i] == pz_pl[j] && pz_pxp[j] + 6 >= pz_zx[i] && pz_pxp[j] <= pz_zx[i] + 24)
            {
                pz_pon[j] = false;
                pz_zfl[i] = 3;
                if(--pz_zhp[i] <= 0)
                {
                    pz_zon[i] = false;
                    Buzzer_PlaySfx(&sfx_explosion);
                    if(++pz_kills >= 5) return MG_WIN;
                }
                break;
            }
    }
    return MG_PLAY;
}

static void PzRender(void)
{
    int16_t i, ly;

    if(s_first)
    {
        for(i = 0; i < 3; i++) Box(PZ_HX, i * PZ_LH, 320 - PZ_HX, PZ_LH, PzBg(i));
        Box(0, 0, PZ_HX, 3 * PZ_LH, 0x8D6E63UL);
        for(i = 0; i < 3; i++) Box(4, i * PZ_LH + 14, 18, 28, 0x5D4037UL);       // doors
        pz_dlane = -1;
        for(i = 0; i < 3; i++) pz_pdr[i] = false;
    }

    if(s_first || pz_lane != pz_dlane)                          // lane marker on the house wall
    {
        if(pz_dlane >= 0) Box(14, pz_dlane * PZ_LH + 4, 10, 8, 0x8D6E63UL);
        Box(14, pz_lane * PZ_LH + 4, 10, 8, pz_cool > 0 ? COLOR_GRAY : COLOR_YELLOW);
        pz_dlane = pz_lane;
    }

    for(i = 0; i < 3; i++)
    {
        if(pz_erase[i]) { Box(PZ_HX, i * PZ_LH + 6, 40, 44, PzBg(i)); pz_erase[i] = false; }
        if(pz_plant[i] && !pz_pdr[i])
        {
            DrawArt(PZ_HX + 2, i * PZ_LH + 12, kArtPeashooter, 16, 16, 2, PzBg(i), true);
            pz_pdr[i] = true;
        }
    }

    for(i = 0; i < PZ_N; i++)
    {
        ly = pz_zl[i] * PZ_LH + 6;
        if(pz_zon[i])
        {
            if(pz_zx[i] != pz_zpx[i] || !pz_zdr[i])
            {
                if(pz_zdr[i]) MoveArt(pz_zpx[i], ly, pz_zx[i], ly, kArtZombie, 12, 22, 2, PzBg(pz_zl[i]));
                else          DrawArt(pz_zx[i], ly, kArtZombie, 12, 22, 2, PzBg(pz_zl[i]), true);
                pz_zpx[i] = pz_zx[i];  pz_zdr[i] = true;
            }
        }
        else if(pz_zdr[i]) { Box(pz_zpx[i], ly, 24, 44, PzBg(pz_zl[i])); pz_zdr[i] = false; }
    }

    for(i = 0; i < 8; i++)
    {
        ly = pz_pl[i] * PZ_LH + 17;
        if(pz_pon[i])
        {
            if(pz_pdr2[i]) MoveArt(pz_ppx[i], ly, pz_pxp[i], ly, kArtPea, 6, 6, 1, PzBg(pz_pl[i]));
            else           DrawArt(pz_pxp[i], ly, kArtPea, 6, 6, 1, PzBg(pz_pl[i]), true);
            pz_ppx[i] = pz_pxp[i];  pz_pdr2[i] = true;
        }
        else if(pz_pdr2[i]) { Box(pz_ppx[i], ly, 6, 6, PzBg(pz_pl[i])); pz_pdr2[i] = false; }
    }
}

// ============================================================================
// BLAST!  (Metroid)  Aim the arm cannon with the STICK, fire with the button.
//         Freeze 5 of the creatures before one reaches Samus.
//         Metroids take 2 hits; Reo bats are fast and wavy.
// ============================================================================
#define MT_BG   0x14001EUL
#define MT_N    4
#define MT_CX   40                          // cannon muzzle origin
#define MT_CY   104

static int16_t mt_ax, mt_ay, mt_dvx, mt_dvy, mt_kills, mt_cd, mt_spd;
static int16_t mt_ex[MT_N], mt_ey[MT_N], mt_pex[MT_N], mt_pey[MT_N], mt_et[MT_N], mt_hp[MT_N], mt_fl[MT_N];
static int16_t mt_bx[5], mt_by[5], mt_bvx[5], mt_bvy[5], mt_pbx[5], mt_pby[5];
static bool    mt_eon[MT_N], mt_edr[MT_N], mt_bon[5], mt_bdr[5], mt_aimDirty;

static void MtAim(void)                                         // stick -> unit-ish vector (|max| = 8)
{
    int16_t s = Max16(Abs16(g_joy.dx), Abs16(g_joy.dy));

    if(s >= 25)
    {
        mt_ax = (int16_t)(((int32_t)g_joy.dx * 8) / s);
        mt_ay = (int16_t)(((int32_t)g_joy.dy * 8) / s);
        if(mt_ax < 2 && mt_ax > -2 && mt_ay < 2 && mt_ay > -2) mt_ax = 8;
    }
}

static void MtInit(uint16_t spd)
{
    int16_t i;

    mt_ax = 8; mt_ay = 0; mt_dvx = mt_dvy = 99;
    mt_kills = 0;
    mt_cd = 15;
    mt_spd = 1 + Min16((int16_t)spd / 3, 1);
    mt_aimDirty = true;
    for(i = 0; i < MT_N; i++) { mt_eon[i] = mt_edr[i] = false; mt_fl[i] = 0; }
    for(i = 0; i < 5; i++) { mt_bon[i] = mt_bdr[i] = false; }
}

static int16_t MtUpdate(void)
{
    int16_t i, j, w, h;

    MtAim();
    if(mt_ax != mt_dvx || mt_ay != mt_dvy) mt_aimDirty = true;

    if(g_joy.btnPressed || g_dpad.lPressed || g_dpad.rPressed || g_dpad.uPressed || g_dpad.dPressed)
        for(i = 0; i < 5; i++)
            if(!mt_bon[i])
            {
                mt_bon[i] = true;  mt_bx[i] = mt_pbx[i] = MT_CX + mt_ax * 2;  mt_by[i] = mt_pby[i] = MT_CY + mt_ay * 2;
                mt_bvx[i] = mt_ax;  mt_bvy[i] = mt_ay;
                Buzzer_PlaySfx(&sfx_laser);
                break;
            }

    if(--mt_cd <= 0)
    {
        for(i = 0; i < MT_N; i++)
            if(!mt_eon[i])
            {
                mt_eon[i] = true;  mt_et[i] = (Rnd(3) == 0) ? 1 : 0;
                mt_ex[i] = mt_pex[i] = 324;  mt_ey[i] = mt_pey[i] = 20 + Rnd(170);
                mt_hp[i] = mt_et[i] ? 1 : 2;  mt_fl[i] = 0;
                break;
            }
        mt_cd = Max16(22, 44 + Rnd(24) - 8 * mt_spd);
    }

    for(i = 0; i < MT_N; i++)
    {
        if(!mt_eon[i]) continue;
        if(mt_fl[i] > 0) mt_fl[i]--;
        mt_ex[i] -= mt_et[i] ? mt_spd + 2 : mt_spd + 1;
        if(((s_frame + i * 7) & 3) == 0)                         // drift toward Samus, bats weave
        {
            if(mt_ey[i] < 100) mt_ey[i]++; else if(mt_ey[i] > 110) mt_ey[i]--;
            if(mt_et[i]) mt_ey[i] += ((s_frame >> 3) & 1) ? 3 : -3;
        }
        w = mt_et[i] ? 12 : 16;  h = mt_et[i] ? 10 : 14;
        if(Hit(mt_ex[i], mt_ey[i], w * 2, h * 2, 8, 88, 30, 44)) return MG_LOSE;
    }

    for(j = 0; j < 5; j++)
    {
        if(!mt_bon[j]) continue;
        mt_bx[j] += mt_bvx[j];  mt_by[j] += mt_bvy[j];
        if(mt_bx[j] < 0 || mt_bx[j] > 330 || mt_by[j] < -6 || mt_by[j] > 226) { mt_bon[j] = false; continue; }
        for(i = 0; i < MT_N; i++)
        {
            if(!mt_eon[i]) continue;
            w = mt_et[i] ? 12 : 16;  h = mt_et[i] ? 10 : 14;
            if(Hit(mt_bx[j] - 3, mt_by[j] - 3, 7, 7, mt_ex[i], mt_ey[i], w * 2, h * 2))
            {
                mt_bon[j] = false;  mt_fl[i] = 3;
                if(--mt_hp[i] <= 0)
                {
                    mt_eon[i] = false;
                    Buzzer_PlaySfx(&sfx_explosion);
                    if(++mt_kills >= 5) return MG_WIN;
                }
                else Buzzer_PlaySfx(&sfx_blip);
                break;
            }
        }
    }
    return MG_PLAY;
}

static void MtBarrel(int16_t vx, int16_t vy, bool on)
{
    int16_t k;
    for(k = 1; k <= 4; k++)
        Box(MT_CX + vx * k - 2, MT_CY + vy * k - 2, 5, 5, on ? COLOR_ORANGE : MT_BG);
    Box(MT_CX + vx * 8 - 1, MT_CY + vy * 8 - 1, 3, 3, on ? COLOR_YELLOW : MT_BG);          // sight
}

static void MtRender(void)
{
    int16_t i;

    if(s_first)
    {
        DrawArt(8, 88, kArtSamus, 14, 22, 2, MT_BG, true);
        mt_dvx = mt_dvy = 99;  mt_aimDirty = true;
        for(i = 0; i < MT_N; i++) mt_edr[i] = false;
    }
    if(mt_aimDirty)
    {
        if(mt_dvx != 99) MtBarrel(mt_dvx, mt_dvy, false);
        MtBarrel(mt_ax, mt_ay, true);
        mt_dvx = mt_ax;  mt_dvy = mt_ay;
        mt_aimDirty = false;
    }

    for(i = 0; i < MT_N; i++)
    {
        if(mt_eon[i])
        {
            if(mt_edr[i]) MoveArt(mt_pex[i], mt_pey[i], mt_ex[i], mt_ey[i], mt_et[i] ? kArtReo : kArtMetroid,
                                  mt_et[i] ? 12 : 16, mt_et[i] ? 10 : 14, 2, MT_BG);
            else          DrawArt(mt_ex[i], mt_ey[i], mt_et[i] ? kArtReo : kArtMetroid,
                                  mt_et[i] ? 12 : 16, mt_et[i] ? 10 : 14, 2, MT_BG, true);
            if(mt_fl[i] > 0) Box(mt_ex[i] + 8, mt_ey[i] + 6, 6, 6, COLOR_WHITE);
            mt_pex[i] = mt_ex[i];  mt_pey[i] = mt_ey[i];  mt_edr[i] = true;
        }
        else if(mt_edr[i]) { Box(mt_pex[i], mt_pey[i], 32, 28, MT_BG); mt_edr[i] = false; }
    }
    for(i = 0; i < 5; i++)
    {
        if(mt_bdr[i]) { Box(mt_pbx[i] - 3, mt_pby[i] - 3, 7, 7, MT_BG); mt_bdr[i] = false; }
        if(mt_bon[i])
        {
            Box(mt_bx[i] - 2, mt_by[i] - 2, 5, 5, COLOR_CYAN);
            mt_pbx[i] = mt_bx[i];  mt_pby[i] = mt_by[i];  mt_bdr[i] = true;
        }
    }
}

// ============================================================================
// SLICE!  (Fruit Ninja)  swipe the cursor through 4 fruits. Avoid the bomb!
//         Sprites are opaque art: draw the new frame first, then erase only the
//         strips the fruit left behind, so nothing flickers.
// ============================================================================
#define FN_N    2
#define FN_BG   0x1A1A2EUL

static const char *const *const kFnArt[4] = { kArtOrange, kArtApple, kArtLime, kArtBomb };

static int16_t fn_x[FN_N], fn_y4[FN_N], fn_vx[FN_N], fn_vy4[FN_N], fn_dx[FN_N], fn_dy[FN_N], fn_k[FN_N];
static bool    fn_on[FN_N], fn_dr[FN_N];
static int16_t fn_cx, fn_cy, fn_pcx, fn_pcy, fn_cd, fn_cnt, fn_shown, fn_rate;

static void FnInit(uint16_t spd)
{
    uint16_t i;

    for(i = 0; i < FN_N; i++) { fn_on[i] = false; fn_dr[i] = false; }
    fn_cx = fn_pcx = 160;
    fn_cy = fn_pcy = 115;
    fn_cd = 10;
    fn_cnt = 0; fn_shown = -1;
    fn_rate = Max16(16, 30 - 2 * (int16_t)spd);
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
                fn_k[i]    = (Rnd(6) == 0) ? 3 : Rnd(3);
                fn_x[i]    = 50 + Rnd(220);
                fn_y4[i]   = (FIELD_H + 20) * 4;
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
        if(fn_vy4[i] > 0 && fn_y4[i] > (FIELD_H + 26) * 4) { fn_on[i] = false; continue; }

        if(Abs16(fn_x[i] - fn_cx) <= 20 && Abs16(fn_y4[i] / 4 - fn_cy) <= 20)
        {
            fn_on[i] = false;
            if(fn_k[i] == 3) { Buzzer_PlaySfx(&sfx_explosion); return MG_LOSE; }
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

    for(i = 0; i < FN_N; i++)
    {
        if(fn_on[i])
        {
            y = fn_y4[i] / 4 - 16;
            if(fn_dr[i])
            {
                DrawArt(fn_x[i] - 16, y, kFnArt[fn_k[i]], 16, 16, 2, FN_BG, true);          // new first...
                EraseOutside(fn_dx[i] - 16, fn_dy[i], 32, 32, fn_x[i] - 16, y, 32, 32, FN_BG); // ...then the leftovers
            }
            else DrawArt(fn_x[i] - 16, y, kFnArt[fn_k[i]], 16, 16, 2, FN_BG, true);
            fn_dx[i] = fn_x[i]; fn_dy[i] = y; fn_dr[i] = true;
        }
        else if(fn_dr[i]) { Box(fn_dx[i] - 16, fn_dy[i], 32, 32, FN_BG); fn_dr[i] = false; }
    }

    if(s_first || fn_cx != fn_pcx || fn_cy != fn_pcy)
    {
        Box(fn_cx - 7, fn_cy - 7, 14, 14, COLOR_WHITE);
        Box(fn_cx - 4, fn_cy - 4, 8, 8, COLOR_CHARCOAL);
        if(!s_first) EraseOutside(fn_pcx - 7, fn_pcy - 7, 14, 14, fn_cx - 7, fn_cy - 7, 14, 14, FN_BG);
        fn_pcx = fn_cx; fn_pcy = fn_cy;
    }

    if(fn_shown != fn_cnt)
    {
        for(i = 0; i < 4; i++)
            Box(10 + i * 22, 8, 16, 16, (int16_t)i < fn_cnt ? COLOR_GOLD : COLOR_DARK_GRAY);
        fn_shown = fn_cnt;
    }
}

// ============================================================================
// Shared pseudo-3D "behind the car / behind the runner" view for RACE! and
// ESCAPE!.  240x180 window, the road is drawn once; only small things move.
// Depth is a step k = 0 (horizon) .. 23 (right at the camera); objects advance
// in 1/16 steps (z16) and are redrawn only when k changes.
// ============================================================================
#define PS_W    240
#define PS_H    180
#define PS_HZ   62
#define PS_K    24
#define PS_CX   120

static int16_t ps_y[PS_K], ps_hw[PS_K], ps_t[PS_K];

static void PsInit(void)
{
    int16_t k;

    for(k = 0; k < PS_K; k++)
    {
        ps_t[k]  = (int16_t)(((int32_t)k * k * 64) / ((PS_K - 1) * (PS_K - 1)));
        ps_y[k]  = PS_HZ + (int16_t)(((int32_t)(PS_H - 1 - PS_HZ) * ps_t[k]) / 64);
        ps_hw[k] = 12 + (int16_t)(((int32_t)106 * ps_t[k]) / 64);
    }
}

static int16_t PsLaneX(int16_t lane, int16_t k) { return PS_CX + (lane - 1) * ((ps_hw[k] * 2) / 3); }

static void PsDrawRoad(uint32_t sky, uint32_t side, uint32_t road, uint32_t edge)
{
    int16_t y, hw;

    Box(0, 0, PS_W, PS_HZ, sky);
    Box(0, PS_HZ, PS_W, PS_H - PS_HZ, side);
    for(y = PS_HZ; y < PS_H; y += 3)
    {
        hw = 12 + (106 * (y - PS_HZ)) / (PS_H - 1 - PS_HZ);
        Box(PS_CX - hw - 3, y, 3, 3, edge);
        Box(PS_CX + hw,     y, 3, 3, edge);
        Box(PS_CX - hw,     y, hw * 2, 3, road);
    }
}

// ============================================================================
// RACE!  (Cruis'n USA)  behind-the-car arcade racer. Stick/L/R steers, hold the
//        button for nitro. Weave through traffic until the clock runs out.
// ============================================================================
#define RS_SKY   0x6EC6FFUL
#define RS_GRASS 0x2E8B2EUL
#define RS_ROAD  0x454B55UL
#define RS_N     7
#define RS_CY    (PS_H - 6)                 // player car bottom

static int16_t rs_px, rs_ppx, rs_cd, rs_sp, rs_ds, rs_dk[10], rs_dl;
static int16_t rs_z[RS_N], rs_l[RS_N], rs_pk[RS_N], rs_c[RS_N];
static bool    rs_on[RS_N], rs_dr[RS_N], rs_nitro;

static const uint32_t kRsCar[4] = { COLOR_YELLOW, COLOR_CYAN, COLOR_WHITE, COLOR_ORANGE };

static void RsInit(uint16_t spd)
{
    int16_t i;

    PsInit();
    rs_px = rs_ppx = PS_CX;
    rs_cd = 14;
    rs_sp = 10 + Min16((int16_t)spd, 4);
    rs_ds = 0;
    rs_nitro = false;
    for(i = 0; i < RS_N; i++) { rs_on[i] = rs_dr[i] = false; }
    for(i = 0; i < 10; i++) rs_dk[i] = -1;
}

static int16_t RsUpdate(void)
{
    int16_t i, k, dxs, step;

    dxs = (g_joy.dx * 7) / 100;
    if(g_dpad.l) dxs = -7;
    if(g_dpad.r) dxs =  7;
    rs_px = Clamp16(rs_px + dxs, 24, PS_W - 24);
    rs_nitro = g_joy.btn;
    step = rs_nitro ? rs_sp + 6 : rs_sp;
    rs_ds = (rs_ds + step) % (PS_K * 16);

    if(--rs_cd <= 0)
    {
        int16_t lane = Rnd(3), mask = 0, q;

        for(q = 0; q < RS_N; q++)                               // lanes with a car just spawned
            if(rs_on[q] && (rs_z[q] >> 4) <= 9) mask |= (1 << rs_l[q]);
        if((mask | (1 << lane)) != 7)                           // always leave a lane open
            for(i = 0; i < RS_N; i++)
                if(!rs_on[i]) { rs_on[i] = true; rs_z[i] = 16; rs_l[i] = lane; rs_c[i] = Rnd(4); rs_pk[i] = -1; break; }
        rs_cd = Max16(8, 15 + Rnd(10) - (rs_sp - 10));
    }
    for(i = 0; i < RS_N; i++)
    {
        if(!rs_on[i]) continue;
        rs_z[i] += step - 7;                                    // traffic drives, but slower than you
        k = rs_z[i] >> 4;
        if(k >= PS_K - 1) { rs_on[i] = false; continue; }
        if(k >= 20 && Abs16(PsLaneX(rs_l[i], k) - rs_px) < 30) return MG_LOSE;
    }
    return MG_PLAY;
}

static void RsCarRect(int16_t k, int16_t lane, int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    *w = 4 + (ps_t[k] * 32) / 64;
    *h = 3 + (ps_t[k] * 22) / 64;
    *x = PsLaneX(lane, k) - *w / 2;
    *y = ps_y[k] - *h;
}

static void RsRender(void)
{
    int16_t i, j, k, x, y, w, h, ox, oy, ow, oh, dw, dh;
    bool    moved;

    if(s_first)
    {
        PsDrawRoad(RS_SKY, RS_GRASS, RS_ROAD, COLOR_WHITE);
        Box(30, 40, 40, 22, 0x4F7F5FUL);   Box(110, 30, 56, 32, 0x4F7F5FUL);  Box(170, 44, 50, 18, 0x4F7F5FUL);
        Box(190, 14, 18, 18, COLOR_YELLOW);
        for(i = 0; i < 10; i++) rs_dk[i] = -1;
        for(i = 0; i < RS_N; i++) rs_pk[i] = -1;
        rs_ppx = -1;
    }

    // lane dashes: 5 per line, redrawn only when their depth step changes
    for(j = 0; j < 5; j++)
    {
        k = ((rs_ds + j * 80) % (PS_K * 16)) >> 4;
        if(rs_dk[j] != k)
        {
            if(rs_dk[j] >= 1)
            {
                dw = 1 + ps_t[rs_dk[j]] / 20;  dh = 2 + ps_t[rs_dk[j]] / 6;
                Box(PS_CX - ps_hw[rs_dk[j]] / 3 - dw / 2, ps_y[rs_dk[j]] - dh, dw, dh, RS_ROAD);
                Box(PS_CX + ps_hw[rs_dk[j]] / 3 - dw / 2, ps_y[rs_dk[j]] - dh, dw, dh, RS_ROAD);
            }
            if(k >= 1)
            {
                dw = 1 + ps_t[k] / 20;  dh = 2 + ps_t[k] / 6;
                Box(PS_CX - ps_hw[k] / 3 - dw / 2, ps_y[k] - dh, dw, dh, COLOR_WHITE);
                Box(PS_CX + ps_hw[k] / 3 - dw / 2, ps_y[k] - dh, dw, dh, COLOR_WHITE);
            }
            rs_dk[j] = k;
        }
    }

    for(i = 0; i < RS_N; i++)                                   // traffic
    {
        if(rs_on[i])
        {
            k = rs_z[i] >> 4;
            if(k != rs_pk[i])
            {
                RsCarRect(k, rs_l[i], &x, &y, &w, &h);
                Box(x, y, w, h, kRsCar[rs_c[i]]);
                if(w >= 10) { Box(x + w / 6, y + h / 5, (w * 2) / 3, h / 4, 0x203040UL);
                              Box(x + 1, y + h - h / 5, w / 5 + 1, h / 5 + 1, COLOR_RED);
                              Box(x + w - w / 5 - 2, y + h - h / 5, w / 5 + 1, h / 5 + 1, COLOR_RED); }
                if(rs_pk[i] >= 0)
                {
                    RsCarRect(rs_pk[i], rs_l[i], &ox, &oy, &ow, &oh);
                    EraseOutside(ox, oy, ow, oh, x, y, w, h, RS_ROAD);
                }
                rs_pk[i] = k;
                rs_dr[i] = true;
            }
        }
        else if(rs_dr[i] && rs_pk[i] >= 0)
        {
            RsCarRect(rs_pk[i], rs_l[i], &ox, &oy, &ow, &oh);
            Box(ox, oy, ow, oh, RS_ROAD);
            rs_dr[i] = false;
        }
    }

    moved = (rs_px != rs_ppx) || rs_nitro;                      // player car (rear view)
    if(!moved)                                                  // a dash/car passing underneath dirties it
        for(j = 0; j < 5; j++) if(rs_dk[j] >= 19) moved = true;
    if(moved)
    {
        Box(rs_px - 20, RS_CY - 26, 40, 5, COLOR_RED);                      // spoiler
        Box(rs_px - 18, RS_CY - 21, 36, 15, 0xE53935UL);                    // body
        Box(rs_px - 12, RS_CY - 20, 24, 7, 0x203040UL);                     // rear glass
        Box(rs_px - 20, RS_CY - 12, 7, 12, COLOR_BLACK);                    // wheels
        Box(rs_px + 13, RS_CY - 12, 7, 12, COLOR_BLACK);
        Box(rs_px - 16, RS_CY - 8, 6, 3, COLOR_ORANGE);                     // tail lights
        Box(rs_px + 10, RS_CY - 8, 6, 3, COLOR_ORANGE);
        if(rs_nitro) { Box(rs_px - 8, RS_CY, 5, 4, COLOR_ORANGE); Box(rs_px + 3, RS_CY, 5, 4, COLOR_ORANGE); }
        if(rs_ppx >= 0 && rs_px != rs_ppx)
            EraseOutside(rs_ppx - 20, RS_CY - 26, 40, 30, rs_px - 20, RS_CY - 26, 40, 30, RS_ROAD);
        else if(!rs_nitro) Box(rs_px - 8, RS_CY, 16, 4, RS_ROAD);
        rs_ppx = rs_px;
    }
}

// ============================================================================
// SURF!  (Subway Surfers)  run the rails. L/R switch lane, UP jumps the
//        barriers, DOWN slides under the signs, grab coins, and stay out of
//        the way of the trains. Survive.   (3D view, shared with RACE!)
// ============================================================================
#define SF_SKY   0x87CEEBUL
#define SF_SIDE  0x5E6270UL
#define SF_ROAD  0x7A6F66UL
#define SF_N     5

static int16_t sf_lane, sf_x, sf_px, sf_jy, sf_jvy, sf_slide, sf_cd, sf_sp, sf_pjy, sf_pslide, sf_pf;
static int16_t sf_z[SF_N], sf_l[SF_N], sf_ty[SF_N], sf_pk[SF_N], sf_c[SF_N];
static bool    sf_on[SF_N], sf_dr[SF_N];

static const uint32_t kSfTrain[3] = { 0x2E7D32UL, 0xD84315UL, 0x1565C0UL };

static int16_t SfX(int16_t lane) { return PsLaneX(lane, PS_K - 1); }

static void SfInit(uint16_t spd)
{
    int16_t i;

    PsInit();
    sf_lane = 1;  sf_x = sf_px = SfX(1);
    sf_jy = sf_jvy = 0;  sf_slide = 0;
    sf_pjy = 0;  sf_pslide = 0;  sf_pf = -1;
    sf_cd = 10;
    sf_sp = 12 + Min16((int16_t)spd, 4);
    for(i = 0; i < SF_N; i++) { sf_on[i] = sf_dr[i] = false; }
}

static int16_t SfUpdate(void)
{
    int16_t i, j, k, t, mask;

    if(EdgeL() && sf_lane > 0) { sf_lane--; Buzzer_PlaySfx(&sfx_blip); }
    if(EdgeR() && sf_lane < 2) { sf_lane++; Buzzer_PlaySfx(&sfx_blip); }
    if(sf_jy == 0 && sf_slide == 0 && (EdgeU() || g_joy.btnPressed)) { sf_jvy = 9; Buzzer_PlaySfx(&sfx_jump); }
    if(sf_jy == 0 && sf_jvy == 0 && EdgeD()) sf_slide = 18;

    if(sf_jvy != 0 || sf_jy > 0)
    {
        sf_jy += sf_jvy;  sf_jvy--;
        if(sf_jy <= 0) { sf_jy = 0; sf_jvy = 0; }
    }
    if(sf_slide > 0) sf_slide--;
    sf_x += Clamp16(SfX(sf_lane) - sf_x, -9, 9);

    if(--sf_cd <= 0)
    {
        for(i = 0; i < SF_N; i++)
            if(!sf_on[i])
            {
                t = Rnd(10);
                t = (t < 3) ? 0 : ((t < 5) ? 1 : ((t < 7) ? 2 : 3));            // train, barrier, sign, coin
                j = Rnd(3);
                mask = 0;                                                      // never wall off all 3 lanes
                for(k = 0; k < SF_N; k++)
                    if(sf_on[k] && sf_ty[k] != 3 && (sf_z[k] >> 4) <= 9) mask |= (1 << sf_l[k]);
                if(t != 3 && (mask | (1 << j)) == 7) t = 3;
                sf_on[i] = true; sf_ty[i] = t; sf_l[i] = j; sf_z[i] = 16; sf_pk[i] = -1; sf_dr[i] = false;
                sf_c[i] = Rnd(3);
                break;
            }
        sf_cd = Max16(9, 18 + Rnd(12) - 2 * (sf_sp - 12));
    }

    for(i = 0; i < SF_N; i++)
    {
        if(!sf_on[i]) continue;
        sf_z[i] += sf_sp;
        k = sf_z[i] >> 4;
        if(k >= PS_K - 1) { sf_on[i] = false; continue; }
        if(k >= 21 && sf_l[i] == sf_lane)
        {
            if(sf_ty[i] == 3)      { sf_on[i] = false; Buzzer_PlaySfx(&sfx_coin); }
            else if(sf_ty[i] == 0) return MG_LOSE;                              // train
            else if(sf_ty[i] == 1 && sf_jy < 12) return MG_LOSE;                // barrier
            else if(sf_ty[i] == 2 && sf_slide == 0) return MG_LOSE;             // overhead sign
        }
    }
    return MG_PLAY;
}

static void SfObjRect(int16_t i, int16_t k, int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    int16_t laneW = (ps_hw[k] * 2) / 3, t = ps_t[k], ty = sf_ty[i];

    *x = PsLaneX(sf_l[i], k);
    if(ty == 0)      { *w = (laneW * 9) / 10; *h = 6 + (t * 48) / 64; *y = ps_y[k] - *h; }
    else if(ty == 1) { *w = (laneW * 3) / 4;  *h = 2 + (t * 14) / 64; *y = ps_y[k] - *h; }
    else if(ty == 2) { *w = laneW;            *h = 2 + (t * 10) / 64; *y = ps_y[k] - (t * 40) / 64 - *h; }
    else             { *w = 2 + (t * 14) / 64; *h = *w;                *y = ps_y[k] - (t * 12) / 64 - *h; }
    *x -= *w / 2;
}

static void SfDrawObj(int16_t i, int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t ty = sf_ty[i];

    if(ty == 0)                                                 // subway train: body, windscreen, headlights
    {
        Box(x, y, w, h, kSfTrain[sf_c[i]]);
        if(w >= 12)
        {
            Box(x + w / 8, y + h / 8, (w * 3) / 4, h / 3, 0x203040UL);
            Box(x + w / 8, y + (h * 3) / 4, w / 6 + 1, h / 8 + 1, COLOR_YELLOW);
            Box(x + w - w / 8 - w / 6 - 1, y + (h * 3) / 4, w / 6 + 1, h / 8 + 1, COLOR_YELLOW);
        }
    }
    else if(ty == 1)                                            // red/white striped barrier
    {
        Box(x, y, w, h, COLOR_RED);
        Box(x + w / 4, y, w / 5 + 1, h, COLOR_WHITE);
        Box(x + (w * 3) / 5, y, w / 5 + 1, h, COLOR_WHITE);
    }
    else if(ty == 2)                                            // overhead sign
    {
        Box(x, y, w, h, 0x1565C0UL);
        if(h >= 6) Box(x + 2, y + h / 4, w - 4, h / 2, COLOR_WHITE);
    }
    else Box(x, y, w, h, 0xFFD700UL);                           // coin
}

// Repaint the railway (ballast, ties, rails) inside a rectangle. Used instead of a
// flat fill whenever something moves off the track, so the rails survive.
static void SfBoxIn(int16_t rx, int16_t ry, int16_t rw, int16_t rh, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c)
{
    int16_t x0 = Max16(rx, x), y0 = Max16(ry, y), x1 = Min16(rx + rw, x + w), y1 = Min16(ry + rh, y + h);

    if(x1 > x0 && y1 > y0) Box(x0, y0, x1 - x0, y1 - y0, c);
}

static void SfPaintTrack(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int16_t yy, hw, i, cx;

    if(y < PS_HZ) SfBoxIn(x, y, w, h, 0, 0, PS_W, PS_HZ, SF_SKY);
    for(yy = PS_HZ; yy < PS_H; yy += 3)                         // same 3 px bands PsDrawRoad() used
    {
        if(yy + 3 <= y || yy >= y + h) continue;
        hw = 12 + (106 * (yy - PS_HZ)) / (PS_H - 1 - PS_HZ);
        SfBoxIn(x, y, w, h, 0,               yy, PS_W,   3, SF_SIDE);
        SfBoxIn(x, y, w, h, PS_CX - hw - 3,  yy, 3,      3, 0xA0A0A8UL);
        SfBoxIn(x, y, w, h, PS_CX + hw,      yy, 3,      3, 0xA0A0A8UL);
        SfBoxIn(x, y, w, h, PS_CX - hw,      yy, hw * 2, 3, SF_ROAD);
    }
    for(yy = PS_HZ + 4; yy < PS_H; yy += 6)                     // ties and rails
    {
        if(yy + 6 <= y || yy >= y + h) continue;
        hw = 12 + (106 * (yy - PS_HZ)) / (PS_H - 1 - PS_HZ);
        SfBoxIn(x, y, w, h, PS_CX - hw, yy, hw * 2, 2, 0x4A4038UL);
        for(i = 0; i < 3; i++)
        {
            cx = PS_CX + (i - 1) * ((hw * 2) / 3);
            SfBoxIn(x, y, w, h, cx - hw / 10 - 1, yy, 2, 6, 0xC8C8D0UL);
            SfBoxIn(x, y, w, h, cx + hw / 10, yy, 2, 6, 0xC8C8D0UL);
        }
    }
}

// Like EraseOutside(), but the vacated pixels get the track repainted.
static void SfEraseOutside(int16_t ox, int16_t oy, int16_t ow, int16_t oh,
                           int16_t nx, int16_t ny, int16_t nw, int16_t nh)
{
    int16_t ix0 = Max16(ox, nx), iy0 = Max16(oy, ny);
    int16_t ix1 = Min16(ox + ow, nx + nw), iy1 = Min16(oy + oh, ny + nh);

    if(ix0 >= ix1 || iy0 >= iy1) { SfPaintTrack(ox, oy, ow, oh); return; }
    if(iy0 > oy)           SfPaintTrack(ox, oy, ow, iy0 - oy);
    if(iy1 < oy + oh)      SfPaintTrack(ox, iy1, ow, oy + oh - iy1);
    if(ix0 > ox)           SfPaintTrack(ox, iy0, ix0 - ox, iy1 - iy0);
    if(ix1 < ox + ow)      SfPaintTrack(ix1, iy0, ox + ow - ix1, iy1 - iy0);
}

static void SfRender(void)
{
    int16_t i, k, x, y, w, h, ox, oy, ow, oh, fy, pose, hw, cx;

    if(s_first)
    {
        PsDrawRoad(SF_SKY, SF_SIDE, SF_ROAD, 0xA0A0A8UL);
        Box(8, 28, 28, 34, 0x7A7A90UL);   Box(40, 38, 22, 24, 0x6A6A80UL);  Box(66, 22, 26, 40, 0x808098UL);   // skyline
        Box(150, 30, 30, 32, 0x7A7A90UL); Box(184, 40, 24, 22, 0x6A6A80UL); Box(212, 26, 22, 36, 0x808098UL);
        for(y = PS_HZ + 4; y < PS_H; y += 6)                                // railway: ties + 2 rails per lane
        {
            hw = 12 + (106 * (y - PS_HZ)) / (PS_H - 1 - PS_HZ);
            Box(PS_CX - hw, y, hw * 2, 2, 0x4A4038UL);
            for(i = 0; i < 3; i++)
            {
                cx = PS_CX + (i - 1) * ((hw * 2) / 3);
                Box(cx - hw / 10 - 1, y, 2, 6, 0xC8C8D0UL);
                Box(cx + hw / 10, y, 2, 6, 0xC8C8D0UL);
            }
        }
        for(i = 0; i < SF_N; i++) sf_pk[i] = -1;
        sf_pf = -1;
    }

    for(i = 0; i < SF_N; i++)                                   // objects: new first, then the leftovers
    {
        if(sf_on[i])
        {
            k = sf_z[i] >> 4;
            if(k != sf_pk[i])
            {
                SfObjRect(i, k, &x, &y, &w, &h);
                SfDrawObj(i, x, y, w, h);
                if(sf_pk[i] >= 0)
                {
                    SfObjRect(i, sf_pk[i], &ox, &oy, &ow, &oh);
                    SfEraseOutside(ox, oy, ow, oh, x, y, w, h);
                }
                sf_pk[i] = k;  sf_dr[i] = true;
            }
        }
        else if(sf_dr[i] && sf_pk[i] >= 0)
        {
            SfObjRect(i, sf_pk[i], &ox, &oy, &ow, &oh);
            SfPaintTrack(ox, oy, ow, oh);
            sf_dr[i] = false;
        }
    }

    fy = PS_H - 4 - sf_jy;
    pose = (s_frame >> 2) & 1;
    if(s_first || sf_x != sf_px || sf_jy != sf_pjy || (sf_slide > 0) != (sf_pslide > 0) || pose != sf_pf)
    {
        int16_t oldFy = PS_H - 4 - sf_pjy;
        int16_t rx0 = Min16(sf_px, sf_x) - 16, ry0 = Min16(oldFy, fy) - 46;

        SfPaintTrack(rx0, ry0, Abs16(sf_px - sf_x) + 34, Abs16(oldFy - fy) + 48);    // old spot back to track...
        if(sf_slide > 0)
        {
            Box(sf_x - 16, fy - 24, 32, 8, 0x1565C0UL);          // cap
            Box(sf_x - 14, fy - 16, 28, 16, 0xF57C00UL);         // sliding on his back
            Box(sf_x - 12, fy - 4, 24, 4, 0x3949ABUL);
        }
        else DrawArt(sf_x - 14, fy - 44, pose ? kArtRun1 : kArtRun0, 14, 22, 2, SF_ROAD, false);   // ...then him on top
        sf_px = sf_x;  sf_pjy = sf_jy;  sf_pslide = sf_slide;  sf_pf = pose;
    }
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

static void HdArrow(int16_t i)
{
    int16_t x = 160 - (hd_n - 1) * 22 + i * 44;

    Box(x - 14, 98, 28, 28, HD_BG);
    Arrow(x, 110, hd_seq[i], 4, i < hd_k ? COLOR_GOLD : COLOR_WHITE);
}

static void HdRender(void)
{
    int16_t i;

    if(s_first)
    {
        Text("STRATAGEM", 160, 40, COLOR_GOLD);
        Text("ENTER THE CODE", 160, 180, COLOR_GRAY);
        hd_shown = -1;
    }
    if(hd_shown != hd_k)
    {
        if(hd_shown < 0 || hd_shown > hd_k) { for(i = 0; i < hd_n; i++) HdArrow(i); }   // start / reset: all
        else                                { for(i = hd_shown; i < hd_k; i++) HdArrow(i); }  // else just the new ones
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
// CHOMP!  (Pac-Man)  a real maze. Steer (it keeps going, and turns when it can).
//         Eat 12 dots or a power pellet then catch the ghost.  Don't get caught.
//         210x182 window, 14 px cells; only changed cells are redrawn.
// ============================================================================
#define PM_C    14
#define PM_GW   15
#define PM_GH   13
#define PM_BG   COLOR_BLACK

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

static void PmInit(uint16_t spd)
{
    int16_t x, y;

    for(y = 0; y < PM_GH; y++)
        for(x = 0; x < PM_GW; x++)
            pm_d[y][x] = (kPmMap[y][x] == '.') ? 1 : ((kPmMap[y][x] == 'o') ? 2 : 0);
    pm_x = pm_ox = 1;  pm_y = pm_oy = 9;
    pm_dx = 1; pm_dy = 0; pm_ndx = 1; pm_ndy = 0;
    pm_pc = 5;  pm_eaten = 0;  pm_fr = 0;
    pm_gm = 9 - Min16((int16_t)spd / 3, 2);
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
        d = (pm_fr > 0) ? Rnd(100) : ((nx - tx) * (nx - tx) + (ny - ty) * (ny - ty));
        if(d < bd) { bd = d; bk = k; }
    }
    if(bk < 0)                                                  // dead end: turn around
    { g[2] = -g[2]; g[3] = -g[3]; bk = -1; }
    else { g[2] = dxs[bk]; g[3] = dys[bk]; }
    g[4] = g[0]; g[5] = g[1];
    g[0] += g[2]; g[1] += g[3];
}

static int16_t PmUpdate(void)
{
    int16_t hx = HeldX(), hy = HeldY(), gi;

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
            if(pm_eaten >= 12) return MG_WIN;
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
        Box(px, py, PM_C, PM_C, COLOR_NAVY);
        Box(px + 2, py + 2, PM_C - 4, PM_C - 4, COLOR_BLUE);
        return;
    }
    Box(px, py, PM_C, PM_C, PM_BG);
    if(pm_d[y][x] == 1)      Box(px + 6, py + 6, 3, 3, COLOR_WHITE);
    else if(pm_d[y][x] == 2) { Box(px + 4, py + 4, 7, 7, COLOR_WHITE); }
}

static void PmRender(void)
{
    int16_t x, y, gi, cx, cy;

    if(s_first)
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
    Box(cx - 4, cy - 6, 8, 12, COLOR_YELLOW);
    Box(cx - 6, cy - 4, 12, 8, COLOR_YELLOW);
    if(s_frame & 4)
        Box(cx + (pm_dx > 0 ? 0 : (pm_dx < 0 ? -6 : -2)), cy + (pm_dy > 0 ? 0 : (pm_dy < 0 ? -6 : -2)),
            pm_dx ? 6 : 4, pm_dy ? 6 : 4, PM_BG);

    for(gi = 0; gi < pm_ng; gi++)
    {
        cx = pm_g[gi][0] * PM_C + 1;  cy = pm_g[gi][1] * PM_C + 1;
        Box(cx + 1, cy, 10, 3, (pm_fr > 0) ? COLOR_BLUE : (gi ? COLOR_PINK : COLOR_RED));
        Box(cx, cy + 3, 12, 9, (pm_fr > 0) ? COLOR_BLUE : (gi ? COLOR_PINK : COLOR_RED));
        Box(cx + 2, cy + 4, 3, 3, COLOR_WHITE);  Box(cx + 7, cy + 4, 3, 3, COLOR_WHITE);
        Box(cx + 3, cy + 5, 1, 2, COLOR_BLACK);  Box(cx + 8, cy + 5, 1, 2, COLOR_BLACK);
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
//        Survive.  Cube and spikes are drawn "new first, then erase the
//        leftovers" so nothing blinks.
// ============================================================================
#define GD_GY   206
#define GD_X    70
#define GD_BG   0x1A0B3BUL

static int16_t gd_y, gd_vy, gd_py, gd_v, gd_cd;
static int16_t gd_ox[3], gd_opx[3], gd_ot[3];                   // 0 spike, 1 double spike, 2 block
static bool    gd_on[3], gd_dr[3], gd_gnd;

static int16_t GdW(int16_t t) { return (t == 0) ? 26 : ((t == 1) ? 52 : 30); }
static int16_t GdH(int16_t t) { return 24; }

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
        gd_cd = (140 + Rnd(90)) / gd_v;
    }
    for(i = 0; i < 3; i++)
        if(gd_on[i])
        {
            gd_ox[i] -= gd_v;
            if(gd_ox[i] < -60) gd_on[i] = false;
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
        else if(Hit(GD_X + 3, gd_y + 4, 14, 14, gd_ox[i] + 7, GD_GY - 16, GdW(gd_ot[i]) - 14, 14))
            return MG_LOSE;
    }

    if(bottom >= sup) { gd_y = sup - 20; gd_vy = 0; gd_gnd = true; }
    else gd_gnd = false;
    return MG_PLAY;
}

static void GdDrawCube(int16_t y)
{
    Box(GD_X, y, 20, 20, COLOR_YELLOW);
    Box(GD_X + 4, y + 5, 4, 4, COLOR_BLACK);
    Box(GD_X + 12, y + 5, 4, 4, COLOR_BLACK);
    Box(GD_X + 4, y + 13, 12, 3, COLOR_BLACK);
}

static void GdRender(void)
{
    uint16_t i;
    int16_t  y = GD_GY - 24;

    if(s_first)
    {
        Box(0, GD_GY, FIELD_W, FIELD_H - GD_GY, 0x6A1B9AUL);
        Box(0, GD_GY, FIELD_W, 3, 0xE040FBUL);
        gd_py = gd_y;
        GdDrawCube(gd_y);
    }

    for(i = 0; i < 3; i++)
    {
        if(gd_on[i])
        {
            if(gd_ot[i] == 2)
            {
                SlideX(y, 24, gd_opx[i], gd_ox[i], 30, COLOR_CYAN, GD_BG);
                if(s_first || gd_ox[i] != gd_opx[i]) Box(gd_ox[i], y, 30, 3, 0xB2EBF2UL);   // lit top edge
            }
            else
            {
                if(gd_ox[i] != gd_opx[i] || !gd_dr[i] || s_first)
                {
                    DrawArt(gd_ox[i], y, kArtGdSpike, 13, 12, 2, GD_BG, true);
                    if(gd_ot[i] == 1) DrawArt(gd_ox[i] + 26, y, kArtGdSpike, 13, 12, 2, GD_BG, true);
                    if(gd_dr[i] && !s_first)
                        EraseOutside(gd_opx[i], y, GdW(gd_ot[i]), 24, gd_ox[i], y, GdW(gd_ot[i]), 24, GD_BG);
                }
            }
            gd_opx[i] = gd_ox[i];
            gd_dr[i]  = true;
        }
        else if(gd_dr[i]) { Box(gd_opx[i], y, GdW(gd_ot[i]), 24, GD_BG); gd_dr[i] = false; }
    }

    if(gd_y != gd_py)                                           // cube: new first, then the strip it left
    {
        GdDrawCube(gd_y);
        EraseOutside(GD_X, gd_py, 20, 20, GD_X, gd_y, 20, 20, GD_BG);
        gd_py = gd_y;
    }
}

// ============================================================================
// LAUNCH!  (Angry Birds)  pull the STICK back like a slingshot: the direction
//          is the angle, how far you pull is the power. A dotted preview shows
//          the shot. Press D-pad DOWN (or click the stick) to let go.
// ============================================================================
#define BD_GY       200
#define BD_SX       44
#define BD_SY       168
#define BD_BG       0x87CEEBUL
#define BD_DOTS     8

static int16_t bd_ax, bd_ay, bd_pig, bd_dx, bd_dy, bd_bx, bd_by;
static int16_t bd_dotx[BD_DOTS], bd_doty[BD_DOTS];
static bool    bd_fly, bd_aimDirty, bd_pigAlive, bd_dotsDr;
static int32_t bd_x, bd_y, bd_vx, bd_vy;

static void BdInit(uint16_t spd)
{
    bd_ax = bd_ay = 0;
    bd_pig = 190 + Rnd(90);
    bd_fly = false;
    bd_aimDirty = true;
    bd_pigAlive = true;
    bd_dotsDr = false;
    bd_dx = bd_bx = BD_SX;
    bd_dy = bd_by = BD_SY;
}

static int16_t BdUpdate(void)
{
    if(!bd_fly)
    {
        if(g_joy.dx != bd_ax || g_joy.dy != bd_ay) { bd_ax = g_joy.dx; bd_ay = g_joy.dy; bd_aimDirty = true; }

        if((g_dpad.dPressed || g_joy.btnPressed) && (Abs16(bd_ax) + Abs16(bd_ay)) >= 15)
        {
            bd_vx = -(int32_t)bd_ax * 9;                        // pull left/down -> fly right/up
            bd_vy = -(int32_t)bd_ay * 9;
            bd_x  = (int32_t)(BD_SX + bd_ax / 4) * 100;
            bd_y  = (int32_t)(BD_SY + bd_ay / 4) * 100;
            bd_fly = true;
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

static void BdDrawBird(int16_t x, int16_t y)                    // 12x12, plain rects so moves are exact
{
    Box(x - 6, y - 6, 12, 12, COLOR_RED);
    Box(x + 1, y - 3, 4, 4, COLOR_WHITE);
    Box(x + 3, y - 2, 2, 2, COLOR_BLACK);
    Box(x + 5, y + 1, 3, 3, COLOR_YELLOW);
    Box(x - 1, y - 5, 6, 2, COLOR_BLACK);
}

static void BdRender(void)
{
    int16_t i, t, nx, ny;

    if(s_first)
    {
        Box(0, BD_GY, FIELD_W, FIELD_H - BD_GY, 0x2E7D32UL);
        Box(BD_SX - 4, BD_SY + 14, 6, BD_GY - BD_SY - 14, COLOR_BROWN);          // slingshot post
        bd_dotsDr = false;
        bd_aimDirty = true;
        if(bd_pigAlive) BdDrawPig();
    }

    if(!bd_fly)
    {
        if(bd_aimDirty)
        {
            if(bd_dotsDr)                                       // erase old preview dots
                for(i = 0; i < BD_DOTS; i++) Box(bd_dotx[i], bd_doty[i], 3, 3, BD_BG);
            nx = BD_SX + bd_ax / 4;  ny = BD_SY + bd_ay / 4;
            BdDrawBird(nx, ny);
            if(!s_first) EraseOutside(bd_bx - 6, bd_by - 6, 12, 12, nx - 6, ny - 6, 12, 12, BD_BG);
            bd_bx = nx; bd_by = ny;

            for(i = 0; i < BD_DOTS; i++)                        // new preview: where the bird will be
            {
                t = (i + 1) * 5;
                bd_dotx[i] = nx + (int16_t)(((int32_t)(-bd_ax) * 9 * t) / 100);
                bd_doty[i] = ny + (int16_t)(((int32_t)(-bd_ay) * 9 * t) / 100 + (int32_t)12 * t * (t + 1) / 100);
                if(bd_doty[i] > BD_GY - 4 || bd_dotx[i] < 0 || bd_dotx[i] > FIELD_W - 3) bd_doty[i] = -10;
                else Box(bd_dotx[i], bd_doty[i], 3, 3, COLOR_WHITE);
            }
            bd_dotsDr = true;
            bd_aimDirty = false;
        }
    }
    else
    {
        if(bd_dotsDr)
        {
            for(i = 0; i < BD_DOTS; i++) Box(bd_dotx[i], bd_doty[i], 3, 3, BD_BG);
            bd_dotsDr = false;
        }
        if(bd_pigAlive) BdDrawPig();
        nx = bd_dx;  ny = Min16(bd_dy, BD_GY - 8);
        BdDrawBird(nx, ny);
        EraseOutside(bd_bx - 6, bd_by - 6, 12, 12, nx - 6, ny - 6, 12, 12, BD_BG);
        bd_bx = nx;  bd_by = ny;
    }

    if(!bd_pigAlive) Box(bd_pig, BD_GY - 24, 24, 24, BD_BG);
}

// ============================================================================
// BEAT!  (Melatonin)  rhythm. "Melatonin Groove" plays (songs.c: 28 bars, one
//        song unit = an eighth note = 150 ms, so a quarter-note beat is 300 ms).
//        Each note flies in and lands on the ring exactly when that note of the
//        song sounds - press any button ON it. Notes are placed from the song's
//        own clock, not frame counts, so they stay in time however fast the
//        screen is. Hit 6 of 8.
//
//        kMlUnit[] is the chart: the song-unit number of each note to hit. Unit n
//        sounds n * 150 ms after the song starts (unit 8 = the G on bar 2 beat 1).
//        To re-chart, pick any non-rest notes of song_melatonin.
// ============================================================================
#define ML_BG       0x2B1B4DUL
#define ML_Z        70
#define ML_N        8
#define ML_UNIT_MS  150                     // = song_melatonin.eighthMs
#define ML_WIN_MS   100                     // +/- this counts as "on the beat"
#define ML_NEED     6

static const int16_t kMlUnit[ML_N] = { 8, 10, 13, 16, 18, 21, 24, 26 };

static int16_t ml_res[ML_N], ml_px[ML_N], ml_hits, ml_miss, ml_flash, ml_v, ml_bdr;
static uint32_t ml_t0;
static bool    ml_started, ml_dr[ML_N];

static int32_t MlNow(void)                  // ms since the song started (0 until it has)
{
    return ml_started ? (int32_t)MsSince(ml_t0) : 0;
}

static int16_t MlX(int16_t i, int32_t now)  // note x: 0.2 px per ms of lead time
{
    int32_t lead = (int32_t)kMlUnit[i] * ML_UNIT_MS - now;
    int32_t x = ML_Z + (lead * 13) / 64;
    return (x > 340) ? 340 : (int16_t)x;
}

static void MlInit(uint16_t spd)
{
    int16_t i;

    ml_hits = ml_miss = ml_flash = 0;
    ml_bdr = -1;
    ml_started = false;
    for(i = 0; i < ML_N; i++) { ml_res[i] = 0; ml_px[i] = 340; ml_dr[i] = false; }
}

static int16_t MlUpdate(void)
{
    int16_t i, best = -1, bd = 9999, d, resolved = 0;
    int32_t now;

    if(!ml_started && s_frame >= 2)                             // start the music after the first screen is up
    {
        Buzzer_PlaySong(&song_melatonin, 1);
        ml_t0 = TimerNow();
        ml_started = true;
    }
    if(!ml_started) return MG_PLAY;
    now = MlNow();
    if(ml_flash > 0) ml_flash--;

    for(i = 0; i < ML_N; i++)
    {
        d = (int16_t)(now - (int32_t)kMlUnit[i] * ML_UNIT_MS);          // + = note is late
        if(ml_res[i] == 0 && d > ML_WIN_MS)
        {
            ml_res[i] = 2;
            if(++ml_miss >= ML_N - ML_NEED + 1) return MG_LOSE;
        }
        if(ml_res[i] != 0) resolved++;
    }

    if(AnyPress())
    {
        for(i = 0; i < ML_N; i++)
        {
            d = Abs16((int16_t)(now - (int32_t)kMlUnit[i] * ML_UNIT_MS));
            if(ml_res[i] == 0 && d <= ML_WIN_MS && d < bd) { bd = d; best = i; }
        }
        if(best >= 0)
        {
            ml_res[best] = 1;  resolved++;
            ml_flash = 6;
            if(++ml_hits >= ML_NEED) return MG_WIN;
        }
    }
    if(resolved >= ML_N) return (ml_hits >= ML_NEED) ? MG_WIN : MG_LOSE;
    return MG_PLAY;
}

static void MlRing(uint32_t c)
{
    Box(ML_Z - 22, 93, 44, 3, c);   Box(ML_Z - 22, 134, 44, 3, c);
    Box(ML_Z - 22, 93, 3, 44, c);   Box(ML_Z + 19, 93, 3, 44, c);
}

static void MlRender(void)
{
    int16_t  i, x, b;
    int32_t  now = MlNow();
    bool     nearRing = false;
    static bool flashDr = false;

    if(s_first)
    {
        Text("PRESS ON THE NOTE", 160, 206, COLOR_GRAY);
        MlRing(COLOR_WHITE);  flashDr = false;  ml_bdr = -1;
    }

    for(i = 0; i < ML_N; i++)
    {
        x = MlX(i, now);
        if(ml_res[i] == 0 && x < 340)
        {
            if(ml_dr[i]) MoveArt(ml_px[i] - 12, 103, x - 12, 103, kArtNote, 12, 12, 2, ML_BG);
            else         DrawArt(x - 12, 103, kArtNote, 12, 12, 2, ML_BG, true);
            ml_px[i] = x;  ml_dr[i] = true;
            if(Abs16(x - ML_Z) < 40) nearRing = true;
        }
        else if(ml_dr[i])
        {
            Box(ml_px[i] - 12, 103, 24, 24, ML_BG);
            ml_dr[i] = false;
            nearRing = true;
        }
    }
    if(nearRing || (ml_flash > 0) != flashDr)                   // ring only when a note touched it / flash changed
    {
        MlRing(ml_flash ? COLOR_LIME : COLOR_WHITE);
        flashDr = (ml_flash > 0);
    }

    if(ml_started)                                              // 4 beat lights show where you are in the bar
    {
        b = (int16_t)((now / (ML_UNIT_MS * 2)) & 3);                   // quarter-note beat within the bar
        if(b != ml_bdr)
        {
            for(i = 0; i < 4; i++) Box(110 + i * 28, 170, 20, 12, i == b ? COLOR_YELLOW : 0x4A3A70UL);
            ml_bdr = b;
        }
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
        for(i = 0; i < 5; i++)
            Box(100 + i * 24, 12, 16, 16, i < rc_n ? COLOR_LIME : COLOR_DARK_GRAY);
        rc_shown = rc_n;
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
    int16_t i, j, sway = (Tri(s_frame, 90, 22) >> 1) << 1;

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

    for(i = 0; i < 10; i++)
    {
        if(!gl_al[i])
        {
            if(gl_dr[i]) { Box(gl_px[i] - 4, gl_py[i], 26, 12, GL_BG); gl_dr[i] = false; }
            continue;
        }
        if(gl_dr[i] && gl_px[i] == gl_ex[i] && gl_py[i] == gl_ey[i]) continue;        // hasn't moved
        if(gl_dr[i]) Box(gl_px[i] - 4, gl_py[i], 26, 12, GL_BG);                       // erase+draw back to back
        Box(gl_ex[i], gl_ey[i] + 2, 18, 8, (i < 5) ? COLOR_YELLOW : COLOR_CYAN);
        Box(gl_ex[i] - 4, gl_ey[i], 5, 10, COLOR_RED);
        Box(gl_ex[i] + 17, gl_ey[i], 5, 10, COLOR_RED);
        Box(gl_ex[i] + 4, gl_ey[i] + 4, 3, 3, COLOR_BLACK);
        Box(gl_ex[i] + 11, gl_ey[i] + 4, 3, 3, COLOR_BLACK);
        gl_px[i] = gl_ex[i]; gl_py[i] = gl_ey[i]; gl_dr[i] = true;
    }
    for(i = 0; i < 2; i++)
    {
        if(gl_bdr[i]) { Box(gl_pbx[i], gl_pby[i], 3, 8, GL_BG); gl_bdr[i] = false; }
        if(gl_bon[i])
        {
            Box(gl_bx[i], gl_by[i], 3, 8, COLOR_WHITE);
            gl_pbx[i] = gl_bx[i]; gl_pby[i] = gl_by[i]; gl_bdr[i] = true;
        }
    }
    if(s_first || gl_sx != gl_psx)
    {
        Box(gl_psx, GL_SY - 7, 26, 20, GL_BG);
        Box(gl_sx, GL_SY, 26, 12, COLOR_WHITE);
        Box(gl_sx + 11, GL_SY - 6, 4, 6, COLOR_WHITE);
        Box(gl_sx, GL_SY + 4, 4, 8, COLOR_RED);  Box(gl_sx + 22, GL_SY + 4, 4, 8, COLOR_RED);
        gl_psx = gl_sx;
    }
}

//*****************************************************************************
// 3. GAME TABLE  <- add a microgame: one line here
//      cmd, inspiration tag, frames, background, winOnTimeout, init, update, render
//*****************************************************************************
#define WFULL 0, 0, FIELD_W, FIELD_H
static const MicroGame kGames[] =
{
    { "CLIMB!",    "Donkey Kong",     260, DK_BG, WFULL,              0, DkInit, DkUpdate, DkRender },
    { "BATTLE!",   "Pokemon",         220, PK_BG, WFULL,              0, PkInit, PkUpdate, PkRender },
    { "FLIP!",     "Pokemon Pinball", 340, PB_BG, 60, 12, 200, 200,   1, PbInit, PbUpdate, PbRender },
    { "RUN!",      "Sonic",           280, SO_BG, 0, 40, 320, 160,    0, SoInit, SoUpdate, SoRender },
    { "PLANT!",    "Plants vs Zombies", 360, PZ_BG0, 0, 24, 320, 168, 0, PzInit, PzUpdate, PzRender },
    { "BLAST!",    "Metroid",         340, MT_BG, WFULL,              0, MtInit, MtUpdate, MtRender },
    { "SLICE!",    "Fruit Ninja",     280, FN_BG, WFULL,              0, FnInit, FnUpdate, FnRender },
    { "RACE!",     "Cruis'n USA",     190, RS_SKY, 40, 24, 240, 180,  1, RsInit, RsUpdate, RsRender },
    { "SURF!",     "Subway Surfers",  230, SF_SKY, 40, 24, 240, 180,  1, SfInit, SfUpdate, SfRender },
    { "CALL IT!",  "Helldivers 2",    170, HD_BG, WFULL,              0, HdInit, HdUpdate, HdRender },
    { "BREAK!",    "Brick Breaker",   280, BK_BG, WFULL,              0, BkInit, BkUpdate, BkRender },
    { "FIT IT!",   "Tetris",          240, TT_BG, WFULL,              0, TtInit, TtUpdate, TtRender },
    { "CHOMP!",    "Pac-Man",         330, PM_BG, 55, 24, 210, 182,   0, PmInit, PmUpdate, PmRender },
    { "CROSS!",    "Frogger",         280, FR_BG, WFULL,              0, FrInit, FrUpdate, FrRender },
    { "FLAP!",     "Flappy Bird",     200, FB_BG, WFULL,              1, FbInit, FbUpdate, FbRender },
    { "JUMP!",     "Chrome Dino",     180, DN_BG, WFULL,              1, DnInit, DnUpdate, DnRender },
    { "STOMP!",    "Super Mario",     260, MR_BG, WFULL,              0, MrInit, MrUpdate, MrRender },
    { "DASH!",     "Geometry Dash",   200, GD_BG, WFULL,              1, GdInit, GdUpdate, GdRender },
    { "LAUNCH!",   "Angry Birds",     280, BD_BG, WFULL,              0, BdInit, BdUpdate, BdRender },
    { "BEAT!",     "Melatonin",       480, ML_BG, WFULL,              0, MlInit, MlUpdate, MlRender },
    { "SORT!",     "Recycler",        220, RC_BG, WFULL,              0, RcInit, RcUpdate, RcRender },
    { "RETURN!",   "Pong",            300, PG_BG, WFULL,              0, PgInit, PgUpdate, PgRender },
    { "SHOOT!",    "Galaga",          320, GL_BG, WFULL,              0, GlInit, GlUpdate, GlRender },
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
static bool     s_repaintAll;                       // Settings overlay wiped the screen
static bool     s_practice;                         // practice mode: no lives, no score
static int16_t  s_menu, s_sel, s_pLevel;            // title cursor, picker cursor, practice speed
static int16_t  s_barPx;
static bool     s_barRed;

// Black out everything on the playfield outside a (smaller) render window, so the
// intro banner and text don't show around it. Costs only the area it covers.
static void ClearOutside(int16_t wx, int16_t wy, int16_t ww, int16_t wh)
{
    if(wy > 0)                 Graphics_FillRect(0, 0, FIELD_W - 1, wy - 1, COLOR_BLACK);
    if(wy + wh < FIELD_H)      Graphics_FillRect(0, wy + wh, FIELD_W - 1, FIELD_H - 1, COLOR_BLACK);
    if(wx > 0)                 Graphics_FillRect(0, wy, wx - 1, wy + wh - 1, COLOR_BLACK);
    if(wx + ww < FIELD_W)      Graphics_FillRect(wx + ww, wy, FIELD_W - 1, wy + wh - 1, COLOR_BLACK);
}

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

// ---- intro taunt: angry Waluigi-coloured Wario -------------------------------
//   "You'll never get to 100!"  typed out one letter at a time with a low blip per
//   letter (Sans-style), his mouth flapping with each one.
//
//   ALL of his picture lives in TauntDrawFace() / TauntDrawMouth(). To use a real
//   image later: replace the DrawArt() call in TauntDrawFace() with
//       Graphics_drawImage(&g_sContext, &yourImage, FACE_X, FACE_Y);
//   and keep (or delete) the TauntDrawMouth() calls for the flapping jaw.
#define TAUNT_LEN   24                      // 12 + space + 11 letters
#define FACE_X      16
#define FACE_Y      116
#define FACE_S      4                       // 24x24 art drawn at 4x = 96x96
#define MOUTH_X     (FACE_X + 7 * FACE_S)   // the 10x5 mouth art sits on his chin
#define MOUTH_Y     (FACE_Y + 19 * FACE_S)
#define TAUNT_BG    0x150A22UL

static const char kTauntA[] = "You'll never";
static const char kTauntB[] = "get to 100!";
static uint16_t   s_tChars, s_tHold, s_tPart;       // letters typed, hold timer, 1 = text dirty, 2 = mouth dirty
static bool       s_tOpen;

static void TauntDrawFace(void)
{
    DrawArt(FACE_X, FACE_Y, kArtWario, 24, 24, FACE_S, TAUNT_BG, true);
}

static void TauntDrawMouth(bool open)
{
    DrawArt(MOUTH_X, MOUTH_Y, open ? kArtMouthO : kArtMouthC, 10, 5, FACE_S, TAUNT_BG, true);
}

static void TauntDrawText(void)
{
    char    a[16], b[16];
    int16_t i, la = Min16((int16_t)s_tChars, 12), lb = Max16(0, (int16_t)s_tChars - 13);

    for(i = 0; i < la; i++) a[i] = kTauntA[i];
    a[la] = 0;
    for(i = 0; i < lb; i++) b[i] = kTauntB[i];
    b[lb] = 0;

    Graphics_FillRect(130, 40, 306, 70, COLOR_WHITE);
    Graphics_FillRect(130, 76, 306, 106, COLOR_WHITE);
    Graphics_setForegroundColor(&g_sContext, COLOR_BLACK);
    if(la) Graphics_drawString(&g_sContext, (int16_t *)a, AUTO_STRING_LENGTH, 136, 44, TRANSPARENT_TEXT);
    if(lb) Graphics_drawString(&g_sContext, (int16_t *)b, AUTO_STRING_LENGTH, 136, 80, TRANSPARENT_TEXT);
}

static void DrawTaunt(void)
{
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, TAUNT_BG);
    TauntDrawFace();
    TauntDrawMouth(s_tOpen);
    Graphics_FillRect(122, 20, 316, 122, COLOR_BLACK);                  // speech bubble + tail
    Graphics_FillRect(125, 23, 313, 119, COLOR_WHITE);
    Graphics_FillRect(150, 119, 176, 124, COLOR_BLACK);
    Graphics_FillRect(153, 119, 173, 123, COLOR_WHITE);
    Graphics_FillRect(154, 124, 166, 128, COLOR_BLACK);
    Graphics_FillRect(156, 124, 164, 127, COLOR_WHITE);
    Graphics_FillRect(156, 128, 160, 131, COLOR_BLACK);
    TauntDrawText();
    Text("button: skip", FIELD_W / 2, 230, COLOR_GRAY);
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
    Text("23 microgames. 4 lives. Go!", FIELD_W / 2, 105, COLOR_WHITE);

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
    s_tChars = 0; s_tHold = 0; s_tPart = 0; s_tOpen = false;
    s_lives = WW_LIVES; s_score = 0; s_played = 0; s_level = 0;
    s_speedUp = 0; s_newBest = false;
    s_bag[NUM_GAMES - 1] = 0xFFFF; s_bagPos = NUM_GAMES;
    Rand_Seed(TimerNow() + 12345UL);
    PickGame();
    Buzzer_PlaySfx(&sfx_menu_select);
    GoPhase(PH_TAUNT);                              // Wario sneers first, then the first microgame
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
    Buzzer_Stop();                                  // music (Melatonin) off before the jingle
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

        case PH_TAUNT:
            if(g_joy.btnPressed || g_dpad.dPressed) { GoPhase(PH_INTRO); break; }
            if(s_tChars < TAUNT_LEN)
            {
                if((s_t % 4) == 0)
                {
                    s_tChars++;
                    if(s_tChars != 13) { Buzzer_PlaySfx(&sfx_talk); s_tOpen = !s_tOpen; }   // letter: blip + flap
                    else s_tOpen = false;                                                    // the space
                    s_tPart |= 3;
                }
            }
            else
            {
                if(s_tOpen) { s_tOpen = false; s_tPart |= 2; }
                if(++s_tHold > 50) GoPhase(PH_INTRO);
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

        case PH_TAUNT:
            if(s_dirty) { DrawTaunt(); s_dirty = false; s_tPart = 0; }
            else if(s_tPart)
            {
                Graphics_setFont(&g_sContext, &g_sFontCmss20b);
                if(s_tPart & 1) TauntDrawText();
                if(s_tPart & 2) TauntDrawMouth(s_tOpen);
                s_tPart = 0;
            }
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
            g_wx = g->wx;  g_wy = g->wy;  g_ww = g->ww;  g_wh = g->wh;
            if(s_dirty)
            {
                ClearOutside(g_wx, g_wy, g_ww, g_wh);
                Graphics_FillRect(g_wx, g_wy, g_wx + g_ww - 1, g_wy + g_wh - 1, g->bg);
                BarFull();
                s_first = true;
                s_dirty = false;
                s_repaintAll = false;
            }
            g->render();
            s_first = false;
            g_wx = 0;  g_wy = 0;  g_ww = FIELD_W;  g_wh = FIELD_H;
            BarUpdate();
            break;

        case PH_RESULT:
            if(s_dirty)
            {
                Graphics_setFont(&g_sContext, &g_sFontCmss20b);
                if(s_repaintAll)                    // only after Settings wiped the screen
                {
                    g_wx = g->wx;  g_wy = g->wy;  g_ww = g->ww;  g_wh = g->wh;
                    ClearOutside(g_wx, g_wy, g_ww, g_wh);
                    Graphics_FillRect(g_wx, g_wy, g_wx + g_ww - 1, g_wy + g_wh - 1, g->bg);
                    s_first = true;
                    g->render();
                    s_first = false;
                    g_wx = 0;  g_wy = 0;  g_ww = FIELD_W;  g_wh = FIELD_H;
                    s_repaintAll = false;
                }
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
    s_repaintAll = true;
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
    "23 microgames, 4 lives",               // hint shown under the menu
    COLOR_MAGENTA,                          // menu tile color
    WW_FRAME_US,                            // frame period
    WW_Init, WW_Update, WW_Render,
    WW_Exit,
    NULL,                                   // not a legacy app
    WW_Redraw
};
