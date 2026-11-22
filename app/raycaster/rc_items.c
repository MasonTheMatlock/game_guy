// rc_items.c - the item catalog (rc.md 7.5): staffs, consumables, gear, relics, Mimic Cloth.
// Numbers are placeholders for the balance patch. Descriptions: up to 3 lines of 12 chars.
#include "rc_priv.h"

// Held-art palettes (rc_kWpnPal index). Art indices (rc_kWpnArt): 0 ray wand, 1 prism,
// 2 storm rod, 3 sunfire, 4 fire flask, 5 frost flask, 6 venom flask, 7 elixir,
// 8 haste brew, 9 tome, 10 warp rune, 11 aegis.
enum { WP_RAY, WP_PRISM, WP_STORM, WP_SUN, WP_FIRE, WP_FROST, WP_VENOM, WP_ELIXIR,
       WP_HASTE, WP_TOME, WP_WARP, WP_AEGIS,
       WP_INFERNAL, WP_THUNDER, WP_GRAVE, WP_PENGINE, WP_ELDRITCH, WP_GILDED, WP_MANA };

// Each row starts with the item's name and description. The macros DROP those two
// strings: tools/textpack.py packs them 2 chars per word into rc_text.c (read them
// with rc_ItemName(i) / rc_ItemDesc(i)). Re-run textpack.py after editing any text.
//   kind, slot, class, effect, art, palette, value, price
#define STAFF(n, d, cls, art, pal, price)       { K_STAFF, SL_WPN, cls, 0, art, pal, 0, price }
#define CONS(n, d, art, pal, cd, price)         { K_CONS, 0, 0, 0, art, pal, (cd) / 10, price }
#define GEAR(n, d, slot, cls, fx, v, price)     { K_GEAR, slot, cls, fx, 0, 0, v, price }
#define RELIC(n, d)                             { K_RELIC, 0, 0, 0, 0, 0, 0, 150 }
#define CLOTH(n, d)                             { K_CLOTH, 0, 0, 0, 0, 0, 0, 0 }

const ItemDef rc_kItems[I_COUNT] =
{
    // ---- staffs (class staffs are granted on first activation, never sold: price 0)
    STAFF("Ray Wand",        "weak starter|staff",            CL_NONE,    0, WP_RAY,        0),
    STAFF("Prism Staff",     "3 beams in|a fan",              CL_NONE,    1, WP_PRISM,     60),
    STAFF("Storm Rod",       "very fast|weak shots",          CL_NONE,    2, WP_STORM,     80),
    STAFF("Sunfire",         "5 flame|beams, each|pierce one", CL_NONE,   3, WP_SUN,      120),
    STAFF("Infernal Scptr",  "hold to|charge a|fire beam",    CL_PYRO,    3, WP_INFERNAL,   0),
    STAFF("Thunderbolt",     "chain|lightning",               CL_STORM,   2, WP_THUNDER,    0),
    STAFF("Gravekeeper",     "spectral|shot, pierces|one",    CL_NECRO,   0, WP_GRAVE,      0),
    STAFF("Prism Engine",    "arcane ray|pierces all",        CL_RAY,     1, WP_PENGINE,    0),
    STAFF("Eldritch Focus",  "hold: beam|grows while|channeled", CL_WARLOCK, 0, WP_ELDRITCH, 0),
    STAFF("Gilded Scepter",  "ricochet|coins, 1 gold|each",   CL_MIDAS,   0, WP_GILDED,     0),

    // ---- consumables (no mana; each has its own cooldown)
    CONS("Fire Flask",   "fireball|ignites foes",   4, WP_FIRE,    60, 25),
    CONS("Frost Flask",  "freezes foes|(bosses half)", 5, WP_FROST, 70, 30),
    CONS("Venom Flask",  "poison cloud",            6, WP_VENOM,  110, 35),
    CONS("Elixir",       "restore all|health",      7, WP_ELIXIR, 300, 40),
    CONS("Haste Brew",   "10s faster|attacks",      8, WP_HASTE,  600, 35),
    CONS("Aegis Charm",  "blocks the|next hit",    11, WP_AEGIS,  300, 45),
    CONS("Arcane Tome",  "3 homing|bolts",          9, WP_TOME,    50, 20),
    CONS("Warp Rune",    "blink 3|tiles ahead",    10, WP_WARP,   120, 25),
    CONS("Mana Potion",  "+25 mana",                7, WP_MANA,    60, 15),

    // ---- general gear (rc.md 7.5.2)
    GEAR("Sturdy Chest",   "+10% resist",       SL_BODY, CL_NONE, FX_RES,       10, 40),
    GEAR("Traveler Cape",  "+10% move|speed",   SL_BODY, CL_NONE, FX_MOVE,      10, 35),
    GEAR("Mage Robes",     "+30 max|mana",      SL_BODY, CL_NONE, FX_MANA,      30, 35),
    GEAR("Berserker Vest", "+15% damage",       SL_BODY, CL_NONE, FX_DMG,       15, 60),
    GEAR("Lucky Helmet",   "+20% gold",         SL_HELM, CL_NONE, FX_GOLD,      20, 35),
    GEAR("Hunter's Hood",  "+8% crit|chance",   SL_HELM, CL_NONE, FX_CRIT,       8, 45),
    GEAR("Scholar Circlt", "+30% mana|regen",   SL_HELM, CL_NONE, FX_REGEN,     30, 30),
    GEAR("Swift Cap",      "+12% attack|speed", SL_HELM, CL_NONE, FX_ASPD,      12, 50),
    GEAR("Ring of Sparks", "+30% ult|charge",   SL_ACC,  CL_NONE, FX_ULT,       30, 40),
    GEAR("Ring of Vigor",  "+1 max|heart",      SL_ACC,  CL_NONE, FX_HP,         2, 60),
    GEAR("Amulet of Flow", "-15% mana|costs",   SL_ACC,  CL_NONE, FX_COST,      15, 45),
    GEAR("Collector Coin", "+50% chest|and boss gold", SL_ACC, CL_NONE, FX_CHESTGOLD, 50, 30),

    // ---- class gear (rc.md 7.5.1). Order per class: body A, body B, helmet A, helmet B, acc A, acc B
    GEAR("Emberweave",     "+25% fire|damage",        SL_BODY, CL_PYRO,  FX_FIRE,     25, 120),
    GEAR("Magma Vest",     "+10% resist",             SL_BODY, CL_PYRO,  FX_RES,      10,  90),
    GEAR("Cinder Crown",   "+50% burn|duration",      SL_HELM, CL_PYRO,  FX_BURNDUR,  50, 110),
    GEAR("Smolder Hood",   "+1 burn dmg|per stack",   SL_HELM, CL_PYRO,  FX_BURNDMG,   1, 110),
    GEAR("Ashen Signet",   "20% hits|ignite",         SL_ACC,  CL_PYRO,  FX_IGNITE,   20, 130),
    GEAR("Flare Charm",    "+30% ult|charge",         SL_ACC,  CL_PYRO,  FX_ULT,      30,  90),

    GEAR("Stormglass",     "+25% light-|ning damage", SL_BODY, CL_STORM, FX_LIGHT,    25, 120),
    GEAR("Thunderhide",    "+12% move|speed",         SL_BODY, CL_STORM, FX_MOVE,     12,  90),
    GEAR("Conductor Crcl", "+20% light-|ning speed",  SL_HELM, CL_STORM, FX_LSPD,     20, 120),
    GEAR("Static Cowl",    "chains reach|+1 foe",     SL_HELM, CL_STORM, FX_CHAIN,     1, 130),
    GEAR("Tempest Loop",   "15% hits|stun",           SL_ACC,  CL_STORM, FX_STUN,     15, 130),
    GEAR("Spark Band",     "+30% ult|charge",         SL_ACC,  CL_STORM, FX_ULT,      30,  90),

    GEAR("Gravebound",     "+50% summon|damage",      SL_BODY, CL_NECRO, FX_SUMDMG,   50, 120),
    GEAR("Shroud of Bones","+50% summon|health",      SL_BODY, CL_NECRO, FX_SUMHP,    50, 100),
    GEAR("Lich King Crwn", "+50% summon|duration",    SL_HELM, CL_NECRO, FX_SUMDUR,   50, 110),
    GEAR("Skull Visage",   "+50% Covenant|healing",   SL_HELM, CL_NECRO, FX_COVEN,    50, 110),
    GEAR("Phylactery Pnd", "+1 summon",               SL_ACC,  CL_NECRO, FX_SUMCNT,    1, 140),
    GEAR("Soul Locket",    "+10% raise|chance",       SL_ACC,  CL_NECRO, FX_REZ,      10, 120),

    GEAR("Prismweave",     "+25% beam|damage",        SL_BODY, CL_RAY,   FX_BEAM,     25, 120),
    GEAR("Lenswoven Clk",  "+30 max|mana",            SL_BODY, CL_RAY,   FX_MANA,     30,  90),
    GEAR("Astral Lens",    "+15% beam|crit",          SL_HELM, CL_RAY,   FX_BCRIT,    15, 120),
    GEAR("Focus Circlet",  "+30% mana|regen",         SL_HELM, CL_RAY,   FX_REGEN,    30,  90),
    GEAR("Singularity Pd", "-25% beam|cost",          SL_ACC,  CL_RAY,   FX_BCOST,    25, 120),
    GEAR("Refraction Rng", "+50% Fracture|beams",     SL_ACC,  CL_RAY,   FX_FRACT,    50, 130),

    GEAR("Pactforged",     "+25% beam|damage",        SL_BODY, CL_WARLOCK, FX_WBEAM,  25, 120),
    GEAR("Bloodwoven",     "+1 max|heart",            SL_BODY, CL_WARLOCK, FX_HP,      2, 110),
    GEAR("Hollow Crown",   "+15 mana per|conversion", SL_HELM, CL_WARLOCK, FX_CONV,   15, 110),
    GEAR("Hollow Visor",   "conversion|costs half|a heart less", SL_HELM, CL_WARLOCK, FX_CONVCOST, 1, 120),
    GEAR("Sanguine Amlt",  "+50% health|from damage", SL_ACC,  CL_WARLOCK, FX_LSTEAL, 50, 130),
    GEAR("Eldritch Eye",   "+50% Singu-|larity size", SL_ACC,  CL_WARLOCK, FX_SINGRAD, 50, 110),

    GEAR("Gilded Aegis",   "+5% dmg per|100 gold",    SL_BODY, CL_MIDAS, FX_GILDDMG,   5, 140),
    GEAR("Crown Fortune",  "+25% gold",               SL_HELM, CL_MIDAS, FX_GOLD,     25, 120),
    GEAR("King's Ransom",  "15% hits|make gold",      SL_ACC,  CL_MIDAS, FX_GOLDPROC, 15, 130),

    // ---- relics (rc.md 14): passive while carried, duplicates do not stack
    RELIC("All-Seeing Eye", "20% chance to|reflect a|projectile"),
    RELIC("Slime Crown",    "kills may|raise a|friendly slime"),
    RELIC("Phylactery",     "once: cheat|death with|one heart"),
    RELIC("Primal Core",    "hits may|burn, freeze|or shock"),
    RELIC("Blood Heart",    "low health:|more damage"),

    // ---- the Mimic Cloth (rc.md 12.4): counts as a class item for every class
    CLOTH("Mimic Cloth", "bound for|good"),
};

// Class names, in CL_* order (packed by tools/textpack.py; read with rc_ClassName(c)).
#define CLASS_NAME(n)
CLASS_NAME("Apprentice") CLASS_NAME("Pyromancer") CLASS_NAME("Stormcaller") CLASS_NAME("Necromancer")
CLASS_NAME("Raycaster")  CLASS_NAME("Warlock")    CLASS_NAME("Midas")


//*****************************************************************************
// Staff behaviour, one row per staff (same order as the item ids).
//   dmg / cooldown frames / beams / spread / pierce / chain jumps / element / mode / mana
//   SM_CHARGE: hold fire to charge (up to 4x), release to shoot.
//   SM_CHANNEL: hold fire: one tick every `cooldown` frames, damage grows while held.
//   EL_GOLD: costs 1 gold per attack (mana when out of gold).
//*****************************************************************************
const StaffDef rc_kStaffs[NUM_STAFFS] =
{
    { 3, 16, 1,  0,  0, 0, EL_ARCANE,   SM_NORMAL,  2, COLOR_TURQUOISE, &sfx_staff_ray   },   // Ray Wand
    { 3, 22, 3, 16,  0, 0, EL_ARCANE,   SM_NORMAL,  6, COLOR_VIOLET,    &sfx_staff_prism },   // Prism Staff
    { 2,  8, 1,  0,  0, 0, EL_LIGHT,    SM_NORMAL,  1, COLOR_PLUM,      &sfx_staff_storm },   // Storm Rod
    { 4, 34, 5, 14,  1, 0, EL_FIRE,     SM_NORMAL, 10, COLOR_CORAL,     &sfx_staff_sun   },   // Sunfire
    { 4, 28, 1,  0,  1, 0, EL_FIRE,     SM_CHARGE,  8, COLOR_ORANGE,    &sfx_staff_sun   },   // Infernal Scepter
    { 4, 18, 1,  0,  0, 2, EL_LIGHT,    SM_NORMAL,  5, COLOR_GOLD,      &sfx_staff_storm },   // Thunderbolt
    { 5, 20, 1,  0,  1, 0, EL_SPECTRAL, SM_NORMAL,  4, COLOR_MINT,      &sfx_staff_lance },   // Gravekeeper
    { 4, 20, 1,  0, 99, 0, EL_ARCANE,   SM_NORMAL,  6, COLOR_VIOLET,    &sfx_staff_prism },   // Prism Engine
    { 1,  6, 1,  0,  0, 0, EL_ELDRITCH, SM_CHANNEL, 1, COLOR_GREEN,     &sfx_staff_ray   },   // Eldritch Focus
    { 4, 18, 1,  0,  0, 2, EL_GOLD,     SM_NORMAL,  2, COLOR_GOLD,      &sfx_coin        },   // Gilded Scepter
};
