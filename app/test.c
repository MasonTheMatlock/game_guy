//#############################################################################
// FILE:   app/test.c
// TITLE:  Test app - menu with an INPUT TEST and a SOUND screen
//
// MENU
//   stick up/down      : choose
//   D-pad D or stick btn tap : open
//   D-pad R            : quit to the launcher
//
// INPUT TEST  (all four D-pad buttons are being tested, so they can't be "back")
//   shows the stick as a dot, a hollow square where it is predicted to be next frame,
//   the raw X/Y numbers, and U/L/R/D + stick button lighting up on presses.
//   R + D together     : back to the menu
//   HOLD stick button  : quit to the launcher (main.c)
//
// SOUND
//   stick up/down      : choose row (Song / Effect / Volume)
//   stick left/right   : change that row's value
//   D-pad D            : play (song row = start/restart looping, effect row =
//                        play the effect over the music, volume row = beep)
//   stick btn tap      : stop all sound
//   D-pad R            : back to the menu (stops sound)
//
// Same app contract as before: update() = logic + dirty flags, render() = draw
// only what is dirty, one public symbol (g_testApp).
//#############################################################################

#include "test.h"

//*****************************************************************************
// Screens
//*****************************************************************************
#define MODE_MENU       0
#define MODE_INPUT      1
#define MODE_SOUND      2

#define MENU_ITEMS      2
#define SND_ROWS        3
#define ROW_SONG        0
#define ROW_SFX         1
#define ROW_VOL         2

#define DIRTY_BARS      0x08            // bit 3 of s_dirtyRow: volume bars (bits 0-2 = rows)

#define PAD_COUNT       5               // U L R D + stick button
#define PAD_STK         4

#define COL_GREEN       0x40FF40UL
#define COL_YELLOW      0xFFD040UL

static const char *const kMenuText[MENU_ITEMS] = { "Input test", "Sound" };
static const char *const kPadLabel[PAD_COUNT]  = { "U", "L", "R", "D", "STK" };

// Sound effects the Sound screen can play. Add more here (name + pointer).
// Sound effects the Sound screen can play. Add more here, one line each.
// The row label comes from each Song's .name, so there's no second list to keep in sync.
static const Song *const kSfx[] =
{
    &sfx_coin,
    &sfx_menu_move,
    &sfx_menu_bump,
    &sfx_menu_select,
    &sfx_menu_back,
    &sfx_pause,
    &sfx_resume,
    &sfx_blip,
    &sfx_thud,
    &sfx_jump,
    &sfx_laser,
    &sfx_powerup,
    &sfx_hit,
    &sfx_explosion,
    &sfx_level_up,
    &sfx_game_over,

    &sfx_tetris_say,
    &sfx_boot_dmg,
    &sfx_boot_gbc
};

#define NUM_SFX  (sizeof(kSfx) / sizeof(kSfx[0]))

//*****************************************************************************
// State
//*****************************************************************************
static uint16_t s_mode;
static uint16_t s_frames;
static bool     s_dirtyFull;            // whole screen must be redrawn
static uint16_t s_dirtyRow;             // menu rows / sound rows / bars

// Stick "flick" detection (one event per push, with hysteresis).
static int16_t  s_stX, s_stY;

// Menu
static uint16_t s_menuSel;

// Input test
static int16_t  s_jx, s_jy;             // current stick reading
static int16_t  s_lastX, s_lastY;       // previous frame (for the prediction)
static int16_t  s_prX, s_prY;           // predicted next reading
static uint16_t s_lit[PAD_COUNT];       // frames left to stay lit
static uint16_t s_litShown[PAD_COUNT];  // what is currently drawn (0xFFFF = unknown)
static bool     s_dirtyStick, s_dirtyPads, s_dirtyReadout;
static int16_t  s_drawDotX, s_drawDotY; // where the markers were last drawn (to erase)
static int16_t  s_drawRingX, s_drawRingY;

// Sound
static uint16_t s_sndRow;
static uint16_t s_songIdx;              // survives between launches
static uint16_t s_sfxIdx;
static uint16_t s_musicOn;

//*****************************************************************************
// Small helpers
//*****************************************************************************
static int16_t Clamp16(int16_t v, int16_t lo, int16_t hi)
{
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

// One event per stick push: returns +1 / -1 when the axis crosses TEST_EDGE_ON,
// then 0 until it has gone back inside TEST_EDGE_OFF.
static int16_t StickEdge(int16_t v, int16_t *state)
{
    int16_t target = *state;

    if(v >= TEST_EDGE_ON)                          target = 1;
    else if(v <= -TEST_EDGE_ON)                    target = -1;
    else if(v < TEST_EDGE_OFF && v > -TEST_EDGE_OFF) target = 0;

    if(target != *state)
    {
        *state = target;
        return target;                              // 0 on release = "no event"
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

static int16_t FmtInt(char *p, int16_t v)
{
    int16_t n = 0;

    if(v < 0)
    {
        p[n++] = '-';
        v = -v;
    }
    return n + Itoa(p + n, (uint16_t)v);
}

// Append up to maxLen characters of s to dst at position n. Returns new n.
static int16_t Cat(char *dst, int16_t n, const char *s, int16_t maxLen)
{
    while(*s && maxLen-- > 0)
        dst[n++] = *s++;
    dst[n] = 0;
    return n;
}

static void DrawText(const char *s, int16_t x, int16_t y, uint32_t color, bool opaque)
{
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH,
                                x, y, opaque ? OPAQUE_TEXT : TRANSPARENT_TEXT);
}

static void DrawTitle(const char *title)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    DrawText(title, SCREEN_W / 2, 15, COLOR_WHITE, true);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
}

static void DrawHint(const char *s)
{
    DrawText(s, SCREEN_W / 2, 228, COLOR_GRAY, false);
}

//*****************************************************************************
// MENU drawing
//*****************************************************************************
#define MENU_Y0   90
#define MENU_DY   40

static void DrawMenuRow(uint16_t i)
{
    int16_t y   = MENU_Y0 + (int16_t)i * MENU_DY;
    bool    sel = (i == s_menuSel);

    Graphics_FillRect(10, y - 14, SCREEN_W - 11, y + 14, COLOR_BLACK);
    DrawText(kMenuText[i], SCREEN_W / 2, y, sel ? COLOR_CYAN : COLOR_GRAY, false);
    if(sel)
        DrawText(">", 75, y, COLOR_CYAN, false);
}

//*****************************************************************************
// INPUT TEST drawing
//*****************************************************************************
static int16_t ToPx(int16_t v)          // -100..100 -> -45..45 (stays inside the box)
{
    return (v * 9) / 20;
}

static int16_t ToPy(int16_t v)
{
#if TEST_INVERT_Y
    return -ToPx(v);
#else
    return ToPx(v);
#endif
}

static void EraseAt(int16_t x, int16_t y)
{
    Graphics_FillRect(x - TEST_ERASE_R, y - TEST_ERASE_R,
                      x + TEST_ERASE_R, y + TEST_ERASE_R, COLOR_BLACK);
}

// Hollow square centered on (x, y), half-size r, in the current foreground color.
// (grlib has no circle, and Graphics_drawRectangle wants a Graphics_Rectangle struct.)
static void DrawSquareOutline(int16_t x, int16_t y, int16_t r)
{
    Graphics_drawLine(&g_sContext, x - r, y - r, x + r, y - r);
    Graphics_drawLine(&g_sContext, x - r, y + r, x + r, y + r);
    Graphics_drawLine(&g_sContext, x - r, y - r, x - r, y + r);
    Graphics_drawLine(&g_sContext, x + r, y - r, x + r, y + r);
}

static void DrawStick(void)
{
    int16_t dx = TEST_BOX_CX + ToPx(s_jx);
    int16_t dy = TEST_BOX_CY + ToPy(s_jy);
    int16_t rx = TEST_BOX_CX + ToPx(s_prX);
    int16_t ry = TEST_BOX_CY + ToPy(s_prY);

    EraseAt(s_drawDotX,  s_drawDotY);
    EraseAt(s_drawRingX, s_drawRingY);

    // Crosshair (the erase squares may have clipped it).
    Graphics_setForegroundColor(&g_sContext, COLOR_DARK_GRAY);
    Graphics_drawLine(&g_sContext, TEST_BOX_CX - TEST_BOX_HALF + 1, TEST_BOX_CY,
                      TEST_BOX_CX + TEST_BOX_HALF - 1, TEST_BOX_CY);
    Graphics_drawLine(&g_sContext, TEST_BOX_CX, TEST_BOX_CY - TEST_BOX_HALF + 1,
                      TEST_BOX_CX, TEST_BOX_CY + TEST_BOX_HALF - 1);

    Graphics_setForegroundColor(&g_sContext, COL_YELLOW);       // predicted: hollow square
    DrawSquareOutline(rx, ry, TEST_RING_R);
    Graphics_FillRect(dx - TEST_DOT_R, dy - TEST_DOT_R,         // current: filled square
                      dx + TEST_DOT_R, dy + TEST_DOT_R, COL_GREEN);

    s_drawDotX  = dx;  s_drawDotY  = dy;
    s_drawRingX = rx;  s_drawRingY = ry;
}

static void DrawReadout(void)
{
    char    buf[20];
    int16_t n;

    n = Cat(buf, 0, "X:", 2);
    n += FmtInt(buf + n, s_jx);
    n = Cat(buf, n, " Y:", 3);
    FmtInt(buf + n, s_jy);

    Graphics_FillRect(TEST_BOX_CX - 70, TEST_READOUT_Y - 10,
                      TEST_BOX_CX + 70, TEST_READOUT_Y + 10, COLOR_BLACK);
    DrawText(buf, TEST_BOX_CX, TEST_READOUT_Y, COLOR_WHITE, false);
}

static void DrawPad(uint16_t i, bool lit)
{
    int16_t cx = TEST_PAD_CX;
    int16_t cy = TEST_PAD_CY;
    int16_t hw = TEST_PAD_HALF;
    int16_t hh = TEST_PAD_HALF;

    switch(i)
    {
        case 0: cy -= TEST_PAD_OFS; break;          // U: top
        case 1: cx -= TEST_PAD_OFS; break;          // L: left
        case 2: cx += TEST_PAD_OFS; break;          // R: right
        case 3: cy += TEST_PAD_OFS; break;          // D: bottom
        default:                                    // stick button: wide strip
            cy = TEST_STK_Y;
            hw = 30;
            hh = 12;
            break;
    }

    Graphics_setForegroundColor(&g_sContext, lit ? COL_GREEN : COLOR_DARK_GRAY);
    Graphics_FillRect(cx - hw, cy - hh, cx + hw, cy + hh,
                      lit ? COL_GREEN : COLOR_DARK_GRAY);
    DrawText(kPadLabel[i], cx, cy, lit ? COLOR_BLACK : COLOR_WHITE, false);
}

static void DrawInputStatic(void)
{
    DrawTitle("Input Test");

    DrawText("STICK",   TEST_BOX_CX, 48, COLOR_GRAY, false);
    DrawText("BUTTONS", TEST_PAD_CX, 48, COLOR_GRAY, false);

    // Box, 1 pixel outside the area the markers can reach.
    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawLine(&g_sContext, TEST_BOX_CX - TEST_BOX_HALF, TEST_BOX_CY - TEST_BOX_HALF,
                      TEST_BOX_CX + TEST_BOX_HALF, TEST_BOX_CY - TEST_BOX_HALF);
    Graphics_drawLine(&g_sContext, TEST_BOX_CX - TEST_BOX_HALF, TEST_BOX_CY + TEST_BOX_HALF,
                      TEST_BOX_CX + TEST_BOX_HALF, TEST_BOX_CY + TEST_BOX_HALF);
    Graphics_drawLine(&g_sContext, TEST_BOX_CX - TEST_BOX_HALF, TEST_BOX_CY - TEST_BOX_HALF,
                      TEST_BOX_CX - TEST_BOX_HALF, TEST_BOX_CY + TEST_BOX_HALF);
    Graphics_drawLine(&g_sContext, TEST_BOX_CX + TEST_BOX_HALF, TEST_BOX_CY - TEST_BOX_HALF,
                      TEST_BOX_CX + TEST_BOX_HALF, TEST_BOX_CY + TEST_BOX_HALF);

    DrawHint("R+D: back  Hold btn: quit");
}

//*****************************************************************************
// SOUND drawing
//*****************************************************************************
static void BuildRow(char *buf, uint16_t row)
{
    bool    sel = (row == s_sndRow);
    int16_t n;

    n = Cat(buf, 0, sel ? "< " : "  ", 2);

    if(row == ROW_SONG)
    {
        n = Cat(buf, n, "Song: ", 6);
        n = Cat(buf, n, songList[s_songIdx]->name, 16);
    }
        else if(row == ROW_SFX)
    {
        n = Cat(buf, n, "Effect: ", 8);
        n = Cat(buf, n, kSfx[s_sfxIdx]->name, 16);      // was kSfxName[s_sfxIdx]
    }
    else
    {
        n = Cat(buf, n, "Volume: ", 8);
        n += Itoa(buf + n, Buzzer_GetVolume() + 1);
        n = Cat(buf, n, "/5", 2);
    }

    Cat(buf, n, sel ? " >" : "  ", 2);
}

static void DrawSndRow(uint16_t row)
{
    char    buf[40];
    int16_t y = TEST_ROW_Y0 + (int16_t)row * TEST_ROW_DY;

    BuildRow(buf, row);
    Graphics_FillRect(10, y - 14, SCREEN_W - 11, y + 14, COLOR_BLACK);
    DrawText(buf, SCREEN_W / 2, y, (row == s_sndRow) ? COLOR_CYAN : COLOR_GRAY, false);
}

// Volume slider: five bars of growing height, filled up to the current level.
static void DrawBars(void)
{
    uint16_t i;
    uint16_t level = Buzzer_GetVolume();
    uint32_t onColor = (s_sndRow == ROW_VOL) ? COLOR_CYAN : COLOR_WHITE;

    for(i = 0; i < BUZZER_NUM_VOLUMES; i++)
    {
        int16_t x0 = TEST_BAR_X0 + (int16_t)i * (TEST_BAR_W + TEST_BAR_GAP);
        int16_t h  = TEST_BAR_H0 + (int16_t)i * 4;
        uint32_t c = (i <= level) ? onColor : COLOR_DARK_GRAY;

        Graphics_setForegroundColor(&g_sContext, c);
        Graphics_FillRect(x0, TEST_BAR_BOTTOM - h, x0 + TEST_BAR_W - 1, TEST_BAR_BOTTOM, c);
    }
}

//*****************************************************************************
// Mode switching (logic only: draws happen in Render via s_dirtyFull)
//*****************************************************************************
static void EnterMode(uint16_t mode)
{
    uint16_t i;

    s_mode      = mode;
    s_dirtyFull = true;

    if(mode == MODE_INPUT)
    {
        s_jx = s_jy = s_lastX = s_lastY = s_prX = s_prY = 0;
        s_drawDotX  = s_drawRingX = TEST_BOX_CX;
        s_drawDotY  = s_drawRingY = TEST_BOX_CY;
        for(i = 0; i < PAD_COUNT; i++)
        {
            s_lit[i]      = 0;
            s_litShown[i] = 0xFFFF;                 // force the first draw
        }
    }
    else if(mode == MODE_SOUND)
    {
        s_sndRow  = ROW_SONG;
        s_musicOn = 0;
    }
}

//*****************************************************************************
// init: runs once when the app starts.
//*****************************************************************************
static void Test_Init(void)
{
    s_frames  = 0;
    s_stX     = 0;
    s_stY     = 0;
    s_menuSel = 0;
    s_sfxIdx  = 0;
    s_dirtyRow = 0;

    Buzzer_Stop();
    EnterMode(MODE_MENU);               // Render draws it on the first frame
}

//*****************************************************************************
// update: once per frame. LOGIC ONLY.
//*****************************************************************************
static AppStatus Test_UpdateMenu(int16_t sy)
{
    if(g_dpad.rPressed)
        return APP_QUIT;

    if(sy != 0)
    {
        uint16_t sel = (sy > 0) ? 1 : 0;        // down = second item
        if(sel != s_menuSel)
        {
            s_menuSel  = sel;
            s_dirtyRow = 0x03;                  // redraw both rows
        }
    }

    if(g_dpad.dPressed || g_joy.btnPressed)
        EnterMode(s_menuSel ? MODE_SOUND : MODE_INPUT);

    return APP_CONTINUE;
}

static AppStatus Test_UpdateInput(void)
{
    uint16_t i;
    bool     changed;

    // Joystick + prediction (linear extrapolation of the last step).
    s_lastX = s_jx;
    s_lastY = s_jy;
    s_jx    = g_joy.dx;
    s_jy    = g_joy.dy;

    {
        int16_t px = Clamp16(s_jx + TEST_PRED_GAIN * (s_jx - s_lastX), -100, 100);
        int16_t py = Clamp16(s_jy + TEST_PRED_GAIN * (s_jy - s_lastY), -100, 100);

        changed = (px != s_prX) || (py != s_prY) ||
                  (s_jx != s_lastX) || (s_jy != s_lastY);
        s_prX = px;
        s_prY = py;
    }

    if(changed) s_dirtyStick = true;
    if(s_jx != s_lastX || s_jy != s_lastY) s_dirtyReadout = true;

    // Buttons: count down, then re-arm on this frame's edges.
    for(i = 0; i < PAD_COUNT; i++)
        if(s_lit[i]) s_lit[i]--;

    if(TEST_PAD_U)        s_lit[0]       = TEST_PAD_LIT_FRAMES;
    if(TEST_PAD_L)        s_lit[1]       = TEST_PAD_LIT_FRAMES;
    if(TEST_PAD_R)        s_lit[2]       = TEST_PAD_LIT_FRAMES;
    if(TEST_PAD_D)        s_lit[3]       = TEST_PAD_LIT_FRAMES;
    if(g_joy.btnPressed)  s_lit[PAD_STK] = TEST_PAD_LIT_FRAMES;

    for(i = 0; i < PAD_COUNT; i++)
        if((s_lit[i] != 0) != (bool)s_litShown[i]) s_dirtyPads = true;

    // R + D pressed together (both edges within TEST_CHORD_SLACK frames) = back.
    if(s_lit[2] >= TEST_PAD_LIT_FRAMES - TEST_CHORD_SLACK &&
       s_lit[3] >= TEST_PAD_LIT_FRAMES - TEST_CHORD_SLACK)
        EnterMode(MODE_MENU);

    return APP_CONTINUE;
}

static void Test_SndChange(int16_t dir)
{
    uint16_t level;

    switch(s_sndRow)
    {
        case ROW_SONG:
            s_songIdx = (uint16_t)((s_songIdx + (dir > 0 ? 1 : numSongs - 1)) % numSongs);
            if(s_musicOn) Buzzer_PlaySong(songList[s_songIdx], 1);
            s_dirtyRow |= (1 << ROW_SONG);
            break;

        case ROW_SFX:
            s_sfxIdx = (uint16_t)((s_sfxIdx + (dir > 0 ? 1 : NUM_SFX - 1)) % NUM_SFX);
            s_dirtyRow |= (1 << ROW_SFX);
            break;

        default:                                // ROW_VOL
            level = Buzzer_GetVolume();
            if(dir > 0 && level < BUZZER_NUM_VOLUMES - 1) level++;
            if(dir < 0 && level > 0)                      level--;
            Buzzer_SetVolume(level);
            if(!Buzzer_IsPlaying())             // nothing to hear the change on: beep
                Buzzer_PlaySfx(&sfx_coin);
            s_dirtyRow |= (1 << ROW_VOL) | DIRTY_BARS;
            break;
    }
}

static AppStatus Test_UpdateSound(int16_t sx, int16_t sy)
{
    if(g_dpad.rPressed)
    {
        Buzzer_Stop();
        EnterMode(MODE_MENU);
        return APP_CONTINUE;
    }

    if(sy != 0)                                 // change row (wraps)
    {
        s_dirtyRow |= (1 << s_sndRow) | DIRTY_BARS;
        s_sndRow = (sy > 0) ? (s_sndRow + 1) % SND_ROWS
                            : (s_sndRow + SND_ROWS - 1) % SND_ROWS;
        s_dirtyRow |= (1 << s_sndRow);
    }

    if(sx != 0)
        Test_SndChange(sx);

    if(g_dpad.dPressed)
    {
        switch(s_sndRow)
        {
            case ROW_SONG:
                Buzzer_PlaySong(songList[s_songIdx], 1);
                s_musicOn = 1;
                break;
            case ROW_SFX:
                Buzzer_PlaySfx(kSfx[s_sfxIdx]);
                break;
            default:
                Buzzer_PlaySfx(&sfx_coin);
                break;
        }
    }

    if(g_joy.btnPressed)
    {
        Buzzer_Stop();
        s_musicOn = 0;
    }

    return APP_CONTINUE;
}

static AppStatus Test_Update(void)
{
    int16_t sx = StickEdge(g_joy.dx, &s_stX);
    int16_t sy = StickEdge(g_joy.dy, &s_stY);

    s_frames++;

    switch(s_mode)
    {
        case MODE_INPUT: return Test_UpdateInput();
        case MODE_SOUND: return Test_UpdateSound(sx, sy);
        default:         return Test_UpdateMenu(sy);
    }
}

//*****************************************************************************
// render: once per frame. DRAWING ONLY.
//*****************************************************************************
static void Test_Render(void)
{
    uint16_t i;
    bool     full = s_dirtyFull;

    if(full)
    {
        s_dirtyFull = false;

        if(s_mode == MODE_INPUT)
        {
            DrawInputStatic();
            s_dirtyStick = s_dirtyPads = s_dirtyReadout = true;
        }
        else if(s_mode == MODE_SOUND)
        {
            DrawTitle("Sound");
            DrawText("Stick: pick / change", SCREEN_W / 2, 52, COLOR_GRAY, false);
            DrawHint("D:play Btn:stop R:back");
            s_dirtyRow = 0x0F;
        }
        else
        {
            DrawTitle("TEST APP");
            DrawHint("Stick: pick  D: open");
            s_dirtyRow = 0x03;
        }
    }

    if(s_mode == MODE_INPUT)
    {
        if(s_dirtyStick)
        {
            DrawStick();
            s_dirtyStick = false;
        }

        // Text is the slowest thing here: at most every 3rd frame.
        if(s_dirtyReadout && (full || (s_frames % 3) == 0))
        {
            DrawReadout();
            s_dirtyReadout = false;
        }

        if(s_dirtyPads)
        {
            for(i = 0; i < PAD_COUNT; i++)
            {
                uint16_t lit = (s_lit[i] != 0);
                if(lit != s_litShown[i])
                {
                    DrawPad(i, lit);
                    s_litShown[i] = lit;
                }
            }
            s_dirtyPads = false;
        }
    }
    else if(s_mode == MODE_SOUND)
    {
        for(i = 0; i < SND_ROWS; i++)
            if(s_dirtyRow & (1 << i)) DrawSndRow(i);
        if(s_dirtyRow & DIRTY_BARS) DrawBars();
        s_dirtyRow = 0;
    }
    else
    {
        for(i = 0; i < MENU_ITEMS; i++)
            if(s_dirtyRow & (1 << i)) DrawMenuRow(i);
        s_dirtyRow = 0;
    }
}

//*****************************************************************************
// exit: leaving the app silences the buzzer.
//*****************************************************************************
static void Test_Exit(void)
{
    Buzzer_Stop();
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//*****************************************************************************
const App g_testApp =
{
    "TEST",                             // name (first letter = menu tile icon)
    "Input and sound tests",            // hint shown under the menu
    COLOR_CYAN,                         // menu tile color
    TEST_FRAME_US,                      // frame period
    Test_Init, Test_Update, Test_Render,
    Test_Exit,
    NULL                                // not a legacy app
};
