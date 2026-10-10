//#############################################################################
// FILE:   app/main_menu.c
// TITLE:  Main menu - smooth horizontal tile carousel
//
// The menu is an App like any other: Menu_Init / Menu_Update / Menu_Render,
// driven by RunApp() in main.c. When the player picks a tile, Menu_Update()
// calls App_Launch(g_apps[sel]) and returns APP_QUIT; main.c then runs that
// app.
//
// Compared with the old Menu_Run() while-loop:
//   * No while(1), no Joy_Update()/Dpad_Update(), no DELAY_US() here.
//   * Locals that lived across frames are now file-static (s_pos, s_target..).
//   * Update only changes state and sets "dirty" flags; Render only draws.
//   * The drawing code (tiles, glyphs, frame, pager, labels) is unchanged
//     except g_apps[i].x became g_apps[i]->x (the table holds pointers now).
//
// Animation is tuned for the slow SPI LCD:
//   * Tile bodies are redrawn during animation, big glyphs only when settled.
//   * Labels are updated after the animation finishes.
//
// CONTROLS
//   stick left/right, D-pad A/B : scroll (hold to auto-repeat)
//   stick button, D-pad D       : start the highlighted app
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

#define MENU_LABEL_AREA_Y  168
#define MENU_NAME_Y        182
#define MENU_HINT_Y        206
#define MENU_REMINDER_Y    228

#define MENU_GLYPH_SCALE   7

//*****************************************************************************
// Feel / animation
//*****************************************************************************
<<<<<<< HEAD

// 15 ms = approximately 66 frames/sec.
// The actual frame rate is limited by LCD SPI drawing time.
#define MENU_LOOP_US       9000uL

// Once the remaining distance is smaller than this, snap to target.
#define MENU_MIN_STEP      6

#define MENU_DAS_FRAMES    6
#define MENU_ARR_FRAMES    2
=======
#define MENU_FRAME_US      9000uL   // frame pause; real rate is limited by SPI
#define MENU_MIN_STEP      6        // snap to target when this close
#define MENU_DAS_FRAMES    6        // auto-repeat: delay before repeating
#define MENU_ARR_FRAMES    2        // auto-repeat: frames between repeats
>>>>>>> header-house-keeping

#define MENU_WRAP          0
#define MENU_PULSE         1

//*****************************************************************************
// Controls
//*****************************************************************************
#define MENU_LEFT_HELD     (g_dpad.a)
#define MENU_RIGHT_HELD    (g_dpad.b)
#define MENU_LEFT_TAP      (g_dpad.aPressed)
#define MENU_RIGHT_TAP     (g_dpad.bPressed)
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
// Menu state (was local variables inside Menu_Run)
//*****************************************************************************
static int16_t  s_sel = 0;          // selected app; survives between launches
static int32_t  s_pos;              // current strip scroll position (pixels)
static int32_t  s_target;           // scroll position we are animating toward
static int16_t  s_hTimer;           // auto-repeat countdown
static int16_t  s_pStep;            // selection-frame pulse phase 0..15
static uint16_t s_tick;

// "Dirty" flags: Update sets them, Render consumes them.
static bool s_drawStrip;            // moving: fast strip render (no glyphs)
static bool s_drawSettled;          // arrived: full strip + pager + labels
static bool s_drawFrame;            // selection frame needs redrawing

//*****************************************************************************
// Small helpers (unchanged)
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

// Rectangle fill that is safe when the rectangle hangs off the left/right edge.
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

//*****************************************************************************
// Drawing (unchanged except g_apps[i]-> )
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

// One tile. drawGlyph = 0 draws only the body (used while animating, because
// the glyph is expensive over SPI and adds little to the motion).
static void Menu_DrawTile(int16_t i, int32_t x, int16_t drawGlyph)
{
    int32_t dist;
    int32_t k;
    uint32_t color;
    uint16_t brightness;

    color = g_apps[i]->color;

    // Distance from tile center to screen center.
    dist = (x + MENU_TILE_W / 2) - MENU_CENTER_X;

    if(dist < 0)
        dist = -dist;

    // k: 256 = centered, 0 = outside the active region.
    k = 256 - (dist * 256) / MENU_PITCH;

    if(k < 0)
        k = 0;

    if(k > 256)
        k = 256;

    // Far tile = dim, center tile = bright.
    brightness = (uint16_t)(56 + ((200 * k) >> 8));

    FillClip(x,
             MENU_TILE_Y,
             x + MENU_TILE_W - 1,
             MENU_TILE_Y + MENU_TILE_H - 1,
             Scale24(color, brightness));

    if(drawGlyph)
    {
        Menu_DrawGlyph(g_apps[i]->name[0],
                       x + (MENU_TILE_W - 5 * MENU_GLYPH_SCALE) / 2,
                       MENU_TILE_Y + (MENU_TILE_H - 7 * MENU_GLYPH_SCALE) / 2,
                       Scale24(color, (uint16_t)((120 * (256 - k)) >> 8)));
    }
}

// Render the visible strip. drawGlyphs = 0 for animation, 1 once settled.
static void Menu_RenderStrip(int32_t pos, int16_t count, int16_t drawGlyphs)
{
    int16_t i;

    int32_t x;
    int32_t x0;
    int32_t x1;

    int32_t cursor = 0;

    for(i = 0; i < count; i++)
    {
        x = MENU_CENTER_X - MENU_TILE_W / 2 + (int32_t)i * MENU_PITCH - pos;

        // Completely off screen.
        if(x >= SCREEN_W || x + MENU_TILE_W <= 0)
            continue;

        Menu_DrawTile(i, x, drawGlyphs);

        // Visible portion.
        x0 = (x < 0) ? 0 : x;
        x1 = x + MENU_TILE_W - 1;

        if(x1 > SCREEN_W - 1)
            x1 = SCREEN_W - 1;

        // Clear the gap before this tile.
        if(x0 > cursor)
        {
            FillClip(cursor, MENU_TILE_Y, x0 - 1,
                     MENU_TILE_Y + MENU_TILE_H - 1, COLOR_BLACK);
        }

        cursor = x1 + 1;
    }

    // Clear anything after the last visible tile.
    if(cursor < SCREEN_W)
    {
        FillClip(cursor, MENU_TILE_Y, SCREEN_W - 1,
                 MENU_TILE_Y + MENU_TILE_H - 1, COLOR_BLACK);
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

// Selection frame pulse color.
static uint32_t Menu_PulseColor(int16_t sel, int16_t step)
{
    int16_t tri;

    tri = (step < 8) ? step : (15 - step);

    return Mix24(g_apps[sel]->color, 0x00FFFFFFUL, (uint16_t)(tri * 32));
}

// Page indicator (dots, or "n / m" when there are too many apps).
static void Menu_DrawPager(int16_t sel, int16_t count)
{
    int16_t i;
    int16_t cx;
    char buf[16];
    int16_t n;

    Graphics_FillRect(0, MENU_DOTS_Y - 10, SCREEN_W - 1, MENU_DOTS_Y + 8,
                      COLOR_BLACK);

    if(count > MENU_MAX_DOTS)
    {
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
        return;
    }

    for(i = 0; i < count; i++)
    {
        cx = (SCREEN_W / 2) - (count * MENU_DOT_STEP) / 2 +
             MENU_DOT_STEP / 2 + i * MENU_DOT_STEP;

        if(i == sel)
        {
            Graphics_FillRect(cx - 4, MENU_DOTS_Y - 4, cx + 3, MENU_DOTS_Y + 3,
                              g_apps[sel]->color);
        }
        else
        {
            Graphics_FillRect(cx - 2, MENU_DOTS_Y - 2, cx + 1, MENU_DOTS_Y + 1,
                              COLOR_GRAY);
        }
    }
}

// Name, control hint, reminder.
static void Menu_DrawLabels(int16_t sel)
{
    Graphics_FillRect(0, MENU_LABEL_AREA_Y, SCREEN_W - 1, SCREEN_H - 1,
                      COLOR_BLACK);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_apps[sel]->name,
                                AUTO_STRING_LENGTH, SCREEN_W / 2,
                                MENU_NAME_Y, TRANSPARENT_TEXT);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_apps[sel]->hint,
                                AUTO_STRING_LENGTH, SCREEN_W / 2,
                                MENU_HINT_Y, TRANSPARENT_TEXT);

    Graphics_setForegroundColor(&g_sContext, Scale24(COLOR_GRAY, 150));
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, SCREEN_W / 2,
                                MENU_REMINDER_Y, TRANSPARENT_TEXT);
}

// Title + divider (never changes while the menu is up).
static void Menu_DrawStatic(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"SELECT GAME",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15,
                                OPAQUE_TEXT);

    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
}

//*****************************************************************************
// App callbacks
//*****************************************************************************

// Called once each time the menu appears: full draw, selection centered.
static void Menu_Init(void)
{
    int16_t count = (int16_t)g_appCount;

    Menu_DrawStatic();

    s_drawStrip   = false;
    s_drawSettled = false;
    s_drawFrame   = false;

    if(count < 1)
        return;                             // nothing to show

    if(s_sel >= count)
        s_sel = 0;

    s_pos    = (int32_t)s_sel * MENU_PITCH;
    s_target = s_pos;
    s_hTimer = 0;
    s_pStep  = 7;
    s_tick   = 0;

    Menu_DrawPager(s_sel, count);
    Menu_DrawLabels(s_sel);
    Menu_RenderStrip(s_pos, count, 1);
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
    int32_t diff;
    int32_t step;

    if(count < 1)
        return APP_CONTINUE;

    // ---- Select: hand the chosen app to main.c and leave the menu ----------
    if(g_joy.btnPressed || MENU_SELECT_TAP)
    {
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
    // Labels are NOT redrawn here; they update when the animation settles.
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
            s_sel    = ns;
            s_target = (int32_t)s_sel * MENU_PITCH;
        }
    }

    // ---- Animate ------------------------------------------------------------
    if(s_pos != s_target)
    {
        diff = s_target - s_pos;
        step = diff / 2;

        if(step == 0)                       // never get stuck
            step = (diff > 0) ? 1 : -1;

        if(diff < 0 ? (-diff <= MENU_MIN_STEP) : (diff <= MENU_MIN_STEP))
            step = diff;                    // close enough: snap

        s_pos += step;

        if(s_pos == s_target)
            s_drawSettled = true;           // full render + pager + labels
        else
            s_drawStrip = true;             // fast render, no glyphs

        s_drawFrame = true;                 // the strip overwrites the frame
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

    if(count < 1)
        return;

    if(s_drawSettled)
    {
        // The full render also draws the tile bodies, so the fast pass that
        // the old code did first on the final step is not needed.
        Menu_RenderStrip(s_pos, count, 1);
        Menu_DrawPager(s_sel, count);
        Menu_DrawLabels(s_sel);
    }
    else if(s_drawStrip)
    {
        Menu_RenderStrip(s_pos, count, 0);
    }

    if(s_drawFrame)
        Menu_DrawFrame(Menu_PulseColor(s_sel, s_pStep));

    s_drawStrip   = false;
    s_drawSettled = false;
    s_drawFrame   = false;
}

//*****************************************************************************
// The menu's descriptor. main.c runs it first and after every app.
// (Not listed in g_apps[], so it never appears as a tile.)
//*****************************************************************************
const App g_menuApp =
{
    "MENU", "", COLOR_WHITE, MENU_FRAME_US,
    Menu_Init, Menu_Update, Menu_Render, NULL,
    NULL
};
