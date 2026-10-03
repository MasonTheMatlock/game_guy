//#############################################################################
// FILE:   app/tetris.c
// TITLE:  Tetris
//
// CONTROLS
//   stick left/right : move piece (hold to auto-repeat)
//   stick down       : soft drop (fall faster, +1 point per row)
//   stick up         : hard drop (instantly to the bottom)
//   button tap       : rotate
//   hold button      : quit to the menu
//
// HOW THIS FILE IS ORGANIZED
//   1. Constants         board size, timing, piece shapes, colors, scoring
//   2. Game state        every variable that changes while playing
//   3. Board rules       Fits / place / clear rows - NO drawing in here
//   4. Drawing           LCD output; only repaints what changed
//   5. Game flow         spawn, reset, shift, rotate, lock, game over
//   6. Tetris_Run()      the one public function: the game loop
//
// DESIGN NOTES (the ideas worth understanding)
//   * Rules and drawing are separate. Section 3 never touches the LCD and
//     section 4 never changes the game state. That makes each easier to
//     debug: if a piece moves wrong it is a rules bug, if it LOOKS wrong it
//     is a drawing bug.
//   * Pieces are 16-bit bitmasks, not arrays of coordinates (see g_shapes).
//   * Drawing is "diff based": g_shown remembers what the LCD currently
//     shows, and only cells that differ are repainted. The LCD is on a slow
//     SPI link, so repainting the whole board every frame would crawl.
//   * Pieces come from a "7-bag" shuffle so you never wait too long for an I.
//   * Timing is frame based: gravity is "move down every N frames", and the
//     loop runs at a roughly fixed rate (see FRAME_US).
//#############################################################################

#include "apps.h"
#include <stdio.h>                      // sprintf, for the score text

//*****************************************************************************
// 1. CONSTANTS
//*****************************************************************************

// ---- Board geometry -------------------------------------------------------
// The playfield is BOARD_W x BOARD_H cells, each CELL x CELL pixels:
//   10 x 10 cells * 10 px = 100 x 200 px, drawn at (BOARD_X, BOARD_Y).
// That leaves ~100 px free on each side for the score panel and next piece.
#define BOARD_W            10
#define BOARD_H            20
#define CELL               10           // pixels per cell
#define BOARD_X            110          // left edge of the playfield (pixels)
#define BOARD_Y            36           // top edge (below the title bar)

// ---- Timing ---------------------------------------------------------------
#define FRAME_US           16000UL      // pause per loop pass (~60 frames/sec
                                        //   before drawing time is added)
#define DAS_FRAMES         9            // "Delayed Auto Shift": frames you must hold
                                        //   left/right before it starts repeating
#define ARR_FRAMES         3            // "Auto Repeat Rate": frames between repeats
#define GAME_OVER_HOLD_MS  1500         // ignore input this long after game over

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
//     row 3:  ....
//
// To test cell (row, col):   mask & (0x8000 >> (row * 4 + col))
//
// Each piece has 4 rotations (the O piece repeats itself 4 times so that every
// piece can be indexed the same way). Order of pieces: I, J, L, O, S, T, Z.
static const uint16_t g_shapes[7][4] =
{
    { 0x0F00, 0x2222, 0x00F0, 0x4444 },   // I
    { 0x44C0, 0x8E00, 0x6440, 0x0E20 },   // J
    { 0x4460, 0x0E80, 0xC440, 0x2E00 },   // L
    { 0xCC00, 0xCC00, 0xCC00, 0xCC00 },   // O
    { 0x06C0, 0x8C40, 0x6C00, 0x4620 },   // S
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 },   // T
    { 0x0C60, 0x4C80, 0xC600, 0x2640 }    // Z
};

// Cell colors, indexed by "color index". Index 0 means an EMPTY cell (black).
// Index 1..7 is piece type + 1, so a locked I piece is stored as 1, J as 2...
static const uint32_t g_colors[8] =
{
    COLOR_BLACK,
    COLOR_TETRIS_I_CYAN,   // I  cyan
    COLOR_TETRIS_J_BLUE,   // J  blue
    COLOR_TETRIS_L_ORANGE,   // L  orange
    COLOR_TETRIS_O_YELLOW,   // O  yellow
    COLOR_TETRIS_S_GREEN,   // S  green
    COLOR_TETRIS_T_PURPLE,   // T  purple
    COLOR_TETRIS_Z_RED    // Z  red
};

// ---- Scoring and speed ----------------------------------------------------
// Points for clearing 0..4 rows at once, multiplied by (level + 1).
// standard tetris scoring.
static const uint16_t g_lineScore[5] = { 0, 40, 100, 300, 1200 };

// Gravity per level: the piece falls one row every N frames. Lower = faster.
// You advance one level every 10 lines, capped at level 20.
static const uint16_t g_gravityFrames[20] = { 30, 26, 22, 18, 15, 12, 10, 8, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 3, 1,};

//*****************************************************************************
// 2. GAME STATE
//*****************************************************************************

// The falling piece. (x, y) is where its 4x4 box sits on the board; the box
// can hang off the edge (x < 0) because the piece's own cells may still be
// inside the board.
typedef struct
{
    int16_t type;       // 0..6, index into g_shapes (I, J, L, O, S, T, Z)
    int16_t rot;        // 0..3, index into g_shapes[type]
    int16_t x;          // column of the 4x4 box's left edge
    int16_t y;          // row of the 4x4 box's top edge
} Piece;

// What is on the board. Only LOCKED blocks live here; the falling piece is
// kept separately in g_cur and merged in only when it lands or is drawn.
// 0 = empty, otherwise a color index (see g_colors).
static uint16_t g_board[BOARD_H][BOARD_W];

// What the LCD is currently showing for each board cell (same encoding).
// 0xFFFF means "unknown, must be redrawn". See RenderTetrisBoard().
static uint16_t g_shown[BOARD_H][BOARD_W];

static Piece    g_cur;                  // the falling piece
static int16_t  g_nextType;             // the piece that spawns next
static int16_t  g_shownNext;            // next-piece preview currently on the LCD

static uint32_t g_score;
static uint16_t g_lines;                // total lines cleared
static uint16_t g_level;                // 0..9

// Values currently shown in the side panel, so text is only redrawn on change.
static uint32_t g_shownScore;
static uint16_t g_shownLines;
static uint16_t g_shownLevel;

static uint16_t g_gravityCount;         // frames since the piece last fell

// 7-bag randomizer state: a shuffled deck of the 7 piece types.
static int16_t  g_bag[7];
static int16_t  g_bagIdx;               // next card to deal; 7 means "deck empty"

//*****************************************************************************
// 3. BOARD RULES (no drawing in this section)
//*****************************************************************************

// 7-bag randomizer: shuffle all 7 pieces, deal them one by one, reshuffle when
// the deck runs out. Every piece appears exactly once per 7, which feels much
// fairer than picking randomly each time (which allows long droughts).
static int16_t NextFromBag(void)
{
    int16_t i, j, t;

    if(g_bagIdx >= 7)
    {
        for(i = 0; i < 7; i++) g_bag[i] = i;

        // Fisher-Yates shuffle: walk from the end, swap each card with a
        // random card at or before it.
        for(i = 6; i > 0; i--)
        {
            j = (int16_t)(Rand16() % (uint16_t)(i + 1));
            t = g_bag[i];
            g_bag[i] = g_bag[j];
            g_bag[j] = t;
        }
        g_bagIdx = 0;
    }
    return g_bag[g_bagIdx++];
}

// Can piece (type, rot) sit with its 4x4 box at (px, py)?
// Returns false if any of its cells would be outside the left/right/bottom
// wall or overlap a locked block. Cells ABOVE the board (row < 0) are allowed,
// because pieces spawn partly above the top edge.
// This one function powers movement, rotation, gravity AND game over.
static bool Fits(int16_t type, int16_t rot, int16_t px, int16_t py)
{
    uint16_t mask = g_shapes[type][rot];
    int16_t r, c, br, bc;                       // box row/col, board row/col

    for(r = 0; r < 4; r++)
    {
        for(c = 0; c < 4; c++)
        {
            if(mask & (0x8000U >> (r * 4 + c)))     // is this cell part of the piece?
            {
                br = py + r;
                bc = px + c;

                if(bc < 0 || bc >= BOARD_W || br >= BOARD_H) return false;  // out of bounds
                if(br >= 0 && g_board[br][bc]) return false;                // hits a block
            }
        }
    }
    return true;
}

// Stamp a piece into the board permanently (called when it lands).
static void PlaceOnBoard(int16_t type, int16_t rot, int16_t px, int16_t py)
{
    uint16_t mask = g_shapes[type][rot];
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
                {
                    g_board[br][bc] = (uint16_t)(type + 1);     // color index
                }
            }
        }
    }
}

// Remove every full row, sliding everything above it down. Returns how many
// rows were removed (0..4).
static int16_t ClearFullRows(void)
{
    int16_t r, c, k, full, cleared = 0;

    r = BOARD_H - 1;                            // start at the bottom row
    while(r >= 0)
    {
        full = 1;
        for(c = 0; c < BOARD_W; c++)
        {
            if(!g_board[r][c]) { full = 0; break; }
        }

        if(full)
        {
            // Copy each row above r down by one, then blank the top row.
            for(k = r; k > 0; k--)
                for(c = 0; c < BOARD_W; c++) g_board[k][c] = g_board[k - 1][c];
            for(c = 0; c < BOARD_W; c++) g_board[0][c] = 0;
            cleared++;
            // Do NOT move r: the row that just slid into position r might
            // also be full, so check the same row again.
        }
        else
        {
            r--;
        }
    }
    return cleared;
}

//*****************************************************************************
// 4. DRAWING
//*****************************************************************************

// Draw one board cell with its top-left corner at pixel (px, py).
// Filled cells are 1 pixel smaller than CELL, which leaves a thin black gap
// between neighbors and gives the blocks a grid look. Empty cells are painted
// the full CELL size so that they also erase that gap.
static void DrawCell(int16_t px, int16_t py, uint16_t colorIdx)
{
     if (colorIdx == 0)
    {

        Graphics_FillRect(px, py, px + CELL - 1, py + CELL - 1, COLOR_BLACK);
        
        Graphics_setForegroundColor(&g_sContext, COLOR_TETRIS_GRID);
        
        // Draw the top and left lines of the cell bounding grid box
        Graphics_drawLine(&g_sContext, px, py, px + CELL - 1, py);
        Graphics_drawLine(&g_sContext, px, py, px, py + CELL - 1);
    }
    else
    {
    int16_t size = (colorIdx == 0) ? CELL : (CELL - 1);
    Graphics_FillRect(px, py, px + size - 1, py + size - 1, g_colors[colorIdx]);
    }
}

// Forget what is on the LCD, redraw next frame.
// Needed after the screen is cleared.
static void InvalidateScreenCache(void)
{
    int16_t r, c;

    for(r = 0; r < BOARD_H; r++)
        for(c = 0; c < BOARD_W; c++) g_shown[r][c] = 0xFFFF;    // matches no real value

    g_shownNext  = -1;
    g_shownScore = 0xFFFFFFFFUL;
    g_shownLines = 0xFFFF;
    g_shownLevel = 0xFFFF;
}

// Clear the screen and draw everything that never changes during a game:
// title, divider, playfield border and the panel labels.
static void DrawTetrisLayout(void)
{
    Graphics_Rectangle border;

    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"TETRIS",
                                AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, 309, 30);

    // Border sits 2 px outside the playfield so it never overlaps a cell.
    border.xMin = BOARD_X - 2;
    border.yMin = BOARD_Y - 2;
    border.xMax = BOARD_X + BOARD_W * CELL + 1;
    border.yMax = BOARD_Y + BOARD_H * CELL + 1;
    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawRectangle(&g_sContext, &border);

    // Left panel: score / lines / level.  Right panel: next piece.
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawString(&g_sContext, (int16_t *)"SCORE", AUTO_STRING_LENGTH, 5, 42,  OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"LINES", AUTO_STRING_LENGTH, 5, 97,  OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"LEVEL", AUTO_STRING_LENGTH, 5, 152, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"NEXT",  AUTO_STRING_LENGTH, 225, 42, OPAQUE_TEXT);

    InvalidateScreenCache();            // the screen was just wiped
}

// Draw the "next piece" preview, but only when the next piece has changed.
static void DrawNextPreview(void)
{
    uint16_t mask;
    int16_t r, c;

    if(g_nextType == g_shownNext) return;       // already showing it
    g_shownNext = g_nextType;

    Graphics_FillRect(225, 72, 225 + 4 * CELL + 4, 72 + 4 * CELL + 4, COLOR_BLACK);   // erase old one

    mask = g_shapes[g_nextType][0];             // always shown in rotation 0
    for(r = 0; r < 4; r++)
        for(c = 0; c < 4; c++)
            if(mask & (0x8000U >> (r * 4 + c)))
                DrawCell(230 + c * CELL, 77 + r * CELL, (uint16_t)(g_nextType + 1));
}

// Redraw score / lines / level, but only the ones whose value changed.
// The trailing spaces in each format string overwrite leftover digits when a
// number gets shorter (text drawn with OPAQUE_TEXT only covers its own glyphs).
static void DrawTetrisStats(void)
{
    char buf[20];

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);

    if(g_score != g_shownScore)
    {
        sprintf(buf, "%lu      ", (unsigned long)g_score);
        Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH, 5, 62, OPAQUE_TEXT);
        g_shownScore = g_score;
    }
    if(g_lines != g_shownLines)
    {
        sprintf(buf, "%u      ", g_lines);
        Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH, 5, 117, OPAQUE_TEXT);
        g_shownLines = g_lines;
    }
    if(g_level != g_shownLevel)
    {
        sprintf(buf, "%u      ", g_level);
        Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH, 5, 172, OPAQUE_TEXT);
        g_shownLevel = g_level;
    }
}

// Paint the playfield. For every cell: work out what SHOULD be there (locked
// block, or a cell of the falling piece on top), compare with what the LCD
// shows (g_shown), and repaint only if they differ. A piece moving one step
// changes a handful of cells, not 200.
static void RenderTetrisBoard(void)
{
    uint16_t mask = g_shapes[g_cur.type][g_cur.rot];
    uint16_t colorIdx;
    int16_t r, c, pr, pc;                       // board row/col, piece-box row/col

    for(r = 0; r < BOARD_H; r++)
    {
        for(c = 0; c < BOARD_W; c++)
        {
            colorIdx = g_board[r][c];           // start with the locked block (or empty)

            // Is this board cell covered by the falling piece's 4x4 box?
            pr = r - g_cur.y;
            pc = c - g_cur.x;
            if(pr >= 0 && pr < 4 && pc >= 0 && pc < 4)
            {
                if(mask & (0x8000U >> (pr * 4 + pc)))
                    colorIdx = (uint16_t)(g_cur.type + 1);      // piece draws on top
            }

            if(colorIdx != g_shown[r][c])       // changed since last frame?
            {
                DrawCell(BOARD_X + c * CELL, BOARD_Y + r * CELL, colorIdx);
                g_shown[r][c] = colorIdx;
            }
        }
    }

    DrawNextPreview();
    DrawTetrisStats();
}

//*****************************************************************************
// 5. GAME FLOW
//*****************************************************************************

// Bring in the next piece at the top. Returns false if it does not fit, which
// means the stack has reached the top: game over.
static bool SpawnPiece(void)
{
    g_cur.type = g_nextType;
    g_cur.rot  = 0;
    g_cur.x    = 3;                     // roughly centered on the 10-wide board
    g_cur.y    = 0;
    g_nextType = NextFromBag();
    g_gravityCount = 0;

    return Fits(g_cur.type, g_cur.rot, g_cur.x, g_cur.y);
}

// Start a new game: empty board, zeroed score, fresh layout, first piece.
static void ResetTetris(void)
{
    int16_t r, c;

    for(r = 0; r < BOARD_H; r++)
        for(c = 0; c < BOARD_W; c++) g_board[r][c] = 0;

    g_score = 0;
    g_lines = 0;
    g_level = 0;
    g_bagIdx = 7;                       // 7 = "deck empty", forces a shuffle
    g_nextType = NextFromBag();

    DrawTetrisLayout();
    SpawnPiece();                       // an empty board always has room
}

// Try to move the falling piece sideways by dir (-1 = left, +1 = right).
static void TetrisShift(int16_t dir)
{
    if(Fits(g_cur.type, g_cur.rot, g_cur.x + dir, g_cur.y)) g_cur.x += dir;
}

// Rotate the falling piece clockwise. If it will not fit in place, try nudging
// it one column left, then right (a simple "wall kick") so that rotating next
// to a wall still works. If nothing fits, the rotation is ignored.
static void TetrisRotate(void)
{
    int16_t nr = (g_cur.rot + 1) & 3;   // 0,1,2,3,0,... ('& 3' wraps at 4)

    if(Fits(g_cur.type, nr, g_cur.x, g_cur.y))              g_cur.rot = nr;
    else if(Fits(g_cur.type, nr, g_cur.x - 1, g_cur.y))   { g_cur.rot = nr; g_cur.x--; }
    else if(Fits(g_cur.type, nr, g_cur.x + 1, g_cur.y))   { g_cur.rot = nr; g_cur.x++; }
}

// The falling piece has landed: lock it into the board, clear any full rows,
// update score and level, and bring in the next piece.
// Returns false if the next piece cannot spawn (game over).
static bool LockAndSpawn(void)
{
    int16_t cleared;

    PlaceOnBoard(g_cur.type, g_cur.rot, g_cur.x, g_cur.y);

    cleared = ClearFullRows();
    if(cleared > 0)
    {
        // (uint32_t) first, so the multiplication is not done in 16 bits.
        g_score += (uint32_t)g_lineScore[cleared] * (g_level + 1);
        g_lines += cleared;
        g_level  = g_lines / 10;                // new level every 10 lines
        if(g_level > 9) g_level = 9;            // g_gravityFrames has 10 entries
    }
    g_score += 4;                               // small bonus for every piece locked

    return SpawnPiece();
}

// Show "GAME OVER", ignore input briefly (so a frantic button press does not
// skip it), then wait for a button press before returning.
static void TetrisGameOver(void)
{
    uint16_t i;

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"GAME OVER", AUTO_STRING_LENGTH,
                                BOARD_X + (BOARD_W * CELL) / 2,
                                BOARD_Y + (BOARD_H * CELL) / 2 - 12, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Press btn", AUTO_STRING_LENGTH,
                                BOARD_X + (BOARD_W * CELL) / 2,
                                BOARD_Y + (BOARD_H * CELL) / 2 + 12, OPAQUE_TEXT);

    for(i = 0; i < GAME_OVER_HOLD_MS / 10; i++) DELAY_US(10000);   // 10 ms steps

    do
    {
        Joy_Update();
        DELAY_US(10000);
    } while(!g_joy.btnPressed);
}

//*****************************************************************************
// 6. PUBLIC ENTRY POINT
//*****************************************************************************

// Play Tetris. Returns when the player holds the button (quit) or after a
// game over has been acknowledged. See the "app contract" in apps.h.
void Tetris_Run(void)
{
    uint16_t interval;                  // frames between gravity steps (from level)
    uint16_t frameNo = 0;               // counts frames, paces the soft drop
    int16_t  hTimer = 0;                // countdown for left/right auto-repeat
    bool     forceStep;                 // true = drop one row this frame no matter what
    bool     gameOver;

    // Seed from the free-running timer: the time between power-up and this
    // moment differs every run, so each game gets a different piece order.
    Rand_Seed(TimerNow());
    ResetTetris();

    while(1)
    {
        gameOver  = false;
        forceStep = false;
        frameNo++;

        // ---- 1. INPUT -----------------------------------------------------
        Joy_Update();
        if(g_joy.btnLong) return;                       // hold button = back to menu

        // Left/right: move once immediately when the stick is first pushed
        // (edgeX != 0), then wait DAS_FRAMES before repeating every ARR_FRAMES.
        if(g_joy.dirX == 0)
        {
            hTimer = 0;                                 // stick centered: reset
        }
        else if(g_joy.edgeX != 0)
        {
            TetrisShift(g_joy.dirX);
            hTimer = DAS_FRAMES;
        }
        else if(--hTimer <= 0)
        {
            TetrisShift(g_joy.dirX);
            hTimer = ARR_FRAMES;
        }

        if(g_joy.btnPressed) TetrisRotate();            // one rotation per tap

        if(g_joy.edgeY < 0)                             // stick pushed up: hard drop
        {
            while(Fits(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1)) g_cur.y++;
            forceStep = true;                           // lock it this frame
        }
        if(g_joy.dirY > 0 && (frameNo & 1)) forceStep = true;   // stick down: soft drop
                                                        // (every other frame, so it is
                                                        //  fast but still controllable)

        // ---- 2. GRAVITY ---------------------------------------------------
        interval = g_gravityFrames[g_level];
        g_gravityCount++;

        if(forceStep || g_gravityCount >= interval)
        {
            g_gravityCount = 0;

            if(Fits(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1))
            {
                g_cur.y++;                              // room below: fall one row
                if(g_joy.dirY > 0) g_score += 1;        // soft-drop bonus
            }
            else
            {
                // Resting on something: lock it. If the next piece cannot
                // spawn, the stack has reached the top.
                if(!LockAndSpawn()) gameOver = true;
            }
        }

        // ---- 3. DRAW ------------------------------------------------------
        RenderTetrisBoard();

        if(gameOver)
        {
            TetrisGameOver();
            return;                                     // back to the menu
        }

        DELAY_US(FRAME_US);                             // pace the loop
    }
}
