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
//  * One big ring-shaped map cut into 6 sections by doors (like COD zombies).
//    Rounds never stop: kill the wave, a short breather, the next wave starts.
//    Doors are BOUGHT with gold (stick button while facing one). Enemies only
//    spawn in sections you have opened and path-find through open doors.
//    Every 6th round is the Lich.
//  * Kills give GOLD (same as before) and XP. When the XP bar fills you level
//    up and get 3 random cards to buy with gold (or reroll them for gold).
//  * D-pad Down cycles the equipped staff / magic item. D-pad Up uses it.
//
// APP CONTRACT: update() = logic only, render() = drawing only, one public
// symbol (g_raycasterApp). The runner already calls Joy_Update()/Dpad_Update().
//#############################################################################

#include "raycaster.h"
#include <math.h>

/* Performance configuration: 60 strips x 5 pixels = 300 pixels. */
#undef RC_COLS
#undef RC_COL_W
#define RC_COLS  60
#define RC_COL_W 5
#undef RC_MAX_SPAWNS
#define RC_MAX_SPAWNS 12                /* 2 spawn markers in each of the 6 sections */

#define COL_CEIL    COLOR_BLACK
#define COL_FLOOR   COLOR_TETRIS_GRID
#define C_BLACK    COLOR_BLACK
#define C_WHITE    COLOR_WHITE
#define C_GRAY     COLOR_GRAY
#define C_DKGRAY   COLOR_DARK_GRAY
#define C_RED      COLOR_CRIMSON
#define C_GOLD     COLOR_GOLD
#define C_GREEN    COLOR_GREEN
#define C_CYAN     COLOR_TURQUOISE
#define C_BAD      COLOR_CORAL
#define C_XP       COLOR_VIOLET

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

//*****************************************************************************
// Mana. Every staff / item costs mana (kStaffMana / kItemMana below). It refills
// slowly while the stick is released and bottles drop from dead enemies.
//*****************************************************************************
#define MANA_MAX            100
#define MANA_REGEN_FRAMES   10          // standing still: +1 mana every N frames (~3/s)
#define MANA_BOTTLE         25          // mana per bottle
#define MANA_DROP_PCT       20          // % chance an enemy drops a bottle (Lich: always)
#define MANA_PICKUP_R       380         // pickup distance (Q10)
#define MANA_BOTTLE_LIFE    450         // frames a bottle stays on the floor
#define C_MANA              COLOR_BLUE

//*****************************************************************************
// Doors and rounds. Door k (1..6) joins section k-1 and section k % 6; the
// sections form a ring, so with every door open you can run laps.
//*****************************************************************************
#define RC_NUM_DOORS        6
#define RC_NUM_SECTIONS     6
#define SEC_START           0           // you start in section A (top-left)
#define DOOR_COST_BASE      100         // first door
#define DOOR_COST_STEP      100         // each door bought makes the next one dearer
#define DOOR_USE_DIST       2600        // Q10: how close you must be to buy
#define ROUND_GAP_FRAMES    150         // breather between rounds (5 s at 30 fps)
#define SPAWN_MIN_DIST      2600        // Q10: never spawn closer than this to the player

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
       PT_FLASK_FIRE, PT_FLASK_FROST, PT_FLASK_VENOM, PT_FLASK_MANA,
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
    { "Ray Wand",    3, 16, 1,  0,  0, COLOR_TURQUOISE },
    { "Prism Staff", 3, 22, 3, 16,  0, COLOR_VIOLET },
    { "Lance",       5, 30, 1,  0, 99, COLOR_KHAKI },
    { "Storm Rod",   2,  8, 1,  0,  0, COLOR_PLUM },
    { "Sunfire",     4, 34, 5, 14,  1, COLOR_CORAL },
};
static const uint16_t kStaffCost[NUM_STAFF] = { 0, 70, 130, 110, 260 };
// Mana per shot, same order as kStaff
static const uint16_t kStaffMana[NUM_STAFF] = { 2, 6, 8, 1, 10 };

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
    { "Fire Flask",   60, COLOR_TETRIS_L_ORANGE },
    { "Frost Flask",  70, COLOR_SKY_BLUE },
    { "Venom Flask", 110, COLOR_MINT },
    { "Elixir",      600, COLOR_CRIMSON },
    { "Haste Brew",  700, COLOR_GOLD },
    { "Arcane Tome",  50, COLOR_VIOLET },
    { "Warp Rune",   120, COLOR_TURQUOISE },
    { "Aegis Charm", 480, COLOR_SKY_BLUE },
};
static const uint16_t kItemCost[NUM_ITEM] = { 90, 90, 110, 80, 100, 170, 120, 150 };
// Mana per use, same order as kItem
static const uint16_t kItemMana[NUM_ITEM] = { 8, 8, 10, 20, 25, 12, 6, 25 };

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
// The map (16 x 16): a ring of six sections around a solid centre block.
//   # stone   P pillar   B brick   . floor   @ player start
//   1..6  door k (3 tiles wide, closed until bought)
//   a..f  enemy spawn marker for section A..F (A top-left, then clockwise)
// Section layout (doors are the numbers between them):
//        A --1-- B --2-- C
//        |               |
//        6               3
//        |               |
//        F --5-- E --4-- D
//*****************************************************************************
#define RC_NUM_MAPS 1

static const char *const kMaps[RC_NUM_MAPS][RC_MAP_H] =
{
    {
        "################",
        "#a..a#b..b#c...#",
        "#....1....2....#",
        "#.P..1.PP.2..P.#",
        "#....1....2....#",
        "#....#....#....#",
        "#..@..####.....#",
        "#.....####....c#",
        "##666######333##",
        "#.....####....d#",
        "#....#....#....#",
        "#....5....4....#",
        "#.P..5....4.P..#",
        "#....5....4....#",
        "#f..f#e..e#...d#",
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
    { {'d',COLOR_OLIVE},{'b',COLOR_DARK_GRAY},{'c',COLOR_GOLD},{'h',COLOR_WHITE},{'f',COLOR_TETRIS_GRID},
      {'e',COLOR_DARK_GRAY},{'a',COLOR_TETRIS_GRID},{'g',COLOR_LIGHT_GRAY},{0,0} };
static const PalEnt kPalHobgob[] =
    { {'d',COLOR_BROWN},{'b',COLOR_BROWN},{'c',COLOR_CRIMSON},{'h',COLOR_WHITE},{'f',COLOR_TETRIS_GRID},
      {'e',COLOR_TETRIS_GRID},{'a',COLOR_TETRIS_GRID},{'g',COLOR_KHAKI},{0,0} };
static const PalEnt kPalBat[]    =
    { {'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_DARK_PURPLE},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kPalSkel[]   =
    { {'d',COLOR_LIGHT_GRAY},{'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_SILVER},{0,0} };
static const PalEnt kPalLich[]   =
    { {'a',COLOR_GOLD},{'b',COLOR_LIGHT_GRAY},{'c',COLOR_CYAN},{'d',COLOR_DARK_PURPLE},
      {'e',COLOR_DARK_PURPLE},{'f',COLOR_BLACK},{0,0} };
static const PalEnt kPalBeholder[] =
    { {'c',COLOR_PURPLE},{'f',COLOR_DARK_PURPLE},{'g',COLOR_WHITE},{'d',COLOR_CRIMSON},{'a',COLOR_BLACK},
      {'h',COLOR_GOLD},{'e',COLOR_WHITE},{'b',COLOR_TETRIS_GRID},{0,0} };
static const PalEnt kPalBeholderHot[] =
    { {'c',COLOR_GRAY},{'f',COLOR_DARK_PURPLE},{'g',COLOR_PINK},{'d',COLOR_KHAKI},{'a',COLOR_CRIMSON},
      {'h',COLOR_CRIMSON},{'e',COLOR_WHITE},{'b',COLOR_TETRIS_GRID},{0,0} };
static const PalEnt kPalFlaskFire[] =
    { {'a',COLOR_TETRIS_L_ORANGE},{'c',COLOR_LIGHT_GRAY},{'d',COLOR_OLIVE},{'b',COLOR_TETRIS_GRID},{0,0} };
static const PalEnt kPalFlaskFrost[] =
    { {'a',COLOR_SKY_BLUE},{'c',COLOR_LIGHT_GRAY},{'d',COLOR_OLIVE},{'b',COLOR_TETRIS_GRID},{0,0} };
static const PalEnt kPalFlaskVenom[] =
    { {'a',COLOR_GREEN},{'c',COLOR_LIGHT_GRAY},{'d',COLOR_OLIVE},{'b',COLOR_TETRIS_GRID},{0,0} };
static const PalEnt kPalFlaskMana[] =
    { {'a',COLOR_BLUE},{'c',COLOR_WHITE},{'d',COLOR_GOLD},{'b',COLOR_NAVY},{0,0} };
static const PalEnt kPalBolt[]   =
    { {'a',COLOR_CRIMSON},{'b',COLOR_WHITE},{0,0} };
static const PalEnt kPalPuffFire[] =
    { {'a',COLOR_BROWN},{'b',COLOR_TETRIS_L_ORANGE},{'c',COLOR_KHAKI},{0,0} };
static const PalEnt kPalPuffFrost[] =
    { {'a',COLOR_TEAL},{'b',COLOR_SKY_BLUE},{'c',COLOR_WHITE},{0,0} };
static const PalEnt kPalPuffVenom[] =
    { {'a',COLOR_DARK_GRAY},{'b',COLOR_OLIVE},{'c',COLOR_MINT},{0,0} };
static const PalEnt kPalPuffArcane[] =
    { {'a',COLOR_GRAY},{'b',COLOR_VIOLET},{'c',COLOR_WHITE},{0,0} };

static const PalEnt *const kPal[PT_COUNT] =
{
    kPalGoblin, kPalBat, kPalHobgob, kPalSkel, kPalBeholder, kPalLich, kPalBeholderHot,
    kPalFlaskFire, kPalFlaskFrost, kPalFlaskVenom, kPalFlaskMana, kPalBolt,
    kPalPuffFire, kPalPuffFrost, kPalPuffVenom, kPalPuffArcane
};

// Sprite tint: 0 none, 1 white (hit), 2 orange (burning), 3 ice blue (frozen)
static const uint32_t kTint[4] = { 0, COLOR_WHITE, COLOR_TETRIS_L_ORANGE, COLOR_SKY_BLUE };

// Wall tiles: 0 unused, 1 stone, 2 pillar, 3 brick, 4 door (closed), 5 portal (open),
// 6 = the portal's second pulse colour (only used for drawing, never in the map)
#define TILE_DOOR    4
#define TILE_PORTAL  5
#define TILE_KINDS   7                  // tile 6 = portal second pulse colour (draw only)
static const uint32_t kWallBase[TILE_KINDS] =
    { 0, COLOR_GRAY, COLOR_GRAY, COLOR_BROWN, COLOR_ORANGE, COLOR_TURQUOISE, COLOR_SKY_BLUE };
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
#define WPN_SCALE    RC_COL_W           // one art cell = one strip (5 px): 100 x 90 on screen
#define WPN_TIP_ROW  1                  // art row the beam leaves from
#define WPN_MAXRUNS  12                 // vertical runs kept per art column
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
    { {'k',K},{'s',COLOR_KHAKI},{'q',COLOR_GRAY},{'c',C},{'d',D},\
      {'w',W},{'x',X},{'g',G},{'h',H},{'f',F},{'y',Y},{'r',R},{0,0} }

static const PalEnt kWpnPalRay[]   = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_TURQUOISE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_GOLD,COLOR_CRIMSON);
static const PalEnt kWpnPalPrism[] = WPAL(COLOR_TETRIS_GRID,COLOR_DARK_PURPLE,COLOR_DARK_GRAY,COLOR_CORAL,COLOR_OLIVE,COLOR_VIOLET,COLOR_WHITE,COLOR_TURQUOISE,COLOR_KHAKI,COLOR_CORAL);
static const PalEnt kWpnPalLance[] = WPAL(COLOR_TETRIS_GRID,COLOR_DARK_GRAY,COLOR_TETRIS_GRID,COLOR_CORAL,COLOR_DARK_GRAY,COLOR_GOLD,COLOR_WHITE,COLOR_KHAKI,COLOR_LIGHT_GRAY,COLOR_CRIMSON);
static const PalEnt kWpnPalStorm[] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_DARK_GRAY,COLOR_SILVER,COLOR_GRAY,COLOR_PLUM,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_KHAKI,COLOR_CORAL);
static const PalEnt kWpnPalSun[]   = WPAL(COLOR_BLACK,COLOR_BROWN,COLOR_MAROON,COLOR_ORANGE,COLOR_OLIVE,COLOR_CORAL,COLOR_KHAKI,COLOR_TETRIS_L_ORANGE,COLOR_KHAKI,COLOR_CRIMSON);
static const PalEnt kWpnPalFire[]  = WPAL(COLOR_BLACK,COLOR_BROWN,COLOR_MAROON,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_TETRIS_L_ORANGE,COLOR_KHAKI,COLOR_TETRIS_L_ORANGE,COLOR_GOLD,COLOR_OLIVE);
static const PalEnt kWpnPalFrost[] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_SKY_BLUE);
static const PalEnt kWpnPalVenom[] = WPAL(COLOR_BLACK,COLOR_DARK_GRAY,COLOR_TETRIS_GRID,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_GREEN,COLOR_MINT,COLOR_MINT,COLOR_LIGHT_GRAY,COLOR_CRIMSON);
static const PalEnt kWpnPalElixir[]= WPAL(COLOR_BLACK,COLOR_DARK_GRAY,COLOR_TETRIS_GRID,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_CRIMSON,COLOR_PLUM,COLOR_CORAL,COLOR_OLIVE,COLOR_GOLD);
static const PalEnt kWpnPalHaste[] = WPAL(COLOR_TETRIS_GRID,COLOR_OLIVE,COLOR_DARK_GRAY,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_GOLD,COLOR_KHAKI,COLOR_KHAKI,COLOR_WHITE,COLOR_ORANGE);
static const PalEnt kWpnPalTome[]  = WPAL(COLOR_TETRIS_GRID,COLOR_DARK_PURPLE,COLOR_DARK_GRAY,COLOR_DARK_PURPLE,COLOR_DARK_PURPLE,COLOR_VIOLET,COLOR_LIGHT_GRAY,COLOR_WHITE,COLOR_GOLD,COLOR_VIOLET);
static const PalEnt kWpnPalWarp[]  = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_TETRIS_GRID,COLOR_TEAL,COLOR_CHARCOAL,COLOR_TURQUOISE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_TURQUOISE);
static const PalEnt kWpnPalAegis[] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_GRAY,COLOR_CHARCOAL,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_GOLD,COLOR_GOLD);

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

// Quarter-wave tables (angles 0..256 = 0..90 deg). The other three quadrants are
// mirrored on lookup, which saves ~3000 words of RAM versus full 1024-entry tables.
static int16_t  s_qsin[257];                    // sin, Q14
static uint16_t s_qinv[257];                    // 256/sin (Q8, clamped): ray step length
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


// sin(a) for any angle 0..1023 (Q14), mirrored from the first quadrant.
static int16_t SinLut(uint16_t a)
{
    uint16_t i = a & 255u;
    int16_t  v;

    if(a & 256u) i = (uint16_t)(256u - i);          // 2nd / 4th quadrant: mirror
    v = s_qsin[i];
    return (a & 512u) ? (int16_t)-v : v;            // 3rd / 4th quadrant: negative
}

// 256/|sin(b)| (Q8). 256/|cos(a)| is QInv(a + 256), 256/|sin(a)| is QInv(a).
static uint16_t QInv(uint16_t b)
{
    uint16_t i = b & 255u;

    if(b & 256u) i = (uint16_t)(256u - i);
    return s_qinv[i];
}

#define SIN(a)  SinLut((uint16_t)(a))
#define COS(a)  SinLut((uint16_t)((uint16_t)(a) + 256u))

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
static uint16_t s_mana, s_manaAcc, s_manaFlash; // mana, regen timer, "not enough" flash frames
static int16_t  s_manaShown = -1;               // mana bar pixels on screen (-1 = repaint all)
static bool     s_manaFlashShown;

// Level
static uint16_t s_level;                        // 0 = first room (endless)
static uint16_t s_lastMap = 0xFFFFu;
static LevelDef s_lv;
static uint16_t s_tile[RC_MAP_CELLS];
static int16_t  s_spX[RC_MAX_SPAWNS], s_spY[RC_MAX_SPAWNS];
static uint16_t s_spCount;
static bool     s_secOpen[RC_NUM_SECTIONS];     // sections the player has unlocked
static bool     s_doorBought[RC_NUM_DOORS + 1]; // index 1..6
static uint16_t s_doorsBought;
static int16_t  s_doorCx[RC_NUM_DOORS + 1], s_doorCy[RC_NUM_DOORS + 1];   // door centres, Q10
static uint16_t s_spSec[RC_MAX_SPAWNS];         // section of each spawn marker
static uint16_t s_doorNear;                     // door the player is facing (0 = none)
static uint16_t s_doorShown = 0xFFFFu;          // prompt state last drawn
static uint16_t s_roundGap;                     // breather frames left (0 = round running)
static uint16_t s_flow[RC_MAP_CELLS];           // steps to the player, 0xFFFF = unreachable
static int16_t  s_flowTx = -1, s_flowTy = -1;   // player tile the flow field was built for
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
enum { OB_FLASK, OB_BOLT, OB_CLOUD, OB_SPARK, OB_MANA };

static uint16_t DoorCost(void)
{
    return (uint16_t)(DOOR_COST_BASE + DOOR_COST_STEP * s_doorsBought);
}
static Obj      s_obj[RC_MAX_OBJ];

// Flow
static uint16_t s_mode, s_timer;
static int16_t  s_stX, s_stY;                   // stick flick state

// Effects (staff beams)
static uint16_t s_fxTimer, s_fxN;
static int16_t  s_fxX[5];
static uint32_t s_fxColor;

// Held item: one list of vertical colour runs per ART column (RC_COL_W px wide),
// packed y0 | y1 << 5 | colour << 10 (rows are art cells).
static uint16_t s_wRun[WPN_W][WPN_MAXRUNS];
static uint16_t s_wRunN[WPN_W];
static uint32_t s_wPal[26];
static uint16_t s_wLoaded = 0xFFFFu;            // slot the runs were built for
static uint16_t s_stepAcc, s_stepBlock, s_stepAlt;   // footsteps: distance walked, sfx guard, foot
static uint16_t s_sfxHold;                      // frames an important sfx is protected
static int16_t  s_cdShown = -1;                 // cooldown strip pixels currently on screen
#define WPN_X0   (RC_VIEW_X0 + WPN_XOFF)
#define WPN_C0   (((WPN_X0) - RC_VIEW_X0) / RC_COL_W)
#define WPN_C1   (((WPN_X0 + WPN_W * WPN_SCALE - 1) - RC_VIEW_X0) / RC_COL_W)
#define WPN_TIP_X (WPN_X0 + 10 * WPN_SCALE)     // shaft is columns 9-10
#if (WPN_XOFF % RC_COL_W) != 0
#error WPN_XOFF must be a multiple of RC_COL_W so the hand lines up with the strips
#endif

// Draw state
static bool     s_dirtyFull, s_invalidate, s_bannerDraw;
static uint16_t s_hud, s_banner, s_viewDirty;
static int16_t  s_rngT[RC_COLS], s_rngB[RC_COLS];       // overlay rows drawn this frame (T > B = none)
static int16_t  s_prvT[RC_COLS], s_prvB[RC_COLS];       // overlay rows drawn last frame

// What is on the LCD, per vertical STRIP, as a list of colour runs. Each frame
// the new picture is built the same way and only the runs whose colour changed
// are sent. A run is (last row << 8) | colour id; the list always covers all
// RC_VIEW_H rows. Hand cells are as wide as a strip,
// so the art cells fit the model exactly.
#define RC_MAXR      12                 // runs stored per strip
#define RC_MAXN      32                 // runs allowed while a strip is being built
#define RC_STRIPS    RC_COLS            // hand cells are as wide as a strip: no splitting
static uint16_t s_sl[RC_STRIPS][RC_MAXR];
static uint16_t s_sn[RC_STRIPS];
uint16_t g_rcOverflow;                          // diagnostics: paints dropped (should stay ~0)
uint16_t g_rcTrunc;                             // diagnostics: strips too busy to store in full

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

    for(i = 0; i < PAL_N; i++) s_pal[type][i] = COLOR_MAGENTA;    // magenta = missing
    for(i = 0; list[i].ch; i++)
        if((uint16_t)(list[i].ch - 'a') < PAL_N)
            s_pal[type][list[i].ch - 'a'] = list[i].rgb;
}

static void BuildTables(void)
{
    uint16_t i, c, sh, x, y, n;
    float    s, co, f;

    if(s_tablesReady) return;

    for(i = 0; i <= 256; i++)
    {
        s = sinf((float)i * (6.2831853f / RC_ANGLES));

        s_qsin[i] = (int16_t)(s * 16384.0f + 0.5f);

        f = (fabsf(s) < 0.0001f) ? 65535.0f : 256.0f / fabsf(s);
        s_qinv[i] = (f > 65535.0f) ? 65535u : (uint16_t)f;
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

static void LoadMap(void)
{
    uint16_t x, y, t, k;
    int32_t  sx[RC_NUM_DOORS + 1], sy[RC_NUM_DOORS + 1];    // sums for the door centres
    uint16_t cnt[RC_NUM_DOORS + 1];

    s_spCount = 0;
    for(k = 0; k <= RC_NUM_DOORS; k++) { sx[k] = 0; sy[k] = 0; cnt[k] = 0; }

    for(y = 0; y < RC_MAP_H; y++)
        for(x = 0; x < RC_MAP_W; x++)
        {
            char ch = kMaps[0][y][x];

            t = 0;

            if(ch >= 'a' && ch <= 'f')                  // spawn marker, letter = section
            {
                if(s_spCount < RC_MAX_SPAWNS)
                {
                    s_spX[s_spCount]   = (int16_t)((x << 10) + 512);
                    s_spY[s_spCount]   = (int16_t)((y << 10) + 512);
                    s_spSec[s_spCount] = (uint16_t)(ch - 'a');
                    s_spCount++;
                }
            }
            else if(ch >= '1' && ch <= '6')             // door tile
            {
                k = (uint16_t)(ch - '0');
                t = TILE_DOOR;
                sx[k] += (int32_t)((x << 10) + 512);
                sy[k] += (int32_t)((y << 10) + 512);
                cnt[k]++;
            }
            else
            {
                switch(ch)
                {
                    case '#': t = 1; break;
                    case 'P': t = 2; break;
                    case 'B': t = 3; break;
                    case '@':
                        s_px = (int16_t)((x << 10) + 512);
                        s_py = (int16_t)((y << 10) + 512);
                        break;
                    default: break;
                }
            }
            s_tile[(y << RC_MAP_SHIFT) + x] = t;
        }

    for(k = 1; k <= RC_NUM_DOORS; k++)
        if(cnt[k])
        {
            s_doorCx[k] = (int16_t)(sx[k] / (int32_t)cnt[k]);
            s_doorCy[k] = (int16_t)(sy[k] / (int32_t)cnt[k]);
        }
}

//*****************************************************************************
// The ray caster: grid DDA, all integer.
//   angle : 0..1023
//   returns the distance ALONG the ray in Q8 (not yet fisheye-corrected)
//   *tile = wall type hit (1..5), *side = 0 for a x-face, 1 for a y-face
// Start point comes from the player position.
//*****************************************************************************
static uint16_t RC_CastRay(int16_t angle, uint16_t *tile, uint16_t *side)
{
    uint16_t a     = (uint16_t)angle & RC_ANGLE_MASK;
    int16_t  c     = COS(a);
    int16_t  s     = SIN(a);
    uint16_t dX    = QInv((uint16_t)(a + 256u));
    uint16_t dY    = QInv(a);
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

static uint16_t SlotMana(uint16_t slot)
{
    return (slot < NUM_STAFF) ? kStaffMana[slot] : kItemMana[slot - NUM_STAFF];
}

static void DropMana(int16_t x, int16_t y);

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

    if(!summoned && (e->type == ET_LICH || (Rand16() % 100u) < MANA_DROP_PCT))
        DropMana(e->x, e->y);

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

static void DropMana(int16_t x, int16_t y)
{
    Obj *o = NewObj(OB_MANA);

    if(o == NULL) return;                       // object list full: no bottle this time
    o->x    = x;
    o->y    = y;
    o->z    = 0;
    o->pal  = PT_FLASK_MANA;
    o->life = MANA_BOTTLE_LIFE;
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

            case OB_MANA:                       // mana bottle on the floor
                if(++o->age >= o->life) { o->alive = 0; break; }
                o->z = (int16_t)((o->age >> 3) & 1u);                   // little bob
                if(s_mana < MANA_MAX &&
                   Dist10(s_px - o->x, s_py - o->y) < MANA_PICKUP_R)
                {
                    s_mana += MANA_BOTTLE;
                    if(s_mana > MANA_MAX) s_mana = MANA_MAX;
                    o->alive = 0;
                    Sfx(&sfx_powerup);
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
    uint16_t cost = SlotMana(slot);

    if(s_cd[slot]) return;

    if(s_mana < cost)                                   // not enough mana
    {
        s_cdMax[slot] = 10;
        s_cd[slot]    = 10;                             // retry delay (holding U won't spam)
        s_manaFlash   = 12;
        Sfx(&sfx_menu_bump);
        return;
    }

    if(slot < NUM_STAFF)
    {
        Fire();
        s_mana -= cost;
        return;
    }

    it = slot - NUM_STAFF;
    if(UseItem(it))
    {
        s_mana       -= cost;                           // only paid if the item was used
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
    bool     found = false;

    if(s_spawnTimer) { s_spawnTimer--; return; }
    if(!s_toSpawn || s_alive >= s_lv.maxAlive || s_spCount == 0) return;

    // First marker in an UNLOCKED section that is not right on top of the player.
    for(tries = 0; tries < s_spCount; tries++)
    {
        idx = s_spawnIdx;
        s_spawnIdx = (s_spawnIdx + 1u) % s_spCount;
        if(!s_secOpen[s_spSec[idx]]) continue;
        if(Dist10(s_spX[idx] - s_px, s_spY[idx] - s_py) > SPAWN_MIN_DIST)
        {
            found = true;
            break;
        }
    }
    if(!found) { s_spawnTimer = 20; return; }

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

//-----------------------------------------------------------------------------
// Path-finding: a breadth-first "steps to the player" map over the 16 x 16 grid.
// Rebuilt whenever the player changes tile or a door opens (a few hundred cells).
// Enemies that can see you walk straight at you; otherwise they follow the map.
//-----------------------------------------------------------------------------
static const int16_t kNbDx[4] = { 1, -1, 0, 0 };
static const int16_t kNbDy[4] = { 0, 0, 1, -1 };

static void BuildFlow(void)
{
    uint16_t i, d, cell, nb;
    int16_t  tx = (int16_t)(s_px >> 10);
    int16_t  ty = (int16_t)(s_py >> 10);
    int16_t  x, y, nx, ny;
    bool     any;

    for(i = 0; i < RC_MAP_CELLS; i++) s_flow[i] = 0xFFFFu;
    s_flowTx = tx;
    s_flowTy = ty;
    if((uint16_t)tx >= RC_MAP_W || (uint16_t)ty >= RC_MAP_H) return;

    s_flow[((uint16_t)ty << RC_MAP_SHIFT) + (uint16_t)tx] = 0;

    // Wavefront: grow the distance one ring at a time (no queue array needed).
    for(d = 0; d < 64u; d++)
    {
        any = false;
        for(cell = 0; cell < RC_MAP_CELLS; cell++)
        {
            if(s_flow[cell] != d) continue;
            x = (int16_t)(cell & 15u);
            y = (int16_t)(cell >> 4);

            for(i = 0; i < 4; i++)
            {
                nx = (int16_t)(x + kNbDx[i]);
                ny = (int16_t)(y + kNbDy[i]);
                if((uint16_t)nx >= RC_MAP_W || (uint16_t)ny >= RC_MAP_H) continue;
                if(Solid(nx, ny)) continue;
                nb = (uint16_t)(((uint16_t)ny << RC_MAP_SHIFT) + (uint16_t)nx);
                if(s_flow[nb] != 0xFFFFu) continue;
                s_flow[nb] = (uint16_t)(d + 1u);
                any = true;
            }
        }
        if(!any) break;
    }
}

static void EnsureFlow(void)
{
    if((int16_t)(s_px >> 10) != s_flowTx || (int16_t)(s_py >> 10) != s_flowTy)
        BuildFlow();
}

// Is the straight line between two points free of walls?
static bool LineClear(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (int16_t)(x1 - x0);
    int16_t dy = (int16_t)(y1 - y0);
    int16_t n  = (int16_t)(Dist10(dx, dy) / 320);
    int16_t i;

    for(i = 1; i < n; i++)
        if(Blocked((int16_t)(x0 + (int16_t)(((int32_t)dx * i) / n)),
                   (int16_t)(y0 + (int16_t)(((int32_t)dy * i) / n)), 220))
            return false;
    return true;
}

// Where an enemy at (ex, ey) should head, as a vector (dx, dy).
static void SteerTarget(int16_t ex, int16_t ey, int16_t *tdx, int16_t *tdy)
{
    int16_t  etx = (int16_t)(ex >> 10);
    int16_t  ety = (int16_t)(ey >> 10);
    int16_t  nx, ny;
    uint16_t d, i;

    *tdx = (int16_t)(s_px - ex);                        // default: straight at the player
    *tdy = (int16_t)(s_py - ey);

    if((uint16_t)etx >= RC_MAP_W || (uint16_t)ety >= RC_MAP_H) return;
    d = s_flow[((uint16_t)ety << RC_MAP_SHIFT) + (uint16_t)etx];
    if(d <= 1u || d == 0xFFFFu) return;                 // next to the player / no route
    if(LineClear(ex, ey, s_px, s_py)) return;           // can see the player

    for(i = 0; i < 4; i++)                              // step to the tile that is closer
    {
        nx = (int16_t)(etx + kNbDx[i]);
        ny = (int16_t)(ety + kNbDy[i]);
        if((uint16_t)nx >= RC_MAP_W || (uint16_t)ny >= RC_MAP_H) continue;
        if(s_flow[((uint16_t)ny << RC_MAP_SHIFT) + (uint16_t)nx] == (uint16_t)(d - 1u))
        {
            *tdx = (int16_t)((nx << 10) + 512 - ex);
            *tdy = (int16_t)((ny << 10) + 512 - ey);
            return;
        }
    }
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
                    int16_t sdx, sdy, sd;

                    SteerTarget(e->x, e->y, &sdx, &sdy);
                    sd = Dist10(sdx, sdy);
                    if(sd < 1) sd = 1;
                    mx = (int16_t)(((int32_t)sdx * sp) / sd);
                    my = (int16_t)(((int32_t)sdy * sp) / sd);
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
            int16_t sdx, sdy, sd;

            SteerTarget(e->x, e->y, &sdx, &sdy);
            sd = Dist10(sdx, sdy);
            if(sd < 1) sd = 1;
            mx = (int16_t)(((int32_t)sdx * sp) / sd);
            my = (int16_t)(((int32_t)sdy * sp) / sd);

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

    // Side margins around the 3D view are always black.
    if(RC_VIEW_X0 > 0)
        Graphics_FillRect(0, RC_VIEW_Y0, RC_VIEW_X0 - 1, RC_VIEW_Y1, C_BLACK);
    if(RC_VIEW_X0 + RC_VIEW_W <= SCREEN_W - 1)
        Graphics_FillRect(RC_VIEW_X0 + RC_VIEW_W, RC_VIEW_Y0, SCREEN_W - 1, RC_VIEW_Y1, C_BLACK);
}

// Bottom line: what is active, else the controls.
static void DrawHint(void)
{
    char    buf[32];
    int16_t n;

    Graphics_FillRect(0, 217, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);
    if(s_doorNear)
    {
        uint16_t cost = DoorCost();

        n  = Cat(buf, 0, "DOOR ", 5);
        n += Itoa(buf + n, cost);
        Cat(buf, n, "G - press stick", 15);
        DrawText(buf, SCREEN_W / 2, 229, (s_gold >= cost) ? C_GREEN : C_BAD);
    }
    else if(s_aegis) DrawText("SHIELD UP!", SCREEN_W / 2, 229, COLOR_SKY_BLUE);
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
            n  = Cat(buf, 0, "ROUND ", 6);
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
        Graphics_FillRect(XP_BAR_X0 + 1, XP_BAR_Y0 + 1, XP_BAR_X1 - 1, XP_BAR_Y1 - 1, COLOR_TETRIS_GRID);
        w = (int16_t)(((int32_t)s_xp * (XP_BAR_X1 - XP_BAR_X0 - 1)) / XpNeed());
        if(w > 0)
            Graphics_FillRect(XP_BAR_X0 + 1, XP_BAR_Y0 + 1, XP_BAR_X0 + w,
                              XP_BAR_Y1 - 1, C_XP);
    }

    if(s_hud & HUD_STAFF)
    {
        Graphics_FillRect(0, RC_VIEW_Y1 + 2, 189, 215, C_BLACK);
        DrawText(SlotName(s_equip), 85, 203, SlotColor(s_equip));
        n  = Cat(buf, 0, "MP ", 3);
        Itoa(buf + n, SlotMana(s_equip));
        DrawSmall(buf, 165, 204, C_MANA, C_BLACK);
        s_cdShown = -1;                     // redraw the cooldown strip too
    }

    if(s_hud & HUD_LEFT)
    {
        Graphics_FillRect(190, RC_VIEW_Y1 + 2, SCREEN_W - 1, 215, C_BLACK);
        if(s_roundGap)
        {
            n  = Cat(buf, 0, "Next round ", 11);
            Itoa(buf + n, (uint16_t)((s_roundGap + 29u) / 30u));
            DrawText(buf, 255, 203, C_GREEN);
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

    if(s_cdShown >= 0 && w > s_cdShown && w < CD_STRIP_W)
    {
        // filling up: only the new part
        Graphics_FillRect(CD_STRIP_X0 + s_cdShown, 213, CD_STRIP_X0 + w - 1, 215, C_GRAY);
    }
    else
    {
        Graphics_FillRect(CD_STRIP_X0, 213, CD_STRIP_X0 + CD_STRIP_W - 1, 215, C_DKGRAY);
        if(w > 0)
            Graphics_FillRect(CD_STRIP_X0, 213, CD_STRIP_X0 + w - 1, 215,
                              (w == CD_STRIP_W) ? SlotColor(s_equip) : C_GRAY);
    }
    s_cdShown = w;
}

// Mana bar: vertical, in the free column left of the 3D view (x 1..8).
// Only the pixels that changed since last frame are redrawn.
#define MANA_X0   1
#define MANA_X1   8
static void DrawManaBar(void)
{
    int16_t  y0 = RC_VIEW_Y0, y1 = RC_VIEW_Y1;
    int16_t  inB = (int16_t)(y1 - 1);                   // bottom inner row
    int16_t  inH = (int16_t)(RC_VIEW_H - 2);
    int16_t  h   = (int16_t)(((int32_t)s_mana * inH) / MANA_MAX);
    bool     fl  = (s_manaFlash != 0);
    uint32_t col = fl ? C_RED : C_MANA;

    if(s_manaFlash) s_manaFlash--;

    if(h == s_manaShown && fl == s_manaFlashShown) return;

    if(s_manaShown < 0 || fl != s_manaFlashShown)       // full repaint
    {
        Graphics_FillRect(0, y0, MANA_X1 + 1, y1, C_BLACK);
        Graphics_FillRect(MANA_X0, y0, MANA_X1, y1, C_DKGRAY);
        Graphics_FillRect(MANA_X0 + 1, y0 + 1, MANA_X1 - 1, y1 - 1, COLOR_TETRIS_GRID);
        if(h > 0)
            Graphics_FillRect(MANA_X0 + 1, inB - h + 1, MANA_X1 - 1, inB, col);
    }
    else if(h > s_manaShown)                            // filled up
        Graphics_FillRect(MANA_X0 + 1, inB - h + 1, MANA_X1 - 1, inB - s_manaShown, col);
    else                                                // used some
        Graphics_FillRect(MANA_X0 + 1, inB - s_manaShown + 1, MANA_X1 - 1, inB - h,
                          COLOR_TETRIS_GRID);

    s_manaShown      = h;
    s_manaFlashShown = fl;
}

//*****************************************************************************
// 3D view
//*****************************************************************************
//-----------------------------------------------------------------------------
// Strip run lists
//-----------------------------------------------------------------------------
#define ID_CEIL     0u
#define ID_FLOOR    1u
#define ID_SPR      64u                 // + palette * 8 + letter
#define ID_TINT     192u                // + tint index (1..3)
#define ID_WPN      200u                // + palette letter
#define ID_UNKNOWN  255u                // "something else is on the LCD here"
#define ID_WALL(tile, shade, side)  (2u + (tile) * 8u + (shade) * 2u + (side))

// One run entry: last row in the high byte, colour id in the low byte. The shift is
// done on an unsigned 16-bit value (rows >= 128 would overflow a signed 16-bit int).
#define RUN_MAKE(row, id)  ((uint16_t)(((uint16_t)(row) << 8) | (uint16_t)(id)))

static uint32_t IdColor(uint16_t id)
{
    if(id == ID_CEIL)  return COL_CEIL;
    if(id == ID_FLOOR) return COL_FLOOR;
    if(id < ID_SPR)  { id -= 2u;       return s_wallCol[id >> 3][(id >> 1) & 3u][id & 1u]; }
    if(id < ID_TINT) { id -= ID_SPR;   return s_pal[id >> 3][id & 7u]; }
    if(id < ID_WPN)  return kTint[id - ID_TINT];
    return s_wPal[id - ID_WPN];
}

// Strip index of column c (half = 0/1 inside the weapon columns).
static uint16_t StripIndex(uint16_t c, uint16_t half)
{
    (void)half;                                 // strips are never split any more
    return c;
}

static void InvalidateView(void)
{
    uint16_t c;

    for(c = 0; c < RC_STRIPS; c++)
    {
        s_sl[c][0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
        s_sn[c]    = 1;
    }
    for(c = 0; c < RC_COLS; c++)
    {
        s_rngT[c] = RC_VIEW_H;  s_rngB[c] = -1;
        s_prvT[c] = RC_VIEW_H;  s_prvB[c] = -1;
    }
}

// Remember that rows y0..y1 of column c hold beam pixels this frame
// (they are repainted next frame because the LCD no longer matches the model).
static void AddRange(int16_t c, int16_t y0, int16_t y1)
{
    if(y0 < 0) y0 = 0;
    if(y1 > RC_VIEW_H - 1) y1 = RC_VIEW_H - 1;
    if(y0 < s_rngT[c]) s_rngT[c] = y0;
    if(y1 > s_rngB[c]) s_rngB[c] = y1;
}

// Paint rows y0..y1 of a run list with colour `id` (later paints cover earlier
// ones). Returns false, leaving the list untouched, if it would not fit.
static bool ListPaint(uint16_t *l, uint16_t *n, uint16_t cap, int16_t y0, int16_t y1, uint16_t id)
{
    uint16_t tmp[RC_MAXN + 3];
    uint16_t m = 0, i, rid;
    int16_t  rs = 0, re;
    bool     placed = false;

    if(y0 < 0) y0 = 0;
    if(y1 > RC_VIEW_H - 1) y1 = RC_VIEW_H - 1;
    if(y1 < y0) return true;

#define LP_PUSH(e, ident)                                                   \
    do {                                                                    \
        if(m && (tmp[m - 1] & 255u) == (ident))                             \
            tmp[m - 1] = (uint16_t)(((uint16_t)(e) << 8) | (ident));        \
        else                                                                \
            tmp[m++] = (uint16_t)(((uint16_t)(e) << 8) | (ident));          \
    } while(0)

    for(i = 0; i < *n; i++)
    {
        re  = (int16_t)(l[i] >> 8);
        rid = l[i] & 255u;

        if(rs <= y0 - 1)                                    // part above the new run
            LP_PUSH((re < y0 - 1) ? re : (int16_t)(y0 - 1), rid);

        if(!placed && re >= y0)
        {
            LP_PUSH(y1, id);
            placed = true;
        }

        if(re > y1)                                         // part below it
            LP_PUSH(re, rid);

        rs = (int16_t)(re + 1);
    }
#undef LP_PUSH

    if(m > cap) { g_rcOverflow++; return false; }

    for(i = 0; i < m; i++) l[i] = tmp[i];
    *n = m;
    return true;
}

// Send the differences between what is on the LCD (old list) and the new
// picture, then remember the new picture. One FillRect per changed run.
static void EmitStrip(uint16_t si, int16_t x0, int16_t w, const uint16_t *nl, uint16_t nn)
{
    uint16_t *ol = s_sl[si];
    uint16_t  on = s_sn[si];
    uint16_t  i = 0, j = 0, k, oid, nid, pId = 0;
    int16_t   row = 0, oe, ne, e, pS = 0, pE = -1;
    int16_t   x1 = (int16_t)(x0 + w - 1);
    bool      pend = false;

    if(on == 0) { ol[0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN); on = 1; }

    while(i < on && j < nn)
    {
        oe  = (int16_t)(ol[i] >> 8);   oid = ol[i] & 255u;
        ne  = (int16_t)(nl[j] >> 8);   nid = nl[j] & 255u;
        e   = (oe < ne) ? oe : ne;

        if(oid != nid)
        {
            if(pend && pId == nid && pE + 1 == row)
                pE = e;                                     // extends the pending fill
            else
            {
                if(pend)
                    Graphics_FillRect(x0, (int16_t)(RC_VIEW_Y0 + pS), x1,
                                      (int16_t)(RC_VIEW_Y0 + pE), IdColor(pId));
                pend = true;  pS = row;  pE = e;  pId = nid;
            }
        }

        row = (int16_t)(e + 1);
        if(oe == e) i++;
        if(ne == e) j++;
    }

    if(pend)
        Graphics_FillRect(x0, (int16_t)(RC_VIEW_Y0 + pS), x1,
                          (int16_t)(RC_VIEW_Y0 + pE), IdColor(pId));

    if(nn <= RC_MAXR)
    {
        for(k = 0; k < nn; k++) ol[k] = nl[k];
        s_sn[si] = nn;
    }
    else                                    // too busy to remember: the tail repaints next frame
    {
        for(k = 0; k < RC_MAXR - 1; k++) ol[k] = nl[k];
        ol[RC_MAXR - 1] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
        s_sn[si] = RC_MAXR;
        g_rcTrunc++;
    }
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
            case OB_MANA:
                if(o->life - o->age < 90u && (o->age & 4u)) break;      // blink before it vanishes
                ProjectOne(o->x, o->y, SH_FLASK, PT_FLASK_MANA, 5, (uint16_t)o->z, 0);
                break;
            default:                                    // cloud / spark
                ProjectOne(o->x, o->y, SH_PUFF, o->pal,
                           (uint16_t)(o->sc + ((o->age >> 1) & 3u)),
                           (uint16_t)o->z, 0);
                break;
        }
    }
}

// Paint one sprite's pixel column (the one under screen x = xc) into a strip.
static void PaintSlice(uint16_t *nl, uint16_t *nn, const Spr *sp, int16_t xc)
{
    uint16_t u, n, r, v, id;
    int16_t  xs = (int16_t)(xc - sp->x0);

    if(xs < 0) xs = 0;
    u = (uint16_t)(((int32_t)xs * sp->uStep) >> 8);
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

        id = sp->flash ? (uint16_t)(ID_TINT + sp->flash)
                       : (uint16_t)(ID_SPR + sp->type * 8u + RUN_CI(v));
        ListPaint(nl, nn, RC_MAXN, y0, y1, id);
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
// Held item: build once per equip, painted into the strip lists
//*****************************************************************************
static char WpnCell(const char *art, uint16_t x, uint16_t y)
{
    if(y < WPN_ART_ROWS) return art[y * WPN_W + x];
    return kWpnArm[(y - WPN_ART_ROWS) * WPN_W + x];
}

static void WpnBuild(uint16_t slot)
{
    uint16_t k, y, y1, n, i;
    char     ch;
    const char *art = kWpnArt[slot];

    for(i = 0; i < 26; i++) s_wPal[i] = COLOR_MAGENTA;
    for(i = 0; kWpnPal[slot][i].ch; i++)
        s_wPal[kWpnPal[slot][i].ch - 'a'] = kWpnPal[slot][i].rgb;

    for(k = 0; k < WPN_W; k++)                          // vertical runs per art column
    {
        n = 0;
        y = 0;
        while(y < WPN_H)
        {
            ch = WpnCell(art, k, y);
            if(ch == '.') { y++; continue; }

            y1 = y;
            while(y1 + 1 < WPN_H && WpnCell(art, k, y1 + 1) == ch) y1++;

            if(n < WPN_MAXRUNS)
                s_wRun[k][n++] = (uint16_t)(y | (y1 << 5) | ((uint16_t)(ch - 'a') << 10));
            y = y1 + 1;
        }
        s_wRunN[k] = n;
    }
    s_wLoaded = slot;
}

// Paint the held item's art column ak (0..WPN_W-1) into a strip list.
static void PaintWeapon(uint16_t *nl, uint16_t *nn, uint16_t ak)
{
    uint16_t r, v;

    for(r = 0; r < s_wRunN[ak]; r++)
    {
        v = s_wRun[ak][r];
        ListPaint(nl, nn, RC_MAXN,
                  (int16_t)(WPN_Y0 + (int16_t)(v & 31u) * WPN_SCALE),
                  (int16_t)(WPN_Y0 + (int16_t)(((v >> 5) & 31u) + 1) * WPN_SCALE - 1),
                  (uint16_t)(ID_WPN + (v >> 10)));
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
    uint16_t c, k, tile, side, dist, perp, lineH, shade, h, nStrips, nn, si, wallId;
    int16_t  top, bot, sx, w, ak;
    uint16_t nl[RC_MAXN];
    int16_t  cxp = RC_VIEW_X0 + RC_VIEW_W / 2;
    int16_t  cyp = RC_VIEW_Y0 + RC_HORIZON;

    if(s_invalidate)
    {
        InvalidateView();
        s_invalidate = false;
    }

    if(s_wLoaded != s_equip)
    {
        WpnBuild(s_equip);          // new art AND palette: repaint the strips behind the hand
        for(c = WPN_C0; c <= WPN_C1; c++)
            for(h = 0; h < 1; h++)
            {
                si = StripIndex(c, h);
                s_sl[si][0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
                s_sn[si]    = 1;
            }
    }

    // Beam overlay bookkeeping: last frame's overlay rows must be repainted.
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
        lineH &= ~1u;                                   // even heights: fewer changed rows
        if(lineH > RC_VIEW_H) lineH = RC_VIEW_H;
        if(lineH < 2)         lineH = 2;

        top = (int16_t)((RC_VIEW_H - lineH) >> 1);
        bot = (int16_t)(top + lineH - 1);

        shade  = (perp < 512) ? 0 : (perp < 1024) ? 1 : (perp < 1792) ? 2 : 3;
        if(tile == TILE_PORTAL && s_portalPhase) tile = TILE_PORTAL + 1u;   // pulse
        wallId = ID_WALL(tile, shade, side);

        nStrips = 1u;
        w       = RC_COL_W;

        for(h = 0; h < nStrips; h++)
        {
            sx = (int16_t)(RC_VIEW_X0 + (int16_t)c * RC_COL_W + (int16_t)h * w);
            si = StripIndex(c, h);

            // background: ceiling / wall / floor
            nn = 0;
            if(top > 0)
                nl[nn++] = RUN_MAKE(top - 1, ID_CEIL);
            nl[nn++] = RUN_MAKE(bot, wallId);
            if(bot < RC_VIEW_H - 1)
                nl[nn++] = RUN_MAKE(RC_VIEW_H - 1, ID_FLOOR);

            // sprites, far to near, hidden behind nearer wall
            for(k = 0; k < s_nSpr; k++)
                if(c >= s_spr[k].c0 && c <= s_spr[k].c1 && s_spr[k].dist < perp)
                    PaintSlice(nl, &nn, &s_spr[k], (int16_t)(sx + (w >> 1)));

            // held item on top of everything
            if((int16_t)c >= WPN_C0 && (int16_t)c <= WPN_C1)
            {
                ak = (int16_t)((int16_t)c - WPN_C0);        // art column == strip
                if(ak < WPN_W) PaintWeapon(nl, &nn, (uint16_t)ak);
            }

            // beam pixels from last frame are not in the model: repaint them
            if(s_prvT[c] <= s_prvB[c])
            {
                uint16_t *ol = s_sl[si];
                uint16_t  on = s_sn[si];

                if(on == 0 || !ListPaint(ol, &on, RC_MAXR, s_prvT[c], s_prvB[c], ID_UNKNOWN))
                {
                    ol[0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
                    on = 1;
                }
                s_sn[si] = on;
            }

            EmitStrip(si, sx, w, nl, nn);
        }
    }

    // Overlays: staff bolts, then the crosshair.
    if(s_fxTimer) DrawFx();

    if(s_mode == MODE_PLAY)
    {
        Graphics_FillRect(cxp - 5, cyp, cxp + 5, cyp, COLOR_SILVER);
        Graphics_FillRect(cxp, cyp - 5, cxp, cyp + 5, COLOR_SILVER);
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
        DrawText("BOSS ROUND", SCREEN_W / 2, 105, C_RED);
        DrawText("The Lich awaits", SCREEN_W / 2, 126, C_GRAY);
    }
    else
    {
        n  = Cat(buf, 0, "ROUND ", 6);
        n += Itoa(buf + n, s_level + 1);
        DrawText(buf, SCREEN_W / 2, 105, C_WHITE);
        DrawText("Buy doors with the stick", SCREEN_W / 2, 126, C_GRAY);
    }
}

static void DrawEndScreen(void)
{
    char    buf[28];
    int16_t n;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    DrawText("GAME OVER", SCREEN_W / 2, 55, C_RED);
    n  = Cat(buf, 0, "Died in round ", 14);
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
    uint32_t fill = sel ? COLOR_CHARCOAL : COLOR_TETRIS_GRID;
    uint32_t band;
    const Offer *of;
    const char *kindName;
    char     stat[16], line[16];
    uint16_t st, cost, k, n;
    int16_t  y;

    Graphics_FillRect(x0, y0, x1, y1, sel ? C_CYAN : COLOR_CHARCOAL);       // border
    Graphics_FillRect(x0 + 2, y0 + 2, x1 - 2, y1 - 2, fill);

    if(s_card[i].off == CARD_NONE)
    {
        DrawSmall("sold out", cx, (int16_t)(y0 + CARD_H / 2), C_DKGRAY, fill);
        return;
    }

    of = &kOffer[s_card[i].off];
    switch(of->kind)
    {
        case K_STAT:  kindName = "UPGRADE"; band = COLOR_CHARCOAL; break;
        case K_TRAIT: kindName = "TRAIT";   band = COLOR_PURPLE; break;
        case K_STAFF: kindName = "STAFF";   band = COLOR_OLIVE; break;
        default:      kindName = "MAGIC";   band = COLOR_TEAL; break;
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
    uint32_t fill = sel ? COLOR_CHARCOAL : COLOR_TETRIS_GRID;
    uint16_t cost = RerollCost();

    Graphics_FillRect(RR_X0, RR_Y0, RR_X1, RR_Y1, sel ? C_CYAN : COLOR_CHARCOAL);
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
    s_mana = MANA_MAX;
    s_manaAcc = s_manaFlash = 0;
    s_manaShown = -1;
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

// A new round starts without touching the map: only the wave numbers change.
static void StartRound(uint16_t r)
{
    s_level = r;
    MakeLevelDef(r, &s_lv);
    s_toSpawn     = s_lv.total;
    s_spawnTimer  = 45;
    s_bossSpawned = s_bossAlive = 0;
    s_roundGap    = 0;
    s_hud        |= HUD_TOPR | HUD_LEFT;
    if(r > 0u) Sfx(&sfx_level_up);
}

// New run: load the ring map, lock every door, start round 1.
static void StartLevel(uint16_t lv)
{
    uint16_t i;

    LoadMap();

    for(i = 0; i < RC_MAX_ENEMIES; i++) s_en[i].alive = 0;
    for(i = 0; i < RC_MAX_OBJ; i++)     s_obj[i].alive = 0;
    for(i = 0; i < RC_NUM_SECTIONS; i++) s_secOpen[i] = (i == SEC_START);
    for(i = 0; i <= RC_NUM_DOORS; i++)   s_doorBought[i] = false;
    s_doorsBought = 0;
    s_doorNear    = 0;
    s_doorShown   = 0xFFFFu;
    s_flowTx      = -1;
    s_alive       = 0;
    s_spawnIdx    = 0;

    StartRound(lv);
    s_spawnTimer = 90;

    s_ang    = 256;                     // face "down" the map: door 6 is right there
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

// Which closed door (if any) is the player facing and close to?
static void UpdateDoorNear(void)
{
    uint16_t k, best = 0;
    int16_t  bestD = 32767, dx, dy, d;
    int32_t  fwd;
    uint16_t key;

    for(k = 1; k <= RC_NUM_DOORS; k++)
    {
        if(s_doorBought[k]) continue;
        dx = (int16_t)(s_doorCx[k] - s_px);
        dy = (int16_t)(s_doorCy[k] - s_py);
        d  = Dist10(dx, dy);
        if(d >= DOOR_USE_DIST || d >= bestD) continue;
        fwd = (((int32_t)dx * COS(s_ang)) + ((int32_t)dy * SIN(s_ang))) >> 14;
        if(fwd * 10L < (int32_t)d * 6L) continue;       // must be roughly in front
        best  = k;
        bestD = d;
    }

    s_doorNear = best;
    key = (uint16_t)(best ? (best * 2u + ((s_gold >= DoorCost()) ? 1u : 0u)) : 0u);
    if(key != s_doorShown)
    {
        s_doorShown = key;
        s_hud      |= HUD_HINT;
    }
}

static void BuyDoor(uint16_t k)
{
    uint16_t i, cost = DoorCost();

    if(s_gold < cost) { Sfx(&sfx_menu_bump); return; }

    s_gold -= cost;
    s_doorsBought++;
    s_doorBought[k] = true;
    for(i = 0; i < RC_MAP_CELLS; i++)                   // the doorway is now open floor
        if(kMaps[0][i >> RC_MAP_SHIFT][i & 15u] == (char)('0' + k)) s_tile[i] = 0;

    s_secOpen[k - 1u]                = true;            // both sides are unlocked
    s_secOpen[k % RC_NUM_SECTIONS]   = true;

    s_flowTx    = -1;                                   // re-path through the new opening
    s_doorShown = 0xFFFFu;
    s_viewDirty = 2;
    s_hud      |= HUD_GOLD | HUD_HINT;
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

    // Mana refills slowly while the stick is released (turning still counts as standing)
    if(jx || jy)
        s_manaAcc = 0;
    else if(s_mana < MANA_MAX && ++s_manaAcc >= MANA_REGEN_FRAMES)
    {
        s_manaAcc = 0;
        s_mana++;
    }

    // Doors: face one and press the stick to buy it
    UpdateDoorNear();
    if(g_joy.btnPressed && s_doorNear) BuyDoor(s_doorNear);

    // Use the equipped staff / item (hold = auto-use)
    if(g_dpad.u) UseEquipped();

    EnsureFlow();
    UpdateObjects();
    UpdateEnemies();
    TrySpawn();

    // Rounds never stop: a short breather, then the next wave starts by itself.
    if(s_mode == MODE_PLAY)
    {
        if(s_roundGap == 0)
        {
            bool cleared = s_lv.boss ? (s_bossSpawned && !s_bossAlive)
                                     : (s_toSpawn == 0 && s_alive == 0);
            if(cleared)
            {
                s_roundGap = ROUND_GAP_FRAMES;
                s_hud     |= HUD_LEFT;
                Sfx(&sfx_level_up);
            }
        }
        else if(--s_roundGap == 0)
            StartRound(s_level + 1u);
        else if((s_roundGap % 30u) == 0)
            s_hud |= HUD_LEFT;
    }

    if(moved || s_alive || s_fxTimer || s_haste || s_aegis)
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
static uint32_t s_lastTick;             // end of the previous frame (for the frame cap)
static uint32_t s_lastUpd;              // time of the previous update
static uint32_t s_owed;                 // simulation time not yet run, in timer ticks

#define RC_MAX_CATCHUP  3               // never run more than this many ticks per frame

static void PaceInit(void)
{
    s_lastTick = TimerNow();
    s_lastUpd  = s_lastTick;
    s_owed     = 0;
}

// Busy-wait until one frame period has passed since the previous frame ended.
// The counter counts DOWN, so elapsed = last - now (wrap-safe in uint32).
static void PaceWait(void)
{
    while((uint32_t)(s_lastTick - TimerNow()) < RC_FRAME_TICKS) { }
    s_lastTick = TimerNow();
}

// The game simulates at a fixed 30 ticks per second no matter how long drawing
// took: if a frame was slow, the missing ticks are run now (up to
// RC_MAX_CATCHUP), so walking speed, cooldowns and enemy speed stay in real
// time instead of slowing down with the frame rate.
static uint16_t TicksOwed(void)
{
    uint32_t now = TimerNow();
    uint16_t n   = 0;

    s_owed   += (uint32_t)(s_lastUpd - now);
    s_lastUpd = now;

    while(s_owed >= RC_FRAME_TICKS && n < RC_MAX_CATCHUP)
    {
        s_owed -= RC_FRAME_TICKS;
        n++;
    }
    if(s_owed > RC_FRAME_TICKS) s_owed = RC_FRAME_TICKS;    // too far behind: let go
    if(n == 0) { n = 1; s_owed = 0; }                       // always make progress
    return n;
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
    uint16_t n, i;

    switch(s_mode)
    {
        case MODE_INTRO:
            n = TicksOwed();
            s_timer = (s_timer > n) ? (uint16_t)(s_timer - n) : 0;
            if(s_timer == 0)
            {
                s_mode       = MODE_PLAY;
                s_invalidate = true;            // wipes the banner
                s_viewDirty  = 2;
            }
            break;

        case MODE_PLAY:
            n = TicksOwed();
            for(i = 0; i < n && s_mode == MODE_PLAY; i++)
            {
                UpdatePlay();
                g_dpad.uPressed = g_dpad.dPressed = 0;      // button edges count once,
                g_dpad.lPressed = g_dpad.rPressed = 0;      // not once per catch-up tick
                g_joy.btnPressed = 0;
            }
            if(s_lvPending && s_mode == MODE_PLAY) EnterCards();
            break;

        case MODE_CARDS:
            UpdateCards();
            s_lastUpd = TimerNow();             // time spent choosing is not owed to the game
            s_owed    = 0;
            break;

        case MODE_CLEAR:
            n = TicksOwed();
            s_timer = (s_timer > n) ? (uint16_t)(s_timer - n) : 0;
            if(s_timer == 0)
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
        s_manaShown  = -1;
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
    DrawManaBar();

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
    PaceInit();                                 // don't "catch up" after the pause
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
    COLOR_CRIMSON,                             // menu tile color
    RC_FRAME_US,
    RC_Init, RC_Update, RC_Render,
    RC_Exit,
    NULL,                                   // not a legacy app
    RC_Redraw
};
