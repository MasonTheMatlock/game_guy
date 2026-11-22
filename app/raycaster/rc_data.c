// rc_data.c - constant game data (enemies, waves, gear, stats, runes, rooms, map).
// Every number here is a placeholder for the balance pass (rc.md section 18, stage 8).
#include "rc_priv.h"


//*****************************************************************************
// Enemies. Tuning anchor (rc.md 21.3): at wave 1 the Ray Wand (3 damage) kills a
// Goblin in 2 hits and a Hobgoblin in about 4 (its shield blocks 1 per frontal hit).
// Ordinary enemies do NOT scale with the wave: pressure comes from numbers.
//*****************************************************************************
const EnemyDef rc_kEnemy[ET_COUNT] =
{
    //  hp  spd gold  xp  rad reach scale zoff shape            pal            shapeB          touch
    {    6,  26,   5,  4, 330,  540,  12,   0, SH_GOBLIN,       PT_GOBLIN,     0,              DMG_SMALL }, // goblin
    {    3,  44,   4,  3, 260,  480,   8,   5, SH_BAT,          PT_BAT,        0,              DMG_SMALL }, // bat
    {   12,  17,  12,  8, 440,  620,  17,   0, SH_HOBGOB,       PT_HOBGOB,     0,              DMG_SMALL }, // hobgoblin (its charge is a big hit)
    {    6,  30,   2,  1, 330,  540,  14,   0, SH_SKELETON,     PT_SKELETON,   0,              DMG_SMALL }, // skeleton (Lich Lord summon)
    {    9,  12,  10,  7, 420,  520,  16,   4, SH_BEHOLDER,     PT_BEHOLDER,   0,              DMG_SMALL }, // watcher
    {  160,  22,  80, 60, 620,  800,  26,   0, SH_LICH,         PT_LICH,       SH_LICH_B,      DMG_BOSS  }, // Lich Lord (mini-boss)
    {  900,  15, 300,150, 820,  900,  38,   0, SH_DRAGON,       PT_DRAGON,     0,              DMG_BOSS  }, // Ancient Dragon
    {    9,  10,   6,  5, 380,  560,  14,   0, SH_SLIME,        PT_SLIME,      0,              DMG_SMALL }, // slime
    {    3,  20,   2,  1, 260,  420,   9,   0, SH_SLIME,        PT_SLIME_S,    0,              DMG_SMALL }, // small slime
    {    8,  20,   7,  6, 300,  500,  10,   6, SH_SPIDER,       PT_SPIDER,     0,              DMG_SMALL }, // spider
    {    9,  24,   8,  7, 360,  520,  14,   3, SH_GHOST,        PT_GHOST,      0,              DMG_SMALL }, // ghost
    {    4,  38,   6,  5, 280,  500,  10,   0, SH_BOMBER,       PT_BOMBER,     0,              0         }, // bomber (the blast hurts)
    {    6,  22,   7,  6, 330,  540,  14,   0, SH_ARCHER,       PT_ARCHER,     0,              DMG_SMALL }, // skeleton archer
    {   20,  26,  30, 20, 420,  620,  12,   0, SH_CHEST,        PT_CHEST,      0,              DMG_BIG   }, // mimic
    {  120,   9,  80, 50, 620,  800,  24,   0, SH_SLIMEKING_A,  PT_SLIMEKING,  SH_SLIMEKING_B, DMG_BOSS  }, // Slime King
    {  140,   8,  90, 55, 640,  820,  26,   0, SH_GOLEM_A,      PT_GOLEM_FIRE, SH_GOLEM_B,     DMG_BOSS  }, // Elemental Golem
    {  110,  28,  90, 60, 420,  620,  20,   0, SH_VAMP_A,       PT_VAMP,       SH_VAMP_B,      DMG_BOSS  }, // Vampire Lord
    {  130,  12, 100, 60, 700,  820,  26,   4, SH_EYEKING_A,    PT_EYEKING,    SH_EYEKING_B,   DMG_BOSS  }, // Beholder
};

// Enemy names, in ET_* order (packed by tools/textpack.py; read with rc_EnemyName(t)).
#define ENEMY_NAME(n)
ENEMY_NAME("Goblin")     ENEMY_NAME("Bat")          ENEMY_NAME("Hobgoblin")       ENEMY_NAME("Skeleton")
ENEMY_NAME("Watcher")    ENEMY_NAME("Lich Lord")    ENEMY_NAME("Ancient Dragon")  ENEMY_NAME("Slime")
ENEMY_NAME("Small Slime") ENEMY_NAME("Spider")      ENEMY_NAME("Ghost")           ENEMY_NAME("Bomber")
ENEMY_NAME("Skeleton Archer") ENEMY_NAME("Mimic")   ENEMY_NAME("Slime King")      ENEMY_NAME("Elemental Golem")
ENEMY_NAME("Vampire Lord") ENEMY_NAME("Beholder")

const uint16_t rc_kMini[RC_NUM_MINI] = { ET_EYEKING, ET_SLIMEKING, ET_LICH, ET_GOLEM, ET_VAMPIRE };


//*****************************************************************************
// Waves (rc.md 3.5 and 13.2)
//*****************************************************************************
// One new enemy type per wave number.
const uint16_t rc_kUnlock[NUM_ORDINARY] =
    { ET_GOBLIN, ET_BAT, ET_ARCHER, ET_HOBGOB, ET_SPIDER, ET_SLIME, ET_GHOST, ET_WATCHER, ET_BOMBER };

// Themes picked at random from wave 11 on (never twice in a row, only if every enemy is unlocked).
const WaveTheme rc_kThemes[NUM_THEMES] =
{
    { "Goblin Horde", 2, { ET_GOBLIN,  ET_HOBGOB,  0 } },
    { "Archer Line",  2, { ET_ARCHER,  ET_WATCHER, 0 } },
    { "Web Nest",     2, { ET_SPIDER,  ET_SLIME,   0 } },
    { "Haunting",     2, { ET_GHOST,   ET_BAT,     0 } },
    { "Swarm",        2, { ET_BAT,     ET_BOMBER,  0 } },
    { "Bomb Rush",    2, { ET_BOMBER,  ET_GOBLIN,  0 } },
    { "Slime Tide",   1, { ET_SLIME,   0,          0 } },
    { "Ambush",       3, { ET_BAT,     ET_GOBLIN,  ET_ARCHER } },
};

// Fixed opening, waves 1..10 (n = 0: treasure wave, or mini-boss + treasure).
const WaveTheme rc_kOpening[10] =
{
    { "Goblin Horde", 1, { ET_GOBLIN,  0,         0 } },
    { "Swarm",        2, { ET_GOBLIN,  ET_BAT,    0 } },
    { "Treasure",     0, { 0,          0,         0 } },
    { "Goblin Horde", 3, { ET_HOBGOB,  ET_GOBLIN, ET_ARCHER } },
    { "Web Nest",     3, { ET_SPIDER,  ET_GOBLIN, ET_ARCHER } },
    { "Treasure",     0, { 0,          0,         0 } },
    { "Haunting",     3, { ET_GHOST,   ET_BAT,    ET_SLIME } },
    { "Archer Line",  2, { ET_ARCHER,  ET_WATCHER,0 } },
    { "Treasure",     0, { 0,          0,         0 } },
    { "Bomb Rush",    2, { ET_BOMBER,  ET_GOBLIN, 0 } },
};


const int16_t  rc_kPotRadius[3] = { 1300, 1500, 1100 };
const uint16_t rc_kPotDmg[3]    = {    5,    2,    1 };
const uint16_t rc_kPotLife[3]   = {   18,   24,  200 };
const uint16_t rc_kPotScale[3]  = {   22,   24,   18 };


//*****************************************************************************
// Direct stat upgrades (rc.md 6.2): max level 8, each level worth a bit less.
// rc_kSoft[lv] is the total effect after lv levels in 1/256 of one full step.
//*****************************************************************************
const uint16_t rc_kSoft[STAT_MAX_LV + 1] = { 0, 256, 486, 691, 870, 1024, 1152, 1254, 1331 };

// name, description (both packed by tools/textpack.py: rc_StatName / rc_StatDesc), step
#define STAT(n, d, step)    { step }
const StatDef rc_kStat[ST_COUNT] =
{
    STAT("Damage",       "+15% damage",            15),
    STAT("Max Health",   "+1 heart and|heal one",    1),
    STAT("Move Speed",   "+8% walk|speed",            8),
    STAT("Attack Speed", "-10% time|between shots",  10),
    STAT("Max Mana",     "+20 max|mana",             20),
    STAT("Mana Regen",   "+15% mana|regen",          15),
    STAT("Resistance",   "6% chance to|block half|a heart", 6),
    STAT("Critical",     "+5% chance|of a 1.5x|hit",  5),
    STAT("Gold Find",    "+15% gold|from all|sources", 15),
};


//*****************************************************************************
// Runes (rc.md 9). cls: 0 general, 1 Pyromancer, 2 Stormcaller, 3 Necromancer,
// 4 Raycaster, 5 Warlock, 6 Midas. Class runes stay out of the offers until a
// class has been activated (Pass 2), and only work while that class is active.
//*****************************************************************************
// name, description (both packed by tools/textpack.py: rc_RuneName / rc_RuneDesc), class
#define RUNE(n, d, cls)     { cls }
const RuneDef rc_kRune[RU_COUNT] =
{
    RUNE("Haste",        "+12% move|speed",                   0),
    RUNE("Force",        "+20% damage",                        0),
    RUNE("Swiftness",    "-15% attack|and item|cooldowns",     0),
    RUNE("Echoes",       "20% chance|of an extra|beam",        0),
    RUNE("Vitality",     "+1 max|heart",                       0),
    RUNE("Leech",        "every 5th|kill heals|half a heart",  0),
    RUNE("Conservation", "-25% mana|costs",                    0),
    RUNE("Fortune",      "+25% gold",                          0),
    RUNE("Momentum",     "a kill gives|3s of +30%|speed",      0),
    RUNE("Hermit",       "+50% mana|regen",                    0),
    RUNE("Cinderwake",   "burning foes|leave fire|on death",   1),
    RUNE("Ashen Harvest","burning kill|restores|mana",          1),
    RUNE("Arc Conduit",  "lightning|may chain|once more",      2),
    RUNE("Static Rsrv",  "lightning|hits charge|the next one", 2),
    RUNE("Soul Siphon",  "kills may|restore|health",           3),
    RUNE("Bone Legion",  "stronger|longer army|skeletons",     3),
    RUNE("Piercing Lt",  "beams pierce|one more",              4),
    RUNE("Afterimage",   "hits may|split a|weak beam",         4),
    RUNE("Twin Spectrm", "+2 beams",                           4),
    RUNE("Astral Rico",  "beams bounce|and seek",              4),
    RUNE("Void Hunger",  "more damage|to wounded|foes",        5),
    RUNE("Eldritch Rch", "low mana|means more|damage",         5),
    RUNE("Gold Arsenal", "damage grows|with carried|gold",     6),
    RUNE("Golden Touch", "kills may|drop bonus|gold",          6),
};


//*****************************************************************************
// Rooms (tile rectangles, inclusive). Themes are fixed per chamber (rc.md 4.6).
//*****************************************************************************
// x0, y0, x1, y1, theme, name, flavour line (both packed by tools/textpack.py:
// rc_RoomName / rc_RoomFlavour; the flavour line is a toast on first entry, <= 30 chars)
#define ROOM(x0, y0, x1, y1, th, n, f)  { x0, y0, x1, y1, th }
const RoomDef rc_kRoom[RM_COUNT] =
{
    ROOM(21, 34, 41, 44, TH_WARRENS,  "Goblin Warrens", "Smells like goblin feet."),  // RM_S (start)
    ROOM( 1, 34, 19, 44, TH_CRYPT,    "Bone Crypt", "The dead sleep lightly here."),  // RM_SW
    ROOM( 3, 18,  6, 32, TH_BASEMENT, "West Corridor", "A cold draft howls past."),  // RM_WC
    ROOM( 1,  8, 19, 16, TH_SEWERS,   "Slime Sewers", "Something squelches nearby."),  // RM_NW
    ROOM(21,  8, 41, 16, TH_CHAPEL,   "Haunted Chapel", "Pews for a forgotten god."),  // RM_N
    ROOM(43,  8, 62, 16, TH_FORGE,    "Elemental Forge", "The anvils are still warm."),  // RM_NE
    ROOM(54, 18, 57, 32, TH_BASEMENT, "East Corridor", "Footprints. Lots of them."),  // RM_EC
    ROOM(43, 34, 62, 44, TH_OBSERV,   "Arcane Observatory", "The stars hum overhead."),  // RM_SE
    ROOM(23, 18, 37, 32, TH_DRAGON,   "Dragon Den", "It reeks of smoke and gold."),  // RM_DEN
    ROOM( 8, 18, 19, 24, TH_SEWERS,   "Slime Vault", "Slime-sealed treasure!"),  // RM_VAULT
    ROOM(26,  1, 36,  6, TH_CHAPEL,   "Haunted Shrine", "A bloody altar waits."),  // RM_SHRINE
    ROOM(59, 25, 62, 32, TH_OBSERV,   "Astral Laboratory", "Runes crackle in the air."),  // RM_LAB
    ROOM(39, 18, 53, 32, TH_WARRENS,  "Goblin Maze", "Goblins built this. Badly."),  // RM_MAZE
    ROOM( 2,  1, 18,  6, TH_FORGE,    "Goblin Treasury", "Goblin gold! Finders keepers."), // RM_TREASURY
    ROOM(44,  1, 61,  6, TH_OBSERV,   "Wason's Study", "Wason's study. Don't touch."),   // RM_STUDY
    ROOM( 8, 26, 20, 31, TH_CRYPT,    "Ossuary", "Bones, cages and regrets."),         // RM_OSSUARY
    ROOM(59, 19, 62, 23, TH_OBSERV,   "Mimic Closet", "Three chests. Hmm..."),         // RM_CLOSET
};


// The map itself lives in rc_map.c (generated by tools/mapgen.py, stored 2 chars per word).

//*****************************************************************************
// Map key: where everything is (tile x, y; x 0..63 left to right, y 0..47 top
// to bottom). Rectangles match rc_kRoom above.
//
//   NW Slime Sewers --4-- N Haunted Chapel --5-- NE Elemental Forge
//        |                 (Shrine above)                 |
//        3                      DDD                       6
//   W corridor   Slime Vault  DRAGON DEN  GOBLIN MAZE  E corridor  Astral Lab
//        2                      DDD                       7
//        |                                                |
//   SW Bone Crypt  --1-- S Goblin Warrens --8-- SE Arcane Observatory
//                           (START)
//
// CHAMBERS (fixed themes, rc.md 4.6)                x       y      key
//   S   Goblin Warrens (start)                    21-41   34-44     -
//   SW  Bone Crypt                                 1-19   34-44   (10,39)
//   NW  Slime Sewers                               1-19    8-16   (10,12)
//   N   Haunted Chapel                            21-41    8-16     -
//   NE  Elemental Forge                           43-62    8-16   (52,12)
//   SE  Arcane Observatory                        43-62   34-44   (52,39)
//   Chest nooks 'C': (14,7) (24,7) (50,7) above the top chambers,
//                    (14,45) (26,45) (50,45) below the bottom chambers.
//
// CORRIDORS
//   W corridor   x 3-5,   y 18-32   nooks 'v' at (6,26) (6,29) (6,32)
//   E corridor   x 55-57, y 18-32   nooks 'v' at (54,24) (54,28) (54,32)
//   Each corridor: a vendor and a chest in two nooks, decoration in the third.
//
// DOORS (bought with gold, rc_DoorCost)        tiles            connects
//   1  S-SW                                     x 20, y 38-40    S chamber  - SW chamber
//   2  SW-W                                     x 3-5, y 33      SW chamber - W corridor
//   3  W-NW                                     x 3-5, y 17      W corridor - NW chamber
//   4  NW-N                                     x 20, y 11-13    NW chamber - N chamber
//   5  N-NE                                     x 42, y 11-13    N chamber  - NE chamber
//   6  NE-E                                     x 55-57, y 17    NE chamber - E corridor
//   7  E-SE                                     x 55-57, y 33    E corridor - SE chamber
//   8  SE-S                                     x 42, y 38-40    SE chamber - S chamber
//   D  Dragon Den gates (open together)         x 29-31, y 17 and y 33   N / S chambers - Den
//
// SECRET ROOMS (secret door 's' needs a key)   room             door
//   Slime Vault        (chest 'V' at 14,21)     x 8-19, y 18-24  (6,21)+(7,21) from the W corridor
//   Haunted Shrine     (altar 'A' at 31,3)      x 26-36, y 1-6   (31,7) from the N chamber
//   Astral Laboratory  (altar 'L' at 60,28)     x 59-62, y 25-32 (58,29) from the E corridor
//   Goblin Maze        (chest at 46,25)         x 39-53, y 18-32 (40,33) from the S chamber;
//                       generated every run      one-way exit 'x' (54,19) onto the E corridor
//   Dragon Den                                  x 23-37, y 18-32 (gates above)
//*****************************************************************************
