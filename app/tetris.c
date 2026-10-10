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
//   After GAME OVER  : press any button to return to the menu
//
// HOW THIS FILE IS ORGANIZED
//   1. Constants         board size, timing, piece shapes, colors, scoring
//   2. Game state        every variable that changes while playing
//   3. Board rules       Fits / place / clear rows - NO drawing in here
//   4. Drawing           LCD output; only repaints what changed
//   5. Game flow         spawn, reset, shift, rotate, lock
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
//   * One public symbol: g_tetrisApp.
//#############################################################################

#include "apps.h"
#include <stdio.h>                      // sprintf, for the score text

//*****************************************************************************
// 1. CONSTANTS
//*****************************************************************************

// ---- Board geometry -------------------------------------------------------
#define BOARD_W            10
#define BOARD_H            20
#define CELL               10           // pixels per cell
#define BOARD_X            110          // left edge of the playfield (pixels)
#define BOARD_Y            36           // top edge (below the title bar)

// ---- Timing ---------------------------------------------------------------
#define TETRIS_FRAME_US    16000UL      // pause after each frame (~60 fps)
#define DAS_FRAMES         9            // frames you must hold left/right before repeat
#define ARR_FRAMES         3            // frames between repeats
#define GAME_OVER_HOLD_FRAMES 90        // ignore input this long after game over
                                        //   (~1.5 s at 16 ms/frame)

// ---- D-pad mapping --------------------------------------------------------
#define KEY_LEFT_HELD      (g_dpad.a)
#define KEY_RIGHT_HELD     (g_dpad.b)
#define KEY_DOWN_HELD      (g_dpad.c)
#define KEY_ROTATE_TAP     (g_dpad.dPressed)

// ---- Piece shapes ---------------------------------------------------------
// Each piece is stored as a 4x4 grid packed into one 16-bit number.
//   * Bit 15 (0x8000) is the top-left cell, bit 0 is the bottom-right.
//   * Cells are read row by row, 4 bits per row, most significant bit first.
//   * A 1 bit means "this cell is part of the piece".
//
// Example: 0x0E40 in binary is 0000 1110 0100 0000, which reads as
//     row 0:  ....
//     row 1:  ###.        <- a T piece pointing down
//     row 2:  .#..
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
static const uint32_t s_colors[8] =
{
    COLOR_BLACK,
    COLOR_TETRIS_I_CYAN,
    COLOR_TETRIS_J_BLUE,
    COLOR_TETRIS_L_ORANGE,
    COLOR_TETRIS_O_YELLOW,
    COLOR_TETRIS_S_GREEN,
    COLOR_TETRIS_T_PURPLE,
    COLOR_TETRIS_Z_RED
};

// ---- Scoring and speed ----------------------------------------------------
// Points for clearing 0..4 rows at once, multiplied by (level + 1).
static const uint16_t s_lineScore[5] = { 0, 40, 100, 300, 1200 };

// Gravity per level: the piece falls one row every N frames. Lower = faster.
static const uint16_t s_gravityFrames[20] =
    { 30, 26, 22, 18, 15, 12, 10, 8, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1 };

//*****************************************************************************
// 2. GAME STATE
//*****************************************************************************

typedef struct
{
    int16_t type;       // 0..6, index into s_shapes (I, J, L, O, S, T, Z)
    int16_t rot;        // 0..3
    int16_t x;          // column of the 4x4 box's left edge
    int16_t y;          // row of the 4x4 box's top edge
} Piece;

// Only LOCKED blocks live here. 0 = empty, otherwise a color index.
static uint16_t s_board[BOARD_H][BOARD_W];

// What the LCD is currently showing per cell. 0xFFFF = unknown, redraw.
static uint16_t s_shown[BOARD_H][BOARD_W];

static Piece    s_cur;                  // the falling piece
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

                if(bc < 0 || bc >= BOARD_W || br >= BOARD_H) return false;
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
                if(br >= 0 && br < BOARD_H && bc >= 0 && bc < BOARD_W)
                    s_board[br][bc] = (uint16_t)(type + 1);
            }
        }
    }
}

// Remove every full row, sliding everything above it down. Returns 0..4.
static int16_t ClearFullRows(void)
{
    int16_t r, c, k, full, cleared = 0;

    r = BOARD_H - 1;
    while(r >= 0)
    {
        full = 1;
        for(c = 0; c < BOARD_W; c++)
        {
            if(!s_board[r][c]) { full = 0; break; }
        }

        if(full)
        {
            for(k = r; k > 0; k--)
                for(c = 0; c < BOARD_W; c++) s_board[k][c] = s_board[k - 1][c];
            for(c = 0; c < BOARD_W; c++) s_board[0][c] = 0;
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

//*****************************************************************************
// 4. DRAWING (no game-state changes in this section, except the s_shown cache)
//*****************************************************************************

static void DrawCell(int16_t px, int16_t py, uint16_t colorIdx)
{
    if(colorIdx == 0)
    {
        Graphics_FillRect(px, py, px + CELL - 1, py + CELL - 1, COLOR_BLACK);

        Graphics_setForegroundColor(&g_sContext, COLOR_TETRIS_GRID);
        Graphics_drawLine(&g_sContext, px, py, px + CELL - 1, py);
        Graphics_drawLine(&g_sContext, px, py, px, py + CELL - 1);
    }
    else
    {
        // 1 px smaller than CELL: thin black gap between blocks.
        Graphics_FillRect(px, py, px + CELL - 2, py + CELL - 2, s_colors[colorIdx]);
    }
}

// Forget what is on the LCD; everything is redrawn on the next render().
static void InvalidateScreenCache(void)
{
    int16_t r, c;

    for(r = 0; r < BOARD_H; r++)
        for(c = 0; c < BOARD_W; c++) s_shown[r][c] = 0xFFFF;

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
    border.xMin = BOARD_X - 2;
    border.yMin = BOARD_Y - 2;
    border.xMax = BOARD_X + BOARD_W * CELL + 1;
    border.yMax = BOARD_Y + BOARD_H * CELL + 1;
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

    Graphics_FillRect(225, 72, 225 + 4 * CELL + 4, 72 + 4 * CELL + 4, COLOR_BLACK);

    mask = s_shapes[s_nextType][0];
    for(r = 0; r < 4; r++)
        for(c = 0; c < 4; c++)
            if(mask & (0x8000U >> (r * 4 + c)))
                DrawCell(230 + c * CELL, 77 + r * CELL, (uint16_t)(s_nextType + 1));
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
    int16_t r, c, pr, pc;

    for(r = 0; r < BOARD_H; r++)
    {
        for(c = 0; c < BOARD_W; c++)
        {
            colorIdx = s_board[r][c];

            pr = r - s_cur.y;
            pc = c - s_cur.x;
            if(pr >= 0 && pr < 4 && pc >= 0 && pc < 4)
            {
                if(mask & (0x8000U >> (pr * 4 + pc)))
                    colorIdx = (uint16_t)(s_cur.type + 1);
            }

            if(colorIdx != s_shown[r][c])
            {
                DrawCell(BOARD_X + c * CELL, BOARD_Y + r * CELL, colorIdx);
                s_shown[r][c] = colorIdx;
            }
        }
    }

    DrawNextPreview();
    DrawTetrisStats();
}

static void DrawGameOver(void)
{
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"GAME OVER", AUTO_STRING_LENGTH,
                                BOARD_X + (BOARD_W * CELL) / 2,
                                BOARD_Y + (BOARD_H * CELL) / 2 - 12, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Press btn", AUTO_STRING_LENGTH,
                                BOARD_X + (BOARD_W * CELL) / 2,
                                BOARD_Y + (BOARD_H * CELL) / 2 + 12, OPAQUE_TEXT);
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

// Lock the piece, clear rows, update score/level, spawn the next piece.
// Returns false if the next piece cannot spawn (game over).
static bool LockAndSpawn(void)
{
    int16_t cleared;

    PlaceOnBoard(s_cur.type, s_cur.rot, s_cur.x, s_cur.y);

    cleared = ClearFullRows();
    if(cleared > 0)
    {
        s_score += (uint32_t)s_lineScore[cleared] * (s_level + 1);
        s_lines += cleared;
        s_level  = s_lines / 10;
        if(s_level > 9) s_level = 9;            // plenty fast by level 9
    }
    s_score += 4;                               // bonus for every piece locked

    return SpawnPiece();
}

//*****************************************************************************
// 6. APP CALLBACKS
//*****************************************************************************

// init: runs once at launch. Reset state AND draw the screen from scratch.
static void Tetris_Init(void)
{
    int16_t r, c;

    // Seed from the free-running timer so each game has a different order.
    Rand_Seed(TimerNow());

    for(r = 0; r < BOARD_H; r++)
        for(c = 0; c < BOARD_W; c++) s_board[r][c] = 0;

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

    DrawTetrisLayout();                 // also invalidates the screen cache
    SpawnPiece();                       // an empty board always has room
    // The first render() paints the board, preview and stats because the
    // cache was just invalidated.
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

    // ---- Game over: wait out the hold time, then any button quits ----------
    if(s_gameOver)
    {
        if(s_gameOverTimer > 0)
            s_gameOverTimer--;
        else if(g_joy.btnPressed || Dpad_AnyPressed())
            return APP_QUIT;            // back to the menu

        return APP_CONTINUE;
    }

    s_frameNo++;

    // ---- 1. INPUT ----------------------------------------------------------
    // Merge stick and D-pad. If both D-pad sides are held they cancel out
    // and the stick decides.
    moveDir = g_joy.dirX;
    if(KEY_LEFT_HELD  && !KEY_RIGHT_HELD) moveDir = -1;
    if(KEY_RIGHT_HELD && !KEY_LEFT_HELD)  moveDir =  1;

    moveEdge      = (moveDir != 0 && moveDir != s_prevMoveDir) ? moveDir : 0;
    s_prevMoveDir = moveDir;

    softDrop = (g_joy.dirY > 0) || KEY_DOWN_HELD;
    rotate   = g_joy.btnPressed || KEY_ROTATE_TAP;

    // Left/right: move once immediately, then wait DAS_FRAMES before
    // repeating every ARR_FRAMES.
    if(moveDir == 0)
    {
        s_hTimer = 0;
    }
    else if(moveEdge != 0)
    {
        TetrisShift(moveDir);
        s_hTimer = DAS_FRAMES;
    }
    else if(--s_hTimer <= 0)
    {
        TetrisShift(moveDir);
        s_hTimer = ARR_FRAMES;
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
            // Stack reached the top. render() will draw the overlay.
            s_gameOver      = true;
            s_gameOverTimer = GAME_OVER_HOLD_FRAMES;
        }
    }

    return APP_CONTINUE;
}

// render: once per frame. DRAWING ONLY.
static void Tetris_Render(void)
{
    if(s_gameOverDrawn)
        return;                                     // nothing changes any more

    DrawTetrisBoard();                              // diff-based, cheap when idle

    if(s_gameOver)
    {
        DrawGameOver();
        s_gameOverDrawn = true;
    }
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
    NULL                                // not a legacy app
};
