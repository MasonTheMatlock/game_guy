//#############################################################################
// FILE:   app/settings.c
// TITLE:  Settings app - a stack of wide rectangles (OG Xbox dashboard style):
//         Volume, Brightness, Apps
//
// CONTROLS
//   Menu:
//     D-pad U/D or stick up/down : move the highlight
//     D-pad R or stick button tap: open
//     D-pad L                    : quit to the launcher
//   Volume / Brightness:
//     D-pad L/R or stick left/right : lower / raise
//     D-pad D  (Volume only)        : play a test sound
//     D-pad U or stick button tap   : back to the menu
//   Apps:
//     D-pad U / L or stick button tap : back to the menu
//   HOLD stick button: quit to the launcher (handled by apps.c)
//
// OVERLAY MODE: holding the stick inside any other app opens this app on top
// of it (g_settingsOverlay = true). The menu then also has Resume (top) and
// Main Menu (bottom); L = resume. Holding the stick in here also resumes.
//
// Same app contract as the others: update() = logic + dirty flags, render() =
// draw only what is dirty, one public symbol (g_settingsApp).
//#############################################################################

#include "settings.h"
#include "drivers/backlight.h"

//*****************************************************************************
// Screens and layout
//*****************************************************************************
#define MODE_MENU       0
#define MODE_VOLUME     1
#define MODE_BRIGHT     2
#define MODE_APPS       3

// Menu item ids. 1..3 double as the mode they open.
#define ID_VOLUME       1
#define ID_BRIGHT       2
#define ID_APPS         3
#define ID_RESUME       4               // overlay only
#define ID_MAINMENU     5               // overlay only

#define ITEM_MAX        5
static const char *const kIdName[6] =
    { "", "Volume", "Brightness", "Apps", "Resume", "Main Menu" };

// Set/read by the runner (apps.c); see settings.h.
bool g_settingsOverlay = false;
bool g_settingsToMenu  = false;

// Menu rectangles: ROW_X0..ROW_X1 wide; the selected one grows 10 px each side.
#define ROW_X0          40
#define ROW_X1          279
#define ROW_Y0          38
#define ROW_H           30
#define ROW_GAP         4

// Apps screen: outlined rectangles, one per installed app.
#define APPS_X0         50
#define APPS_X1         269
#define APPS_Y0         46
#define APPS_H          24
#define APPS_PITCH      32
#define APPS_MAX_ROWS   5

#define LEVEL_TEXT_Y    100             // "3 / 5"
#define LEVEL_BAR_BOTTOM 170

#define EDGE_ON         60              // stick flick thresholds (same as test app)
#define EDGE_OFF        30

#define COL_GREEN       0x3C9A2EUL      // selected rectangle
#define COL_EDGE        0x8CE070UL      // its bright border
#define COL_ROW_OFF     0x282828UL      // unselected rectangle
#define COL_BAR_OFF     0x404040UL

//*****************************************************************************
// State
//*****************************************************************************
static uint16_t s_mode;
static uint16_t s_sel;                  // highlighted menu row
static uint16_t s_ids[ITEM_MAX];        // item id shown in each menu row
static uint16_t s_count;                // number of menu rows
static int16_t  s_stX, s_stY;           // stick flick detection
static bool     s_dirtyFull;            // whole screen must be redrawn
static uint16_t s_dirtyRows;            // bit i = menu rectangle i needs redrawing
static bool     s_dirtyValue;           // volume / brightness readout needs redrawing

//*****************************************************************************
// Small helpers
//*****************************************************************************

// One event per stick push: +1 / -1 when the axis crosses EDGE_ON, then 0
// until it has gone back inside EDGE_OFF.
static int16_t StickEdge(int16_t v, int16_t *state)
{
    int16_t target = *state;

    if(v >= EDGE_ON)                          target = 1;
    else if(v <= -EDGE_ON)                    target = -1;
    else if(v < EDGE_OFF && v > -EDGE_OFF)    target = 0;

    if(target != *state)
    {
        *state = target;
        return target;                        // 0 on release = "no event"
    }
    return 0;
}

// Write v as decimal text into p and terminate it. Returns characters written.
static int16_t Itoa(char *p, uint16_t v)
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

static void DrawText(const char *s, int16_t x, int16_t y, uint32_t color)
{
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH,
                                x, y, TRANSPARENT_TEXT);
}

static void DrawTitle(const char *title)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    DrawText(title, SCREEN_W / 2, 15, COLOR_WHITE);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
}

static void DrawHint(const char *s)
{
    DrawText(s, SCREEN_W / 2, 228, COLOR_GRAY);
}

// 2-pixel outline (grlib's Graphics_drawRectangle wants a struct; this is simpler).
static void FrameRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color)
{
    Graphics_FillRect(x0, y0,     x1,     y0 + 1, color);
    Graphics_FillRect(x0, y1 - 1, x1,     y1,     color);
    Graphics_FillRect(x0, y0,     x0 + 1, y1,     color);
    Graphics_FillRect(x1 - 1, y0, x1,     y1,     color);
}

//*****************************************************************************
// Menu drawing
//*****************************************************************************
static void DrawMenuRow(uint16_t i)
{
    int16_t top = ROW_Y0 + (int16_t)i * (ROW_H + ROW_GAP);
    bool    sel = (i == s_sel);

    // Clear the whole band first: the selected rectangle is wider than the others.
    Graphics_FillRect(10, top - 1, SCREEN_W - 11, top + ROW_H, COLOR_BLACK);

    if(sel)
    {
        Graphics_FillRect(ROW_X0 - 10, top,     ROW_X1 + 10, top + ROW_H - 1, COL_EDGE);
        Graphics_FillRect(ROW_X0 - 8,  top + 2, ROW_X1 + 8,  top + ROW_H - 3, COL_GREEN);
    }
    else
    {
        Graphics_FillRect(ROW_X0, top, ROW_X1, top + ROW_H - 1, COL_ROW_OFF);
    }

    DrawText(kIdName[s_ids[i]], SCREEN_W / 2, top + ROW_H / 2,
             sel ? COLOR_WHITE : COLOR_GRAY);
}

//*****************************************************************************
// Volume / Brightness drawing: "n / N" and a row of bars that grow in height.
//*****************************************************************************
static void DrawLevel(void)
{
    uint16_t n     = (s_mode == MODE_VOLUME) ? BUZZER_NUM_VOLUMES : BACKLIGHT_NUM_LEVELS;
    uint16_t level = (s_mode == MODE_VOLUME) ? Buzzer_GetVolume() : Backlight_GetLevel();
    char     buf[12];
    int16_t  k;
    int16_t  slot, barW, gap, x0;
    uint16_t i;

    // Readout
    k = Itoa(buf, level + 1);
    buf[k++] = ' ';
    buf[k++] = '/';
    buf[k++] = ' ';
    Itoa(buf + k, n);

    Graphics_FillRect(60, LEVEL_TEXT_Y - 14, SCREEN_W - 61, LEVEL_TEXT_Y + 14, COLOR_BLACK);
    DrawText(buf, SCREEN_W / 2, LEVEL_TEXT_Y, COLOR_WHITE);

    // Bars: fit n bars into ~240 px, centered.
    slot = 240 / (int16_t)n;
    barW = (slot * 3) / 4;
    gap  = slot - barW;
    x0   = SCREEN_W / 2 - ((int16_t)n * slot - gap) / 2;

    for(i = 0; i < n; i++)
    {
        int16_t  x = x0 + (int16_t)i * slot;
        int16_t  h = 8 + ((int16_t)i * 28) / ((int16_t)n - 1);
        uint32_t c = (i <= level) ? COL_GREEN : COL_BAR_OFF;

        Graphics_FillRect(x, LEVEL_BAR_BOTTOM - h, x + barW - 1, LEVEL_BAR_BOTTOM, c);
    }
}

//*****************************************************************************
// Apps drawing: one outlined rectangle per installed app, in the app's color.
//*****************************************************************************
static void DrawApps(void)
{
    uint16_t i;

    for(i = 0; i < g_appCount && i < APPS_MAX_ROWS; i++)
    {
        int16_t top = APPS_Y0 + (int16_t)i * APPS_PITCH;

        FrameRect(APPS_X0, top, APPS_X1, top + APPS_H - 1, g_apps[i]->color);
        DrawText(g_apps[i]->name, SCREEN_W / 2, top + APPS_H / 2, COLOR_WHITE);
    }
}

//*****************************************************************************
// Mode switching (logic only; Render draws via s_dirtyFull)
//*****************************************************************************
static void EnterMode(uint16_t mode)
{
    s_mode      = mode;
    s_dirtyFull = true;
}

//*****************************************************************************
// init: runs once when the app starts.
//*****************************************************************************
static void Settings_Init(void)
{
    s_count = 0;
    if(g_settingsOverlay) s_ids[s_count++] = ID_RESUME;
    s_ids[s_count++] = ID_VOLUME;
    s_ids[s_count++] = ID_BRIGHT;
    s_ids[s_count++] = ID_APPS;
    if(g_settingsOverlay) s_ids[s_count++] = ID_MAINMENU;

    g_settingsToMenu = false;
    s_sel        = 0;
    s_stX        = 0;
    s_stY        = 0;
    s_dirtyRows  = 0;
    s_dirtyValue = false;
    EnterMode(MODE_MENU);               // Render draws it on the first frame
}

//*****************************************************************************
// update: once per frame. LOGIC ONLY.
//*****************************************************************************
static AppStatus Settings_Update(void)
{
    int16_t sx    = StickEdge(g_joy.dx, &s_stX);
    int16_t sy    = StickEdge(g_joy.dy, &s_stY);      // dy is +DOWN
    bool    up    = g_dpad.uPressed || (sy < 0);
    bool    down  = g_dpad.dPressed || (sy > 0);
    bool    left  = g_dpad.lPressed || (sx < 0);
    bool    right = g_dpad.rPressed || (sx > 0);
    bool    tap   = g_joy.btnPressed;
    uint16_t level;
    uint16_t old;

    switch(s_mode)
    {
        case MODE_MENU:
            if(left)
                return APP_QUIT;        // overlay: resume; launcher: exit

            if(up || down)
            {
                old   = s_sel;
                s_sel = up ? (s_sel + s_count - 1) % s_count
                           : (s_sel + 1) % s_count;
                s_dirtyRows |= (1 << old) | (1 << s_sel);
                Buzzer_PlaySfx(&sfx_menu_move);                 // <-- new
            }

            if(right || tap)
            {
                uint16_t id = s_ids[s_sel];

                if(id == ID_RESUME)
                    return APP_QUIT;
                if(id == ID_MAINMENU)
                {
                    g_settingsToMenu = true;
                    return APP_QUIT;
                }
                EnterMode(id);
                Buzzer_PlaySfx(&sfx_menu_select);               // <-- new, before EnterMode(id)
            }
            break;

        case MODE_VOLUME:
            if(up || tap)
            {
                EnterMode(MODE_MENU);
                break;
            }

            level = Buzzer_GetVolume();
            old   = level;
            if(right && level < BUZZER_NUM_VOLUMES - 1) level++;
            if(left  && level > 0)                      level--;
            if(level != old)
            {
                Buzzer_SetVolume(level);
                Buzzer_PlaySfx(&sfx_coin);              // hear the new level
                s_dirtyValue = true;
            }
            else if(down)
            {
                Buzzer_PlaySfx(&sfx_coin);              // test sound
            }
            break;

        case MODE_BRIGHT:
            if(up || tap)
            {
                EnterMode(MODE_MENU);
                break;
            }

            level = Backlight_GetLevel();
            old   = level;
            if(right && level < BACKLIGHT_NUM_LEVELS - 1) level++;
            if(left  && level > 0)                        level--;
            if(level != old)
            {
                Backlight_SetLevel(level);
                s_dirtyValue = true;
            }
            break;

        default:                                        // MODE_APPS
            if(up || left || tap)
                EnterMode(MODE_MENU);
            break;
    }

    return APP_CONTINUE;
}

//*****************************************************************************
// render: once per frame, AFTER update. DRAWING ONLY.
//*****************************************************************************
static void Settings_Render(void)
{
    uint16_t i;

    if(s_dirtyFull)
    {
        s_dirtyFull = false;

        switch(s_mode)
        {
            case MODE_MENU:
                DrawTitle("Settings");
                DrawHint(g_settingsOverlay ? "U/D:pick  R:select  L:resume"
                                           : "U/D:pick  R:open  L:exit");
                s_dirtyRows = (1 << s_count) - 1;       // all rectangles
                break;

            case MODE_VOLUME:
                DrawTitle("Volume");
                DrawHint("L/R:change  D:test  U:back");
                s_dirtyValue = true;
                break;

            case MODE_BRIGHT:
                DrawTitle("Brightness");
                DrawHint("L/R:change  U:back");
                s_dirtyValue = true;
                break;

            default:                                    // MODE_APPS
                DrawTitle("Apps");
                DrawHint("U/L:back");
                DrawApps();
                break;
        }
    }

    if(s_mode == MODE_MENU)
    {
        for(i = 0; i < s_count; i++)
            if(s_dirtyRows & (1 << i)) DrawMenuRow(i);
        s_dirtyRows = 0;
    }
    else if(s_dirtyValue)
    {
        DrawLevel();
        s_dirtyValue = false;
    }
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//   Already declared in apps.h and listed in g_apps[] in app/apps.c.
//*****************************************************************************
const App g_settingsApp =
{
    "Settings",                         // name (first letter = menu tile icon)
    "Change User Settings",             // hint shown under the menu
    COLOR_DARK_GRAY,                    // menu tile color
    RC_FRAME_US,                        // frame period
    Settings_Init, Settings_Update, Settings_Render,
    Save_SyncSettings,                  // exit(): save volume + brightness to flash
    NULL,                               // not a legacy app
    NULL                                // no redraw() (Settings is never paused)
};
