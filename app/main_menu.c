//#############################################################################
// FILE:   app/main_menu.c
// TITLE:  Main menu - smooth horizontal tile carousel
//
// DESCRIPTION
//   Draws one tile per entry in g_apps[] in a horizontal strip. A fixed
//   selection frame sits in the middle of the screen; moving left/right
//   slides the strip underneath it.
//
//   Animation is optimized for the relatively slow SPI LCD:
//     * Tile bodies are redrawn during animation.
//     * Large tile glyphs are NOT redrawn during every animation frame.
//     * Glyphs are drawn once the carousel reaches its destination.
//     * Labels are updated after the animation finishes.
//     * Only the visible portion of the strip is rendered.
//
// CONTROLS
//   stick left/right, D-pad A/B  : scroll (hold to auto-repeat)
//   stick button, D-pad D        : start the highlighted app
//
//#############################################################################

#include "apps.h"

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

// 15 ms = approximately 66 frames/sec.
// The actual frame rate is limited by LCD SPI drawing time.
#define MENU_LOOP_US       9000uL

// Once the remaining distance is smaller than this, snap to target.
#define MENU_MIN_STEP      6

#define MENU_DAS_FRAMES    6
#define MENU_ARR_FRAMES    2

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
// Small helpers
//*****************************************************************************

// Blend two 0xRRGGBB colors.
// t = 0 gives a.
// t = 256 gives b.
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


// Brightness scale:
// 0   = black
// 256 = original color
static uint32_t Scale24(uint32_t c, uint16_t s)
{
    return Mix24(0UL, c, s);
}


// Rectangle fill that is safe when the rectangle hangs off the
// left/right edge of the LCD.
static void FillClip(int32_t x0,
                     int16_t y0,
                     int32_t x1,
                     int16_t y1,
                     uint32_t color)
{
    if(x0 < 0)
        x0 = 0;

    if(x1 > SCREEN_W - 1)
        x1 = SCREEN_W - 1;

    if(x1 < x0)
        return;

    Graphics_FillRect((int16_t)x0,
                      y0,
                      (int16_t)x1,
                      y1,
                      color);
}


// Write v as decimal text into p.
// Returns number of characters written.
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


// Draw one big letter.
//
// The glyph is deliberately drawn as merged vertical rectangles rather
// than individual pixels to reduce SPI traffic.
static void Menu_DrawGlyph(char ch,
                           int32_t x,
                           int16_t y,
                           uint32_t color)
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


//*****************************************************************************
// Draw one tile.
//
// drawGlyph = 1:
//     Draw complete tile including large letter.
//
// drawGlyph = 0:
//     Draw only the tile body.
//
// During animation we use drawGlyph = 0 because the glyph is expensive
// over SPI and contributes very little to the visual motion.
//*****************************************************************************

static void Menu_DrawTile(int16_t i,
                          int32_t x,
                          int16_t drawGlyph)
{
    int32_t dist;
    int32_t k;
    uint32_t color;
    uint16_t brightness;

    color = g_apps[i].color;

    // Distance from the tile center to the screen center.
    dist = (x + MENU_TILE_W / 2) - MENU_CENTER_X;

    if(dist < 0)
        dist = -dist;

    // 256 = centered.
    // 0 = completely outside the active region.
    k = 256 - (dist * 256) / MENU_PITCH;

    if(k < 0)
        k = 0;

    if(k > 256)
        k = 256;

    // Far tile = dim.
    // Center tile = bright.
    brightness = (uint16_t)(56 + ((200 * k) >> 8));

    FillClip(x,
             MENU_TILE_Y,
             x + MENU_TILE_W - 1,
             MENU_TILE_Y + MENU_TILE_H - 1,
             Scale24(color, brightness));

    // This is the expensive part.
    //
    // During animation it is skipped.
    if(drawGlyph)
    {
        Menu_DrawGlyph(g_apps[i].name[0],
                       x + (MENU_TILE_W - 5 * MENU_GLYPH_SCALE) / 2,
                       MENU_TILE_Y +
                           (MENU_TILE_H - 7 * MENU_GLYPH_SCALE) / 2,
                       Scale24(color,
                               (uint16_t)((120 * (256 - k)) >> 8)));
    }
}


//*****************************************************************************
// Render the visible strip.
//
// drawGlyphs = 0:
//     Fast animation render.
//
// drawGlyphs = 1:
//     Complete render used when the animation settles.
//
// This is the main performance improvement.
//*****************************************************************************

static void Menu_RenderStrip(int32_t pos,
                             int16_t count,
                             int16_t drawGlyphs)
{
    int16_t i;

    int32_t x;
    int32_t x0;
    int32_t x1;

    int32_t cursor = 0;

    for(i = 0; i < count; i++)
    {
        x = MENU_CENTER_X -
            MENU_TILE_W / 2 +
            (int32_t)i * MENU_PITCH -
            pos;

        // Completely off screen.
        if(x >= SCREEN_W ||
           x + MENU_TILE_W <= 0)
        {
            continue;
        }

        // Draw tile body.
        Menu_DrawTile(i, x, drawGlyphs);

        // Determine visible portion.
        x0 = (x < 0) ? 0 : x;

        x1 = x + MENU_TILE_W - 1;

        if(x1 > SCREEN_W - 1)
            x1 = SCREEN_W - 1;

        // Clear the gap before this tile.
        if(x0 > cursor)
        {
            FillClip(cursor,
                     MENU_TILE_Y,
                     x0 - 1,
                     MENU_TILE_Y + MENU_TILE_H - 1,
                     COLOR_BLACK);
        }

        cursor = x1 + 1;
    }

    // Clear anything after the last visible tile.
    if(cursor < SCREEN_W)
    {
        FillClip(cursor,
                 MENU_TILE_Y,
                 SCREEN_W - 1,
                 MENU_TILE_Y + MENU_TILE_H - 1,
                 COLOR_BLACK);
    }
}


//*****************************************************************************
// Fixed selection frame
//*****************************************************************************

static void Menu_DrawFrame(uint32_t color)
{
    int16_t out;

    int32_t x0;
    int32_t x1;

    int16_t y0;
    int16_t y1;

    out = MENU_FRAME_PAD + MENU_FRAME_THICK;

    x0 = MENU_CENTER_X -
         MENU_TILE_W / 2 -
         out;

    x1 = MENU_CENTER_X +
         MENU_TILE_W / 2 -
         1 +
         out;

    y0 = MENU_TILE_Y - out;

    y1 = MENU_TILE_Y +
         MENU_TILE_H -
         1 +
         out;

    // Top
    FillClip(x0,
             y0,
             x1,
             y0 + MENU_FRAME_THICK - 1,
             color);

    // Bottom
    FillClip(x0,
             y1 - MENU_FRAME_THICK + 1,
             x1,
             y1,
             color);

    // Left
    FillClip(x0,
             y0,
             x0 + MENU_FRAME_THICK - 1,
             y1,
             color);

    // Right
    FillClip(x1 - MENU_FRAME_THICK + 1,
             y0,
             x1,
             y1,
             color);
}


//*****************************************************************************
// Selection frame pulse
//*****************************************************************************

static uint32_t Menu_PulseColor(int16_t sel,
                                int16_t step)
{
    int16_t tri;

    tri = (step < 8) ?
          step :
          (15 - step);

    return Mix24(g_apps[sel].color,
                 0x00FFFFFFUL,
                 (uint16_t)(tri * 32));
}


//*****************************************************************************
// Page indicator
//*****************************************************************************

static void Menu_DrawPager(int16_t sel,
                           int16_t count)
{
    int16_t i;
    int16_t cx;

    char buf[16];

    int16_t n;

    // Clear pager area.
    Graphics_FillRect(0,
                      MENU_DOTS_Y - 10,
                      SCREEN_W - 1,
                      MENU_DOTS_Y + 8,
                      COLOR_BLACK);

    // Too many apps for dots.
    if(count > MENU_MAX_DOTS)
    {
        n = Menu_Itoa(buf,
                      (uint16_t)(sel + 1));

        buf[n++] = ' ';
        buf[n++] = '/';
        buf[n++] = ' ';

        n += Menu_Itoa(buf + n,
                       (uint16_t)count);

        buf[n] = 0;

        Graphics_setForegroundColor(&g_sContext,
                                    COLOR_GRAY);

        Graphics_drawStringCentered(&g_sContext,
                                    (int16_t *)buf,
                                    AUTO_STRING_LENGTH,
                                    SCREEN_W / 2,
                                    MENU_DOTS_Y,
                                    TRANSPARENT_TEXT);

        return;
    }

    // Draw dots.
    for(i = 0; i < count; i++)
    {
        cx = (SCREEN_W / 2) -
             (count * MENU_DOT_STEP) / 2 +
             MENU_DOT_STEP / 2 +
             i * MENU_DOT_STEP;

        if(i == sel)
        {
            Graphics_FillRect(cx - 4,
                              MENU_DOTS_Y - 4,
                              cx + 3,
                              MENU_DOTS_Y + 3,
                              g_apps[sel].color);
        }
        else
        {
            Graphics_FillRect(cx - 2,
                              MENU_DOTS_Y - 2,
                              cx + 1,
                              MENU_DOTS_Y + 1,
                              COLOR_GRAY);
        }
    }
}


//*****************************************************************************
// Labels
//*****************************************************************************

static void Menu_DrawLabels(int16_t sel)
{
    // Clear label area.
    Graphics_FillRect(0,
                      MENU_LABEL_AREA_Y,
                      SCREEN_W - 1,
                      SCREEN_H - 1,
                      COLOR_BLACK);

    // Game name.
    Graphics_setForegroundColor(&g_sContext,
                                COLOR_WHITE);

    Graphics_drawStringCentered(&g_sContext,
                                (int16_t *)g_apps[sel].name,
                                AUTO_STRING_LENGTH,
                                SCREEN_W / 2,
                                MENU_NAME_Y,
                                TRANSPARENT_TEXT);

    // Control hint.
    Graphics_setForegroundColor(&g_sContext,
                                COLOR_GRAY);

    Graphics_drawStringCentered(&g_sContext,
                                (int16_t *)g_apps[sel].hint,
                                AUTO_STRING_LENGTH,
                                SCREEN_W / 2,
                                MENU_HINT_Y,
                                TRANSPARENT_TEXT);

    // Reminder.
    Graphics_setForegroundColor(&g_sContext,
                                Scale24(COLOR_GRAY, 150));

    Graphics_drawStringCentered(&g_sContext,
                                (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH,
                                SCREEN_W / 2,
                                MENU_REMINDER_Y,
                                TRANSPARENT_TEXT);
}


//*****************************************************************************
// Static menu elements
//*****************************************************************************

static void Menu_DrawStatic(void)
{
    Graphics_setBackgroundColor(&g_sContext,
                                COLOR_BLACK);

    Graphics_setFont(&g_sContext,
                     &g_sFontCmss20b);

    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext,
                                COLOR_WHITE);

    Graphics_drawStringCentered(&g_sContext,
                                (int16_t *)"SELECT GAME",
                                AUTO_STRING_LENGTH,
                                SCREEN_W / 2,
                                15,
                                OPAQUE_TEXT);

    Graphics_drawLine(&g_sContext,
                      10,
                      30,
                      SCREEN_W - 11,
                      30);
}


//*****************************************************************************
// Menu_Run
//*****************************************************************************

// Shows the menu and blocks until the player picks an app.
//
// Returns:
//     index into g_apps[] of selected app
//
// Returns -1 if there are no apps.
//*****************************************************************************

int16_t Menu_Run(void)
{
    static int16_t sel = 0;

    int16_t count;

    int32_t pos;
    int32_t target;
    int32_t diff;
    int32_t step;

    int16_t move;
    int16_t heldDir;
    int16_t hTimer = 0;

    int16_t pStep = 7;
    int16_t newStep;

    uint16_t tick = 0;

    bool redrawFrame;

    count = (int16_t)g_appCount;

    if(count < 1)
        return -1;

    if(sel >= count)
        sel = 0;

    // Start with selected item centered.
    pos = (int32_t)sel * MENU_PITCH;
    target = pos;

    // Initial complete draw.
    Menu_DrawStatic();

    Menu_DrawPager(sel,
                   count);

    Menu_DrawLabels(sel);

    Menu_RenderStrip(pos,
                     count,
                     1);

    Menu_DrawFrame(Menu_PulseColor(sel,
                                   pStep));


    //*************************************************************************
    // Main menu loop
    //*************************************************************************

    while(1)
    {
        Joy_Update();
        Dpad_Update();


        //*********************************************************************
        // Select
        //*********************************************************************

        if(g_joy.btnPressed ||
           MENU_SELECT_TAP)
        {
            return sel;
        }


        //*********************************************************************
        // Determine requested movement
        //*********************************************************************

        move = g_joy.edgeX;

        if(MENU_LEFT_TAP)
            move = -1;

        if(MENU_RIGHT_TAP)
            move = 1;


        heldDir = g_joy.dirX;

        if(MENU_LEFT_HELD &&
           !MENU_RIGHT_HELD)
        {
            heldDir = -1;
        }

        if(MENU_RIGHT_HELD &&
           !MENU_LEFT_HELD)
        {
            heldDir = 1;
        }


        //*********************************************************************
        // Handle auto-repeat
        //*********************************************************************

        if(move != 0)
        {
            hTimer = MENU_DAS_FRAMES;
        }
        else if(heldDir == 0)
        {
            hTimer = 0;
        }
        else if(--hTimer <= 0)
        {
            move = heldDir;
            hTimer = MENU_ARR_FRAMES;
        }


        //*********************************************************************
        // Change selected item
        //*********************************************************************

        if(move != 0)
        {
            int16_t ns;

            ns = sel + move;

#if MENU_WRAP

            if(ns < 0)
                ns = count - 1;

            if(ns >= count)
                ns = 0;

#else

            if(ns < 0)
                ns = 0;

            if(ns >= count)
                ns = count - 1;

#endif

            if(ns != sel)
            {
                sel = ns;

                target = (int32_t)sel * MENU_PITCH;

                // IMPORTANT:
                //
                // Do NOT redraw the labels here.
                //
                // The labels are updated after the carousel finishes.
                //
                // This prevents the expensive text rendering from
                // interrupting the beginning of the animation.
            }
        }


        //*********************************************************************
        // Animate
        //*********************************************************************

        redrawFrame = false;


        if(pos != target)
        {
            diff = target - pos;

            // ---------------------------------------------------------------
            // Smooth movement
            //
            // Instead of dividing the entire remaining distance by two,
            // use approximately 1/3 of the remaining distance.
            //
            // This gives a smoother transition and avoids the noticeable
            // "fast -> slow -> stop" feeling.
            // ---------------------------------------------------------------

            step = diff /2;

            // Make sure the movement never gets stuck.
            if(step == 0)
            {
                if(diff > 0)
                    step = 1;
                else
                    step = -1;
            }

            // Snap when close enough.
            if(diff < 0)
            {
                if(-diff <= MENU_MIN_STEP)
                    step = diff;
            }
            else
            {
                if(diff <= MENU_MIN_STEP)
                    step = diff;
            }

            pos += step;


            // ---------------------------------------------------------------
            // FAST RENDER
            //
            // No large glyphs during animation.
            //
            // This is the major SPI performance improvement.
            // ---------------------------------------------------------------

            Menu_RenderStrip(pos,
                             count,
                             0);

            redrawFrame = true;


            // ---------------------------------------------------------------
            // If we reached the target, perform one complete render.
            // ---------------------------------------------------------------

            if(pos == target)
            {
                Menu_RenderStrip(pos,
                                 count,
                                 1);

                Menu_DrawPager(sel,
                               count);

                Menu_DrawLabels(sel);

                redrawFrame = true;
            }
        }


        //*********************************************************************
        // Pulse selection frame
        //*********************************************************************

#if MENU_PULSE

        tick++;

        newStep = (int16_t)((tick >> 2) & 15);

        if(newStep != pStep)
        {
            pStep = newStep;
            redrawFrame = true;
        }

#endif


        //*********************************************************************
        // Redraw frame after strip rendering.
        //
        // The strip can overwrite the frame while tiles move.
        //*********************************************************************

        if(redrawFrame)
        {
            Menu_DrawFrame(Menu_PulseColor(sel,
                                           pStep));
        }


        //*********************************************************************
        // Frame timing
        //*********************************************************************

        DELAY_US(MENU_LOOP_US);
    }
}
