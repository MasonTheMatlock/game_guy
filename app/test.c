//#############################################################################
// FILE:   app/test.c
// TITLE:  Test app - the smallest useful example of the callback app contract
//
// WHAT IT DOES
//   A colored square you move around a play area.
//
// CONTROLS
//   stick            : move the square (|stick| < 20% is ignored = deadzone)
//   stick button tap
//   or D-pad D       : cycle the square's color
//   D-pad C          : quit to the menu (shows an app-initiated APP_QUIT)
//   HOLD stick button: quit to the menu (free, handled by main.c)
//
// WHAT IT DEMONSTRATES
//   * State lives in file-static variables (survives between frames).
//   * init()   draws everything from scratch.
//   * update() changes state and sets dirty flags. It never draws.
//   * render() draws ONLY what the dirty flags say changed (the LCD is slow).
//   * No while(1), no Joy_Update(), no Dpad_Update(), no DELAY_US():
//     main.c's RunApp() does all of that.
//   * One public symbol: g_testApp.
//#############################################################################

#include "test.h"      // g_testApp, layout and feel constants

//*****************************************************************************
// State
//*****************************************************************************
static const uint32_t kColors[] =
{
    COLOR_RED,                         // red
    0x40FF40UL,                         // green
    0x4080FFUL,                         // blue
    0xFFD040UL                          // yellow
};
#define TEST_NUM_COLORS  (sizeof(kColors) / sizeof(kColors[0]))

static int16_t  s_x, s_y;               // square's top-left corner
static int16_t  s_oldX, s_oldY;         // where it was last drawn (to erase it)
static uint16_t s_colorIdx;
static uint16_t s_frames;

// Dirty flags: Update sets them, Render consumes them.
static bool s_dirtySquare;              // square moved or changed color
static bool s_dirtyCounter;             // counter text needs redrawing

//*****************************************************************************
// Small helpers
//*****************************************************************************

// Write v as decimal text into p and terminate it. Returns characters written.
static int16_t Test_Itoa(char *p, uint16_t v)
{
    char    tmp[6];
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

    p[n] = 0;
    return n;
}

static int16_t Clamp16(int16_t v, int16_t lo, int16_t hi)
{
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static void Test_DrawSquare(int16_t x, int16_t y, uint32_t color)
{
    Graphics_FillRect(x, y, x + TEST_SQUARE - 1, y + TEST_SQUARE - 1, color);
}

static void Test_DrawCounter(void)
{
    char buf[16] = "frames: ";

    Test_Itoa(buf + 8, s_frames);

    // Clear the old text first (the new number may be shorter), then draw
    // with a transparent background.
    Graphics_FillRect(0, TEST_COUNTER_Y - 10, SCREEN_W - 1, TEST_COUNTER_Y + 10,
                      COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH,
                                SCREEN_W / 2, TEST_COUNTER_Y, TRANSPARENT_TEXT);
}

//*****************************************************************************
// init: runs once when the app starts. Draw everything from scratch.
//*****************************************************************************
static void Test_Init(void)
{
    // 1. Reset state.
    s_x = (TEST_PLAY_X0 + TEST_PLAY_X1) / 2 - TEST_SQUARE / 2;
    s_y = (TEST_PLAY_Y0 + TEST_PLAY_Y1) / 2 - TEST_SQUARE / 2;
    s_oldX = s_x;
    s_oldY = s_y;
    s_colorIdx = 0;
    s_frames = 0;

    // 2. Draw the screen. Never assume what the LCD showed before: it was the
    //    menu.
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"TEST APP",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15,
                                OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);

    // Play area border, 1 pixel outside the area the square can reach.
    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawLine(&g_sContext, TEST_PLAY_X0 - 1, TEST_PLAY_Y0 - 1,
                      TEST_PLAY_X1 + 1, TEST_PLAY_Y0 - 1);
    Graphics_drawLine(&g_sContext, TEST_PLAY_X0 - 1, TEST_PLAY_Y1 + 1,
                      TEST_PLAY_X1 + 1, TEST_PLAY_Y1 + 1);
    Graphics_drawLine(&g_sContext, TEST_PLAY_X0 - 1, TEST_PLAY_Y0 - 1,
                      TEST_PLAY_X0 - 1, TEST_PLAY_Y1 + 1);
    Graphics_drawLine(&g_sContext, TEST_PLAY_X1 + 1, TEST_PLAY_Y0 - 1,
                      TEST_PLAY_X1 + 1, TEST_PLAY_Y1 + 1);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext,
                                (int16_t *)"Btn:color C:menu hold:menu",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, TEST_HINT_Y,
                                TRANSPARENT_TEXT);

    Test_DrawSquare(s_x, s_y, kColors[s_colorIdx]);
    Test_DrawCounter();

    // 3. Nothing is pending: everything above is already on screen.
    s_dirtySquare  = false;
    s_dirtyCounter = false;
}

//*****************************************************************************
// update: once per frame. LOGIC ONLY. No drawing here.
//*****************************************************************************
static AppStatus Test_Update(void)
{
    int16_t nx;
    int16_t ny;

    // An app-initiated quit. (Hold-the-button quit is handled by main.c.)
    if(g_dpad.cPressed)
        return APP_QUIT;

    s_frames++;

    // Move. g_joy.dx / dy are -100..100.
    nx = s_x + (g_joy.dx / TEST_STICK_DIV);
#if TEST_INVERT_Y
    ny = s_y - (g_joy.dy / TEST_STICK_DIV);
#else
    ny = s_y + (g_joy.dy / TEST_STICK_DIV);
#endif

    nx = Clamp16(nx, TEST_PLAY_X0, TEST_PLAY_X1 - TEST_SQUARE + 1);
    ny = Clamp16(ny, TEST_PLAY_Y0, TEST_PLAY_Y1 - TEST_SQUARE + 1);

    if(nx != s_x || ny != s_y)
    {
        s_oldX = s_x;                   // Render needs this to erase the old one
        s_oldY = s_y;
        s_x = nx;
        s_y = ny;
        s_dirtySquare = true;
    }

    // Tap = cycle color. btnPressed is a one-frame edge, not a hold.
    if(g_joy.btnPressed || g_dpad.dPressed)
    {
        s_colorIdx = (uint16_t)((s_colorIdx + 1) % TEST_NUM_COLORS);
        s_dirtySquare = true;
    }

    // Only redraw the text now and then; text is the most expensive thing here.
    if((s_frames % TEST_COUNTER_EVERY) == 0)
        s_dirtyCounter = true;

    return APP_CONTINUE;
}

//*****************************************************************************
// render: once per frame. DRAWING ONLY. Draw just what Update flagged.
//*****************************************************************************
static void Test_Render(void)
{
    if(s_dirtySquare)
    {
        // Erase where it was, then draw where it is now. (If it did not move,
        // old == current and this just repaints it in the new color.)
        Test_DrawSquare(s_oldX, s_oldY, COLOR_BLACK);
        Test_DrawSquare(s_x, s_y, kColors[s_colorIdx]);

        s_oldX = s_x;
        s_oldY = s_y;
    }

    if(s_dirtyCounter)
        Test_DrawCounter();

    s_dirtySquare  = false;
    s_dirtyCounter = false;
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//   Register it: extern in apps.h, one pointer in g_apps[] in main.c.
//*****************************************************************************
const App g_testApp =
{
    "TEST",                             // name (first letter = menu tile icon)
    "Stick: move  Btn: color",          // hint shown under the menu
    COLOR_CYAN,                         // menu tile color
    TEST_FRAME_US,                      // frame period
    Test_Init, Test_Update, Test_Render,
    NULL,                               // no exit() cleanup needed
    NULL                                // not a legacy app
};
