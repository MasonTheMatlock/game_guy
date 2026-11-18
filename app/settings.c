//#############################################################################
// FILE:   app/settings.c
// TITLE:  PAUSE MENU - Volume, Brightness and the running app's own settings
//
// Two ways in:
//   * from the launcher tile "Settings":   Volume, Brightness, Apps
//                                          Apps lists every app; each opens ITS page
//   * by holding the joystick button inside an app (the pause overlay): Resume,
//     Volume, Brightness, <that app's> settings (only its own), Main Menu
//     apps.c runs the overlay on top of the app that was running (g_pausedApp).
//
// An app's settings are plain data (see AppSettings in apps.h): info rows,
// choice rows, colour pickers and actions with an optional confirm screen.
// This file draws and runs them, so an app never draws its own settings UI.
//
// Same app contract as the others: update() = logic + dirty flags, render() =
// draw only what is dirty, one public symbol (g_settingsApp).
//#############################################################################

#include "settings.h"
#include "drivers/backlight.h"


static const uint16_t s_mp3Vol[6] = { 2, 4, 6, 8, 10, 13 };   // DFPlayer volume (0..30) per level
//*****************************************************************************
// Screens and layout
//*****************************************************************************
#define MODE_MENU       0
#define MODE_VOLUME     1
#define MODE_BRIGHT     2
#define MODE_APPSET     3               // the running app's settings list
#define MODE_COLOR      4               // SET_COLOR swatch picker
#define MODE_CONFIRM    5               // "are you sure?" Cancel / <button>
#define MODE_RESULT     6               // text an action returned
#define MODE_APPS       7               // Settings > Apps: pick an app

// Menu item ids. 1, 2 double as the mode they open.
#define ID_VOLUME       1
#define ID_BRIGHT       2
#define ID_APPSET       3
#define ID_RESUME       4               // overlay only
#define ID_MAINMENU     5               // overlay only
#define ID_APPS         6               // launcher tile only

#define ITEM_MAX        5
static const char *const kIdName[7] =
    { "", "Volume", "Brightness", "", "Resume", "Main Menu", "Apps" };

// Set/read by the runner (apps.c); see settings.h.
bool g_settingsOverlay = false;
bool g_settingsToMenu  = false;

// Rectangles: ROW_X0..ROW_X1 wide; the selected one grows 10 px each side.
#define ROW_X0          40
#define ROW_X1          279
#define ROW_Y0          38
#define ROW_H           30
#define ROW_GAP         4

// Colour picker: name, preview box, row of swatches.
#define BG_NAME_Y       72
#define BG_PREV_X0      60
#define BG_PREV_X1      259
#define BG_PREV_Y0      90
#define BG_PREV_Y1      150
#define BG_SW_Y         170
#define BG_SW_ROW_W     280             // all swatches together
#define BG_SW_MAXW      28
#define BG_SW_GAP       6
#define BG_MAX          12

// Confirm / result screens: three lines of text, then two buttons.
#define FR_TEXT_Y0      58
#define FR_TEXT_PITCH   26
#define FR_BTN_Y0       132
#define FR_BTN_NUM      2               // 0 = Cancel, 1 = the action

#define LEVEL_TEXT_Y    100             // "3 / 5"
#define LEVEL_BAR_BOTTOM 170

#define EDGE_ON         60              // stick flick thresholds (same as test app)
#define EDGE_OFF        30

#define COL_GREEN       0x3C9A2EUL      // selected rectangle
#define COL_EDGE        0x8CE070UL      // its bright border
#define COL_ROW_OFF     0x282828UL      // unselected rectangle
#define COL_BAR_OFF     0x404040UL
#define COL_RED_EDGE    0xFF6060UL      // selected dangerous button
#define COL_RED         0xA02020UL

//*****************************************************************************
// State
//*****************************************************************************
static uint16_t s_mode;
static uint16_t s_sel;                  // highlighted pause-menu row
static uint16_t s_ids[ITEM_MAX];        // item id shown in each menu row
static uint16_t s_count;                // number of menu rows
static int16_t  s_stX, s_stY;           // stick flick detection
static bool     s_dirtyFull;            // whole screen must be redrawn
static uint16_t s_dirtyRows;            // bit i = rectangle i needs redrawing
static bool     s_dirtyValue;           // readout / picker needs redrawing

static const AppSettings *s_set;        // the running app's page (NULL = none)
static uint16_t s_setSel;               // highlighted row on that page
static uint16_t s_appSel;               // highlighted app in Settings > Apps
static bool     s_fromApps;             // the page was opened from the Apps list
static const SettingItem *s_cur;        // item being picked / confirmed / reported
static uint16_t s_pick;                 // SET_COLOR: highlighted palette entry
static uint16_t s_confSel;              // confirm: 0 = Cancel, 1 = do it
static const char *s_result;            // text returned by the last action

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

// Copy the n-th '|'-separated piece of s into buf (at most 31 characters).
static const char *Piece(const char *s, uint16_t n, char *buf)
{
    uint16_t k = 0;

    while(n && *s)
    {
        if(*s++ == '|') n--;
    }
    while(*s && *s != '|' && k < 31) buf[k++] = *s++;
    buf[k] = 0;
    return buf;
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

// One selectable band (menu row, settings row, confirm button).
static void DrawBand(int16_t top, bool sel, const char *text, bool dim, uint32_t edge, uint32_t fill)
{
    // Clear the whole band first: the selected rectangle is wider than the others.
    Graphics_FillRect(10, top - 1, SCREEN_W - 11, top + ROW_H, COLOR_BLACK);

    if(sel)
    {
        Graphics_FillRect(ROW_X0 - 10, top,     ROW_X1 + 10, top + ROW_H - 1, edge);
        Graphics_FillRect(ROW_X0 - 8,  top + 2, ROW_X1 + 8,  top + ROW_H - 3, dim ? COL_ROW_OFF : fill);
    }
    else
    {
        Graphics_FillRect(ROW_X0, top, ROW_X1, top + ROW_H - 1, COL_ROW_OFF);
    }

    DrawText(text, SCREEN_W / 2, top + ROW_H / 2,
             dim ? COLOR_DARK_GRAY : (sel ? COLOR_WHITE : COLOR_GRAY));
}

//*****************************************************************************
// Pause-menu drawing
//*****************************************************************************
static const char *MenuLabel(uint16_t id)
{
    if(id == ID_APPSET) return s_set->title;
    return kIdName[id];
}

static void DrawMenuRow(uint16_t i)
{
    DrawBand(ROW_Y0 + (int16_t)i * (ROW_H + ROW_GAP), i == s_sel,
             MenuLabel(s_ids[i]), false, COL_EDGE, COL_GREEN);
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
// App settings page: one band per item. Choice / info rows show "Label: value".
//*****************************************************************************
static bool ItemEnabled(const SettingItem *it)
{
    return (it->enabled == NULL) || it->enabled();
}

static void DrawSetRow(uint16_t i)
{
    const SettingItem *it = &s_set->items[i];
    char     buf[48];
    int16_t  n = 0;
    const char *p = it->label;

    while(*p && n < 24) buf[n++] = *p++;
    if((it->kind == SET_CHOICE || it->kind == SET_INFO) && it->text)
    {
        buf[n++] = ':';
        buf[n++] = ' ';
        p = it->text();
        while(*p && n < 46) buf[n++] = *p++;
    }
    buf[n] = 0;

    DrawBand(ROW_Y0 + (int16_t)i * (ROW_H + ROW_GAP), i == s_setSel, buf,
             !ItemEnabled(it), COL_EDGE, COL_GREEN);
}

//*****************************************************************************
// Colour picker: name, a preview box in that colour, and a row of swatches with
// the chosen one outlined in white.
//*****************************************************************************
static void DrawColorPicker(void)
{
    uint16_t n = (s_cur->nColors > BG_MAX) ? BG_MAX : s_cur->nColors;
    uint16_t i;
    int16_t  w, x, x0;

    Graphics_FillRect(60, BG_NAME_Y - 14, SCREEN_W - 61, BG_NAME_Y + 14, COLOR_BLACK);
    DrawText(s_cur->colorName(s_pick), SCREEN_W / 2, BG_NAME_Y, COLOR_WHITE);

    FrameRect(BG_PREV_X0 - 3, BG_PREV_Y0 - 3, BG_PREV_X1 + 3, BG_PREV_Y1 + 3, COLOR_WHITE);
    Graphics_FillRect(BG_PREV_X0, BG_PREV_Y0, BG_PREV_X1, BG_PREV_Y1, s_cur->color(s_pick));
    if(s_cur->preview)
        DrawText(s_cur->preview, SCREEN_W / 2, (BG_PREV_Y0 + BG_PREV_Y1) / 2, COLOR_WHITE);

    w = (BG_SW_ROW_W - ((int16_t)n - 1) * BG_SW_GAP) / (int16_t)n;
    if(w > BG_SW_MAXW) w = BG_SW_MAXW;
    x0 = (SCREEN_W - ((int16_t)n * w + ((int16_t)n - 1) * BG_SW_GAP)) / 2;

    // Every swatch has a frame so Black is visible on the black screen.
    for(i = 0; i < n; i++)
    {
        x = x0 + (int16_t)i * (w + BG_SW_GAP);

        FrameRect(x - 3, BG_SW_Y - 3, x + w + 2, BG_SW_Y + w + 2,
                  (i == s_pick) ? COLOR_WHITE : COL_BAR_OFF);
        Graphics_FillRect(x, BG_SW_Y, x + w - 1, BG_SW_Y + w - 1, s_cur->color(i));
    }
}

//*****************************************************************************
// Confirm and result screens (three '|'-separated lines of text each)
//*****************************************************************************
static void DrawLines(const char *text, uint32_t c0)
{
    char     buf[32];
    uint16_t i;

    for(i = 0; i < 3; i++)
    {
        Piece(text, i, buf);
        if(buf[0]) DrawText(buf, SCREEN_W / 2, FR_TEXT_Y0 + (int16_t)i * FR_TEXT_PITCH, (i == 0) ? c0 : COLOR_GRAY);
    }
}

static void DrawConfirmButton(uint16_t i)
{
    bool danger = s_cur->danger && (i == 1);

    DrawBand(FR_BTN_Y0 + (int16_t)i * (ROW_H + ROW_GAP), i == s_confSel,
             (i == 0) ? "Cancel" : (s_cur->confirmButton ? s_cur->confirmButton : s_cur->label),
             false, danger ? COL_RED_EDGE : COL_EDGE, danger ? COL_RED : COL_GREEN);
}

//*****************************************************************************
// Settings > Apps: one band per app (the list is capped at ITEM_MAX rows; with
// more apps this needs paging). An app with no settings page is shown dimmed.
//*****************************************************************************
static uint16_t AppRows(void)
{
    return (g_appCount > ITEM_MAX) ? ITEM_MAX : g_appCount;
}

static void DrawAppRow(uint16_t i)
{
    const App *a = g_apps[i];

    DrawBand(ROW_Y0 + (int16_t)i * (ROW_H + ROW_GAP), i == s_appSel, a->name,
             a->settings == NULL, COL_EDGE, COL_GREEN);
}

//*****************************************************************************
// Mode switching (logic only; Render draws via s_dirtyFull)
//*****************************************************************************
static void EnterMode(uint16_t mode)
{
    s_mode      = mode;
    s_dirtyFull = true;
    s_dirtyRows = 0;                    // Render sets the rows the new screen needs
}

//*****************************************************************************
// init: runs once when the app starts.
//*****************************************************************************
static void Settings_Init(void)
{
    s_set = (g_pausedApp != NULL && g_pausedApp->settings != NULL && g_pausedApp->settings->count)
          ? g_pausedApp->settings : NULL;

    s_count = 0;
    if(g_settingsOverlay) s_ids[s_count++] = ID_RESUME;
    s_ids[s_count++] = ID_VOLUME;
    s_ids[s_count++] = ID_BRIGHT;
    if(g_settingsOverlay)
    {
        if(s_set) s_ids[s_count++] = ID_APPSET;         // pause menu: THIS app's settings only
        s_ids[s_count++] = ID_MAINMENU;
    }
    else
        s_ids[s_count++] = ID_APPS;                     // launcher tile: every app's settings

    g_settingsToMenu = false;
    s_sel        = 0;
    s_setSel     = 0;
    s_appSel     = 0;
    s_fromApps   = false;
    s_confSel    = 0;                   // Cancel is the safe default
    s_cur        = NULL;
    s_result     = NULL;
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
    uint16_t n;

    switch(s_mode)
    {
        case MODE_MENU:
            if(left)
                return APP_QUIT;        // overlay: resume

            if(up || down)
            {
                old   = s_sel;
                s_sel = up ? (s_sel + s_count - 1) % s_count
                           : (s_sel + 1) % s_count;
                s_dirtyRows |= (1 << old) | (1 << s_sel);
                Buzzer_PlaySfx(&sfx_menu_move);
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
                Buzzer_PlaySfx(&sfx_menu_select);
                if(id == ID_APPS) { EnterMode(MODE_APPS); break; }
                if(id == ID_APPSET) { s_fromApps = false; s_setSel = 0; }
                EnterMode(id == ID_APPSET ? MODE_APPSET : id);
            }
            break;

        case MODE_VOLUME:
            if(up || tap)
            {
                Memory_SyncSettings();        // write to flash now, not only when Settings exits
                EnterMode(MODE_MENU);
                break;
            }

            level = Buzzer_GetVolume();
            old   = level;
            if(right && level < BUZZER_NUM_VOLUMES - 1) level++;
            if(left  && level > 0)                      level--;
            if(level != old)
            {   
               MP3_setVolume(s_mp3Vol[level]);
                MP3_playTrack(0005);
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
                Memory_SyncSettings();        // write to flash now, not only when Settings exits
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

        case MODE_APPS:
            n = AppRows();
            if(left)
            {
                Buzzer_PlaySfx(&sfx_menu_back);
                EnterMode(MODE_MENU);
                break;
            }

            if(up || down)
            {
                old      = s_appSel;
                s_appSel = up ? (s_appSel + n - 1) % n : (s_appSel + 1) % n;
                s_dirtyRows |= (1 << old) | (1 << s_appSel);
                Buzzer_PlaySfx(&sfx_menu_move);
            }

            if(right || tap)
            {
                if(g_apps[s_appSel]->settings != NULL && g_apps[s_appSel]->settings->count)
                {
                    s_set      = g_apps[s_appSel]->settings;
                    s_setSel   = 0;
                    s_fromApps = true;
                    Buzzer_PlaySfx(&sfx_menu_select);
                    EnterMode(MODE_APPSET);
                }
                else
                {
                    Buzzer_PlaySfx(&sfx_menu_bump);     // nothing to configure for this app yet
                }
            }
            break;

        case MODE_APPSET:
            if(left)
            {
                Memory_Flush();               // choices / colours are RAM until now
                Buzzer_PlaySfx(&sfx_menu_back);
                EnterMode(s_fromApps ? MODE_APPS : MODE_MENU);
                break;
            }

            n = s_set->count;
            if(up || down)
            {
                old      = s_setSel;
                s_setSel = up ? (s_setSel + n - 1) % n : (s_setSel + 1) % n;
                s_dirtyRows |= (1 << old) | (1 << s_setSel);
                Buzzer_PlaySfx(&sfx_menu_move);
            }

            if(right || tap)
            {
                const SettingItem *it = &s_set->items[s_setSel];

                s_cur = it;
                switch(it->kind)
                {
                    case SET_CHOICE:
                        if(it->step) it->step(1);
                        s_dirtyRows |= (1 << s_setSel);
                        Buzzer_PlaySfx(&sfx_menu_move);
                        break;

                    case SET_COLOR:
                        s_pick = it->get ? it->get() : 0;
                        Buzzer_PlaySfx(&sfx_menu_select);
                        EnterMode(MODE_COLOR);
                        break;

                    case SET_ACTION:
                        if(it->confirm)
                        {
                            s_confSel = 0;                      // always start on Cancel
                            Buzzer_PlaySfx(&sfx_menu_select);
                            EnterMode(MODE_CONFIRM);
                        }
                        else
                        {
                            s_result = it->run ? it->run() : NULL;
                            Buzzer_PlaySfx(&sfx_menu_select);
                            EnterMode(s_result ? MODE_RESULT : MODE_APPSET);
                            if(!s_result) s_dirtyRows = (1 << n) - 1;
                        }
                        break;

                    default:                                    // SET_INFO
                        Buzzer_PlaySfx(&sfx_menu_bump);
                        break;
                }
            }
            break;

        case MODE_COLOR:
            if(up || tap)
            {
                Memory_Flush();               // leaving the screen: write to flash if changed
                EnterMode(MODE_APPSET);
                break;
            }

            n   = (s_cur->nColors > BG_MAX) ? BG_MAX : s_cur->nColors;
            old = s_pick;
            if(right) s_pick = (s_pick + 1) % n;
            if(left)  s_pick = (s_pick + n - 1) % n;
            if(s_pick != old)
            {
                if(s_cur->set) s_cur->set(s_pick);   // RAM only; the app reads it when it next draws
                Buzzer_PlaySfx(&sfx_menu_move);
                s_dirtyValue = true;
            }
            break;

        case MODE_CONFIRM:
            if(left)                                    // back out, nothing changed
            {
                Buzzer_PlaySfx(&sfx_menu_back);
                EnterMode(MODE_APPSET);
                break;
            }

            if(up || down)
            {
                s_confSel ^= 1;                         // only two buttons
                s_dirtyRows |= (1 << FR_BTN_NUM) - 1;
                Buzzer_PlaySfx(&sfx_menu_move);
            }

            if(right || tap)
            {
                if(s_confSel == 0)                      // Cancel
                {
                    Buzzer_PlaySfx(&sfx_menu_back);
                    EnterMode(MODE_APPSET);
                    break;
                }
                s_result = s_cur->run ? s_cur->run() : NULL;   // may block briefly (flash write)
                Buzzer_PlaySfx(&sfx_menu_select);
                EnterMode(s_result ? MODE_RESULT : MODE_APPSET);
                if(!s_result) s_dirtyRows = (1 << s_set->count) - 1;
            }
            break;

        default:                                        // MODE_RESULT
            if(left || right || up || down || tap)
            {
                Buzzer_PlaySfx(&sfx_menu_back);
                EnterMode(MODE_APPSET);
            }
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
                DrawTitle(g_settingsOverlay ? "Pause" : "Settings");
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

            case MODE_APPS:
                DrawTitle("Apps");
                DrawHint("U/D:pick  R:open  L:back");
                s_dirtyRows = (1 << AppRows()) - 1;
                break;

            case MODE_APPSET:
                DrawTitle(s_set->title);
                DrawHint("U/D:pick  R:choose  L:back");
                s_dirtyRows = (1 << s_set->count) - 1;  // all rows
                break;

            case MODE_COLOR:
                DrawTitle(s_cur->label);
                DrawHint("L/R:change  U:back");
                s_dirtyValue = true;
                break;

            case MODE_CONFIRM:
                DrawTitle(s_cur->label);
                DrawLines(s_cur->confirm, COLOR_WHITE);
                DrawHint("U/D:pick  R:choose  L:back");
                s_dirtyRows = (1 << FR_BTN_NUM) - 1;    // both buttons
                break;

            default:                                    // MODE_RESULT
                DrawTitle(s_cur ? s_cur->label : s_set->title);
                if(s_result) DrawLines(s_result, COLOR_WHITE);
                DrawHint("Any button: back");
                break;
        }
    }

    if(s_mode == MODE_MENU)
    {
        for(i = 0; i < s_count; i++)
            if(s_dirtyRows & (1 << i)) DrawMenuRow(i);
        s_dirtyRows = 0;
    }
    else if(s_mode == MODE_APPS)
    {
        for(i = 0; i < AppRows(); i++)
            if(s_dirtyRows & (1 << i)) DrawAppRow(i);
        s_dirtyRows = 0;
    }
    else if(s_mode == MODE_APPSET)
    {
        for(i = 0; i < s_set->count; i++)
            if(s_dirtyRows & (1 << i)) DrawSetRow(i);
        s_dirtyRows = 0;
    }
    else if(s_mode == MODE_CONFIRM)
    {
        for(i = 0; i < FR_BTN_NUM; i++)
            if(s_dirtyRows & (1 << i)) DrawConfirmButton(i);
        s_dirtyRows = 0;
    }
    else if(s_dirtyValue)
    {
        if(s_mode == MODE_COLOR) DrawColorPicker();
        else if(s_mode == MODE_VOLUME || s_mode == MODE_BRIGHT) DrawLevel();
        s_dirtyValue = false;
    }
}

//*****************************************************************************
// The Settings entry in Settings > Apps: Factory Reset (asks first).
// Appends one record of factory values (blocks briefly) and sets the live volume and
// brightness. Reachable only from the launcher's Settings tile, so no game is paused
// underneath whose cached scores could be written back afterwards.
//*****************************************************************************
static const char *FactoryReset(void)
{
    return Memory_FactoryReset()
        ? "Factory reset complete.|All saves and settings|are back to defaults."
        : "Factory reset failed.|The flash write did not|work. Try again.";
}

static const SettingItem s_selfItems[] =
{
    {   "Factory Reset", SET_ACTION,
        NULL, NULL,
        0, NULL, NULL, NULL, NULL, NULL,
        FactoryReset, "Erase ALL saved data?|Scores and all settings|This cannot be undone.",
        "Erase Everything", true, NULL },
};

static const AppSettings s_selfSettings = { "Factory Reset", s_selfItems, 1 };

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//   Not listed in g_apps[]: the runner opens it on top of the running app.
//*****************************************************************************
const App g_settingsApp =
{
    "Settings",                         // name (launcher tile and Settings > Apps row)
    "Volume, brightness, app settings", // hint
    COLOR_DARK_GRAY,                    // color
    RC_FRAME_US,                        // frame period
    Settings_Init, Settings_Update, Settings_Render,
    Memory_SyncSettings,                // exit(): save volume + brightness to flash
    NULL,                               // not a legacy app
    NULL,                               // no redraw() (the pause menu is never paused)
    &s_selfSettings                     // Settings > Apps > Settings = Factory Reset
};
