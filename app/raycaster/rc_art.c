// rc_art.c - sprite art, palettes, wall themes and the first-person item art.
#include "rc_priv.h"












static const PalEnt kP_HOBGOB[] = { {'a',COLOR_TETRIS_GRID},{'b',COLOR_BROWN},{'c',COLOR_GRAY},{'d',COLOR_DARK_GRAY},{'e',COLOR_LIGHT_GRAY},{'f',COLOR_CRIMSON},{'h',COLOR_WHITE},{0,0} };
static const PalEnt kP_SLIME[] = { {'a',COLOR_TEAL},{'b',COLOR_GREEN},{'c',COLOR_TETRIS_GRID},{'h',COLOR_MINT},{0,0} };
static const PalEnt kP_SLIME_S[] = { {'a',COLOR_OLIVE},{'b',COLOR_MINT},{'c',COLOR_TETRIS_GRID},{'h',COLOR_WHITE},{0,0} };
static const PalEnt kP_SPIDER[] = { {'a',COLOR_GRAY},{'b',COLOR_PURPLE},{'c',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_GHOST[] = { {'a',COLOR_SKY_BLUE},{'b',COLOR_WHITE},{'c',COLOR_NAVY},{0,0} };
static const PalEnt kP_GHOST_FADE[] = { {'a',COLOR_TEAL},{'b',COLOR_CYAN},{'c',COLOR_NAVY},{0,0} };
static const PalEnt kP_IMP[] = { {'a',COLOR_MAROON},{'b',COLOR_CRIMSON},{'c',COLOR_ORANGE},{'d',COLOR_BLACK},{'f',COLOR_GOLD},{0,0} };
static const PalEnt kP_IMP_LIT[] = { {'a',COLOR_MAROON},{'b',COLOR_TETRIS_L_ORANGE},{'c',COLOR_GOLD},{'d',COLOR_BLACK},{'f',COLOR_WHITE},{0,0} };
static const PalEnt kP_EYEKING[] = { {'a',COLOR_BLACK},{'b',COLOR_TETRIS_GRID},{'c',COLOR_MAROON},{'d',COLOR_CRIMSON},{'e',COLOR_WHITE},{'f',COLOR_BROWN},{'g',COLOR_WHITE},{'h',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_EYEKING_FIRE[] = { {'a',COLOR_BLACK},{'b',COLOR_TETRIS_GRID},{'c',COLOR_MAROON},{'d',COLOR_TETRIS_L_ORANGE},{'e',COLOR_WHITE},{'f',COLOR_BROWN},{'g',COLOR_WHITE},{'h',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_EYEKING_ICE[] = { {'a',COLOR_BLACK},{'b',COLOR_TETRIS_GRID},{'c',COLOR_MAROON},{'d',COLOR_SKY_BLUE},{'e',COLOR_WHITE},{'f',COLOR_BROWN},{'g',COLOR_WHITE},{'h',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_EYEKING_BOLT[] = { {'a',COLOR_BLACK},{'b',COLOR_TETRIS_GRID},{'c',COLOR_MAROON},{'d',COLOR_GOLD},{'e',COLOR_WHITE},{'f',COLOR_BROWN},{'g',COLOR_WHITE},{'h',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_EYEKING_GRAV[] = { {'a',COLOR_BLACK},{'b',COLOR_TETRIS_GRID},{'c',COLOR_MAROON},{'d',COLOR_VIOLET},{'e',COLOR_WHITE},{'f',COLOR_BROWN},{'g',COLOR_WHITE},{'h',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_EYEKING_NULL[] = { {'a',COLOR_BLACK},{'b',COLOR_TETRIS_GRID},{'c',COLOR_MAROON},{'d',COLOR_GRAY},{'e',COLOR_WHITE},{'f',COLOR_BROWN},{'g',COLOR_WHITE},{'h',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_SLIMEKING[] = { {'a',COLOR_TEAL},{'b',COLOR_GREEN},{'c',COLOR_TETRIS_GRID},{'g',COLOR_GOLD},{'h',COLOR_MINT},{0,0} };
static const PalEnt kP_GOLEM_FIRE[] = { {'a',COLOR_DARK_GRAY},{'b',COLOR_GRAY},{'c',COLOR_TETRIS_L_ORANGE},{'d',COLOR_TETRIS_GRID},{'e',COLOR_GOLD},{0,0} };
static const PalEnt kP_GOLEM_ICE[] = { {'a',COLOR_DARK_GRAY},{'b',COLOR_GRAY},{'c',COLOR_SKY_BLUE},{'d',COLOR_TETRIS_GRID},{'e',COLOR_WHITE},{0,0} };
static const PalEnt kP_GOLEM_STORM[] = { {'a',COLOR_DARK_GRAY},{'b',COLOR_GRAY},{'c',COLOR_GOLD},{'d',COLOR_TETRIS_GRID},{'e',COLOR_WHITE},{0,0} };
static const PalEnt kP_VAMP[] = { {'a',COLOR_TETRIS_GRID},{'b',COLOR_LIGHT_GRAY},{'c',COLOR_MAROON},{'d',COLOR_CRIMSON},{'e',COLOR_BLACK},{'f',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_CHEST[] = { {'a',COLOR_BLACK},{'b',COLOR_BROWN},{'c',COLOR_KHAKI},{'d',COLOR_GOLD},{0,0} };
static const PalEnt kP_CHEST_CURSED[] = { {'a',COLOR_BLACK},{'b',COLOR_DARK_PURPLE},{'c',COLOR_CRIMSON},{'d',COLOR_VIOLET},{0,0} };
static const PalEnt kP_MIMIC[] = { {'a',COLOR_BLACK},{'b',COLOR_BROWN},{'c',COLOR_CRIMSON},{'d',COLOR_GOLD},{'e',COLOR_WHITE},{0,0} };
static const PalEnt kP_ORB_PURPLE[] = { {'a',COLOR_DARK_PURPLE},{'b',COLOR_VIOLET},{'c',COLOR_PINK},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_ORB_GREEN[] = { {'a',COLOR_TEAL},{'b',COLOR_GREEN},{'c',COLOR_MINT},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_ORB_BLOOD[] = { {'a',COLOR_MAROON},{'b',COLOR_CRIMSON},{'c',COLOR_CORAL},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_ORB_FIRE[] = { {'a',COLOR_MAROON},{'b',COLOR_TETRIS_L_ORANGE},{'c',COLOR_GOLD},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_ORB_ICE[] = { {'a',COLOR_TEAL},{'b',COLOR_SKY_BLUE},{'c',COLOR_WHITE},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_ORB_BOLT[] = { {'a',COLOR_ORANGE},{'b',COLOR_GOLD},{'c',COLOR_KHAKI},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_WEB[] = { {'a',COLOR_LIGHT_GRAY},{'b',COLOR_WHITE},{0,0} };
static const PalEnt kP_PUDDLE_SLIME[] = { {'a',COLOR_OLIVE},{'b',COLOR_GREEN},{'h',COLOR_MINT},{0,0} };
static const PalEnt kP_PUDDLE_FIRE[] = { {'a',COLOR_MAROON},{'b',COLOR_TETRIS_L_ORANGE},{'h',COLOR_GOLD},{0,0} };
static const PalEnt kP_PUDDLE_WEB[] = { {'a',COLOR_GRAY},{'b',COLOR_LIGHT_GRAY},{'h',COLOR_WHITE},{0,0} };
static const PalEnt kP_PUDDLE_BLAST[] = { {'a',COLOR_BROWN},{'b',COLOR_TETRIS_L_ORANGE},{'h',COLOR_KHAKI},{0,0} };
static const PalEnt kP_WARN[] = { {'a',COLOR_CRIMSON},{0,0} };
static const PalEnt kP_WAVE_SLIME[] = { {'a',COLOR_OLIVE},{'b',COLOR_GREEN},{0,0} };
static const PalEnt kP_WAVE_FIRE[] = { {'a',COLOR_MAROON},{'b',COLOR_TETRIS_L_ORANGE},{0,0} };
static const PalEnt kP_WAVE_ICE[] = { {'a',COLOR_TEAL},{'b',COLOR_SKY_BLUE},{0,0} };
static const PalEnt kP_WAVE_STORM[] = { {'a',COLOR_ORANGE},{'b',COLOR_GOLD},{0,0} };
static const PalEnt kP_WAVE_NOVA[] = { {'a',COLOR_DARK_PURPLE},{'b',COLOR_VIOLET},{'h',COLOR_PINK},{0,0} };
static const PalEnt kP_ALERT[] = { {'a',COLOR_CRIMSON},{'b',COLOR_GOLD},{0,0} };

static const PalEnt kP_ARCHER[] = { {'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_BROWN},{'d',COLOR_LIGHT_GRAY},{'e',COLOR_WHITE},{0,0} };
static const PalEnt kP_KEY[] = { {'a',COLOR_GOLD},{'b',COLOR_BROWN},{'c',COLOR_WHITE},{0,0} };
static const PalEnt kP_HEART[] = { {'a',COLOR_CRIMSON},{'b',COLOR_PINK},{'c',COLOR_MAROON},{0,0} };
static const PalEnt kP_ALTAR[] = { {'a',COLOR_DARK_PURPLE},{'b',COLOR_GOLD},{'c',COLOR_KHAKI},{'d',COLOR_WHITE},{0,0} };
static const PalEnt kP_CHEST_GOOD[] = { {'a',COLOR_BLACK},{'b',COLOR_TEAL},{'c',COLOR_GOLD},{'d',COLOR_WHITE},{0,0} };

static const PalEnt kP_MERCHANT[] = { {'a',COLOR_DARK_PURPLE},{'b',COLOR_VIOLET},{'c',COLOR_TEAL},{'d',COLOR_KHAKI},{'e',COLOR_BLACK},{'f',COLOR_GOLD},{'g',COLOR_BROWN},{'h',COLOR_WHITE},{0,0} };

// Sprite art, 16x16, packed 4 bits per pixel (0 = transparent, 1..8 = palette letter a..h), 4 pixels
// per word, 4 words per row (leftmost pixel in the lowest nibble). The ASCII picture above each
// sprite is the source; keep the two in step when editing. (Packing saves ~8K words of flash.)
const uint16_t rc_kArt[SH_COUNT][SPR_N * 4] =
{
    // kArtGoblin
    //   ................
    //   ................
    //   ..dd.bbbbbb.dd..
    //   ..ddbbddddbbdd..
    //   ...dbdcddcdbd...
    //   ....bddddddb....
    //   ....bdhffhdb....
    //   .....bdddddb....
    //   ...deeeeeeeed.g.
    //   ..dbeeaaaaeebdg.
    //   ..dbeeeeeeeebdg.
    //   ..db.eeeeee.bda.
    //   .....ee..ee.....
    //   .....ee..ee.....
    //   ....aaa..aaa....
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x4400, 0x2220, 0x0222, 0x0044,
        0x4400, 0x4422, 0x2244, 0x0044,
        0x4000, 0x4342, 0x2434, 0x0004,
        0x0000, 0x4442, 0x2444, 0x0000,
        0x0000, 0x6842, 0x2486, 0x0000,
        0x0000, 0x4420, 0x2444, 0x0000,
        0x4000, 0x5555, 0x5555, 0x0704,
        0x2400, 0x1155, 0x5511, 0x0742,
        0x2400, 0x5555, 0x5555, 0x0742,
        0x2400, 0x5550, 0x0555, 0x0142,
        0x0000, 0x0550, 0x0550, 0x0000,
        0x0000, 0x0550, 0x0550, 0x0000,
        0x0000, 0x0111, 0x1110, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtBat
    //   ................
    //   ................
    //   .aa..........aa.
    //   .aaa...aa...aaa.
    //   .aaaa.aaaa.aaaa.
    //   aaaaaaabbaaaaaaa
    //   .accaaaaaaaacca.
    //   ..ac.aaaaaa.ca..
    //   ...a..adda..a...
    //   ......aaaa......
    //   .......aa.......
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0110, 0x0000, 0x0000, 0x0110,
        0x1110, 0x1000, 0x0001, 0x0111,
        0x1110, 0x1101, 0x1011, 0x0111,
        0x1111, 0x2111, 0x1112, 0x1111,
        0x3310, 0x1111, 0x1111, 0x0133,
        0x3100, 0x1110, 0x0111, 0x0013,
        0x1000, 0x4100, 0x0014, 0x0001,
        0x0000, 0x1100, 0x0011, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtSkeleton
    //   ................
    //   .....dddddd.....
    //   ....dddddddd....
    //   ....dbbddbbd....
    //   ....dddaaddd....
    //   .....dddddd.....
    //   ......dadad.....
    //   .dd.dddddddd.dd.
    //   ..d..daddad..d.c
    //   ..d..dddddd..d.c
    //   ..d..daddad..d.c
    //   ..d..dddddd..dcc
    //   .....dddddd.....
    //   .....dd..dd.....
    //   .....dd..dd.....
    //   ....ddd..ddd....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4440, 0x0444, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x0000, 0x4224, 0x4224, 0x0000,
        0x0000, 0x1444, 0x4441, 0x0000,
        0x0000, 0x4440, 0x0444, 0x0000,
        0x0000, 0x1400, 0x0414, 0x0000,
        0x0440, 0x4444, 0x4444, 0x0440,
        0x0400, 0x4140, 0x0414, 0x3040,
        0x0400, 0x4440, 0x0444, 0x3040,
        0x0400, 0x4140, 0x0414, 0x3040,
        0x0400, 0x4440, 0x0444, 0x3340,
        0x0000, 0x4440, 0x0444, 0x0000,
        0x0000, 0x0440, 0x0440, 0x0000,
        0x0000, 0x0440, 0x0440, 0x0000,
        0x0000, 0x0444, 0x4440, 0x0000,
    },
    // kArtLichLordA
    //   ..............c.
    //   .............c..
    //   ..c..aaaaaa.cc..
    //   ...caaccccaac...
    //   ....acdccdcca...
    //   ....aacccccaa...
    //   ....aaacccaaa..f
    //   ...aaaaaaaaaaa.f
    //   ..aabaaaaaabaaef
    //   ..aabaaaaaabaa.f
    //   .caabaaaaaabaa.f
    //   ..aabaaaaaabaa.f
    //   ..aaabaaaaabaaa.
    //   ..aaabaaaabaaaa.
    //   .aaaabaaaabaaaa.
    //   .a.aa.aa.aa.a.a.
    {
        0x0000, 0x0000, 0x0000, 0x0300,
        0x0000, 0x0000, 0x0000, 0x0030,
        0x0300, 0x1110, 0x0111, 0x0033,
        0x3000, 0x3311, 0x1133, 0x0003,
        0x0000, 0x3431, 0x3343, 0x0001,
        0x0000, 0x3311, 0x1333, 0x0001,
        0x0000, 0x3111, 0x1133, 0x6001,
        0x1000, 0x1111, 0x1111, 0x6011,
        0x1100, 0x1112, 0x2111, 0x6511,
        0x1100, 0x1112, 0x2111, 0x6011,
        0x1130, 0x1112, 0x2111, 0x6011,
        0x1100, 0x1112, 0x2111, 0x6011,
        0x1100, 0x1121, 0x2111, 0x0111,
        0x1100, 0x1121, 0x1211, 0x0111,
        0x1110, 0x1121, 0x1211, 0x0111,
        0x1010, 0x1101, 0x0110, 0x0101,
    },
    // kArtBeholder
    //   ..h..h....h..h..
    //   ..h..h....h..h..
    //   ...f.f....f.f...
    //   ....f.f..f.f....
    //   .....fccccf.....
    //   ....cccccccc....
    //   ...ccggggggcc...
    //   ...ccgddddgcc...
    //   ...ccgdaadgcc...
    //   ...ccgddddgcc...
    //   ...ccggggggcc...
    //   ....cccccccc....
    //   ....cebebecc....
    //   .....cccccc.....
    //   ......ffff......
    //   ................
    {
        0x0800, 0x0080, 0x0800, 0x0080,
        0x0800, 0x0080, 0x0800, 0x0080,
        0x6000, 0x0060, 0x0600, 0x0006,
        0x0000, 0x0606, 0x6060, 0x0000,
        0x0000, 0x3360, 0x0633, 0x0000,
        0x0000, 0x3333, 0x3333, 0x0000,
        0x3000, 0x7773, 0x3777, 0x0003,
        0x3000, 0x4473, 0x3744, 0x0003,
        0x3000, 0x1473, 0x3741, 0x0003,
        0x3000, 0x4473, 0x3744, 0x0003,
        0x3000, 0x7773, 0x3777, 0x0003,
        0x0000, 0x3333, 0x3333, 0x0000,
        0x0000, 0x5253, 0x3352, 0x0000,
        0x0000, 0x3330, 0x0333, 0x0000,
        0x0000, 0x6600, 0x0066, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtFlask
    //   ................
    //   ................
    //   ................
    //   ................
    //   .......bb.......
    //   ......bddb......
    //   ......bccb......
    //   .....bcaacb.....
    //   ....bcaaaacb....
    //   ....baaaaaab....
    //   ....baaaaaab....
    //   .....baaaab.....
    //   ......bbbb......
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x2000, 0x0002, 0x0000,
        0x0000, 0x4200, 0x0024, 0x0000,
        0x0000, 0x3200, 0x0023, 0x0000,
        0x0000, 0x1320, 0x0231, 0x0000,
        0x0000, 0x1132, 0x2311, 0x0000,
        0x0000, 0x1112, 0x2111, 0x0000,
        0x0000, 0x1112, 0x2111, 0x0000,
        0x0000, 0x1120, 0x0211, 0x0000,
        0x0000, 0x2200, 0x0022, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtBolt
    //   ................
    //   ................
    //   ................
    //   ................
    //   .......aa.......
    //   ......aaaa......
    //   .....aabbaa.....
    //   ....aabbbbaa....
    //   ....aabbbbaa....
    //   .....aabbaa.....
    //   ......aaaa......
    //   .......aa.......
    //   ................
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x1100, 0x0011, 0x0000,
        0x0000, 0x2110, 0x0112, 0x0000,
        0x0000, 0x2211, 0x1122, 0x0000,
        0x0000, 0x2211, 0x1122, 0x0000,
        0x0000, 0x2110, 0x0112, 0x0000,
        0x0000, 0x1100, 0x0011, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtPuff
    //   ................
    //   ................
    //   .....aabbaa.....
    //   ...aabbbbbbaa...
    //   ..aabbccbbbbaa..
    //   .aabbbccbbbbbaa.
    //   .abbbbbbbbccbba.
    //   aabbccbbbbbbbbaa
    //   abbbbbbbccbbbbba
    //   abbbccbbbbbbbbba
    //   .abbbbbbbbbccba.
    //   .aabbbccbbbbbaa.
    //   ..aabbbbbbbbaa..
    //   ...aabbbbbbaa...
    //   .....aaaaaa.....
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x2110, 0x0112, 0x0000,
        0x1000, 0x2221, 0x1222, 0x0001,
        0x1100, 0x3322, 0x2222, 0x0011,
        0x2110, 0x3322, 0x2222, 0x0112,
        0x2210, 0x2222, 0x3322, 0x0122,
        0x2211, 0x2233, 0x2222, 0x1122,
        0x2221, 0x2222, 0x2233, 0x1222,
        0x2221, 0x2233, 0x2222, 0x1222,
        0x2210, 0x2222, 0x3222, 0x0123,
        0x2110, 0x3322, 0x2222, 0x0112,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x1000, 0x2221, 0x1222, 0x0001,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtDragon
    //   ................
    //   .......aa.......
    //   ......abca......
    //   .....abbbca.....
    //   ....abbbbbca....
    //   ...abbbddbbca...
    //   ..abbbbdcbbbc...
    //   .abbbbbbbbbbbba.
    //   .abbbbbbbbbbbba.
    //   ..abbbbbbbbbba..
    //   ...abbbbbbba....
    //   ....abbbbbba....
    //   ...aa..bb..aa...
    //   ..aa...bb...aa..
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x2100, 0x0013, 0x0000,
        0x0000, 0x2210, 0x0132, 0x0000,
        0x0000, 0x2221, 0x1322, 0x0000,
        0x1000, 0x4222, 0x3224, 0x0001,
        0x2100, 0x4222, 0x2223, 0x0003,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2100, 0x2222, 0x2222, 0x0012,
        0x1000, 0x2222, 0x1222, 0x0000,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x1000, 0x2001, 0x1002, 0x0001,
        0x1100, 0x2000, 0x0002, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtDragonAttack
    //   ................
    //   .......aa.......
    //   ......abca......
    //   .....abbbca.....
    //   ....abbbbbca....
    //   ...abbbddbbca...
    //   ..abbbbdcbbbc...
    //   .abbbbbbbbbbbba.
    //   .abbbbbbbbbbbba.
    //   ..abbbbbbbbcba..
    //   ...abbbbbeeea...
    //   ....abbbbeeea...
    //   .....abbeeea....
    //   ......aeee......
    //   .......ee.......
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x2100, 0x0013, 0x0000,
        0x0000, 0x2210, 0x0132, 0x0000,
        0x0000, 0x2221, 0x1322, 0x0000,
        0x1000, 0x4222, 0x3224, 0x0001,
        0x2100, 0x4222, 0x2223, 0x0003,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2100, 0x2222, 0x3222, 0x0012,
        0x1000, 0x2222, 0x5552, 0x0001,
        0x0000, 0x2221, 0x5552, 0x0001,
        0x0000, 0x2210, 0x1555, 0x0000,
        0x0000, 0x5100, 0x0055, 0x0000,
        0x0000, 0x5000, 0x0005, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtHobgob
    //   ................
    //   .....aaaaaa.....
    //   ....aeeeeeea....
    //   ...aeebbbbeea...
    //   ...aebhffhbea...
    //   ....abddddba....
    //   ...aaabbbbaaa...
    //   ..acccaaaacccca.
    //   ..acdccccccdcca.
    //   .aacdceeeecdcaa.
    //   .aecdceddeecdcea
    //   .aecdccccccdcea.
    //   ..aabbccccbbaa..
    //   ....bba..abb....
    //   ....bba..abb....
    //   ...aaaa..aaaa...
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x5551, 0x1555, 0x0000,
        0x1000, 0x2255, 0x5522, 0x0001,
        0x1000, 0x6825, 0x5286, 0x0001,
        0x0000, 0x4421, 0x1244, 0x0000,
        0x1000, 0x2211, 0x1122, 0x0001,
        0x3100, 0x1133, 0x3311, 0x0133,
        0x3100, 0x3334, 0x4333, 0x0133,
        0x3110, 0x5534, 0x4355, 0x0113,
        0x3510, 0x4534, 0x3554, 0x1534,
        0x3510, 0x3334, 0x4333, 0x0153,
        0x1100, 0x3322, 0x2233, 0x0011,
        0x0000, 0x0122, 0x2210, 0x0000,
        0x0000, 0x0122, 0x2210, 0x0000,
        0x1000, 0x0111, 0x1110, 0x0001,
    },
    // kArtSlime
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ......aaaa......
    //   ....aabbbbaa....
    //   ...abbbbbbbba...
    //   ..abbhhbbbbbba..
    //   ..abhhbbbbbbba..
    //   .abbbbcbbcbbbba.
    //   .abbbbcbbcbbbba.
    //   .abbbbbbbbbbbba.
    //   .abbbbbccbbbbba.
    //   ..aabbbbbbbbaa..
    //   ...aaaaaaaaaa...
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1100, 0x0011, 0x0000,
        0x0000, 0x2211, 0x1122, 0x0000,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x2100, 0x2882, 0x2222, 0x0012,
        0x2100, 0x2288, 0x2222, 0x0012,
        0x2210, 0x2322, 0x2232, 0x0122,
        0x2210, 0x2322, 0x2232, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x3222, 0x2223, 0x0122,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x1000, 0x1111, 0x1111, 0x0001,
    },
    // kArtSpider
    //   ................
    //   ................
    //   .a............a.
    //   .aa..........aa.
    //   ..aa...aa...aa..
    //   ...aa.abba.aa...
    //   .a..aaabbaaa..a.
    //   ..aaaabbbbaaaa..
    //   ...a.abccba.a...
    //   ..a..abbbbba.a..
    //   .a....abbba..a..
    //   .a...aabbbaa..a.
    //   .a..a..aaa..a.a.
    //   ....a......a....
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0010, 0x0000, 0x0000, 0x0100,
        0x0110, 0x0000, 0x0000, 0x0110,
        0x1100, 0x1000, 0x0001, 0x0011,
        0x1000, 0x2101, 0x1012, 0x0001,
        0x0010, 0x2111, 0x1112, 0x0100,
        0x1100, 0x2211, 0x1122, 0x0011,
        0x1000, 0x3210, 0x0123, 0x0001,
        0x0100, 0x2210, 0x1222, 0x0010,
        0x0010, 0x2100, 0x0122, 0x0010,
        0x0010, 0x2110, 0x1122, 0x0100,
        0x0010, 0x1001, 0x0011, 0x0101,
        0x0000, 0x0001, 0x1000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtGhost
    //   ................
    //   .....aaaaaa.....
    //   ....aabbbbaa....
    //   ...abbbbbbbba...
    //   ...abccbbccba...
    //   ...abccbbccba...
    //   ...abbbbbbbba...
    //   ...abbbccbbba...
    //   ...abbbccbbba...
    //   ..aabbbbbbbbaa..
    //   ..abbbbbbbbbba..
    //   ..abbbbbbbbbba..
    //   ..abbabbbbabba..
    //   ..aba.abba.aba..
    //   ..aa...aa...aa..
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x2211, 0x1122, 0x0000,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1000, 0x2332, 0x2332, 0x0001,
        0x1000, 0x2332, 0x2332, 0x0001,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1000, 0x3222, 0x2223, 0x0001,
        0x1000, 0x3222, 0x2223, 0x0001,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x2100, 0x2222, 0x2222, 0x0012,
        0x2100, 0x2222, 0x2222, 0x0012,
        0x2100, 0x2212, 0x2122, 0x0012,
        0x2100, 0x2101, 0x1012, 0x0012,
        0x1100, 0x1000, 0x0001, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtGhostFade
    //   ................
    //   .....a.a.a......
    //   ....a.b.b.a.....
    //   ...a.b.b.b.a....
    //   ....bcc.bcc.a...
    //   ...a.cc.bcc.b...
    //   ....b.b.b.b.a...
    //   ...a.b.ccb.b....
    //   ....b.b.ccb.a...
    //   ..a.b.b.b.b.b.a.
    //   ...b.b.b.b.b.b..
    //   ..a.b.b.b.b.b.a.
    //   ...b.a.b.b.a.b..
    //   ..a.a.a.ba.a.a..
    //   ...a...a.a...a..
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1010, 0x0010, 0x0000,
        0x0000, 0x0201, 0x0102, 0x0000,
        0x1000, 0x2020, 0x1020, 0x0000,
        0x0000, 0x0332, 0x0332, 0x0001,
        0x1000, 0x0330, 0x0332, 0x0002,
        0x0000, 0x0202, 0x0202, 0x0001,
        0x1000, 0x3020, 0x2023, 0x0000,
        0x0000, 0x0202, 0x0233, 0x0001,
        0x0100, 0x0202, 0x0202, 0x0102,
        0x2000, 0x2020, 0x2020, 0x0020,
        0x0100, 0x0202, 0x0202, 0x0102,
        0x2000, 0x2010, 0x1020, 0x0020,
        0x0100, 0x0101, 0x1012, 0x0010,
        0x1000, 0x1000, 0x0010, 0x0010,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtImp
    //   ................
    //   .......ff.......
    //   ......ff........
    //   .....aaaa.......
    //   ...a.bbbb.a.....
    //   ..aa.bbbb.aa....
    //   ..a.bbbbbb.a....
    //   ...bbdbbdbb.....
    //   ...bbbbbbbb.....
    //   ...bddddddb.....
    //   ..bbbbbbbbbb....
    //   .abbbcccbbbba...
    //   .a.bbbcccbbb.a..
    //   ...bbbbbbbb.....
    //   ...bb....bb.....
    //   ..aaa....aaa....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x6000, 0x0006, 0x0000,
        0x0000, 0x6600, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0001, 0x0000,
        0x1000, 0x2220, 0x0102, 0x0000,
        0x1100, 0x2220, 0x1102, 0x0000,
        0x0100, 0x2222, 0x1022, 0x0000,
        0x2000, 0x2242, 0x0224, 0x0000,
        0x2000, 0x2222, 0x0222, 0x0000,
        0x2000, 0x4444, 0x0244, 0x0000,
        0x2200, 0x2222, 0x2222, 0x0000,
        0x2210, 0x3332, 0x2222, 0x0001,
        0x2010, 0x3322, 0x2223, 0x0010,
        0x2000, 0x2222, 0x0222, 0x0000,
        0x2000, 0x0002, 0x0220, 0x0000,
        0x1100, 0x0001, 0x1110, 0x0000,
    },
    // kArtEyeA
    //   .h..h..h..h..h..
    //   .h..h..h..h..h..
    //   ..f..f.hh.f..f..
    //   ...f..f..f..f...
    //   ....ffccccff....
    //   ...ccccccccc....
    //   ..cccgggggccc...
    //   .cccgggddgggccc.
    //   .cccgddaaddgccc.
    //   .cccgddaaddgccc.
    //   .cccgggddgggccc.
    //   ..cccgggggccc...
    //   ...ccccccccc....
    //   ....cebebec.....
    //   .....cccccc.....
    //   ......ffff......
    {
        0x0080, 0x8008, 0x0800, 0x0080,
        0x0080, 0x8008, 0x0800, 0x0080,
        0x0600, 0x8060, 0x0608, 0x0060,
        0x6000, 0x0600, 0x0060, 0x0006,
        0x0000, 0x3366, 0x6633, 0x0000,
        0x3000, 0x3333, 0x3333, 0x0000,
        0x3300, 0x7773, 0x3377, 0x0003,
        0x3330, 0x4777, 0x7774, 0x0333,
        0x3330, 0x1447, 0x7441, 0x0333,
        0x3330, 0x1447, 0x7441, 0x0333,
        0x3330, 0x4777, 0x7774, 0x0333,
        0x3300, 0x7773, 0x3377, 0x0003,
        0x3000, 0x3333, 0x3333, 0x0000,
        0x0000, 0x5253, 0x0352, 0x0000,
        0x0000, 0x3330, 0x0333, 0x0000,
        0x0000, 0x6600, 0x0066, 0x0000,
    },
    // kArtEyeB
    //   h..h..h..h..h..h
    //   h..h..h..h..h..h
    //   .f..f..hh..f..f.
    //   ..f..f.hh.f..f..
    //   ....ffccccff....
    //   ...ccccccccc....
    //   ..cccgggggccc...
    //   .cccgggddgggccc.
    //   .cccgdaaaaaggcc.
    //   .cccgdaaaaaggcc.
    //   .cccgggddgggccc.
    //   ..cccgggggccc...
    //   ...ccccccccc....
    //   ....cebebec.....
    //   .....cccccc.....
    //   .....ffffff.....
    {
        0x8008, 0x0800, 0x0080, 0x8008,
        0x8008, 0x0800, 0x0080, 0x8008,
        0x0060, 0x8006, 0x6008, 0x0600,
        0x0600, 0x8060, 0x0608, 0x0060,
        0x0000, 0x3366, 0x6633, 0x0000,
        0x3000, 0x3333, 0x3333, 0x0000,
        0x3300, 0x7773, 0x3377, 0x0003,
        0x3330, 0x4777, 0x7774, 0x0333,
        0x3330, 0x1147, 0x7111, 0x0337,
        0x3330, 0x1147, 0x7111, 0x0337,
        0x3330, 0x4777, 0x7774, 0x0333,
        0x3300, 0x7773, 0x3377, 0x0003,
        0x3000, 0x3333, 0x3333, 0x0000,
        0x0000, 0x5253, 0x0352, 0x0000,
        0x0000, 0x3330, 0x0333, 0x0000,
        0x0000, 0x6660, 0x0666, 0x0000,
    },
    // kArtSlimeKingA
    //   ................
    //   ...g.g.g.g.g....
    //   ...gggggggggg...
    //   ....aaaaaaaa....
    //   ...abbbbbbbba...
    //   ..abbhhbbbbbba..
    //   ..abhhbbbbbbba..
    //   .abbbccbbccbbba.
    //   .abbbccbbccbbba.
    //   .abbbbbbbbbbbba.
    //   .abbbbbcccbbbba.
    //   .abbbbbbbbbbbba.
    //   ..abbbbbbbbbba..
    //   ..aabbbbbbbbaa..
    //   ...aaaaaaaaaa...
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x7000, 0x7070, 0x7070, 0x0000,
        0x7000, 0x7777, 0x7777, 0x0007,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x2100, 0x2882, 0x2222, 0x0012,
        0x2100, 0x2288, 0x2222, 0x0012,
        0x2210, 0x2332, 0x2332, 0x0122,
        0x2210, 0x2332, 0x2332, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x3222, 0x2233, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2100, 0x2222, 0x2222, 0x0012,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x1000, 0x1111, 0x1111, 0x0001,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtSlimeKingB
    //   ................
    //   ................
    //   ...g.g.g.g.g....
    //   ...gggggggggg...
    //   .....aaaaaa.....
    //   ...aabbbbbbaa...
    //   ..abbhhbbbbbba..
    //   .abbhhbbbbbbbba.
    //   abbbbccbbccbbbba
    //   abbbbccbbccbbbba
    //   abbbbbbbbbbbbbba
    //   abbbbbbcccbbbbba
    //   .abbbbbbbbbbbba.
    //   .aabbbbbbbbbbaa.
    //   ..aaaaaaaaaaaa..
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x7000, 0x7070, 0x7070, 0x0000,
        0x7000, 0x7777, 0x7777, 0x0007,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x1000, 0x2221, 0x1222, 0x0001,
        0x2100, 0x2882, 0x2222, 0x0012,
        0x2210, 0x2288, 0x2222, 0x0122,
        0x2221, 0x2332, 0x2332, 0x1222,
        0x2221, 0x2332, 0x2332, 0x1222,
        0x2221, 0x2222, 0x2222, 0x1222,
        0x2221, 0x3222, 0x2233, 0x1222,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2110, 0x2222, 0x2222, 0x0112,
        0x1100, 0x1111, 0x1111, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtGolemA
    //   ................
    //   ....dddddddd....
    //   ...daaaaaaaad...
    //   ...dabbbbbbad...
    //   ...dabeebbeaad..
    //   ...dabbbbbbad...
    //   .dddaabbbbaaddd.
    //   daaadaaccaadaaad
    //   dabbdabccbadbbad
    //   dabbdabccbadbbad
    //   daaadaabbaadaaad
    //   .ddd.daaaad.ddd.
    //   .....dbbbbd.....
    //   ....dbbddbbd....
    //   ....daad.daad...
    //   ....dddd.dddd...
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x4000, 0x1111, 0x1111, 0x0004,
        0x4000, 0x2221, 0x1222, 0x0004,
        0x4000, 0x5521, 0x1522, 0x0041,
        0x4000, 0x2221, 0x1222, 0x0004,
        0x4440, 0x2211, 0x1122, 0x0444,
        0x1114, 0x3114, 0x4113, 0x4111,
        0x2214, 0x3214, 0x4123, 0x4122,
        0x2214, 0x3214, 0x4123, 0x4122,
        0x1114, 0x2114, 0x4112, 0x4111,
        0x4440, 0x1140, 0x0411, 0x0444,
        0x0000, 0x2240, 0x0422, 0x0000,
        0x0000, 0x4224, 0x4224, 0x0000,
        0x0000, 0x4114, 0x1140, 0x0004,
        0x0000, 0x4444, 0x4440, 0x0004,
    },
    // kArtGolemB
    //   ................
    //   ................
    //   ....dddddddd....
    //   ...daaaaaaaad...
    //   ...dabbbbbbad...
    //   ...dabeebbead...
    //   .dddaabbbbaaddd.
    //   daaadaaccaadaaad
    //   dabbdabccbadbbad
    //   dabbdabccbadbbad
    //   daaadaabbaadaaad
    //   .ddd.daaaad.ddd.
    //   .....dbbbbd.....
    //   ....dbbddbbd....
    //   ...daad...daad..
    //   ...dddd...dddd..
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x4000, 0x1111, 0x1111, 0x0004,
        0x4000, 0x2221, 0x1222, 0x0004,
        0x4000, 0x5521, 0x1522, 0x0004,
        0x4440, 0x2211, 0x1122, 0x0444,
        0x1114, 0x3114, 0x4113, 0x4111,
        0x2214, 0x3214, 0x4123, 0x4122,
        0x2214, 0x3214, 0x4123, 0x4122,
        0x1114, 0x2114, 0x4112, 0x4111,
        0x4440, 0x1140, 0x0411, 0x0444,
        0x0000, 0x2240, 0x0422, 0x0000,
        0x0000, 0x4224, 0x4224, 0x0000,
        0x4000, 0x0411, 0x1400, 0x0041,
        0x4000, 0x0444, 0x4400, 0x0044,
    },
    // kArtVampA
    //   ................
    //   .....eeeeee.....
    //   ....eaaaaaae....
    //   ....eabbbbae....
    //   ....ebebbebe....
    //   ....ebbbbbbe....
    //   ....ebbffbbe....
    //   ..eeccebbeccee..
    //   .ecccccddcccccce
    //   .eccdcccddcccdce
    //   .ecccdccddccdcce
    //   ..eccccddccccce.
    //   ..ecccceecccce..
    //   ...ecccceeccce..
    //   ....ecc..ccee...
    //   .....ee..ee.....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x5550, 0x0555, 0x0000,
        0x0000, 0x1115, 0x5111, 0x0000,
        0x0000, 0x2215, 0x5122, 0x0000,
        0x0000, 0x2525, 0x5252, 0x0000,
        0x0000, 0x2225, 0x5222, 0x0000,
        0x0000, 0x6225, 0x5226, 0x0000,
        0x5500, 0x2533, 0x3352, 0x0055,
        0x3350, 0x4333, 0x3334, 0x5333,
        0x3350, 0x3334, 0x3344, 0x5343,
        0x3350, 0x3343, 0x3344, 0x5334,
        0x3500, 0x4333, 0x3334, 0x0533,
        0x3500, 0x5333, 0x3335, 0x0053,
        0x5000, 0x3333, 0x3355, 0x0053,
        0x0000, 0x0335, 0x5330, 0x0005,
        0x0000, 0x0550, 0x0550, 0x0000,
    },
    // kArtVampB
    //   ................
    //   ................
    //   .....eeeeee.....
    //   ....eaaaaaae....
    //   ....eabbbbae....
    //   ....ebebbebe....
    //   ....ebbbbbbe....
    //   ...eebbffbbee...
    //   ..ecceebbeecce..
    //   .eccccddddcccce.
    //   ecccdcddddcdccce
    //   eccdccddddccdcce
    //   .ecccceddeccce..
    //   ..eccceddeccce..
    //   ...eccee.eccee..
    //   ....ee....ee....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x5550, 0x0555, 0x0000,
        0x0000, 0x1115, 0x5111, 0x0000,
        0x0000, 0x2215, 0x5122, 0x0000,
        0x0000, 0x2525, 0x5252, 0x0000,
        0x0000, 0x2225, 0x5222, 0x0000,
        0x5000, 0x6225, 0x5226, 0x0005,
        0x3500, 0x2553, 0x3552, 0x0053,
        0x3350, 0x4433, 0x3344, 0x0533,
        0x3335, 0x4434, 0x4344, 0x5333,
        0x4335, 0x4433, 0x3344, 0x5334,
        0x3350, 0x4533, 0x3354, 0x0053,
        0x3500, 0x4533, 0x3354, 0x0053,
        0x5000, 0x5533, 0x3350, 0x0055,
        0x0000, 0x0055, 0x5500, 0x0000,
    },
    // kArtLichLordB
    //   ..............c.
    //   .............c.e
    //   ..c..aaaaaa.ccee
    //   ...caaccccaac.ef
    //   ....acdccdcca..f
    //   ....aacccccaa..f
    //   ....aaacccaaa..f
    //   ...aaaaaaaaaaacf
    //   ..aabaaaaaabaa.f
    //   ..aabaaaaaabaa.f
    //   .caabaaaaaabaa..
    //   ..aabaaaaaabaa..
    //   ..aaabaaaaabaaa.
    //   ..aaabaaaabaaaa.
    //   .aaaabaaaabaaaa.
    //   a.aa.aa.aa.aa.a.
    {
        0x0000, 0x0000, 0x0000, 0x0300,
        0x0000, 0x0000, 0x0000, 0x5030,
        0x0300, 0x1110, 0x0111, 0x5533,
        0x3000, 0x3311, 0x1133, 0x6503,
        0x0000, 0x3431, 0x3343, 0x6001,
        0x0000, 0x3311, 0x1333, 0x6001,
        0x0000, 0x3111, 0x1133, 0x6001,
        0x1000, 0x1111, 0x1111, 0x6311,
        0x1100, 0x1112, 0x2111, 0x6011,
        0x1100, 0x1112, 0x2111, 0x6011,
        0x1130, 0x1112, 0x2111, 0x0011,
        0x1100, 0x1112, 0x2111, 0x0011,
        0x1100, 0x1121, 0x2111, 0x0111,
        0x1100, 0x1121, 0x1211, 0x0111,
        0x1110, 0x1121, 0x1211, 0x0111,
        0x1101, 0x0110, 0x1011, 0x0101,
    },
    // kArtChest
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ....aaaaaaaa....
    //   ..aabbbbbbbbaa..
    //   .abbbbbbbbbbbba.
    //   .abbddddddddbba.
    //   .aaaaaaccaaaaaa.
    //   .abbbbbcacbbbba.
    //   .abbbbbbbbbbbba.
    //   .abbbbbbbbbbbba.
    //   .adddddddddddda.
    //   ..aaaaaaaaaaaa..
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x4444, 0x4444, 0x0122,
        0x1110, 0x3111, 0x1113, 0x0111,
        0x2210, 0x3222, 0x2231, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x4410, 0x4444, 0x4444, 0x0144,
        0x1100, 0x1111, 0x1111, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtMimic
    //   ................
    //   ................
    //   ................
    //   ....aaaaaaaa....
    //   ..aabbbbbbbbaa..
    //   .abbbbbbbbbbbba.
    //   .abbddddddddbba.
    //   .aaaaaaaaaaaaaa.
    //   .aeeaeeaeeaeeea.
    //   .aeeaeeaeeaeeea.
    //   .aaaeaeaeaeaaaa.
    //   ..aacccccccaa...
    //   .abbccccccccbba.
    //   .adddddddddddda.
    //   ..aaaaaaaaaaaa..
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x2210, 0x4444, 0x4444, 0x0122,
        0x1110, 0x1111, 0x1111, 0x0111,
        0x5510, 0x1551, 0x5155, 0x0155,
        0x5510, 0x1551, 0x5155, 0x0155,
        0x1110, 0x1515, 0x1515, 0x0111,
        0x1100, 0x3333, 0x1333, 0x0001,
        0x2210, 0x3333, 0x3333, 0x0122,
        0x4410, 0x4444, 0x4444, 0x0144,
        0x1100, 0x1111, 0x1111, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtOrb
    //   ................
    //   ................
    //   ................
    //   ................
    //   .....aaaaaa.....
    //   ....aabbbbaa....
    //   ...aabccccbaa...
    //   ...abbcddcbba...
    //   ...abbcddcbba...
    //   ...aabccccbaa...
    //   ....aabbbbaa....
    //   .....aaaaaa.....
    //   ................
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x2211, 0x1122, 0x0000,
        0x1000, 0x3321, 0x1233, 0x0001,
        0x1000, 0x4322, 0x2234, 0x0001,
        0x1000, 0x4322, 0x2234, 0x0001,
        0x1000, 0x3321, 0x1233, 0x0001,
        0x0000, 0x2211, 0x1122, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtWeb
    //   ................
    //   ................
    //   ................
    //   ....a..a..a.....
    //   .....a.a.a......
    //   ......aaa.......
    //   ..aaaaabaaaaa...
    //   ......aaa.......
    //   .....a.a.a......
    //   ....a..a..a.....
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1001, 0x0100, 0x0000,
        0x0000, 0x1010, 0x0010, 0x0000,
        0x0000, 0x1100, 0x0001, 0x0000,
        0x1100, 0x2111, 0x1111, 0x0001,
        0x0000, 0x1100, 0x0001, 0x0000,
        0x0000, 0x1010, 0x0010, 0x0000,
        0x0000, 0x1001, 0x0100, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtPuddle
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ....aaaaaaaa....
    //   ..aabbbbbbbbaa..
    //   .abbbbhhbbbbbba.
    //   .abbbbbbbbbbbba.
    //   ..aabbbbbbbbaa..
    //   ....aaaaaaaa....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x2210, 0x8822, 0x2222, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x1100, 0x2222, 0x2222, 0x0011,
        0x0000, 0x1111, 0x1111, 0x0000,
    },
    // kArtWarn
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ....aaaaaaaa....
    //   ..aa........aa..
    //   .a............a.
    //   .a............a.
    //   ..aa........aa..
    //   ....aaaaaaaa....
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1100, 0x0000, 0x0000, 0x0011,
        0x0010, 0x0000, 0x0000, 0x0100,
        0x0010, 0x0000, 0x0000, 0x0100,
        0x1100, 0x0000, 0x0000, 0x0011,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtWave
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ...aaaaaaaaaa...
    //   .aabbbbbbbbbbaa.
    //   abb..........bba
    //   abb..........bba
    //   .aabbbbbbbbbbaa.
    //   ...aaaaaaaaaa...
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x1000, 0x1111, 0x1111, 0x0001,
        0x2110, 0x2222, 0x2222, 0x0112,
        0x0221, 0x0000, 0x0000, 0x1220,
        0x0221, 0x0000, 0x0000, 0x1220,
        0x2110, 0x2222, 0x2222, 0x0112,
        0x1000, 0x1111, 0x1111, 0x0001,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtAlert
    //   ................
    //   .......aa.......
    //   ......abba......
    //   ......abba......
    //   ......abba......
    //   ......abba......
    //   ......abba......
    //   .......bb.......
    //   .......bb.......
    //   ................
    //   .......aa.......
    //   ......abba......
    //   .......aa.......
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x2000, 0x0002, 0x0000,
        0x0000, 0x2000, 0x0002, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtMerchant
    //   ................
    //   .....aaaaaa.....
    //   ....abbbbbba....
    //   ...abbbbbbbba...
    //   ...abbddddbba...
    //   ...abdeddeddba..
    //   ...abdddddddba..
    //   ...abbdffdbbba..
    //   ..aaabbbbbbaaa..
    //   .aggaccccccaggaa
    //   .agghaccccccgga.
    //   .aggcccffcccgga.
    //   .agacccccccccaa.
    //   ..aaccccccccaa..
    //   ...acc.aa.cca...
    //   ...aaa.aa.aaa...
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1000, 0x4422, 0x2244, 0x0001,
        0x1000, 0x4542, 0x4454, 0x0012,
        0x1000, 0x4442, 0x4444, 0x0012,
        0x1000, 0x6422, 0x2246, 0x0012,
        0x1100, 0x2221, 0x1222, 0x0011,
        0x7710, 0x3331, 0x1333, 0x1177,
        0x7710, 0x3318, 0x3333, 0x0177,
        0x7710, 0x6333, 0x3336, 0x0177,
        0x1710, 0x3333, 0x3333, 0x0113,
        0x1100, 0x3333, 0x3333, 0x0011,
        0x1000, 0x1033, 0x3301, 0x0001,
        0x1000, 0x1011, 0x1101, 0x0001,
    },
    // kArtArcher
    //   ................
    //   .....dddddd.....
    //   ....dddddddd....
    //   ....dbbddbbd.c..
    //   ....dddaaddd.ce.
    //   .....dddddd..c.e
    //   ......dadad..c.e
    //   .dd.dddddddddc.e
    //   ..d..daddad..c.e
    //   ..d..dddddd..c.e
    //   ..d..daddad..c.e
    //   .....dddddd..ce.
    //   .....dddddd..c..
    //   .....dd..dd.....
    //   .....dd..dd.....
    //   ....ddd..ddd....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4440, 0x0444, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x0000, 0x4224, 0x4224, 0x0030,
        0x0000, 0x1444, 0x4441, 0x0530,
        0x0000, 0x4440, 0x0444, 0x5030,
        0x0000, 0x1400, 0x0414, 0x5030,
        0x0440, 0x4444, 0x4444, 0x5034,
        0x0400, 0x4140, 0x0414, 0x5030,
        0x0400, 0x4440, 0x0444, 0x5030,
        0x0400, 0x4140, 0x0414, 0x5030,
        0x0000, 0x4440, 0x0444, 0x0530,
        0x0000, 0x4440, 0x0444, 0x0030,
        0x0000, 0x0440, 0x0440, 0x0000,
        0x0000, 0x0440, 0x0440, 0x0000,
        0x0000, 0x0444, 0x4440, 0x0000,
    },
    // kArtKey
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ....aaaa........
    //   ...ac..ba.......
    //   ...a....a.......
    //   ...ab..ba.......
    //   ....aaaaaaaaaa..
    //   ...........a.a..
    //   ...........a.a..
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x0000, 0x0000,
        0x1000, 0x2003, 0x0001, 0x0000,
        0x1000, 0x0000, 0x0001, 0x0000,
        0x1000, 0x2002, 0x0001, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0011,
        0x0000, 0x0000, 0x1000, 0x0010,
        0x0000, 0x0000, 0x1000, 0x0010,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtHeart
    //   ................
    //   ................
    //   ................
    //   ................
    //   ...aaa...aaa....
    //   ..abbaa.aaaaa...
    //   ..abaaaaaaaaa...
    //   ..aaaaaaaaaaa...
    //   ..caaaaaaaaac...
    //   ...caaaaaaac....
    //   ....caaaaac.....
    //   .....caaac......
    //   ......cac.......
    //   .......c........
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x1000, 0x0011, 0x1110, 0x0000,
        0x2100, 0x0112, 0x1111, 0x0001,
        0x2100, 0x1111, 0x1111, 0x0001,
        0x1100, 0x1111, 0x1111, 0x0001,
        0x1300, 0x1111, 0x1111, 0x0003,
        0x3000, 0x1111, 0x3111, 0x0000,
        0x0000, 0x1113, 0x0311, 0x0000,
        0x0000, 0x1130, 0x0031, 0x0000,
        0x0000, 0x1300, 0x0003, 0x0000,
        0x0000, 0x3000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
};
      // list ends with ch = 0

static const PalEnt kPalGoblin[] =
    { {'d',COLOR_OLIVE},{'b',COLOR_DARK_GRAY},{'c',COLOR_GOLD},{'h',COLOR_WHITE},{'f',COLOR_TETRIS_GRID},
      {'e',COLOR_DARK_GRAY},{'a',COLOR_TETRIS_GRID},{'g',COLOR_LIGHT_GRAY},{0,0} };

static const PalEnt kPalBat[]    =
    { {'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_DARK_PURPLE},{'d',COLOR_WHITE},{0,0} };

static const PalEnt kPalSkel[]   =
    { {'d',COLOR_LIGHT_GRAY},{'a',COLOR_GRAY},{'b',COLOR_CRIMSON},{'c',COLOR_SILVER},{0,0} };

static const PalEnt kPalLich[]   =                     // Lich Lord
    { {'a',0x00141018UL},{'b',0x003A3644UL},{'c',0x00E6E0CCUL},{'d',0x0040FF60UL},
      {'e',0x0020C040UL},{'f',COLOR_GRAY},{0,0} };

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


static const PalEnt kPalDragon[] =
    { {'a',COLOR_DARK_PURPLE},{'b',COLOR_PURPLE},{'c',COLOR_CRIMSON},{'d',COLOR_GOLD},
      {'e',COLOR_TETRIS_L_ORANGE},{0,0} };

static const PalEnt kPalDragonHot[] =
    { {'a',COLOR_DARK_PURPLE},{'b',COLOR_CRIMSON},{'c',COLOR_TETRIS_L_ORANGE},{'d',COLOR_WHITE},
      {'e',COLOR_GOLD},{0,0} };


const PalEnt *const rc_kPal[PT_COUNT] =
{
    kPalGoblin, kPalBat, kP_HOBGOB, kPalSkel, kPalBeholder, kPalLich, kPalBeholderHot,
    kPalFlaskFire, kPalFlaskFrost, kPalFlaskVenom, kPalFlaskMana, kPalBolt,
    kPalPuffFire, kPalPuffFrost, kPalPuffVenom, kPalPuffArcane,
    kPalDragon, kPalDragonHot,
    kP_SLIME, kP_SLIME_S, kP_SPIDER, kP_GHOST,
    kP_GHOST_FADE, kP_IMP, kP_IMP_LIT,
    kP_EYEKING, kP_EYEKING_FIRE, kP_EYEKING_ICE, kP_EYEKING_BOLT,
    kP_EYEKING_GRAV, kP_EYEKING_NULL, kP_SLIMEKING,
    kP_GOLEM_FIRE, kP_GOLEM_ICE, kP_GOLEM_STORM, kP_VAMP,
    kP_CHEST, kP_CHEST_CURSED, kP_MIMIC, kP_ORB_PURPLE,
    kP_ORB_GREEN, kP_ORB_BLOOD, kP_ORB_FIRE, kP_ORB_ICE,
    kP_ORB_BOLT, kP_WEB, kP_PUDDLE_SLIME, kP_PUDDLE_FIRE,
    kP_PUDDLE_WEB, kP_PUDDLE_BLAST, kP_WARN, kP_WAVE_SLIME,
    kP_WAVE_FIRE, kP_WAVE_ICE, kP_WAVE_STORM, kP_WAVE_NOVA,
    kP_ALERT,
    kP_MERCHANT, kP_ARCHER, kP_KEY, kP_HEART, kP_ALTAR, kP_CHEST_GOOD
};


// Sprite tint: 0 none, 1 white (hit), 2 orange (burning), 3 ice blue (frozen)
const uint32_t rc_kTint[4] = { 0, COLOR_WHITE, COLOR_TETRIS_L_ORANGE, COLOR_SKY_BLUE };


const uint32_t rc_kWallBase[TILE_KINDS] =
    { 0, COLOR_GRAY, COLOR_GRAY, COLOR_BROWN, COLOR_ORANGE, COLOR_TURQUOISE, COLOR_SKY_BLUE, COLOR_GRAY };

const uint16_t rc_kShadeMul[4] = { 256, 190, 130, 84 };

// Wall themes: base stone, mortar / seams, accent, floor, ceiling, wall pattern.
// Chambers have fixed themes (rc.md 4.6); a mini-boss or the Dragon overrides them while alive.
const Theme rc_kTheme[TH_COUNT] =
{
    //   base        mortar      accent      floor       ceiling      pattern
    { 0x008E939BUL, 0x004D525AUL, 0x007C9468UL, 0x003B3128UL, 0x0015171DUL, PAT_NONE   },  // corridors: grey stone, moss
    { 0x004E9448UL, 0x00234A20UL, 0x00A6E070UL, 0x002A4326UL, 0x000E1A0EUL, PAT_DRIPS  },  // Slime King
    { 0x008A8A94UL, 0x0045454CUL, 0x00FF8A2EUL, 0x0034343AUL, 0x0016161CUL, PAT_CRACKS },  // Elemental Golem
    { 0x008C2A38UL, 0x003E1018UL, 0x00D84A5AUL, 0x003A161CUL, 0x00160609UL, PAT_VEINS  },  // Vampire Lord
    { 0x006E3F96UL, 0x00331B50UL, 0x00F0E680UL, 0x002F1B42UL, 0x00100A1CUL, PAT_EYES   },  // Beholder
    { 0x002E3A34UL, 0x00101814UL, 0x0040E070UL, 0x00182019UL, 0x00080C0AUL, PAT_BANDS  },  // Lich Lord: dark crypt, green glow
    { 0x006A3A28UL, 0x002A140CUL, 0x00FF7A20UL, 0x003A1A0CUL, 0x00140806UL, PAT_LAVA   },  // Dragon Den: basalt, lava
    { 0x00806A50UL, 0x00403020UL, 0x009A8A3AUL, 0x003A2A1AUL, 0x00140E08UL, PAT_NONE   },  // Goblin Warrens: earth and timber
    { 0x009A9690UL, 0x00504C46UL, 0x00E2DCC8UL, 0x00302C28UL, 0x00100E0CUL, PAT_BANDS  },  // Bone Crypt: pale stone, bone
    { 0x005A6E58UL, 0x00283428UL, 0x0088C060UL, 0x00263024UL, 0x000C120CUL, PAT_DRIPS  },  // Slime Sewers: wet green stone
    { 0x007A7088UL, 0x00383040UL, 0x00B0A0D8UL, 0x002C2832UL, 0x000E0C12UL, PAT_NONE   },  // Haunted Chapel: cold violet stone
    { 0x00705048UL, 0x00301C18UL, 0x00FF9030UL, 0x00302018UL, 0x00140A06UL, PAT_CRACKS },  // Elemental Forge: soot, embers
    { 0x00485890UL, 0x00202848UL, 0x00E0E8FFUL, 0x001C2238UL, 0x00080A16UL, PAT_EYES   },  // Arcane Observatory: night blue, stars
};

// Pillar banner cloth colour per theme (drawn on the pillars, only up close).
const uint32_t rc_kBannerRgb[TH_COUNT] =
{
    0x00B02828UL,       // corridors: red
    0x00D8B840UL,       // Slime King: gold
    0x002850A0UL,       // Elemental Golem: royal blue
    0x00C8A030UL,       // Vampire Lord: gold
    0x0030A090UL,       // Beholder: teal
    0x0030C050UL,       // Lich Lord: green
    0x00A81818UL,       // Dragon Den: crimson
    0x00A05020UL,       // Goblin Warrens: rust
    0x00402850UL,       // Bone Crypt: purple
    0x00C0A030UL,       // Slime Sewers: gold
    0x00802030UL,       // Haunted Chapel: wine
    0x002040A0UL,       // Elemental Forge: blue
    0x00C8B040UL,       // Arcane Observatory: gold
};

const char rc_kWpnArm[] =
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


const char *const rc_kWpnArt[NUM_SLOT] =
{
    kArtRay, kArtPrism, kArtStorm, kArtSun,
    kArtFire, kArtFrost, kArtVenom, kArtElixir, kArtHaste,
    kArtTome, kArtWarp, kArtAegis
};


static const PalEnt kWpnPalRay[]   = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_TURQUOISE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_GOLD,COLOR_CRIMSON);

static const PalEnt kWpnPalPrism[] = WPAL(COLOR_TETRIS_GRID,COLOR_DARK_PURPLE,COLOR_DARK_GRAY,COLOR_CORAL,COLOR_OLIVE,COLOR_VIOLET,COLOR_WHITE,COLOR_TURQUOISE,COLOR_KHAKI,COLOR_CORAL);


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


const PalEnt *const rc_kWpnPal[NUM_SLOT] =
{
    kWpnPalRay, kWpnPalPrism, kWpnPalStorm, kWpnPalSun,
    kWpnPalFire, kWpnPalFrost, kWpnPalVenom, kWpnPalElixir, kWpnPalHaste,
    kWpnPalTome, kWpnPalWarp, kWpnPalAegis
};
