//#############################################################################
// FILE:   app/wason/wason.c
// TITLE:  Wason Ware - framework and app descriptor (see wason.h)
//
//   This file is the "game show host": title -> taunt -> intro -> play ->
//   result -> ... -> game over, the fuse bar, the practice picker, the saved
//   best score (drivers/memory.h, MEM_SLOT_WASONWARE) and the callbacks.
//   The microgames themselves live in wason_<name>.c; see wason_priv.h for
//   the file map and how to add one.
//
//   CONTROLS (all games): stick or D-pad. "Any press" = stick button or any
//   D-pad button. Hold the stick button to open Settings (done by apps.c).
//   Title / game over: D or stick button = start, R = back to the menu.
//
//   C28x notes: char/int are 16 bits, so products that can pass 32767 are
//   cast to int32_t, and only int16_t/uint16_t/uint32_t are used.
//#############################################################################

#include "wason_priv.h"

enum { PH_TITLE, PH_INTRO, PH_PLAY, PH_RESULT, PH_OVER, PH_PICK, PH_TAUNT };

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
    ww_DrawArt(FACE_X, FACE_Y, ww_kArtWario, 24, 24, FACE_S, TAUNT_BG, true);
}

static void TauntDrawMouth(bool open)
{
    ww_DrawArt(MOUTH_X, MOUTH_Y, open ? ww_kArtMouthO : ww_kArtMouthC, 10, 5, FACE_S, TAUNT_BG, true);
}

static void TauntDrawText(void)
{
    char    a[16], b[16];
    int16_t i, la = ww_Min16((int16_t)s_tChars, 12), lb = ww_Max16(0, (int16_t)s_tChars - 13);

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
    ww_Text("button: skip", FIELD_W / 2, 230, COLOR_GRAY);
}

// ---- screens ----------------------------------------------------------------
static void Banner(const char *s, uint32_t bg, uint32_t fg)
{
    Graphics_FillRect(0, 85, FIELD_W - 1, 145, bg);
    ww_BoldText(s, FIELD_W / 2, 115, fg);
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
    ww_BoldText("WASON WARE", FIELD_W / 2 + 2, 72, COLOR_MAGENTA);
    ww_BoldText("WASON WARE", FIELD_W / 2,     70, COLOR_YELLOW);
    ww_Text("23 microgames. 4 lives. Go!", FIELD_W / 2, 105, COLOR_WHITE);

    ww_Label(buf, "Best: ", s_best);
    ww_Text(buf, FIELD_W / 2, 132, COLOR_GOLD);

    if(s_menu == 0) Graphics_FillRect(80, 152, 239, 177, COLOR_YELLOW);
    else            Graphics_FillRect(80, 182, 239, 207, COLOR_YELLOW);
    ww_Text("PLAY",     FIELD_W / 2, 165, s_menu == 0 ? COLOR_BLACK : COLOR_WHITE);
    ww_Text("PRACTICE", FIELD_W / 2, 195, s_menu == 1 ? COLOR_BLACK : COLOR_WHITE);
    ww_Text("U/D choose   button: go   R: back", FIELD_W / 2, 228, COLOR_GRAY);
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
    ww_Text(i == 0 ? "< BACK" : ww_games[i - 1].inspo, FIELD_W / 2, y,
         (i == s_sel) ? COLOR_BLACK : COLOR_WHITE);
}

static void DrawPickSpeed(void)
{
    char buf[24];

    Graphics_FillRect(0, 206, FIELD_W - 1, 226, COLOR_BLACK);
    ww_Label(buf, "Speed: ", (uint32_t)s_pLevel);
    ww_Text(buf, FIELD_W / 2, 218, COLOR_CYAN);
}

static void DrawPick(void)                          // whole page
{
    char    pg[8];
    int16_t page = s_sel / PICK_ROWS, i;

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);
    ww_BoldText("PRACTICE", FIELD_W / 2, 16, COLOR_GOLD);

    pg[0] = (char)('1' + page);  pg[1] = '/';  pg[2] = (char)('0' + PickPages());  pg[3] = 0;
    ww_Text(pg, 292, 16, COLOR_GRAY);

    for(i = page * PICK_ROWS; i < (page + 1) * PICK_ROWS; i++) DrawPickRow(i);

    DrawPickSpeed();
    ww_Text("U/D pick   L/R speed   button: go", FIELD_W / 2, 234, COLOR_GRAY);
}

static void DrawIntro(void)
{
    char    buf[24];
    int16_t i;
    const MicroGame *g = &ww_games[s_cur];

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);

    if(!s_practice)
        for(i = 0; i < WW_LIVES; i++)
            Graphics_FillRect(110 + i * 24, 12, 125 + i * 24, 27,
                              (uint16_t)i < s_lives ? COLOR_RED : COLOR_DARK_GRAY);

    Banner(g->cmd, kBanner[s_cur % 6], COLOR_BLACK);
    ww_Text(g->inspo, FIELD_W / 2, 165, COLOR_GRAY);
    if(s_practice)
    {
        ww_BoldText("PRACTICE", FIELD_W / 2, 40, COLOR_GOLD);
        ww_Label(buf, "Speed: ", s_level);
    }
    else
    {
        ww_Label(buf, "Score: ", s_score);
        if(s_speedUp) ww_BoldText("SPEED UP!", FIELD_W / 2, 55, COLOR_ORANGE);
    }
    ww_Text(buf, FIELD_W / 2, 205, COLOR_WHITE);
}

static void DrawOver(void)
{
    char buf[24];

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_FillRect(0, 0, FIELD_W - 1, 239, COLOR_BLACK);
    ww_BoldText("GAME OVER", FIELD_W / 2, 55, COLOR_RED);
    ww_Label(buf, "Score: ", s_score);
    ww_Text(buf, FIELD_W / 2, 105, COLOR_WHITE);
    ww_Label(buf, "Best: ", s_best);
    ww_Text(buf, FIELD_W / 2, 135, COLOR_GOLD);
    if(s_newBest) ww_BoldText("NEW BEST!", FIELD_W / 2, 170, COLOR_LIME);
    ww_Text("button: retry    R: back", FIELD_W / 2, 220, COLOR_GRAY);
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
    const MicroGame *g = &ww_games[s_cur];

    s_total = g->frames;
    if(!g->winOnTimeout)                            // "do it" games get tighter each level
        s_total -= (uint16_t)(((uint32_t)g->frames * ww_Min16((int16_t)s_level, 5) * 6) / 100);
    s_left  = s_total;
    ww_frame = 0;
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
    MemWason sv;

    s_newBest = (s_score > s_best);
    if(s_newBest)
    {
        s_best = s_score;
        sv.best        = s_best;
        sv.reserved[0] = 0;
        sv.reserved[1] = 0;
        Memory_Write(MEM_SLOT_WASONWARE, &sv, sizeof sv);
        Memory_Flush();                             // blocks briefly (flash)
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
    MemWason sv;

    s_best = Memory_Read(MEM_SLOT_WASONWARE, &sv, sizeof sv) ? sv.best : 0;
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
            if(ww_EdgeU() || ww_EdgeD()) { s_menu ^= 1; s_dirty = true; Buzzer_PlaySfx(&sfx_blip); }
            if(g_joy.btnPressed)
            {
                if(s_menu == 0) StartRun();
                else { Buzzer_PlaySfx(&sfx_menu_select); s_pickPart = 0; GoPhase(PH_PICK); }
            }
            break;

        case PH_PICK:
            if((ww_EdgeU() && s_sel > 0) || (ww_EdgeD() && s_sel < (int16_t)NUM_GAMES))
            {
                s_pickOld = s_sel;
                s_sel += ww_EdgeU() ? -1 : 1;
                if(s_sel / PICK_ROWS != s_pickOld / PICK_ROWS) s_dirty = true;   // new page: full repaint
                else s_pickPart |= 1;                                            // same page: 2 rows only
                Buzzer_PlaySfx(&sfx_blip);
            }
            if(ww_EdgeL() && s_pLevel > 0) { s_pLevel--; s_pickPart |= 2; }
            if(ww_EdgeR() && s_pLevel < 6) { s_pLevel++; s_pickPart |= 2; }
            if(g_joy.btnPressed)
            {
                if(s_sel == 0) GoPhase(PH_TITLE);
                else           StartPractice((uint16_t)(s_sel - 1));
            }
            break;

        case PH_TAUNT:
            if(g_joy.btnPressed || g_dpad.dPressed)
             {
            GoPhase(PH_INTRO);
             break;
             }

            if(s_tChars < TAUNT_LEN)
            {
            if((s_t % 4) == 0)
            {
                s_tChars++;

            if(s_tChars != 13) {
                WasonVoice_Tick(s_tChars);
                s_tOpen = !s_tOpen;
            }
            else {
                s_tOpen = false;  // space
            }

            s_tPart |= 3;
        }
    }
            else
            {
            if(s_tOpen) {
                s_tOpen = false;
                s_tPart |= 2;
            }

            if(++s_tHold > 50)
                GoPhase(PH_INTRO);
            }
            break;

        case PH_INTRO:
            if(s_t >= WW_INTRO_FRAMES) EnterPlay();
            break;

        case PH_PLAY:
            ww_frame++;
            r = ww_games[s_cur].update();
            if(s_left > 0) s_left--;
            if(r == MG_PLAY && s_left == 0)
                r = ww_games[s_cur].winOnTimeout ? MG_WIN : MG_LOSE;
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
    const MicroGame *g = &ww_games[s_cur];

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
            ww_wx = g->wx;  ww_wy = g->wy;  ww_ww = g->ww;  ww_wh = g->wh;
            if(s_dirty)
            {
                ClearOutside(ww_wx, ww_wy, ww_ww, ww_wh);
                Graphics_FillRect(ww_wx, ww_wy, ww_wx + ww_ww - 1, ww_wy + ww_wh - 1, g->bg);
                BarFull();
                ww_first = true;
                s_dirty = false;
                s_repaintAll = false;
            }
            g->render();
            ww_first = false;
            ww_wx = 0;  ww_wy = 0;  ww_ww = FIELD_W;  ww_wh = FIELD_H;
            BarUpdate();
            break;

        case PH_RESULT:
            if(s_dirty)
            {
                Graphics_setFont(&g_sContext, &g_sFontCmss20b);
                if(s_repaintAll)                    // only after Settings wiped the screen
                {
                    ww_wx = g->wx;  ww_wy = g->wy;  ww_ww = g->ww;  ww_wh = g->wh;
                    ClearOutside(ww_wx, ww_wy, ww_ww, ww_wh);
                    Graphics_FillRect(ww_wx, ww_wy, ww_wx + ww_ww - 1, ww_wy + ww_wh - 1, g->bg);
                    ww_first = true;
                    g->render();
                    ww_first = false;
                    ww_wx = 0;  ww_wy = 0;  ww_ww = FIELD_W;  ww_wh = FIELD_H;
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
