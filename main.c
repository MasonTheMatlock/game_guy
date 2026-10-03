//###########################################################################
// FILE:   main_menu.c
// TITLE:  Joystick game menu: TETRIS / RAYCASTER
//         TMS320F28069 + BOOSTXL-K350QVG-S1 (SSD2119) + analog joystick
//
//  Hardware (same as your joystick test):
//    VRx -> J7-63 / ADCINB7 (SOC0, CHSEL 15)
//    VRy -> J7-64 / ADCINB4 (SOC1, CHSEL 12)
//    SW  -> J2-11 / GPIO55, active low, internal pull-up
//
//  Controls
//    MENU      stick up/down = highlight, button = start
//    TETRIS    stick left/right = move (auto-repeat)   stick down = soft drop
//              stick up = hard drop                    button tap = rotate
//    RAYCASTER stick left/right = turn (analog)        stick up/down = walk
//              button tap = fire laser
//    ANY GAME  HOLD the button ~1.2 s = back to the menu
//
//  Keep the stick centered at power-up: it is calibrated at start.
//
//  Raycaster math is float. Build with --float_support=fpu32 for speed; it
//  also works (slower) with software floating point. Link the RTS math lib.
//###########################################################################

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "grlib.h"
#include <stdio.h>
#include <math.h>
#include "drivers/drivers.h"      // joystick + timer driver (joystick.c)


Graphics_Context g_sContext;

static uint32_t g_seed = 987654321UL;

//*****************************************************************************
// Small shared helpers
//*****************************************************************************
static uint16_t Rand16(void)
{
    g_seed = g_seed * 1664525UL + 1013904223UL;
    return (uint16_t)(g_seed >> 16);
}

static void FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t color)
{
    Graphics_Rectangle r;

    if(y1 < y0 || x1 < x0) return;
    r.xMin = x0; r.yMin = y0; r.xMax = x1; r.yMax = y1;
    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_fillRectangle(&g_sContext, &r);
}

//*****************************************************************************
//*****************************************************************************
//  GAME 1: TETRIS
//*****************************************************************************
//*****************************************************************************
#define BOARD_W            10
#define BOARD_H            20
#define CELL               10       // pixels per cell
#define BOARD_X            110      // top-left of playfield (pixels)
#define BOARD_Y            36

#define T_FRAME_US         16000UL
#define DAS_FRAMES         9        // delay before left/right auto-repeat
#define ARR_FRAMES         3        // frames between auto-repeat moves
#define GAME_OVER_HOLD_MS  1500

// Piece data (4x4 bitmasks, bit 0x8000 = row0/col0), 7 pieces x 4 rotations
// Order: I, J, L, O, S, T, Z
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

// Index 0 = empty, 1..7 = piece type + 1
static const uint32_t g_colors[8] =
{
    RGB_BLACK,
    colo,   // I  cyan
    0x0000FF,   // J  blue
    0xFF8000,   // L  orange
    0xFFFF00,   // O  yellow
    0x00FF00,   // S  green
    0xA000FF,   // T  purple
    0xFF0000    // Z  red
};

static const uint16_t g_lineScore[5]      = { 0, 40, 100, 300, 1200 };
static const uint16_t g_gravityFrames[10] = { 30, 26, 22, 18, 15, 12, 10, 8, 6, 5 };

typedef struct
{
    int16_t type;   // 0..6
    int16_t rot;    // 0..3
    int16_t x;      // column of 4x4 box's left edge (can be negative)
    int16_t y;      // row of 4x4 box's top edge
} Piece;

static uint16_t g_board[BOARD_H][BOARD_W];   // 0 = empty, else color index
static uint16_t g_shown[BOARD_H][BOARD_W];   // what is currently on the LCD

static Piece    g_cur;
static int16_t  g_nextType;
static int16_t  g_shownNext;

static uint32_t g_score;
static uint16_t g_lines;
static uint16_t g_level;
static uint32_t g_shownScore;
static uint16_t g_shownLines;
static uint16_t g_shownLevel;

static uint16_t g_gravityCount;

// 7-bag randomizer
static int16_t  g_bag[7];
static int16_t  g_bagIdx;

static int16_t NextFromBag(void)
{
    int16_t i, j, t;

    if(g_bagIdx >= 7)
    {
        for(i = 0; i < 7; i++) g_bag[i] = i;
        for(i = 6; i > 0; i--)
        {
            j = (int16_t)(Rand16() % (uint16_t)(i + 1));
            t = g_bag[i]; g_bag[i] = g_bag[j]; g_bag[j] = t;
        }
        g_bagIdx = 0;
    }
    return g_bag[g_bagIdx++];
}

// Does piece (type, rot) fit with its 4x4 box at (px, py)?
static int16_t Fits(int16_t type, int16_t rot, int16_t px, int16_t py)
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
                if(bc < 0 || bc >= BOARD_W || br >= BOARD_H) return 0;
                if(br >= 0 && g_board[br][bc]) return 0;
            }
        }
    }
    return 1;
}

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
                    g_board[br][bc] = (uint16_t)(type + 1);
                }
            }
        }
    }
}

// Remove full rows; returns how many were removed
static int16_t ClearFullRows(void)
{
    int16_t r, c, k, full, cleared = 0;

    r = BOARD_H - 1;
    while(r >= 0)
    {
        full = 1;
        for(c = 0; c < BOARD_W; c++)
        {
            if(!g_board[r][c]) { full = 0; break; }
        }

        if(full)
        {
            for(k = r; k > 0; k--)
                for(c = 0; c < BOARD_W; c++) g_board[k][c] = g_board[k - 1][c];
            for(c = 0; c < BOARD_W; c++) g_board[0][c] = 0;
            cleared++;
        }
        else
        {
            r--;
        }
    }
    return cleared;
}

// ---- Tetris drawing --------------------------------------------------------
static void DrawCell(int16_t px, int16_t py, uint16_t colorIdx)
{
    int16_t size = (colorIdx == 0) ? CELL : (CELL - 1);   // 1px gap = grid look
    FillRect(px, py, px + size - 1, py + size - 1, g_colors[colorIdx]);
}

static void InvalidateScreenCache(void)
{
    int16_t r, c;
    for(r = 0; r < BOARD_H; r++)
        for(c = 0; c < BOARD_W; c++) g_shown[r][c] = 0xFFFF;

    g_shownNext  = -1;
    g_shownScore = 0xFFFFFFFFUL;
    g_shownLines = 0xFFFF;
    g_shownLevel = 0xFFFF;
}

static void DrawTetrisLayout(void)
{
    Graphics_Rectangle border;

    Graphics_setBackgroundColor(&g_sContext, RGB_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCm20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"TETRIS",
                                AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, 309, 30);

    border.xMin = BOARD_X - 2;
    border.yMin = BOARD_Y - 2;
    border.xMax = BOARD_X + BOARD_W * CELL + 1;
    border.yMax = BOARD_Y + BOARD_H * CELL + 1;
    Graphics_setForegroundColor(&g_sContext, RGB_GRAY);
    Graphics_drawRectangle(&g_sContext, &border);

    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    Graphics_drawString(&g_sContext, (int16_t *)"SCORE", AUTO_STRING_LENGTH, 5, 42,  OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"LINES", AUTO_STRING_LENGTH, 5, 97,  OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"LEVEL", AUTO_STRING_LENGTH, 5, 152, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int16_t *)"NEXT",  AUTO_STRING_LENGTH, 225, 42, OPAQUE_TEXT);

    InvalidateScreenCache();
}

static void DrawNextPreview(void)
{
    uint16_t mask;
    int16_t r, c;

    if(g_nextType == g_shownNext) return;
    g_shownNext = g_nextType;

    FillRect(225, 72, 225 + 4 * CELL + 4, 72 + 4 * CELL + 4, RGB_BLACK);

    mask = g_shapes[g_nextType][0];
    for(r = 0; r < 4; r++)
        for(c = 0; c < 4; c++)
            if(mask & (0x8000U >> (r * 4 + c)))
                DrawCell(230 + c * CELL, 77 + r * CELL, (uint16_t)(g_nextType + 1));
}

static void DrawTetrisStats(void)
{
    char buf[16];

    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);

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

// Compose locked board + active piece; redraw only cells that changed
static void RenderTetrisBoard(void)
{
    uint16_t mask = g_shapes[g_cur.type][g_cur.rot];
    uint16_t colorIdx;
    int16_t r, c, pr, pc;

    for(r = 0; r < BOARD_H; r++)
    {
        for(c = 0; c < BOARD_W; c++)
        {
            colorIdx = g_board[r][c];

            pr = r - g_cur.y;
            pc = c - g_cur.x;
            if(pr >= 0 && pr < 4 && pc >= 0 && pc < 4)
            {
                if(mask & (0x8000U >> (pr * 4 + pc)))
                    colorIdx = (uint16_t)(g_cur.type + 1);
            }

            if(colorIdx != g_shown[r][c])
            {
                DrawCell(BOARD_X + c * CELL, BOARD_Y + r * CELL, colorIdx);
                g_shown[r][c] = colorIdx;
            }
        }
    }
    DrawNextPreview();
    DrawTetrisStats();
}

// ---- Tetris game logic -----------------------------------------------------
static int16_t SpawnPiece(void)
{
    g_cur.type = g_nextType;
    g_cur.rot  = 0;
    g_cur.x    = 3;
    g_cur.y    = 0;
    g_nextType = NextFromBag();
    g_gravityCount = 0;

    return Fits(g_cur.type, g_cur.rot, g_cur.x, g_cur.y);
}

static void ResetTetris(void)
{
    int16_t r, c;

    for(r = 0; r < BOARD_H; r++)
        for(c = 0; c < BOARD_W; c++) g_board[r][c] = 0;

    g_score = 0;
    g_lines = 0;
    g_level = 0;
    g_bagIdx = 7;                 // force reshuffle
    g_nextType = NextFromBag();

    DrawTetrisLayout();
    SpawnPiece();
}

static void TetrisShift(int16_t dir)
{
    if(Fits(g_cur.type, g_cur.rot, g_cur.x + dir, g_cur.y)) g_cur.x += dir;
}

static void TetrisRotate(void)
{
    int16_t nr = (g_cur.rot + 1) & 3;

    if(Fits(g_cur.type, nr, g_cur.x, g_cur.y))              g_cur.rot = nr;
    else if(Fits(g_cur.type, nr, g_cur.x - 1, g_cur.y))   { g_cur.rot = nr; g_cur.x--; }   // wall kick
    else if(Fits(g_cur.type, nr, g_cur.x + 1, g_cur.y))   { g_cur.rot = nr; g_cur.x++; }
}

static void TetrisLock(int16_t *gameOver)
{
    int16_t cleared;

    PlaceOnBoard(g_cur.type, g_cur.rot, g_cur.x, g_cur.y);

    cleared = ClearFullRows();
    if(cleared > 0)
    {
        g_score += (uint32_t)g_lineScore[cleared] * (g_level + 1);
        g_lines += cleared;
        g_level  = g_lines / 10;
        if(g_level > 9) g_level = 9;
    }
    g_score += 4;   // small bonus per piece locked

    if(cleared > 0) RenderTetrisBoard();

    if(!SpawnPiece()) *gameOver = 1;
}

static void TetrisGameOver(void)
{
    uint16_t i;

    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    Graphics_setBackgroundColor(&g_sContext, RGB_BLACK);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"GAME OVER", AUTO_STRING_LENGTH,
                                BOARD_X + (BOARD_W * CELL) / 2,
                                BOARD_Y + (BOARD_H * CELL) / 2 - 12, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Press btn", AUTO_STRING_LENGTH,
                                BOARD_X + (BOARD_W * CELL) / 2,
                                BOARD_Y + (BOARD_H * CELL) / 2 + 12, OPAQUE_TEXT);

    for(i = 0; i < GAME_OVER_HOLD_MS / 10; i++) DELAY_US(10000);   // ignore input briefly

    do
    {
        Joy_Update();
        DELAY_US(10000);
    } while(!g_joy.btnPressed);
}

static void Tetris_Run(void)
{
    uint16_t interval;
    uint16_t frameNo = 0;
    int16_t  gameOver, forceStep, hTimer = 0;

    g_seed ^= CpuTimer0Regs.TIM.all;      // different game every time
    ResetTetris();

    while(1)
    {
        gameOver  = 0;
        forceStep = 0;
        frameNo++;

        // ---- 1. Input -----------------------------------------------------
        Joy_Update();
        if(g_joy.btnLong) return;                      // hold button = menu

        if(g_joy.dirX == 0)
        {
            hTimer = 0;
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

        if(g_joy.btnPressed) TetrisRotate();

        if(g_joy.edgeY < 0)                            // stick up = hard drop
        {
            while(Fits(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1)) g_cur.y++;
            forceStep = 1;
        }
        if(g_joy.dirY > 0 && (frameNo & 1)) forceStep = 1;   // stick down = soft drop

        // ---- 2. Gravity ---------------------------------------------------
        interval = g_gravityFrames[g_level];
        g_gravityCount++;

        if(forceStep || g_gravityCount >= interval)
        {
            g_gravityCount = 0;

            if(Fits(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1))
            {
                g_cur.y++;
                if(g_joy.dirY > 0) g_score += 1;
            }
            else
            {
                TetrisLock(&gameOver);
            }
        }

        // ---- 3. Draw ------------------------------------------------------
        RenderTetrisBoard();

        if(gameOver)
        {
            TetrisGameOver();
            return;
        }

        DELAY_US(T_FRAME_US);
    }
}

//*****************************************************************************
//*****************************************************************************
//  GAME 2: RAYCASTER (maze + enemies + laser)
//*****************************************************************************
//*****************************************************************************

// Conventions: map[y][x], y grows downward. Heading is a float in units of
// 1/512 turn: 0 = east(+x), 128 = south(+y), 256 = west, 384 = north.
// Increasing the heading turns the viewer to the RIGHT.

#define VIEW_H             200      // 3D view is 320 x 200, HUD below it
#define COL_W              4        // pixels per ray column (8 = faster, blockier)
#define NUM_COLS           (SCREEN_W / COL_W)

#define MAP_W              17       // must be odd
#define MAP_H              17       // must be odd

#define ANG_STEPS          512
#define PLANE_LEN          0.66f    // camera plane length (~66 deg field of view)
#define PROJ_SCALE         ((SCREEN_W / 2) / PLANE_LEN)   // wall height at dist 1
#define FOG_DIST           7.0f
#define MAX_RAY_STEPS      64

// Player speeds at full stick deflection (frame-rate independent)
#define MOVE_RATE          2.4f     // cells per second
#define TURN_RATE          180.0f   // heading units per second (~127 deg/s)
#define PLAYER_RADIUS      0.20f

// Enemies
#define NUM_ENEMIES        6
#define ENEMY_HP           2        // laser hits to kill
#define SPRITE_SIZE        0.45f    // world size of an enemy
#define SPRITE_Z           0.45f    // hover height (0 = floor, 1 = ceiling)
#define SPRITE_BOB         0.04f
#define SPRITE_MIN_DEPTH   0.25f

// Laser
#define LASER_RANGE        7.0f     // cells
#define LASER_FRAMES       6        // beam duration in frames
#define LASER_COOLDOWN     5
#define EXPLODE_FRAMES     10
#define BANNER_HOLD_MS     1200

#define RGB_CEILING        0x1A2030
#define RGB_FLOOR          0x3A3026
#define RGB_MM_FLOOR       0x202020
#define RGB_MM_EXIT        0xFF0000
#define RGB_MM_ENEMY       0xFF40FF
#define RGB_MM_PLAYER      0xFFFF00
#define RGB_BODY           0xB040F0
#define RGB_EYE_A          0xFF2020
#define RGB_EYE_B          0xFFE020

// Mini-map / HUD layout
#define MM_SCALE           2        // pixels per map cell
#define MM_X               4
#define MM_Y               (VIEW_H + 3)
#define HUD_TEXT_X         48
#define HUD_TEXT_Y         (VIEW_H + 10)

// Enemy states
#define ES_DEAD            0
#define ES_ALIVE           1
#define ES_DYING           2

typedef struct
{
    int16_t  cx, cy;         // map cell
    float    x, y;           // world position (cell center)
    int16_t  hp;
    int16_t  state;
    int16_t  timer;          // explosion countdown
    int16_t  phase;          // animation phase offset
    // filled in by the renderer each frame (used by the laser)
    int16_t  vis;            // 1 = at least one column of it is visible
    float    scrX, scrY;     // screen position of its center
    float    depth;          // distance along the view direction
    float    half;           // half-size in pixels (before explosion scaling)
} Enemy;

typedef struct
{
    float    scrX, cy, half, depth;
    uint32_t cTop, cMid, cBot;
    float    bandLo, bandHi, bandT;   // middle band rows (x half) and max |t|
} VisSprite;

// Wall type 1..4 base colors (index 0 unused)
static const uint32_t g_wallBase[5] =
{
    0x000000, 0xD04040, 0x40B040, 0x4060E0, 0xD0B030
};

static uint16_t g_map[MAP_H][MAP_W];     // 0 = open, 1..4 = wall type
static int16_t  g_stack[MAP_W * MAP_H];  // maze generation stack

static float    g_cosTab[ANG_STEPS];
static float    g_sinTab[ANG_STEPS];

static float    g_px, g_py;              // player position (map units)
static float    g_head;                  // heading, 0..512

static int16_t  g_exitX, g_exitY;
static int16_t  g_mazeDone;
static uint16_t g_mazeNum;
static uint16_t g_frame;

static Enemy     g_enemy[NUM_ENEMIES];
static VisSprite g_vis[NUM_ENEMIES];
static int16_t   g_nvis;
static uint16_t  g_kills;
static uint16_t  g_enemyTotal;
static int16_t   g_laserTimer;
static int16_t   g_laserCooldown;
static int16_t   g_laserTarget;

// Per-frame wall info (z-buffer) and the on-screen column cache
static int16_t   g_wTop[NUM_COLS];
static int16_t   g_wBot[NUM_COLS];
static uint16_t  g_wKey[NUM_COLS];
static float     g_zbuf[NUM_COLS];
static int16_t   g_colTop[NUM_COLS];     // -1 = column must be fully redrawn
static int16_t   g_colBot[NUM_COLS];
static uint16_t  g_colKey[NUM_COLS];

// Mini-map / HUD caches
static int16_t   g_mmX, g_mmY;
static uint16_t  g_shownMaze, g_shownKills;

static const int16_t g_ndx[4] = {  1, 0, -1,  0 };   // E, S, W, N
static const int16_t g_ndy[4] = {  0, 1,  0, -1 };

// ---- Raycaster helpers -----------------------------------------------------
static void BuildTrigTables(void)
{
    int16_t i;
    float a;

    for(i = 0; i < ANG_STEPS; i++)
    {
        a = (float)i * (6.2831853f / (float)ANG_STEPS);
        g_cosTab[i] = cosf(a);
        g_sinTab[i] = sinf(a);
    }
}

static int16_t HeadIdx(void)
{
    int16_t i = (int16_t)g_head;

    if(i < 0) i = 0;
    if(i > ANG_STEPS - 1) i = ANG_STEPS - 1;
    return i;
}

// Brightness level s = 0..7 -> scales each channel by (s+1)/8
static uint32_t ShadeColor(uint32_t base, int16_t s)
{
    uint32_t k = (uint32_t)(s + 1);
    uint32_t r = (((base >> 16) & 0xFF) * k) >> 3;
    uint32_t g = (((base >>  8) & 0xFF) * k) >> 3;
    uint32_t b = ((base & 0xFF) * k) >> 3;

    return (r << 16) | (g << 8) | b;
}

static uint32_t WallColor(uint16_t key)
{
    return ShadeColor(g_wallBase[key >> 3], (int16_t)(key & 7));
}

static int16_t FogLevel(float depth, float minBright)
{
    float b = 1.0f - depth / FOG_DIST;
    int16_t s;

    if(b < minBright) b = minBright;
    s = (int16_t)(b * 8.0f);
    if(s > 7) s = 7;
    if(s < 0) s = 0;
    return s;
}

// ---- Maze generation (DFS "recursive backtracker") -------------------------
static void GenerateMaze(void)
{
    int16_t x, y, sp, d, n, cur, cx, cy, nx, ny;
    int16_t list[4];

    // Start with solid walls; wall type varies by region for orientation
    for(y = 0; y < MAP_H; y++)
        for(x = 0; x < MAP_W; x++)
            g_map[y][x] = (uint16_t)(1 + (((x >> 2) + (y >> 2)) & 3));

    g_map[1][1] = 0;
    sp = 0;
    g_stack[sp++] = 1 * MAP_W + 1;

    while(sp > 0)
    {
        cur = g_stack[sp - 1];
        cx = cur % MAP_W;
        cy = cur / MAP_W;

        n = 0;
        for(d = 0; d < 4; d++)
        {
            nx = cx + 2 * g_ndx[d];
            ny = cy + 2 * g_ndy[d];
            if(nx > 0 && nx < MAP_W - 1 && ny > 0 && ny < MAP_H - 1 && g_map[ny][nx] != 0)
                list[n++] = d;
        }

        if(n == 0)
        {
            sp--;                           // dead end: backtrack
        }
        else
        {
            d = list[Rand16() % (uint16_t)n];
            g_map[cy + g_ndy[d]][cx + g_ndx[d]] = 0;          // knock down wall between
            g_map[cy + 2 * g_ndy[d]][cx + 2 * g_ndx[d]] = 0;  // open the next cell
            g_stack[sp++] = (cy + 2 * g_ndy[d]) * MAP_W + (cx + 2 * g_ndx[d]);
        }
    }

    g_exitX = MAP_W - 2;
    g_exitY = MAP_H - 2;
}

// ---- Enemies: placement, mini-map, HUD --------------------------------------
static void PlaceEnemies(void)
{
    int16_t i, j, n, x, y, ok, tries;

    for(i = 0; i < NUM_ENEMIES; i++) g_enemy[i].state = ES_DEAD;

    n = 0;
    tries = 0;
    while(n < NUM_ENEMIES && tries < 500)
    {
        tries++;
        x = 1 + 2 * (int16_t)(Rand16() % ((MAP_W - 1) / 2));
        y = 1 + 2 * (int16_t)(Rand16() % ((MAP_H - 1) / 2));

        if((x - 1) + (y - 1) < 4) continue;                 // keep the spawn area clear
        if(x == g_exitX && y == g_exitY) continue;

        ok = 1;
        for(j = 0; j < n; j++)
            if(g_enemy[j].cx == x && g_enemy[j].cy == y) ok = 0;
        if(!ok) continue;

        g_enemy[n].cx    = x;
        g_enemy[n].cy    = y;
        g_enemy[n].x     = (float)x + 0.5f;
        g_enemy[n].y     = (float)y + 0.5f;
        g_enemy[n].hp    = ENEMY_HP;
        g_enemy[n].state = ES_ALIVE;
        g_enemy[n].timer = 0;
        g_enemy[n].phase = (int16_t)(Rand16() & 511);
        g_enemy[n].vis   = 0;
        n++;
    }

    g_enemyTotal = (uint16_t)n;
    g_kills = 0;
}

static int16_t EnemyAliveAt(int16_t cx, int16_t cy)
{
    int16_t i;

    for(i = 0; i < NUM_ENEMIES; i++)
        if(g_enemy[i].state == ES_ALIVE && g_enemy[i].cx == cx && g_enemy[i].cy == cy)
            return 1;
    return 0;
}

static void DrawMiniCell(int16_t cx, int16_t cy)
{
    uint32_t c = RGB_MM_FLOOR;
    int16_t  x = MM_X + cx * MM_SCALE;
    int16_t  y = MM_Y + cy * MM_SCALE;

    if(g_map[cy][cx])                           c = g_wallBase[g_map[cy][cx]];
    else if(cx == g_exitX && cy == g_exitY)     c = RGB_MM_EXIT;
    else if(EnemyAliveAt(cx, cy))               c = RGB_MM_ENEMY;

    FillRect(x, y, x + MM_SCALE - 1, y + MM_SCALE - 1, c);
}

static void DrawHudStatic(void)
{
    int16_t x, y;

    FillRect(0, VIEW_H, SCREEN_W - 1, 239, RGB_BLACK);
    Graphics_setForegroundColor(&g_sContext, RGB_GRAY);
    Graphics_drawLine(&g_sContext, 0, VIEW_H, SCREEN_W - 1, VIEW_H);

    for(y = 0; y < MAP_H; y++)
        for(x = 0; x < MAP_W; x++)
            DrawMiniCell(x, y);

    g_mmX = -1;
    g_mmY = -1;
    g_shownMaze  = 0xFFFF;
    g_shownKills = 0xFFFF;
}

static void DrawMiniPlayer(void)
{
    int16_t ix = (int16_t)(g_px * (float)MM_SCALE) - 1;
    int16_t iy = (int16_t)(g_py * (float)MM_SCALE) - 1;
    int16_t cx, cy;

    if(ix == g_mmX && iy == g_mmY) return;

    // Restore the map cells under the old dot
    if(g_mmX >= 0)
    {
        for(cy = g_mmY / MM_SCALE; cy <= (g_mmY + 1) / MM_SCALE; cy++)
            for(cx = g_mmX / MM_SCALE; cx <= (g_mmX + 1) / MM_SCALE; cx++)
                DrawMiniCell(cx, cy);
    }

    FillRect(MM_X + ix, MM_Y + iy, MM_X + ix + 1, MM_Y + iy + 1, RGB_MM_PLAYER);
    g_mmX = ix;
    g_mmY = iy;
}

static void DrawHud(void)
{
    char buf[28];

    if(g_mazeNum == g_shownMaze && g_kills == g_shownKills) return;

    sprintf(buf, "Maze %u  Kills %u/%u     ", g_mazeNum, g_kills, g_enemyTotal);
    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    Graphics_drawString(&g_sContext, (int16_t *)buf, AUTO_STRING_LENGTH,
                        HUD_TEXT_X, HUD_TEXT_Y, OPAQUE_TEXT);
    g_shownMaze  = g_mazeNum;
    g_shownKills = g_kills;
}

// ---- Ray casting (walls) ----------------------------------------------------

// Cast the ray for one screen column. Returns wall top/bottom rows, a "key"
// (wall type + quantized brightness) and the perpendicular distance (z-buffer).
static void CastColumn(int16_t col, float dirX, float dirY,
                       float planeX, float planeY,
                       int16_t *pTop, int16_t *pBot, uint16_t *pKey, float *pDist)
{
    float camX  = (2.0f * ((float)col + 0.5f)) / (float)NUM_COLS - 1.0f;
    float rayX  = dirX + planeX * camX;
    float rayY  = dirY + planeY * camX;
    int16_t mapX = (int16_t)g_px;
    int16_t mapY = (int16_t)g_py;
    float deltaX = (rayX == 0.0f) ? 1.0e30f : fabsf(1.0f / rayX);
    float deltaY = (rayY == 0.0f) ? 1.0e30f : fabsf(1.0f / rayY);
    float sideX, sideY, perp, hf;
    int16_t stepX, stepY, side = 0, i, h, s;

    if(rayX < 0.0f) { stepX = -1; sideX = (g_px - (float)mapX) * deltaX; }
    else            { stepX =  1; sideX = ((float)mapX + 1.0f - g_px) * deltaX; }

    if(rayY < 0.0f) { stepY = -1; sideY = (g_py - (float)mapY) * deltaY; }
    else            { stepY =  1; sideY = ((float)mapY + 1.0f - g_py) * deltaY; }

    // DDA: walk grid line to grid line until a wall cell is entered
    for(i = 0; i < MAX_RAY_STEPS; i++)
    {
        if(sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
        else              { sideY += deltaY; mapY += stepY; side = 1; }

        if(g_map[mapY][mapX]) break;
    }

    // Perpendicular distance (no fisheye)
    perp = (side == 0) ? (sideX - deltaX) : (sideY - deltaY);
    if(perp < 0.05f) perp = 0.05f;

    // Wall height on screen, forced even so it is centered on the view
    hf = PROJ_SCALE / perp;
    h  = (hf > (float)VIEW_H) ? VIEW_H : (int16_t)hf;
    h  = h & 0xFFFE;
    if(h < 2) h = 2;

    *pTop  = VIEW_H / 2 - h / 2;
    *pBot  = VIEW_H / 2 + h / 2 - 1;
    *pDist = perp;

    // Brightness: fog with distance, darker on y-facing walls
    s = FogLevel(perp, 0.15f);
    if(side == 1) s = (s * 7) / 10;

    *pKey = (uint16_t)((g_map[mapY][mapX] << 3) | s);
}

// Draw one wall-only column, touching only pixels that changed since last
// frame. The wall is always centered on the view, so the old and new spans
// always overlap (rows 99..100), which keeps the delta logic simple.
static void DrawColumn(int16_t col, int16_t nt, int16_t nb, uint16_t nk)
{
    int16_t  x0 = col * COL_W;
    int16_t  x1 = x0 + COL_W - 1;
    int16_t  pt = g_colTop[col];
    int16_t  pb = g_colBot[col];
    uint16_t pk = g_colKey[col];
    uint32_t wc = WallColor(nk);
    int16_t  lo, hi;

    if(pt < 0)
    {
        // Cache invalid (first time, or a sprite/laser/crosshair touched it)
        FillRect(x0, 0,      x1, nt - 1,     RGB_CEILING);
        FillRect(x0, nt,     x1, nb,         wc);
        FillRect(x0, nb + 1, x1, VIEW_H - 1, RGB_FLOOR);
    }
    else
    {
        if(nt == pt && nb == pb && nk == pk) return;

        if(nt > pt)      FillRect(x0, pt, x1, nt - 1, RGB_CEILING);
        else if(nt < pt) FillRect(x0, nt, x1, pt - 1, wc);

        if(nb < pb)      FillRect(x0, nb + 1, x1, pb, RGB_FLOOR);
        else if(nb > pb) FillRect(x0, pb + 1, x1, nb, wc);

        if(nk != pk)
        {
            lo = (nt > pt) ? nt : pt;
            hi = (nb < pb) ? nb : pb;
            FillRect(x0, lo, x1, hi, wc);
        }
    }

    g_colTop[col] = nt;
    g_colBot[col] = nb;
    g_colKey[col] = nk;
}

// ---- Sprites (enemies) ------------------------------------------------------

// Project every enemy into screen space, work out which are visible (using
// the wall z-buffer), style them, and build a near-to-far list to draw.
static void ProjectEnemies(float dirX, float dirY)
{
    int16_t i, j, col, c0, c1, s, eyeS, flash;
    float sx, sy, depth, lat, scrX, half, H2, z, cy, tt;
    Enemy *e;
    VisSprite v;

    g_nvis = 0;

    for(i = 0; i < NUM_ENEMIES; i++)
    {
        e = &g_enemy[i];
        e->vis = 0;
        if(e->state == ES_DEAD) continue;

        sx    = e->x - g_px;
        sy    = e->y - g_py;
        depth = dirX * sx + dirY * sy;                 // distance along view direction
        if(depth < SPRITE_MIN_DEPTH) continue;

        lat  = dirX * sy - dirY * sx;                  // offset to the viewer's right
        scrX = (float)(SCREEN_W / 2) + lat * PROJ_SCALE / depth;
        H2   = PROJ_SCALE / depth * 0.5f;              // half wall height in pixels
        half = SPRITE_SIZE * H2;                       // half sprite size in pixels
        z    = SPRITE_Z + g_sinTab[(g_frame * 12 + e->phase) & (ANG_STEPS - 1)] * SPRITE_BOB;
        cy   = (float)(VIEW_H / 2) + H2 * (1.0f - 2.0f * z);

        e->scrX  = scrX;
        e->scrY  = cy;
        e->depth = depth;
        e->half  = half;

        if(e->state == ES_DYING)
        {
            tt = (float)(EXPLODE_FRAMES - e->timer);
            half *= (1.0f + 0.10f * tt);               // explosion expands
        }

        if(scrX + half < 0.0f || scrX - half >= (float)SCREEN_W) continue;

        c0 = (int16_t)((scrX - half) / (float)COL_W);
        c1 = (int16_t)((scrX + half) / (float)COL_W);
        if(c0 < 0) c0 = 0;
        if(c1 > NUM_COLS - 1) c1 = NUM_COLS - 1;

        for(col = c0; col <= c1; col++)
        {
            if(depth < g_zbuf[col]) { e->vis = 1; break; }
        }
        if(!e->vis) continue;

        v.scrX  = scrX;
        v.half  = half;
        v.cy    = cy;
        v.depth = depth;

        if(e->state == ES_ALIVE)
        {
            s     = FogLevel(depth, 0.25f);
            eyeS  = (s < 3) ? 3 : s;
            flash = (g_laserTimer > 0 && g_laserTarget == i);

            if(flash)
            {
                v.cTop = 0xFFFFFF;
                v.cBot = 0xC0C0C0;
                v.cMid = (g_frame & 1) ? 0xFFFF00 : 0xFF0000;
            }
            else
            {
                v.cTop = ShadeColor(RGB_BODY, s);
                v.cBot = ShadeColor(RGB_BODY, (s >= 3) ? (s - 3) : 0);
                v.cMid = ShadeColor((((g_frame >> 3) + e->phase) & 1) ? RGB_EYE_A : RGB_EYE_B, eyeS);
            }
            v.bandLo = -0.30f;
            v.bandHi =  0.05f;
            v.bandT  =  0.65f;
        }
        else
        {
            // Explosion
            if(e->timer <= 3)     { v.cTop = 0x802000; v.cBot = 0x802000; v.cMid = 0xC04000; }
            else if(e->timer & 1) { v.cTop = 0xFF6000; v.cBot = 0xFF6000; v.cMid = 0xFFFF00; }
            else                  { v.cTop = 0xFF9000; v.cBot = 0xFF9000; v.cMid = 0xFFFFFF; }
            v.bandLo = -0.40f;
            v.bandHi =  0.40f;
            v.bandT  =  0.75f;
        }

        // Insert sorted by depth, nearest first
        j = g_nvis++;
        while(j > 0 && g_vis[j - 1].depth > v.depth)
        {
            g_vis[j] = g_vis[j - 1];
            j--;
        }
        g_vis[j] = v;
    }
}

// Fill rows y0..y1 of a column with what is behind the sprites
static void FillBackground(int16_t x0, int16_t x1, int16_t y0, int16_t y1,
                           int16_t nt, int16_t nb, uint32_t wc)
{
    int16_t a, b;

    if(y1 < y0) return;

    b = (y1 < nt - 1) ? y1 : (nt - 1);
    FillRect(x0, y0, x1, b, RGB_CEILING);

    a = (y0 > nt) ? y0 : nt;
    b = (y1 < nb) ? y1 : nb;
    FillRect(x0, a, x1, b, wc);

    a = (y0 > nb + 1) ? y0 : (nb + 1);
    FillRect(x0, a, x1, y1, RGB_FLOOR);
}

// If a sprite covers this column, draw the whole column in one top-to-bottom
// pass (every pixel written once, so no flicker) and return 1.
static int16_t DrawSpriteColumn(int16_t col)
{
    int16_t   k, yT, yB, bt, bb;
    int16_t   x0 = col * COL_W;
    int16_t   x1 = x0 + COL_W - 1;
    int16_t   nt = g_wTop[col];
    int16_t   nb = g_wBot[col];
    float     px = (float)(x0 + COL_W / 2);
    float     t, dyf;
    uint32_t  wc = WallColor(g_wKey[col]);
    VisSprite *v;

    for(k = 0; k < g_nvis; k++)
    {
        v = &g_vis[k];
        if(v->depth >= g_zbuf[col]) continue;          // hidden behind a wall

        t = (px - v->scrX) / v->half;                  // -1..1 across the sprite
        if(t <= -1.0f || t >= 1.0f) continue;

        dyf = sqrtf(1.0f - t * t);                     // circular outline
        yT = (int16_t)(v->cy - v->half * dyf);
        yB = (int16_t)(v->cy + v->half * dyf);
        if(yT < 0) yT = 0;
        if(yB > VIEW_H - 1) yB = VIEW_H - 1;
        if(yT > yB) continue;

        FillBackground(x0, x1, 0, yT - 1, nt, nb, wc);

        if(t > -v->bandT && t < v->bandT)
        {
            bt = (int16_t)(v->cy + v->bandLo * v->half);
            bb = (int16_t)(v->cy + v->bandHi * v->half);
            if(bt < yT) bt = yT;
            if(bt > yB + 1) bt = yB + 1;
            if(bb < bt - 1) bb = bt - 1;
            if(bb > yB) bb = yB;

            FillRect(x0, yT,     x1, bt - 1, v->cTop);
            FillRect(x0, bt,     x1, bb,     v->cMid);
            FillRect(x0, bb + 1, x1, yB,     v->cBot);
        }
        else
        {
            FillRect(x0, yT, x1, yB, v->cBot);         // dark rim
        }

        FillBackground(x0, x1, yB + 1, VIEW_H - 1, nt, nb, wc);

        g_colTop[col] = -1;                            // repaint fully next frame
        return 1;
    }
    return 0;
}

// Enemy under the center of the screen (nearest), or -1
static int16_t CrosshairTarget(void)
{
    int16_t i, best = -1;
    float   bd = 1.0e9f, err;
    Enemy  *e;

    for(i = 0; i < NUM_ENEMIES; i++)
    {
        e = &g_enemy[i];
        if(e->state != ES_ALIVE || !e->vis || e->depth > LASER_RANGE) continue;

        err = fabsf(e->scrX - (float)(SCREEN_W / 2));
        if(err <= e->half && e->depth < bd)
        {
            bd = e->depth;
            best = i;
        }
    }
    return best;
}

// Laser beam overlay, drawn after the scene. The columns it touches are
// invalidated so they get repainted (erasing the beam) next frame.
static void DrawLaser(void)
{
    Enemy   *e;
    int16_t mx = SCREEN_W / 2;
    int16_t my = VIEW_H - 1;
    int16_t ex, ey, minX, maxX, c0, c1, c;
    uint32_t core;

    if(g_laserTimer <= 0) return;
    e = &g_enemy[g_laserTarget];
    if(!e->vis) return;

    ex = (int16_t)e->scrX;
    ey = (int16_t)e->scrY;
    if(ex < 4) ex = 4;
    if(ex > SCREEN_W - 5) ex = SCREEN_W - 5;
    if(ey < 4) ey = 4;
    if(ey > VIEW_H - 5) ey = VIEW_H - 5;

    core = (g_frame & 1) ? 0xFFFFFF : 0xFFC0C0;

    Graphics_setForegroundColor(&g_sContext, 0xFF0000);          // red glow
    Graphics_drawLine(&g_sContext, mx - 2, my, ex - 2, ey);
    Graphics_drawLine(&g_sContext, mx + 2, my, ex + 2, ey);
    Graphics_setForegroundColor(&g_sContext, core);              // hot core
    Graphics_drawLine(&g_sContext, mx - 1, my, ex - 1, ey);
    Graphics_drawLine(&g_sContext, mx,     my, ex,     ey);
    Graphics_drawLine(&g_sContext, mx + 1, my, ex + 1, ey);

    FillRect(ex - 3, ey - 3, ex + 3, ey + 3, 0xFFFF80);          // impact spark

    minX = ((mx - 2) < (ex - 4)) ? (mx - 2) : (ex - 4);
    maxX = ((mx + 2) > (ex + 4)) ? (mx + 2) : (ex + 4);
    c0 = minX / COL_W;
    c1 = maxX / COL_W;
    if(c0 < 0) c0 = 0;
    if(c1 > NUM_COLS - 1) c1 = NUM_COLS - 1;
    for(c = c0; c <= c1; c++) g_colTop[c] = -1;
}

// Crosshair: white normally, red when an enemy is under it
static void DrawCrosshair(void)
{
    int16_t  mx = SCREEN_W / 2;
    int16_t  my = VIEW_H / 2;
    int16_t  c, c0 = (mx - 8) / COL_W, c1 = (mx + 7) / COL_W;
    uint32_t color = (CrosshairTarget() >= 0) ? 0xFF3030 : RGB_WHITE;

    FillRect(mx - 8, my - 1, mx + 7, my,     color);
    FillRect(mx - 1, my - 8, mx,     my + 7, color);

    for(c = c0; c <= c1; c++) g_colTop[c] = -1;      // repaint (erase) next frame
}

static void RenderView(void)
{
    int16_t idx    = HeadIdx();
    float dirX     = g_cosTab[idx];
    float dirY     = g_sinTab[idx];
    float planeX   = -dirY * PLANE_LEN;       // camera plane points to the viewer's right
    float planeY   =  dirX * PLANE_LEN;
    int16_t col;

    // 1. Walls (also fills the z-buffer)
    for(col = 0; col < NUM_COLS; col++)
    {
        CastColumn(col, dirX, dirY, planeX, planeY,
                   &g_wTop[col], &g_wBot[col], &g_wKey[col], &g_zbuf[col]);
    }

    // 2. Enemies
    ProjectEnemies(dirX, dirY);

    // 3. Draw columns: sprite columns in one pass, others with delta updates
    for(col = 0; col < NUM_COLS; col++)
    {
        if(!DrawSpriteColumn(col))
            DrawColumn(col, g_wTop[col], g_wBot[col], g_wKey[col]);
    }

    // 4. Laser and crosshair on top
    DrawLaser();
    DrawCrosshair();
}

// ---- Combat -----------------------------------------------------------------
static void KillEnemy(int16_t i)
{
    Enemy *e = &g_enemy[i];

    if(e->state != ES_ALIVE) return;

    e->hp    = 0;
    e->state = ES_DYING;
    e->timer = EXPLODE_FRAMES;
    g_kills++;
    DrawMiniCell(e->cx, e->cy);
}

// Fire the laser at whatever enemy is under the crosshair
static void TryFire(void)
{
    int16_t best;

    if(g_laserTimer > 0 || g_laserCooldown > 0) return;

    best = CrosshairTarget();
    if(best >= 0)
    {
        g_laserTarget = best;
        g_laserTimer  = LASER_FRAMES;
        g_enemy[best].hp--;
    }
}

static void UpdateCombat(void)
{
    int16_t i;

    g_frame++;

    if(g_laserTimer > 0)
    {
        g_laserTimer--;
        if(g_laserTimer == 0)
        {
            g_laserCooldown = LASER_COOLDOWN;
            if(g_enemy[g_laserTarget].hp <= 0) KillEnemy(g_laserTarget);
        }
    }
    else if(g_laserCooldown > 0)
    {
        g_laserCooldown--;
    }

    for(i = 0; i < NUM_ENEMIES; i++)
    {
        if(g_enemy[i].state == ES_DYING)
        {
            if(--g_enemy[i].timer <= 0) g_enemy[i].state = ES_DEAD;
        }
    }
}

// ---- Player movement (joystick) ---------------------------------------------
static int16_t IsWall(float x, float y)
{
    return g_map[(int16_t)y][(int16_t)x] != 0;
}

static void TryMove(float mx, float my)
{
    float nx, ny;

    if(mx != 0.0f)
    {
        nx = g_px + mx;
        if(!IsWall(nx + ((mx > 0.0f) ? PLAYER_RADIUS : -PLAYER_RADIUS), g_py)) g_px = nx;
    }
    if(my != 0.0f)
    {
        ny = g_py + my;
        if(!IsWall(g_px, ny + ((my > 0.0f) ? PLAYER_RADIUS : -PLAYER_RADIUS))) g_py = ny;
    }
}

// Stick left/right = turn, stick up/down = walk. dt is the frame time in seconds.
static void UpdatePlayer(float dt)
{
    int16_t idx;
    float   fwd;

    g_head += ((float)g_joy.dx * (TURN_RATE / 100.0f)) * dt;
    while(g_head >= (float)ANG_STEPS) g_head -= (float)ANG_STEPS;
    while(g_head < 0.0f)              g_head += (float)ANG_STEPS;

    idx = HeadIdx();
    fwd = ((float)(-g_joy.dy) * (MOVE_RATE / 100.0f)) * dt;    // stick up = forward

    TryMove(g_cosTab[idx] * fwd, g_sinTab[idx] * fwd);
}

// ---- Game flow --------------------------------------------------------------
static void NewMaze(void)
{
    int16_t i;

    GenerateMaze();
    PlaceEnemies();

    g_px = 1.5f;
    g_py = 1.5f;
    g_head = (g_map[1][2] == 0) ? 0.0f : 128.0f;     // face an open direction

    g_laserTimer    = 0;
    g_laserCooldown = 0;
    g_laserTarget   = 0;
    g_mazeNum++;
    g_mazeDone      = 0;

    for(i = 0; i < NUM_COLS; i++) g_colTop[i] = -1;   // force full redraw

    DrawHudStatic();
}

static void ShowBanner(void)
{
    uint16_t i;

    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    Graphics_setBackgroundColor(&g_sContext, RGB_BLACK);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"EXIT REACHED!",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, VIEW_H / 2 - 20,
                                OPAQUE_TEXT);

    for(i = 0; i < BANNER_HOLD_MS / 10; i++) DELAY_US(10000);
}

static void Raycaster_Run(void)
{
    uint32_t lastTick;
    uint32_t ticks;
    float    dt;

    g_seed ^= CpuTimer0Regs.TIM.all;                  // different maze every time

    Graphics_setBackgroundColor(&g_sContext, RGB_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCm20b);
    Graphics_clearDisplay(&g_sContext);

    g_mazeNum = 0;
    g_frame   = 0;
    NewMaze();
    lastTick = TimerNow();

    while(1)
    {
        // 1. Input
        Joy_Update();
        if(g_joy.btnLong) return;                     // hold button = menu
        if(g_joy.btnPressed) TryFire();

        // 2. Frame time (seconds), clamped so a slow frame cannot teleport us
        ticks    = lastTick - TimerNow();
        lastTick = TimerNow();
        dt = (float)ticks / (float)(TICKS_PER_MS * 1000UL);
        if(dt > 0.1f) dt = 0.1f;

        // 3. Update
        UpdatePlayer(dt);
        UpdateCombat();

        if(fabsf(g_px - ((float)g_exitX + 0.5f)) < 0.35f &&
           fabsf(g_py - ((float)g_exitY + 0.5f)) < 0.35f)
        {
            g_mazeDone = 1;
        }

        // 4. Draw
        RenderView();
        DrawMiniPlayer();
        DrawHud();

        if(g_mazeDone)
        {
            ShowBanner();
            NewMaze();
            lastTick = TimerNow();
        }
    }
}

//*****************************************************************************
//*****************************************************************************
//  MAIN MENU
//*****************************************************************************
//*****************************************************************************
#define MENU_ITEMS         2
#define MENU_BOX_X0        60
#define MENU_BOX_X1        259
#define MENU_BOX_H         45
#define MENU_FIRST_Y       52
#define MENU_SPACING       60

static const char * const g_menuNames[MENU_ITEMS] = { "TETRIS", "RAYCASTER" };
static const char * const g_menuHints[MENU_ITEMS] = { "Btn: rotate  Up: drop", "Stick: walk  Btn: fire" };
static const uint32_t     g_menuColors[MENU_ITEMS] = { 0xA000FF, 0xFF8000 };

static void Menu_DrawItem(int16_t i, int16_t selected)
{
    Graphics_Rectangle box;
    int16_t y = MENU_FIRST_Y + i * MENU_SPACING;

    box.xMin = MENU_BOX_X0; box.xMax = MENU_BOX_X1;
    box.yMin = y;           box.yMax = y + MENU_BOX_H - 1;

    if(selected)
    {
        FillRect(box.xMin, box.yMin, box.xMax, box.yMax, g_menuColors[i]);
        Graphics_setForegroundColor(&g_sContext, RGB_BLACK);
    }
    else
    {
        FillRect(box.xMin, box.yMin, box.xMax, box.yMax, RGB_BLACK);
        Graphics_setForegroundColor(&g_sContext, RGB_GRAY);
        Graphics_drawRectangle(&g_sContext, &box);
        Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    }

    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_menuNames[i], AUTO_STRING_LENGTH,
                                (MENU_BOX_X0 + MENU_BOX_X1) / 2, y + MENU_BOX_H / 2,
                                TRANSPARENT_TEXT);
}

static void Menu_DrawAll(int16_t sel, int16_t full)
{
    int16_t i;

    if(full)
    {
        Graphics_setBackgroundColor(&g_sContext, RGB_BLACK);
        Graphics_setFont(&g_sContext, &g_sFontCm20b);
        Graphics_clearDisplay(&g_sContext);

        Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
        Graphics_drawStringCentered(&g_sContext, (int16_t *)"SELECT GAME",
                                    AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);
        Graphics_drawLine(&g_sContext, 10, 30, 309, 30);
    }

    for(i = 0; i < MENU_ITEMS; i++) Menu_DrawItem(i, (i == sel));

    // Hint lines
    FillRect(0, 176, SCREEN_W - 1, 239, RGB_BLACK);
    Graphics_setForegroundColor(&g_sContext, RGB_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_menuHints[sel], AUTO_STRING_LENGTH,
                                159, 186, TRANSPARENT_TEXT);
    Graphics_setForegroundColor(&g_sContext, RGB_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu", AUTO_STRING_LENGTH,
                                159, 214, TRANSPARENT_TEXT);
}

// Returns the chosen game index
static int16_t Menu_Run(void)
{
    static int16_t sel = 0;           // remembers the last choice

    Menu_DrawAll(sel, 1);

    while(1)
    {
        Joy_Update();

        if(g_joy.edgeY != 0)
        {
            sel += g_joy.edgeY;                       // +1 = stick down
            if(sel < 0) sel = MENU_ITEMS - 1;
            if(sel >= MENU_ITEMS) sel = 0;
            Menu_DrawAll(sel, 0);
        }

        if(g_joy.btnPressed) return sel;

        DELAY_US(15000);
    }
}

//*****************************************************************************
// main
//*****************************************************************************
void main(void)
{
#ifdef _RELEASE
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart,
           (Uint32)&RamfuncsLoadSize);
#endif

    int16_t choice;

    // Step 1. Initialize system control and default GPIOs
    InitSysCtrl();
    InitGpio();

    DINT;
    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    InitPieVectTable();

    // Step 2. Joystick switch + ADC + timer, then stick calibration.
    //         Keep the stick centered right now. Returns 0 if it looked wrong.
    Joystick_Init();

    // Step 2b. LCD
    Kitronix320x240x16_SSD2119Init();
    Graphics_initContext(&g_sContext, &g_sKitronix320x240x16_SSD2119);

    BuildTrigTables();

    // Step 3. Menu -> game -> menu ...
    while(1)
    {
        choice = Menu_Run();
        Joy_WaitRelease();

        if(choice == 0) Tetris_Run();
        else            Raycaster_Run();

        Joy_WaitRelease();
    }
}
