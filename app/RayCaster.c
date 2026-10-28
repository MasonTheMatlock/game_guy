//#############################################################################
// FILE:   app/raycaster.c
// TITLE:  RAYCASTER - first-person endless dungeon roguelike (F28069 + SSD2119)
//
// HOW IT IS BUILT FOR THIS HARDWARE (C28x @ 90 MHz, SPI LCD, no frame buffer)
//
//  * The LCD is the bottleneck, not the CPU. So the 3D view is drawn as
//    RC_COLS vertical strips, and each strip is DIFFED against what is already
//    on the screen: only the pixels whose wall height / shade changed are
//    sent. A still camera costs almost nothing.
//  * Everything is integer / table driven. No float after init.
//  * int is 16 bits on C28x: every product that can exceed 32767 is cast to
//    int32_t first. There is no uint8_t on this compiler, so none is used.
//  * Positions are Q10 (1024 = 1 tile) in int16_t. Ray distances are Q8.
//  * Enemies, flying flasks, poison clouds and enemy laser bolts are all
//    16x16 pixel-art sprites turned into per-column colour runs at init, then
//    drawn as a few FillRects per screen column, z-tested against the wall.
//  * The beam / bolt animations are a few tiny rectangles per frame (no
//    pixel-by-pixel lines), and only the rows they touched get repaired.
//
// GAME FLOW
//  * Endless rooms. Clear every enemy and the room's DOOR opens (it glows);
//    walk into it to reach the next room. Every 6th room is the Lich.
//  * Kills give GOLD (same as before) and XP. When the XP bar fills you level
//    up and get 3 random cards to buy with gold (or reroll them for gold).
//  * D-pad Down cycles the equipped staff / magic item. D-pad Up uses it.
//
// APP CONTRACT: update() = logic only, render() = drawing only, one public
// symbol (g_raycasterApp). The runner already calls Joy_Update()/Dpad_Update().
//#############################################################################

#include "raycaster.h"
#include <math.h>

/* Performance configuration: 40 strips x 8 pixels = 320 pixels. */
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

#define RC_VIEW_W   (RC_COLS * RC_COL_W)
#define RC_VIEW_X0 (((SCREEN_W - RC_VIEW_W) / 2) + RC_VIEW_X_OFFSET)

#define RC_VIEW_Y1      (RC_VIEW_Y0 + RC_VIEW_H - 1)
#define RC_HORIZON      (RC_VIEW_H / 2)                 // row (inside view) of the horizon
#define RC_PROJ         ((uint16_t)(RC_VIEW_H * 256UL)) // wall is full height at 1 tile
#define RC_MIN_DIST     40                              // Q8, stops divide blow-up
#define RC_MAX_STEPS    28

#define RC_FOOTSTEPS    1
#define RC_STEP_DIST    560
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
#define C_XP            0xB070FFUL

// Game modes
#define MODE_INTRO      0               // "ROOM n" banner, world frozen
#define MODE_PLAY       1
#define MODE_CARDS      2               // level-up card screen
#define MODE_CLEAR      3               // "ROOM CLEARED" banner
#define MODE_OVER       4

// Banners
#define BAN_LEVEL       1
#define BAN_CLEAR       2

// HUD dirty bits
#define HUD_HEARTS      0x01
#define HUD_GOLD        0x02
#define HUD_LEVEL       0x04
#define HUD_LEFT        0x08
#define HUD_STAFF       0x10
#define HUD_XP          0x20
#define HUD_HINT        0x40
#define HUD_ALL         0x7F
#define HUD_TOPR        (HUD_GOLD | HUD_LEVEL | HUD_XP)

#define RC_BOSS_HP      140
#define RC_BOSS_EVERY   6               // every 6th room is the Lich

/* Frame cap. Reuses the free-running 32-bit down counter that joystick.c
 * already starts (CpuTimer0): TimerNow() and TICKS_PER_MS come from drivers.h. */
#define RC_TARGET_FPS   30UL
#define RC_FRAME_TICKS  (((uint32_t)TICKS_PER_MS * 1000UL) / RC_TARGET_FPS)

//*****************************************************************************
// Enemies
//   goblin   : plain melee                    hobgoblin : tough goblin
//   bat      : fast, flies                    skeleton  : melee, the Lich summons them
//   beholder : floats at range, shoots dodgeable RED LASER bolts
//   lich     : boss. Summons skeletons, charges when angry
//*****************************************************************************
enum { ET_GOBLIN, ET_BAT, ET_HOBGOB, ET_SKELETON, ET_BEHOLDER, ET_LICH, ET_COUNT };

// Sprite shapes (16x16 art) and palettes. Palette ids 0..5 match ET_*.
enum { SH_GOBLIN, SH_BAT, SH_SKELETON, SH_LICH, SH_BEHOLDER,
       SH_FLASK, SH_BOLT, SH_PUFF, SH_COUNT };
enum { PT_GOBLIN, PT_BAT, PT_HOBGOB, PT_SKELETON, PT_BEHOLDER, PT_LICH,
       PT_BEHOLDER_HOT,
       PT_FLASK_FIRE, PT_FLASK_FROST, PT_FLASK_VENOM,
       PT_BOLT,
       PT_PUFF_FIRE, PT_PUFF_FROST, PT_PUFF_VENOM, PT_PUFF_ARCANE,
       PT_COUNT };

typedef struct
{
    int16_t  hp;
    int16_t  speed;         // Q10 units per frame
    int16_t  gold;
    int16_t  xp;
    int16_t  radius;        // how fat the beam hit-test thinks it is (Q10)
    int16_t  reach;         // contact distance that hurts the player (Q10)
    uint16_t scale;         // sprite height, Q4 fraction of wall height (16 = 1.0)
    uint16_t zoff;          // sprite lift, Q4 fraction of wall height
    uint16_t shape;
} EnemyDef;

static const EnemyDef kEnemy[ET_COUNT] =
{
    //  hp  spd gold  xp  rad reach scale zoff shape
    {    6,  26,   8,  4, 330,  540,  12,   0, SH_GOBLIN   },   // goblin
    {    3,  44,   6,  3, 260,  480,   8,   5, SH_BAT      },   // bat
    {   18,  17,  25, 10, 440,  620,  17,   0, SH_GOBLIN   },   // hobgoblin
    {    8,  30,  10,  5, 330,  540,  14,   0, SH_SKELETON },   // skeleton
    {   14,  12,  30, 14, 420,  520,  16,   4, SH_BEHOLDER },   // beholder
    { RC_BOSS_HP, 22, 200, 80, 620, 800, 26,  0, SH_LICH   },   // lich
};

typedef struct
{
    int16_t  x, y;          // Q10
    int16_t  hp;
    uint16_t type;
    uint16_t flash;         // frames of white hit-flash left
    uint16_t slow;          // frames of frost left
    uint16_t freeze;        // frames frozen solid (no move, no attack)
    uint16_t burn;          // frames of burning left
    uint16_t aux;           // boss charge / beholder shot timer / "summoned" flag
    uint16_t aux2;          // boss summon timer
    uint16_t alive;
} Enemy;

//*****************************************************************************
// Equipment. Slots 0..4 are staffs, slots 5..12 are magic items.
// D-pad Down cycles through the ones you own; D-pad Up uses the equipped one.
//*****************************************************************************
#define NUM_STAFF   5
#define NUM_ITEM    8
#define NUM_SLOT    (NUM_STAFF + NUM_ITEM)

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

/* Shot sound for each staff (same order as kStaff). Defined in songs.c. */
static const Song *const kStaffSfx[NUM_STAFF] =
{
    &sfx_staff_ray, &sfx_staff_prism, &sfx_staff_lance,
    &sfx_staff_storm, &sfx_staff_sun
};

//   Fire / Frost / Venom Flask : thrown, shatter in a splash (fire burns, frost
//                                freezes + slows, venom leaves a lingering cloud)
//   Elixir     : drink to heal two hearts
//   Haste Brew : faster movement, faster firing, faster cooldowns for ~10 s
//   Arcane Tome: three homing bolts, one per nearest visible enemy
//   Warp Rune  : blink 3 tiles forward
//   Aegis Charm: brief invulnerability + a shockwave that shoves enemies away
enum { IT_FIRE, IT_FROST, IT_VENOM, IT_ELIXIR, IT_HASTE, IT_TOME, IT_WARP, IT_AEGIS };

typedef struct
{
    const char *name;
    uint16_t    cooldown;   // frames
    uint32_t    color;      // HUD name color
} ItemDef;

static const ItemDef kItem[NUM_ITEM] =
{
    { "Fire Flask",   60, 0xFF7020UL },
    { "Frost Flask",  70, 0x60D0FFUL },
    { "Venom Flask", 110, 0x60E040UL },
    { "Elixir",      600, 0xFF4068UL },
    { "Haste Brew",  700, 0xFFE040UL },
    { "Arcane Tome",  50, 0xC070FFUL },
    { "Warp Rune",   120, 0x40FFC0UL },
    { "Aegis Charm", 480, 0x70A0FFUL },
};
static const uint16_t kItemCost[NUM_ITEM] = { 90, 90, 110, 80, 100, 170, 120, 150 };

// Potion kinds (also the order of the three flask palettes / puff palettes)
enum { PK_FIRE, PK_FROST, PK_VENOM, PK_ARCANE };
static const int16_t  kPotRadius[3] = { 1300, 1500, 1100 };   // splash radius (Q10)
static const uint16_t kPotDmg[3]    = {    5,    2,    1 };   // base splash damage
static const uint16_t kPotLife[3]   = {   18,   24,  200 };   // puff frames (venom lingers)
static const uint16_t kPotScale[3]  = {   22,   24,   18 };   // puff size

enum { TR_VAMP, TR_PIERCE, TR_THICK, TR_KNOCK, TR_LUCKY, TR_FROST, TR_COUNT };
static const uint16_t kTraitCost[TR_COUNT] = { 120, 140, 90, 80, 110, 100 };

enum { ST_DMG, ST_RATE, ST_SPEED, ST_HEART, ST_HEAL, ST_GOLD, ST_COUNT };

//*****************************************************************************
// Level-up offers. Every card is one of these. kind/idx say what it is;
// desc lines are separated by '|' (each line must fit a card: 12 chars max).
//*****************************************************************************
enum { K_STAT, K_TRAIT, K_STAFF, K_ITEM };

typedef struct
{
    uint16_t    kind, idx;
    const char *name;           // <= 11 chars
    const char *desc;
} Offer;

#define NUM_OFFER 24
static const Offer kOffer[NUM_OFFER] =
{
    { K_STAT,  ST_DMG,   "Damage",      "+25% damage|each shot|(max Lv 6)" },
    { K_STAT,  ST_RATE,  "Fire Rate",   "-9% cooldown|for staffs|(max Lv 5)" },
    { K_STAT,  ST_SPEED, "Move Speed",  "walk faster|(max Lv 4)" },
    { K_STAT,  ST_HEART, "Max Heart",   "+1 heart and|heal one" },
    { K_STAT,  ST_HEAL,  "Heal",        "restore one|heart now" },
    { K_STAT,  ST_GOLD,  "Gold Find",   "+20% gold|from kills|(max Lv 4)" },

    { K_TRAIT, TR_VAMP,   "Vampire",    "every 10th|kill heals|one heart" },
    { K_TRAIT, TR_PIERCE, "Piercing",   "beams hit|one extra|enemy" },
    { K_TRAIT, TR_THICK,  "Thick Skin", "longer safe|time after|a hit" },
    { K_TRAIT, TR_KNOCK,  "Knockback",  "hits shove|enemies|back" },
    { K_TRAIT, TR_LUCKY,  "Lucky",      "1 in 5 shots|do double|damage" },
    { K_TRAIT, TR_FROST,  "Frost Touch","hits slow|enemies" },

    { K_STAFF, 1, "Prism Staff", "3 beams in|a fan, wide|coverage" },
    { K_STAFF, 2, "Lance",       "heavy beam|pierces all|enemies" },
    { K_STAFF, 3, "Storm Rod",   "very fast|weak shots" },
    { K_STAFF, 4, "Sunfire",     "5 flame|beams, each|pierce one" },

    { K_ITEM, IT_FIRE,   "Fire Flask",  "lob a flask,|fiery splash|burns foes" },
    { K_ITEM, IT_FROST,  "Frost Flask", "icy splash|freezes then|slows foes" },
    { K_ITEM, IT_VENOM,  "Venom Flask", "poison cloud|that keeps|hurting" },
    { K_ITEM, IT_ELIXIR, "Elixir",      "drink: heal|two hearts|slow refill" },
    { K_ITEM, IT_HASTE,  "Haste Brew",  "10s: faster|move, fire|and cooldown" },
    { K_ITEM, IT_TOME,   "Arcane Tome", "3 homing|bolts at the|nearest foes" },
    { K_ITEM, IT_WARP,   "Warp Rune",   "blink three|tiles ahead" },
    { K_ITEM, IT_AEGIS,  "Aegis Charm", "brief shield|and shove|enemies away" },
};

//*****************************************************************************
// Rooms. Endless: the generator below scales the numbers with depth.
//*****************************************************************************
typedef struct
{
    uint16_t total, maxAlive, gap;
    uint16_t pBat, pBrute, pSkel, pBeh;     // percent chances
    uint16_t boss;
} LevelDef;

//*****************************************************************************
// Maps (16 x 16).  # stone   P pillar   B brick   . floor   D door
//                  e enemy spawn marker   @ player start
// The door is a wall tile on the top edge: dark wood while the room is
// full, a glowing portal once it is cleared. Walk into it for the next room.
//*****************************************************************************
#define RC_NUM_MAPS 6
#define MAP_BOSS    5

static const char *const kMaps[RC_NUM_MAPS][RC_MAP_H] =
{
    {   // 0 - Entry Hall
        "#######D########",
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
        "#######D########",
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
        "#######D########",
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
        "#######D########",
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
        "#######D########",
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
        "#######D########",
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
// Sprite art (16 x 16).  '.' = transparent, letters a..p = palette entries.
//*****************************************************************************
#define SPR_N       16
#define SPR_MAXRUNS 6

static const char *const kArtGoblin[SPR_N] =
{
    "................",
    "................",
    "..dd.bbbbbb.dd..",
    "..ddbbddddbbdd..",
    "...dbdcddcdbd...",
    "....bddddddb....",
    "....bdhffhdb....",
    ".....bdddddb....",
    "...deeeeeeeed.g.",
    "..dbeeaaaaeebdg.",
    "..dbeeeeeeeebdg.",
    "..db.eeeeee.bda.",
    ".....ee..ee.....",
    ".....ee..ee.....",
    "....aaa..aaa....",
    "................"
};

static const char *const kArtBat[SPR_N] =
{
    "................",
    "................",
    ".aa..........aa.",
    ".aaa...aa...aaa.",
    ".aaaa.aaaa.aaaa.",
    "aaaaaaabbaaaaaaa",
    ".accaaaaaaaacca.",
    "..ac.aaaaaa.ca..",
    "...a..adda..a...",
    "......aaaa......",
    ".......aa.......",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArtSkeleton[SPR_N] =
{
    "................",
    ".....dddddd.....",
    "....dddddddd....",
    "....dbbddbbd....",
    "....dddaaddd....",
    ".....dddddd.....",
    "......dadad.....",
    ".dd.dddddddd.dd.",
    "..d..daddad..d.c",
    "..d..dddddd..d.c",
    "..d..daddad..d.c",
    "..d..dddddd..dcc",
    ".....dddddd.....",
    ".....dd..dd.....",
    ".....dd..dd.....",
    "....ddd..ddd...."
};

static const char *const kArtLich[SPR_N] =
{
    "....a.a..a.a....",
    "....aaaaaaaa....",
    "....bbbbbbbb....",
    "....bccbbccb....",
    "....bbbbbbbb....",
    ".....bbffbb.....",
    "...dddeeeeddd...",
    "..deeeeeeeeeed..",
    ".deeeeeeeeeeeed.",
    ".deebeeeeeebeed.",
    ".deeeeeeeeeeeed.",
    "..deeeeeeeeeed..",
    "..deeeeeeeeeed..",
    "...deeeeeeeed...",
    "...dddeeeeddd...",
    "....dddddddd...."
};

static const char *const kArtBeholder[SPR_N] =
{
    "..h..h....h..h..",
    "..h..h....h..h..",
    "...f.f....f.f...",
    "....f.f..f.f....",
    ".....fccccf.....",
    "....cccccccc....",
    "...ccggggggcc...",
    "...ccgddddgcc...",
    "...ccgdaadgcc...",
    "...ccgddddgcc...",
    "...ccggggggcc...",
    "....cccccccc....",
    "....cebebecc....",
    ".....cccccc.....",
    "......ffff......",
    "................"
};

static const char *const kArtFlask[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    ".......bb.......",
    "......bddb......",
    "......bccb......",
    ".....bcaacb.....",
    "....bcaaaacb....",
    "....baaaaaab....",
    "....baaaaaab....",
    ".....baaaab.....",
    "......bbbb......",
    "................",
    "................",
    "................"
};

static const char *const kArtBolt[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    ".......aa.......",
    "......aaaa......",
    ".....aabbaa.....",
    "....aabbbbaa....",
    "....aabbbbaa....",
    ".....aabbaa.....",
    "......aaaa......",
    ".......aa.......",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArtPuff[SPR_N] =
{
    "................",
    "................",
    ".....aabbaa.....",
    "...aabbbbbbaa...",
    "..aabbccbbbbaa..",
    ".aabbbccbbbbbaa.",
    ".abbbbbbbbccbba.",
    "aabbccbbbbbbbbaa",
    "abbbbbbbccbbbbba",
    "abbbccbbbbbbbbba",
    ".abbbbbbbbbccba.",
    ".aabbbccbbbbbaa.",
    "..aabbbbbbbbaa..",
    "...aabbbbbbaa...",
    ".....aaaaaa.....",
    "................"
};

static const char *const *const kArt[SH_COUNT] =
{
    kArtGoblin, kArtBat, kArtSkeleton, kArtLich, kArtBeholder,
    kArtFlask, kArtBolt, kArtPuff
};

typedef struct { char ch; uint32_t rgb; } PalEnt;      // list ends with ch = 0

static const PalEnt kPalGoblin[] =
    { {'d',0x40B030UL},{'b',0x206018UL},{'c',0xFFE020UL},{'h',0xFFFFFFUL},{'f',0x301010UL},
      {'e',0x7A5A30UL},{'a',0x4A3018UL},{'g',0xC8D0D8UL},{0,0} };
static const PalEnt kPalHobgob[] =
    { {'d',0xC06030UL},{'b',0x703010UL},{'c',0xFF3020UL},{'h',0xFFFFFFUL},{'f',0x301010UL},
      {'e',0x502020UL},{'a',0x301818UL},{'g',0xFFD060UL},{0,0} };
static const PalEnt kPalBat[]    =
    { {'a',0x8848C8UL},{'b',0xFF4040UL},{'c',0x502888UL},{'d',0xFFFFFFUL},{0,0} };
static const PalEnt kPalSkel[]   =
    { {'d',0xE8E4D0UL},{'a',0x706C58UL},{'b',0xFF3030UL},{'c',0xA0A8B8UL},{0,0} };
static const PalEnt kPalLich[]   =
    { {'a',0xFFD020UL},{'b',0xE8E8D8UL},{'c',0x40FFFFUL},{'d',0x34206CUL},
      {'e',0x6038B0UL},{'f',0x101010UL},{0,0} };
static const PalEnt kPalBeholder[] =
    { {'c',0x7A3C8CUL},{'f',0x4A1C5CUL},{'g',0xF0F0E8UL},{'d',0xD02020UL},{'a',0x101010UL},
      {'h',0xFFD030UL},{'e',0xFFFFFFUL},{'b',0x200820UL},{0,0} };
static const PalEnt kPalBeholderHot[] =
    { {'c',0x9A4CAAUL},{'f',0x4A1C5CUL},{'g',0xFFD0D0UL},{'d',0xFFFF50UL},{'a',0xFF2020UL},
      {'h',0xFF4040UL},{'e',0xFFFFFFUL},{'b',0x200820UL},{0,0} };
static const PalEnt kPalFlaskFire[] =
    { {'a',0xFF6020UL},{'c',0xD8E8F0UL},{'d',0x9A6A30UL},{'b',0x182028UL},{0,0} };
static const PalEnt kPalFlaskFrost[] =
    { {'a',0x60D8FFUL},{'c',0xD8E8F0UL},{'d',0x9A6A30UL},{'b',0x182028UL},{0,0} };
static const PalEnt kPalFlaskVenom[] =
    { {'a',0x50E040UL},{'c',0xD8E8F0UL},{'d',0x9A6A30UL},{'b',0x182028UL},{0,0} };
static const PalEnt kPalBolt[]   =
    { {'a',0xFF2020UL},{'b',0xFFFFFFUL},{0,0} };
static const PalEnt kPalPuffFire[] =
    { {'a',0xC03010UL},{'b',0xFF8020UL},{'c',0xFFE060UL},{0,0} };
static const PalEnt kPalPuffFrost[] =
    { {'a',0x3080C0UL},{'b',0x80D0FFUL},{'c',0xFFFFFFUL},{0,0} };
static const PalEnt kPalPuffVenom[] =
    { {'a',0x207020UL},{'b',0x50C030UL},{'c',0xB0FF70UL},{0,0} };
static const PalEnt kPalPuffArcane[] =
    { {'a',0x7040C0UL},{'b',0xC090FFUL},{'c',0xFFFFFFUL},{0,0} };

static const PalEnt *const kPal[PT_COUNT] =
{
    kPalGoblin, kPalBat, kPalHobgob, kPalSkel, kPalBeholder, kPalLich, kPalBeholderHot,
    kPalFlaskFire, kPalFlaskFrost, kPalFlaskVenom, kPalBolt,
    kPalPuffFire, kPalPuffFrost, kPalPuffVenom, kPalPuffArcane
};

// Sprite tint: 0 none, 1 white (hit), 2 orange (burning), 3 ice blue (frozen)
static const uint32_t kTint[4] = { 0, 0xFFFFFFUL, 0xFF8820UL, 0x70C8FFUL };

// Wall tiles: 0 unused, 1 stone, 2 pillar, 3 brick, 4 door (closed), 5 portal (open)
#define TILE_DOOR    4
#define TILE_PORTAL  5
#define TILE_KINDS   6
static const uint32_t kWallBase[TILE_KINDS] =
    { 0, 0x8088A0UL, 0xB09060UL, 0xA05040UL, 0x7A4A24UL, 0x30E0B0UL };
#define PORTAL_ALT   0xA0FFE8UL                 // portal pulse color
static const uint16_t kShadeMul[4] = { 256, 190, 130, 84 };
//*****************************************************************************
// First-person art (what Ray Caster holds in his LEFT hand). 20 x 18 cells,
// WPN_SCALE pixels per cell. '.' = transparent, a..z index the palette.
//   * rows 0..9  are unique to each staff / item (each is its own pixel art)
//   * rows 10..17 are the shared left hand (thumb on the right) and sleeve
// The shaft / neck is always columns 8-11 so every item joins the same fist.
//   k outline  s glove  q glove shade  c sleeve  d sleeve shade
//   w body     x body shade   g gem / liquid   h highlight
//   f flash    y accent       r accent 2
//*****************************************************************************
#define WPN_W        20
#define WPN_H        18
#define WPN_SCALE    4                  // 80 x 72 pixels on screen
#define WPN_TIP_ROW  1                  // art row the beam leaves from
#define WPN_RECT_MAX 110                // rectangles after merging
#define WPN_Y0       (RC_VIEW_H - WPN_H * WPN_SCALE)   // top row of the art in the view
#define WPN_XOFF     20                 // art sits left of centre: it is the LEFT hand

#define WPN_ART_ROWS 10                 // rows unique to each staff / item
static const char kWpnArm[] =
    ".....kssssssssk.kk.."
    ".....ksssssssskkssk."
    ".....kssssssssqkssk."
    ".....kssssssssqkkk.."
    "....kkqqqqqqqqkk...."
    "..kccccccccccck....."
    ".kccdccccccccdck...."
    "kccdcccccccccccdck..";

// ---- 0: RAY WAND (cyan crystal on a slim silver wand)
static const char kArtRay[] =
    "...................."
    ".........kk........."
    "........kghk........"
    "........kggk........"
    "........kggk........"
    ".........kk........."
    ".......kkwxkk......."
    "........kwxk........"
    "........kwxk........"
    "........kwxk........";

// ---- 1: PRISM STAFF (gold shaft, glass prism)
static const char kArtPrism[] =
    ".........kk........."
    "........kghk........"
    ".......kgghgk......."
    "......kgggghgk......"
    "......kkkkkkkk......"
    ".......kkwxkk......."
    "......kkkwxkkk......"
    "........kwxk........"
    "........kwxk........"
    "........kwxk........";

// ---- 2: LANCE (wooden haft, steel blade, red tassel)
static const char kArtLance[] =
    ".........kk........."
    "........kyhk........"
    "........kyhk........"
    "........kyhk........"
    ".......kkyhkk......."
    "......kkkkkkkk......"
    ".......rkwxkr......."
    ".......rkwxkr......."
    "........kwxk........"
    "........kwxk........";

// ---- 3: STORM ROD (steel fork with a spark between the prongs)
static const char kArtStorm[] =
    "......k......k......"
    "......kw....wk......"
    ".......kw..wk......."
    ".......kwggwk......."
    "........kwwk........"
    "........kxwk........"
    ".......kkwxkk......."
    "........kwxk........"
    "........kwxk........"
    "........kwxk........";

// ---- 4: SUNFIRE (golden staff, burning sun)
static const char kArtSun[] =
    ".........ff........."
    ".......kkkkkk......."
    "......kgghhggk......"
    "......kghhhhgk......"
    "......kgghhggk......"
    ".......kkkkkk......."
    "......kkkwxkkk......"
    "........kwxk........"
    "........kwxk........"
    "........kwxk........";

// ---- 5: FIRE FLASK (round bulb, burning wick)
static const char kArtFire[] =
    ".........ff........."
    "........fyyf........"
    "........krrk........"
    "........kwwk........"
    "........kwxk........"
    ".......kwggxk......."
    "......kwggghxk......"
    ".....kwgggghgxk....."
    "....kgggggghgggk...."
    "....kgggggggggxk....";

// ---- 6: FROST FLASK (tall vial, ice crystal stopper)
static const char kArtFrost[] =
    ".........hh........."
    "........hffh........"
    "........khhk........"
    "........kwwk........"
    ".......kwggxk......."
    ".......kwgghk......."
    ".......kwggxk......."
    ".......kgghgk......."
    ".......kggggk......."
    ".......kggggk.......";

// ---- 7: VENOM FLASK (skull stopper, toxic liquid)
static const char kArtVenom[] =
    ".......kyyyyk......."
    "......kyrkkryk......"
    ".......kkyykk......."
    "........kwwk........"
    ".......kwggxk......."
    "......kwgghgxk......"
    ".....kwggggggxk....."
    ".....kwghggggxk....."
    ".....kggggggggk....."
    "....kgghggggggxk....";

// ---- 8: ELIXIR (heart bottle with a gold ribbon)
static const char kArtElixir[] =
    ".........kk........."
    "........kyyk........"
    "........kwwk........"
    ".......rrrrrr......."
    ".......kwggxk......."
    ".....kwggkkggxk....."
    "....kwgggghgggxk...."
    "....kggggggggggk...."
    "....kkggggggggkk...."
    "......kkggggkk......";

// ---- 9: HASTE BREW (golden draught, lightning swirl)
static const char kArtHaste[] =
    ".........kk........."
    "........kyyk........"
    "........kwwk........"
    "........kwwk........"
    ".......kwggxk......."
    "......kwgyggxk......"
    ".....kwggyyggxk....."
    ".....kwgggyggxk....."
    "....kggggyyggggk...."
    "....kggggyggggxk....";

// ---- 10: ARCANE TOME (open spellbook, glowing runes)
static const char kArtTome[] =
    ".....f........f....."
    "...kkkkkkkkkkkkkk..."
    "...khhhhhxxhhhhhk..."
    "...khhgghxxhgghhk..."
    "...khhghhxxhhghhk..."
    "...khhgghxxhgghhk..."
    "...khhhhhxxhhhhhk..."
    "...kwwwwwxxwwwwwk..."
    "...kkkkkkxxkkkkkk..."
    "...kkkkkxxxxkkkkk...";

// ---- 11: WARP RUNE (glowing rune stone)
static const char kArtWarp[] =
    ".........ff........."
    "........kkkk........"
    ".......kwwwwk......."
    "......kwggggwk......"
    "......kwghhgwk......"
    "......kwghhgwk......"
    "......kwggggwk......"
    "......kwwwwwwk......"
    "......kxwwwwxk......"
    "......kkxwwxkk......";

// ---- 12: AEGIS CHARM (round shield medallion)
static const char kArtAegis[] =
    ".........kk........."
    "........kyyk........"
    ".......kkyykk......."
    "......kyyyyyyk......"
    ".....kywwhhwwyk....."
    ".....kywwhhwwyk....."
    ".....kywhhhhwyk....."
    ".....kywwhhwwyk....."
    ".....kkywwwwykk....."
    ".....kkkywwykkk.....";

static const char *const kWpnArt[NUM_SLOT] =
{
    kArtRay, kArtPrism, kArtLance, kArtStorm, kArtSun,
    kArtFire, kArtFrost, kArtVenom, kArtElixir, kArtHaste,
    kArtTome, kArtWarp, kArtAegis
};

// k outline, s/q glove, c/d sleeve, w/x body, g gem, h highlight, f flash, y/r accents
#define WPAL(K,C,D,W,X,G,H,F,Y,R) \
    { {'k',K},{'s',0xE0B890UL},{'q',0x9C7450UL},{'c',C},{'d',D},\
      {'w',W},{'x',X},{'g',G},{'h',H},{'f',F},{'y',Y},{'r',R},{0,0} }

static const PalEnt kWpnPalRay[]   = WPAL(0x0C1420UL,0x305078UL,0x203858UL,0xC8D8E8UL,0x7088A0UL,0x40E0FFUL,0xFFFFFFUL,0x80F0FFUL,0xFFD040UL,0xE04040UL);
static const PalEnt kWpnPalPrism[] = WPAL(0x180C20UL,0x602870UL,0x401850UL,0xF0C048UL,0x946820UL,0xFF60FFUL,0xFFFFFFUL,0x60FFFFUL,0xFFFF60UL,0xFF5050UL);
static const PalEnt kWpnPalLance[] = WPAL(0x201008UL,0x6C4C28UL,0x483018UL,0xB07C3CUL,0x6C4420UL,0xE0E040UL,0xFFFFFFUL,0xFFFF90UL,0xD8E0F0UL,0xE02828UL);
static const PalEnt kWpnPalStorm[] = WPAL(0x101020UL,0x383868UL,0x282848UL,0xA8A8C8UL,0x606084UL,0xA0A0FFUL,0xFFFFFFUL,0x7088FFUL,0xFFFF80UL,0xFF6060UL);
static const PalEnt kWpnPalSun[]   = WPAL(0x200808UL,0x882414UL,0x581008UL,0xE0A030UL,0x946010UL,0xFF9030UL,0xFFE880UL,0xFF5010UL,0xFFF0A0UL,0xFF4020UL);
static const PalEnt kWpnPalFire[]  = WPAL(0x200808UL,0x882414UL,0x581008UL,0xD8E8F0UL,0x8898A8UL,0xFF6020UL,0xFFE080UL,0xFF9020UL,0xFFE040UL,0x9A6A30UL);
static const PalEnt kWpnPalFrost[] = WPAL(0x081420UL,0x305878UL,0x203850UL,0xE0F0FFUL,0x90B0D0UL,0x60D8FFUL,0xFFFFFFUL,0xA0F0FFUL,0xFFFFFFUL,0x60D8FFUL);
static const PalEnt kWpnPalVenom[] = WPAL(0x081008UL,0x285028UL,0x183018UL,0xD8E8F0UL,0x8898A8UL,0x50E040UL,0xB8FF80UL,0x80FF60UL,0xE8E4D0UL,0xFF3030UL);
static const PalEnt kWpnPalElixir[]= WPAL(0x200810UL,0x703050UL,0x481830UL,0xD8E8F0UL,0x8898A8UL,0xE02050UL,0xFF90B0UL,0xFF60A0UL,0x9A6A30UL,0xFFD040UL);
static const PalEnt kWpnPalHaste[] = WPAL(0x201808UL,0x887020UL,0x584810UL,0xD8E8F0UL,0x8898A8UL,0xFFD030UL,0xFFFFA0UL,0xFFFF60UL,0xFFFFFFUL,0xFFB020UL);
static const PalEnt kWpnPalTome[]  = WPAL(0x100820UL,0x482878UL,0x301850UL,0x6038A8UL,0x402070UL,0xC070FFUL,0xE8E0C8UL,0xFFFFFFUL,0xFFD040UL,0xC070FFUL);
static const PalEnt kWpnPalWarp[]  = WPAL(0x082018UL,0x1E5048UL,0x143830UL,0x30B090UL,0x1A6A58UL,0x40FFC0UL,0xFFFFFFUL,0x80FFE0UL,0xFFFFFFUL,0x40FFC0UL);
static const PalEnt kWpnPalAegis[] = WPAL(0x101028UL,0x28407AUL,0x182850UL,0x4070D0UL,0x2A4890UL,0x70A0FFUL,0xFFFFFFUL,0xB0D0FFUL,0xFFD040UL,0xFFD040UL);

static const PalEnt *const kWpnPal[NUM_SLOT] =
{
    kWpnPalRay, kWpnPalPrism, kWpnPalLance, kWpnPalStorm, kWpnPalSun,
    kWpnPalFire, kWpnPalFrost, kWpnPalVenom, kWpnPalElixir, kWpnPalHaste,
    kWpnPalTome, kWpnPalWarp, kWpnPalAegis
};


//*****************************************************************************
// Tables (built once, in RAM)
//*****************************************************************************
#define PAL_N 8                                 // sprite palette letters a..h

static int16_t  s_sin[RC_ANGLES];               // Q14. cos(a) = s_sin[a + 256]
static uint16_t s_dx[RC_ANGLES];                // 256/|cos| (Q8, clamped)
static uint16_t s_dy[RC_ANGLES];                // 256/|sin|
static int16_t  s_colOff[RC_COLS];              // ray angle offset per column
static int16_t  s_colCos[RC_COLS];              // Q14 fisheye correction
static int16_t  s_atan[257];                    // atan(i/256) in angle units
static uint32_t s_wallCol[TILE_KINDS][4][2];    // [tile][shade][side]
static uint32_t s_pal[PT_COUNT][PAL_N];         // [palette][letter - 'a']
// Sprite columns as vertical runs, packed: y0 | y1 << 4 | colour << 8
static uint16_t s_run[SH_COUNT][SPR_N][SPR_MAXRUNS];
static uint16_t s_runCnt[SH_COUNT][SPR_N];
static bool     s_tablesReady = false;

#define RUN_Y0(v)   ((v) & 15u)
#define RUN_Y1(v)   (((v) >> 4) & 15u)
#define RUN_CI(v)   ((v) >> 8)

#define SIN(a)  s_sin[(uint16_t)(a) & RC_ANGLE_MASK]
#define COS(a)  s_sin[((uint16_t)(a) + 256u) & RC_ANGLE_MASK]

//*****************************************************************************
// State
//*****************************************************************************
// Player
static int16_t  s_px, s_py;                     // Q10
static int16_t  s_ang;                          // 0..1023
static uint16_t s_hp, s_maxHp, s_gold;
static uint16_t s_invuln, s_kills;
static uint16_t s_lvDmg, s_lvRate, s_lvSpeed, s_lvGold;
static uint16_t s_traits, s_staffOwned, s_itemOwned, s_equip;
static uint16_t s_cd[NUM_SLOT], s_cdMax[NUM_SLOT];  // per-slot cooldown (frames)
static uint16_t s_haste, s_aegis;               // buff frames left
static uint16_t s_xp, s_xpLv, s_lvPending;      // XP bar, player level, unspent level-ups

// Level
static uint16_t s_level;                        // 0 = first room (endless)
static uint16_t s_lastMap = 0xFFFFu;
static LevelDef s_lv;
static uint16_t s_tile[RC_MAP_CELLS];
static int16_t  s_spX[RC_MAX_SPAWNS], s_spY[RC_MAX_SPAWNS];
static uint16_t s_spCount;
static int16_t  s_doorX, s_doorY;               // tile coordinates of the door
static bool     s_doorOpen;
static uint16_t s_portalTick, s_portalPhase;
static Enemy    s_en[RC_MAX_ENEMIES];
static uint16_t s_alive, s_toSpawn, s_spawnTimer, s_spawnIdx;
static uint16_t s_bossSpawned, s_bossAlive;

// Thrown flasks, clouds, sparks and enemy laser bolts
typedef struct
{
    int16_t  x, y, vx, vy;  // Q10
    int16_t  z;             // sprite lift, Q4 fraction of wall height
    uint16_t kind, pal, sc, life, age, aux, alive;
} Obj;
enum { OB_FLASK, OB_BOLT, OB_CLOUD, OB_SPARK };
static Obj      s_obj[RC_MAX_OBJ];

// Flow
static uint16_t s_mode, s_timer;
static int16_t  s_stX, s_stY;                   // stick flick state

// Effects (staff beams)
static uint16_t s_fxTimer, s_fxN;
static int16_t  s_fxX[5];
static uint32_t s_fxColor;

// Held item art (static; only columns something changed under are redrawn)
typedef struct { int16_t x, y, w, h; uint16_t ci; } WpnRect;
static WpnRect  s_wRect[WPN_RECT_MAX];
static uint16_t s_wCnt;
static uint32_t s_wPal[26];
static uint32_t s_wCov[WPN_H];                  // build scratch
static uint16_t s_wpnColDirty[RC_COLS];         // columns whose art pixels need redrawing
static uint16_t s_wLoaded = 0xFFFFu;            // slot the rects were built for
static uint16_t s_stepAcc, s_stepBlock, s_stepAlt;   // footsteps: distance walked, sfx guard, foot
static uint16_t s_sfxHold;                      // frames an important sfx is protected
static int16_t  s_cdShown = -1;                 // cooldown strip pixels currently on screen
#define WPN_X0   (RC_VIEW_X0 + WPN_XOFF)
#define WPN_C0   (((WPN_X0) - RC_VIEW_X0) / RC_COL_W)
#define WPN_C1   (((WPN_X0 + WPN_W * WPN_SCALE - 1) - RC_VIEW_X0) / RC_COL_W)
#define WPN_TIP_X (WPN_X0 + 10 * WPN_SCALE)     // shaft is columns 9-10

// Draw state
static bool     s_dirtyFull, s_invalidate, s_bannerDraw;
static uint16_t s_hud, s_banner, s_viewDirty;
static int16_t  s_cTop[RC_COLS], s_cBot[RC_COLS];       // what is on the LCD now
static uint32_t s_cCol[RC_COLS];
static int16_t  s_rngT[RC_COLS], s_rngB[RC_COLS];       // overlay rows drawn this frame (T > B = none)
static int16_t  s_prvT[RC_COLS], s_prvB[RC_COLS];       // overlay rows drawn last frame

// Level-up cards
#define CARD_NONE   0xFFFFu
typedef struct { uint16_t off, sold; } Card;
static Card     s_card[3];
static uint16_t s_cardSel;                      // 0..2 = card, 3 = reroll button
static uint16_t s_cardDirty, s_cardMsg, s_rerolls;
#define CD_REROLL   0x08
#define CD_GOLD     0x10
#define CD_MSG      0x20
#define CD_ALL      0x3F
static const char *const kMsgText[5] =
    { "", "Bought!", "Not enough gold", "Unavailable", "Rerolled" };

// Sprite draw list
typedef struct
{
    int16_t  x0;            // left edge in screen pixels
    int16_t  top;           // top row inside the view (can be negative)
    uint16_t size;          // sprite height/width in pixels
    uint16_t c0, c1;        // first / last screen column it touches
    uint16_t dist;          // forward distance, Q8
    uint16_t uStep;         // 16/size in Q8
    uint16_t shape, type, flash;    // type = palette, flash = tint index
} Spr;

#define RC_SPR_CAP  (RC_MAX_ENEMIES + RC_MAX_OBJ)
static Spr      s_spr[RC_SPR_CAP];
static uint16_t s_nSpr;
static int16_t  s_projC, s_projS;

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

// Centered text. `small` uses the 12 px font (card text, top bar).
static void DrawTextEx(const char *s, int16_t x, int16_t y, uint32_t fg,
                       uint32_t bg, bool small)
{
    int16_t len;

    if(s == NULL) return;
    len = TextLength(s);
    if(len == 0) return;

    if(small) Graphics_setFont(&g_sContext, &g_sFontCmss12b);
    Graphics_setForegroundColor(&g_sContext, fg);
    Graphics_setBackgroundColor(&g_sContext, bg);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)s, len,
                                x, y, OPAQUE_TEXT);
    if(small) Graphics_setFont(&g_sContext, &g_sFontCmss20b);
}

static void DrawText(const char *s, int16_t x, int16_t y, uint32_t color)
{
    DrawTextEx(s, x, y, color, C_BLACK, false);
}

static void DrawSmall(const char *s, int16_t x, int16_t y, uint32_t fg, uint32_t bg)
{
    DrawTextEx(s, x, y, fg, bg, true);
}

static void Sfx(const Song *s)
{
    s_sfxHold = 10;                             // staff shots wait for this to finish
    s_stepBlock = 10;                           // so do footsteps
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

    for(i = 0; i < PAL_N; i++) s_pal[type][i] = 0xFF00FFUL;    // magenta = missing
    for(i = 0; list[i].ch; i++)
        if((uint16_t)(list[i].ch - 'a') < PAL_N)
            s_pal[type][list[i].ch - 'a'] = list[i].rgb;
}

// The portal pulses between two colours; only its 8 entries ever change.
static void SetPortalPhase(uint16_t ph)
{
    uint32_t base = ph ? PORTAL_ALT : kWallBase[TILE_PORTAL];
    uint16_t sh;

    for(sh = 0; sh < 4; sh++)
    {
        s_wallCol[TILE_PORTAL][sh][0] = ScaleRgb(base, kShadeMul[sh]);
        s_wallCol[TILE_PORTAL][sh][1] = ScaleRgb(s_wallCol[TILE_PORTAL][sh][0], 200);
    }
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

    // Wall colours: tiles 1..5, 4 distance shades, 2 sides (side 1 is darker).
    for(i = 1; i < TILE_KINDS; i++)
        for(sh = 0; sh < 4; sh++)
        {
            s_wallCol[i][sh][0] = ScaleRgb(kWallBase[i], kShadeMul[sh]);
            s_wallCol[i][sh][1] = ScaleRgb(s_wallCol[i][sh][0], 200);
        }

    for(i = 0; i < PT_COUNT; i++) SetPal(i, kPal[i]);

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
                    s_run[sh][x][n] = (uint16_t)(y | (y1 << 4) |
                                      ((uint16_t)(ch - 'a') << 8));
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
// Walls block; an OPEN door (portal) does not, so you can walk into it.
static bool Solid(int16_t tx, int16_t ty)
{
    uint16_t t;

    if((uint16_t)tx >= RC_MAP_W || (uint16_t)ty >= RC_MAP_H) return true;
    t = s_tile[((uint16_t)ty << RC_MAP_SHIFT) + (uint16_t)tx];
    return (t != 0) && (t != TILE_PORTAL);
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

// flip = mirror the room left/right (more variety from the same six maps)
static void LoadMap(uint16_t m, bool flip)
{
    uint16_t x, y, t;

    s_spCount = 0;
    s_doorX = s_doorY = 0;

    for(y = 0; y < RC_MAP_H; y++)
        for(x = 0; x < RC_MAP_W; x++)
        {
            char ch = kMaps[m][y][flip ? (RC_MAP_W - 1 - x) : x];

            t = 0;
            switch(ch)
            {
                case '#': t = 1; break;
                case 'P': t = 2; break;
                case 'B': t = 3; break;
                case 'D':
                    t = TILE_DOOR;
                    s_doorX = (int16_t)x;
                    s_doorY = (int16_t)y;
                    break;
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
//   *tile = wall type hit (1..5), *side = 0 for a x-face, 1 for a y-face
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

    *tile = (hit == 0 || hit >= TILE_KINDS) ? 1u : hit;
    *side = sd;
    return (dist > 65535UL) ? 65535u : (uint16_t)dist;
}

//*****************************************************************************
// Game: flow, player, enemies
//*****************************************************************************
#define RC_BOLT_SPEED   80              // enemy laser bolt, Q10 per frame

static void EnterOver(void);

static bool IsBossLevel(uint16_t lv)
{
    return ((lv + 1u) % RC_BOSS_EVERY) == 0;
}

// Level-up: how much XP the next level needs.
static uint16_t XpNeed(void)
{
    return (uint16_t)(24u + 12u * s_xpLv);
}

static void GainXp(uint16_t amt)
{
    uint16_t need;

    s_xp += amt;
    for(;;)
    {
        need = XpNeed();
        if(s_xp < need) break;
        s_xp -= need;
        s_xpLv++;
        s_lvPending++;
    }
    s_hud |= HUD_XP | HUD_LEVEL;
}

//-----------------------------------------------------------------------------
// Equipment slots
//-----------------------------------------------------------------------------
static bool SlotOwned(uint16_t slot)
{
    if(slot < NUM_STAFF) return (s_staffOwned & (1u << slot)) != 0;
    return (s_itemOwned & (1u << (slot - NUM_STAFF))) != 0;
}

static const char *SlotName(uint16_t slot)
{
    return (slot < NUM_STAFF) ? kStaff[slot].name : kItem[slot - NUM_STAFF].name;
}

static uint32_t SlotColor(uint16_t slot)
{
    return (slot < NUM_STAFF) ? kStaff[slot].color : kItem[slot - NUM_STAFF].color;
}

static uint16_t StaffCooldown(uint16_t st)
{
    int16_t cd = (int16_t)((kStaff[st].cooldown * (100 - 9 * s_lvRate)) / 100);

    if(s_haste) cd >>= 1;
    return (cd < 4) ? 4 : (uint16_t)cd;
}

static void CycleEquip(void)
{
    uint16_t i, slot = s_equip;

    for(i = 1; i < NUM_SLOT; i++)
    {
        slot = (slot + 1u) % NUM_SLOT;
        if(SlotOwned(slot)) break;
    }

    if(slot != s_equip && SlotOwned(slot))
    {
        s_equip = slot;
        s_hud  |= HUD_STAFF;
        s_viewDirty = 2;                // redraw the new item in the hand
        Sfx(&sfx_menu_move);
    }
}

//-----------------------------------------------------------------------------
// Player damage / enemy damage
//-----------------------------------------------------------------------------
static void HurtPlayer(void)
{
    if(s_invuln || s_hp == 0 || s_aegis) return;

    s_hp--;
    s_invuln = (s_traits & (1u << TR_THICK)) ? RC_THICK_FRAMES : RC_INVULN_FRAMES;
    s_hud   |= HUD_HEARTS;
    s_viewDirty = 2;

    if(s_hp == 0) EnterOver();
    else          Sfx(&sfx_hit);
}

static void KillEnemy(Enemy *e)
{
    const EnemyDef *d = &kEnemy[e->type];
    uint16_t i, g, xp;
    bool     summoned = (e->type == ET_SKELETON && e->aux == 1);

    e->alive = 0;
    if(s_alive) s_alive--;

    if(summoned)                                // the Lich's skeletons: tiny reward
    {
        g  = 2;
        xp = 1;
    }
    else
    {
        g  = (uint16_t)(((uint32_t)d->gold * (100u + 20u * s_lvGold)) / 100u);
        xp = (uint16_t)(d->xp + s_level / 4u);
    }

    s_gold += g;
    s_kills++;
    s_hud |= HUD_GOLD | HUD_LEFT;
    GainXp(xp);

    if((s_traits & (1u << TR_VAMP)) && (s_kills % 10u) == 0 && s_hp < s_maxHp)
    {
        s_hp++;
        s_hud |= HUD_HEARTS;
    }

    Sfx(&sfx_explosion);

    if(e->type == ET_LICH)
    {
        s_bossAlive = 0;
        for(i = 0; i < RC_MAX_ENEMIES; i++)         // minions die with their master
            s_en[i].alive = 0;
        s_alive = 0;
    }
}

// quiet = damage-over-time / splash tick: no knockback, no frost, no blip.
static void DamageEnemyEx(Enemy *e, uint16_t dmg, int16_t c, int16_t s, bool quiet)
{
    e->hp   -= (int16_t)dmg;
    e->flash = 3;
    s_hud   |= HUD_LEFT;

    if(e->hp <= 0)
    {
        KillEnemy(e);
        return;
    }

    if(quiet) return;

    if((s_traits & (1u << TR_KNOCK)) && e->type != ET_LICH)
    {
        int16_t nx = e->x + (int16_t)(((int32_t)c * 260L) >> 14);
        int16_t ny = e->y + (int16_t)(((int32_t)s * 260L) >> 14);

        if(!Blocked(nx, e->y, 220)) e->x = nx;
        if(!Blocked(e->x, ny, 220)) e->y = ny;
    }

    if(s_traits & (1u << TR_FROST)) e->slow = 90;

    SfxSoft(&sfx_blip);
}

static void DamageEnemy(Enemy *e, uint16_t dmg, int16_t c, int16_t s)
{
    DamageEnemyEx(e, dmg, c, s, false);
}

// Damage scaled by the Damage stat: base * (4 + level) / 4.
static uint16_t ScaledDmg(uint16_t base)
{
    uint16_t d = (uint16_t)((base * (4u + s_lvDmg) + 2u) >> 2);
    return (d < 1u) ? 1u : d;
}

//-----------------------------------------------------------------------------
// Objects (flasks in flight, clouds, sparks, enemy laser bolts)
//-----------------------------------------------------------------------------
static Obj *NewObj(uint16_t kind)
{
    uint16_t i, pick = 0xFFFFu, oldest = 0;
    Obj     *o;

    for(i = 0; i < RC_MAX_OBJ; i++)
        if(!s_obj[i].alive) { pick = i; break; }

    if(pick == 0xFFFFu)                         // full: recycle a spark or the oldest cloud
    {
        for(i = 0; i < RC_MAX_OBJ; i++)
        {
            if(s_obj[i].kind != OB_SPARK && s_obj[i].kind != OB_CLOUD) continue;
            if(pick == 0xFFFFu || s_obj[i].kind == OB_SPARK ||
               s_obj[i].age > oldest)
            {
                pick   = i;
                oldest = s_obj[i].age;
                if(s_obj[i].kind == OB_SPARK) break;
            }
        }
        if(pick == 0xFFFFu) return NULL;
    }

    o = &s_obj[pick];
    o->x = o->y = o->vx = o->vy = o->z = 0;
    o->kind = kind;
    o->pal  = 0;
    o->sc   = 0;
    o->life = 0;
    o->age  = 0;
    o->aux  = 0;
    o->alive = 1;
    return o;
}

static void AddSpark(int16_t x, int16_t y, int16_t z, uint16_t sc, uint16_t pal, uint16_t life)
{
    Obj *o = NewObj(OB_SPARK);

    if(o == NULL) return;
    o->x = x;  o->y = y;  o->z = z;
    o->sc = sc;  o->pal = pal;  o->life = life;
}

// A flask breaks: splash everything near, leave a puff (venom: a lingering cloud).
static void Shatter(Obj *o)
{
    uint16_t kind = o->aux;
    int16_t  rad  = kPotRadius[kind];
    uint16_t dmg  = ScaledDmg(kPotDmg[kind]);
    uint16_t i;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        Enemy *e = &s_en[i];

        if(!e->alive) continue;
        if(Dist10(e->x - o->x, e->y - o->y) >= rad + (kEnemy[e->type].radius >> 1))
            continue;

        DamageEnemyEx(e, dmg, 0, 0, true);
        if(!e->alive) continue;

        if(kind == PK_FIRE)       e->burn = 90;
        else if(kind == PK_FROST) { e->freeze = 40; e->slow = 220; }
        else                      e->slow = 60;
    }

    o->kind = OB_CLOUD;                         // the flask becomes its own splash cloud
    o->age  = 0;
    o->life = kPotLife[kind];
    o->sc   = kPotScale[kind];
    o->pal  = PT_PUFF_FIRE + kind;
    o->z    = 0;
    o->vx   = o->vy = 0;
    Sfx(&sfx_explosion);
}

static void UpdateObjects(void)
{
    uint16_t i, j;
    int16_t  nx, ny;

    for(i = 0; i < RC_MAX_OBJ; i++)
    {
        Obj *o = &s_obj[i];

        if(!o->alive) continue;

        switch(o->kind)
        {
            case OB_FLASK:
                o->age++;
                nx = o->x + o->vx;
                ny = o->y + o->vy;
                if(Blocked(nx, ny, 60)) { Shatter(o); break; }
                o->x = nx;
                o->y = ny;
                o->z = (int16_t)(9 - (int16_t)((o->age * 7u) / o->life));
                for(j = 0; j < RC_MAX_ENEMIES; j++)
                {
                    if(!s_en[j].alive) continue;
                    if(Dist10(s_en[j].x - o->x, s_en[j].y - o->y) <
                       kEnemy[s_en[j].type].radius + 150)
                    {
                        Shatter(o);
                        break;
                    }
                }
                if(o->kind == OB_FLASK && o->age >= o->life) Shatter(o);
                break;

            case OB_BOLT:                       // enemy red laser: dodge it
                nx = o->x + o->vx;
                ny = o->y + o->vy;
                if(Blocked(nx, ny, 80)) { o->alive = 0; break; }
                o->x = nx;
                o->y = ny;
                if(Dist10(s_px - o->x, s_py - o->y) < 300)
                {
                    o->alive = 0;
                    HurtPlayer();               // (does nothing during invulnerability / Aegis)
                    break;
                }
                if(++o->age >= o->life) o->alive = 0;
                break;

            case OB_CLOUD:
                if(++o->age >= o->life) { o->alive = 0; break; }
                if(o->aux == PK_VENOM && (o->age % 12u) == 0)
                {
                    for(j = 0; j < RC_MAX_ENEMIES; j++)
                    {
                        Enemy *e = &s_en[j];

                        if(!e->alive) continue;
                        if(Dist10(e->x - o->x, e->y - o->y) >=
                           kPotRadius[PK_VENOM] + (kEnemy[e->type].radius >> 1))
                            continue;
                        DamageEnemyEx(e, (uint16_t)(1u + s_lvDmg / 3u), 0, 0, true);
                        if(e->alive) e->slow = 30;
                    }
                }
                break;

            default:                            // OB_SPARK
                if(++o->age >= o->life) o->alive = 0;
                break;
        }
    }
}

//-----------------------------------------------------------------------------
// Staffs: hitscan beams
//-----------------------------------------------------------------------------
static void FireBeam(int16_t angle, uint16_t st)
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

    left = 1u + kStaff[st].pierce + ((s_traits & (1u << TR_PIERCE)) ? 1u : 0u);

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

        dmg = ScaledDmg(kStaff[st].dmg);
        if((s_traits & (1u << TR_LUCKY)) && (Rand16() % 5u) == 0) dmg *= 2u;

        DamageEnemy(&s_en[best], dmg, c, s);
    }
}

static void Fire(void)
{
    uint16_t     st = s_equip;
    const Staff *sp = &kStaff[st];
    uint16_t     i;
    int16_t      off;

    s_cdMax[st] = StaffCooldown(st);
    s_cd[st]    = s_cdMax[st];
    s_fxN       = sp->beams;
    s_fxColor   = sp->color;

    for(i = 0; i < sp->beams; i++)
    {
        off = (int16_t)((((int16_t)(2 * i) - (int16_t)(sp->beams - 1)) * (int16_t)sp->spread) / 2);
        FireBeam(s_ang + off, st);
        s_fxX[i] = (int16_t)(RC_VIEW_X0 + RC_VIEW_W / 2 + off * RC_PX_PER_UNIT);
    }

    s_fxTimer   = 4;
    s_viewDirty = 2;
    if(s_sfxHold == 0) Buzzer_PlaySfx(kStaffSfx[st]);
    s_stepBlock = 5;                            // footsteps never cut a shot off
}

//-----------------------------------------------------------------------------
// Magic items
//-----------------------------------------------------------------------------
static bool ThrowFlask(uint16_t kind)
{
    int16_t c = COS(s_ang);
    int16_t s = SIN(s_ang);
    int16_t sx, sy;
    Obj    *o = NewObj(OB_FLASK);

    if(o == NULL) return false;

    sx = s_px + (int16_t)(((int32_t)c * 300L) >> 14);
    sy = s_py + (int16_t)(((int32_t)s * 300L) >> 14);
    if(Blocked(sx, sy, 60)) { sx = s_px; sy = s_py; }       // nose against a wall

    o->x    = sx;
    o->y    = sy;
    o->vx   = (int16_t)(((int32_t)c * 110L) >> 14);
    o->vy   = (int16_t)(((int32_t)s * 110L) >> 14);
    o->z    = 9;
    o->aux  = kind;
    o->life = 42;
    o->pal  = PT_FLASK_FIRE + kind;
    Buzzer_PlaySfx(&sfx_jump);
    return true;
}

// Nearest visible enemies in front of the player (arc +-90 units, line of sight).
static uint16_t AcquireTargets(uint16_t maxN, uint16_t *out)
{
    int16_t  c = COS(s_ang);
    int16_t  s = SIN(s_ang);
    uint16_t cand[RC_MAX_ENEMIES], candD[RC_MAX_ENEMIES];
    uint16_t nc = 0, n = 0, i, k, tile, side;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        const Enemy *e = &s_en[i];
        int16_t rx, ry, along, sd, ratio, theta;
        uint16_t wd;

        if(!e->alive) continue;
        rx    = e->x - s_px;
        ry    = e->y - s_py;
        along = (int16_t)(((int32_t)rx * c + (int32_t)ry * s) >> 14);
        if(along < 200) continue;

        sd    = (int16_t)(((int32_t)ry * c - (int32_t)rx * s) >> 14);
        ratio = (int16_t)(((int32_t)sd * 256L) / along);
        theta = (ratio < 0) ? -ratio : ratio;
        if(theta > 256) continue;
        theta = s_atan[theta];
        if(theta > 90) continue;
        if(ratio < 0) theta = -theta;

        wd = RC_CastRay(s_ang + theta, &tile, &side);           // Q8
        if(wd < (uint16_t)(along >> 2)) continue;               // a wall is in the way

        cand[nc]  = i;
        candD[nc] = (uint16_t)along;
        nc++;
    }

    while(n < maxN && nc > 0)
    {
        k = 0;
        for(i = 1; i < nc; i++)
            if(candD[i] < candD[k]) k = i;
        out[n++] = cand[k];
        nc--;
        cand[k]  = cand[nc];
        candD[k] = candD[nc];
    }
    return n;
}

static bool CastTome(void)
{
    uint16_t idx[3], n, i;
    uint16_t dmg = ScaledDmg(3);

    n = AcquireTargets(3, idx);
    if(n == 0) { Sfx(&sfx_menu_bump); return false; }

    for(i = 0; i < n; i++)
    {
        Enemy *e = &s_en[idx[i]];

        AddSpark(e->x, e->y, 6, 8, PT_PUFF_ARCANE, 8);
        DamageEnemy(e, dmg, 0, 0);
    }
    Buzzer_PlaySfx(&sfx_staff_prism);
    return true;
}

static void CastWarp(void)
{
    int16_t c = COS(s_ang);
    int16_t s = SIN(s_ang);
    int16_t nx, ny, i;

    for(i = 0; i < 24; i++)                     // up to 24 * 128 = 3 tiles
    {
        nx = s_px + (int16_t)(((int32_t)c * 128L) >> 14);
        ny = s_py + (int16_t)(((int32_t)s * 128L) >> 14);
        if(Blocked(nx, ny, RC_PLAYER_R)) break;
        s_px = nx;
        s_py = ny;
    }

    if(s_invuln < 20) s_invuln = 20;
    s_stepAcc   = 0;
    s_viewDirty = 2;
    Buzzer_PlaySfx(&sfx_staff_storm);
}

static void CastAegis(void)
{
    uint16_t i;

    s_aegis = 150;
    s_hud  |= HUD_HINT;

    for(i = 0; i < RC_MAX_ENEMIES; i++)         // shockwave
    {
        Enemy  *e = &s_en[i];
        int16_t dx, dy, d, nx, ny;

        if(!e->alive) continue;
        dx = e->x - s_px;
        dy = e->y - s_py;
        d  = Dist10(dx, dy);
        if(d >= 2000 || d <= 0) continue;

        nx = e->x + (int16_t)(((int32_t)dx * 600L) / d);
        ny = e->y + (int16_t)(((int32_t)dy * 600L) / d);
        if(!Blocked(nx, e->y, 230)) e->x = nx;
        if(!Blocked(e->x, ny, 230)) e->y = ny;
        e->slow = 60;
    }
    Sfx(&sfx_menu_select);
}

// Returns true if the item was used (so its cooldown starts).
static bool UseItem(uint16_t it)
{
    switch(it)
    {
        case IT_FIRE:
        case IT_FROST:
        case IT_VENOM:
            return ThrowFlask(it);                      // IT_ order == PK_ order

        case IT_ELIXIR:
            if(s_hp >= s_maxHp) { Sfx(&sfx_menu_bump); return false; }
            s_hp += 2;
            if(s_hp > s_maxHp) s_hp = s_maxHp;
            s_hud |= HUD_HEARTS;
            Sfx(&sfx_powerup);
            return true;

        case IT_HASTE:
            s_haste = 300;
            s_hud  |= HUD_HINT;
            Sfx(&sfx_powerup);
            return true;

        case IT_TOME:
            return CastTome();

        case IT_WARP:
            CastWarp();
            return true;

        default:                                        // IT_AEGIS
            CastAegis();
            return true;
    }
}

static void UseEquipped(void)
{
    uint16_t slot = s_equip;
    uint16_t it;

    if(s_cd[slot]) return;

    if(slot < NUM_STAFF)
    {
        Fire();
        return;
    }

    it = slot - NUM_STAFF;
    if(UseItem(it))
    {
        s_cdMax[slot] = kItem[it].cooldown;
        s_cd[slot]    = kItem[it].cooldown;
    }
    else
        s_cd[slot] = 10;                                // brief retry delay
    s_viewDirty = 2;
}

//-----------------------------------------------------------------------------
// Spawning
//-----------------------------------------------------------------------------
static Enemy *SpawnEnemy(uint16_t type, int16_t x, int16_t y)
{
    uint16_t i;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        if(!s_en[i].alive)
        {
            int16_t hp = kEnemy[type].hp;

            if(type == ET_LICH)                         // each Lich is tougher than the last
                hp = (int16_t)(hp + 50 * (int16_t)((s_level + 1u) / RC_BOSS_EVERY - 1u));
            else                                        // later rooms are tougher
                hp = (int16_t)(((int32_t)hp * (8 + s_level)) / 8);

            s_en[i].x = x;      s_en[i].y = y;
            s_en[i].hp = hp;    s_en[i].type = type;
            s_en[i].flash = 0;  s_en[i].slow = 0;
            s_en[i].freeze = 0; s_en[i].burn = 0;
            s_en[i].aux = 0;    s_en[i].aux2 = 0;
            s_en[i].alive = 1;
            s_alive++;
            s_hud |= HUD_LEFT;
            return &s_en[i];
        }
    }
    return NULL;
}

static uint16_t CountType(uint16_t type)
{
    uint16_t i, n = 0;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
        if(s_en[i].alive && s_en[i].type == type) n++;
    return n;
}

static void TrySpawn(void)
{
    uint16_t tries, idx = 0, type, r, gap;
    uint16_t behCap;

    if(s_spawnTimer) { s_spawnTimer--; return; }
    if(!s_toSpawn || s_alive >= s_lv.maxAlive || s_spCount == 0) return;

    // First marker that is not right on top of the player.
    for(tries = 0; tries < s_spCount; tries++)
    {
        idx = s_spawnIdx;
        s_spawnIdx = (s_spawnIdx + 1u) % s_spCount;
        if(Dist10(s_spX[idx] - s_px, s_spY[idx] - s_py) > 3072) break;
    }

    gap = s_lv.gap;

    if(s_lv.boss)
    {
        type = ET_LICH;                                 // the Lich, then only its skeletons
        s_bossSpawned = 1;
        s_bossAlive   = 1;
        s_toSpawn     = 1;                              // (decremented below -> 0)
    }
    else
    {
        behCap = 1u + s_level / 6u;
        if(behCap > 3u) behCap = 3u;

        r    = Rand16() % 100u;
        type = ET_GOBLIN;
        if(r < s_lv.pBeh)
        {
            if(CountType(ET_BEHOLDER) < behCap) type = ET_BEHOLDER;
        }
        else if(r < s_lv.pBeh + s_lv.pBrute)                         type = ET_HOBGOB;
        else if(r < s_lv.pBeh + s_lv.pBrute + s_lv.pBat)             type = ET_BAT;
        else if(r < s_lv.pBeh + s_lv.pBrute + s_lv.pBat + s_lv.pSkel) type = ET_SKELETON;
    }

    SpawnEnemy(type, s_spX[idx], s_spY[idx]);
    s_toSpawn--;
    s_spawnTimer = gap;
}

//-----------------------------------------------------------------------------
// Enemy behaviour
//-----------------------------------------------------------------------------
static uint16_t BeholderCadence(void)
{
    uint16_t cut = (uint16_t)(2u * s_level);

    if(cut > 45u) cut = 45u;
    return (uint16_t)(100u - cut);                      // frames between shots
}

// Beholders aim at where you ARE: strafe and the red lasers miss.
static void BeholderShoot(const Enemy *e)
{
    int16_t dx   = s_px - e->x;
    int16_t dy   = s_py - e->y;
    int16_t dist = Dist10(dx, dy);
    int16_t vx, vy, k, kmax;

    if(dist < 400) return;

    vx = (int16_t)(((int32_t)dx * RC_BOLT_SPEED) / dist);
    vy = (int16_t)(((int32_t)dy * RC_BOLT_SPEED) / dist);
    kmax = (s_level >= 8) ? 1 : 0;                      // deeper: three-way fan

    for(k = -kmax; k <= kmax; k++)
    {
        Obj *o = NewObj(OB_BOLT);

        if(o == NULL) break;
        o->x    = e->x;
        o->y    = e->y;
        o->vx   = (int16_t)(vx - (int16_t)(((int32_t)vy * k * 56L) >> 8));
        o->vy   = (int16_t)(vy + (int16_t)(((int32_t)vx * k * 56L) >> 8));
        o->z    = 6;
        o->life = 130;
        o->pal  = PT_BOLT;
    }
    SfxSoft(&sfx_laser);
}

static void LichSummon(const Enemy *lich, uint16_t n)
{
    uint16_t k;

    for(k = 0; k < n; k++)
    {
        int16_t sx = lich->x + ((k == 0) ? 520 : (k == 1) ? -520 : 0);
        int16_t sy = lich->y + ((k == 2) ? 520 : 0);
        Enemy  *p;

        if(s_alive >= RC_MAX_ENEMIES - 1) break;
        if(Blocked(sx, sy, 230)) { sx = lich->x; sy = lich->y; }

        p = SpawnEnemy(ET_SKELETON, sx, sy);
        if(p == NULL) break;
        p->aux = 1;                                     // marks "summoned"
        AddSpark(sx, sy, 6, 10, PT_PUFF_ARCANE, 10);
    }
    SfxSoft(&sfx_powerup);
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

        if(e->burn)                                     // burning: 1 damage every 16 frames
        {
            e->burn--;
            if((e->burn & 15u) == 0)
            {
                DamageEnemyEx(e, 1, 0, 0, true);
                if(!e->alive) continue;
            }
        }

        if(e->freeze) { e->freeze--; continue; }        // frozen solid: no move, no attack

        sp = d->speed;
        if(e->slow) { e->slow--; sp >>= 1; }

        if(e->type == ET_LICH)
        {
            bool angry = (e->hp < RC_BOSS_HP / 2);

            e->aux++;
            if(angry) sp += (sp >> 2);
            if(e->aux >= 220)                           // charge!
            {
                sp *= 3;
                if(e->aux >= 265) e->aux = 0;
            }

            e->aux2++;
            if(e->aux2 >= (angry ? 110u : 170u))        // summon skeletons
            {
                e->aux2 = 0;
                LichSummon(e, angry ? 3u : 2u);
            }
        }
        else if(e->type == ET_BEHOLDER)
        {
            e->aux++;
            if(e->aux >= BeholderCadence())
            {
                e->aux = 0;
                BeholderShoot(e);
            }
        }

        dx   = s_px - e->x;
        dy   = s_py - e->y;
        dist = Dist10(dx, dy);

        if(e->type == ET_BEHOLDER)
        {
            // Keeps 2.6 - 3.8 tiles away: closes in, backs off, or drifts sideways.
            if(dist > 0)
            {
                if(dist > 3800)
                {
                    mx = (int16_t)(((int32_t)dx * sp) / dist);
                    my = (int16_t)(((int32_t)dy * sp) / dist);
                }
                else if(dist < 2600)
                {
                    mx = (int16_t)(-(((int32_t)dx * (sp + (sp >> 1))) / dist));
                    my = (int16_t)(-(((int32_t)dy * (sp + (sp >> 1))) / dist));
                }
                else
                {
                    int16_t sgn = (i & 1u) ? 1 : -1;

                    mx = (int16_t)(sgn * (((int32_t)-dy * (sp >> 1)) / dist));
                    my = (int16_t)(sgn * (((int32_t) dx * (sp >> 1)) / dist));
                }

                nx = e->x + mx;
                if(!Blocked(nx, e->y, 230)) e->x = nx;
                ny = e->y + my;
                if(!Blocked(e->x, ny, 230)) e->y = ny;
            }
        }
        else if(dist > 200)
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
}

// Bottom line: what is active, else the controls.
static void DrawHint(void)
{
    Graphics_FillRect(0, 217, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);
    if(s_aegis)      DrawText("SHIELD UP!", SCREEN_W / 2, 229, 0x70A0FFUL);
    else if(s_haste) DrawText("HASTE!", SCREEN_W / 2, 229, C_GOLD);
    else             DrawText("U:use  D:swap  L/R:turn", SCREEN_W / 2, 229, C_DKGRAY);
}

#define XP_BAR_X0   114
#define XP_BAR_X1   317
#define XP_BAR_Y0   16
#define XP_BAR_Y1   21

static void DrawHud(void)
{
    char    buf[24];
    int16_t n, i;

    if(s_hud & HUD_HEARTS)
    {
        Graphics_FillRect(0, 0, 111, RC_VIEW_Y0 - 1, C_BLACK);
        for(i = 0; i < (int16_t)s_maxHp; i++)
            DrawHeart(5 + i * 18, 6, (i < (int16_t)s_hp) ? C_RED : C_DKGRAY);
    }

    if(s_hud & HUD_TOPR)                    // room, player level, gold, XP bar
    {
        int16_t w;

        Graphics_FillRect(112, 0, SCREEN_W - 1, RC_VIEW_Y0 - 1, C_BLACK);

        if(IsBossLevel(s_level))
            DrawSmall("BOSS", 140, 8, C_RED, C_BLACK);
        else
        {
            n  = Cat(buf, 0, "ROOM ", 5);
            Itoa(buf + n, s_level + 1);
            DrawSmall(buf, 140, 8, C_WHITE, C_BLACK);
        }

        n  = Cat(buf, 0, "LV ", 3);
        Itoa(buf + n, s_xpLv);
        DrawSmall(buf, 207, 8, C_XP, C_BLACK);

        n  = Cat(buf, 0, "G ", 2);
        Itoa(buf + n, s_gold);
        DrawSmall(buf, 280, 8, C_GOLD, C_BLACK);

        Graphics_FillRect(XP_BAR_X0, XP_BAR_Y0, XP_BAR_X1, XP_BAR_Y1, C_DKGRAY);
        Graphics_FillRect(XP_BAR_X0 + 1, XP_BAR_Y0 + 1, XP_BAR_X1 - 1, XP_BAR_Y1 - 1, 0x180830UL);
        w = (int16_t)(((int32_t)s_xp * (XP_BAR_X1 - XP_BAR_X0 - 1)) / XpNeed());
        if(w > 0)
            Graphics_FillRect(XP_BAR_X0 + 1, XP_BAR_Y0 + 1, XP_BAR_X0 + w,
                              XP_BAR_Y1 - 1, C_XP);
    }

    if(s_hud & HUD_STAFF)
    {
        Graphics_FillRect(0, RC_VIEW_Y1 + 2, 189, 215, C_BLACK);
        DrawText(SlotName(s_equip), 95, 203, SlotColor(s_equip));
        s_cdShown = -1;                     // redraw the cooldown strip too
    }

    if(s_hud & HUD_LEFT)
    {
        Graphics_FillRect(190, RC_VIEW_Y1 + 2, SCREEN_W - 1, 215, C_BLACK);
        if(s_doorOpen)
        {
            DrawText("DOOR OPEN", 255, 203, C_GREEN);
        }
        else
        {
            if(s_lv.boss)
            {
                int16_t hp = 0;
                for(i = 0; i < RC_MAX_ENEMIES; i++)
                    if(s_en[i].alive && s_en[i].type == ET_LICH) hp = s_en[i].hp;
                n  = Cat(buf, 0, "Boss HP ", 8);
                Itoa(buf + n, (uint16_t)((hp > 0) ? hp : 0));
            }
            else
            {
                n  = Cat(buf, 0, "Left ", 5);
                Itoa(buf + n, s_toSpawn + s_alive);
            }
            DrawText(buf, 255, 203, C_WHITE);
        }
    }

    if(s_hud & HUD_HINT) DrawHint();

    s_hud = 0;
}

// Thin strip under the item name: fills as the equipped slot's cooldown ends.
#define CD_STRIP_X0   20
#define CD_STRIP_W    150
static void DrawCdStrip(void)
{
    uint16_t cd = s_cd[s_equip];
    uint16_t mx = s_cdMax[s_equip];
    int16_t  w;

    if(cd == 0 || mx == 0) w = CD_STRIP_W;
    else
    {
        w = (int16_t)(CD_STRIP_W - (int16_t)(((int32_t)cd * CD_STRIP_W) / mx));
        if(w < 0) w = 0;
    }

    if(w == s_cdShown) return;
    s_cdShown = w;

    Graphics_FillRect(CD_STRIP_X0, 213, CD_STRIP_X0 + CD_STRIP_W - 1, 215, C_DKGRAY);
    if(w > 0)
        Graphics_FillRect(CD_STRIP_X0, 213, CD_STRIP_X0 + w - 1, 215,
                          (w == CD_STRIP_W) ? SlotColor(s_equip) : C_GRAY);
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

// Project one world point into the sprite list (sorted far to near).
static void ProjectOne(int16_t wx, int16_t wy, uint16_t shape, uint16_t pal,
                       uint16_t scale, uint16_t zoff, uint16_t tint)
{
    Spr     *sp;
    int16_t  rx, ry, along, side, ratio, theta, cx, x0, x1, lo, hi;
    uint16_t perp8, lineH, size, k;
    int16_t  floorY, bottom;

    if(s_nSpr >= RC_SPR_CAP) return;

    rx    = wx - s_px;
    ry    = wy - s_py;
    along = (int16_t)(((int32_t)rx * s_projC + (int32_t)ry * s_projS) >> 14);
    if(along < 150) return;                                 // behind / too close

    side  = (int16_t)(((int32_t)ry * s_projC - (int32_t)rx * s_projS) >> 14);
    ratio = (int16_t)(((int32_t)side * 256L) / along);
    theta = (ratio < 0) ? -ratio : ratio;
    if(theta > 256) return;                                 // way outside the view
    theta = s_atan[theta];
    if(ratio < 0) theta = -theta;

    cx = (int16_t)(RC_VIEW_X0 + RC_VIEW_W / 2 + theta * RC_PX_PER_UNIT);

    perp8 = (uint16_t)(along >> 2);
    if(perp8 < RC_MIN_DIST) perp8 = RC_MIN_DIST;
    lineH = RC_PROJ / perp8;                                // wall height here
    size  = (uint16_t)(((uint32_t)lineH * scale) >> 4);
    if(size > 460) size = 460;
    if(size < 2)   return;

    floorY = (int16_t)(RC_HORIZON + (lineH >> 1));
    bottom = (int16_t)(floorY - (int16_t)(((uint32_t)lineH * zoff) >> 4));

    x0 = cx - (int16_t)(size >> 1);
    x1 = x0 + (int16_t)size - 1;
    if(x1 < RC_VIEW_X0 || x0 >= RC_VIEW_X0 + RC_VIEW_W) return;

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
    sp->shape = shape;
    sp->type  = pal;
    sp->flash = tint;

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

// Turn every enemy and object into a screen-space sprite.
static void ProjectSprites(void)
{
    uint16_t i;

    s_nSpr  = 0;
    s_projC = COS(s_ang);
    s_projS = SIN(s_ang);

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        const Enemy    *e = &s_en[i];
        const EnemyDef *d;
        uint16_t        pal, tint;

        if(!e->alive) continue;
        d = &kEnemy[e->type];

        pal = e->type;                                  // PT_ order matches ET_ order
        if(e->type == ET_BEHOLDER && e->aux + 25u >= BeholderCadence())
            pal = PT_BEHOLDER_HOT;                      // eye glows just before it fires

        tint = e->flash ? 1u : e->freeze ? 3u : (e->burn && (e->burn & 8u)) ? 2u : 0u;

        ProjectOne(e->x, e->y, d->shape, pal, d->scale, d->zoff, tint);
    }

    for(i = 0; i < RC_MAX_OBJ; i++)
    {
        const Obj *o = &s_obj[i];

        if(!o->alive) continue;

        switch(o->kind)
        {
            case OB_FLASK:
                ProjectOne(o->x, o->y, SH_FLASK, o->pal, 4, (uint16_t)o->z, 0);
                break;
            case OB_BOLT:
                ProjectOne(o->x, o->y, SH_BOLT, PT_BOLT, 3, (uint16_t)o->z, 0);
                break;
            default:                                    // cloud / spark
                ProjectOne(o->x, o->y, SH_PUFF, o->pal,
                           (uint16_t)(o->sc + ((o->age >> 1) & 3u)),
                           (uint16_t)o->z, 0);
                break;
        }
    }
}

static void DrawSlice(const Spr *sp, uint16_t c)
{
    int16_t  xl = (int16_t)(RC_VIEW_X0 + (int16_t)c * RC_COL_W);
    int16_t  xc = xl + (RC_COL_W >> 1) - sp->x0;
    uint16_t u, n, r, v;

    if(xc < 0) xc = 0;
    u = (uint16_t)(((int32_t)xc * sp->uStep) >> 8);
    if(u > SPR_N - 1) u = SPR_N - 1;

    n = s_runCnt[sp->shape][u];

    for(r = 0; r < n; r++)
    {
        int16_t y0, y1;

        v  = s_run[sp->shape][u][r];
        y0 = (int16_t)(sp->top + (int16_t)(((int32_t)RUN_Y0(v) * sp->size) >> 4));
        y1 = (int16_t)(sp->top + (int16_t)(((int32_t)(RUN_Y1(v) + 1) * sp->size) >> 4) - 1);

        if(y0 < 0) y0 = 0;
        if(y1 > RC_VIEW_H - 1) y1 = RC_VIEW_H - 1;
        if(y1 < y0) continue;

        Graphics_FillRect(xl, RC_VIEW_Y0 + y0, xl + RC_COL_W - 1, RC_VIEW_Y0 + y1,
                          sp->flash ? kTint[sp->flash] : s_pal[sp->type][RUN_CI(v)]);
    }
}

// Tell the repair logic that screen rectangle (x0,y0)-(x1,y1) was drawn over.
static void MarkRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t a, b, c;

    if(x0 < RC_VIEW_X0) x0 = RC_VIEW_X0;
    if(x1 > RC_VIEW_X0 + RC_VIEW_W - 1) x1 = RC_VIEW_X0 + RC_VIEW_W - 1;
    if(x1 < x0) return;

    a = (int16_t)((x0 - RC_VIEW_X0) / RC_COL_W);
    b = (int16_t)((x1 - RC_VIEW_X0) / RC_COL_W);
    for(c = a; c <= b; c++)
        AddRange(c, (int16_t)(y0 - RC_VIEW_Y0), (int16_t)(y1 - RC_VIEW_Y0));
}

//*****************************************************************************
// Held item: build once per equip, draw per screen column
//*****************************************************************************
static char WpnCell(const char *art, uint16_t x, uint16_t y)
{
    if(y < WPN_ART_ROWS) return art[y * WPN_W + x];
    return kWpnArm[(y - WPN_ART_ROWS) * WPN_W + x];
}

static void WpnBuild(uint16_t slot)
{
    uint16_t x, y, w, h, xx, yy, n, i;
    char     ch;
    bool     ok;
    const char *art = kWpnArt[slot];

    for(i = 0; i < 26; i++) s_wPal[i] = 0xFF00FFUL;
    for(i = 0; kWpnPal[slot][i].ch; i++)
        s_wPal[kWpnPal[slot][i].ch - 'a'] = kWpnPal[slot][i].rgb;

    for(y = 0; y < WPN_H; y++) s_wCov[y] = 0;
    n = 0;

    for(y = 0; y < WPN_H; y++)
    {
        for(x = 0; x < WPN_W; x++)
        {
            ch = WpnCell(art, x, y);
            if(ch == '.' || (s_wCov[y] & (1UL << x))) continue;

            w = 1;
            while(x + w < WPN_W && WpnCell(art, x + w, y) == ch &&
                  !(s_wCov[y] & (1UL << (x + w)))) w++;

            h = 1;
            while(y + h < WPN_H)
            {
                ok = true;
                for(xx = x; xx < x + w; xx++)
                    if(WpnCell(art, xx, y + h) != ch ||
                       (s_wCov[y + h] & (1UL << xx))) { ok = false; break; }
                if(!ok) break;
                h++;
            }

            for(yy = y; yy < y + h; yy++)
                for(xx = x; xx < x + w; xx++)
                    s_wCov[yy] |= (1UL << xx);

            if(n < WPN_RECT_MAX)
            {
                s_wRect[n].x = (int16_t)x;  s_wRect[n].y = (int16_t)y;
                s_wRect[n].w = (int16_t)w;  s_wRect[n].h = (int16_t)h;
                s_wRect[n].ci = (uint16_t)(ch - 'a');
                n++;
            }
        }
    }
    s_wCnt    = n;
    s_wLoaded = slot;
}

// Draw the part of the held item that lies inside screen column c.
static void DrawStaffCol(uint16_t c)
{
    int16_t  cx0 = (int16_t)(RC_VIEW_X0 + (int16_t)c * RC_COL_W);
    int16_t  cx1 = cx0 + RC_COL_W - 1;
    int16_t  sx, ex, sy, ey;
    uint16_t i;
    const WpnRect *r;

    for(i = 0; i < s_wCnt; i++)
    {
        r  = &s_wRect[i];
        sx = (int16_t)(WPN_X0 + r->x * WPN_SCALE);
        ex = (int16_t)(sx + r->w * WPN_SCALE - 1);
        if(ex < cx0 || sx > cx1) continue;
        if(sx < cx0) sx = cx0;
        if(ex > cx1) ex = cx1;

        sy = (int16_t)(WPN_Y0 + r->y * WPN_SCALE);
        ey = (int16_t)(sy + r->h * WPN_SCALE - 1);
        Graphics_FillRect(sx, RC_VIEW_Y0 + sy, ex, RC_VIEW_Y0 + ey, s_wPal[r->ci]);
    }
}

//-----------------------------------------------------------------------------
// Staff beam animation. Cheap on purpose: a bolt of 3 small dots travels from
// the staff tip to the crosshair over 3 frames, then a small impact burst.
// No lines, and only the few rows these rectangles touch are repaired.
//-----------------------------------------------------------------------------
static void FxDot(int16_t x, int16_t y, uint32_t color)
{
    Graphics_FillRect(x - 2, y - 2, x + 2, y + 2, color);
    Graphics_FillRect(x - 1, y - 1, x + 1, y + 1, C_WHITE);
    MarkRect(x - 2, y - 2, x + 2, y + 2);
}

static void DrawFx(void)
{
    int16_t  tx = WPN_TIP_X;
    int16_t  ty = (int16_t)(RC_VIEW_Y0 + WPN_Y0 + WPN_TIP_ROW * WPN_SCALE);
    int16_t  cy = (int16_t)(RC_VIEW_Y0 + RC_HORIZON);
    uint16_t b, k, t0, t1, t;
    int16_t  ex, px, py;

    if(s_fxTimer == 4)                              // muzzle flash
    {
        Graphics_FillRect(tx - 4, ty - 4, tx + 4, ty + 4, s_fxColor);
        Graphics_FillRect(tx - 2, ty - 2, tx + 2, ty + 2, C_WHITE);
        MarkRect(tx - 4, ty - 4, tx + 4, ty + 4);
    }

    for(b = 0; b < s_fxN; b++)
    {
        ex = s_fxX[b];

        if(s_fxTimer >= 2)                          // bolt in flight
        {
            t0 = (s_fxTimer == 4) ?   0u : (s_fxTimer == 3) ?  80u : 170u;
            t1 = (s_fxTimer == 4) ? 100u : (s_fxTimer == 3) ? 190u : 256u;

            for(k = 0; k < 3; k++)
            {
                t  = t0 + ((t1 - t0) * k) / 2u;
                px = (int16_t)(tx + (int16_t)(((int32_t)(ex - tx) * (int32_t)t) >> 8));
                py = (int16_t)(ty + (int16_t)(((int32_t)(cy - ty) * (int32_t)t) >> 8));
                FxDot(px, py, s_fxColor);
            }
        }

        if(s_fxTimer <= 2)                          // impact burst
        {
            int16_t r = (s_fxTimer == 2) ? 6 : 3;

            Graphics_FillRect(ex - r, cy - 1, ex + r, cy + 1, s_fxColor);
            Graphics_FillRect(ex - 1, cy - r, ex + 1, cy + r, s_fxColor);
            Graphics_FillRect(ex - 1, cy - 1, ex + 1, cy + 1, C_WHITE);
            MarkRect(ex - r, cy - r, ex + r, cy + r);
        }
    }
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

    if(s_wLoaded != s_equip)
    {
        uint16_t wc;

        WpnBuild(s_equip);
        for(wc = WPN_C0; wc <= WPN_C1; wc++)            // repaint only the rows the hand covers
            AddRange((int16_t)wc, WPN_Y0, RC_VIEW_H - 1);
    }

    // Overlay bookkeeping: last frame's overlay columns must be repainted.
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

        {
            // Did this column repaint anything down in the held item's rows?
            bool wd = false;

            if((int16_t)c >= WPN_C0 && (int16_t)c <= WPN_C1)
            {
                int16_t lo = (s_cBot[c] > bot) ? s_cBot[c] : bot;

                if(s_cTop[c] < 0) wd = true;
                else if(s_prvT[c] <= s_prvB[c] && s_prvB[c] >= WPN_Y0) wd = true;
                else if(lo >= WPN_Y0 &&
                        (top != s_cTop[c] || bot != s_cBot[c] ||
                         s_wallCol[tile][shade][side] != s_cCol[c])) wd = true;
            }

            DrawColumn(c, top, bot, s_wallCol[tile][shade][side],
                       s_prvT[c], s_prvB[c]);

            if(wd) s_wpnColDirty[c] = 1;
        }

        for(k = 0; k < s_nSpr; k++)
        {
            if(c >= s_spr[k].c0 && c <= s_spr[k].c1 && s_spr[k].dist < perp)
            {
                DrawSlice(&s_spr[k], c);
                if((int16_t)c >= WPN_C0 && (int16_t)c <= WPN_C1 &&
                   s_spr[k].top + (int16_t)s_spr[k].size > WPN_Y0)
                    s_wpnColDirty[c] = 1;
                AddRange((int16_t)c, s_spr[k].top,
                         (int16_t)(s_spr[k].top + (int16_t)s_spr[k].size - 1));
            }
        }

        if(s_wpnColDirty[c])
        {
            DrawStaffCol(c);
            s_wpnColDirty[c] = 0;
        }
    }

    // Overlays: staff bolts, then the crosshair.
    if(s_fxTimer) DrawFx();

    if(s_mode == MODE_PLAY)
    {
        Graphics_FillRect(cxp - 5, cyp, cxp + 5, cyp, 0xC0C0C0UL);
        Graphics_FillRect(cxp, cyp - 5, cxp, cyp + 5, 0xC0C0C0UL);
    }
}

//*****************************************************************************
// Banners and end screen
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
        DrawText("Find the glowing door", SCREEN_W / 2, 126, C_GRAY);
    }
    else if(s_lv.boss)
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

static void DrawEndScreen(void)
{
    char    buf[28];
    int16_t n;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    DrawText("GAME OVER", SCREEN_W / 2, 55, C_RED);
    n  = Cat(buf, 0, "Died in room ", 13);
    Itoa(buf + n, s_level + 1);
    DrawText(buf, SCREEN_W / 2, 90, C_WHITE);

    n  = Cat(buf, 0, "Level ", 6);
    Itoa(buf + n, s_xpLv);
    DrawText(buf, SCREEN_W / 2, 120, C_XP);

    n  = Cat(buf, 0, "Kills ", 6);
    Itoa(buf + n, s_kills);
    DrawText(buf, SCREEN_W / 2, 148, C_GRAY);

    n  = Cat(buf, 0, "Gold ", 5);
    Itoa(buf + n, s_gold);
    DrawText(buf, SCREEN_W / 2, 174, C_GOLD);

    DrawText("Press D to exit", SCREEN_W / 2, 215, C_CYAN);
}

//*****************************************************************************
// Level-up cards
//*****************************************************************************
#define CARD_W      100
#define CARD_H      138
#define CARD_X0     5
#define CARD_GAP    5
#define CARD_Y0     38
#define RR_X0       5
#define RR_X1       165
#define RR_Y0       186
#define RR_Y1       212

// Returns: 0 = can buy, 1 = can't afford, 2 = unavailable (owned / maxed / full).
// Fills the short status text ("90g", "OWNED", "MAX"...) and the price.
static uint16_t OfferState(uint16_t off, char *stat, uint16_t *costOut)
{
    const Offer *of = &kOffer[off];
    uint16_t cost = 0, lv = 0, mx = 0;
    int16_t  n;

    stat[0]  = 0;
    *costOut = 0;

    if(of->kind == K_STAT)
    {
        switch(of->idx)
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
    }
    else if(of->kind == K_TRAIT)
    {
        if(s_traits & (1u << of->idx)) { Cat(stat, 0, "OWNED", 5); return 2; }
        cost = kTraitCost[of->idx];
    }
    else if(of->kind == K_STAFF)
    {
        if(s_staffOwned & (1u << of->idx)) { Cat(stat, 0, "OWNED", 5); return 2; }
        cost = kStaffCost[of->idx];
    }
    else
    {
        if(s_itemOwned & (1u << of->idx)) { Cat(stat, 0, "OWNED", 5); return 2; }
        cost = kItemCost[of->idx];
    }

    n = Itoa(stat, cost);
    Cat(stat, n, "g", 1);
    *costOut = cost;
    return (s_gold >= cost) ? 0 : 1;
}

// Three distinct random offers from whatever is still available.
static void RollCards(void)
{
    uint16_t pool[NUM_OFFER], n = 0, i, k, cost;
    char     stat[16];

    for(i = 0; i < NUM_OFFER; i++)
        if(OfferState(i, stat, &cost) != 2) pool[n++] = i;

    for(i = 0; i < 3; i++)
    {
        if(n == 0)
        {
            s_card[i].off  = CARD_NONE;
            s_card[i].sold = 1;
            continue;
        }
        k = Rand16() % n;
        s_card[i].off  = pool[k];
        s_card[i].sold = 0;
        pool[k] = pool[--n];
    }
    s_cardDirty = CD_ALL;
}

static uint16_t RerollCost(void)
{
    return (uint16_t)(15u + 10u * s_rerolls);
}

static void DrawCard(uint16_t i)
{
    int16_t  x0 = (int16_t)(CARD_X0 + (int16_t)i * (CARD_W + CARD_GAP));
    int16_t  y0 = CARD_Y0;
    int16_t  x1 = (int16_t)(x0 + CARD_W - 1);
    int16_t  y1 = (int16_t)(y0 + CARD_H - 1);
    int16_t  cx = (int16_t)(x0 + CARD_W / 2);
    bool     sel = (s_cardSel == i);
    uint32_t fill = sel ? 0x2A3458UL : 0x16161EUL;
    uint32_t band;
    const Offer *of;
    const char *kindName;
    char     stat[16], line[16];
    uint16_t st, cost, k, n;
    int16_t  y;

    Graphics_FillRect(x0, y0, x1, y1, sel ? C_CYAN : 0x404050UL);       // border
    Graphics_FillRect(x0 + 2, y0 + 2, x1 - 2, y1 - 2, fill);

    if(s_card[i].off == CARD_NONE)
    {
        DrawSmall("sold out", cx, (int16_t)(y0 + CARD_H / 2), C_DKGRAY, fill);
        return;
    }

    of = &kOffer[s_card[i].off];
    switch(of->kind)
    {
        case K_STAT:  kindName = "UPGRADE"; band = 0x2C7A34UL; break;
        case K_TRAIT: kindName = "TRAIT";   band = 0x7A3A9CUL; break;
        case K_STAFF: kindName = "STAFF";   band = 0xA06A18UL; break;
        default:      kindName = "MAGIC";   band = 0x1C62A0UL; break;
    }

    Graphics_FillRect(x0 + 2, y0 + 2, x1 - 2, y0 + 17, band);
    DrawSmall(kindName, cx, (int16_t)(y0 + 10), C_WHITE, band);

    DrawSmall(of->name, cx, (int16_t)(y0 + 32), C_WHITE, fill);

    // Description: lines separated by '|'
    y = (int16_t)(y0 + 54);
    n = 0;
    for(k = 0; ; k++)
    {
        char ch = of->desc[k];

        if(ch == '|' || ch == 0)
        {
            line[n] = 0;
            DrawSmall(line, cx, y, C_GRAY, fill);
            y = (int16_t)(y + 15);
            n = 0;
            if(ch == 0) break;
        }
        else if(n < 14)
            line[n++] = ch;
    }

    // Price / state
    if(s_card[i].sold)
    {
        DrawText("SOLD", cx, (int16_t)(y1 - 16), C_DKGRAY);
    }
    else
    {
        st  = OfferState(s_card[i].off, stat, &cost);
        DrawTextEx(stat, cx, (int16_t)(y1 - 16),
                   (st == 0) ? C_GREEN : (st == 1) ? C_BAD : C_GRAY, fill, false);
    }
}

static void DrawReroll(void)
{
    char     buf[16];
    int16_t  n;
    bool     sel = (s_cardSel == 3);
    uint32_t fill = sel ? 0x2A3458UL : 0x16161EUL;
    uint16_t cost = RerollCost();

    Graphics_FillRect(RR_X0, RR_Y0, RR_X1, RR_Y1, sel ? C_CYAN : 0x404050UL);
    Graphics_FillRect(RR_X0 + 2, RR_Y0 + 2, RR_X1 - 2, RR_Y1 - 2, fill);

    n  = Cat(buf, 0, "REROLL  ", 8);
    n += Itoa(buf + n, cost);
    Cat(buf, n, "g", 1);
    DrawTextEx(buf, (RR_X0 + RR_X1) / 2, (RR_Y0 + RR_Y1) / 2,
               (s_gold >= cost) ? C_WHITE : C_BAD, fill, false);
}

static void DrawCardsGold(void)
{
    char    buf[16];
    int16_t n;

    Graphics_FillRect(190, 2, SCREEN_W - 1, 28, C_BLACK);
    n = Cat(buf, 0, "G ", 2);
    Itoa(buf + n, s_gold);
    DrawText(buf, 275, 15, C_GOLD);
}

static void DrawCardMsg(void)
{
    Graphics_FillRect(172, RR_Y0, SCREEN_W - 1, RR_Y1, C_BLACK);
    DrawText(kMsgText[s_cardMsg], 245, (RR_Y0 + RR_Y1) / 2,
             (s_cardMsg == 1 || s_cardMsg == 4) ? C_GREEN : C_BAD);
}

static void DrawCardsFull(void)
{
    char    buf[24];
    int16_t n;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    n  = Cat(buf, 0, "LEVEL UP!  Lv ", 14);
    n += Itoa(buf + n, s_xpLv);
    DrawText(buf, 105, 15, C_XP);
    if(s_lvPending > 1)
    {
        n  = Cat(buf, 0, "x", 1);
        Itoa(buf + n, s_lvPending);
        DrawSmall(buf, 215, 15, C_GRAY, C_BLACK);
    }

    Graphics_setForegroundColor(&g_sContext, C_GRAY);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
    DrawText("L/R:pick  U:buy  D:continue", SCREEN_W / 2, 230, C_DKGRAY);
    s_cardDirty = CD_ALL;
}

static void CardsRender(void)
{
    uint16_t i;

    for(i = 0; i < 3; i++)
        if(s_cardDirty & (1u << i)) DrawCard(i);
    if(s_cardDirty & CD_REROLL) DrawReroll();
    if(s_cardDirty & CD_GOLD)   DrawCardsGold();
    if(s_cardDirty & CD_MSG)    DrawCardMsg();

    s_cardDirty = 0;
}

static void EnterCards(void)
{
    s_mode      = MODE_CARDS;
    s_dirtyFull = true;
    s_cardSel   = 1;
    s_cardMsg   = 0;
    s_rerolls   = 0;
    s_stX = s_stY = 1;                  // ignore a stick that is already pushed
    RollCards();
    Sfx(&sfx_level_up);
}

static void BuyCard(uint16_t i)
{
    const Offer *of;
    char         stat[16];
    uint16_t     cost, st;

    s_cardDirty |= CD_MSG;

    if(s_card[i].off == CARD_NONE || s_card[i].sold)
    {
        s_cardMsg = 3;
        Sfx(&sfx_menu_bump);
        return;
    }

    of = &kOffer[s_card[i].off];
    st = OfferState(s_card[i].off, stat, &cost);

    if(st == 2)         { s_cardMsg = 3; Sfx(&sfx_menu_bump); return; }
    if(s_gold < cost)   { s_cardMsg = 2; Sfx(&sfx_menu_bump); return; }

    s_gold -= cost;

    switch(of->kind)
    {
        case K_STAT:
            switch(of->idx)
            {
                case ST_DMG:   s_lvDmg++;   break;
                case ST_RATE:  s_lvRate++;  break;
                case ST_SPEED: s_lvSpeed++; break;
                case ST_GOLD:  s_lvGold++;  break;
                case ST_HEART: s_maxHp++; s_hp++; break;
                default:       s_hp++;      break;              // heal
            }
            break;

        case K_TRAIT:
            s_traits |= (1u << of->idx);
            break;

        case K_STAFF:
            s_staffOwned |= (1u << of->idx);
            s_equip       = of->idx;
            break;

        default:                                                // K_ITEM
            s_itemOwned |= (1u << of->idx);
            s_equip      = NUM_STAFF + of->idx;
            break;
    }

    s_card[i].sold = 1;
    s_cardMsg      = 1;
    s_cardDirty   |= CD_GOLD | CD_REROLL | 0x07;                // prices may now be affordable / not
    Sfx(&sfx_coin);
}

static void RerollCards(void)
{
    uint16_t cost = RerollCost();

    s_cardDirty |= CD_MSG;

    if(s_gold < cost)
    {
        s_cardMsg = 2;
        Sfx(&sfx_menu_bump);
        return;
    }

    s_gold -= cost;
    s_rerolls++;
    RollCards();
    s_cardMsg    = 4;
    s_cardDirty |= CD_GOLD | CD_REROLL | CD_MSG;
    Sfx(&sfx_menu_select);
}

static void LeaveCards(void)
{
    Sfx(&sfx_menu_back);

    if(s_lvPending) s_lvPending--;

    if(s_lvPending > 0)                 // more level-ups waiting: next set of cards
    {
        s_rerolls   = 0;
        s_cardMsg   = 0;
        s_cardSel   = 1;
        s_dirtyFull = true;
        RollCards();
    }
    else
    {
        s_mode      = MODE_PLAY;
        s_dirtyFull = true;
    }
}

static void UpdateCards(void)
{
    int16_t  sx = StickEdge(g_joy.dx, &s_stX);
    uint16_t old = s_cardSel;

    (void)StickEdge(g_joy.dy, &s_stY);

    if(g_dpad.dPressed) { LeaveCards(); return; }

    if(g_dpad.lPressed || sx < 0)
        s_cardSel = (s_cardSel + 3u) % 4u;
    else if(g_dpad.rPressed || sx > 0)
        s_cardSel = (s_cardSel + 1u) % 4u;

    if(s_cardSel != old)
    {
        s_cardDirty |= (old == 3 ? CD_REROLL : (1u << old)) |
                       (s_cardSel == 3 ? CD_REROLL : (1u << s_cardSel));
        Sfx(&sfx_menu_move);
    }

    if(g_dpad.uPressed || g_joy.btnPressed)
    {
        if(s_cardSel < 3) BuyCard(s_cardSel);
        else              RerollCards();
    }
}

//*****************************************************************************
// Run / level flow
//*****************************************************************************
static void ResetRun(void)
{
    uint16_t i;

    s_hp = s_maxHp = RC_START_HEARTS;
    s_gold = 0;
    s_kills = 0;
    s_lvDmg = s_lvRate = s_lvSpeed = s_lvGold = 0;
    s_traits = 0;
    s_staffOwned = 1;
    s_itemOwned  = 0;
    s_equip = 0;
    for(i = 0; i < NUM_SLOT; i++) { s_cd[i] = 0; s_cdMax[i] = 0; }
    s_haste = s_aegis = 0;
    s_xp = 0;
    s_xpLv = 1;
    s_lvPending = 0;
    s_lastMap = 0xFFFFu;
    s_wLoaded = 0xFFFFu;
}

static void MakeLevelDef(uint16_t d, LevelDef *o)
{
    uint16_t t;

    o->boss = IsBossLevel(d) ? 1u : 0u;

    t = (uint16_t)(8u + 3u * d);                // more enemies each room
    if(t > 70u) t = 70u;
    o->total = o->boss ? 1u : t;

    t = (uint16_t)(4u + d / 2u);                // more at once
    if(t > 10u) t = 10u;
    o->maxAlive = t;

    o->gap = (d >= 14u) ? 18u : (uint16_t)(60u - 3u * d);   // faster arrivals

    o->pSkel  = (d >= 1u) ? ((8u * d  > 26u) ? 26u : 8u * d)  : 0u;
    o->pBat   = (d >= 1u) ? ((10u + 2u * d > 22u) ? 22u : 10u + 2u * d) : 0u;
    o->pBrute = (d >= 2u) ? ((6u * (d - 1u) > 24u) ? 24u : 6u * (d - 1u)) : 0u;
    o->pBeh   = (d >= 3u) ? ((4u * (d - 2u) > 14u) ? 14u : 4u * (d - 2u)) : 0u;
}

static uint16_t PickMap(uint16_t lv)
{
    uint16_t m;

    if(IsBossLevel(lv)) return MAP_BOSS;
    if(lv < 5u) return lv;                      // the first rooms in order
    do { m = Rand16() % 5u; } while(m == s_lastMap);
    return m;
}

static void StartLevel(uint16_t lv)
{
    uint16_t i, m;

    s_level = lv;
    MakeLevelDef(lv, &s_lv);

    m = PickMap(lv);
    s_lastMap = m;
    LoadMap(m, (lv > 0u) ? ((Rand16() & 1u) != 0) : false);

    for(i = 0; i < RC_MAX_ENEMIES; i++) s_en[i].alive = 0;
    for(i = 0; i < RC_MAX_OBJ; i++)     s_obj[i].alive = 0;

    s_alive = 0;
    s_toSpawn = s_lv.total;
    s_spawnTimer = 90;
    s_spawnIdx = 0;
    s_bossSpawned = s_bossAlive = 0;

    s_doorOpen = false;
    s_portalTick = s_portalPhase = 0;
    SetPortalPhase(0);

    s_ang    = 768;                     // face "up" the map (the door is on that wall)
    s_invuln = 60;
    s_fxTimer = 0;
    s_stepAcc = 0;

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

// Last enemy down: the door lights up. Walk into it for the next room.
static void LevelClear(void)
{
    s_doorOpen = true;
    s_tile[((uint16_t)s_doorY << RC_MAP_SHIFT) + (uint16_t)s_doorX] = TILE_PORTAL;
    s_portalTick = s_portalPhase = 0;
    SetPortalPhase(0);

    s_mode       = MODE_CLEAR;
    s_timer      = 70;
    s_banner     = BAN_CLEAR;
    s_bannerDraw = true;
    s_hud       |= HUD_LEFT;
    Sfx(&sfx_level_up);
}

static void UpdatePlay(void)
{
    bool    moved = false;
    int16_t jx, jy, fwd, str, c, s, mx, my, nx, ny, sp;
    uint16_t i;

    if(s_invuln)  s_invuln--;
    if(s_fxTimer) s_fxTimer--;
    if(s_sfxHold) s_sfxHold--;
    if(s_stepBlock) s_stepBlock--;

    for(i = 0; i < NUM_SLOT; i++)               // cooldowns (Haste ticks them twice)
    {
        if(s_cd[i]) s_cd[i]--;
        if(s_haste && s_cd[i]) s_cd[i]--;
    }
    if(s_haste) { s_haste--; if(!s_haste) s_hud |= HUD_HINT; }
    if(s_aegis) { s_aegis--; if(!s_aegis) s_hud |= HUD_HINT; }

    // D-pad down: next staff / item
    if(g_dpad.dPressed) CycleEquip();

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
        if(s_haste) sp += (int16_t)(sp >> 1);
        fwd = (int16_t)(((int32_t)jy * sp * 41L) >> 12);        // stick/100 * speed
        str = (int16_t)(((int32_t)jx * sp * 41L) >> 12);
        c   = COS(s_ang);
        s   = SIN(s_ang);
        mx  = (int16_t)((((int32_t)fwd * c) - ((int32_t)str * s)) >> 14);
        my  = (int16_t)((((int32_t)fwd * s) + ((int32_t)str * c)) >> 14);

        nx = s_px + mx;
        if(!Blocked(nx, s_py, RC_PLAYER_R)) { s_px = nx; moved = true; s_stepAcc += (uint16_t)Abs16(mx); }
        ny = s_py + my;
        if(!Blocked(s_px, ny, RC_PLAYER_R)) { s_py = ny; moved = true; s_stepAcc += (uint16_t)Abs16(my); }
    }

#if RC_FOOTSTEPS
    // Footsteps: one rustle per RC_STEP_DIST walked, left/right alternate.
    if(s_stepAcc >= RC_STEP_DIST)
    {
        s_stepAcc -= RC_STEP_DIST;
        if(s_stepAcc >= RC_STEP_DIST) s_stepAcc = 0;        // e.g. after a long pause
        if(s_mode == MODE_PLAY && s_stepBlock == 0)
            Buzzer_PlaySfx(s_stepAlt ? &sfx_step_b : &sfx_step_a);
        s_stepAlt ^= 1u;
    }
#endif

    // Use the equipped staff / item (hold = auto-use)
    if(g_dpad.u) UseEquipped();

    UpdateObjects();
    UpdateEnemies();
    TrySpawn();

    if(s_mode == MODE_PLAY && !s_doorOpen)
    {
        bool cleared = s_lv.boss ? (s_bossSpawned && !s_bossAlive)
                                 : (s_toSpawn == 0 && s_alive == 0);
        if(cleared) LevelClear();
    }

    // Through the open door -> next room
    if(s_mode == MODE_PLAY && s_doorOpen &&
       (s_px >> 10) == s_doorX && (s_py >> 10) == s_doorY)
    {
        StartLevel(s_level + 1u);
        return;
    }

    if(s_doorOpen)                              // portal pulse
    {
        if(++s_portalTick >= 6u)
        {
            s_portalTick  = 0;
            s_portalPhase ^= 1u;
            SetPortalPhase(s_portalPhase);
        }
    }

    if(moved || s_alive || s_fxTimer || s_doorOpen || s_haste || s_aegis)
        s_viewDirty = 2;
    else
    {
        for(i = 0; i < RC_MAX_OBJ; i++)
            if(s_obj[i].alive) { s_viewDirty = 2; break; }
    }
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
    uint16_t i;

    PaceInit();
    BuildTables();
    ResetRun();
    for(i = 0; i < RC_MAX_ENEMIES; i++) s_en[i].alive = 0;
    for(i = 0; i < RC_MAX_OBJ; i++)     s_obj[i].alive = 0;
    s_stX = s_stY = 0;
    s_hud = 0;
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
            if(s_lvPending && s_mode == MODE_PLAY) EnterCards();
            break;

        case MODE_CARDS:
            UpdateCards();
            break;

        case MODE_CLEAR:
            if(--s_timer == 0)
            {
                s_mode       = MODE_PLAY;
                s_invalidate = true;            // wipes the banner
                s_viewDirty  = 2;
                if(s_lvPending) EnterCards();
            }
            break;

        default:                                // OVER
            if(g_dpad.dPressed || g_joy.btnPressed) return APP_QUIT;
            break;
    }

    return APP_CONTINUE;
}

static void FullRepaint(void)
{
    if(s_mode == MODE_CARDS)
        DrawCardsFull();
    else if(s_mode == MODE_OVER)
        DrawEndScreen();
    else
    {
        DrawHudFrame();
        s_hud        = HUD_ALL;
        s_invalidate = true;
        s_viewDirty  = 2;
        s_cdShown    = -1;
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

    if(s_mode == MODE_CARDS)
    {
        CardsRender();
        return;
    }

    if(s_mode == MODE_OVER) return;

    if(s_hud) DrawHud();

    if(s_viewDirty)
    {
        RenderView();
        s_viewDirty--;
    }

    if(s_mode == MODE_PLAY) DrawCdStrip();

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
    if(s_mode != MODE_OVER)
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
    "Stick:move L/R:turn U:use D:swap",     // hint shown under the menu
    0x7050FFUL,                             // menu tile color
    RC_FRAME_US,
    RC_Init, RC_Update, RC_Render,
    RC_Exit,
    NULL,                                   // not a legacy app
    RC_Redraw
};
