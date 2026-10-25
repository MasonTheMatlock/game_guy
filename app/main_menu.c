//#############################################################################
// FILE:   app/main_menu.c
// TITLE:  Main menu - smooth horizontal tile carousel (incremental redraw)
//
// The menu is an App like any other: Menu_Init / Menu_Update / Menu_Render,
// driven by RunApp() in main.c. When the player picks a tile, Menu_Update()
// calls App_Launch(g_apps[sel]) and returns APP_QUIT; main.c then runs that
// app.
//
// WHY THIS VERSION IS FASTER
//   The SPI LCD is the bottleneck: every pixel costs 16 bits on the wire.
//   The old code repainted the whole 320x96 strip (30,720 px) every frame,
//   then the pager (6k px) and a 320x72 label area (23k px) when it settled.
//
//   Now:
//     * While scrolling, a tile is a SOLID rectangle that moves. Only the
//       columns it newly covers (leading edge) and newly uncovers (trailing
//       edge) are painted: about 2 * dx * 96 px per tile per frame.
//     * Tile colors do not change while moving (no per-frame fade). The
//       highlight (bright tile + big letters) is handed over on arrival.
//     * The big letters are erased once when a move starts (one small fill
//       per tile) and redrawn on arrival.
//     * Animation is a fixed number of frames with an ease-out curve, so
//       the duration no longer depends on how far the tiles have to go.
//     * Pager: only the old and new dot are touched.
//     * Labels: the old text is erased by redrawing it in the background
//       color, instead of clearing a 320x72 block. The static reminder text
//       is drawn once in Menu_DrawStatic().
//
// CONTROLS
//   stick left/right, D-pad L/R : scroll (hold to auto-repeat)
//   stick button, D-pad D       : start the highlighted app
//
// BACKGROUND COLOR
//   Chosen in Settings > Apps > Menu and saved in flash (SAVE_SLOT_MENU).
//   Menu_Init() loads it into s_bg; every fill that used to be COLOR_BLACK
//   now uses s_bg. Menu_Exit() restores the black drawing background that
//   the other apps expect.
//#############################################################################

#include "main_menu.h"   // g_menuApp (pulls in apps.h)

//*****************************************************************************
// Layout (pixels). Screen is 320 x 240.
//*****************************************************************************
#define MENU_TILE_W        96
#define MENU_TILE_H        96
#define MENU_TILE_GAP      14
#define MENU_PITCH         (MENU_TILE_W + MENU_TILE_GAP)

#define MENU_TILE_Y        62
#define MENU_CENTER_X      (SCREEN_W / 2)

#define MENU_FRAME_PAD     4
#define MENU_FRAME_THICK   3

#define MENU_DOTS_Y        42
#define MENU_DOT_STEP      28
#define MENU_MAX_DOTS      16

#define MENU_NAME_Y        182
#define MENU_HINT_Y        206
#define MENU_REMINDER_Y    228

#define MENU_GLYPH_SCALE   7
#define MENU_GLYPH_W       (5 * MENU_GLYPH_SCALE)               // 35
#define MENU_GLYPH_H       (7 * MENU_GLYPH_SCALE)               // 49
#define MENU_GLYPH_DX      ((MENU_TILE_W - MENU_GLYPH_W) / 2)   // inside tile
#define MENU_GLYPH_DY      ((MENU_TILE_H - MENU_GLYPH_H) / 2)

// Tile brightness (256 = the app's full color).
// Setting MENU_DIM_BRIGHT equal to MENU_LIT_BRIGHT removes the hand-over
// repaint of two 96x96 tiles when a move ends (the frame still marks the
// selection). It is the biggest single saving left if you need more speed.
#define MENU_LIT_BRIGHT    256      // selected tile
#define MENU_DIM_BRIGHT    56       // every other tile

//*****************************************************************************
// Feel / animation
//*****************************************************************************
// Scroll time ~= MENU_ANIM_FRAMES * (time to render one frame + MENU_FRAME_US).
// Fewer frames = faster but choppier. Start at 10 and tune on the hardware.
#define MENU_ANIM_FRAMES   10

#define MENU_FRAME_US      2000uL   // frame pause (was 9000); SPI sets the pace
#define MENU_DAS_FRAMES    6        // auto-repeat: delay before repeating
#define MENU_ARR_FRAMES    2        // auto-repeat: frames between repeats

#define MENU_WRAP          0
#define MENU_PULSE         1

// 1: erase old label text by redrawing it in the background color (fewest
//    pixels; exact for 1-bpp fonts). 0: clear the two text rows instead
//    (use this if you ever see ghost pixels around the text).
#define MENU_LABEL_ERASE_BY_REDRAW   1

//*****************************************************************************
// Controls
//*****************************************************************************
#define MENU_LEFT_HELD     (g_dpad.l)
#define MENU_RIGHT_HELD    (g_dpad.r)
#define MENU_LEFT_TAP      (g_dpad.lPressed)
#define MENU_RIGHT_TAP     (g_dpad.rPressed)
#define MENU_SELECT_TAP    (g_dpad.dPressed)

//*****************************************************************************
// Tiny 5x7 font for the big tile letters.
//*****************************************************************************
static const uint16_t g_glyphs[37][5] =
{
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},   // A B
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},   // C D
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},   // E F
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},   // G H
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},   // I J
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},   // K L
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},   // M N
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},   // O P
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},   // Q R
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},   // S T
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},   // U V
    {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},   // W X
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},   // Y Z

    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},   // 0 1
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},   // 2 3
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},   // 4 5
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},   // 6 7
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E},   // 8 9

    {0x02,0x01,0x51,0x09,0x06}                                // ?
};

//*****************************************************************************
// Menu state
//*****************************************************************************
static int16_t  s_sel = 0;          // selected app; survives between launches
static int32_t  s_pos;              // scroll position the animation has reached
static int32_t  s_drawnPos;         // scroll position currently on the LCD
static int32_t  s_from;             // scroll position the animation started at
static int32_t  s_target;           // scroll position we are animating toward
static int16_t  s_animF;            // animation frame counter
static int16_t  s_hTimer;           // auto-repeat countdown
static int16_t  s_pStep;            // selection-frame pulse phase 0..15
static uint16_t s_tick;
static uint32_t s_bg = 0x000000UL;  // menu background color (a Menu_BgColor() entry)

static int16_t  s_lit;              // tile currently painted bright
static int16_t  s_lblSel;           // app whose name/hint is on screen (-1 none)
static int16_t  s_pgSel;            // dot currently highlighted (-1 none)
static bool     s_glyphsOn;         // big letters currently on screen
static bool     s_needSettle;       // selection changed: hand over on arrival
static bool     s_drawFrame;        // whole selection frame needs redrawing
static bool     s_drawSides;        // strip moved: only the side bars were hit

//*****************************************************************************
// Background color: palette + save access (see main_menu.h)
//   Keep the colors DARK: the white title and gray labels must stay readable.
//*****************************************************************************
static const uint32_t s_bgColors[MENU_NUM_BGS] =
{
    0x000000UL,         // Black (default)
    0x0A1A4AUL,         // Navy
    0x0C3A1CUL,         // Forest
    0x4A0C14UL,         // Maroon
    0x2E1050UL,         // Purple
    0x0A3C44UL,         // Teal
    0x23272EUL,         // Slate
    0x3A2410UL          // Brown
};

static const char *const s_bgNames[MENU_NUM_BGS] =
{
    "Black", "Navy", "Forest", "Maroon", "Purple", "Teal", "Slate", "Brown"
};

uint32_t Menu_BgColor(uint16_t idx)
{
    return s_bgColors[(idx < MENU_NUM_BGS) ? idx : 0];
}

const char *Menu_BgName(uint16_t idx)
{
    return s_bgNames[(idx < MENU_NUM_BGS) ? idx : 0];
}

// Saved palette index; 0 (Black) if nothing was ever saved.
uint16_t Menu_GetBg(void)
{
    SaveMenu m;

    if(Save_Read(SAVE_SLOT_MENU, &m, sizeof m) && m.bgIndex < MENU_NUM_BGS)
        return m.bgIndex;
    return 0;
}

// Store the index in the save driver's RAM copy (marks it dirty only if it
// changed). Whoever calls this decides when to Save_Flush().
void Menu_SetBg(uint16_t idx)
{
    SaveMenu m;

    m.bgIndex  = (idx < MENU_NUM_BGS) ? idx : 0;
    m.reserved = 0;
    Save_Write(SAVE_SLOT_MENU, &m, sizeof m);
}

//*****************************************************************************
// Small helpers
//*****************************************************************************

// Blend two 0xRRGGBB colors. t = 0 gives a, t = 256 gives b.
static uint32_t Mix24(uint32_t a, uint32_t b, uint16_t t)
{
    uint32_t r;
    uint32_t g;
    uint32_t bl;

    r  = ((((a >> 16) & 0xFFUL) * (256UL - t)) +
          (((b >> 16) & 0xFFUL) * t)) >> 8;

    g  = ((((a >> 8) & 0xFFUL) * (256UL - t)) +
          (((b >> 8) & 0xFFUL) * t)) >> 8;

    bl = (((a & 0xFFUL) * (256UL - t)) +
          ((b & 0xFFUL) * t)) >> 8;

    return (r << 16) | (g << 8) | bl;
}

// Brightness: 0 = black, 256 = original color.
static uint32_t Scale24(uint32_t c, uint16_t s)
{
    return Mix24(0UL, c, s);
}

// Rectangle fill that is safe when the rectangle hangs off the left/right edge
// (or is empty: x1 < x0 draws nothing).
static void FillClip(int32_t x0, int16_t y0, int32_t x1, int16_t y1, uint32_t color)
{
    if(x0 < 0)
        x0 = 0;

    if(x1 > SCREEN_W - 1)
        x1 = SCREEN_W - 1;

    if(x1 < x0)
        return;

    Graphics_FillRect((int16_t)x0, y0, (int16_t)x1, y1, color);
}

// Write v as decimal text into p. Returns number of characters written.
static int16_t Menu_Itoa(char *p, uint16_t v)
{
    char tmp[6];
    int16_t n = 0;
    int16_t i;

    do
    {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while(v);

    for(i = 0; i < n; i++)
        p[i] = tmp[n - 1 - i];

    return n;
}

// Left edge of tile i when the strip is scrolled to pos.
static int32_t Menu_TileX(int16_t i, int32_t pos)
{
    return MENU_CENTER_X - MENU_TILE_W / 2 + (int32_t)i * MENU_PITCH - pos;
}

static bool Menu_TileVisible(int32_t x)
{
    return (x < SCREEN_W) && (x + MENU_TILE_W > 0);
}

// Tile body color: bright for the selected tile, dim for the rest.
static uint32_t Menu_TileColor(int16_t i)
{
    return Scale24(g_apps[i]->color,
                   (i == s_lit) ? MENU_LIT_BRIGHT : MENU_DIM_BRIGHT);
}

// Letter color: dark on the bright tile, light on the dim ones.
static uint32_t Menu_GlyphColor(int16_t i)
{
    return (i == s_lit) ? Scale24(g_apps[i]->color, 0)
                        : Scale24(g_apps[i]->color, 120);
}

//*****************************************************************************
// Drawing
//*****************************************************************************

static int16_t GlyphIndex(char c)
{
    if(c >= 'a' && c <= 'z')
        c = (char)(c - 32);

    if(c >= 'A' && c <= 'Z')
        return (int16_t)(c - 'A');

    if(c >= '0' && c <= '9')
        return (int16_t)(26 + (c - '0'));

    return 36;
}

// One big letter, drawn as merged vertical rectangles to reduce SPI traffic.
static void Menu_DrawGlyph(char ch, int32_t x, int16_t y, uint32_t color)
{
    const uint16_t *g = g_glyphs[GlyphIndex(ch)];

    int16_t col;
    int16_t row;
    int16_t start;

    for(col = 0; col < 5; col++)
    {
        row = 0;

        while(row < 7)
        {
            if(g[col] & (1U << row))
            {
                start = row;

                while(row < 7 && (g[col] & (1U << row)))
                    row++;

                FillClip(x + col * MENU_GLYPH_SCALE,
                         y + start * MENU_GLYPH_SCALE,
                         x + (col + 1) * MENU_GLYPH_SCALE - 1,
                         y + row * MENU_GLYPH_SCALE - 1,
                         color);
            }
            else
            {
                row++;
            }
        }
    }
}

// Paint tile i's body at scroll position pos.
static void Menu_PaintBody(int16_t i, int32_t pos)
{
    int32_t x = Menu_TileX(i, pos);

    FillClip(x, MENU_TILE_Y, x + MENU_TILE_W - 1,
             MENU_TILE_Y + MENU_TILE_H - 1, Menu_TileColor(i));
}

// Draw the big letter of every visible tile (strip settled).
static void Menu_DrawGlyphs(int32_t pos, int16_t count)
{
    int16_t i;
    int32_t x;

    for(i = 0; i < count; i++)
    {
        x = Menu_TileX(i, pos);

        if(!Menu_TileVisible(x))
            continue;

        Menu_DrawGlyph(g_apps[i]->name[0],
                       x + MENU_GLYPH_DX,
                       MENU_TILE_Y + MENU_GLYPH_DY,
                       Menu_GlyphColor(i));
    }
}

// Cover each visible letter with its tile color: one small fill per tile.
// Done once when a move starts, so the moving tiles can stay plain rectangles.
static void Menu_EraseGlyphs(int32_t pos, int16_t count)
{
    int16_t i;
    int32_t x;

    for(i = 0; i < count; i++)
    {
        x = Menu_TileX(i, pos);

        if(!Menu_TileVisible(x))
            continue;

        FillClip(x + MENU_GLYPH_DX,
                 MENU_TILE_Y + MENU_GLYPH_DY,
                 x + MENU_GLYPH_DX + MENU_GLYPH_W - 1,
                 MENU_TILE_Y + MENU_GLYPH_DY + MENU_GLYPH_H - 1,
                 Menu_TileColor(i));
    }
}

// Full strip paint for Menu_Init (the screen was just cleared to s_bg).
static void Menu_PaintAllTiles(int32_t pos, int16_t count)
{
    int16_t i;

    for(i = 0; i < count; i++)
    {
        if(Menu_TileVisible(Menu_TileX(i, pos)))
            Menu_PaintBody(i, pos);
    }
}

// Move the strip picture from scroll position p0 to p1 by painting only the
// columns that change. Each tile is a solid rectangle, so for every tile:
//   pass 1: columns it left behind     -> background
//   pass 2: columns it newly covers    -> tile color
// Pass 1 runs for all tiles before pass 2, so a tile that slides over a
// neighbor's old position is never erased by the neighbor's trailing edge.
static void Menu_ScrollStrip(int32_t p0, int32_t p1, int16_t count)
{
    int32_t d = p1 - p0;        // > 0: tiles move left
    int32_t xo;
    int32_t xn;
    int32_t a;
    int32_t b;
    int16_t i;

    if(d == 0)
        return;

    for(i = 0; i < count; i++)
    {
        xo = Menu_TileX(i, p0);
        xn = Menu_TileX(i, p1);

        if(!Menu_TileVisible(xo))
            continue;

        if(d > 0)
        {
            a = (xo > xn + MENU_TILE_W) ? xo : (xn + MENU_TILE_W);
            b = xo + MENU_TILE_W - 1;
        }
        else
        {
            a = xo;
            b = (xo + MENU_TILE_W - 1 < xn - 1) ? (xo + MENU_TILE_W - 1)
                                                : (xn - 1);
        }

        FillClip(a, MENU_TILE_Y, b, MENU_TILE_Y + MENU_TILE_H - 1, s_bg);
    }

    for(i = 0; i < count; i++)
    {
        xo = Menu_TileX(i, p0);
        xn = Menu_TileX(i, p1);

        if(!Menu_TileVisible(xn))
            continue;

        if(d > 0)
        {
            a = xn;
            b = (xn + MENU_TILE_W - 1 < xo - 1) ? (xn + MENU_TILE_W - 1)
                                                : (xo - 1);
        }
        else
        {
            a = (xn > xo + MENU_TILE_W) ? xn : (xo + MENU_TILE_W);
            b = xn + MENU_TILE_W - 1;
        }

        FillClip(a, MENU_TILE_Y, b, MENU_TILE_Y + MENU_TILE_H - 1,
                 Menu_TileColor(i));
    }
}

// Fixed selection frame.
static void Menu_DrawFrame(uint32_t color)
{
    int16_t out;

    int32_t x0;
    int32_t x1;

    int16_t y0;
    int16_t y1;

    out = MENU_FRAME_PAD + MENU_FRAME_THICK;

    x0 = MENU_CENTER_X - MENU_TILE_W / 2 - out;
    x1 = MENU_CENTER_X + MENU_TILE_W / 2 - 1 + out;
    y0 = MENU_TILE_Y - out;
    y1 = MENU_TILE_Y + MENU_TILE_H - 1 + out;

    FillClip(x0, y0, x1, y0 + MENU_FRAME_THICK - 1, color);             // top
    FillClip(x0, y1 - MENU_FRAME_THICK + 1, x1, y1, color);             // bottom
    FillClip(x0, y0, x0 + MENU_FRAME_THICK - 1, y1, color);             // left
    FillClip(x1 - MENU_FRAME_THICK + 1, y0, x1, y1, color);             // right
}

// Only the two side bars, limited to the strip rows. The top and bottom bars
// lie outside the strip, so a scroll step can never damage them.
static void Menu_DrawFrameSides(uint32_t color)
{
    int16_t out;
    int32_t x0;
    int32_t x1;

    out = MENU_FRAME_PAD + MENU_FRAME_THICK;

    x0 = MENU_CENTER_X - MENU_TILE_W / 2 - out;
    x1 = MENU_CENTER_X + MENU_TILE_W / 2 - 1 + out;

    FillClip(x0, MENU_TILE_Y, x0 + MENU_FRAME_THICK - 1,
             MENU_TILE_Y + MENU_TILE_H - 1, color);                     // left
    FillClip(x1 - MENU_FRAME_THICK + 1, MENU_TILE_Y, x1,
             MENU_TILE_Y + MENU_TILE_H - 1, color);                     // right
}

// Selection frame pulse color.
static uint32_t Menu_PulseColor(int16_t sel, int16_t step)
{
    int16_t tri;

    tri = (step < 8) ? step : (15 - step);

    return Mix24(g_apps[sel]->color, 0x00FFFFFFUL, (uint16_t)(tri * 32));
}

// ---- Pager ------------------------------------------------------------------

static int16_t Menu_DotX(int16_t i, int16_t count)
{
    return (SCREEN_W / 2) - (count * MENU_DOT_STEP) / 2 +
           MENU_DOT_STEP / 2 + i * MENU_DOT_STEP;
}

static void Menu_DrawDot(int16_t i, int16_t count, bool on)
{
    int16_t cx = Menu_DotX(i, count);

    if(on)
    {
        Graphics_FillRect(cx - 4, MENU_DOTS_Y - 4, cx + 3, MENU_DOTS_Y + 3,
                          g_apps[i]->color);
    }
    else
    {
        Graphics_FillRect(cx - 4, MENU_DOTS_Y - 4, cx + 3, MENU_DOTS_Y + 3,
                          s_bg);
        Graphics_FillRect(cx - 2, MENU_DOTS_Y - 2, cx + 1, MENU_DOTS_Y + 1,
                          COLOR_GRAY);
    }
}

// Page indicator (dots, or "n / m" when there are too many apps).
// Only the dots that changed are touched.
static void Menu_UpdatePager(int16_t sel, int16_t count)
{
    int16_t i;
    char buf[16];
    int16_t n;

    if(s_pgSel == sel)
        return;

    if(count > MENU_MAX_DOTS)
    {
        Graphics_FillRect(SCREEN_W / 2 - 60, MENU_DOTS_Y - 10,
                          SCREEN_W / 2 + 59, MENU_DOTS_Y + 8, s_bg);

        n = Menu_Itoa(buf, (uint16_t)(sel + 1));

        buf[n++] = ' ';
        buf[n++] = '/';
        buf[n++] = ' ';

        n += Menu_Itoa(buf + n, (uint16_t)count);
        buf[n] = 0;

        Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
        Graphics_drawStringCentered(&g_sContext, (int16_t *)buf,
                                    AUTO_STRING_LENGTH, SCREEN_W / 2,
                                    MENU_DOTS_Y, TRANSPARENT_TEXT);
    }
    else if(s_pgSel < 0)
    {
        for(i = 0; i < count; i++)
            Menu_DrawDot(i, count, i == sel);
    }
    else
    {
        Menu_DrawDot(s_pgSel, count, false);
        Menu_DrawDot(sel, count, true);
    }

    s_pgSel = sel;
}

// ---- Labels -----------------------------------------------------------------

// Name + hint of one app, in the given colors.
static void Menu_DrawLabelText(int16_t sel, uint32_t nameColor, uint32_t hintColor)
{
    Graphics_setForegroundColor(&g_sContext, nameColor);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_apps[sel]->name,
                                AUTO_STRING_LENGTH, SCREEN_W / 2,
                                MENU_NAME_Y, TRANSPARENT_TEXT);

    Graphics_setForegroundColor(&g_sContext, hintColor);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_apps[sel]->hint,
                                AUTO_STRING_LENGTH, SCREEN_W / 2,
                                MENU_HINT_Y, TRANSPARENT_TEXT);
}

static void Menu_UpdateLabels(int16_t sel)
{
    if(s_lblSel == sel)
        return;

    if(s_lblSel >= 0)
    {
#if MENU_LABEL_ERASE_BY_REDRAW
        Menu_DrawLabelText(s_lblSel, s_bg, s_bg);
#else
        Graphics_FillRect(0, MENU_NAME_Y - 14, SCREEN_W - 1,
                          MENU_HINT_Y + 11, s_bg);
#endif
    }

    Menu_DrawLabelText(sel, COLOR_WHITE, COLOR_GRAY);
    s_lblSel = sel;
}

// Title, divider and the reminder line (never change while the menu is up).
static void Menu_DrawStatic(void)
{
    Graphics_setBackgroundColor(&g_sContext, s_bg);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"SELECT GAME",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15,
                                OPAQUE_TEXT);

    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);

    Graphics_setForegroundColor(&g_sContext, Scale24(COLOR_GRAY, 150));
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, SCREEN_W / 2,
                                MENU_REMINDER_Y, TRANSPARENT_TEXT);
}

//*****************************************************************************
// App callbacks
//*****************************************************************************

// Called once each time the menu appears: full draw, selection centered.
static void Menu_Init(void)
{
    int16_t count = (int16_t)g_appCount;

    s_bg = Menu_BgColor(Menu_GetBg());      // saved background color
    Menu_DrawStatic();

    s_drawFrame  = false;
    s_drawSides  = false;
    s_needSettle = false;
    s_glyphsOn   = false;
    s_lblSel     = -1;
    s_pgSel      = -1;

    if(count < 1)
        return;                             // nothing to show

    if(s_sel >= count)
        s_sel = 0;

    s_pos      = (int32_t)s_sel * MENU_PITCH;
    s_drawnPos = s_pos;
    s_from     = s_pos;
    s_target   = s_pos;
    s_animF    = 0;
    s_hTimer   = 0;
    s_pStep    = 7;
    s_tick     = 0;
    s_lit      = s_sel;

    Menu_UpdatePager(s_sel, count);
    Menu_UpdateLabels(s_sel);
    Menu_PaintAllTiles(s_pos, count);
    Menu_DrawGlyphs(s_pos, count);
    s_glyphsOn = true;
    Menu_DrawFrame(Menu_PulseColor(s_sel, s_pStep));
}

// Logic only: read input, move the selection, advance the animation, and
// record WHAT needs redrawing. No drawing here.
static AppStatus Menu_Update(void)
{
    int16_t count = (int16_t)g_appCount;
    int16_t move;
    int16_t heldDir;
    int16_t newStep;
    int16_t ns;
    int32_t u;
    int32_t e;

    if(count < 1)
        return APP_CONTINUE;

    // ---- Select: hand the chosen app to main.c and leave the menu ----------
    if(g_joy.btnPressed || MENU_SELECT_TAP)
    {
        Sfx_PlayWait(&sfx_menu_select);
        App_Launch(g_apps[s_sel]);
        return APP_QUIT;
    }

    // ---- Requested movement -------------------------------------------------
    move = g_joy.edgeX;

    if(MENU_LEFT_TAP)
        move = -1;

    if(MENU_RIGHT_TAP)
        move = 1;

    heldDir = g_joy.dirX;

    if(MENU_LEFT_HELD && !MENU_RIGHT_HELD)
        heldDir = -1;

    if(MENU_RIGHT_HELD && !MENU_LEFT_HELD)
        heldDir = 1;

    // ---- Auto-repeat --------------------------------------------------------
    if(move != 0)
    {
        s_hTimer = MENU_DAS_FRAMES;
    }
    else if(heldDir == 0)
    {
        s_hTimer = 0;
    }
    else if(--s_hTimer <= 0)
    {
        move = heldDir;
        s_hTimer = MENU_ARR_FRAMES;
    }

    // ---- Change selected item ----------------------------------------------
    // Labels and the highlight are NOT redrawn here; they hand over when the
    // animation settles. A new target restarts the ease from where we are.
    if(move != 0)
    {
        ns = s_sel + move;

#if MENU_WRAP
        if(ns < 0)       ns = count - 1;
        if(ns >= count)  ns = 0;
#else
        if(ns < 0)       ns = 0;
        if(ns >= count)  ns = count - 1;
#endif

        if(ns != s_sel)
        {
            s_sel        = ns;
            s_from       = s_pos;
            s_target     = (int32_t)s_sel * MENU_PITCH;
            s_animF      = 0;
            s_needSettle = true;
            s_drawFrame  = true;            // frame color follows the app
            Buzzer_PlaySfx(&sfx_menu_move);
        }
        else if(s_hTimer == MENU_DAS_FRAMES)        // bump only on a fresh
            Buzzer_PlaySfx(&sfx_menu_bump);         // press, not auto-repeat
    }

    // ---- Animate: fixed frame count, ease-out (fast start, soft landing) ----
    if(s_pos != s_target)
    {
        if(++s_animF >= MENU_ANIM_FRAMES)
        {
            s_pos = s_target;
        }
        else
        {
            u = 256 - ((int32_t)s_animF * 256) / MENU_ANIM_FRAMES;  // 256..0
            e = 256 - ((u * u) >> 8);                               // 0..256
            s_pos = s_from + ((s_target - s_from) * e) / 256;
        }

        s_drawSides = true;                 // the strip may overwrite the side bars
    }

    // ---- Pulse --------------------------------------------------------------
#if MENU_PULSE
    s_tick++;
    newStep = (int16_t)((s_tick >> 2) & 15);

    if(newStep != s_pStep)
    {
        s_pStep = newStep;
        s_drawFrame = true;
    }
#endif

    return APP_CONTINUE;
}

// Drawing only: do whatever Update flagged.
static void Menu_Render(void)
{
    int16_t count = (int16_t)g_appCount;
    int16_t old;

    if(count < 1)
        return;

    // ---- Scroll: repaint only the columns that changed ----------------------
    if(s_pos != s_drawnPos)
    {
        if(s_glyphsOn)
        {
            Menu_EraseGlyphs(s_drawnPos, count);
            s_glyphsOn = false;
        }

        Menu_ScrollStrip(s_drawnPos, s_pos, count);
        s_drawnPos = s_pos;
    }

    // ---- Arrived: hand over the highlight, letters, pager and labels --------
    if(s_needSettle && s_pos == s_target)
    {
        s_needSettle = false;

        if(s_lit != s_sel)
        {
            old   = s_lit;
            s_lit = s_sel;
#if MENU_DIM_BRIGHT != MENU_LIT_BRIGHT
            Menu_PaintBody(old, s_pos);         // old tile -> dim
            Menu_PaintBody(s_lit, s_pos);       // new tile -> bright
#else
            (void)old;
#endif
        }

        Menu_DrawGlyphs(s_pos, count);
        s_glyphsOn = true;

        Menu_UpdatePager(s_sel, count);
        Menu_UpdateLabels(s_sel);
    }

    if(s_drawFrame)
        Menu_DrawFrame(Menu_PulseColor(s_sel, s_pStep));
    else if(s_drawSides)
        Menu_DrawFrameSides(Menu_PulseColor(s_sel, s_pStep));

    s_drawFrame = false;
    s_drawSides = false;
}

// Other apps assume a BLACK drawing background (they usually just clear the
// screen), so put it back when the menu is left.
static void Menu_Exit(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
}

//*****************************************************************************
// The menu's descriptor. main.c runs it first and after every app.
// (Not listed in g_apps[], so it never appears as a tile.)
//*****************************************************************************
const App g_menuApp =
{
    "MENU", "", COLOR_WHITE, MENU_FRAME_US,
    Menu_Init, Menu_Update, Menu_Render, Menu_Exit,
    NULL
};
