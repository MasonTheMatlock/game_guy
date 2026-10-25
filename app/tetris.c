//#############################################################################
// FILE:   app/tetris.c
// TITLE:  Tetris (callback app contract)
//
// CONTROLS
//   stick left/right : move piece (hold to auto-repeat)
//   stick down       : soft drop (fall faster, +1 point per row)
//   button tap       : rotate
//   HOLD stick button: quit to the menu (free, handled by main.c)
//   D-pad            : A = left, B = right, C = soft drop, D = rotate
//                      (change the mapping in the KEY_ defines below)
//   Title screen     : Start / High Scores (section 5b below)
//   After GAME OVER  : if the score makes the top 5, any button opens the
//                      name-entry screen (5 characters), then the title
//                      screen. Otherwise any button returns to the title.
//
// HOW THIS FILE IS ORGANIZED
//   1. Constants         board size, timing, piece shapes, colors, scoring
//   2. Game state        every variable that changes while playing
//   3. Board rules       Fits / place / clear rows - NO drawing in here
//   4. Drawing           LCD output; only repaints what changed
//   5. Game flow         spawn, reset, shift, rotate, lock
//   5b. Title + scores   Start / High Scores screen, name entry, top-5 list
//                        saved in flash. FUNCTION ORDER MATTERS in this
//                        section: each function must be defined above the
//                        first function that calls it.
//   6. App callbacks     Tetris_Init / Tetris_Update / Tetris_Render
//
// DESIGN NOTES
//   * update() = rules and input, render() = drawing. update() never touches
//     the LCD; render() never changes game state.
//   * Pieces are 16-bit bitmasks (see s_shapes).
//   * Drawing is "diff based": s_shown remembers what the LCD currently
//     shows, and only cells that differ are repainted.
//   * Pieces come from a "7-bag" shuffle.
//   * Timing is frame based: gravity is "move down every N frames".
//   * No while(1), no Joy_Update(), no Dpad_Update(), no DELAY_US():
//     main.c's RunApp() does all of that. Even the game-over wait is a
//     state (s_gameOver + s_gameOverTimer), not a blocking loop.
//   * Line clears: LockAndSpawn() only DETECTS full rows and starts a short
//     flash (s_clearing). The rows are removed by FinishClear() when the flash
//     ends. A 4-row clear shows a TETRIS! banner; passing the saved #1 score
//     shows a NEW #1! banner (both drawn by DrawBanner()).
//   * One public symbol: g_tetrisApp.
//#############################################################################

#include "tetris.h"   // g_tetrisApp, board/timing constants, key map, TetrisPiece
#include <stdio.h>                      // sprintf, for the score text

//*****************************************************************************
// 1. CONSTANTS
//*****************************************************************************

// ---- Piece shapes ---------------------------------------------------------
// Each piece is a 4x4 grid packed into one 16-bit number. Bit 15 (0x8000) is
// the top-left cell, bit 0 the bottom-right, read row by row, 4 bits per row.
// Test cell (row, col):   mask & (0x8000 >> (row * 4 + col))
// 4 rotations per piece. Order: I, J, L, O, S, T, Z.
static const uint16_t s_shapes[7][4] =
{
    { 0x0F00, 0x2222, 0x00F0, 0x4444 },   // I
    { 0x44C0, 0x8E00, 0x6440, 0x0E20 },   // J
    { 0x4460, 0x0E80, 0xC440, 0x2E00 },   // L
    { 0xCC00, 0xCC00, 0xCC00, 0xCC00 },   // O
    { 0x06C0, 0x8C40, 0x6C00, 0x4620 },   // S
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 },   // T
    { 0x0C60, 0x4C80, 0xC600, 0x2640 }    // Z
};

// Cell colors. Index 0 = EMPTY. Index 1..7 = piece type + 1.
static const uint32_t s_colors[9] =
{
    COLOR_BLACK,
    COLOR_TETRIS_I_CYAN,
    COLOR_TETRIS_J_BLUE,
    COLOR_TETRIS_L_ORANGE,
    COLOR_TETRIS_O_YELLOW,
    COLOR_TETRIS_S_GREEN,
    COLOR_TETRIS_T_PURPLE,
    COLOR_TETRIS_Z_RED,
    COLOR_WHITE                 // 8 = line-clear flash (CELL_FLASH)
};

// ---- Scoring and speed ----------------------------------------------------
// Points for clearing 0..4 rows at once, multiplied by (level + 1).
static const uint16_t s_lineScore[5] = { 0, 40, 100, 300, 1200 };

// Gravity per level: the piece falls one row every N frames. Lower = faster.
static const uint16_t s_gravityFrames[20] =
    { 30, 26, 22, 18, 15, 12, 10, 8, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1 };

// ---- Name entry characters -------------------------------------------------
// The characters a player can pick, in cycling order. The grlib font draws
// them; this table only decides WHICH ones are selectable.
// (On the C28x sizeof counts 16-bit words = chars, so this is correct.)
static const char s_nameCharset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#define NAME_NCHARS   (sizeof(s_nameCharset) - 1)

// ---- Line-clear flash + banner ---------------------------------------------
#define CELL_FLASH    8             // s_colors[] index used for the white flash

// The banner ("TETRIS!" / "NEW #1!") is a box across the playfield covering
// cell rows BANNER_ROW0..BANNER_ROW1 (3 rows = 30 px). Because it lines up
// with whole cell rows, it can be erased just by marking those cells
// "unknown" in s_shown so the board repaints them.
#define BANNER_NONE   0
#define BANNER_TETRIS 1
#define BANNER_RECORD 2
#define BANNER_ROW0   9
#define BANNER_ROW1   11
#define BANNER_X0     TETRIS_BOARD_X
#define BANNER_X1     (TETRIS_BOARD_X + TETRIS_BOARD_W * TETRIS_CELL - 1)
#define BANNER_Y0     (TETRIS_BOARD_Y + BANNER_ROW0 * TETRIS_CELL)
#define BANNER_Y1     (TETRIS_BOARD_Y + (BANNER_ROW1 + 1) * TETRIS_CELL - 1)

//*****************************************************************************
// 2. GAME STATE
//*****************************************************************************


// Only LOCKED blocks live here. 0 = empty, otherwise a color index.
static uint16_t s_board[TETRIS_BOARD_H][TETRIS_BOARD_W];

// What the LCD is currently showing per cell. 0xFFFF = unknown, redraw.
static uint16_t s_shown[TETRIS_BOARD_H][TETRIS_BOARD_W];

static TetrisPiece    s_cur;                  // the falling piece
static int16_t  s_nextType;             // the piece that spawns next
static int16_t  s_shownNext;            // next-piece preview currently on the LCD

static uint32_t s_score;
static uint16_t s_lines;
static uint16_t s_level;                // 0..9

// Values currently shown in the side panel
static uint32_t s_shownScore;
static uint16_t s_shownLines;
static uint16_t s_shownLevel;

static uint16_t s_gravityCount;         // frames since the piece last fell

// 7-bag randomizer
static int16_t  s_bag[7];
static int16_t  s_bagIdx;               // next card to deal; 7 means "deck empty"

// Input / frame state that used to be locals in the old Tetris_Run() loop.
// It has to be file-static now because update() returns every frame.
static uint16_t s_frameNo;              // paces the soft drop
static int16_t  s_hTimer;               // countdown for left/right auto-repeat
static int16_t  s_prevMoveDir;          // last frame's merged left/right input

// Game over state
static bool     s_gameOver;             // true once the stack reached the top
static uint16_t s_gameOverTimer;        // frames left in the "ignore input" window
static bool     s_gameOverDrawn;        // GAME OVER text already on the LCD
static int16_t  s_gameOverRank;         // high-score rank earned this game, -1 = none

// Line-clear animation: full rows flash white for TETRIS_CLEAR_FRAMES, THEN
// they are removed. The board is frozen while this runs.
static bool     s_clearing;
static uint16_t s_clearTimer;           // frames left in the flash
static uint32_t s_clearMask;            // bit r = row r is being cleared
static int16_t  s_clearCount;           // rows being cleared (1..4)

// Banner over the playfield
static uint16_t s_bannerKind;           // BANNER_NONE / _TETRIS / _RECORD
static uint16_t s_bannerTimer;          // frames left on screen
static bool     s_bannerDirty;          // box must be (re)drawn
static bool     s_bannerErase;          // box just expired: repaint the board under it
static bool     s_recordShown;          // NEW #1! already shown this game

// Title screen (Start / High Scores) is shown before each game.
static bool     s_inTitle;
static uint16_t s_titleView;            // TITLE_VIEW_MENU or TITLE_VIEW_SCORES
static uint16_t s_titleSel;             // highlighted row: 0 Start, 1 High Scores
static int16_t  s_titleStickY;          // stick flick detection
static bool     s_titleDirtyFull;       // whole title screen must be redrawn
static uint16_t s_titleDirtyRows;       // bit i = menu rectangle i needs redrawing

// High scores: RAM copy of the saved top-5 list (best first, 0 = empty).
static SaveTetrisScores s_hs;
static SaveTetrisNames  s_hsn;          // names matching s_hs.score[]
static bool     s_hsLoaded;

// Name entry (after a top-5 game over)
static bool     s_nameActive;
static uint16_t s_nameCur;                      // cursor position 0..TETRIS_NAME_LEN-1
static uint16_t s_nameIdx[TETRIS_NAME_LEN];     // index into s_nameCharset
static bool     s_nameDirtyFull;
static bool     s_nameDirtyBoxes;
static int16_t  s_nameStickX;
static int16_t  s_nameStickY;

//*****************************************************************************
// 3. BOARD RULES (no drawing in this section)
//*****************************************************************************

static int16_t NextFromBag(void)
{
    int16_t i, j, t;

    if(s_bagIdx >= 7)
    {
        for(i = 0; i < 7; i++) s_bag[i] = i;

        // Fisher-Yates shuffle
        for(i = 6; i > 0; i--)
        {
            j = (int16_t)(Rand16() % (uint16_t)(i + 1));
            t = s_bag[i];
            s_bag[i] = s_bag[j];
            s_bag[j] = t;
        }
        s_bagIdx = 0;
    }
    return s_bag[s_bagIdx++];
}

// Can piece (type, rot) sit with its 4x4 box at (px, py)?
// Cells ABOVE the board (row < 0) are allowed (pieces spawn partly above).
static bool Fits(int16_t type, int16_t rot, int16_t px, int16_t py)
{
    uint16_t mask = s_shapes[type][rot];
    int16_t r, c, br, bc;

    for(r = 0; r < 4; r++)
    {
        for(c = 0; c < 4; c++)
        {
            if(mask & (0x8000U >> (r * 4 + c)))
            {
                br = py + r;
                bc = px + c;

                if(bc < 0 || bc >= TETRIS_BOARD_W || br >= TETRIS_BOARD_H) return false;
                if(br >= 0 && s_board[br][bc]) return false;
            }
        }
    }
    return true;
}

// Stamp a piece into the board permanently (called when it lands).
static void PlaceOnBoard(int16_t type, int16_t rot, int16_t px, int16_t py)
{
    uint16_t mask = s_shapes[type][rot];
    int16_t r, c, br, bc;

    for(r = 0; r < 4; r++)
    {
        for(c = 0; c < 4; c++)
        {
            if(mask & (0x8000U >> (r * 4 + c)))
            {
                br = py + r;
                bc = px + c;
                if(br >= 0 && br < TETRIS_BOARD_H && bc >= 0 && bc < TETRIS_BOARD_W)
                    s_board[br][bc] = (uint16_t)(type + 1);
            }
        }
    }
}

// Remove every full row, sliding everything above it down. Returns 0..4.
static int16_t ClearFullRows(void)
{
    int16_t r, c, k, full, cleared = 0;

    r = TETRIS_BOARD_H - 1;
    while(r >= 0)
    {
        full = 1;
        for(c = 0; c < TETRIS_BOARD_W; c++)
        {
            if(!s_board[r][c]) { full = 0; break; }
        }

        if(full)
        {
            for(k = r; k > 0; k--)
                for(c = 0; c < TETRIS_BOARD_W; c++) s_board[k][c] = s_board[k - 1][c];
            for(c = 0; c < TETRIS_BOARD_W; c++) s_board[0][c] = 0;
            cleared++;
            // Do NOT move r: the row that slid into r might also be full.
        }
        else
        {
            r--;
        }
    }
    return cleared;
}

// Which rows are full RIGHT NOW? Returns a bitmask (bit r = row r) and the
// count. Does NOT change the board: the flash plays first, ClearFullRows()
// runs afterwards.  (1UL: the C28x int is only 16 bits, rows go up to 19.)
static uint32_t FindFullRows(int16_t *count)
{
    uint32_t mask = 0;
    int16_t  r, c, n = 0;

    for(r = 0; r < TETRIS_BOARD_H; r++)
    {
        for(c = 0; c < TETRIS_BOARD_W; c++)
            if(!s_board[r][c]) break;

        if(c == TETRIS_BOARD_W)
        {
            mask |= 1UL << r;
            n++;
        }
    }
    *count = n;
    return mask;
}

//*****************************************************************************
// 4. DRAWING (no game-state changes in this section, except the s_shown cache)
//*****************************************************************************

static void DrawCell(int16_t px, int16_t py, uint16_t colorIdx)
{
    if(colorIdx == 0)
    {
        Graphics_FillRect(px, py, px + TETRIS_CELL - 1, py + TETRIS_CELL - 1, COLOR_BLACK);

        Graphics_setForegroundColor(&g_sContext, COLOR_TETRIS_GRID);
        Graphics_drawLine(&g_sContext, px, py, px + TETRIS_CELL - 1, py);
        Graphics_drawLine(&g_sContext, px, py, px, py + TETRIS_CELL - 1);
    }
    else
    {
        // 1 px smaller than TETRIS_CELL: thin black gap between blocks.
        Graphics_FillRect(px, py, px + TETRIS_CELL - 2, py + TETRIS_CELL - 2, s_colors[colorIdx]);
    }
}

// Forget what is on the LCD; everything is redrawn on the next render().
static void InvalidateScreenCache(void)
{
    int16_t r, c;

    for(r = 0; r < TETRIS_BOARD_H; r++)
        for(c = 0; c < TETRIS_BOARD_W; c++) s_shown[r][c] = 0xFFFF;

    s_shownNext  = -1;
    s_shownScore = 0xFFFFFFFFUL;
    s_shownLines = 0xFFFF;
    s_shownLevel = 0xFFFF;
}

// Clear the screen and draw everything that never changes during a game.
// Called from init() only.
static void DrawTetrisLayout(void)
{
    Graphics_Rectangle border;

    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"TETRIS",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);

    // Border sits 2 px outside the playfield so it never overlaps a cell.
    border.xMin = TETRIS_BOARD_X - 2;
    border.yMin = TETRIS_BOARD_Y - 2;
    border.xMax = TETRIS_BOARD_X + TETRIS_BOARD_W * TETRIS_CELL + 1;
    border.yMax = TETRIS_BOARD_Y + TETRIS_BOARD_H * TETRIS_CELL + 1;
    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawRectangle(&g_sContext, &border);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawString(&g_sContext, (int16_t *)"SCORE", AUTO_STRING_LENGTH, 5, 42,  OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"LINES", AUTO_STRING_LENGTH, 5, 97,  OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"LEVEL", AUTO_STRING_LENGTH, 5, 152, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"NEXT",  AUTO_STRING_LENGTH, 225, 42, OPAQUE_TEXT);

    InvalidateScreenCache();            // the screen was just wiped
}

// Draw the "next piece" preview, but only when it changed.
static void DrawNextPreview(void)
{
    uint16_t mask;
    int16_t r, c;

    if(s_nextType == s_shownNext) return;
    s_shownNext = s_nextType;

    Graphics_FillRect(225, 72, 225 + 4 * TETRIS_CELL + 4, 72 + 4 * TETRIS_CELL + 4, COLOR_BLACK);

    mask = s_shapes[s_nextType][0];
    for(r = 0; r < 4; r++)
        for(c = 0; c < 4; c++)
            if(mask & (0x8000U >> (r * 4 + c)))
                DrawCell(230 + c * TETRIS_CELL, 77 + r * TETRIS_CELL, (uint16_t)(s_nextType + 1));
}

// Redraw score / lines / level, only the ones whose value changed.
// Trailing spaces overwrite leftover digits (OPAQUE_TEXT only covers glyphs).
static void DrawTetrisStats(void)
{
    char buf[20];

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);

    if(s_score != s_shownScore)
    {
        sprintf(buf, "%lu      ", (unsigned long)s_score);
        Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH, 5, 62, OPAQUE_TEXT);
        s_shownScore = s_score;
    }
    if(s_lines != s_shownLines)
    {
        sprintf(buf, "%u      ", s_lines);
        Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH, 5, 117, OPAQUE_TEXT);
        s_shownLines = s_lines;
    }
    if(s_level != s_shownLevel)
    {
        sprintf(buf, "%u      ", s_level);
        Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH, 5, 172, OPAQUE_TEXT);
        s_shownLevel = s_level;
    }
}

// Paint the playfield: compare what SHOULD be in each cell with what the LCD
// shows (s_shown) and repaint only the differences.
static void DrawTetrisBoard(void)
{
    uint16_t mask = s_shapes[s_cur.type][s_cur.rot];
    uint16_t colorIdx;
    bool     flashOn = false;
    int16_t  r, c, pr, pc;

    // Line-clear flash: white for TETRIS_CLEAR_FLASH_FRAMES, normal for the
    // next TETRIS_CLEAR_FLASH_FRAMES, and so on. Starts and ends on white.
    if(s_clearing)
        flashOn = ((((TETRIS_CLEAR_FRAMES - s_clearTimer) / TETRIS_CLEAR_FLASH_FRAMES) & 1) == 0);

    for(r = 0; r < TETRIS_BOARD_H; r++)
    {
        for(c = 0; c < TETRIS_BOARD_W; c++)
        {
            colorIdx = s_board[r][c];

            if(s_clearing)
            {
                // The locked piece is already in s_board: don't draw s_cur on top.
                if(flashOn && ((s_clearMask >> r) & 1UL))
                    colorIdx = CELL_FLASH;
            }
            else
            {
                pr = r - s_cur.y;
                pc = c - s_cur.x;
                if(pr >= 0 && pr < 4 && pc >= 0 && pc < 4)
                {
                    if(mask & (0x8000U >> (pr * 4 + pc)))
                        colorIdx = (uint16_t)(s_cur.type + 1);
                }
            }

            if(colorIdx != s_shown[r][c])
            {
                DrawCell(TETRIS_BOARD_X + c * TETRIS_CELL, TETRIS_BOARD_Y + r * TETRIS_CELL, colorIdx);
                s_shown[r][c] = colorIdx;

                // A cell under the banner was repainted: the banner needs redrawing.
                if(r >= BANNER_ROW0 && r <= BANNER_ROW1)
                    s_bannerDirty = true;
            }
        }
    }

    DrawNextPreview();
    DrawTetrisStats();
}

// Draw the banner box, but only when it is new or something painted over it.
static void DrawBanner(void)
{
    const char *msg;
    uint32_t    border, text;

    if(s_bannerKind == BANNER_NONE || !s_bannerDirty) return;
    s_bannerDirty = false;

    if(s_bannerKind == BANNER_TETRIS)
    {
        msg    = "TETRIS!";
        border = COLOR_TETRIS_I_CYAN;
        text   = COLOR_WHITE;
    }
    else
    {
        msg    = "NEW #1!";
        border = COLOR_GOLD;
        text   = COLOR_GOLD;
    }

    Graphics_FillRect(BANNER_X0,     BANNER_Y0,     BANNER_X1,     BANNER_Y1,     border);
    Graphics_FillRect(BANNER_X0 + 2, BANNER_Y0 + 2, BANNER_X1 - 2, BANNER_Y1 - 2, COLOR_BLACK);

    Graphics_setForegroundColor(&g_sContext, text);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)msg, AUTO_STRING_LENGTH,
                                (BANNER_X0 + BANNER_X1) / 2, (BANNER_Y0 + BANNER_Y1) / 2,
                                TRANSPARENT_TEXT);
}

// The banner expired (or the game ended): forget what the LCD shows in those
// rows so DrawTetrisBoard() paints the real cells back over the box.
static void EraseBanner(void)
{
    int16_t r, c;

    s_bannerErase = false;
    for(r = BANNER_ROW0; r <= BANNER_ROW1; r++)
        for(c = 0; c < TETRIS_BOARD_W; c++) s_shown[r][c] = 0xFFFF;
}

static void DrawGameOver(void)
{
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"GAME OVER", AUTO_STRING_LENGTH,
                                TETRIS_BOARD_X + (TETRIS_BOARD_W * TETRIS_CELL) / 2,
                                TETRIS_BOARD_Y + (TETRIS_BOARD_H * TETRIS_CELL) / 2 - 12, OPAQUE_TEXT);

    if(s_gameOverRank >= 0)
    {
        // Made the top 5: say so, and push "Press btn" down a line.
        Graphics_setForegroundColor(&g_sContext, COLOR_GOLD);
        Graphics_drawStringCentered(&g_sContext,
                                    (int16_t *)((s_gameOverRank == 0) ? "NEW #1!" : "TOP 5!"),
                                    AUTO_STRING_LENGTH,
                                    TETRIS_BOARD_X + (TETRIS_BOARD_W * TETRIS_CELL) / 2,
                                    TETRIS_BOARD_Y + (TETRIS_BOARD_H * TETRIS_CELL) / 2 + 12, OPAQUE_TEXT);
        Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
        Graphics_drawStringCentered(&g_sContext, (int16_t *)"Press btn", AUTO_STRING_LENGTH,
                                    TETRIS_BOARD_X + (TETRIS_BOARD_W * TETRIS_CELL) / 2,
                                    TETRIS_BOARD_Y + (TETRIS_BOARD_H * TETRIS_CELL) / 2 + 36, OPAQUE_TEXT);
    }
    else
    {
        Graphics_drawStringCentered(&g_sContext, (int16_t *)"Press btn", AUTO_STRING_LENGTH,
                                    TETRIS_BOARD_X + (TETRIS_BOARD_W * TETRIS_CELL) / 2,
                                    TETRIS_BOARD_Y + (TETRIS_BOARD_H * TETRIS_CELL) / 2 + 12, OPAQUE_TEXT);
    }
}

//*****************************************************************************
// 5. GAME FLOW
//*****************************************************************************

// Bring in the next piece at the top. Returns false if it does not fit.
static bool SpawnPiece(void)
{
    s_cur.type = s_nextType;
    s_cur.rot  = 0;
    s_cur.x    = 3;
    s_cur.y    = 0;
    s_nextType = NextFromBag();
    s_gravityCount = 0;

    return Fits(s_cur.type, s_cur.rot, s_cur.x, s_cur.y);
}

static void TetrisShift(int16_t dir)
{
    if(Fits(s_cur.type, s_cur.rot, s_cur.x + dir, s_cur.y)) s_cur.x += dir;
}

// Rotate clockwise with a simple wall kick (try left, then right).
static void TetrisRotate(void)
{
    int16_t nr = (s_cur.rot + 1) & 3;

    if(Fits(s_cur.type, nr, s_cur.x, s_cur.y))              s_cur.rot = nr;
    else if(Fits(s_cur.type, nr, s_cur.x - 1, s_cur.y))   { s_cur.rot = nr; s_cur.x--; }
    else if(Fits(s_cur.type, nr, s_cur.x + 1, s_cur.y))   { s_cur.rot = nr; s_cur.x++; }
}

// Start a banner over the playfield.
static void ShowBanner(uint16_t kind)
{
    s_bannerKind  = kind;
    s_bannerTimer = TETRIS_BANNER_FRAMES;
    s_bannerDirty = true;
    s_bannerErase = false;
}

// Lock the piece. If any rows are full, START the flash (the rows are removed
// later by FinishClear(), and the next piece spawns then). Otherwise spawn now.
// Returns false if the next piece cannot spawn (game over).
static bool LockAndSpawn(void)
{
    PlaceOnBoard(s_cur.type, s_cur.rot, s_cur.x, s_cur.y);

    s_clearMask = FindFullRows(&s_clearCount);
    if(s_clearMask)
    {
        s_clearing   = true;
        s_clearTimer = TETRIS_CLEAR_FRAMES;

        if(s_clearCount == 4)
        {
            Buzzer_PlaySfx(&sfx_tetris_say);        // four rows = "TETRIS!"
            ShowBanner(BANNER_TETRIS);
        }
        else
        {
            Buzzer_PlaySfx(&sfx_coin);              // 1-3 rows: chime
        }
        return true;                                // spawn happens in FinishClear()
    }

    Buzzer_PlaySfx(&sfx_thud);                      // optional: piece landed, no clear
    s_score += 4;                                   // bonus for every piece locked
    return SpawnPiece();
}

// The flash is over: remove the rows, score them, spawn the next piece.
// Returns false if the next piece cannot spawn (game over).
static bool FinishClear(void)
{
    int16_t cleared = ClearFullRows();

    s_score += (uint32_t)s_lineScore[cleared] * (s_level + 1);
    s_lines += cleared;
    s_level  = s_lines / 10;
    if(s_level > 9) s_level = 9;                    // plenty fast by level 9
    s_score += 4;                                   // bonus for every piece locked

    s_clearing  = false;
    s_clearMask = 0;
    return SpawnPiece();
}

//*****************************************************************************
// 5b. TITLE SCREEN + HIGH SCORES + NAME ENTRY
//     Title: D-pad U/D or stick up/down = move, R or stick tap = select,
//            L = leave Tetris.   Scores view: any button = back.
//
//     ORDER IN THIS SECTION (each function only calls ones above it):
//       HsEnsureLoaded, HsSubmit
//       TitleStickEdge, TitleText, TitleTextLeft, TitleHeader
//       TitleDrawRow, TitleDrawScores
//       TitleInit, TitleRedraw
//       NameEdge, NameBegin, NameFinish, NameUpdate, NameRender
//       TitleUpdate, TitleRender
//*****************************************************************************

#define TITLE_VIEW_MENU     0
#define TITLE_VIEW_SCORES   1
#define TITLE_NUM_ROWS      2

typedef enum
{
    TITLE_CONTINUE = 0,
    TITLE_START,                        // player chose Start
    TITLE_QUIT                          // player pressed L: leave Tetris
} TitleResult;

static const char *const s_titleRowName[TITLE_NUM_ROWS] = { "Start", "High Scores" };

// ---- High scores ----------------------------------------------------------

// Load the list (and names) from the save driver the first time they are needed.
static void HsEnsureLoaded(void)
{
    uint16_t i, j;

    if(s_hsLoaded) return;
    s_hsLoaded = true;

    if(!Save_Read(SAVE_SLOT_TETRIS_SCORES, &s_hs, sizeof s_hs))
        for(i = 0; i < TETRIS_NUM_SCORES; i++)
            s_hs.score[i] = 0;                  // nothing saved yet

    if(!Save_Read(SAVE_SLOT_TETRIS_NAMES, &s_hsn, sizeof s_hsn))
        for(i = 0; i < TETRIS_NUM_SCORES; i++)
            for(j = 0; j < TETRIS_NAME_LEN; j++)
                s_hsn.c[i][j] = ' ';            // old save: blank names

    // Sanitize: anything outside ' '..'Z' becomes a space.
    for(i = 0; i < TETRIS_NUM_SCORES; i++)
        for(j = 0; j < TETRIS_NAME_LEN; j++)
            if(s_hsn.c[i][j] < ' ' || s_hsn.c[i][j] > 'Z')
                s_hsn.c[i][j] = ' ';
}

// Insert `score` into the top-5 list with a BLANK name and save right away,
// so the score is safe even if the player quits during name entry.
// Returns the rank 0..4, or -1 if it did not place. Blocks briefly (flash).
static int16_t HsSubmit(uint32_t score)
{
    int16_t i, j;
    int16_t rank = -1;

    HsEnsureLoaded();

    if(score == 0) return -1;

    for(i = 0; i < TETRIS_NUM_SCORES; i++)
    {
        if(score > s_hs.score[i])
        {
            rank = i;
            break;
        }
    }
    if(rank < 0) return -1;

    for(i = TETRIS_NUM_SCORES - 1; i > rank; i--)
    {
        s_hs.score[i] = s_hs.score[i - 1];
        for(j = 0; j < TETRIS_NAME_LEN; j++)
            s_hsn.c[i][j] = s_hsn.c[i - 1][j];
    }
    s_hs.score[rank] = score;
    for(j = 0; j < TETRIS_NAME_LEN; j++)
        s_hsn.c[rank][j] = ' ';

    Save_Write(SAVE_SLOT_TETRIS_SCORES, &s_hs,  sizeof s_hs);
    Save_Write(SAVE_SLOT_TETRIS_NAMES,  &s_hsn, sizeof s_hsn);
    Save_Flush();
    return rank;
}

// ---- Title screen helpers -------------------------------------------------

// One event per stick push: +1 / -1 when the axis crosses TETRIS_STICK_ON,
// then 0 until it has gone back inside TETRIS_STICK_OFF.
static int16_t TitleStickEdge(int16_t v, int16_t *state)
{
    int16_t target = *state;

    if(v >= TETRIS_STICK_ON)                                target = 1;
    else if(v <= -TETRIS_STICK_ON)                          target = -1;
    else if(v < TETRIS_STICK_OFF && v > -TETRIS_STICK_OFF)  target = 0;

    if(target != *state)
    {
        *state = target;
        return target;                  // 0 on release = "no event"
    }
    return 0;
}

// Centered text.
static void TitleText(const char *s, int16_t x, int16_t y, uint32_t color)
{
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH,
                                x, y, TRANSPARENT_TEXT);
}

// Left-aligned text; y is the vertical CENTRE of the line (same as TitleText).
static void TitleTextLeft(const char *s, int16_t x, int16_t y, uint32_t color)
{
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_drawString(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH,
                        x, y - 10, TRANSPARENT_TEXT);
}

static void TitleHeader(const char *title)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    TitleText(title, SCREEN_W / 2, 15, COLOR_WHITE);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
}

static void TitleDrawRow(uint16_t i)
{
    int16_t top = TETRIS_TITLE_ROW_Y0 + (int16_t)i * (TETRIS_TITLE_ROW_H + TETRIS_TITLE_ROW_GAP);
    bool    sel = (i == s_titleSel);

    // Clear the whole band first: the selected rectangle is wider.
    Graphics_FillRect(TETRIS_TITLE_ROW_X0 - 12, top - 1,
                      TETRIS_TITLE_ROW_X1 + 12, top + TETRIS_TITLE_ROW_H, COLOR_BLACK);

    if(sel)
    {
        Graphics_FillRect(TETRIS_TITLE_ROW_X0 - 10, top,
                          TETRIS_TITLE_ROW_X1 + 10, top + TETRIS_TITLE_ROW_H - 1, COLOR_TETRIS_GRID);
        Graphics_FillRect(TETRIS_TITLE_ROW_X0 - 8,  top + 2,
                          TETRIS_TITLE_ROW_X1 + 8,  top + TETRIS_TITLE_ROW_H - 3, COLOR_TETRIS_T_PURPLE);
    }
    else
    {
        Graphics_FillRect(TETRIS_TITLE_ROW_X0, top,
                          TETRIS_TITLE_ROW_X1, top + TETRIS_TITLE_ROW_H - 1, COLOR_TETRIS_GHOST);
    }

    TitleText(s_titleRowName[i], SCREEN_W / 2, top + TETRIS_TITLE_ROW_H / 2,
              sel ? COLOR_WHITE : COLOR_TETRIS_GRID);
}   // end TitleDrawRow

// Rank, name and score on one line. The font is proportional, so the three
// pieces are drawn separately at fixed x positions instead of padded with spaces.
static void TitleDrawScores(void)
{
    char     buf[24];
    char     nm[TETRIS_NAME_LEN + 1];
    int16_t  i, j, y;
    uint32_t col;

    for(i = 0; i < TETRIS_NUM_SCORES; i++)
    {
        y   = TETRIS_SCORE_Y0 + i * TETRIS_SCORE_PITCH;
        col = (i == 0) ? COLOR_GOLD : COLOR_WHITE;

        sprintf(buf, "%u.", (unsigned)(i + 1));
        TitleTextLeft(buf, 40, y, col);

        if(s_hs.score[i] == 0)
        {
            TitleTextLeft("-", 75, y, col);
            continue;
        }

        for(j = 0; j < TETRIS_NAME_LEN; j++) nm[j] = (char)s_hsn.c[i][j];
        nm[TETRIS_NAME_LEN] = 0;
        TitleTextLeft(nm, 75, y, col);

        sprintf(buf, "%lu", (unsigned long)s_hs.score[i]);
        TitleTextLeft(buf, 195, y, col);
    }
}

// ---- Title screen: init / redraw ------------------------------------------

static void TitleInit(void)
{
    HsEnsureLoaded();
    s_titleView      = TITLE_VIEW_MENU;
    s_titleSel       = 0;
    s_titleStickY    = 0;
    s_titleDirtyFull = true;
    s_titleDirtyRows = 0;
}

static void TitleRedraw(void)
{
    s_titleDirtyFull = true;
}

// ---- Name entry -----------------------------------------------------------
// U/D (D-pad or stick) = change letter, L/R = move cursor,
// button tap = next position; on the last letter it confirms.

#define NAME_BOX_W   36
#define NAME_BOX_GAP 8
#define NAME_BOX_X0  ((SCREEN_W - (TETRIS_NAME_LEN * NAME_BOX_W + (TETRIS_NAME_LEN - 1) * NAME_BOX_GAP)) / 2)
#define NAME_BOX_Y   100
#define NAME_BOX_H   44

// One event per stick push: returns v only on the frame it changes to nonzero.
static int16_t NameEdge(int16_t v, int16_t *prev)
{
    int16_t e = (v != 0 && v != *prev) ? v : 0;
    *prev = v;
    return e;
}

static void NameBegin(void)
{
    uint16_t i;

    for(i = 0; i < TETRIS_NAME_LEN; i++) s_nameIdx[i] = 0;     // 'A'
    s_nameCur        = 0;
    s_nameStickX     = g_joy.dirX;     // a stick still held doesn't fire
    s_nameStickY     = g_joy.dirY;
    s_nameActive     = true;
    s_nameDirtyFull  = true;
    s_nameDirtyBoxes = true;
}

// Store the typed name at this game's rank, save, and go to the title.
static void NameFinish(void)
{
    uint16_t j;

    for(j = 0; j < TETRIS_NAME_LEN; j++)
        s_hsn.c[s_gameOverRank][j] = (uint16_t)s_nameCharset[s_nameIdx[j]];

    Save_Write(SAVE_SLOT_TETRIS_NAMES, &s_hsn, sizeof s_hsn);
    Save_Flush();                       // blocks briefly (flash)
    Buzzer_PlaySfx(&sfx_menu_select);

    s_nameActive = false;
    s_inTitle    = true;
    TitleInit();
}

// Logic only.
static void NameUpdate(void)
{
    int16_t sx = NameEdge(g_joy.dirX, &s_nameStickX);
    int16_t sy = NameEdge(g_joy.dirY, &s_nameStickY);   // +1 = down
    bool up    = g_dpad.uPressed || (sy < 0);
    bool down  = g_dpad.dPressed || (sy > 0);
    bool left  = g_dpad.lPressed || (sx < 0);
    bool right = g_dpad.rPressed || (sx > 0);
    bool tap   = g_joy.btnPressed;

    if(up)
    {
        s_nameIdx[s_nameCur] = (uint16_t)((s_nameIdx[s_nameCur] + 1) % NAME_NCHARS);
        s_nameDirtyBoxes = true;
        Buzzer_PlaySfx(&sfx_menu_move);
    }
    if(down)
    {
        s_nameIdx[s_nameCur] = (uint16_t)((s_nameIdx[s_nameCur] + NAME_NCHARS - 1) % NAME_NCHARS);
        s_nameDirtyBoxes = true;
        Buzzer_PlaySfx(&sfx_menu_move);
    }
    if(left && s_nameCur > 0)
    {
        s_nameCur--;
        s_nameDirtyBoxes = true;
    }
    if(right && s_nameCur < TETRIS_NAME_LEN - 1)
    {
        s_nameCur++;
        s_nameDirtyBoxes = true;
    }
    if(tap)
    {
        if(s_nameCur < TETRIS_NAME_LEN - 1)
        {
            s_nameCur++;
            s_nameDirtyBoxes = true;
        }
        else
        {
            NameFinish();
        }
    }
}

// Drawing only.
static void NameRender(void)
{
    char     buf[24];
    char     ch[2];
    uint16_t i;
    int16_t  x;
    bool     sel;

    if(s_nameDirtyFull)
    {
        s_nameDirtyFull = false;
        TitleHeader((s_gameOverRank == 0) ? "NEW #1 HIGH SCORE!" : "NEW HIGH SCORE!");
        sprintf(buf, "Score: %lu", (unsigned long)s_score);
        TitleText(buf, SCREEN_W / 2, 60, COLOR_GOLD);
        TitleText("Enter your name", SCREEN_W / 2, 82, COLOR_WHITE);
        TitleText("U/D:letter  L/R:move  Btn:next/OK", SCREEN_W / 2, 228, COLOR_WHITE);
    }

    if(s_nameDirtyBoxes)
    {
        s_nameDirtyBoxes = false;
        ch[1] = 0;

        for(i = 0; i < TETRIS_NAME_LEN; i++)
        {
            x   = NAME_BOX_X0 + (int16_t)i * (NAME_BOX_W + NAME_BOX_GAP);
            sel = (i == s_nameCur);

            Graphics_FillRect(x, NAME_BOX_Y, x + NAME_BOX_W - 1, NAME_BOX_Y + NAME_BOX_H - 1,
                              sel ? COLOR_TETRIS_T_PURPLE : COLOR_TETRIS_GHOST);

            ch[0] = s_nameCharset[s_nameIdx[i]];
            TitleText(ch, x + NAME_BOX_W / 2, NAME_BOX_Y + NAME_BOX_H / 2, COLOR_WHITE);
        }
    }
}

// ---- Title screen: update / render ----------------------------------------

// Logic only. Tells Tetris_Update what the player chose.
static TitleResult TitleUpdate(void)
{
    int16_t  sy    = TitleStickEdge(g_joy.dy, &s_titleStickY);    // dy is +DOWN
    bool     up    = g_dpad.uPressed || (sy < 0);
    bool     down  = g_dpad.dPressed || (sy > 0);
    bool     left  = g_dpad.lPressed;
    bool     right = g_dpad.rPressed;
    bool     tap   = g_joy.btnPressed;
    uint16_t old;

    if(s_titleView == TITLE_VIEW_SCORES)
    {
        if(up || down || left || right || tap)
        {
            s_titleView      = TITLE_VIEW_MENU;
            s_titleDirtyFull = true;
        }
        return TITLE_CONTINUE;
    }

    if(left)
        return TITLE_QUIT;

    if(up || down)
    {
        old        = s_titleSel;
        s_titleSel = (s_titleSel + 1) % TITLE_NUM_ROWS;           // only two rows
        s_titleDirtyRows |= (1 << old) | (1 << s_titleSel);
        Buzzer_PlaySfx(&sfx_menu_move);
    }

    if(right || tap)
    {
        Buzzer_PlaySfx(&sfx_menu_select);
        if(s_titleSel == 0)
            return TITLE_START;

        s_titleView      = TITLE_VIEW_SCORES;
        s_titleDirtyFull = true;
    }

    return TITLE_CONTINUE;
}

// Drawing only.
static void TitleRender(void)
{
    char     buf[24];
    uint16_t i;

    if(s_titleDirtyFull)
    {
        s_titleDirtyFull = false;

        if(s_titleView == TITLE_VIEW_MENU)
        {
            TitleHeader("TETRIS");
            sprintf(buf, "Best: %lu", (unsigned long)s_hs.score[0]);
            TitleText(buf, SCREEN_W / 2, 55, COLOR_GOLD);
            TitleText("L:exit    U/D:pick    R:select", SCREEN_W / 2, 228, COLOR_WHITE);
            s_titleDirtyRows = (1 << TITLE_NUM_ROWS) - 1;
        }
        else
        {
            TitleHeader("High Scores");
            TitleDrawScores();
            TitleText("any button: back", SCREEN_W / 2, 228, COLOR_TETRIS_GHOST);
        }
    }

    if(s_titleView == TITLE_VIEW_MENU)
    {
        for(i = 0; i < TITLE_NUM_ROWS; i++)
            if(s_titleDirtyRows & (1 << i)) TitleDrawRow(i);
        s_titleDirtyRows = 0;
    }
}

//*****************************************************************************
// 6. APP CALLBACKS
//*****************************************************************************

// Begin a new game: reset state AND draw the playfield from scratch.
// (Called when the player picks Start on the title screen.)
static void StartGame(void)
{
    int16_t r, c;

    // Seed from the free-running timer so each game has a different order.
    Rand_Seed(TimerNow());

    for(r = 0; r < TETRIS_BOARD_H; r++)
        for(c = 0; c < TETRIS_BOARD_W; c++) s_board[r][c] = 0;

    s_score = 0;
    s_lines = 0;
    s_level = 0;
    s_bagIdx = 7;                       // 7 = "deck empty", forces a shuffle
    s_nextType = NextFromBag();

    s_frameNo       = 0;
    s_hTimer        = 0;
    s_prevMoveDir   = 0;
    s_gameOver      = false;
    s_gameOverTimer = 0;
    s_gameOverDrawn = false;
    s_gameOverRank  = -1;

    s_clearing      = false;
    s_clearMask     = 0;
    s_clearTimer    = 0;
    s_bannerKind    = BANNER_NONE;
    s_bannerTimer   = 0;
    s_bannerDirty   = false;
    s_bannerErase   = false;
    s_recordShown   = false;

    DrawTetrisLayout();                 // also invalidates the screen cache
    SpawnPiece();                       // an empty board always has room
    Buzzer_PlaySong(&song_tetris, 1);   // background music (stopped by the app loop on exit)
    // The first render() paints the board, preview and stats because the
    // cache was just invalidated.
}

// init: runs once at launch. Show the title screen; the game starts when the
// player picks Start.
static void Tetris_Init(void)
{
    s_gameOverRank = -1;                // no gold highlight until a score is earned
    s_hsLoaded     = false;             // never trust power-up RAM: reload the list from the save driver
    s_nameActive   = false;
    s_inTitle = true;
    TitleInit();
}

// The stack reached the top. render() draws the overlay.
static void TetrisGameOver(void)
{
    s_gameOver      = true;
    s_gameOverTimer = TETRIS_GAME_OVER_HOLD_FRAMES;
    s_clearing      = false;

    if(s_bannerKind != BANNER_NONE)             // GAME OVER text replaces the banner
    {
        s_bannerKind  = BANNER_NONE;
        s_bannerTimer = 0;
        s_bannerErase = true;
    }

    Buzzer_Stop();                              // silence on game over

    // Save the score (blank name) if it made the top 5. Blocks briefly
    // (flash). The name is typed afterwards.
    s_gameOverRank = HsSubmit(s_score);
}

// update: once per frame. LOGIC ONLY. No drawing here.
static AppStatus Tetris_Update(void)
{
    uint16_t interval;
    bool     forceStep = false;
    int16_t  moveDir;                   // -1 left, 0 none, +1 right
    int16_t  moveEdge;                  // moveDir, but only on the frame it starts
    bool     softDrop;
    bool     rotate;

    // ---- Title screen: Start / High Scores ---------------------------------
    if(s_inTitle)
    {
        TitleResult r = TitleUpdate();

        if(r == TITLE_QUIT)  return APP_QUIT;       // back to the main menu
        if(r == TITLE_START) { s_inTitle = false; StartGame(); }
        return APP_CONTINUE;
    }

    // ---- Game over: wait out the hold time, then any button ----------------
    //      top 5 -> name entry -> title;  otherwise -> title
    if(s_gameOver)
    {
        if(s_nameActive)
        {
            NameUpdate();
        }
        else if(s_gameOverTimer > 0)
        {
            s_gameOverTimer--;
        }
        else if(g_joy.btnPressed || Dpad_AnyPressed())
        {
            if(s_gameOverRank >= 0)
            {
                NameBegin();            // top 5: ask for a name first
            }
            else
            {
                s_inTitle = true;
                TitleInit();
            }
        }

        return APP_CONTINUE;
    }

    // ---- Banner countdown (keeps running during the flash) -----------------
    if(s_bannerTimer > 0 && --s_bannerTimer == 0)
    {
        s_bannerKind  = BANNER_NONE;
        s_bannerErase = true;
    }

    // ---- Line-clear flash: the board is frozen until it ends ---------------
    if(s_clearing)
    {
        if(--s_clearTimer == 0 && !FinishClear())
            TetrisGameOver();
        return APP_CONTINUE;
    }

    s_frameNo++;

    // ---- 1. INPUT ----------------------------------------------------------
    // Merge stick and D-pad. If both D-pad sides are held they cancel out
    // and the stick decides.
    moveDir = g_joy.dirX;
    if(TETRIS_KEY_LEFT_HELD  && !TETRIS_KEY_RIGHT_HELD) moveDir = -1;
    if(TETRIS_KEY_RIGHT_HELD && !TETRIS_KEY_LEFT_HELD)  moveDir =  1;

    moveEdge      = (moveDir != 0 && moveDir != s_prevMoveDir) ? moveDir : 0;
    s_prevMoveDir = moveDir;

    softDrop = (g_joy.dirY > 0) || TETRIS_KEY_DOWN_HELD;
    rotate   = g_joy.btnPressed || TETRIS_KEY_ROTATE_TAP;

    // Left/right: move once immediately, then wait TETRIS_DAS_FRAMES before
    // repeating every TETRIS_ARR_FRAMES.
    if(moveDir == 0)
    {
        s_hTimer = 0;
    }
    else if(moveEdge != 0)
    {
        TetrisShift(moveDir);
        s_hTimer = TETRIS_DAS_FRAMES;
    }
    else if(--s_hTimer <= 0)
    {
        TetrisShift(moveDir);
        s_hTimer = TETRIS_ARR_FRAMES;
    }

    if(rotate) TetrisRotate();                      // one rotation per tap

    if(softDrop && (s_frameNo & 1)) forceStep = true;   // every other frame

    // ---- 2. GRAVITY --------------------------------------------------------
    interval = s_gravityFrames[s_level];
    s_gravityCount++;

    if(forceStep || s_gravityCount >= interval)
    {
        s_gravityCount = 0;

        if(Fits(s_cur.type, s_cur.rot, s_cur.x, s_cur.y + 1))
        {
            s_cur.y++;                              // room below: fall one row
            if(softDrop) s_score += 1;              // soft-drop bonus
        }
        else if(!LockAndSpawn())
        {
            TetrisGameOver();
        }
    }

    // ---- 3. NEW #1: once per game, when the saved record is passed ---------
    // (Waits for a free banner, so it never cuts off a TETRIS! banner.)
    if(!s_recordShown && s_hs.score[0] > 0 && s_score > s_hs.score[0] && s_bannerTimer == 0)
    {
        s_recordShown = true;
        ShowBanner(BANNER_RECORD);
        Buzzer_PlaySfx(&sfx_coin);              // swap for a fancier sfx from songs.h
    }

    return APP_CONTINUE;
}

// render: once per frame. DRAWING ONLY.
static void Tetris_Render(void)
{
    if(s_inTitle)
    {
        TitleRender();
        return;
    }

    if(s_nameActive)
    {
        NameRender();
        return;
    }

    if(s_gameOverDrawn)
        return;                                     // nothing changes any more

    if(s_bannerErase) EraseBanner();                // must come BEFORE the board repaint

    DrawTetrisBoard();                              // diff-based, cheap when idle
    DrawBanner();                                   // only draws when new / painted over

    if(s_gameOver)
    {
        DrawGameOver();
        s_gameOverDrawn = true;
    }
}


// redraw: called after the Settings overlay. Repaint only; keep game state.
static void Tetris_Redraw(void)
{
    if(s_inTitle)
    {
        TitleRedraw();           // title screen: just repaint it
        return;
    }

    if(s_nameActive)
    {
        s_nameDirtyFull  = true;        // name screen: just repaint it
        s_nameDirtyBoxes = true;
        return;
    }

    DrawTetrisLayout();                 // wipes LCD + invalidates s_shown / stats cache
    s_gameOverDrawn = false;            // if the game was over, render() redraws the overlay
    s_bannerDirty = (s_bannerKind != BANNER_NONE);  // the wipe removed the banner box
    s_bannerErase = false;
    if(!s_gameOver)
        Buzzer_PlaySong(&song_tetris, 1);   // the runner stopped the music; restart it
}


//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//*****************************************************************************
const App g_tetrisApp =
{
    "TETRIS",                           // name (first letter = menu tile icon)
    "Stick: move/drop  Btn: rotate",    // hint shown under the menu
    COLOR_TETRIS_T_PURPLE,                // menu tile color
    TETRIS_FRAME_US,                    // frame period
        Tetris_Init, Tetris_Update, Tetris_Render,
    NULL,                               // no exit() cleanup needed
    NULL,                               // not a legacy app
    Tetris_Redraw                       // repaint after Settings overlay
};
