// rc_priv.h - private header shared by the modules of the raycaster app.
#ifndef RC_PRIV_H
#define RC_PRIV_H
//#############################################################################
// FILE:   app/raycaster/rc_priv.h
// TITLE:  RAY CASTER - first-person survival roguelike (F28069 + SSD2119)
//
// HOW IT IS BUILT FOR THIS HARDWARE (C28x @ 90 MHz, SPI LCD, no frame buffer)
//
//  * The LCD is the bottleneck, not the CPU. The 3D view is RC_COLS vertical
//    strips; each strip is DIFFED against what is already on the screen and
//    only the runs whose colour changed are sent.
//  * Everything is integer / table driven (the ray DDA uses float for exact
//    corner hits). int is 16 bits on C28x: every product that can exceed
//    32767 is cast to int32_t first. There is no uint8_t on this compiler.
//  * World positions are Q10 (1024 = 1 tile) in int32_t (the 64-tile-wide map
//    does not fit a 16-bit Q10 coordinate). Differences between two points
//    are clamped to int16_t with RD() (about 32 tiles), which is far more than
//    any interaction range. Ray distances are Q8.
//
// GAME (see app/rc.md for the full design)
//  * A 64 x 48 dungeon: a loop of six chambers and two corridors around the
//    sealed Dragon Den. Eight doors are bought with gold; four keys open four
//    secret rooms (Slime Vault, Haunted Shrine, Astral Laboratory, and the
//    Goblin Maze, which is generated fresh every run).
//  * Endless waves with a fixed schedule: one new enemy type per wave, a
//    mini-boss every 2nd wave, a treasure wave every 3rd. All eight doors plus
//    three mini-bosses open the Den; after the first Dragon the Dragon returns
//    every 5th wave with a growing escort of mini-bosses.
//  * Health is in half hearts. Level-ups offer one stat upgrade and two runes.
//
// APP CONTRACT: update() = logic only, render() = drawing only, one public
// symbol (g_raycasterApp). The runner already calls Joy_Update()/Dpad_Update().
//#############################################################################

#include "raycaster.h"
#include "rc_sound.h"
#include "rc_text.h"
#include "../../drivers/memory.h"

#include <math.h>


//*****************************************************************************
// Render / world size
//*****************************************************************************
#undef RC_COLS
#undef RC_COL_W
#define RC_COLS  60                     // 60 strips x 5 pixels = 300 pixels
#define RC_COL_W 5

#undef RC_MAP_W
#undef RC_MAP_H
#define RC_MAP_W        64
#define RC_MAP_H        48
#define RC_MAP_SHIFT    6               // log2(RC_MAP_W)
#define RC_MAP_CELLS    (RC_MAP_W * RC_MAP_H)

#undef RC_MAX_SPAWNS
#define RC_MAX_SPAWNS   20              // chamber spawn markers ('a'..'f') + Den ('g')
#undef RC_MAX_OBJ
#define RC_MAX_OBJ      20              // projectiles, clouds, sparks, hazards, pickups
#define RC_MAX_PROPS    20              // chests, vendors, keys, altars (static things)

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

extern const Song song_corridor_run;

//*****************************************************************************
// Constants
//*****************************************************************************
#define RC_ANGLES       1024
#define RC_ANGLE_MASK   1023

#define RC_VIEW_W   (RC_COLS * RC_COL_W)
#define RC_VIEW_X0 (((SCREEN_W - RC_VIEW_W) / 2) + RC_VIEW_X_OFFSET)
#define RC_VIEW_Y1      (RC_VIEW_Y0 + RC_VIEW_H - 1)
#define RC_HORIZON      (RC_VIEW_H / 2)
#define RC_PROJ         ((uint16_t)(RC_VIEW_H * 256UL))
#define RC_MIN_DIST     40
#define RC_MAX_STEPS    56              // DDA cell steps per ray (long corridors)
#define RC_SEE_DIST     12288           // Q10: enemies farther than 12 tiles path-find without a sight check

#define RC_FOOTSTEPS    1
#define RC_STEP_DIST    560
#define RC_PX_PER_UNIT  ((RC_COLS * RC_COL_W) / RC_FOV_UNITS)

// Game modes
#define MODE_INTRO      0               // wave banner, world frozen
#define MODE_PLAY       1
#define MODE_CARDS      2               // card screen: level-up, vendor, blessing, rune altar
#define MODE_CLEAR      3
#define MODE_OVER       4               // game over: stats
#define MODE_TITLE      5
#define MODE_HOWTO      6
#define MODE_LETTER     7
#define MODE_NAME       8               // high-score name entry
#define MODE_SCORES     9               // high-score table, "Main Menu"
#define MODE_INV        10              // inventory / vendor / full-bag swap list (game paused)

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
#define HUD_ULT         0x80            // ultimate meter column
#define HUD_ALL         0xFF
#define HUD_TOPR        (HUD_GOLD | HUD_LEVEL | HUD_XP)

#define RC_WALL_DECOR   1
#define RC_WALL_THEMES  1
#define RC_WALL_ART     1
#define RC_ART_MAX_SHADE 1
#if RC_ART_MAX_SHADE > 1
#error RC_ART_MAX_SHADE must be 0 or 1
#endif

//*****************************************************************************
// Health (half hearts), mana, damage sizes
//*****************************************************************************
#define HP_UNITS_PER_HEART  2
#define HP_START            (RC_START_HEARTS * HP_UNITS_PER_HEART)   // 6 units = 3 hearts
#define HP_CAP              24          // 12 hearts
#define HP_HUD_ICONS        6
#define DMG_SMALL           1           // ordinary hit: half a heart
#define DMG_BIG             2           // big hit: one heart
#define DMG_BOSS            2           // boss hit
#define DMG_BOSS_HEAVY      4           // boss heavy hit: two hearts
#define HEART_DROP_PCT      8           // ordinary enemy drops a heart pickup (never at full health)

#define MANA_BASE           100
#define MANA_REGEN_FRAMES   10          // standing still: +1 mana every N frames (before upgrades)
#define MANA_BOTTLE         25
#define MANA_DROP_PCT       20
#define MANA_PICKUP_R       380
#define MANA_BOTTLE_LIFE    450
#define C_MANA              COLOR_BLUE

#define RC_CRIT_BASE_PCT    5           // every attack can crit for 1.5x

//*****************************************************************************
// Doors, keys, waves
//*****************************************************************************
#define RC_NUM_DOORS        8           // 1 S-SW, 2 SW-W, 3 W-NW, 4 NW-N, 5 N-NE, 6 NE-E, 7 E-SE, 8 SE-S
#define DOOR_COST_FLAT      10          // rc.md 4.2: flat 10 gold for now (rc_DoorCost)
#define DOOR_USE_DIST       2600
#define INTERACT_DIST       1500        // chests, vendors, altars, secret doors
#define KEY_CARRY_MAX       3
#define RC_NUM_KEYS         4

#define ROUND_GAP_FRAMES    150         // 5 s break between waves
#define SPAWN_GAP_FRAMES    45          // fixed spawn interval (does not shrink)
#define SPAWN_MIN_DIST      2600
#define RC_ALIVE_CAP        10          // hard cap of regular enemies alive
#define RC_DRAGON_MINIS     3           // mini-bosses to beat before the Den opens
#define RC_MINI_STAGGER     900         // endless Dragon wave: frames between escort mini-bosses (30 s)
#define RC_TREASURE_EVERY   3
#define RC_MINI_EVERY       2
#define RC_DRAGON_EVERY     5

#define RC_FRAME_TICKS  (((uint32_t)TICKS_PER_MS * 1000UL) / RC_TARGET_FPS)
#define RC_TARGET_FPS   30UL
#define RC_MAX_CATCHUP  3

//*****************************************************************************
// Rooms. Chambers form a loop: S, SW, W corridor, NW, N, NE, E corridor, SE.
//*****************************************************************************
enum { RM_S, RM_SW, RM_WC, RM_NW, RM_N, RM_NE, RM_EC, RM_SE,
       RM_DEN, RM_VAULT, RM_SHRINE, RM_LAB, RM_MAZE, RM_COUNT, RM_NONE = 0xFF };
#define RM_IS_CHAMBER(r) ((r) == RM_S || (r) == RM_SW || (r) == RM_NW || (r) == RM_N || \
                          (r) == RM_NE || (r) == RM_SE)
typedef struct { uint16_t x0, y0, x1, y1, theme; } RoomDef;          // name: rc_RoomName(r)
extern const RoomDef rc_kRoom[RM_COUNT];
uint16_t rc_RoomAt(int32_t x, int32_t y);
extern uint16_t rc_playerRoom;

// Goblin Maze block (15 x 15 tiles, generated every run)
#define MAZE_X0     39
#define MAZE_Y0     18
#define MAZE_N      15

// The Dragon Den (inside the gates)
#define RC_DEN_X0   23
#define RC_DEN_X1   37
#define RC_DEN_Y0   18
#define RC_DEN_Y1   32
enum { DEN_SEALED, DEN_OPEN, DEN_FIGHT };
extern uint16_t rc_denState;

//*****************************************************************************
// Enemies
//*****************************************************************************
enum { ET_GOBLIN, ET_BAT, ET_HOBGOB, ET_SKELETON, ET_WATCHER, ET_LICH, ET_DRAGON,
       ET_SLIME, ET_SLIME_S, ET_SPIDER, ET_GHOST, ET_BOMBER, ET_ARCHER, ET_MIMIC,
       ET_SLIMEKING, ET_GOLEM, ET_VAMPIRE, ET_EYEKING, ET_COUNT };

#define RC_NUM_MINI     5
#define RC_IS_MINI(t)   ((t) == ET_LICH || (t) == ET_EYEKING || (t) == ET_SLIMEKING || \
                         (t) == ET_GOLEM || (t) == ET_VAMPIRE)
#define RC_IS_BOSS(t)   (RC_IS_MINI(t) || (t) == ET_DRAGON)
extern const uint16_t rc_kMini[RC_NUM_MINI];

enum { SH_GOBLIN, SH_BAT, SH_SKELETON, SH_LICH, SH_BEHOLDER,
       SH_FLASK, SH_BOLT, SH_PUFF, SH_DRAGON, SH_DRAGON_HEAD,   // the Dragon's body and head
       SH_HOBGOB, SH_SLIME, SH_SPIDER, SH_GHOST, SH_GHOST_FADE, SH_BOMBER,
       SH_EYEKING_A, SH_EYEKING_B, SH_SLIMEKING_A, SH_SLIMEKING_B,
       SH_GOLEM_A, SH_GOLEM_B, SH_VAMP_A, SH_VAMP_B, SH_LICH_B,
       SH_CHEST, SH_MIMIC, SH_ORB, SH_WEB, SH_PUDDLE, SH_WARN, SH_WAVE,
       SH_ALERT, SH_MERCHANT, SH_ARCHER, SH_KEY, SH_HEART,
       SH_GOLEM_C, SH_GOLEM_D, SH_SPIKE,           // Golem dash / cast poses, ice spikes
       SH_BARREL, SH_BONES, SH_CRYSTAL, SH_CANDLE,  // floor decorations
       SH_DRAGON_ROAR, SH_WING_UP, SH_WING_DOWN,   // Dragon: roaring head, left wing frames (mirrored for the right)
       SH_ICON_STAFF, SH_ICON_BODY, SH_ICON_HELM, SH_ICON_RING, SH_ICON_RUNE,   // inventory icons
       SH_SKEL_BACK,                               // a summoned skeleton seen from behind (allies face away)
       SH_COUNT };
#define SH_FLIP   0x8000u                       // OR into a shape: draw it mirrored left-right

enum { PT_GOBLIN, PT_BAT, PT_HOBGOB, PT_SKELETON, PT_BEHOLDER, PT_LICH,
       PT_BEHOLDER_HOT,
       PT_FLASK_FIRE, PT_FLASK_FROST, PT_FLASK_VENOM, PT_FLASK_MANA,
       PT_BOLT,
       PT_PUFF_FIRE, PT_PUFF_FROST, PT_PUFF_VENOM, PT_PUFF_ARCANE,
       PT_DRAGON, PT_DRAGON_HOT,
       PT_SLIME, PT_SLIME_S, PT_SPIDER, PT_GHOST, PT_GHOST_FADE,
       PT_BOMBER, PT_BOMBER_LIT,
       PT_EYEKING, PT_EYEKING_FIRE, PT_EYEKING_ICE, PT_EYEKING_BOLT,
       PT_EYEKING_GRAV, PT_EYEKING_NULL,
       PT_SLIMEKING, PT_GOLEM_FIRE, PT_GOLEM_ICE, PT_GOLEM_STORM, PT_VAMP,
       PT_CHEST, PT_CHEST_CURSED, PT_MIMIC,
       PT_ORB_PURPLE, PT_ORB_GREEN, PT_ORB_BLOOD, PT_ORB_FIRE, PT_ORB_ICE, PT_ORB_BOLT,
       PT_WEB, PT_PUDDLE_SLIME, PT_PUDDLE_FIRE, PT_PUDDLE_WEB, PT_PUDDLE_BLAST,
       PT_WARN, PT_WAVE_SLIME, PT_WAVE_FIRE, PT_WAVE_ICE, PT_WAVE_STORM, PT_WAVE_NOVA,
       PT_ALERT, PT_MERCHANT, PT_ARCHER, PT_KEY, PT_HEART, PT_ALTAR, PT_CHEST_GOOD,
       PT_GOLEM_EARTH, PT_ROCK, PT_SPIKE,
       PT_MARK_EARTH, PT_MARK_BLUE, PT_MARK_WHITE, PT_MARK_GREEN,   // floor marks (telegraphs)
       PT_BARREL, PT_BARREL_G, PT_BONES, PT_CRYSTAL, PT_CANDLE,     // floor decorations
       PT_DRAGON_CORE,                                              // Dragon with its weak spot exposed
       PT_ALLY,                                                     // the player's summons: spectral violet
       PT_COUNT };
#define DR_EXPOSED  3u                          // Dragon state: landed, weak spot open (2x damage)

typedef struct
{
    int16_t  hp;
    int16_t  speed;         // Q10 units per frame
    int16_t  gold;
    int16_t  xp;
    int16_t  radius;        // beam hit-test half width (Q10)
    int16_t  reach;         // contact distance that hurts the player (Q10)
    uint16_t scale;         // sprite height, Q4 fraction of wall height (16 = 1.0)
    uint16_t zoff;          // sprite lift, Q4 fraction of wall height
    uint16_t shape;
    uint16_t pal;
    uint16_t shapeB;        // walk frame B (0 = none)
    uint16_t touch;         // contact damage in half hearts
} EnemyDef;
extern const EnemyDef rc_kEnemy[ET_COUNT];

typedef struct
{
    int32_t  x, y;          // Q10
    int16_t  hp;
    int16_t  mhp;
    uint16_t type;
    uint16_t flash;
    uint16_t slow;
    uint16_t freeze;
    uint16_t burn;
    uint16_t aux;
    uint16_t aux2;
    uint16_t st;
    uint16_t tm;
    uint16_t an;
    uint16_t cnt;
    uint16_t sum;           // SUM_*: hostile, hostile summon (dies with its master), or friendly ally
    uint16_t burnStk;       // burn stacks (0..5): stacks x damage every second
    uint16_t life;          // allies: frames left before they crumble
    int16_t  tx, ty;        // charge velocity / bat swoop angle
    uint16_t alive;
} Enemy;

enum { SUM_NONE, SUM_MINION, SUM_ALLY };       // Enemy.sum: hostile / a boss's summon / on the player's side
enum { AK_SUMMON, AK_RISEN, AK_ARMY, AK_SLIME }; // an ally's kind (kept in Enemy.st)

//*****************************************************************************
// Waves: unlock order, themes (rc.md 3.5, 13.2)
//*****************************************************************************
#define NUM_ORDINARY    9
extern const uint16_t rc_kUnlock[NUM_ORDINARY];        // enemy type unlocked at wave i + 1
typedef struct { const char *name; uint16_t n; uint16_t t[3]; } WaveTheme;
#define NUM_THEMES      8
extern const WaveTheme rc_kThemes[NUM_THEMES];
extern const WaveTheme rc_kOpening[10];                 // waves 1..10 (treasure waves: n = 0)

//*****************************************************************************
// Progression effects. EVERY bonus in the game (stat levels, runes, equipped
// gear, carried relics, blessings) is summed into one table rc_fx[] by
// rc_RecalcFx(); game code only ever reads rc_fx[FX_x]. The first nine entries
// line up with the nine direct stats (rc.md 6.2).
//*****************************************************************************
enum
{
    FX_DMG, FX_HP, FX_MOVE, FX_ASPD, FX_MANA, FX_REGEN, FX_RES, FX_CRIT, FX_GOLD,   // = ST_* order
    FX_ULT,         // % faster ultimate charge
    FX_COST,        // % cheaper mana costs
    FX_CHESTGOLD,   // % more gold from chests and bosses
    FX_FIRE,        // % fire damage                    (Pyromancer gear)
    FX_BURNDUR,     // % longer burns
    FX_BURNDMG,     // + burn damage per tick per stack
    FX_IGNITE,      // % chance any hit ignites
    FX_LIGHT,       // % lightning damage               (Stormcaller gear)
    FX_LSPD,        // % faster lightning staffs
    FX_STUN,        // % chance a lightning hit stuns
    FX_CHAIN,       // + chain targets
    FX_SUMDMG,      // % summon damage                  (Necromancer gear)
    FX_SUMHP,       // % summon health
    FX_SUMDUR,      // % summon duration
    FX_SUMCNT,      // + summons at once
    FX_COVEN,       // % more Vampiric Covenant healing
    FX_REZ,         // + % resurrection chance
    FX_BEAM,        // % arcane beam damage             (Raycaster gear)
    FX_BCRIT,       // + % arcane beam crit chance
    FX_BCOST,       // % cheaper arcane beams
    FX_FRACT,       // % stronger Reality Fracture beams
    FX_WBEAM,       // % eldritch beam damage           (Warlock gear)
    FX_CONV,        // + mana per Blood Pact conversion
    FX_CONVCOST,    // Blood Pact costs this many half hearts less
    FX_LSTEAL,      // % more health from damage dealt
    FX_SINGRAD,     // % bigger Singularity
    FX_GILDDMG,     // % damage per 100 gold carried    (Midas gear)
    FX_GOLDPROC,    // % chance a hit makes bonus gold
    FX_COUNT
};
extern int16_t  rc_fx[FX_COUNT];
void rc_RecalcFx(void);                       // after ANY change to stats, runes, gear, relics

enum { ST_DMG, ST_HP, ST_MOVE, ST_ASPD, ST_MANA, ST_REGEN, ST_RES, ST_CRIT, ST_GOLD, ST_COUNT };
#define STAT_MAX_LV     8
typedef struct { uint16_t step; } StatDef;          // text: rc_StatName / rc_StatDesc
extern const StatDef  rc_kStat[ST_COUNT];
extern const uint16_t rc_kSoft[STAT_MAX_LV + 1];       // cumulative effect per level, Q8 of one step

enum { RU_HASTE, RU_FORCE, RU_SWIFT, RU_ECHOES, RU_VITAL, RU_LEECH, RU_CONSERVE,
       RU_FORTUNE, RU_MOMENTUM, RU_HERMIT,
       // class runes: offered once any class has been active; work only while it is
       RU_CINDERWAKE, RU_ASHEN, RU_ARC, RU_STATIC, RU_SIPHON, RU_LEGION,
       RU_PIERCE, RU_AFTERIMAGE, RU_TWIN, RU_RICOCHET, RU_VOID, RU_REACH,
       RU_GARSENAL, RU_GTOUCH, RU_COUNT };
#define RU_GENERAL      10
#define RUNE_SLOTS      4
#define RUNE_NONE       0xFFFFu
#define RUNE_PRICE      70
typedef struct { uint16_t cls; } RuneDef;           // cls 0 = general; text: rc_RuneName / rc_RuneDesc
extern const RuneDef rc_kRune[RU_COUNT];
bool rc_HasRune(uint16_t r);                  // equipped AND (general, or its class is active)

//*****************************************************************************
// Classes (rc.md 8)
//*****************************************************************************
enum { CL_NONE, CL_PYRO, CL_STORM, CL_NECRO, CL_RAY, CL_WARLOCK, CL_MIDAS, CL_COUNT };
extern uint16_t rc_class;                     // active class (CL_NONE = Apprentice)
extern uint16_t rc_classSeen;                 // bit c: class c has been active this run
extern uint16_t rc_ult;                       // ultimate meter, 0..ULT_FULL
extern uint16_t rc_ultT;                      // frames of the running ultimate
#define ULT_FULL        240                   // damage dealt to fill the meter

//*****************************************************************************
// Items (rc.md 7). One catalog for staffs, consumables, gear, relics and the
// Mimic Cloth. Gear gives ONE effect: rc_fx[fx] += val.
//*****************************************************************************
enum
{
    // staffs (the Weapon slot)
    I_RAYWAND, I_PRISM, I_STORMROD, I_SUNFIRE,
    I_INFERNAL, I_THUNDER, I_GRAVE, I_PENGINE, I_ELDRITCH, I_GILDED,
    // consumables (stack to 3, at most 3 types carried)
    I_FIRE, I_FROST, I_VENOM, I_ELIXIR, I_HASTE, I_AEGIS, I_TOME, I_WARP, I_MANAPOT,
    // general gear: body, helmet, accessory
    I_CHESTPLATE, I_CAPE, I_ROBES, I_BERSERK,
    I_LUCKYHELM, I_HUNTERHOOD, I_SCHOLAR, I_SWIFTCAP,
    I_SPARKS, I_VIGOR, I_FLOW, I_COIN,
    // class gear: per class body A, body B, helmet A, helmet B, accessory A, accessory B
    I_CLASS0,
    I_MIDAS0 = I_CLASS0 + 30,                // Midas: body, helmet, accessory only
    // mini-boss relics (passive while carried)
    I_EYE = I_MIDAS0 + 3, I_SLIMECROWN, I_PHYLACTERY, I_PRIMAL, I_BLOODHEART,
    I_CLOTH,                                  // Mimic Cloth (only ever bound, never carried)
    I_COUNT,
    I_NONE = 0xFF
};
#define I_FIRST_CONS    I_FIRE
#define I_FIRST_GEAR    I_CHESTPLATE
#define I_FIRST_RELIC   I_EYE
#define NUM_STAFFS      (I_FIRST_CONS)
#define NUM_CONS        (I_FIRST_GEAR - I_FIRST_CONS)

enum { K_STAFF, K_CONS, K_GEAR, K_RELIC, K_CLOTH };
enum { SL_BODY, SL_HELM, SL_ACC, SL_WPN, SL_COUNT };
typedef struct
{
    // name / description: rc_ItemName(i) / rc_ItemDesc(i) (up to 3 lines of 12 chars, '|' separated)
    uint16_t kind : 3;          // K_*
    uint16_t slot : 2;          // gear: SL_BODY / SL_HELM / SL_ACC
    uint16_t cls  : 3;          // class gear / class staff: CL_*
    uint16_t fx   : 8;          // gear effect: FX_*
    uint16_t art  : 4;          // held art (staffs, consumables): rc_kWpnArt index
    uint16_t pal  : 5;          //                                 rc_kWpnPal index
    uint16_t val  : 7;          // gear: effect amount; consumable: cooldown / 10 frames
    uint16_t price;             // shop price (sells for 10%, rounded up)
} ItemDef;
extern const ItemDef rc_kItems[I_COUNT];

// Staff behaviour (rc.md 7.5.3): all ten are one beam routine driven by this table.
enum { EL_ARCANE, EL_FIRE, EL_LIGHT, EL_SPECTRAL, EL_ELDRITCH, EL_GOLD };
enum { SM_NORMAL, SM_CHARGE, SM_CHANNEL };
typedef struct
{
    uint16_t dmg, cooldown, beams, spread, pierce, chain, elem, mode, mana;
    uint32_t color;
    const Song *sfx;
} StaffDef;
extern const StaffDef rc_kStaffs[NUM_STAFFS];

enum { PK_FIRE, PK_FROST, PK_VENOM, PK_ARCANE };
extern const int16_t  rc_kPotRadius[3];
extern const uint16_t rc_kPotDmg[3];
extern const uint16_t rc_kPotLife[3];
extern const uint16_t rc_kPotScale[3];

//*****************************************************************************
// Inventory (rc.md 7.1): 12 slots, 4 equipment slots, Mimic Cloth bindings.
//*****************************************************************************
#define INV_SLOTS       12
#define CONS_STACK      3
#define CONS_TYPES      3
typedef struct { uint16_t item, cnt; } InvSlot;
extern InvSlot  rc_inv[INV_SLOTS];
extern uint16_t rc_eq[SL_COUNT];              // equipped item per slot (I_NONE = empty)
extern uint16_t rc_clothMask;                 // bit s: Mimic Cloth bound to slot s (forever)
extern uint16_t rc_sel;                       // the Up cycle: 0xFFFF = the weapon, else an inventory slot (consumable)
extern uint16_t rc_cdStaff;                   // weapon cooldown, frames
extern uint16_t rc_cdStaffMax;
extern uint16_t rc_cdCons[NUM_CONS];          // per-consumable cooldowns
extern uint16_t rc_relicMask;                 // bit r: relic r carried (rc_RecalcFx)
extern uint16_t rc_relicCnt[5];               // relics carried (Relics tab), per mini-boss relic
extern uint16_t rc_phylacteryUsed;
extern uint16_t rc_manaDebt;                  // max mana lost to cursed-chest trades
extern uint16_t rc_hpBonus;                   // extra half hearts from the Shrine

uint16_t rc_GiveItem(uint16_t item);          // GIVE_* result
enum { GIVE_OK, GIVE_EQUIPPED, GIVE_FULL, GIVE_NOROOM };
bool     rc_Owns(uint16_t item);              // in the bag or equipped
uint16_t rc_SellValue(uint16_t item);
void     rc_CycleSelection(void);
uint16_t rc_SelItem(void);                    // the item the Up cycle points at
void     rc_UseSelected(void);                // Down: fire the staff / use the consumable
void     rc_UpdateClass(void);                // after any gear change
void     rc_EnterInventory(uint16_t screen);  // INV_BAG / INV_SHOP / INV_SWAP
void     rc_UpdateInventory(void);
void     rc_DrawInventoryFull(void);
void     rc_InventoryRender(void);
void     rc_QueueLoot(uint16_t item);         // gear won from a chest: equip / bag / swap screen
bool     rc_LootPending(void);
enum { INV_BAG, INV_SHOP, INV_SWAP };

//*****************************************************************************
// Map (64 x 48)
//   # stone  P pillar  B brick  . floor  @ start
//   1..8 purchasable doors     D Dragon Den gate     s secret door (needs a key)
//   x one-way maze exit (wall from outside)          K key
//   c chamber chest nook       v corridor nook (vendor / chest / decoration)
//   V Slime Vault chest        A Shrine altar        L Lab rune altar
//   G maze good chest (placed by the generator)      M maze block (generated)
//   a..f spawn markers (chambers S, SW, NW, N, NE, SE)   g Den marker
//*****************************************************************************
extern const uint16_t rc_kMapPk[RC_MAP_H * RC_MAP_W / 2];     // rc_map.c (generated, 2 chars per word)

// Map character at tile (x, y). Even x is the high byte of its word.
static __inline char rc_MapCh(uint16_t x, uint16_t y)
{
    uint16_t w = rc_kMapPk[(y << (RC_MAP_SHIFT - 1)) + (x >> 1)];
    return (char)((x & 1u) ? (w & 0xFFu) : (w >> 8));
}

//*****************************************************************************
// Sprite art
//*****************************************************************************
#define SPR_N       16
extern const uint16_t rc_kArt[SH_COUNT][SPR_N * 4];
// A sprite palette: the colours of letters a..h (COLOR_MAGENTA = unused letter).
extern const uint32_t *const rc_kPal[PT_COUNT];
extern const uint32_t rc_kTint[5];          // 0 none, 1 hit, 2 burning, 3 frozen, 4 gold statue

// Wall tiles: 0 floor, 1 stone, 2 pillar, 3 brick, 4 door, 5 portal, 6 portal pulse,
// 7 metal (Den gate, Golem seal)
#define TILE_WALL_STONE 1
#define TILE_DOOR       4
#define TILE_PORTAL     5
#define TILE_METAL_DOOR 7
#define TILE_EARTH      6                   // the Earth Golem's temporary seal wall
#define TILE_KINDS      8
#define TILE_BLOCK      8                   // a decoration that blocks movement: solid, but rays pass through it
extern const uint32_t rc_kWallBase[TILE_KINDS];
extern const uint16_t rc_kShadeMul[4];

//*****************************************************************************
// First-person art (held in the LEFT hand). See rc_art.c.
//*****************************************************************************
#define WPN_W        20
#define WPN_H        18
#define WPN_SCALE    RC_COL_W
#define WPN_TIP_ROW  1
#define WPN_MAXRUNS  12
#define WPN_Y0       (RC_VIEW_H - WPN_H * WPN_SCALE)
#define WPN_XOFF     20
#define WPN_ART_ROWS 10
// Held art is packed 4 bits per cell (rc_art.c, tools/wpnpack.py): 5 words per 20-cell row.
// Cell codes: 0 '.' transparent, 1 k outline, 2 s glove, 3 q glove shade, 4 c sleeve,
// 5 d sleeve shade, 6 w body, 7 x body shade, 8 g gem, 9 h highlight, 10 f flash, 11 y / 12 r accents.
#define WPN_SYMS  13
#define WPN_ARTS  12
#define WPN_PALS  19
extern const uint16_t rc_kWpnArm[40];
extern const uint16_t *const rc_kWpnArt[WPN_ARTS];
// A held-art palette: one colour per cell code (the glove is always khaki / grey).
#define WPAL(K,C,D,W,X,G,H,F,Y,R) { 0, K, COLOR_KHAKI, COLOR_GRAY, C, D, W, X, G, H, F, Y, R }
extern const uint32_t *const rc_kWpnPal[WPN_PALS];

//*****************************************************************************
// Tables (built once, in RAM)
//*****************************************************************************
#define PAL_N 8
extern int16_t  rc_qsin[257];
extern uint16_t rc_qinv[257];
extern float    rc_qinvF[257];
extern int16_t  rc_colOff[RC_COLS];
extern int16_t  rc_colCos[RC_COLS];
extern int16_t  rc_atan[257];
extern uint32_t rc_wallCol[TILE_KINDS][4][2];
#define SPR_COLORS  120
extern uint32_t rc_sprRgb[SPR_COLORS];
extern uint16_t rc_palId[PT_COUNT][PAL_N];
extern bool     rc_tablesReady;

static __inline int16_t SinLut(uint16_t a)
{
    uint16_t i = a & 255u;
    int16_t  v;

    if(a & 256u) i = (uint16_t)(256u - i);
    v = rc_qsin[i];
    return (a & 512u) ? (int16_t)-v : v;
}

static __inline float QInvF(uint16_t b)
{
    uint16_t i = b & 255u;

    if(b & 256u) i = (uint16_t)(256u - i);
    return rc_qinvF[i];
}

#define SIN(a)  SinLut((uint16_t)(a))
#define COS(a)  SinLut((uint16_t)((uint16_t)(a) + 256u))

//*****************************************************************************
// Small helpers
//*****************************************************************************
static __inline int16_t Abs16(int16_t v) { return (v < 0) ? -v : v; }

// Difference of two Q10 positions, clamped to int16_t (about +-32 tiles).
static __inline int16_t RD(int32_t d)
{
    return (d > 32767L) ? 32767 : (d < -32767L) ? -32767 : (int16_t)d;
}

// Cheap distance: max + 0.375 * min (error under 4 percent). Q10 in, Q10 out.
static __inline int16_t Dist10(int16_t dx, int16_t dy)
{
    int16_t a = Abs16(dx);
    int16_t b = Abs16(dy);

    if(a < b) { int16_t t = a; a = b; b = t; }
    {
        int32_t d = (int32_t)a + (b >> 2) + (b >> 3);
        return (d > 32767L) ? 32767 : (int16_t)d;
    }
}

// Distance between two world points.
static __inline int16_t DistP(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    return Dist10(RD(x1 - x0), RD(y1 - y0));
}

#define TILE_C(t)   ((int32_t)(t) * 1024L + 512L)       // centre of tile t, Q10
#define RAND(n)     ((uint16_t)(Rand16() % (uint16_t)(n)))

//*****************************************************************************
// Player and run state
//*****************************************************************************
extern int32_t  rc_px;
extern int32_t  rc_py;
extern int16_t  rc_ang;
extern uint16_t rc_hp;                  // half hearts
extern uint16_t rc_maxHp;               // half hearts
extern uint16_t rc_gold;
extern uint16_t rc_invuln;
extern uint16_t rc_kills;
extern uint16_t rc_lvStat[ST_COUNT];
extern uint16_t rc_rune[RUNE_SLOTS];
extern uint16_t rc_classSeen;           // a class has been active this run (unlocks class runes)
extern uint16_t rc_haste;               // Haste Brew frames
extern uint16_t rc_aegis;               // Aegis Charm: 1 = the next hit is blocked
extern uint16_t rc_silence;             // Beholder spell-disable: no staff / consumables
extern uint16_t rc_confuse;             // Beholder confusion: movement reversed
extern uint16_t rc_momentum;            // Momentum rune: speed boost frames
extern uint16_t rc_leechCnt;
extern uint16_t rc_xp;
extern uint16_t rc_xpLv;
extern uint16_t rc_lvPending;
extern uint16_t rc_mana;
extern uint16_t rc_manaAcc;
extern uint16_t rc_manaFlash;
extern int16_t  rc_manaShown;
extern bool     rc_manaFlashShown;
extern uint16_t rc_keys;
extern uint16_t rc_keyTaken;            // bit k: key k picked up
uint16_t rc_ManaMax(void);
uint16_t rc_MaxHpCap(void);

// Waves
typedef struct
{
    uint16_t total, maxAlive, gap;
    uint16_t mini;              // 1 = a mini-boss joins this wave
    uint16_t treasure;          // 1 = treasure chest this wave
    uint16_t dragon;            // 1 = endless Dragon wave
    uint16_t theme;             // index into rc_kThemes (or 0xFF = opening table)
} LevelDef;
extern uint16_t rc_wave;                // 1-based wave number
extern LevelDef rc_lv;
extern uint16_t rc_lastTheme;
extern uint16_t rc_roundGap;
extern uint16_t rc_alive;
extern uint16_t rc_toSpawn;
extern uint16_t rc_spawnTimer;
extern uint16_t rc_miniPending;
extern uint16_t rc_miniAlive;
extern uint16_t rc_miniKills;
extern uint16_t rc_miniBag;             // bit k: rc_kMini[k] already used in the first five
extern uint16_t rc_lastMini;
extern uint16_t rc_dragonKills;
extern uint16_t rc_dragonAlive;
extern uint16_t rc_escortLeft;          // endless Dragon wave: mini-bosses still to come
extern uint16_t rc_escortTimer;
extern uint16_t rc_treasureOpen;        // treasure chest of this wave not yet opened
extern uint16_t rc_seed;

// Map
extern uint16_t rc_tile[RC_MAP_CELLS];
extern int32_t  rc_spX[RC_MAX_SPAWNS];
extern int32_t  rc_spY[RC_MAX_SPAWNS];
extern uint16_t rc_spRoom[RC_MAX_SPAWNS];
extern uint16_t rc_spCount;
extern bool     rc_doorBought[RC_NUM_DOORS + 1];
extern uint16_t rc_doorsBought;
extern int32_t  rc_doorCx[RC_NUM_DOORS + 1];
extern int32_t  rc_doorCy[RC_NUM_DOORS + 1];
extern uint16_t rc_flow[RC_MAP_CELLS];
extern uint16_t rc_flowQ[RC_MAP_CELLS];
extern int16_t  rc_flowTx;
extern int16_t  rc_flowTy;
extern uint16_t rc_portalPhase;
extern Enemy    rc_en[RC_MAX_ENEMIES];

uint16_t rc_DoorCost(void);

// Objects: projectiles, clouds, sparks, hazards, pickups
typedef struct
{
    int32_t  x, y;          // Q10
    int16_t  vx, vy;
    int16_t  z;
    int16_t  r;
    uint16_t kind, pal, sc, life, age, aux, alive;
} Obj;

enum { OB_FLASK, OB_BOLT, OB_CLOUD, OB_SPARK, OB_MANA,
       OB_WARN, OB_AREA, OB_WAVE, OB_HEART,
       OB_SING };                               // Eldritch Singularity: pulls and hurts enemies
enum { BK_LASER, BK_FLAME, BK_ORB, BK_DRAIN, BK_WEB, BK_ICE, BK_FIRE, BK_SPARK, BK_ARROW,
       BK_ROCK,                                 // Earth Golem rock
       BK_DEATH,                                // Lich Lord green death bolt
       BK_GLOB,                                 // Slime King glob: leaves an acid puddle where it lands
       BK_FIREBALL,                             // Dragon / Fire Golem: leaves burning ground where it lands
       BK_COUNT };
enum { AR_SLIME, AR_FIRE, AR_BLAST, AR_WEB, AR_ARCANE,
       AR_PFIRE,                                // the player's burning ground: hurts ENEMIES only
       AR_NOVA };                               // Lich Lord death blast (a warning ring, then one explosion)
enum { WV_SLIME, WV_FIRE, WV_ICE, WV_STORM, WV_NOVA };
extern Obj      rc_obj[RC_MAX_OBJ];

// Props: static things in the world (one per map marker, plus the treasure-wave chest)
enum { PR_CHEST, PR_VENDOR, PR_KEY, PR_ALTAR_SHRINE, PR_ALTAR_LAB, PR_DECOR };
enum { CH_REGULAR, CH_MIMIC, CH_CURSED, CH_GOOD, CH_BOSS, CH_DRAGON };
extern uint16_t rc_chestKind;               // the chest being opened (CH_*)
extern uint16_t rc_chestGold;               // gold offered by its gold card
typedef struct
{
    int32_t  x, y;
    uint16_t kind;          // PR_*
    uint16_t sub;           // chest: CH_*, key: key index
    uint16_t used;          // opened / taken
    uint16_t room;
    uint16_t alive;
    uint16_t arg;           // boss chest: its relic
} Prop;
// Floor decorations (map 'o' blocks movement, '%' is clutter you walk over), themed per room.
#define RC_MAX_DECOR    32
typedef struct { uint16_t tx, ty, shape, pal, scale; } Decor;
extern Decor    rc_decor[RC_MAX_DECOR];
extern uint16_t rc_nDecor;
extern Prop     rc_prop[RC_MAX_PROPS];
extern uint16_t rc_nProp;
extern int16_t  rc_treasureProp;        // prop index of this wave's treasure chest (-1 none)

// Interaction (stick button)
enum { IA_NONE, IA_DOOR, IA_SECRET, IA_EXIT, IA_CHEST, IA_VENDOR, IA_SHRINE, IA_LAB, IA_DEN };
extern uint16_t rc_iaKind;
extern uint16_t rc_iaIdx;               // door number / prop index / tile cell
extern uint16_t rc_iaShown;
extern uint16_t rc_nearSecret;             // standing next to a closed secret door

// Flow / screens
extern uint16_t rc_mode;
extern uint16_t rc_timer;
extern int16_t  rc_stX;
extern int16_t  rc_stY;
extern uint16_t rc_fxTimer;
extern uint16_t rc_fxN;
extern int16_t  rc_fxX[5];
extern uint32_t rc_fxColor;
extern uint16_t rc_wRun[WPN_W][WPN_MAXRUNS];
extern uint16_t rc_wRunN[WPN_W];
extern uint32_t rc_wPal[26];
extern uint16_t rc_wLoaded;
extern uint16_t rc_stepAcc;
extern uint16_t rc_stepBlock;
extern uint16_t rc_stepAlt;
extern uint16_t rc_sfxHold;
extern int16_t  rc_cdShown;
#define WPN_X0   (RC_VIEW_X0 + WPN_XOFF)
#define WPN_C0   (((WPN_X0) - RC_VIEW_X0) / RC_COL_W)
#define WPN_C1   (((WPN_X0 + WPN_W * WPN_SCALE - 1) - RC_VIEW_X0) / RC_COL_W)
#define WPN_TIP_X (WPN_X0 + 10 * WPN_SCALE)
#if (WPN_XOFF % RC_COL_W) != 0
#error WPN_XOFF must be a multiple of RC_COL_W so the hand lines up with the strips
#endif
extern bool     rc_dirtyFull;
extern bool     rc_invalidate;
extern bool     rc_bannerDraw;
extern uint16_t rc_hud;
extern uint16_t rc_banner;
extern uint16_t rc_viewDirty;
extern int16_t  rc_rngT[RC_COLS];
extern int16_t  rc_rngB[RC_COLS];
extern int16_t  rc_prvT[RC_COLS];
extern int16_t  rc_prvB[RC_COLS];

#define RC_MAXR      16
#define RC_MAXN      40
#define RC_STRIPS    RC_COLS
extern uint16_t rc_sl[RC_STRIPS][RC_MAXR];
extern uint16_t rc_sn[RC_STRIPS];
extern uint16_t g_rcOverflow;
extern uint16_t g_rcTrunc;

//*****************************************************************************
// Card screen (level-up, vendor, shrine blessing, lab rune altar, rune swap)
//*****************************************************************************
enum { CS_LEVEL, CS_SHRINE, CS_LAB, CS_SWAP, CS_CHEST, CS_CURSED, CS_BIND };
enum { CK_NONE, CK_STAT, CK_RUNE, CK_ITEM, CK_GOLD, CK_SLOT,
       CK_SHRINE_GOLD, CK_SHRINE_HEART,                 // Haunted Shrine trades
       CK_CLOTH, CK_POWER, CK_REJECT,                   // cursed chest choices
       CK_BIND };                                       // Mimic Cloth: which slot (idx = SL_*)
typedef struct { uint16_t kind, idx, sold; } Card;
#define CARDS_MAX   4
extern Card     rc_card[CARDS_MAX];
extern uint16_t rc_nCard;
extern uint16_t rc_cardScreen;          // CS_*
extern uint16_t rc_cardSel;
extern uint16_t rc_cardDirty;
extern uint16_t rc_cardMsg;
extern uint16_t rc_rerolls;             // rerolls used this level-up (first one is free)
extern uint16_t rc_swapRune;            // rune waiting for a free slot (CS_SWAP)
#define CD_REROLL   0x10
#define CD_GOLD     0x20
#define CD_MSG      0x40
#define CD_ALL      0x7F
extern const char *const rc_kMsgText[7];

// Sprite draw list
typedef struct
{
    int16_t  x0;
    int16_t  top;
    uint16_t size;
    uint16_t c0, c1;
    uint16_t dist;
    uint16_t uStep;
    uint16_t shape, type, flash;
} Spr;
// Floor marks (rc_boss.c): tiles painted on the floor to telegraph boss attacks.
#define RC_MAX_MARKS  48
#define MK_STEADY     1u                        // lit without flashing (lava, spikes)
#define MK_SPIKE      2u                        // drawn as ice spikes
#define MK_WALL       4u                        // marks a doorway about to be sealed
typedef struct { uint16_t tx, ty, pal, life, flags; } Mark;
extern Mark     rc_mark[RC_MAX_MARKS];
extern uint16_t rc_nMark;
void rc_MarkTile(int16_t tx, int16_t ty, uint16_t pal, uint16_t life, uint16_t flags);
void rc_MarkLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t pal, uint16_t life);
void rc_MarksClear(void);
bool rc_ThinkBoss(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist);
bool rc_BossLook(const Enemy *e, uint16_t *shape, uint16_t *pal);
bool rc_Untargetable(const Enemy *e);           // ghost phasing, Slime King split
void rc_DragonPose(const Enemy *e, uint16_t *head, int16_t *headZ, uint16_t *pal, uint16_t *wing, int16_t *lift);
void rc_BossTick(void);
void rc_ResetBosses(void);

// Movement / attack helpers shared by the enemy and boss AI (rc_enemy.c)
void     rc_Slide(Enemy *e, int16_t mx, int16_t my);
void     rc_MoveAlong(Enemy *e, int16_t vx, int16_t vy, int16_t sp);
void     rc_Chase(Enemy *e, int16_t sp, int16_t side, int16_t dx, int16_t dy, int16_t dist);
void     rc_KeepRange(Enemy *e, int16_t sp, int16_t neer, int16_t faar, int16_t sgn, int16_t dx, int16_t dy, int16_t dist);
int16_t  rc_SgnOf(const Enemy *e);
uint16_t rc_SummonCount(uint16_t type);
void     rc_SummonAround(const Enemy *m, uint16_t type, uint16_t n);
uint16_t rc_Phase(const Enemy *e);
void     rc_AimCharge(Enemy *e, int16_t sp, int16_t dx, int16_t dy);
bool     rc_ChargeStep(Enemy *e);
void     rc_BehindPlayer(int32_t *bx, int32_t *by);

#define RC_SPR_CAP  (RC_MAX_ENEMIES + RC_MAX_OBJ + RC_MAX_PROPS + RC_MAX_MARKS + RC_MAX_DECOR)
extern Spr      rc_spr[RC_SPR_CAP];
extern uint16_t rc_nSpr;
extern int16_t  rc_projC;
extern int16_t  rc_projS;

// High scores (rc.md 5.5)
extern MemRcScores rc_scores;
extern uint16_t    rc_scoreRank;        // rank being entered (0..4), 0xFFFF = none
extern uint16_t    rc_nameChar[RC_NAME_LEN];
extern uint16_t    rc_namePos;

//*****************************************************************************
// Functions
//*****************************************************************************
int16_t rc_StickEdge(int16_t v, int16_t *state);
int16_t rc_Itoa(char *p, uint16_t v);
int16_t rc_Cat(char *dst, int16_t n, const char *s, int16_t maxLen);
void rc_DrawTextEx(const char *s, int16_t x, int16_t y, uint32_t fg, uint32_t bg, bool small);
void rc_DrawText(const char *s, int16_t x, int16_t y, uint32_t color);
void rc_DrawSmall(const char *s, int16_t x, int16_t y, uint32_t fg, uint32_t bg);
void rc_Sfx(const Song *s);
void rc_SfxSoft(const Song *s);
void rc_BuildTables(void);

// title screen etc. (rc_cards.c)
void rc_EnterTitle(void);
bool rc_UpdateTitle(uint16_t ticks);
void rc_DrawTitleFull(void);
void rc_TitleRender(void);

// Map + collision
static __inline bool Solid(int16_t tx, int16_t ty)
{
    uint16_t t;

    if((uint16_t)tx >= RC_MAP_W || (uint16_t)ty >= RC_MAP_H) return true;
    t = rc_tile[((uint16_t)ty << RC_MAP_SHIFT) + (uint16_t)tx];
    return (t != 0) && (t != TILE_PORTAL);
}

static __inline bool Blocked(int32_t x, int32_t y, int16_t r)
{
    int16_t x0 = (int16_t)((x - r) >> 10);
    int16_t x1 = (int16_t)((x + r) >> 10);
    int16_t y0 = (int16_t)((y - r) >> 10);
    int16_t y1 = (int16_t)((y + r) >> 10);

    return Solid(x0, y0) || Solid(x1, y0) || Solid(x0, y1) || Solid(x1, y1);
}
void rc_LoadMap(void);
void rc_BuildMaze(void);
uint16_t rc_RC_CastRay(int16_t angle, uint16_t *tile, uint16_t *side);
extern int16_t rc_rayCx;
extern int16_t rc_rayCy;
void rc_SetTiles(char ch, uint16_t tile);        // every map cell with this character -> tile

// Game: flow, player, enemies
#define RC_BOLT_SPEED   80
uint16_t rc_XpNeed(void);
uint16_t rc_StaffMana(void);            // mana per attack of the equipped staff (after discounts)
void rc_HurtPlayer(uint16_t units);
void rc_HealPlayer(uint16_t units);
void rc_GainGold(uint16_t base);
void rc_GainXp(uint16_t amt);
void rc_DamageEnemyEx(Enemy *e, uint16_t dmg, int16_t c, int16_t s, bool quiet);
void rc_UpdateObjects(void);
void rc_TrySpawn(void);
void rc_RefreshTheme(void);
uint16_t rc_BeholderCadence(void);
void rc_BeholderShoot(const Enemy *e);
void rc_DragonBreath(const Enemy *e);

Obj     *rc_NewObj(uint16_t kind);
Enemy   *rc_SpawnEnemy(uint16_t type, int32_t x, int32_t y);
void     rc_AddSpark(int32_t x, int32_t y, int16_t z, uint16_t sc, uint16_t pal, uint16_t life);
uint16_t rc_CountRegular(void);
uint16_t rc_CountType(uint16_t type);
uint16_t rc_CountBosses(void);
void     rc_Explode(int32_t x, int32_t y);
void     rc_HealEnemy(Enemy *e, int16_t amt);
void     rc_MakeWarn(int32_t x, int32_t y, int16_t r, uint16_t life, uint16_t area, uint16_t areaLife);
void     rc_MakeWave(int32_t x, int32_t y, int16_t speed, int16_t maxR, uint16_t flavour);
void     rc_ShootBolt(const Enemy *e, int16_t angleOff, uint16_t kind, int16_t speed, uint16_t life);
void     rc_EnemyLook(const Enemy *e, uint16_t *shape, uint16_t *pal);
bool     rc_CanSee(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
bool     rc_TeleportEnemy(Enemy *e, int32_t cx, int32_t cy, int16_t rMin, int16_t rMax);
void     rc_PlayerPush(int16_t dx, int16_t dy);
bool     rc_PickMarker(bool den, int32_t *x, int32_t *y);
Enemy   *rc_SpawnMini(void);
void     rc_DropHeart(int32_t x, int32_t y);
void     rc_OpenChest(uint16_t p);
int16_t  rc_AddProp(uint16_t kind, uint16_t sub, int32_t x, int32_t y);

// One-line message in the hint bar (loot, keys, warnings) for a couple of seconds.
extern char     rc_toast[32];
extern uint16_t rc_toastT;
void rc_Toast(const char *a, const char *b);

// Combat (rc_combat.c)
#define HF_QUIET    1u                          // PlayerHit flags: no hit sound
#define HF_DOT      2u                          //                  damage over time: no on-hit effects
void     rc_PlayerHit(Enemy *e, uint16_t base, uint16_t elem, uint16_t flags);
void     rc_IgniteEnemy(Enemy *e, uint16_t stacks);
Enemy   *rc_SpawnAlly(uint16_t type, int32_t x, int32_t y, uint16_t life, uint16_t kind);
uint16_t rc_CountAllies(uint16_t kind);         // 0xFFFF = all kinds
void     rc_Shatter(Obj *o);
void     rc_FireInput(bool held);
void     rc_TryUltimate(void);
void     rc_ClassTick(void);
bool     rc_GoldStatues(void);
void     rc_ResetCombat(void);
uint16_t rc_RandomChestItem(bool classOnly);
void     rc_OnBossDeath(const Enemy *e, int32_t x, int32_t y);  // rc_boss.c
void     rc_GiveBossPackage(uint16_t relic, uint16_t general, uint16_t clsItem);
void     rc_SpendSelected(void);
void     rc_ResetInventory(void);
void     rc_OfferRune(uint16_t rune);           // equip a rune, or ask which one to replace

extern uint16_t rc_pSlow;
extern uint16_t rc_nullify;
extern uint16_t rc_pull;
extern int32_t  rc_pullX, rc_pullY;

// Wall themes: chambers, corridors, and one per boss
enum { TH_BASEMENT, TH_SLIME, TH_GOLEM, TH_VAMPIRE, TH_EYE, TH_LICH, TH_DRAGON,
       TH_WARRENS, TH_CRYPT, TH_SEWERS, TH_CHAPEL, TH_FORGE, TH_OBSERV, TH_COUNT };
enum { PAT_NONE, PAT_DRIPS, PAT_CRACKS, PAT_VEINS, PAT_EYES, PAT_BANDS, PAT_LAVA };
typedef struct { uint32_t base, mortar, accent, floor, ceil; uint16_t pat; } Theme;
extern const Theme rc_kTheme[TH_COUNT];
extern uint32_t    rc_themeCol[2][4][2];
extern const uint32_t rc_kBannerRgb[TH_COUNT];
extern uint32_t    rc_bannerCol[4];
extern uint32_t    rc_colFloor, rc_colCeil;
extern uint16_t    rc_theme;
void rc_ApplyTheme(uint16_t t);
uint16_t rc_ThemeOfMini(uint16_t type);

extern const int16_t rc_kNbDx[4];
extern const int16_t rc_kNbDy[4];
void rc_EnsureFlow(void);
void rc_UpdateEnemies(void);
void rc_DrawHudFrame(void);

#define XP_BAR_X0   114
#define XP_BAR_X1   317
#define XP_BAR_Y0   16
#define XP_BAR_Y1   21
void rc_DrawHud(void);
#define CD_STRIP_X0   20
#define CD_STRIP_W    150
void rc_DrawCdStrip(void);
#define MANA_X0   1
#define MANA_X1   8
#define ULT_X0    311                        // ultimate meter: the free column right of the view
#define ULT_X1    318
void rc_DrawManaBar(void);

// Strip run lists
#define ID_CEIL     0u
#define ID_FLOOR    1u
#define ID_SPR      64u
#define ID_TINT     192u
#define ID_WPN      200u
#define ID_THEME0   226u
#define ID_UNKNOWN  255u
#define ID_TEX_WOOD0 58u
#define ID_TEX_MET0  61u
#define ID_BANNER0   184u
#define ID_DOOR_FRAME 196u
#define ID_EMBER     242u
#define ID_FLAME     243u
#define ID_WALL(tile, shade, side)  (2u + (tile) * 8u + (shade) * 2u + (side))
#define RUN_MAKE(row, id)  ((uint16_t)(((uint16_t)(row) << 8) | (uint16_t)(id)))
void rc_RenderView(void);
void rc_DrawBanner(void);
void rc_DrawEndScreen(void);

// Card screens
#define CARD_W      100
#define CARD_H      138
#define CARD_X0     5
#define CARD_GAP    5
#define CARD_Y0     38
#define RR_X0       5
#define RR_X1       165
#define RR_Y0       186
#define RR_Y1       212
void rc_DrawCardsFull(void);
void rc_CardsRender(void);
void rc_EnterCards(uint16_t screen);
void rc_UpdateCards(void);
void rc_ResetRun(void);
void rc_StartLevel(void);
void rc_EnterOver(bool mimic);              // mimic = the third Mimic Cloth ending
bool rc_UpdateOverScreens(void);           // true = leave to the main menu
void rc_OverRender(void);
void rc_UpdatePlay(void);
void rc_LoadScores(void);

extern const App g_raycasterApp;

//*****************************************************************************
// Packed table text (rc_text.c, generated by tools/textpack.py from the tables in
// rc_items.c / rc_data.c). rc_Text unpacks string #id into one of 4 rotating RAM
// buffers, so up to 4 results can be in use at once; copy anything kept longer.
//*****************************************************************************
extern const uint16_t rc_kTextPk[];
const char *rc_Text(uint16_t id);
#define rc_ItemName(i)   rc_Text((uint16_t)(TX_ITEM + 2u * (i)))
#define rc_ItemDesc(i)   rc_Text((uint16_t)(TX_ITEM + 2u * (i) + 1u))
#define rc_StatName(i)   rc_Text((uint16_t)(TX_STAT + 2u * (i)))
#define rc_StatDesc(i)   rc_Text((uint16_t)(TX_STAT + 2u * (i) + 1u))
#define rc_RuneName(i)   rc_Text((uint16_t)(TX_RUNE + 2u * (i)))
#define rc_RuneDesc(i)   rc_Text((uint16_t)(TX_RUNE + 2u * (i) + 1u))
#define rc_RoomName(i)   rc_Text((uint16_t)(TX_ROOM + (i)))
#define rc_EnemyName(i)  rc_Text((uint16_t)(TX_ENEMY + (i)))
#define rc_ClassName(i)  rc_Text((uint16_t)(TX_CLASS + (i)))

// The title-screen picture (rc_splash_image.c, generated by tools/splashpack.py).
#define RC_SPLASH_W     128
#define RC_SPLASH_H     72
extern const uint32_t rc_kSplashPal[16];
extern const uint16_t rc_kSplash[RC_SPLASH_W * RC_SPLASH_H / 4];

#endif // RC_PRIV_H
