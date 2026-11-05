// rc_art.h  --  ALL sprite art for the Raycaster: 16 x 16 frames and their palettes.
// Included by RayCaster.c. Edit freely: '.' = transparent, letters a..h = palette entries.
// Every row must be exactly 16 characters. Frames are listed in the SH_ enum; an enemy's
// animation is a run of consecutive frames (see kAnim in RayCaster.c).
// (Authored with tools/genart.py; the header itself is the source of truth now.)
#ifndef RC_ART_H
#define RC_ART_H

#include "Apps.h"

#ifndef SPR_N
#define SPR_N 16
#endif
enum {
    SH_GOBLIN0,
    SH_GOBLIN1,
    SH_GOBLIN2,
    SH_BAT0,
    SH_BAT1,
    SH_BAT2,
    SH_SKEL0,
    SH_SKEL1,
    SH_SKEL2,
    SH_LICH0,
    SH_LICH1,
    SH_LICH2,
    SH_BEHOLDER0,
    SH_BEHOLDER1,
    SH_BEHOLDER2,
    SH_DRAGON0,
    SH_DRAGON1,
    SH_DRAGON2,
    SH_ARCHER0,
    SH_ARCHER1,
    SH_ARCHER2,
    SH_SPIDER0,
    SH_SPIDER1,
    SH_SPIDER2,
    SH_IMP0,
    SH_IMP1,
    SH_IMP2,
    SH_BOMBER0,
    SH_BOMBER1,
    SH_BOMBER2,
    SH_SLIME0,
    SH_SLIME1,
    SH_SLIME2,
    SH_GOLEM0,
    SH_GOLEM1,
    SH_GOLEM2,
    SH_MERCH0,
    SH_MERCH1,
    SH_CHEST0,
    SH_CHEST1,
    SH_MINO0,
    SH_MINO1,
    SH_MINO2,
    SH_MINO3,
    SH_ARROW,
    SH_ORB0,
    SH_ORB1,
    SH_BOMB0,
    SH_BOMB1,
    SH_RING0,
    SH_RING1,
    SH_METEOR0,
    SH_METEOR1,
    SH_FLASK,
    SH_BOLT0,
    SH_BOLT1,
    SH_PUFF,
    SH_COUNT
};

enum {
    PT_GOBLIN,
    PT_BAT,
    PT_HOBGOB,
    PT_SKELETON,
    PT_BEHOLDER,
    PT_LICH,
    PT_BEHOLDER_HOT,
    PT_FLASK_FIRE,
    PT_FLASK_FROST,
    PT_FLASK_VENOM,
    PT_FLASK_MANA,
    PT_BOLT,
    PT_PUFF_FIRE,
    PT_PUFF_FROST,
    PT_PUFF_VENOM,
    PT_PUFF_ARCANE,
    PT_DRAGON,
    PT_DRAGON_HOT,
    PT_DRAGON_SLEEP,
    PT_ARCHER,
    PT_SPIDER,
    PT_IMP,
    PT_BOMBER,
    PT_MINO,
    PT_MINO_HOT,
    PT_ALLY_IMP,
    PT_ALLY_KNIGHT,
    PT_ALLY_WISP,
    PT_ARROW,
    PT_ORB,
    PT_BOMB,
    PT_BOMB_HOT,
    PT_RING,
    PT_METEOR,
    PT_SOUL,
    PT_SLIME,
    PT_SLIMEKING,
    PT_GOLEM,
    PT_GOLEM_ICE,
    PT_GOLEM_ZAP,
    PT_GOLEM_HOT,
    PT_VAMPIRE,
    PT_VAMPIRE_HOT,
    PT_MERCH,
    PT_ICE,
    PT_HOLE,
    PT_CHEST,
    PT_BOLT_ARROW_FIRE,
    PT_TOXIC,
    PT_COUNT
};


typedef struct { char ch; uint32_t rgb; } PalEnt;      // list ends with ch = 0

static const PalEnt kPal_GOBLIN[] =
    { {'d',COLOR_OLIVE},{'b',COLOR_DARK_GRAY},{'c',COLOR_GOLD},{'h',COLOR_WHITE},{'f',COLOR_TETRIS_GRID},{'e',COLOR_DARK_GRAY},{'a',COLOR_TETRIS_GRID},{'g',COLOR_LIGHT_GRAY},{0,0} };

static const PalEnt kPal_BAT[] =
    { {'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_DARK_PURPLE},{'d',COLOR_WHITE},{0,0} };

static const PalEnt kPal_HOBGOB[] =
    { {'d',COLOR_BROWN},{'b',COLOR_BROWN},{'c',COLOR_CRIMSON},{'h',COLOR_WHITE},{'f',COLOR_TETRIS_GRID},{'e',COLOR_TETRIS_GRID},{'a',COLOR_TETRIS_GRID},{'g',COLOR_KHAKI},{0,0} };

static const PalEnt kPal_SKELETON[] =
    { {'d',COLOR_LIGHT_GRAY},{'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_SILVER},{0,0} };

static const PalEnt kPal_BEHOLDER[] =
    { {'c',COLOR_PURPLE},{'f',COLOR_DARK_PURPLE},{'g',COLOR_WHITE},{'d',COLOR_CRIMSON},{'a',COLOR_BLACK},{'h',COLOR_GOLD},{'e',COLOR_WHITE},{'b',COLOR_TETRIS_GRID},{0,0} };

static const PalEnt kPal_LICH[] =
    { {'a',COLOR_GOLD},{'b',COLOR_LIGHT_GRAY},{'c',COLOR_CYAN},{'d',COLOR_DARK_PURPLE},{'e',COLOR_DARK_PURPLE},{'f',COLOR_BLACK},{0,0} };

static const PalEnt kPal_BEHOLDER_HOT[] =
    { {'c',COLOR_GRAY},{'f',COLOR_DARK_PURPLE},{'g',COLOR_PINK},{'d',COLOR_KHAKI},{'a',COLOR_CRIMSON},{'h',COLOR_CRIMSON},{'e',COLOR_WHITE},{'b',COLOR_TETRIS_GRID},{0,0} };

static const PalEnt kPal_FLASK_FIRE[] =
    { {'a',COLOR_TETRIS_L_ORANGE},{'c',COLOR_LIGHT_GRAY},{'d',COLOR_OLIVE},{'b',COLOR_TETRIS_GRID},{0,0} };

static const PalEnt kPal_FLASK_FROST[] =
    { {'a',COLOR_SKY_BLUE},{'c',COLOR_LIGHT_GRAY},{'d',COLOR_OLIVE},{'b',COLOR_TETRIS_GRID},{0,0} };

static const PalEnt kPal_FLASK_VENOM[] =
    { {'a',COLOR_GREEN},{'c',COLOR_LIGHT_GRAY},{'d',COLOR_OLIVE},{'b',COLOR_TETRIS_GRID},{0,0} };

static const PalEnt kPal_FLASK_MANA[] =
    { {'a',COLOR_BLUE},{'c',COLOR_WHITE},{'d',COLOR_GOLD},{'b',COLOR_NAVY},{0,0} };

static const PalEnt kPal_BOLT[] =
    { {'a',COLOR_CRIMSON},{'b',COLOR_WHITE},{0,0} };

static const PalEnt kPal_PUFF_FIRE[] =
    { {'a',COLOR_BROWN},{'b',COLOR_TETRIS_L_ORANGE},{'c',COLOR_KHAKI},{0,0} };

static const PalEnt kPal_PUFF_FROST[] =
    { {'a',COLOR_TEAL},{'b',COLOR_SKY_BLUE},{'c',COLOR_WHITE},{0,0} };

static const PalEnt kPal_PUFF_VENOM[] =
    { {'a',COLOR_DARK_GRAY},{'b',COLOR_OLIVE},{'c',COLOR_MINT},{0,0} };

static const PalEnt kPal_PUFF_ARCANE[] =
    { {'a',COLOR_GRAY},{'b',COLOR_VIOLET},{'c',COLOR_WHITE},{0,0} };

static const PalEnt kPal_DRAGON[] =
    { {'a',COLOR_DARK_PURPLE},{'b',COLOR_PURPLE},{'c',COLOR_CRIMSON},{'d',COLOR_GOLD},{'e',COLOR_TETRIS_L_ORANGE},{0,0} };

static const PalEnt kPal_DRAGON_HOT[] =
    { {'a',COLOR_DARK_PURPLE},{'b',COLOR_CRIMSON},{'c',COLOR_TETRIS_L_ORANGE},{'d',COLOR_WHITE},{'e',COLOR_GOLD},{0,0} };

static const PalEnt kPal_DRAGON_SLEEP[] =
    { {'a',COLOR_TETRIS_GRID},{'b',COLOR_DARK_GRAY},{'c',COLOR_GRAY},{'d',COLOR_DARK_GRAY},{'e',COLOR_GRAY},{0,0} };

static const PalEnt kPal_ARCHER[] =
    { {'d',COLOR_LIGHT_GRAY},{'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_BROWN},{'h',COLOR_TEAL},{'e',COLOR_WHITE},{0,0} };

static const PalEnt kPal_SPIDER[] =
    { {'a',COLOR_TETRIS_GRID},{'b',COLOR_DARK_GRAY},{'e',COLOR_CRIMSON},{'f',COLOR_WHITE},{'g',COLOR_GREEN},{0,0} };

static const PalEnt kPal_IMP[] =
    { {'a',COLOR_DARK_PURPLE},{'b',COLOR_CRIMSON},{'c',COLOR_GOLD},{'d',COLOR_TETRIS_L_ORANGE},{'e',COLOR_GOLD},{0,0} };

static const PalEnt kPal_BOMBER[] =
    { {'d',COLOR_TETRIS_L_ORANGE},{'b',COLOR_BROWN},{'c',COLOR_GOLD},{'h',COLOR_WHITE},{'f',COLOR_TETRIS_GRID},{'e',COLOR_BROWN},{'a',COLOR_TETRIS_GRID},{'g',COLOR_BLACK},{0,0} };

static const PalEnt kPal_MINO[] =
    { {'b',COLOR_BROWN},{'h',COLOR_LIGHT_GRAY},{'d',COLOR_DARK_GRAY},{'e',COLOR_CRIMSON},{'f',COLOR_KHAKI},{'c',COLOR_GOLD},{0,0} };

static const PalEnt kPal_MINO_HOT[] =
    { {'b',COLOR_CRIMSON},{'h',COLOR_WHITE},{'d',COLOR_BROWN},{'e',COLOR_WHITE},{'f',COLOR_PINK},{'c',COLOR_GOLD},{0,0} };

static const PalEnt kPal_ALLY_IMP[] =
    { {'a',COLOR_NAVY},{'b',COLOR_SKY_BLUE},{'c',COLOR_WHITE},{'d',COLOR_TEAL},{'e',COLOR_CYAN},{0,0} };

static const PalEnt kPal_ALLY_KNIGHT[] =
    { {'d',COLOR_SKY_BLUE},{'a',COLOR_NAVY},{'b',COLOR_WHITE},{'c',COLOR_GOLD},{0,0} };

static const PalEnt kPal_ALLY_WISP[] =
    { {'a',COLOR_TEAL},{'b',COLOR_CYAN},{'c',COLOR_WHITE},{0,0} };

static const PalEnt kPal_ARROW[] =
    { {'c',COLOR_SILVER},{'d',COLOR_BROWN},{'e',COLOR_WHITE},{0,0} };

static const PalEnt kPal_ORB[] =
    { {'a',COLOR_DARK_PURPLE},{'b',COLOR_VIOLET},{'c',COLOR_WHITE},{0,0} };

static const PalEnt kPal_BOMB[] =
    { {'a',COLOR_TETRIS_GRID},{'b',COLOR_GRAY},{'e',COLOR_TETRIS_L_ORANGE},{'d',COLOR_BROWN},{0,0} };

static const PalEnt kPal_BOMB_HOT[] =
    { {'a',COLOR_CRIMSON},{'b',COLOR_PINK},{'e',COLOR_WHITE},{'d',COLOR_GOLD},{0,0} };

static const PalEnt kPal_RING[] =
    { {'a',COLOR_CRIMSON},{'b',COLOR_TETRIS_L_ORANGE},{'c',COLOR_WHITE},{0,0} };

static const PalEnt kPal_METEOR[] =
    { {'a',COLOR_GRAY},{'e',COLOR_TETRIS_L_ORANGE},{'c',COLOR_GOLD},{'d',COLOR_WHITE},{0,0} };

static const PalEnt kPal_SOUL[] =
    { {'a',COLOR_TEAL},{'b',COLOR_WHITE},{0,0} };

static const PalEnt kPal_SLIME[] =
    { {'b',COLOR_GREEN},{'c',COLOR_MINT},{'e',COLOR_WHITE},{'f',COLOR_BLACK},{0,0} };

static const PalEnt kPal_SLIMEKING[] =
    { {'b',COLOR_TEAL},{'c',COLOR_GOLD},{'e',COLOR_WHITE},{'f',COLOR_CRIMSON},{0,0} };

static const PalEnt kPal_GOLEM[] =
    { {'g',COLOR_GRAY},{'d',COLOR_DARK_GRAY},{'e',COLOR_TETRIS_L_ORANGE},{'a',COLOR_BLACK},{0,0} };

static const PalEnt kPal_GOLEM_ICE[] =
    { {'g',COLOR_SKY_BLUE},{'d',COLOR_TEAL},{'e',COLOR_WHITE},{'a',COLOR_NAVY},{0,0} };

static const PalEnt kPal_GOLEM_ZAP[] =
    { {'g',COLOR_KHAKI},{'d',COLOR_BROWN},{'e',COLOR_WHITE},{'a',COLOR_BLACK},{0,0} };

static const PalEnt kPal_GOLEM_HOT[] =
    { {'g',COLOR_CORAL},{'d',COLOR_CRIMSON},{'e',COLOR_WHITE},{'a',COLOR_BLACK},{0,0} };

static const PalEnt kPal_VAMPIRE[] =
    { {'a',COLOR_BLACK},{'b',COLOR_LIGHT_GRAY},{'c',COLOR_CRIMSON},{'d',COLOR_DARK_GRAY},{'e',COLOR_MAROON},{'f',COLOR_WHITE},{0,0} };

static const PalEnt kPal_VAMPIRE_HOT[] =
    { {'a',COLOR_BLACK},{'b',COLOR_PINK},{'c',COLOR_WHITE},{'d',COLOR_CRIMSON},{'e',COLOR_CRIMSON},{'f',COLOR_WHITE},{0,0} };

static const PalEnt kPal_MERCH[] =
    { {'h',COLOR_BROWN},{'f',COLOR_LIGHT_GRAY},{'e',COLOR_GOLD},{'d',COLOR_GOLD},{0,0} };

static const PalEnt kPal_ICE[] =
    { {'a',COLOR_TEAL},{'b',COLOR_SKY_BLUE},{'c',COLOR_WHITE},{0,0} };

static const PalEnt kPal_HOLE[] =
    { {'a',COLOR_BLACK},{'b',COLOR_DARK_PURPLE},{'c',COLOR_VIOLET},{0,0} };

static const PalEnt kPal_CHEST[] =
    { {'a',COLOR_BROWN},{'b',COLOR_GOLD},{'c',COLOR_DARK_GRAY},{'d',COLOR_TETRIS_L_ORANGE},{0,0} };

static const PalEnt kPal_BOLT_ARROW_FIRE[] =
    { {'c',COLOR_SILVER},{'d',COLOR_BROWN},{'e',COLOR_TETRIS_L_ORANGE},{0,0} };

static const PalEnt kPal_TOXIC[] =
    { {'a',COLOR_DARK_GRAY},{'b',COLOR_GREEN},{'c',COLOR_MINT},{0,0} };

static const PalEnt *const kPal[PT_COUNT] =
{
    kPal_GOBLIN,
    kPal_BAT,
    kPal_HOBGOB,
    kPal_SKELETON,
    kPal_BEHOLDER,
    kPal_LICH,
    kPal_BEHOLDER_HOT,
    kPal_FLASK_FIRE,
    kPal_FLASK_FROST,
    kPal_FLASK_VENOM,
    kPal_FLASK_MANA,
    kPal_BOLT,
    kPal_PUFF_FIRE,
    kPal_PUFF_FROST,
    kPal_PUFF_VENOM,
    kPal_PUFF_ARCANE,
    kPal_DRAGON,
    kPal_DRAGON_HOT,
    kPal_DRAGON_SLEEP,
    kPal_ARCHER,
    kPal_SPIDER,
    kPal_IMP,
    kPal_BOMBER,
    kPal_MINO,
    kPal_MINO_HOT,
    kPal_ALLY_IMP,
    kPal_ALLY_KNIGHT,
    kPal_ALLY_WISP,
    kPal_ARROW,
    kPal_ORB,
    kPal_BOMB,
    kPal_BOMB_HOT,
    kPal_RING,
    kPal_METEOR,
    kPal_SOUL,
    kPal_SLIME,
    kPal_SLIMEKING,
    kPal_GOLEM,
    kPal_GOLEM_ICE,
    kPal_GOLEM_ZAP,
    kPal_GOLEM_HOT,
    kPal_VAMPIRE,
    kPal_VAMPIRE_HOT,
    kPal_MERCH,
    kPal_ICE,
    kPal_HOLE,
    kPal_CHEST,
    kPal_BOLT_ARROW_FIRE,
    kPal_TOXIC
};
// rc_art.h  --  ALL sprite art for the Raycaster: 16 x 16 frames and their palettes.
// Included by RayCaster.c. Edit freely: '.' = transparent, letters a..h = palette entries.
// Every row must be exactly 16 characters. Frames are listed in the SH_ enum; an enemy's
// animation is a run of consecutive frames (see kAnim in RayCaster.c).
// (Authored with tools/genart.py; the header itself is the source of truth now.)



static const char *const kArt_GOBLIN0[SPR_N] =
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

static const char *const kArt_GOBLIN1[SPR_N] =
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
    "....ee....ee....",
    "...ee......ee...",
    "..aaa......aaa..",
    "................"
};

static const char *const kArt_GOBLIN2[SPR_N] =
{
    "................",
    "................",
    "..dd.bbbbbb.ddg.",
    "..ddbbddddbbddg.",
    "...dbdcddcdbd.g.",
    "....bddddddb..g.",
    "....bdhhhhdb....",
    ".....bdddddb....",
    "...deeeeeeeed.d.",
    "..dbeeaaaaeebd..",
    "..dbeeeeeeeeb...",
    "..db.eeeeee.b...",
    ".....ee..ee.....",
    ".....ee..ee.....",
    "....aaa..aaa....",
    "................"
};

static const char *const kArt_BAT0[SPR_N] =
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

static const char *const kArt_BAT1[SPR_N] =
{
    "................",
    "a..............a",
    "aa............aa",
    "aaa....aa....aaa",
    "aaaa..abba..aaaa",
    ".aaaaaaaaaaaaaa.",
    ".accaaaaaaaacca.",
    "..a..aaaaaa..a..",
    "......adda......",
    ".......aa.......",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_BAT2[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "......aaaa......",
    ".....aabbaa.....",
    "..aaaaaaaaaaaa..",
    ".aaccaaaaaaccaa.",
    "aaa..aaddaa..aaa",
    "aa...aaaaaa...aa",
    "a.....aaaa.....a",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_SKEL0[SPR_N] =
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

static const char *const kArt_SKEL1[SPR_N] =
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
    "....dd....dd....",
    "...dd......dd...",
    "..ddd......ddd.."
};

static const char *const kArt_SKEL2[SPR_N] =
{
    "................",
    ".....dddddd.....",
    "....dddddddd..c.",
    "....dbbddbbd.cc.",
    "....dddaaddd.cc.",
    ".....dddddd.cc..",
    "......dadad.dd..",
    ".dd.dddddddd.d..",
    "..d..daddad..d..",
    "..d..dddddd..d..",
    "..d..daddad..d..",
    "..d..dddddd..d..",
    ".....dddddd.....",
    ".....dd..dd.....",
    ".....dd..dd.....",
    "....ddd..ddd...."
};

static const char *const kArt_LICH0[SPR_N] =
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

static const char *const kArt_LICH1[SPR_N] =
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
    "..deeeeeeeeeed..",
    "..dddeeeeeeddd..",
    ".....dddddd....."
};

static const char *const kArt_LICH2[SPR_N] =
{
    "..c.a.a..a.a.c..",
    "....aaaaaaaa....",
    "....bbbbbbbb....",
    "..c.bccbbccb.c..",
    ".cc.bbbbbbbb.cc.",
    ".cd..bbffbb..dc.",
    ".d.dddeeeeddd.d.",
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

static const char *const kArt_BEHOLDER0[SPR_N] =
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

static const char *const kArt_BEHOLDER1[SPR_N] =
{
    "...h.h....h.h...",
    "..h..h....h..h..",
    "..f.f......f.f..",
    "...f.f....f.f...",
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

static const char *const kArt_BEHOLDER2[SPR_N] =
{
    "..h..h....h..h..",
    "..h..h....h..h..",
    "...f.f....f.f...",
    "....f.f..f.f....",
    ".....fccccf.....",
    "....cccccccc....",
    "...ccggggggcc...",
    "...ccgdaadgcc...",
    "...ccgaaaagcc...",
    "...ccgdaadgcc...",
    "...ccggggggcc...",
    "....cccccccc....",
    "....cbbbbbbc....",
    ".....cbbbbc.....",
    "......ffff......",
    "................"
};

static const char *const kArt_DRAGON0[SPR_N] =
{
    ".....d....d.....",
    ".....da..ad.....",
    "....abbbbbba....",
    "....bdbbbbdb....",
    "....bbbbbbbb....",
    "....abccccba....",
    "..a.abbbbbba.a..",
    ".aaabbbbbbbbaaa.",
    "aaaabbcbbcbbaaaa",
    "aabaabccccbaabaa",
    ".aabbbccccbbbaa.",
    "..aabbccccbbaa..",
    "...aabbbbbbaa...",
    "....aabbbbaa....",
    "....ab.bb.ba....",
    "................"
};

static const char *const kArt_DRAGON1[SPR_N] =
{
    "aa............aa",
    "aaa..........aaa",
    "abaa.d....d.aaba",
    "abbaadabbadaabba",
    "aabbabbbbbbabbaa",
    ".aabbbccccbbbaa.",
    "..aabbbbbbbbaa..",
    "...abbbbbbbba...",
    "....bbcbbcbb....",
    "....bbccccbb....",
    ".a..bbccccbb..a.",
    "..a.abccccba.a..",
    "...aabbbbbbaa...",
    "....aabbbbaa....",
    "....ab.bb.ba....",
    "................"
};

static const char *const kArt_DRAGON2[SPR_N] =
{
    ".....d....d.....",
    ".....da..ad.....",
    "....abbbbbba....",
    "....bdbbbbdb....",
    "....bbeeeebb....",
    "....abeeeeba....",
    "..a.abeeeeba.a..",
    ".aaabbeeeebbaaa.",
    "aaaabbcbbcbbaaaa",
    "aabaabccccbaabaa",
    ".aabbbccccbbbaa.",
    "..aabbccccbbaa..",
    "...aabbbbbbaa...",
    "....aabbbbaa....",
    "....ab.bb.ba....",
    "................"
};

static const char *const kArt_ARCHER0[SPR_N] =
{
    "................",
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hdbddbdh....",
    "....hdddddh.....",
    ".....hhhhhh.....",
    "......dadad.....",
    ".dd.dddddddd.dc.",
    "..d..daddad.cde.",
    "..d..dddddd.c.de",
    "..d..daddad.cde.",
    "..d..dddddd.dce.",
    ".....dddddd.....",
    ".....dd..dd.....",
    ".....dd..dd.....",
    "....ddd..ddd...."
};

static const char *const kArt_ARCHER1[SPR_N] =
{
    "................",
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hdbddbdh....",
    "....hdddddh.....",
    ".....hhhhhh.....",
    "......dadad.....",
    ".dd.dddddddd.dc.",
    "..d..daddad.cde.",
    "..d..dddddd.c.de",
    "..d..daddad.cde.",
    "..d..dddddd.dce.",
    ".....dddddd.....",
    "....dd....dd....",
    "...dd......dd...",
    "..ddd......ddd.."
};

static const char *const kArt_ARCHER2[SPR_N] =
{
    "................",
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hdbddbdh....",
    "....hdddddh.....",
    ".....hhhhhh.....",
    "......dadad.....",
    ".dd.dddddddd.dc.",
    "..d..dadda.ccde.",
    "..deeeeeeeeccde.",
    "..d..daddad.ccd.",
    "..d..dddddd.dce.",
    ".....dddddd.....",
    ".....dd..dd.....",
    ".....dd..dd.....",
    "....ddd..ddd...."
};

static const char *const kArt_SPIDER0[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "...a........a...",
    "..a..bbbbbb..a..",
    ".a..bbbbbbbb..a.",
    "a..bbeebbeebb..a",
    "a.abbbbffbbbba.a",
    ".a.bbbbbbbbbb.a.",
    "..abbggbbggbba..",
    "..a.bbbbbbbb.a..",
    ".a...bb..bb...a.",
    "a.............a.",
    "................",
    "................"
};

static const char *const kArt_SPIDER1[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "..a..........a..",
    "...a.bbbbbb.a...",
    "..a.bbbbbbbb.a..",
    ".a.bbeebbeebb.a.",
    ".aabbbbffbbbbaa.",
    "a..bbbbbbbbbb..a",
    "a.abbggbbggbba.a",
    "..a.bbbbbbbb.a..",
    "..a..bb..bb..a..",
    ".a..........a...",
    "................",
    "................"
};

static const char *const kArt_SPIDER2[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "...a........a...",
    "..a..bbbbbb..a..",
    ".a..bbbbbbbb..a.",
    "a..bbeebbeebb..a",
    "a.abbbgggbbbba.a",
    ".a.bbbgggggbb.a.",
    "..abbggbbggbba..",
    "..a.bbbggbbb.a..",
    ".a...bb..bb...a.",
    "a.............a.",
    "................",
    "................"
};

static const char *const kArt_IMP0[SPR_N] =
{
    "..a..........a..",
    "..aa........aa..",
    "...aa.dddd.aa...",
    "...addddddda....",
    "...dcdddddcd....",
    "....dddddddd....",
    "....ddaaaadd....",
    "..bbddddddddbb..",
    ".bbbbddddddddbbb",
    ".bbb.dddddd.bbb.",
    "..b..dd..dd..b..",
    "......dd..dd....",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_IMP1[SPR_N] =
{
    "..a..........a..",
    "..aa........aa..",
    "...aa.dddd.aa...",
    "...addddddda....",
    "...dcdddddcd....",
    "....dddddddd....",
    "....ddaaaadd....",
    "...bddddddddb...",
    "..bbbddddddbbb..",
    ".bbb.dddddd.bbb.",
    "bb...dd..dd...bb",
    "......dd..dd....",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_IMP2[SPR_N] =
{
    "..a..........a..",
    "..aa........aa..",
    "e.aa..dddd..aa.e",
    "e..addddddda..e.",
    "ee.dcdddddcd.ee.",
    ".e..dddddddd..e.",
    "....ddaaaadd....",
    "...bddddddddb...",
    "..bbbddddddbbb..",
    ".bbb.dddddd.bbb.",
    "..b..dd..dd..b..",
    "......dd..dd....",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_BOMBER0[SPR_N] =
{
    "................",
    "................",
    "..dd.bbbbbb.dd..",
    "..ddbbddddbbdd..",
    "...dbdcddcdbd...",
    "....bddddddb....",
    "....bdhffhdb....",
    ".....bdddddb..c.",
    "...deeeeeeeed.gg",
    "..dbeeaaaaeebggg",
    "..dbeeeeeeeebdgg",
    "..db.eeeeee.b.d.",
    ".....ee..ee.....",
    ".....ee..ee.....",
    "....aaa..aaa....",
    "................"
};

static const char *const kArt_BOMBER1[SPR_N] =
{
    "................",
    "................",
    "..dd.bbbbbb.dd..",
    "..ddbbddddbbdd..",
    "...dbdcddcdbd...",
    "....bddddddb....",
    "....bdhffhdb....",
    ".....bdddddb..c.",
    "...deeeeeeeed.gg",
    "..dbeeaaaaeebggg",
    "..dbeeeeeeeebdgg",
    "..db.eeeeee.b.d.",
    "....ee....ee....",
    "...ee......ee...",
    "..aaa......aaa..",
    "................"
};

static const char *const kArt_BOMBER2[SPR_N] =
{
    "................",
    "................",
    "..dd.bbbbbb.ddc.",
    "..ddbbddddbbddgg",
    "...dbdcddcdbdggg",
    "....bddddddb.ggg",
    "....bdhhhhdb.gg.",
    ".....bdddddb....",
    "...deeeeeeeed.d.",
    "..dbeeaaaaeebd..",
    "..dbeeeeeeeeb...",
    "..db.eeeeee.b...",
    ".....ee..ee.....",
    ".....ee..ee.....",
    "....aaa..aaa....",
    "................"
};

static const char *const kArt_SLIME0[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "....bbbbbbbb....",
    "..bbbbbbbbbbbb..",
    ".bbbcbbbbbbbbbb.",
    ".bbbbbbbbbbbbbb.",
    ".bbeeebbbeeebbb.",
    ".bbeffbbbeffbbb.",
    ".bbbbbbbbbbbbbb.",
    "..bbbbbbbbbbbb..",
    "....bbbbbbbb....",
    "................"
};

static const char *const kArt_SLIME1[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......bbbb......",
    "....bbbbbbbb....",
    "...bbcbbbbbbb...",
    "..bbbbbbbbbbbb..",
    "..bbeeebbeeebb..",
    "..bbeffbbeffbb..",
    ".bbbbbbbbbbbbbb.",
    "bbbbbbbbbbbbbbbb",
    ".bbbbbbbbbbbbbb.",
    "................"
};

static const char *const kArt_SLIME2[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "....bbbbbbbb....",
    "..bbbbcbbbbbbb..",
    ".bbbbbbbbbbbbbb.",
    ".bbeeebbbeeebbb.",
    ".bbeffbbbeffbbb.",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    ".bbbbbbbbbbbbbb.",
    "................"
};

static const char *const kArt_GOLEM0[SPR_N] =
{
    "................",
    ".....gggggg.....",
    "....gggggggg....",
    "....ggeggegg....",
    "....gggggggg....",
    "..gggg.gg.gggg..",
    ".ggggggggggggggg",
    "gggdggggggggdggg",
    "gggdgggaagggdggg",
    "gggdggggggggdggg",
    ".ggg.gggggg.ggg.",
    "....gggggggg....",
    "....ggg..ggg....",
    "....ggg..ggg....",
    "...dddd..dddd...",
    "................"
};

static const char *const kArt_GOLEM1[SPR_N] =
{
    "................",
    ".....gggggg.....",
    "....gggggggg....",
    "....ggeggegg....",
    "....gggggggg....",
    "..gggg.gg.gggg..",
    ".ggggggggggggggg",
    "gggdggggggggdggg",
    "gggdgggaagggdggg",
    "gggdggggggggdggg",
    ".ggg.gggggg.ggg.",
    "....gggggggg....",
    "...ggg....ggg...",
    "...ggg....ggg...",
    "..dddd....dddd..",
    "................"
};

static const char *const kArt_GOLEM2[SPR_N] =
{
    ".....gggggg.....",
    "...gg.gggggg.gg.",
    "...gg.gggggg.gg.",
    "...gg.ggeegg.gg.",
    "...gg.gggggg.gg.",
    "...ggggggggggg..",
    "..ggggggggggggg.",
    "gggdggggggggdggg",
    "gggdgggaagggdggg",
    "gggdggggggggdggg",
    ".ggg.gggggg.ggg.",
    "....gggggggg....",
    "....ggg..ggg....",
    "....ggg..ggg....",
    "...dddd..dddd...",
    "................"
};

static const char *const kArt_MERCH0[SPR_N] =
{
    "................",
    "................",
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hhfffhhh....",
    "....hhfeefhh....",
    "....hhhfffhh....",
    "...hhhhhhhhhhh..",
    "..hhhhhhhhhhhhd.",
    "..hhdhhhhhhhhdd.",
    "..hhhhhhhhhhhdd.",
    "..hhhhhhhhhhhdd.",
    "...hhhhhhhhhhd..",
    "...hhhhhhhhhh...",
    "....hhh..hhh....",
    "................"
};

static const char *const kArt_MERCH1[SPR_N] =
{
    "................",
    "................",
    ".....hhhhhh.....",
    "....hhhhhhhh....",
    "....hhffhhhh....",
    "....hhfeefhh....",
    "....hhhfffhh....",
    "...hhhhhhhhhhh..",
    "..hhhhhhhhhhhd..",
    "..hhdhhhhhhhhd..",
    "..hhhhhhhhhhhdd.",
    "..hhhhhhhhhhhdd.",
    "...hhhhhhhhhhd..",
    "...hhhhhhhhhh...",
    "....hhh..hhh....",
    "................"
};

static const char *const kArt_CHEST0[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "....aaaaaaaaaa..",
    "...aaaaaaaaaaaa.",
    "...abbbbbbbbbba.",
    "...aaaaabbaaaaa.",
    "...aaaaabbaaaaa.",
    "...acaaaaaaaaca.",
    "...aaaaaaaaaaaa.",
    "....dddddddddd..",
    "................"
};

static const char *const kArt_CHEST1[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "...aa..........a",
    "..aaaaaaaaaaaaaa",
    "...aaaaaaaaaaaa.",
    "....bbbbbbbbbb..",
    "...aaaaaaaaaaaa.",
    "...abbbbbbbbbba.",
    "...aaaaabbaaaaa.",
    "...acaaaaaaaaca.",
    "...aaaaaaaaaaaa.",
    "....dddddddddd..",
    "................"
};

static const char *const kArt_MINO0[SPR_N] =
{
    "h..............h",
    "hh............hh",
    ".hh..bbbbbb..hh.",
    "..hhbbbbbbbbhh..",
    "...bbbebbebbb...",
    "...bbbbbbbbbb...",
    "....bbffffbb....",
    "....bbfccfbb....",
    "..bbbbbbbbbbbb..",
    ".bbbddbbbbddbbb.",
    ".bbbddbbbbddbbb.",
    ".bb.bbbbbbbb.bb.",
    "....bbb..bbb....",
    "....bbb..bbb....",
    "....ddd..ddd....",
    "................"
};

static const char *const kArt_MINO1[SPR_N] =
{
    "h..............h",
    "hh............hh",
    ".hh..bbbbbb..hh.",
    "..hhbbbbbbbbhh..",
    "...bbbebbebbb...",
    "...bbbbbbbbbb...",
    "....bbffffbb....",
    "....bbfccfbb....",
    "..bbbbbbbbbbbb..",
    ".bbbddbbbbddbbb.",
    ".bbbddbbbbddbbb.",
    ".bb.bbbbbbbb.bb.",
    "...bbb....bbb...",
    "...bbb....bbb...",
    "...ddd....ddd...",
    "................"
};

static const char *const kArt_MINO2[SPR_N] =
{
    "................",
    "h..............h",
    "hh.bbbbbbbbb.hh.",
    ".hhbbbbbbbbbbhh.",
    "..bbbbebbebbbb..",
    "..bbbbbbbbbbbb..",
    "...bbbffffbbb...",
    "...bbbfccfbbb...",
    "..bbbbbbbbbbbb..",
    ".bbbddbbbbddbbb.",
    ".bbbddbbbbddbbb.",
    ".bb.bbbbbbbb.bb.",
    "...bbb....bbb...",
    "...bbb....bbb...",
    "....ddd..ddd....",
    "................"
};

static const char *const kArt_MINO3[SPR_N] =
{
    "................",
    "................",
    "h..............h",
    "hhh..........hhh",
    ".hhhbbbbbbbbhhh.",
    "..bbbbebbebbbb..",
    "..bbbbbbbbbbbb..",
    "..bbbbffffbbbb..",
    "..bbbbfccfbbbb..",
    ".bbbbbbbbbbbbbb.",
    "bbbddbbbbbbddbbb",
    ".bb..bbbbbb..bb.",
    "....bb....bb....",
    "...bb......bb...",
    "...dd......dd...",
    "................"
};

static const char *const kArt_ARROW[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    ".......cc.......",
    "......cddc......",
    ".......dd.......",
    ".......dd.......",
    ".......dd.......",
    ".......ee.......",
    "......e..e......",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_ORB0[SPR_N] =
{
    "................",
    "................",
    "................",
    "......aaaa......",
    "....aabbbbaa....",
    "...abbccbbbba...",
    "...abcbbbbbba...",
    "..abbbbbbbcbba..",
    "..abbbbbbbbbba..",
    "...abbbbccbba...",
    "...abbbbbbbba...",
    "....aabbbbaa....",
    "......aaaa......",
    "................",
    "................",
    "................"
};

static const char *const kArt_ORB1[SPR_N] =
{
    "................",
    "................",
    "................",
    "......aaaa......",
    "....aabbbbaa....",
    "...abbbbbbbba...",
    "...abbbbcbbba...",
    "..abbbbbbbbbba..",
    "..abbbcbbbbbba..",
    "...abbbbbccba...",
    "...abbbbbbbba...",
    "....aabbbbaa....",
    "......aaaa......",
    "................",
    "................",
    "................"
};

static const char *const kArt_BOMB0[SPR_N] =
{
    "................",
    "................",
    "........e.......",
    ".......ed.......",
    "......dd........",
    "....aaaaaa......",
    "...aabbaaaa.....",
    "..aabbaaaaaa....",
    "..aaaaaaaaaa....",
    "..aaaaaaaaaa....",
    "...aaaaaaaa.....",
    "....aaaaaa......",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_BOMB1[SPR_N] =
{
    "................",
    "................",
    ".......e........",
    "......de........",
    "......dd........",
    "....aaaaaa......",
    "...aabbaaaa.....",
    "..aabbaaaaaa....",
    "..aaaaaaaaaa....",
    "..aaaaaaaaaa....",
    "...aaaaaaaa.....",
    "....aaaaaa......",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_RING0[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....aaaaaa.....",
    "..aaa......aaa..",
    ".aa..........aa.",
    "aa............aa",
    ".aa..........aa.",
    "..aaa......aaa..",
    "................"
};

static const char *const kArt_RING1[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    ".....bbbbbb.....",
    "..bbb......bbb..",
    ".bb....cc....bb.",
    "bb....cccc....bb",
    ".bb....cc....bb.",
    "..bbb......bbb..",
    ".....bbbbbb....."
};

static const char *const kArt_METEOR0[SPR_N] =
{
    "................",
    "......ee........",
    ".....eeee.......",
    "....eeccee......",
    "...eeccccee.....",
    "...eccddcce.....",
    "...eccddcce.....",
    "...eeccccee.....",
    "....eeccee......",
    "....aeeeea......",
    ".....a.ee.a.....",
    "......a..a......",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_METEOR1[SPR_N] =
{
    "................",
    ".....ee.........",
    ".....eeee.......",
    "....eeccee......",
    "...eeccccee.....",
    "...eccddcce.....",
    "...eccddcce.....",
    "...eeccccee.....",
    "....eeccee......",
    "....aeeeea......",
    "....a.ee..a.....",
    ".....a..a.......",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_FLASK[SPR_N] =
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

static const char *const kArt_BOLT0[SPR_N] =
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

static const char *const kArt_BOLT1[SPR_N] =
{
    "................",
    "................",
    "................",
    "................",
    "................",
    ".......aa.......",
    "......abba......",
    ".....aabbaa.....",
    ".....aabbaa.....",
    "......abba......",
    ".......aa.......",
    "................",
    "................",
    "................",
    "................",
    "................"
};

static const char *const kArt_PUFF[SPR_N] =
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


/* Frame lookup table. Keep this in the same order as the SH_* enum above. */
static const char *const *const kArt[SH_COUNT] =
{
    kArt_GOBLIN0,
    kArt_GOBLIN1,
    kArt_GOBLIN2,
    kArt_BAT0,
    kArt_BAT1,
    kArt_BAT2,
    kArt_SKEL0,
    kArt_SKEL1,
    kArt_SKEL2,
    kArt_LICH0,
    kArt_LICH1,
    kArt_LICH2,
    kArt_BEHOLDER0,
    kArt_BEHOLDER1,
    kArt_BEHOLDER2,
    kArt_DRAGON0,
    kArt_DRAGON1,
    kArt_DRAGON2,
    kArt_ARCHER0,
    kArt_ARCHER1,
    kArt_ARCHER2,
    kArt_SPIDER0,
    kArt_SPIDER1,
    kArt_SPIDER2,
    kArt_IMP0,
    kArt_IMP1,
    kArt_IMP2,
    kArt_BOMBER0,
    kArt_BOMBER1,
    kArt_BOMBER2,
    kArt_SLIME0,
    kArt_SLIME1,
    kArt_SLIME2,
    kArt_GOLEM0,
    kArt_GOLEM1,
    kArt_GOLEM2,
    kArt_MERCH0,
    kArt_MERCH1,
    kArt_CHEST0,
    kArt_CHEST1,
    kArt_MINO0,
    kArt_MINO1,
    kArt_MINO2,
    kArt_MINO3,
    kArt_ARROW,
    kArt_ORB0,
    kArt_ORB1,
    kArt_BOMB0,
    kArt_BOMB1,
    kArt_RING0,
    kArt_RING1,
    kArt_METEOR0,
    kArt_METEOR1,
    kArt_FLASK,
    kArt_BOLT0,
    kArt_BOLT1,
    kArt_PUFF
};

#endif /* RC_ART_H */
