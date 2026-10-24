//#############################################################################
// FILE:   app/raycaster.c
// TITLE:  RAYCASTER - first-person arena roguelike for the F28069 + SSD2119
//
// HOW IT IS BUILT FOR THIS HARDWARE (C28x @ 90 MHz, SPI LCD, no frame buffer)
//
//  * The LCD is the bottleneck, not the CPU. So the 3D view is drawn as
//    RC_COLS vertical strips, and each strip is DIFFED against what is already
//    on the screen: only the pixels whose wall height / shade changed are
//    sent. A still camera costs almost nothing.
//  * Everything is integer / table driven. No float after init:
//        - sin/cos        : 1024-entry Q14 table
//        - 1/|cos|,1/|sin|: 1024-entry Q8 tables  (the DDA step lengths)
//        - atan           : 257-entry table (for sprite projection)
//        - fisheye fix    : per-column cos table
//    The only divisions per frame are one per column (wall height) and a few
//    per enemy.
//  * int is 16 bits on C28x: every product that can exceed 32767 is cast to
//    int32_t first. There is no uint8_t on this compiler, so none is used.
//  * Positions are Q10 (1024 = 1 tile) in int16_t. Ray distances are Q8.
//  * Optional: define RC_USE_RAMFUNCS (project predefined symbols) to place the
//    ray caster in the "ramfuncs" section so it runs with zero wait states.
//    Only do this if your linker .cmd places ramfuncs (flash load, RAM run).
//  * Enemies are 16x16 pixel-art sprites turned into per-column colour runs at
//    init, then drawn as a few FillRects per screen column, z-tested against
//    the wall distance of that column.
//
// APP CONTRACT: update() = logic only, render() = drawing only, one public
// symbol (g_raycasterApp). The runner already calls Joy_Update()/Dpad_Update().
//#############################################################################

#include "raycaster.h"
#include <math.h>

/* Performance configuration: 40 strips x 8 pixels = 320 pixels.
 * This halves ray-column work and reduces sprite LCD writes. */
#undef RC_COLS
#undef RC_COL_W
#define RC_COLS  40
#define RC_COL_W 8

/* Corridor Run is the looping background theme declared with the other songs. */
extern const Song song_corridor_run;

//*****************************************************************************
// Constants
//*****************************************************************************
#define RC_ANGLES       1024
#define RC_ANGLE_MASK   1023
#define RC_MAP_SHIFT    4
#define RC_MAP_CELLS    (RC_MAP_W * RC_MAP_H)

#define RC_VIEW_W       (RC_COLS * RC_COL_W)
#define RC_VIEW_X0      ((SCREEN_W - RC_VIEW_W) / 2)
#define RC_VIEW_Y1      (RC_VIEW_Y0 + RC_VIEW_H - 1)
#define RC_HORIZON      (RC_VIEW_H / 2)                 // row (inside view) of the horizon
#define RC_PROJ         ((uint16_t)(RC_VIEW_H * 256UL)) // wall is full height at 1 tile
#define RC_MIN_DIST     40                              // Q8, stops divide blow-up
#define RC_MAX_STEPS    28
#define RC_PX_PER_UNIT  ((RC_COLS * RC_COL_W) / RC_FOV_UNITS)

#define COL_CEIL        0x17121EUL
#define COL_FLOOR       0x2E261AUL
#define C_BLACK         0x000000UL
#define C_WHITE         0xFFFFFFUL
#define C_GRAY          0x909090UL
#define C_DKGRAY        0x404040UL
#define C_RED           0xE02020UL
#define C_GOLD          0xFFD040UL
#define C_GREEN         0x40FF40UL
#define C_CYAN          0x40E0FFUL
#define C_BAD           0xFF6060UL

// Game modes
#define MODE_INTRO      0               // "ROOM n" banner, world frozen
#define MODE_PLAY       1
#define MODE_SHOP       2
#define MODE_CLEAR      3               // "ROOM CLEARED" banner
#define MODE_OVER       4
#define MODE_WIN        5

// Banners
#define BAN_LEVEL       1
#define BAN_CLEAR       2

// HUD dirty bits
#define HUD_HEARTS      0x01
#define HUD_GOLD        0x02
#define HUD_LEVEL       0x04
#define HUD_LEFT        0x08
#define HUD_STAFF       0x10
#define HUD_ALL         0x1F

#define RC_BOSS_HP      140

/* Frame cap. Reuses the free-running 32-bit down counter that joystick.c
 * already starts (CpuTimer0): TimerNow() and TICKS_PER_MS come from drivers.h. */
#define RC_TARGET_FPS   30UL
#define RC_FRAME_TICKS  (((uint32_t)TICKS_PER_MS * 1000UL) / RC_TARGET_FPS)

//*****************************************************************************
// Enemies
//*****************************************************************************
enum { ET_ZOMBIE, ET_BAT, ET_BRUTE, ET_BOSS, ET_COUNT };
enum { SH_ZOMBIE, SH_BAT, SH_LICH, SH_COUNT };

typedef struct
{
    int16_t  hp;
    int16_t  speed;         // Q10 units per frame
    int16_t  gold;
    int16_t  radius;        // how fat the beam hit-test thinks it is (Q10)
    int16_t  reach;         // contact distance that hurts the player (Q10)
    uint16_t scale;         // sprite height, Q4 fraction of wall height (16 = 1.0)
    uint16_t zoff;          // sprite lift, Q4 fraction of wall height
    uint16_t shape;
} EnemyDef;

static const EnemyDef kEnemy[ET_COUNT] =
{
    //  hp  spd gold  rad reach scale zoff shape
    {    6,  26,   8, 330,  540,  12,   0, SH_ZOMBIE },   // zombie
    {    3,  44,   6, 260,  480,   8,   5, SH_BAT    },   // bat (fast, flies)
    {   18,  17,  25, 440,  620,  17,   0, SH_ZOMBIE },   // brute (recoloured zombie)
    {  RC_BOSS_HP, 22, 200, 620, 800, 26, 0, SH_LICH },   // boss
};

typedef struct
{
    int16_t  x, y;          // Q10
    int16_t  hp;
    uint16_t type;
    uint16_t flash;         // frames of white hit-flash left
    uint16_t slow;          // frames of frost left
    uint16_t aux;           // boss charge timer
    uint16_t alive;
} Enemy;

//*****************************************************************************
// Staffs, traits, stats
//*****************************************************************************
#define NUM_STAFF   5

typedef struct
{
    const char *name;
    uint16_t    dmg;
    uint16_t    cooldown;   // frames between shots
    uint16_t    beams;
    uint16_t    spread;     // angle units between neighbouring beams
    uint16_t    pierce;     // extra enemies a beam passes through (99 = all)
    uint32_t    color;
} Staff;

static const Staff kStaff[NUM_STAFF] =
{
    { "Ray Wand",    3, 16, 1,  0,  0, 0x40E0FFUL },
    { "Prism Staff", 3, 22, 3, 16,  0, 0xFF60FFUL },
    { "Lance",       5, 30, 1,  0, 99, 0xFFFF60UL },
    { "Storm Rod",   2,  8, 1,  0,  0, 0xA0A0FFUL },
    { "Sunfire",     4, 34, 5, 14,  1, 0xFF9030UL },
};
static const uint16_t kStaffCost[NUM_STAFF] = { 0, 70, 130, 110, 260 };

enum { TR_VAMP, TR_PIERCE, TR_THICK, TR_KNOCK, TR_LUCKY, TR_FROST, TR_COUNT };
static const char *const kTraitName[TR_COUNT] =
    { "Vampire", "Piercing", "Thick skin", "Knockback", "Lucky", "Frost" };
static const uint16_t kTraitCost[TR_COUNT] = { 120, 140, 90, 80, 110, 100 };

enum { ST_DMG, ST_RATE, ST_SPEED, ST_HEART, ST_HEAL, ST_GOLD, ST_COUNT };
static const char *const kStatName[ST_COUNT] =
    { "Damage", "Fire rate", "Move speed", "Max heart", "Heal", "Gold find" };

//*****************************************************************************
// Levels. map = index into kMaps. pBat / pBrute are percent chances.
//*****************************************************************************
typedef struct
{
    uint16_t map, total, maxAlive, gap, pBat, pBrute, boss;
} LevelDef;

static const LevelDef kLevel[RC_NUM_LEVELS] =
{
    { 0,  8, 4, 60,  0,  0, 0 },
    { 1, 12, 5, 50, 25,  0, 0 },
    { 2, 16, 6, 45, 25, 15, 0 },
    { 3, 20, 6, 40, 30, 25, 0 },
    { 4, 26, 7, 36, 30, 30, 0 },
    { 5,  0, 5, 150, 30,  0, 1 },       // boss room: boss + endless adds
};

//*****************************************************************************
// Maps (16 x 16).  # stone   P pillar   B brick   . floor
//                  e enemy spawn marker   @ player start
//*****************************************************************************
static const char *const kMaps[RC_NUM_LEVELS][RC_MAP_H] =
{
    {   // 0 - Entry Hall
        "################",
        "#e............e#",
        "#..............#",
        "#...P......P...#",
        "#..............#",
        "#..............#",
        "#...P......P...#",
        "#..............#",
        "#......@.......#",
        "#..............#",
        "#...P......P...#",
        "#..............#",
        "#..............#",
        "#...P......P...#",
        "#e............e#",
        "################"
    },
    {   // 1 - Notched Court
        "################",
        "##e..........e##",
        "#..............#",
        "#....##..##....#",
        "#....#....#....#",
        "#..............#",
        "#..............#",
        "#.......@......#",
        "#..............#",
        "#..............#",
        "#....#....#....#",
        "#....##..##....#",
        "#..............#",
        "#..............#",
        "##e..........e##",
        "################"
    },
    {   // 2 - Cloister
        "################",
        "#e............e#",
        "#.####....####.#",
        "#.#..........#.#",
        "#.#...P..P...#.#",
        "#..............#",
        "#..............#",
        "#.......@......#",
        "#..............#",
        "#..............#",
        "#.#...P..P...#.#",
        "#.#..........#.#",
        "#.####....####.#",
        "#..............#",
        "#e............e#",
        "################"
    },
    {   // 3 - Crypt
        "################",
        "#e............e#",
        "#..P.....P.....#",
        "#.....BB.......#",
        "#.P....BB..P...#",
        "#..............#",
        "#.....P....P...#",
        "#BB..........BB#",
        "#BB....@.....BB#",
        "#..............#",
        "#...P....P.....#",
        "#..............#",
        "#...P..BB..P...#",
        "#..............#",
        "#e....P..P....e#",
        "################"
    },
    {   // 4 - The Ring
        "################",
        "#e............e#",
        "#..............#",
        "#..PP......PP..#",
        "#..PP......PP..#",
        "#......@.......#",
        "#.....####.....#",
        "#.....####.....#",
        "#.....####.....#",
        "#.....####.....#",
        "#..............#",
        "#..PP......PP..#",
        "#..PP......PP..#",
        "#..............#",
        "#e............e#",
        "################"
    },
    {   // 5 - Boss arena
        "################",
        "##....e..e....##",
        "#..............#",
        "#...P......P...#",
        "#..............#",
        "#..............#",
        "#......BB......#",
        "#..............#",
        "#..............#",
        "#..............#",
        "#...P......P...#",
        "#..............#",
        "#..............#",
        "#......@.......#",
        "##............##",
        "################"
    },
};

//*****************************************************************************
// Sprite art (16 x 16).  '.' = transparent, lower-case letters = palette.
//*****************************************************************************
#define SPR_N       16
#define SPR_MAXRUNS 6

static const char *const kArtZombie[SPR_N] =
{
    "................",
    ".....dddddd.....",
    "....dggggggd....",
    "....dgrggrgd....",
    "....dggggggd....",
    ".....dgkkgd.....",
    "......dddd......",
    "..dd.kkkkkk.dd..",
    ".dggdkkkkkkdggd.",
    ".dggdkkkkkkdggd.",
    ".dg..kkkkkk..gd.",
    ".....kkkkkk.....",
    ".....kk..kk.....",
    ".....kk..kk.....",
    "....kkk..kkk....",
    "................"
};

static const char *const kArtBat[SPR_N] =
{
    "................",
    "................",
    ".pp..........pp.",
    ".ppp...pp...ppp.",
    ".pppp.pppp.pppp.",
    "ppppppprrppppppp",
    ".pvvppppppppvvp.",
    "..pv.pppppp.vp..",
    "...p..pwwp..p...",
    "......pppp......",
    ".......pp.......",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArtLich[SPR_N] =
{
    "....y.y..y.y....",
    "....yyyyyyyy....",
    "....wwwwwwww....",
    "....wrrwwrrw....",
    "....wwwwwwww....",
    ".....wwkkww.....",
    "...vvvppppvvv...",
    "..vppppppppppv..",
    ".vppppppppppppv.",
    ".vppwppppppwppv.",
    ".vppppppppppppv.",
    "..vppppppppppv..",
    "..vppppppppppv..",
    "...vppppppppv...",
    "...vvvppppvvv...",
    "....vvvvvvvv...."
};

static const char *const *const kArt[SH_COUNT] = { kArtZombie, kArtBat, kArtLich };

typedef struct { char ch; uint32_t rgb; } PalEnt;      // list ends with ch = 0

static const PalEnt kPalZombie[] =
    { {'g',COLOR_GREEN},{'d',0x205828UL},{'r',0xFF3030UL},{'k',0x383868UL},{0,0} };
static const PalEnt kPalBrute[]  =
    { {'g',0xC06030UL},{'d',0x703010UL},{'r',0xFFE020UL},{'k',0x402828UL},{0,0} };
static const PalEnt kPalBat[]    =
    { {'p',0x8848C8UL},{'v',0x502888UL},{'r',0xFF4040UL},{'w',0xFFFFFFUL},{0,0} };
static const PalEnt kPalBoss[]   =
    { {'p',0x6038B0UL},{'v',0x34206CUL},{'w',0xE8E8D8UL},{'r',0x40FFFFUL},
      {'y',0xFFD020UL},{'k',0x101010UL},{0,0} };

static const uint32_t kWallBase[4] = { 0, 0x8088A0UL, 0xB09060UL, 0xA05040UL };
static const uint16_t kShadeMul[4] = { 256, 190, 130, 84 };

//*****************************************************************************
// Tables (built once, in RAM)
//*****************************************************************************
static int16_t  s_sin[RC_ANGLES];               // Q14. cos(a) = s_sin[a + 256]
static uint16_t s_dx[RC_ANGLES];                // 256/|cos| (Q8, clamped)
static uint16_t s_dy[RC_ANGLES];                // 256/|sin|
static int16_t  s_colOff[RC_COLS];              // ray angle offset per column
static int16_t  s_colCos[RC_COLS];              // Q14 fisheye correction
static int16_t  s_atan[257];                    // atan(i/256) in angle units
static uint32_t s_wallCol[4][4][2];             // [tile][shade][side]
static uint32_t s_pal[ET_COUNT][26];            // [type][letter - 'a']
static int16_t  s_runY0[SH_COUNT][SPR_N][SPR_MAXRUNS];
static int16_t  s_runY1[SH_COUNT][SPR_N][SPR_MAXRUNS];
static int16_t  s_runCi[SH_COUNT][SPR_N][SPR_MAXRUNS];
static uint16_t s_runCnt[SH_COUNT][SPR_N];
static bool     s_tablesReady = false;

#define SIN(a)  s_sin[(uint16_t)(a) & RC_ANGLE_MASK]
#define COS(a)  s_sin[((uint16_t)(a) + 256u) & RC_ANGLE_MASK]

//*****************************************************************************
// State
//*****************************************************************************
// Player
static int16_t  s_px, s_py;                     // Q10
static int16_t  s_ang;                          // 0..1023
static uint16_t s_hp, s_maxHp, s_gold;
static uint16_t s_invuln, s_cool, s_kills;
static uint16_t s_lvDmg, s_lvRate, s_lvSpeed, s_lvGold;
static uint16_t s_traits, s_staffOwned, s_staff;

// Level
static uint16_t s_level;
static uint16_t s_tile[RC_MAP_CELLS];
static int16_t  s_spX[RC_MAX_SPAWNS], s_spY[RC_MAX_SPAWNS];
static uint16_t s_spCount;
static Enemy    s_en[RC_MAX_ENEMIES];
static uint16_t s_alive, s_toSpawn, s_spawnTimer, s_spawnIdx;
static uint16_t s_bossSpawned, s_bossAlive;

// Flow
static uint16_t s_mode, s_timer;
static bool     s_shopNext;                     // shop was opened by a cleared room
static int16_t  s_stX, s_stY;                   // stick flick state

// Effects
static uint16_t s_fxTimer, s_fxN, s_hurtFx;
static int16_t  s_fxX[5];
static uint32_t s_fxColor;

// Draw state
static bool     s_dirtyFull, s_invalidate, s_bannerDraw;
static uint16_t s_hud, s_banner, s_viewDirty;
static int16_t  s_cTop[RC_COLS], s_cBot[RC_COLS];       // what is on the LCD now
static uint32_t s_cCol[RC_COLS];
static int16_t  s_rngT[RC_COLS], s_rngB[RC_COLS];       // overlay rows drawn this frame (T > B = none)
static int16_t  s_prvT[RC_COLS], s_prvB[RC_COLS];       // overlay rows drawn last frame

// Shop
#define TAB_STATS   0
#define TAB_TRAITS  1
#define TAB_STAFFS  2
#define TAB_COUNT   3
#define SD_TABS     0x040
#define SD_GOLD     0x080
#define SD_MSG      0x100
#define SD_ALL      0x1FF
#define SHOP_Y0     70
#define SHOP_DY     23

static uint16_t s_shopTab, s_shopRow, s_shopDirty, s_shopMsg;
static const uint16_t kTabRows[TAB_COUNT] = { ST_COUNT, TR_COUNT, NUM_STAFF };
static const char *const kTabName[TAB_COUNT] = { "STATS", "TRAITS", "STAFFS" };
static const char *const kMsgText[5] =
    { "", "Bought!", "Not enough gold", "Already maxed", "Equipped" };

// Sprite draw list
typedef struct
{
    int16_t  x0;            // left edge in screen pixels
    int16_t  top;           // top row inside the view (can be negative)
    uint16_t size;          // sprite height/width in pixels
    uint16_t c0, c1;        // first / last screen column it touches
    uint16_t dist;          // forward distance, Q8
    uint16_t uStep;         // 16/size in Q8
    uint16_t shape, type, flash;
} Spr;

static Spr      s_spr[RC_MAX_ENEMIES];
static uint16_t s_nSpr;

//*****************************************************************************
// Small helpers
//*****************************************************************************
static int16_t Abs16(int16_t v)
{
    return (v < 0) ? -v : v;
}

// Cheap distance: max + 0.375 * min (error under 4 percent). Q10 in, Q10 out.
static int16_t Dist10(int16_t dx, int16_t dy)
{
    int16_t a = Abs16(dx);
    int16_t b = Abs16(dy);

    if(a < b) { int16_t t = a; a = b; b = t; }
    return a + (b >> 2) + (b >> 3);
}

static int16_t StickEdge(int16_t v, int16_t *state)
{
    int16_t target = *state;

    if(v >= RC_EDGE_ON)                                target = 1;
    else if(v <= -RC_EDGE_ON)                          target = -1;
    else if(v < RC_EDGE_OFF && v > -RC_EDGE_OFF)       target = 0;

    if(target != *state)
    {
        *state = target;
        return target;
    }
    return 0;
}

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

static int16_t Cat(char *dst, int16_t n, const char *s, int16_t maxLen)
{
    while(*s && maxLen-- > 0)
        dst[n++] = *s++;
    dst[n] = 0;
    return n;
}

static int16_t TextLength(const char *s)
{
    int16_t n = 0;
    while(s[n] != 0) n++;
    return n;
}

static void DrawText(const char *s, int16_t x, int16_t y, uint32_t color)
{
    int16_t len;

    if(s == NULL) return;
    len = TextLength(s);
    if(len == 0) return;

    Graphics_setForegroundColor(&g_sContext, color);
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, len,
                                x, y, TRANSPARENT_TEXT);
}

static void Sfx(const Song *s)
{
    Buzzer_PlaySfx(s);
}

// Low-priority effect: never cuts off whatever is already playing.
static void SfxSoft(const Song *s)
{
    if(!Buzzer_IsPlaying()) Buzzer_PlaySfx(s);
}

//*****************************************************************************
// Init-time table builders
//*****************************************************************************
static uint32_t ScaleRgb(uint32_t rgb, uint16_t m)      // m is Q8
{
    uint32_t r = (((rgb >> 16) & 0xFFUL) * m) >> 8;
    uint32_t g = (((rgb >>  8) & 0xFFUL) * m) >> 8;
    uint32_t b = (( rgb        & 0xFFUL) * m) >> 8;
    return (r << 16) | (g << 8) | b;
}

static char ArtAt(const char *row, uint16_t x)
{
    uint16_t i;

    for(i = 0; i < x; i++)
        if(row[i] == 0) return '.';             // short row: rest is transparent
    return row[x] ? row[x] : '.';
}

static void SetPal(uint16_t type, const PalEnt *list)
{
    uint16_t i;

    for(i = 0; i < 26; i++) s_pal[type][i] = 0xFF00FFUL;    // magenta = missing
    for(i = 0; list[i].ch; i++)
        s_pal[type][list[i].ch - 'a'] = list[i].rgb;
}

static void BuildTables(void)
{
    uint16_t i, c, sh, x, y, n;
    float    s, co, f;

    if(s_tablesReady) return;

    for(i = 0; i < RC_ANGLES; i++)
    {
        s  = sinf((float)i * (6.2831853f / RC_ANGLES));
        co = cosf((float)i * (6.2831853f / RC_ANGLES));

        s_sin[i] = (int16_t)(s * 16384.0f + ((s >= 0.0f) ? 0.5f : -0.5f));

        f = (fabsf(co) < 0.0001f) ? 65535.0f : 256.0f / fabsf(co);
        s_dx[i] = (f > 65535.0f) ? 65535u : (uint16_t)f;

        f = (fabsf(s) < 0.0001f) ? 65535.0f : 256.0f / fabsf(s);
        s_dy[i] = (f > 65535.0f) ? 65535u : (uint16_t)f;
    }

    for(c = 0; c < RC_COLS; c++)
    {
        int16_t off = (int16_t)(((int32_t)(2 * (int16_t)c - (RC_COLS - 1)) * RC_FOV_UNITS)
                                / (2 * RC_COLS));
        s_colOff[c] = off;
        s_colCos[c] = COS(off);
    }

    for(i = 0; i <= 256; i++)
        s_atan[i] = (int16_t)(atanf((float)i / 256.0f) * (RC_ANGLES / 6.2831853f) + 0.5f);

    // Wall colours: tile 1..3, 4 distance shades, 2 sides (side 1 is darker).
    for(i = 1; i < 4; i++)
        for(sh = 0; sh < 4; sh++)
        {
            s_wallCol[i][sh][0] = ScaleRgb(kWallBase[i], kShadeMul[sh]);
            s_wallCol[i][sh][1] = ScaleRgb(s_wallCol[i][sh][0], 200);
        }

    SetPal(ET_ZOMBIE, kPalZombie);
    SetPal(ET_BAT,    kPalBat);
    SetPal(ET_BRUTE,  kPalBrute);
    SetPal(ET_BOSS,   kPalBoss);

    // Sprite art -> per-column vertical runs.
    for(sh = 0; sh < SH_COUNT; sh++)
        for(x = 0; x < SPR_N; x++)
        {
            n = 0;
            y = 0;
            while(y < SPR_N)
            {
                char ch = ArtAt(kArt[sh][y], x);

                if(ch == '.') { y++; continue; }

                if(n < SPR_MAXRUNS)
                {
                    uint16_t y1 = y;
                    while(y1 + 1 < SPR_N && ArtAt(kArt[sh][y1 + 1], x) == ch) y1++;
                    s_runY0[sh][x][n] = (int16_t)y;
                    s_runY1[sh][x][n] = (int16_t)y1;
                    s_runCi[sh][x][n] = (int16_t)(ch - 'a');
                    n++;
                    y = y1 + 1;
                }
                else
                    break;
            }
            s_runCnt[sh][x] = n;
        }

    s_tablesReady = true;
}

//*****************************************************************************
// Map + collision
//*****************************************************************************
static bool Solid(int16_t tx, int16_t ty)
{
    if((uint16_t)tx >= RC_MAP_W || (uint16_t)ty >= RC_MAP_H) return true;
    return s_tile[((uint16_t)ty << RC_MAP_SHIFT) + (uint16_t)tx] != 0;
}

// Is a box of half-size r (all Q10) centered on (x, y) touching a wall?
static bool Blocked(int16_t x, int16_t y, int16_t r)
{
    int16_t x0 = (int16_t)((x - r) >> 10);
    int16_t x1 = (int16_t)((x + r) >> 10);
    int16_t y0 = (int16_t)((y - r) >> 10);
    int16_t y1 = (int16_t)((y + r) >> 10);

    return Solid(x0, y0) || Solid(x1, y0) || Solid(x0, y1) || Solid(x1, y1);
}

static void LoadMap(uint16_t m)
{
    uint16_t x, y, t;

    s_spCount = 0;

    for(y = 0; y < RC_MAP_H; y++)
        for(x = 0; x < RC_MAP_W; x++)
        {
            char ch = kMaps[m][y][x];

            t = 0;
            switch(ch)
            {
                case '#': t = 1; break;
                case 'P': t = 2; break;
                case 'B': t = 3; break;
                case 'e':
                    if(s_spCount < RC_MAX_SPAWNS)
                    {
                        s_spX[s_spCount] = (int16_t)((x << 10) + 512);
                        s_spY[s_spCount] = (int16_t)((y << 10) + 512);
                        s_spCount++;
                    }
                    break;
                case '@':
                    s_px = (int16_t)((x << 10) + 512);
                    s_py = (int16_t)((y << 10) + 512);
                    break;
                default: break;
            }
            s_tile[(y << RC_MAP_SHIFT) + x] = t;
        }
}

//*****************************************************************************
// The ray caster: grid DDA, all integer.
//   angle : 0..1023
//   returns the distance ALONG the ray in Q8 (not yet fisheye-corrected)
//   *tile = wall type hit, *side = 0 for a x-face, 1 for a y-face
// Start point comes from the player position.
//*****************************************************************************
#ifdef RC_USE_RAMFUNCS      // opt-in: needs a ramfuncs entry in your linker .cmd
#pragma CODE_SECTION(RC_CastRay, "ramfuncs");
#endif
static uint16_t RC_CastRay(int16_t angle, uint16_t *tile, uint16_t *side)
{
    uint16_t a     = (uint16_t)angle & RC_ANGLE_MASK;
    int16_t  c     = COS(a);
    int16_t  s     = SIN(a);
    uint16_t dX    = s_dx[a];
    uint16_t dY    = s_dy[a];
    int16_t  mx    = (int16_t)(s_px >> 10);
    int16_t  my    = (int16_t)(s_py >> 10);
    uint16_t fx    = (uint16_t)(s_px >> 2) & 255u;         // fraction inside tile, Q8
    uint16_t fy    = (uint16_t)(s_py >> 2) & 255u;
    int16_t  stepX, stepY;
    uint32_t sideX, sideY, dist = 0;
    uint16_t hit = 1, sd = 0, i;

    if(c < 0) { stepX = -1; sideX = ((uint32_t)fx * dX) >> 8; }
    else      { stepX =  1; sideX = ((uint32_t)(256u - fx) * dX) >> 8; }

    if(s < 0) { stepY = -1; sideY = ((uint32_t)fy * dY) >> 8; }
    else      { stepY =  1; sideY = ((uint32_t)(256u - fy) * dY) >> 8; }

    for(i = 0; i < RC_MAX_STEPS; i++)
    {
        if(sideX < sideY) { dist = sideX; sideX += dX; mx += stepX; sd = 0; }
        else              { dist = sideY; sideY += dY; my += stepY; sd = 1; }

        if((uint16_t)mx >= RC_MAP_W || (uint16_t)my >= RC_MAP_H) { hit = 1; break; }

        hit = s_tile[((uint16_t)my << RC_MAP_SHIFT) + (uint16_t)mx];
        if(hit) break;
    }

    *tile = hit & 3u;
    if(*tile == 0) *tile = 1;
    *side = sd;
    return (dist > 65535UL) ? 65535u : (uint16_t)dist;
}

//*****************************************************************************
// Game: flow, player, enemies
//*****************************************************************************
static void EnterOver(void);
static void EnterWin(void);

static uint16_t Cooldown(void)
{
    int16_t cd = (int16_t)((kStaff[s_staff].cooldown * (100 - 9 * s_lvRate)) / 100);
    return (cd < 4) ? 4 : (uint16_t)cd;
}

static void HurtPlayer(void)
{
    if(s_invuln || s_hp == 0) return;

    s_hp--;
    s_invuln = (s_traits & (1u << TR_THICK)) ? RC_THICK_FRAMES : RC_INVULN_FRAMES;
    s_hurtFx = 8;
    s_hud   |= HUD_HEARTS;
    s_viewDirty = 2;

    if(s_hp == 0) EnterOver();
    else          Sfx(&sfx_hit);
}

static void KillEnemy(Enemy *e)
{
    const EnemyDef *d = &kEnemy[e->type];
    uint16_t i, g;

    e->alive = 0;
    if(s_alive) s_alive--;

    g = (uint16_t)(((uint32_t)d->gold * (100u + 20u * s_lvGold)) / 100u);
    s_gold += g;
    s_kills++;
    s_hud |= HUD_GOLD | HUD_LEFT;

    if((s_traits & (1u << TR_VAMP)) && (s_kills % 10u) == 0 && s_hp < s_maxHp)
    {
        s_hp++;
        s_hud |= HUD_HEARTS;
    }

    Sfx(&sfx_explosion);

    if(e->type == ET_BOSS)
    {
        s_bossAlive = 0;
        for(i = 0; i < RC_MAX_ENEMIES; i++)         // adds die with their master
            s_en[i].alive = 0;
        s_alive = 0;
    }
}

static void DamageEnemy(Enemy *e, uint16_t dmg, int16_t c, int16_t s)
{
    e->hp   -= (int16_t)dmg;
    e->flash = 3;
    s_hud   |= HUD_LEFT;

    if(e->hp <= 0)
    {
        KillEnemy(e);
        return;
    }

    if((s_traits & (1u << TR_KNOCK)) && e->type != ET_BOSS)
    {
        int16_t nx = e->x + (int16_t)(((int32_t)c * 260L) >> 14);
        int16_t ny = e->y + (int16_t)(((int32_t)s * 260L) >> 14);

        if(!Blocked(nx, e->y, 220)) e->x = nx;
        if(!Blocked(e->x, ny, 220)) e->y = ny;
    }

    if(s_traits & (1u << TR_FROST)) e->slow = 90;

    SfxSoft(&sfx_blip);
}

// One hitscan beam along `angle`.
static void FireBeam(int16_t angle)
{
    uint16_t tile, side;
    int16_t  c, s, best;
    int32_t  wall, bestAlong, along, perp;
    uint16_t hitMask = 0, left, i, dmg;
    int16_t  rx, ry;

    angle &= RC_ANGLE_MASK;
    wall = (int32_t)RC_CastRay(angle, &tile, &side) << 2;       // Q8 -> Q10
    c = COS(angle);
    s = SIN(angle);

    left = 1u + kStaff[s_staff].pierce + ((s_traits & (1u << TR_PIERCE)) ? 1u : 0u);

    while(left--)
    {
        best      = -1;
        bestAlong = wall;

        for(i = 0; i < RC_MAX_ENEMIES; i++)
        {
            if(!s_en[i].alive || (hitMask & (1u << i))) continue;

            rx    = s_en[i].x - s_px;
            ry    = s_en[i].y - s_py;
            along = ((int32_t)rx * c + (int32_t)ry * s) >> 14;
            if(along <= 0 || along >= bestAlong) continue;

            perp = ((int32_t)ry * c - (int32_t)rx * s) >> 14;
            if(perp < 0) perp = -perp;
            if(perp > kEnemy[s_en[i].type].radius) continue;

            best      = (int16_t)i;
            bestAlong = along;
        }

        if(best < 0) break;

        hitMask |= (1u << best);

        dmg = (uint16_t)((kStaff[s_staff].dmg * (4u + s_lvDmg) + 2u) >> 2);
        if((s_traits & (1u << TR_LUCKY)) && (Rand16() % 5u) == 0) dmg *= 2u;

        DamageEnemy(&s_en[best], dmg, c, s);
    }
}

static void Fire(void)
{
    const Staff *st = &kStaff[s_staff];
    uint16_t i;
    int16_t  off;

    s_cool    = Cooldown();
    s_fxN     = st->beams;
    s_fxColor = st->color;

    for(i = 0; i < st->beams; i++)
    {
        off = (int16_t)((((int16_t)(2 * i) - (int16_t)(st->beams - 1)) * (int16_t)st->spread) / 2);
        FireBeam(s_ang + off);
        s_fxX[i] = (int16_t)(RC_VIEW_X0 + RC_VIEW_W / 2 + off * RC_PX_PER_UNIT);
    }

    s_fxTimer   = 3;
    s_viewDirty = 2;
    SfxSoft(&sfx_laser);
}

static void SpawnEnemy(uint16_t type, int16_t x, int16_t y)
{
    uint16_t i;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        if(!s_en[i].alive)
        {
            int16_t hp = kEnemy[type].hp;

            if(type != ET_BOSS)                         // later rooms are tougher
                hp = (int16_t)(((int32_t)hp * (8 + s_level)) / 8);

            s_en[i].x = x;      s_en[i].y = y;
            s_en[i].hp = hp;    s_en[i].type = type;
            s_en[i].flash = 0;  s_en[i].slow = 0;
            s_en[i].aux = 0;    s_en[i].alive = 1;
            s_alive++;
            s_hud |= HUD_LEFT;
            return;
        }
    }
}

static void TrySpawn(void)
{
    const LevelDef *lv = &kLevel[s_level];
    uint16_t tries, idx = 0, type, r, gap;

    if(s_spawnTimer) { s_spawnTimer--; return; }
    if(!s_toSpawn || s_alive >= lv->maxAlive || s_spCount == 0) return;

    // First marker that is not right on top of the player.
    for(tries = 0; tries < s_spCount; tries++)
    {
        idx = s_spawnIdx;
        s_spawnIdx = (s_spawnIdx + 1u) % s_spCount;
        if(Dist10(s_spX[idx] - s_px, s_spY[idx] - s_py) > 3072) break;
    }

    gap = lv->gap;

    if(lv->boss && !s_bossSpawned)
    {
        type = ET_BOSS;
        s_bossSpawned = 1;
        s_bossAlive   = 1;
        s_toSpawn     = 60000u;                         // endless adds from now on
        gap           = 120;
    }
    else
    {
        r = Rand16() % 100u;
        if(r < lv->pBrute)                  type = ET_BRUTE;
        else if(r < lv->pBrute + lv->pBat)  type = ET_BAT;
        else                                type = ET_ZOMBIE;

        if(lv->boss) gap = (uint16_t)(gap / 2u);
    }

    SpawnEnemy(type, s_spX[idx], s_spY[idx]);
    s_toSpawn--;
    s_spawnTimer = gap;
}

static void UpdateEnemies(void)
{
    uint16_t i, j;
    int16_t  dx, dy, dist, sp, mx, my, nx, ny;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        Enemy          *e = &s_en[i];
        const EnemyDef *d;

        if(!e->alive) continue;
        d = &kEnemy[e->type];

        if(e->flash) e->flash--;

        sp = d->speed;
        if(e->slow) { e->slow--; sp >>= 1; }

        if(e->type == ET_BOSS)
        {
            e->aux++;
            if(e->hp < RC_BOSS_HP / 2) sp += (sp >> 2);         // enraged
            if(e->aux >= 220)                                    // charge!
            {
                sp *= 3;
                if(e->aux >= 265) e->aux = 0;
            }
        }

        dx   = s_px - e->x;
        dy   = s_py - e->y;
        dist = Dist10(dx, dy);

        if(dist > 200)
        {
            mx = (int16_t)(((int32_t)dx * sp) / dist);
            my = (int16_t)(((int32_t)dy * sp) / dist);

            nx = e->x + mx;
            if(!Blocked(nx, e->y, 230)) e->x = nx;
            ny = e->y + my;
            if(!Blocked(e->x, ny, 230)) e->y = ny;
        }

        if(dist < d->reach) HurtPlayer();
    }

    // Keep them from stacking into one blob.
    for(i = 0; i + 1 < RC_MAX_ENEMIES; i++)
    {
        if(!s_en[i].alive) continue;
        for(j = i + 1; j < RC_MAX_ENEMIES; j++)
        {
            if(!s_en[j].alive) continue;
            dx = s_en[i].x - s_en[j].x;
            dy = s_en[i].y - s_en[j].y;
            if(Abs16(dx) < 300 && Abs16(dy) < 300)
            {
                int16_t px = (dx >= 0) ? 5 : -5;
                int16_t py = (dy >= 0) ? 5 : -5;

                if(!Blocked(s_en[i].x + px, s_en[i].y, 230)) s_en[i].x += px;
                if(!Blocked(s_en[i].x, s_en[i].y + py, 230)) s_en[i].y += py;
                if(!Blocked(s_en[j].x - px, s_en[j].y, 230)) s_en[j].x -= px;
                if(!Blocked(s_en[j].x, s_en[j].y - py, 230)) s_en[j].y -= py;
            }
        }
    }
}

//*****************************************************************************
// HUD drawing (top bar y 0..23, bottom bar y 192..239)
//*****************************************************************************
static void DrawHeart(int16_t x, int16_t y, uint32_t c)
{
    Graphics_FillRect(x + 1, y,     x + 5,  y + 1, c);
    Graphics_FillRect(x + 7, y,     x + 11, y + 1, c);
    Graphics_FillRect(x,     y + 2, x + 12, y + 5, c);
    Graphics_FillRect(x + 1, y + 6, x + 11, y + 6, c);
    Graphics_FillRect(x + 2, y + 7, x + 10, y + 7, c);
    Graphics_FillRect(x + 3, y + 8, x + 9,  y + 8, c);
    Graphics_FillRect(x + 4, y + 9, x + 8,  y + 9, c);
    Graphics_FillRect(x + 5, y + 10, x + 7, y + 10, c);
    Graphics_FillRect(x + 6, y + 11, x + 6, y + 11, c);
}

static void DrawHudFrame(void)
{
    Graphics_FillRect(0, 0, SCREEN_W - 1, RC_VIEW_Y0 - 1, C_BLACK);
    Graphics_FillRect(0, RC_VIEW_Y1 + 1, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);
    DrawText("U:fire  D:shop  L/R:turn", SCREEN_W / 2, 229, C_DKGRAY);
}

static void DrawHud(void)
{
    char    buf[24];
    int16_t n, i;

    if(s_hud & HUD_HEARTS)
    {
        Graphics_FillRect(0, 0, 112, RC_VIEW_Y0 - 1, C_BLACK);
        for(i = 0; i < (int16_t)s_maxHp; i++)
            DrawHeart(5 + i * 18, 6, (i < (int16_t)s_hp) ? C_RED : C_DKGRAY);
    }

    if(s_hud & HUD_LEVEL)
    {
        Graphics_FillRect(114, 0, 204, RC_VIEW_Y0 - 1, C_BLACK);
        if(kLevel[s_level].boss)
            DrawText("BOSS", 160, 12, C_RED);
        else
        {
            n  = Cat(buf, 0, "ROOM ", 5);
            n += Itoa(buf + n, s_level + 1);
            Cat(buf, n, "/5", 2);
            DrawText(buf, 160, 12, C_WHITE);
        }
    }

    if(s_hud & HUD_GOLD)
    {
        Graphics_FillRect(206, 0, SCREEN_W - 1, RC_VIEW_Y0 - 1, C_BLACK);
        n  = Cat(buf, 0, "G ", 2);
        Itoa(buf + n, s_gold);
        DrawText(buf, 265, 12, C_GOLD);
    }

    if(s_hud & HUD_STAFF)
    {
        Graphics_FillRect(0, RC_VIEW_Y1 + 2, 189, 215, C_BLACK);
        DrawText(kStaff[s_staff].name, 95, 205, kStaff[s_staff].color);
    }

    if(s_hud & HUD_LEFT)
    {
        Graphics_FillRect(190, RC_VIEW_Y1 + 2, SCREEN_W - 1, 215, C_BLACK);
        if(kLevel[s_level].boss)
        {
            int16_t hp = 0;
            for(i = 0; i < RC_MAX_ENEMIES; i++)
                if(s_en[i].alive && s_en[i].type == ET_BOSS) hp = s_en[i].hp;
            n  = Cat(buf, 0, "Boss HP ", 8);
            Itoa(buf + n, (uint16_t)((hp > 0) ? hp : 0));
        }
        else
        {
            n  = Cat(buf, 0, "Left ", 5);
            Itoa(buf + n, s_toSpawn + s_alive);
        }
        DrawText(buf, 255, 205, C_WHITE);
    }

    s_hud = 0;
}

//*****************************************************************************
// 3D view
//*****************************************************************************
static void InvalidateView(void)
{
    uint16_t c;

    for(c = 0; c < RC_COLS; c++)
    {
        s_cTop[c]  = -1;
        s_rngT[c]  = RC_VIEW_H;  s_rngB[c] = -1;
        s_prvT[c]  = RC_VIEW_H;  s_prvB[c] = -1;
    }
}

// Remember that rows y0..y1 of column c hold sprite/beam pixels this frame.
static void AddRange(int16_t c, int16_t y0, int16_t y1)
{
    if(y0 < 0) y0 = 0;
    if(y1 > RC_VIEW_H - 1) y1 = RC_VIEW_H - 1;
    if(y0 < s_rngT[c]) s_rngT[c] = y0;
    if(y1 > s_rngB[c]) s_rngB[c] = y1;
}

// Paint one wall strip, sending only what changed since the last frame.
// t / b are the first / last wall rows inside the view.
// rt..rb = rows last frame's sprite/beam touched; only those get repainted.
static void DrawColumn(uint16_t c, int16_t t1, int16_t b1, uint32_t col,
                       int16_t rt, int16_t rb)
{
    int16_t x0 = (int16_t)(RC_VIEW_X0 + (int16_t)c * RC_COL_W);
    int16_t x1 = x0 + RC_COL_W - 1;
    int16_t y0 = RC_VIEW_Y0;
    int16_t t0 = s_cTop[c];
    int16_t b0 = s_cBot[c];
    int16_t a, e;

    if(t0 < 0)
    {
        if(t1 > 0)
            Graphics_FillRect(x0, y0, x1, y0 + t1 - 1, COL_CEIL);
        Graphics_FillRect(x0, y0 + t1, x1, y0 + b1, col);
        if(b1 < RC_VIEW_H - 1)
            Graphics_FillRect(x0, y0 + b1 + 1, x1, y0 + RC_VIEW_H - 1, COL_FLOOR);
    }
    else if(col != s_cCol[c])
    {
        Graphics_FillRect(x0, y0 + t1, x1, y0 + b1, col);
        if(t1 > t0) Graphics_FillRect(x0, y0 + t0, x1, y0 + t1 - 1, COL_CEIL);
        if(b1 < b0) Graphics_FillRect(x0, y0 + b1 + 1, x1, y0 + b0, COL_FLOOR);
    }
    else
    {
        if(t1 > t0)      Graphics_FillRect(x0, y0 + t0, x1, y0 + t1 - 1, COL_CEIL);
        else if(t1 < t0) Graphics_FillRect(x0, y0 + t1, x1, y0 + t0 - 1, col);

        if(b1 < b0)      Graphics_FillRect(x0, y0 + b1 + 1, x1, y0 + b0, COL_FLOOR);
        else if(b1 > b0) Graphics_FillRect(x0, y0 + b0 + 1, x1, y0 + b1, col);
    }

    // Repair only the rows a sprite/beam dirtied last frame
    if(rt <= rb)
    {
        if(rt < t1)                                     // ceiling part
        {
            e = (rb < t1 - 1) ? rb : (int16_t)(t1 - 1);
            Graphics_FillRect(x0, y0 + rt, x1, y0 + e, COL_CEIL);
        }
        a = (rt > t1) ? rt : t1;                        // wall part
        e = (rb < b1) ? rb : b1;
        if(a <= e) Graphics_FillRect(x0, y0 + a, x1, y0 + e, col);
        if(rb > b1)                                     // floor part
        {
            a = (rt > b1 + 1) ? rt : (int16_t)(b1 + 1);
            Graphics_FillRect(x0, y0 + a, x1, y0 + rb, COL_FLOOR);
        }
    }

    s_cTop[c] = t1;
    s_cBot[c] = b1;
    s_cCol[c] = col;
}

// Turn every living enemy into a screen-space sprite, sorted far to near.
static void ProjectSprites(void)
{
    int16_t  c = COS(s_ang);
    int16_t  s = SIN(s_ang);
    uint16_t i, k;

    s_nSpr = 0;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        const Enemy    *e = &s_en[i];
        const EnemyDef *d;
        Spr            *sp;
        int16_t  rx, ry, along, side, ratio, theta, cx, x0, x1, lo, hi;
        uint16_t perp8, lineH, size;
        int16_t  floorY, bottom;

        if(!e->alive) continue;
        d = &kEnemy[e->type];

        rx    = e->x - s_px;
        ry    = e->y - s_py;
        along = (int16_t)(((int32_t)rx * c + (int32_t)ry * s) >> 14);
        if(along < 150) continue;                               // behind / too close

        side  = (int16_t)(((int32_t)ry * c - (int32_t)rx * s) >> 14);
        ratio = (int16_t)(((int32_t)side * 256L) / along);
        theta = (ratio < 0) ? -ratio : ratio;
        if(theta > 256) continue;                               // way outside the view
        theta = s_atan[theta];
        if(ratio < 0) theta = -theta;

        cx = (int16_t)(RC_VIEW_X0 + RC_VIEW_W / 2 + theta * RC_PX_PER_UNIT);

        perp8 = (uint16_t)(along >> 2);
        if(perp8 < RC_MIN_DIST) perp8 = RC_MIN_DIST;
        lineH = RC_PROJ / perp8;                                // wall height here
        size  = (uint16_t)(((uint32_t)lineH * d->scale) >> 4);
        if(size > 460) size = 460;
        if(size < 2)   continue;

        floorY = (int16_t)(RC_HORIZON + (lineH >> 1));
        bottom = (int16_t)(floorY - (int16_t)(((uint32_t)lineH * d->zoff) >> 4));

        x0 = cx - (int16_t)(size >> 1);
        x1 = x0 + (int16_t)size - 1;
        if(x1 < RC_VIEW_X0 || x0 >= RC_VIEW_X0 + RC_VIEW_W) continue;

        lo = (x0 < RC_VIEW_X0) ? RC_VIEW_X0 : x0;
        hi = (x1 > RC_VIEW_X0 + RC_VIEW_W - 1) ? (RC_VIEW_X0 + RC_VIEW_W - 1) : x1;

        sp = &s_spr[s_nSpr];
        sp->x0    = x0;
        sp->size  = size;
        sp->top   = (int16_t)(bottom - (int16_t)size + 1);
        sp->c0    = (uint16_t)(lo - RC_VIEW_X0) / RC_COL_W;
        sp->c1    = (uint16_t)(hi - RC_VIEW_X0) / RC_COL_W;
        sp->dist  = (uint16_t)(along >> 2);
        sp->uStep = (uint16_t)(4096u / size);
        sp->shape = d->shape;
        sp->type  = e->type;
        sp->flash = e->flash;

        // insertion sort, farthest first
        k = s_nSpr++;
        while(k > 0 && s_spr[k - 1].dist < s_spr[k].dist)
        {
            Spr tmp = s_spr[k - 1];
            s_spr[k - 1] = s_spr[k];
            s_spr[k] = tmp;
            k--;
        }
    }
}

static void DrawSlice(const Spr *sp, uint16_t c)
{
    int16_t  xl = (int16_t)(RC_VIEW_X0 + (int16_t)c * RC_COL_W);
    int16_t  xc = xl + (RC_COL_W >> 1) - sp->x0;
    uint16_t u, n, r;

    if(xc < 0) xc = 0;
    u = (uint16_t)(((int32_t)xc * sp->uStep) >> 8);
    if(u > SPR_N - 1) u = SPR_N - 1;

    n = s_runCnt[sp->shape][u];

    for(r = 0; r < n; r++)
    {
        int16_t y0 = (int16_t)(sp->top + (int16_t)(((int32_t)s_runY0[sp->shape][u][r] * sp->size) >> 4));
        int16_t y1 = (int16_t)(sp->top + (int16_t)(((int32_t)(s_runY1[sp->shape][u][r] + 1) * sp->size) >> 4) - 1);

        if(y0 < 0) y0 = 0;
        if(y1 > RC_VIEW_H - 1) y1 = RC_VIEW_H - 1;
        if(y1 < y0) continue;

        Graphics_FillRect(xl, RC_VIEW_Y0 + y0, xl + RC_COL_W - 1, RC_VIEW_Y0 + y1,
                          sp->flash ? C_WHITE : s_pal[sp->type][s_runCi[sp->shape][u][r]]);
    }
}

static void MarkOverlay(int16_t xa, int16_t xb)
{
    int16_t a, b, c;

    if(xa > xb) { int16_t t = xa; xa = xb; xb = t; }
    a = (int16_t)((xa - RC_VIEW_X0) / RC_COL_W) - 1;
    b = (int16_t)((xb - RC_VIEW_X0) / RC_COL_W) + 1;
    if(a < 0) a = 0;
    if(b > RC_COLS - 1) b = RC_COLS - 1;
    for(c = a; c <= b; c++) AddRange(c, 0, RC_VIEW_H - 1);
}

static void RenderView(void)
{
    uint16_t c, k, tile, side, dist, perp, lineH, shade;
    int16_t  top, bot;
    int16_t  cxp = RC_VIEW_X0 + RC_VIEW_W / 2;
    int16_t  cyp = RC_VIEW_Y0 + RC_HORIZON;

    if(s_invalidate)
    {
        InvalidateView();
        s_invalidate = false;
    }

    // Overlay bookkeeping: last frame's overlay columns must be repainted whole.
    for(c = 0; c < RC_COLS; c++)
    {
        s_prvT[c] = s_rngT[c];  s_prvB[c] = s_rngB[c];
        s_rngT[c] = RC_VIEW_H;  s_rngB[c] = -1;
    }

    ProjectSprites();

    for(c = 0; c < RC_COLS; c++)
    {
        dist = RC_CastRay(s_ang + s_colOff[c], &tile, &side);
        perp = (uint16_t)(((uint32_t)dist * (uint16_t)s_colCos[c]) >> 14);
        if(perp < RC_MIN_DIST) perp = RC_MIN_DIST;

        lineH = RC_PROJ / perp;
        lineH &= ~1u;                                   // even heights: fewer redraws
        if(lineH > RC_VIEW_H) lineH = RC_VIEW_H;
        if(lineH < 2)         lineH = 2;

        top = (int16_t)((RC_VIEW_H - lineH) >> 1);
        bot = (int16_t)(top + lineH - 1);

        shade = (perp < 512) ? 0 : (perp < 1024) ? 1 : (perp < 1792) ? 2 : 3;

        DrawColumn(c, top, bot, s_wallCol[tile][shade][side],
                   s_prvT[c], s_prvB[c]);

        for(k = 0; k < s_nSpr; k++)
        {
            if(c >= s_spr[k].c0 && c <= s_spr[k].c1 && s_spr[k].dist < perp)
            {
                DrawSlice(&s_spr[k], c);
                AddRange((int16_t)c, s_spr[k].top,
                         (int16_t)(s_spr[k].top + (int16_t)s_spr[k].size - 1));
            }
        }
    }

    // Overlays: beams, hit flash on the screen edges, crosshair.
    if(s_fxTimer)
    {
        int16_t b;

        for(b = 0; b < (int16_t)s_fxN; b++)
        {
            int16_t ex = s_fxX[b];

            Graphics_setForegroundColor(&g_sContext, s_fxColor);
            Graphics_drawLine(&g_sContext, cxp,     RC_VIEW_Y1, ex,     cyp);
            Graphics_drawLine(&g_sContext, cxp + 1, RC_VIEW_Y1, ex + 1, cyp);
            Graphics_FillRect(ex - 3, cyp - 3, ex + 3, cyp + 3, s_fxColor);
            MarkOverlay(cxp, ex + 4);
            MarkOverlay(ex - 4, cxp);
        }
    }

    if(s_hurtFx)
    {
        Graphics_FillRect(RC_VIEW_X0, RC_VIEW_Y0, RC_VIEW_X0 + 7, RC_VIEW_Y1, C_RED);
        Graphics_FillRect(RC_VIEW_X0 + RC_VIEW_W - 8, RC_VIEW_Y0,
                          RC_VIEW_X0 + RC_VIEW_W - 1, RC_VIEW_Y1, C_RED);
        AddRange(0, 0, RC_VIEW_H - 1);
        AddRange(1, 0, RC_VIEW_H - 1);
        AddRange(RC_COLS - 2, 0, RC_VIEW_H - 1);
        AddRange(RC_COLS - 1, 0, RC_VIEW_H - 1);
    }

    if(s_mode == MODE_PLAY)
    {
        Graphics_FillRect(cxp - 5, cyp, cxp + 5, cyp, 0xC0C0C0UL);
        Graphics_FillRect(cxp, cyp - 5, cxp, cyp + 5, 0xC0C0C0UL);
    }
}

//*****************************************************************************
// Banners and end screens
//*****************************************************************************
static void DrawBanner(void)
{
    char    buf[24];
    int16_t n;

    Graphics_FillRect(50, 84, SCREEN_W - 51, 140, C_BLACK);
    Graphics_FillRect(50, 84, SCREEN_W - 51, 85, C_GOLD);
    Graphics_FillRect(50, 139, SCREEN_W - 51, 140, C_GOLD);

    if(s_banner == BAN_CLEAR)
    {
        DrawText("ROOM CLEARED!", SCREEN_W / 2, 105, C_GREEN);
        DrawText("Time to shop...", SCREEN_W / 2, 126, C_GRAY);
    }
    else if(kLevel[s_level].boss)
    {
        DrawText("BOSS ROOM", SCREEN_W / 2, 105, C_RED);
        DrawText("The Lich awaits", SCREEN_W / 2, 126, C_GRAY);
    }
    else
    {
        n  = Cat(buf, 0, "ROOM ", 5);
        n += Itoa(buf + n, s_level + 1);
        DrawText(buf, SCREEN_W / 2, 105, C_WHITE);
        DrawText("Clear the wave!", SCREEN_W / 2, 126, C_GRAY);
    }
}

static void DrawEndScreen(bool win)
{
    char    buf[28];
    int16_t n;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    if(win)
    {
        DrawText("VICTORY!", SCREEN_W / 2, 70, C_GOLD);
        DrawText("The dungeon is clean.", SCREEN_W / 2, 100, C_WHITE);
    }
    else
    {
        DrawText("GAME OVER", SCREEN_W / 2, 70, C_RED);
        n  = Cat(buf, 0, "Died in room ", 13);
        Itoa(buf + n, s_level + 1);
        DrawText(buf, SCREEN_W / 2, 100, C_WHITE);
    }

    n  = Cat(buf, 0, "Kills ", 6);
    Itoa(buf + n, s_kills);
    DrawText(buf, SCREEN_W / 2, 135, C_GRAY);

    n  = Cat(buf, 0, "Gold ", 5);
    Itoa(buf + n, s_gold);
    DrawText(buf, SCREEN_W / 2, 160, C_GOLD);

    DrawText("Press D to exit", SCREEN_W / 2, 215, C_CYAN);
}

//*****************************************************************************
// Shop
//*****************************************************************************
// Fills the status text and returns: 0 = can act, 1 = can't afford, 2 = done.
static uint16_t ShopQuery(uint16_t tab, uint16_t row, const char **name,
                          char *stat, uint16_t *costOut)
{
    uint16_t cost = 0, lv = 0, mx = 0;
    int16_t  n;

    stat[0]  = 0;
    *costOut = 0;

    if(tab == TAB_STATS)
    {
        *name = kStatName[row];

        switch(row)
        {
            case ST_DMG:   lv = s_lvDmg;   mx = 6; cost = 30 + 25 * lv; break;
            case ST_RATE:  lv = s_lvRate;  mx = 5; cost = 30 + 25 * lv; break;
            case ST_SPEED: lv = s_lvSpeed; mx = 4; cost = 25 + 20 * lv; break;
            case ST_GOLD:  lv = s_lvGold;  mx = 4; cost = 40 + 30 * lv; break;
            case ST_HEART:
                lv = s_maxHp - RC_START_HEARTS;
                mx = RC_MAX_HEARTS - RC_START_HEARTS;
                cost = 70 + 50 * lv;
                break;
            default:                                            // ST_HEAL
                if(s_hp >= s_maxHp) { Cat(stat, 0, "FULL", 4); return 2; }
                cost = 20;
                n = Itoa(stat, cost);
                Cat(stat, n, "g", 1);
                *costOut = cost;
                return (s_gold >= cost) ? 0 : 1;
        }

        if(lv >= mx) { Cat(stat, 0, "MAX", 3); return 2; }

        n  = Cat(stat, 0, "Lv", 2);
        n += Itoa(stat + n, lv);
        n  = Cat(stat, n, " ", 1);
        n += Itoa(stat + n, cost);
        Cat(stat, n, "g", 1);
        *costOut = cost;
        return (s_gold >= cost) ? 0 : 1;
    }

    if(tab == TAB_TRAITS)
    {
        *name = kTraitName[row];
        if(s_traits & (1u << row)) { Cat(stat, 0, "OWNED", 5); return 2; }
        cost = kTraitCost[row];
    }
    else
    {
        *name = kStaff[row].name;
        if(s_staffOwned & (1u << row))
        {
            if(s_staff == row) { Cat(stat, 0, "EQUIPPED", 8); return 2; }
            Cat(stat, 0, "EQUIP", 5);
            return 0;
        }
        cost = kStaffCost[row];
    }

    n = Itoa(stat, cost);
    Cat(stat, n, "g", 1);
    *costOut = cost;
    return (s_gold >= cost) ? 0 : 1;
}

static void ShopAct(void)
{
    char        stat[16];
    const char *nm;
    uint16_t    cost, st;

    st = ShopQuery(s_shopTab, s_shopRow, &nm, stat, &cost);

    s_shopDirty |= SD_MSG | (1u << s_shopRow);

    // Owned staff that is not equipped: equip it.
    if(s_shopTab == TAB_STAFFS && (s_staffOwned & (1u << s_shopRow)))
    {
        if(s_staff != s_shopRow)
        {
            s_staff = s_shopRow;
            s_hud  |= HUD_STAFF;
            s_shopMsg = 4;
            s_shopDirty |= 0x1F;
            Sfx(&sfx_menu_select);
        }
        else
        {
            s_shopMsg = 3;
            Sfx(&sfx_menu_bump);
        }
        return;
    }

    if(st == 2)         { s_shopMsg = 3; Sfx(&sfx_menu_bump); return; }
    if(s_gold < cost)   { s_shopMsg = 2; Sfx(&sfx_menu_bump); return; }

    s_gold -= cost;

    if(s_shopTab == TAB_STATS)
    {
        switch(s_shopRow)
        {
            case ST_DMG:   s_lvDmg++;   break;
            case ST_RATE:  s_lvRate++;  break;
            case ST_SPEED: s_lvSpeed++; break;
            case ST_GOLD:  s_lvGold++;  break;
            case ST_HEART: s_maxHp++; s_hp++; break;
            default:       s_hp++;      break;                  // heal
        }
    }
    else if(s_shopTab == TAB_TRAITS)
    {
        s_traits |= (1u << s_shopRow);
    }
    else
    {
        s_staffOwned |= (1u << s_shopRow);
        s_staff       = s_shopRow;
        s_shopDirty  |= 0x1F;                                   // EQUIPPED label moves
    }

    s_shopMsg    = 1;
    s_shopDirty |= SD_GOLD | SD_MSG | 0x3F;
    s_hud       |= HUD_GOLD | HUD_HEARTS | HUD_STAFF;
    Sfx(&sfx_coin);
}

static void DrawShopRow(uint16_t i)
{
    char        stat[16];
    const char *nm;
    uint16_t    cost, st;
    int16_t     y   = SHOP_Y0 + (int16_t)i * SHOP_DY;
    bool        sel = (i == s_shopRow);

    Graphics_FillRect(6, y - 11, SCREEN_W - 7, y + 11, C_BLACK);
    if(i >= kTabRows[s_shopTab]) return;

    st = ShopQuery(s_shopTab, i, &nm, stat, &cost);

    if(sel) DrawText(">", 16, y, C_CYAN);
    DrawText(nm, 105, y, sel ? C_CYAN : C_GRAY);
    DrawText(stat, 255, y, (st == 0) ? C_GREEN : (st == 1) ? C_BAD : C_GRAY);
}

static void DrawShopTabs(void)
{
    uint16_t t;

    Graphics_FillRect(6, 34, SCREEN_W - 7, 56, C_BLACK);
    for(t = 0; t < TAB_COUNT; t++)
        DrawText(kTabName[t], 60 + (int16_t)t * 100, 46,
                 (t == s_shopTab) ? C_CYAN : C_DKGRAY);
}

static void DrawShopGold(void)
{
    char    buf[16];
    int16_t n;

    Graphics_FillRect(0, 2, SCREEN_W - 1, 28, C_BLACK);
    n = Cat(buf, 0, "G ", 2);
    Itoa(buf + n, s_gold);
    DrawText(buf, 265, 15, C_GOLD);
}

static void DrawShopFull(void)
{
    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);
    DrawText("SHOP", 60, 15, C_WHITE);
    Graphics_setForegroundColor(&g_sContext, C_GRAY);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
    DrawText("Stick:pick U:buy L/R:tab D:close", SCREEN_W / 2, 230, C_DKGRAY);
    s_shopDirty = SD_ALL;
}

static void ShopRender(void)
{
    uint16_t i;

    if(s_shopDirty & SD_TABS) DrawShopTabs();
    if(s_shopDirty & SD_GOLD) DrawShopGold();

    for(i = 0; i < 6; i++)
        if(s_shopDirty & (1u << i)) DrawShopRow(i);

    if(s_shopDirty & SD_MSG)
    {
        Graphics_FillRect(6, 240, SCREEN_W, 320, COLOR_BLACK);
        DrawText(kMsgText[s_shopMsg], SCREEN_W / 2, 208,
                 (s_shopMsg == 1 || s_shopMsg == 4) ? C_GREEN : C_BAD);
    }

    s_shopDirty = 0;
}

static void EnterShop(void)
{
    s_mode      = MODE_SHOP;
    s_dirtyFull = true;
    s_shopMsg   = 0;
    s_stX = s_stY = 1;                  // ignore a stick that is already pushed
    Sfx(&sfx_menu_select);
}

static void StartLevel(uint16_t lv);

static void ExitShop(void)
{
    Sfx(&sfx_menu_back);

    if(s_shopNext)
    {
        s_shopNext = false;
        StartLevel(s_level + 1);
    }
    else
    {
        s_mode      = MODE_PLAY;
        s_dirtyFull = true;
    }
}

static void UpdateShop(void)
{
    int16_t  sx = StickEdge(g_joy.dx, &s_stX);
    int16_t  sy = StickEdge(g_joy.dy, &s_stY);
    uint16_t rows = kTabRows[s_shopTab];

    if(g_dpad.dPressed) { ExitShop(); return; }

    if(sy != 0)
    {
        uint16_t old = s_shopRow;

        s_shopRow = (sy > 0) ? (s_shopRow + 1u) % rows : (s_shopRow + rows - 1u) % rows;
        s_shopDirty |= (1u << old) | (1u << s_shopRow);
        Sfx(&sfx_menu_move);
    }

    if(g_dpad.lPressed || g_dpad.rPressed || sx != 0)
    {
        bool left = g_dpad.lPressed || (sx < 0);

        s_shopTab = left ? (s_shopTab + TAB_COUNT - 1u) % TAB_COUNT
                         : (s_shopTab + 1u) % TAB_COUNT;
        s_shopRow   = 0;
        s_shopMsg   = 0;
        s_shopDirty = SD_ALL;
        Sfx(&sfx_menu_move);
    }

    if(g_dpad.uPressed || g_joy.btnPressed)
        ShopAct();
}

//*****************************************************************************
// Run / level flow
//*****************************************************************************
static void ResetRun(void)
{
    s_hp = s_maxHp = RC_START_HEARTS;
    s_gold = 0;
    s_kills = 0;
    s_lvDmg = s_lvRate = s_lvSpeed = s_lvGold = 0;
    s_traits = 0;
    s_staffOwned = 1;
    s_staff = 0;
    s_shopTab = s_shopRow = 0;
    s_shopNext = false;
}

static void StartLevel(uint16_t lv)
{
    uint16_t i;

    s_level = lv;
    LoadMap(kLevel[lv].map);

    for(i = 0; i < RC_MAX_ENEMIES; i++) s_en[i].alive = 0;

    s_alive = 0;
    s_toSpawn = kLevel[lv].boss ? 1u : kLevel[lv].total;
    s_spawnTimer = 90;
    s_spawnIdx = 0;
    s_bossSpawned = s_bossAlive = 0;

    s_ang    = 768;                     // face "up" the map
    s_invuln = 60;
    s_cool   = 0;
    s_fxTimer = s_hurtFx = 0;

    s_mode      = MODE_INTRO;
    s_timer     = 75;
    s_banner    = BAN_LEVEL;
    s_dirtyFull = true;
}

static void EnterOver(void)
{
    s_mode      = MODE_OVER;
    s_dirtyFull = true;
    Sfx(&sfx_game_over);
}

static void EnterWin(void)
{
    s_mode      = MODE_WIN;
    s_dirtyFull = true;
    Sfx(&sfx_level_up);
}

static void LevelClear(void)
{
    s_mode       = MODE_CLEAR;
    s_timer      = 80;
    s_banner     = BAN_CLEAR;
    s_bannerDraw = true;
    Sfx(&sfx_level_up);
}

static void UpdatePlay(void)
{
    const LevelDef *lv = &kLevel[s_level];
    bool    moved = false;
    int16_t jx, jy, fwd, str, c, s, mx, my, nx, ny, sp;

    if(s_cool)    s_cool--;
    if(s_invuln)  s_invuln--;
    if(s_fxTimer) s_fxTimer--;
    if(s_hurtFx)  s_hurtFx--;

    // Shop
    if(g_dpad.dPressed)
    {
        s_shopNext = false;
        EnterShop();
        return;
    }

    // Turn
    if(g_dpad.l) { s_ang -= RC_TURN_SPEED; moved = true; }
    if(g_dpad.r) { s_ang += RC_TURN_SPEED; moved = true; }
    s_ang &= RC_ANGLE_MASK;

    // Move: stick up = forward, stick right = strafe right.
    jx = g_joy.dx;
    jy = RC_INVERT_Y ? g_joy.dy : (int16_t)-g_joy.dy;
    if(Abs16(jx) < RC_STICK_DEAD) jx = 0;
    if(Abs16(jy) < RC_STICK_DEAD) jy = 0;

    if(jx || jy)
    {
        sp  = (int16_t)(RC_MOVE_SPEED + ((RC_MOVE_SPEED * s_lvSpeed) >> 3));
        fwd = (int16_t)(((int32_t)jy * sp * 41L) >> 12);        // stick/100 * speed
        str = (int16_t)(((int32_t)jx * sp * 41L) >> 12);
        c   = COS(s_ang);
        s   = SIN(s_ang);
        mx  = (int16_t)((((int32_t)fwd * c) - ((int32_t)str * s)) >> 14);
        my  = (int16_t)((((int32_t)fwd * s) + ((int32_t)str * c)) >> 14);

        nx = s_px + mx;
        if(!Blocked(nx, s_py, RC_PLAYER_R)) { s_px = nx; moved = true; }
        ny = s_py + my;
        if(!Blocked(s_px, ny, RC_PLAYER_R)) { s_py = ny; moved = true; }
    }

    // Fire (hold = auto-fire)
    if(g_dpad.u && s_cool == 0) Fire();

    UpdateEnemies();
    TrySpawn();

    if(s_mode == MODE_PLAY)
    {
        bool cleared = lv->boss ? (s_bossSpawned && !s_bossAlive)
                                : (s_toSpawn == 0 && s_alive == 0);
        if(cleared) LevelClear();
    }

    if(moved || s_alive || s_fxTimer || s_hurtFx) s_viewDirty = 2;
}

//*****************************************************************************
// App callbacks
//*****************************************************************************
static uint32_t s_lastTick;

static void PaceInit(void)
{
    s_lastTick = TimerNow();
}

// Busy-wait until one frame period has passed since the previous frame ended.
// The counter counts DOWN, so elapsed = last - now (wrap-safe in uint32).
static void PaceWait(void)
{
    while((uint32_t)(s_lastTick - TimerNow()) < RC_FRAME_TICKS) { }
    s_lastTick = TimerNow();
}

static void RC_Init(void)
{
    PaceInit();
    BuildTables();
    ResetRun();
    s_stX = s_stY = 0;
    StartLevel(0);

    /* Loop the main theme; SFX temporarily take the buzzer and music resumes. */
    Buzzer_PlaySong(&song_corridor_run, 1);
}

static AppStatus RC_Update(void)
{
    switch(s_mode)
    {
        case MODE_INTRO:
            if(--s_timer == 0)
            {
                s_mode       = MODE_PLAY;
                s_invalidate = true;            // wipes the banner
                s_viewDirty  = 2;
            }
            break;

        case MODE_PLAY:
            UpdatePlay();
            break;

        case MODE_SHOP:
            UpdateShop();
            break;

        case MODE_CLEAR:
            if(--s_timer == 0)
            {
                if(s_level + 1 >= RC_NUM_LEVELS) EnterWin();
                else { s_shopNext = true; EnterShop(); }
            }
            break;

        default:                                // OVER / WIN
            if(g_dpad.dPressed || g_joy.btnPressed) return APP_QUIT;
            break;
    }

    return APP_CONTINUE;
}

static void FullRepaint(void)
{
    if(s_mode == MODE_SHOP)
        DrawShopFull();
    else if(s_mode == MODE_OVER)
        DrawEndScreen(false);
    else if(s_mode == MODE_WIN)
        DrawEndScreen(true);
    else
    {
        DrawHudFrame();
        s_hud        = HUD_ALL;
        s_invalidate = true;
        s_viewDirty  = 2;
        s_bannerDraw = (s_mode == MODE_INTRO || s_mode == MODE_CLEAR);
    }
}

static void RC_RenderInner(void)
{
    if(s_dirtyFull)
    {
        s_dirtyFull = false;
        FullRepaint();
    }

    if(s_mode == MODE_SHOP)
    {
        ShopRender();
        return;
    }

    if(s_mode == MODE_OVER || s_mode == MODE_WIN) return;

    if(s_hud) DrawHud();

    if(s_viewDirty)
    {
        RenderView();
        s_viewDirty--;
    }

    if(s_bannerDraw)
    {
        DrawBanner();
        s_bannerDraw = false;
    }
}

static void RC_Render(void)
{
    RC_RenderInner();
    PaceWait();
}

// After the pause overlay: repaint everything from the current state.
static void RC_Redraw(void)
{
    s_lastTick = TimerNow();                    // don't "catch up" after the pause
    s_dirtyFull = true;
    if(s_mode != MODE_OVER && s_mode != MODE_WIN)
        Buzzer_PlaySong(&song_corridor_run, 1);
}

static void RC_Exit(void)
{
    Buzzer_Stop();
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//*****************************************************************************
const App g_raycasterApp =
{
    "RAYCASTER",                            // name (first letter = menu tile icon)
    "Stick:move L/R:turn U:fire D:shop",    // hint shown under the menu
    0x7050FFUL,                             // menu tile color
    RC_FRAME_US,
    RC_Init, RC_Update, RC_Render,
    RC_Exit,
    NULL,                                   // not a legacy app
    RC_Redraw
};
