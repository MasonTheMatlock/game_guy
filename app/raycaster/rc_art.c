// rc_art.c - sprite art, palettes, wall themes and the first-person item art.
#include "rc_priv.h"












static const uint32_t kP_HOBGOB[PAL_N] = { COLOR_TETRIS_GRID, COLOR_BROWN, COLOR_GRAY, COLOR_DARK_GRAY, COLOR_LIGHT_GRAY, COLOR_CRIMSON, COLOR_MAGENTA, COLOR_WHITE };
static const uint32_t kP_SLIME[PAL_N] = { COLOR_TEAL, COLOR_GREEN, COLOR_TETRIS_GRID, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MINT };
static const uint32_t kP_SLIME_S[PAL_N] = { COLOR_OLIVE, COLOR_MINT, COLOR_TETRIS_GRID, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_WHITE };
static const uint32_t kP_SPIDER[PAL_N] = { 0x00302838UL, 0x00503060UL, COLOR_RED, 0x00905CA8UL, COLOR_WHITE, COLOR_ORANGE, COLOR_MAGENTA, COLOR_MAGENTA };   // legs, body, eyes, sheen, fangs, knee bands
static const uint32_t kP_GHOST[PAL_N] = { COLOR_SKY_BLUE, COLOR_WHITE, COLOR_NAVY, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_GHOST_FADE[PAL_N] = { COLOR_TEAL, COLOR_CYAN, COLOR_NAVY, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_IMP[PAL_N] = { COLOR_MAROON, COLOR_CRIMSON, COLOR_ORANGE, COLOR_BLACK, COLOR_MAGENTA, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_IMP_LIT[PAL_N] = { COLOR_MAROON, COLOR_TETRIS_L_ORANGE, COLOR_GOLD, COLOR_BLACK, COLOR_MAGENTA, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_EYEKING[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_MAROON, COLOR_CRIMSON, COLOR_WHITE, COLOR_BROWN, COLOR_WHITE, COLOR_CRIMSON };
static const uint32_t kP_EYEKING_FIRE[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_MAROON, COLOR_TETRIS_L_ORANGE, COLOR_WHITE, COLOR_BROWN, COLOR_WHITE, COLOR_CRIMSON };
static const uint32_t kP_EYEKING_ICE[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_MAROON, COLOR_SKY_BLUE, COLOR_WHITE, COLOR_BROWN, COLOR_WHITE, COLOR_CRIMSON };
static const uint32_t kP_EYEKING_BOLT[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_MAROON, COLOR_GOLD, COLOR_WHITE, COLOR_BROWN, COLOR_WHITE, COLOR_CRIMSON };
static const uint32_t kP_EYEKING_GRAV[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_MAROON, COLOR_VIOLET, COLOR_WHITE, COLOR_BROWN, COLOR_WHITE, COLOR_CRIMSON };
static const uint32_t kP_EYEKING_NULL[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_MAROON, COLOR_GRAY, COLOR_WHITE, COLOR_BROWN, COLOR_WHITE, COLOR_CRIMSON };
static const uint32_t kP_SLIMEKING[PAL_N] = { COLOR_TEAL, COLOR_GREEN, COLOR_TETRIS_GRID, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_GOLD, COLOR_MINT };
static const uint32_t kP_GOLEM_FIRE[PAL_N] = { COLOR_MAROON, 0x00B0401CUL, COLOR_TETRIS_L_ORANGE, COLOR_TETRIS_GRID, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_GOLEM_ICE[PAL_N] = { COLOR_TEAL, 0x0060B0C8UL, COLOR_WHITE, COLOR_TETRIS_GRID, COLOR_SKY_BLUE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_GOLEM_STORM[PAL_N] = { 0x006A5A10UL, COLOR_GOLD, COLOR_WHITE, COLOR_TETRIS_GRID, COLOR_KHAKI, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_VAMP[PAL_N] = { COLOR_TETRIS_GRID, COLOR_LIGHT_GRAY, COLOR_MAROON, COLOR_CRIMSON, COLOR_BLACK, COLOR_CRIMSON, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_CHEST[PAL_N] = { COLOR_BLACK, COLOR_BROWN, COLOR_KHAKI, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_CHEST_CURSED[PAL_N] = { COLOR_BLACK, COLOR_DARK_PURPLE, COLOR_CRIMSON, COLOR_VIOLET, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_MIMIC[PAL_N] = { COLOR_BLACK, COLOR_BROWN, COLOR_CRIMSON, COLOR_GOLD, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ORB_PURPLE[PAL_N] = { COLOR_DARK_PURPLE, COLOR_VIOLET, COLOR_PINK, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ORB_GREEN[PAL_N] = { COLOR_TEAL, COLOR_GREEN, COLOR_MINT, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ORB_BLOOD[PAL_N] = { COLOR_MAROON, COLOR_CRIMSON, COLOR_CORAL, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ORB_FIRE[PAL_N] = { COLOR_MAROON, COLOR_TETRIS_L_ORANGE, COLOR_GOLD, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ORB_ICE[PAL_N] = { COLOR_TEAL, COLOR_SKY_BLUE, COLOR_WHITE, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ORB_BOLT[PAL_N] = { COLOR_ORANGE, COLOR_GOLD, COLOR_KHAKI, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_WEB[PAL_N] = { COLOR_LIGHT_GRAY, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_PUDDLE_SLIME[PAL_N] = { COLOR_OLIVE, COLOR_GREEN, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MINT };
static const uint32_t kP_PUDDLE_FIRE[PAL_N] = { COLOR_MAROON, COLOR_TETRIS_L_ORANGE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_GOLD };
static const uint32_t kP_PUDDLE_WEB[PAL_N] = { COLOR_GRAY, COLOR_LIGHT_GRAY, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_WHITE };
static const uint32_t kP_PUDDLE_BLAST[PAL_N] = { COLOR_BROWN, COLOR_TETRIS_L_ORANGE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_KHAKI };
static const uint32_t kP_WARN[PAL_N] = { COLOR_CRIMSON, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_WAVE_SLIME[PAL_N] = { COLOR_OLIVE, COLOR_GREEN, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MINT };
static const uint32_t kP_WAVE_FIRE[PAL_N] = { COLOR_MAROON, COLOR_TETRIS_L_ORANGE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_GOLD };
static const uint32_t kP_WAVE_ICE[PAL_N] = { COLOR_TEAL, COLOR_SKY_BLUE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_WHITE };
static const uint32_t kP_WAVE_STORM[PAL_N] = { COLOR_ORANGE, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_WHITE };
static const uint32_t kP_WAVE_NOVA[PAL_N] = { COLOR_DARK_PURPLE, COLOR_VIOLET, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_PINK };
static const uint32_t kP_ALERT[PAL_N] = { COLOR_CRIMSON, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kP_ARCHER[PAL_N] = { COLOR_GRAY, COLOR_CRIMSON, COLOR_BROWN, COLOR_LIGHT_GRAY, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_KEY[PAL_N] = { COLOR_GOLD, COLOR_BROWN, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_HEART[PAL_N] = { COLOR_CRIMSON, COLOR_PINK, COLOR_MAROON, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_ALTAR[PAL_N] = { COLOR_DARK_PURPLE, COLOR_GOLD, COLOR_KHAKI, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_CHEST_GOOD[PAL_N] = { COLOR_BLACK, COLOR_TEAL, COLOR_GOLD, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

// Elemental Golem forms (rc.md 14.6): body and glow in the form's colour.
//                                        a dark body   b body        c core / glow  d outline          e eyes / hands
static const uint32_t kP_GOLEM_EARTH[PAL_N] = { 0x004A3420UL, 0x008A6440UL, 0x00C07A30UL, COLOR_TETRIS_GRID, COLOR_KHAKI, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
// Earth rocks, ice spikes, and floor marks (letters a, b, h of the puddle shape)
static const uint32_t kP_ROCK[PAL_N]       = { 0x004A3420UL, 0x008A6440UL, COLOR_GRAY, COLOR_LIGHT_GRAY, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_SPIKE[PAL_N]      = { COLOR_WHITE, COLOR_SKY_BLUE, COLOR_TEAL, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_MARK_EARTH[PAL_N] = { 0x005A3A18UL, 0x009A6A30UL, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, 0x00C08A40UL };
static const uint32_t kP_MARK_BLUE[PAL_N]  = { COLOR_NAVY, COLOR_BLUE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_SKY_BLUE };
static const uint32_t kP_MARK_WHITE[PAL_N] = { COLOR_LIGHT_GRAY, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_SKY_BLUE };
static const uint32_t kP_MARK_GREEN[PAL_N] = { 0x00104020UL, COLOR_GREEN, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MINT };

// Vendor (a wizard clone): a outline, b robe, c -, d skin, e eyes, f gold, g staff, h beard / orb
static const uint32_t kP_MERCHANT[PAL_N] = { COLOR_NAVY, 0x002848B0UL, COLOR_MAGENTA, COLOR_KHAKI, COLOR_BLACK, COLOR_GOLD, COLOR_BROWN, COLOR_WHITE };
// Floor decorations
static const uint32_t kP_BARREL[PAL_N]   = { 0x003A3A40UL, 0x008A5A2AUL, 0x005A3A1AUL, 0x00C08A4AUL, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_BARREL_G[PAL_N] = { 0x003A3A40UL, 0x004A7A30UL, 0x002A4A1AUL, COLOR_MINT, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_BONES[PAL_N]    = { 0x00E0DCC8UL, 0x00302820UL, COLOR_BLACK, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_CRYSTAL[PAL_N]  = { 0x003A2A7AUL, 0x007A6AE0UL, 0x00C0B8FFUL, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };
static const uint32_t kP_CANDLE[PAL_N]   = { 0x00303034UL, 0x00E8E0C0UL, COLOR_ORANGE, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

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
    // kArtDragonBody
    //   ................
    //   ................
    //   ................
    //   .....aaaaaa.....
    //   ....abbbbbba....
    //   ...abbccccbba...
    //   ..abbccccccbba..
    //   ..abccceeccbba..
    //   ..abcceeeeccba..
    //   ..abbcceeccbba..
    //   ..abbbccccbbba..
    //   ...abbbbbbbba...
    //   ..aabba..abbaa..
    //   .abba.....abba..
    //   .dddd.....dddd..
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x1000, 0x3322, 0x2233, 0x0001,
        0x2100, 0x3332, 0x2333, 0x0012,
        0x2100, 0x5333, 0x2335, 0x0012,
        0x2100, 0x5533, 0x3355, 0x0012,
        0x2100, 0x5332, 0x2335, 0x0012,
        0x2100, 0x3322, 0x2233, 0x0012,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1100, 0x0122, 0x2210, 0x0011,
        0x2210, 0x0001, 0x2100, 0x0012,
        0x4440, 0x0004, 0x4400, 0x0044,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtDragonHead
    //   .d............d.
    //   .dd..........dd.
    //   ..dd..aaaa..dd..
    //   ...daabbbbaad...
    //   ....abbbbbba....
    //   ...abfbbbbfba...
    //   ...abbbbbbbba...
    //   ...abbbbbbbba...
    //   ....abbccbba....
    //   ....abccccba....
    //   .....acccca.....
    //   .....abccba.....
    //   ......abba......
    //   .......aa.......
    //   ................
    //   ................
    {
        0x0040, 0x0000, 0x0000, 0x0400,
        0x0440, 0x0000, 0x0000, 0x0440,
        0x4400, 0x1100, 0x0011, 0x0044,
        0x4000, 0x2211, 0x1122, 0x0004,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x1000, 0x2262, 0x2622, 0x0001,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x0000, 0x3221, 0x1223, 0x0000,
        0x0000, 0x3321, 0x1233, 0x0000,
        0x0000, 0x3310, 0x0133, 0x0000,
        0x0000, 0x3210, 0x0123, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
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
    //   ...f........f...
    //   ...aa......aa...
    //   ..a.f......f.a..
    //   ..a.adbbbbda.a..
    //   ..aabbddddbbaa..
    //   .a.abbbddbbba.a.
    //   .aa.abbbbbba.aa.
    //   a.aa.abccba.aa.a
    //   a.aa.acbbca.aa.a
    //   .a.a.aebbea.a.a.
    //   .aa.a.e..e.a.aa.
    //   a.a.a......a.a.a
    //   a.a.a......a.a.a
    //   .a.a........a.a.
    //   .a.a........a.a.
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x6000, 0x0000, 0x0000, 0x0006,
        0x1000, 0x0001, 0x1000, 0x0001,
        0x0100, 0x0006, 0x6000, 0x0010,
        0x0100, 0x2241, 0x1422, 0x0010,
        0x1100, 0x4422, 0x2244, 0x0011,
        0x1010, 0x4222, 0x2224, 0x0101,
        0x0110, 0x2221, 0x1222, 0x0110,
        0x1101, 0x3210, 0x0123, 0x1011,
        0x1101, 0x2310, 0x0132, 0x1011,
        0x1010, 0x2510, 0x0152, 0x0101,
        0x0110, 0x0501, 0x1050, 0x0110,
        0x0101, 0x0001, 0x1000, 0x1010,
        0x0101, 0x0001, 0x1000, 0x1010,
        0x1010, 0x0000, 0x0000, 0x0101,
        0x1010, 0x0000, 0x0000, 0x0101,
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
    //   ........a.......
    //   .......aba......
    //   ......abfba.....
    //   ......abbba.....
    //   .....abbfbba..h.
    //   .....abbbbba.hhh
    //   ...aaffffffaa.h.
    //   ....adeddeda..g.
    //   ....ahhhhhha..g.
    //   ...abhhhhhhba.g.
    //   ..abbbhhhhbbbagg
    //   ..abbbbhhbbbba.g
    //   ..abbfbbbbfbba.g
    //   ..abbbbbbbbbba.g
    //   ...abbbbbbbba..g
    //   ...aaaaaaaaaa..g
    {
        0x0000, 0x0000, 0x0001, 0x0000,
        0x0000, 0x1000, 0x0012, 0x0000,
        0x0000, 0x2100, 0x0126, 0x0000,
        0x0000, 0x2100, 0x0122, 0x0000,
        0x0000, 0x2210, 0x1226, 0x0800,
        0x0000, 0x2210, 0x1222, 0x8880,
        0x1000, 0x6661, 0x1666, 0x0801,
        0x0000, 0x4541, 0x1454, 0x0700,
        0x0000, 0x8881, 0x1888, 0x0700,
        0x1000, 0x8882, 0x2888, 0x0701,
        0x2100, 0x8822, 0x2288, 0x7712,
        0x2100, 0x8222, 0x2228, 0x7012,
        0x2100, 0x2262, 0x2622, 0x7012,
        0x2100, 0x2222, 0x2222, 0x7012,
        0x1000, 0x2222, 0x2222, 0x7001,
        0x1000, 0x1111, 0x1111, 0x7001,
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
    // kArtGolemC
    //   ................
    //   ................
    //   ................
    //   ................
    //   ....dddddddd....
    //   ...daaaaaaaad...
    //   ...dabeebbead...
    //   .dddaabbbbaaddd.
    //   daabdaaccaadbaad
    //   dabbdabccbadbbad
    //   .dddaabbbbaaddd.
    //   ...daaaaaaaad...
    //   ..dbbbd..dbbbd..
    //   .daad......daad.
    //   .dddd......dddd.
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x4000, 0x1111, 0x1111, 0x0004,
        0x4000, 0x5521, 0x1522, 0x0004,
        0x4440, 0x2211, 0x1122, 0x0444,
        0x2114, 0x3114, 0x4113, 0x4112,
        0x2214, 0x3214, 0x4123, 0x4122,
        0x4440, 0x2211, 0x1122, 0x0444,
        0x4000, 0x1111, 0x1111, 0x0004,
        0x2400, 0x0422, 0x2240, 0x0042,
        0x1140, 0x0004, 0x4000, 0x0411,
        0x4440, 0x0004, 0x4000, 0x0444,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtGolemD
    //   ................
    //   ....dddddddd....
    //   ...daaaaaaaad...
    //   ...dabeebbead...
    //   ...dabbbbbbad...
    //   dddddaabbaaddddd
    //   eaaaaaaccaaaaaae
    //   eabbbdaccadbbbae
    //   dddddaabbaaddddd
    //   .....daaaad.....
    //   .....dbbbbd.....
    //   ....dbbddbbd....
    //   ....dbbddbbd....
    //   ....daad.daad...
    //   ....dddd.dddd...
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x4000, 0x1111, 0x1111, 0x0004,
        0x4000, 0x5521, 0x1522, 0x0004,
        0x4000, 0x2221, 0x1222, 0x0004,
        0x4444, 0x2114, 0x4112, 0x4444,
        0x1115, 0x3111, 0x1113, 0x5111,
        0x2215, 0x3142, 0x2413, 0x5122,
        0x4444, 0x2114, 0x4112, 0x4444,
        0x0000, 0x1140, 0x0411, 0x0000,
        0x0000, 0x2240, 0x0422, 0x0000,
        0x0000, 0x4224, 0x4224, 0x0000,
        0x0000, 0x4224, 0x4224, 0x0000,
        0x0000, 0x4114, 0x1140, 0x0004,
        0x0000, 0x4444, 0x4440, 0x0004,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtIceSpike
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   .......a........
    //   .......a........
    //   ...a..aba...a...
    //   ...a..aba..aa...
    //   ..aba.aba..ab...
    //   ..abb.abba.aba..
    //   .abbbaabbaabba..
    //   .abbbbabbbabbba.
    //   cccccccccccccccc
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x1000, 0x2100, 0x0001, 0x0001,
        0x1000, 0x2100, 0x1001, 0x0001,
        0x2100, 0x2101, 0x1001, 0x0002,
        0x2100, 0x2102, 0x1012, 0x0012,
        0x2210, 0x2112, 0x2112, 0x0012,
        0x2210, 0x2122, 0x2122, 0x0122,
        0x3333, 0x3333, 0x3333, 0x3333,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtBarrel
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   ....aaaaaaaa....
    //   ...abbbbbbbba...
    //   ...abcbbcbbca...
    //   ...aaaaaaaaaa...
    //   ...abcbbcbbca...
    //   ...abcbdcbbca...
    //   ...abcbbcbbca...
    //   ...aaaaaaaaaa...
    //   ...abcbbcbbca...
    //   ....aaaaaaaa....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1000, 0x2232, 0x3223, 0x0001,
        0x1000, 0x1111, 0x1111, 0x0001,
        0x1000, 0x2232, 0x3223, 0x0001,
        0x1000, 0x4232, 0x3223, 0x0001,
        0x1000, 0x2232, 0x3223, 0x0001,
        0x1000, 0x1111, 0x1111, 0x0001,
        0x1000, 0x2232, 0x3223, 0x0001,
        0x0000, 0x1111, 0x1111, 0x0000,
    },
    // kArtBones
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
    //   ......aaaa......
    //   .....aaaaaa.....
    //   ..a..acaaca..aa.
    //   .aaa.aaaaaa.aa..
    //   ..aaaabaab.aa...
    //   .bbbbbbbbbbbbbb.
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
        0x0000, 0x1100, 0x0011, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0100, 0x1310, 0x0131, 0x0110,
        0x1110, 0x1110, 0x0111, 0x0011,
        0x1100, 0x1211, 0x1021, 0x0001,
        0x2220, 0x2222, 0x2222, 0x0222,
    },
    // kArtCrystal
    //   ................
    //   ................
    //   ................
    //   ................
    //   .......d........
    //   ......acb.......
    //   ......acb...d...
    //   ..d...acb..acb..
    //   ..cb..abcb.acb..
    //   .acb.aabcb.abcb.
    //   .acbbaabcbbabcb.
    //   .aabbaabcbbabbb.
    //   ..abbaaabbbabb..
    //   ..aabbaabbbaab..
    //   ...aaaaaaaaaa...
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4000, 0x0000, 0x0000,
        0x0000, 0x3100, 0x0002, 0x0000,
        0x0000, 0x3100, 0x0002, 0x0004,
        0x0400, 0x3100, 0x1002, 0x0023,
        0x2300, 0x2100, 0x1023, 0x0023,
        0x2310, 0x2110, 0x1023, 0x0232,
        0x2310, 0x2112, 0x1223, 0x0232,
        0x2110, 0x2112, 0x1223, 0x0222,
        0x2100, 0x1112, 0x1222, 0x0022,
        0x1100, 0x1122, 0x1222, 0x0021,
        0x1000, 0x1111, 0x1111, 0x0001,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtCandle
    //   ................
    //   ...d...d...d....
    //   ...c...c...c....
    //   ...c...c...c....
    //   ...b...b...b....
    //   ...b...b...b....
    //   ..aaa.aaa.aaa...
    //   ...a...a...a....
    //   ...aaaaaaaaa....
    //   .......a........
    //   .......a........
    //   .......a........
    //   .......a........
    //   ......aaa.......
    //   .....aaaaa......
    //   ....aaaaaaa.....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x4000, 0x4000, 0x4000, 0x0000,
        0x3000, 0x3000, 0x3000, 0x0000,
        0x3000, 0x3000, 0x3000, 0x0000,
        0x2000, 0x2000, 0x2000, 0x0000,
        0x2000, 0x2000, 0x2000, 0x0000,
        0x1100, 0x1101, 0x1101, 0x0001,
        0x1000, 0x1000, 0x1000, 0x0000,
        0x1000, 0x1111, 0x1111, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x0000, 0x1000, 0x0000, 0x0000,
        0x0000, 0x1100, 0x0001, 0x0000,
        0x0000, 0x1110, 0x0011, 0x0000,
        0x0000, 0x1111, 0x0111, 0x0000,
    },
    // kArtDragonRoar
    //   .d............d.
    //   .dd..........dd.
    //   ..dd..aaaa..dd..
    //   ...daabbbbaad...
    //   ....abbbbbba....
    //   ...abfbbbbfba...
    //   ...abbbbbbbba...
    //   ...abbbbbbbba...
    //   ....abbbbbba....
    //   ....aeeeeeea....
    //   ....aeggggea....
    //   .....aeggea.....
    //   ......aeea......
    //   .......aa.......
    //   ................
    //   ................
    {
        0x0040, 0x0000, 0x0000, 0x0400,
        0x0440, 0x0000, 0x0000, 0x0440,
        0x4400, 0x1100, 0x0011, 0x0044,
        0x4000, 0x2211, 0x1122, 0x0004,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x1000, 0x2262, 0x2622, 0x0001,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x1000, 0x2222, 0x2222, 0x0001,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x0000, 0x5551, 0x1555, 0x0000,
        0x0000, 0x7751, 0x1577, 0x0000,
        0x0000, 0x7510, 0x0157, 0x0000,
        0x0000, 0x5100, 0x0015, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtWingUp
    //   a...............
    //   aa..............
    //   aba.............
    //   abba..........a.
    //   abbba........aa.
    //   abbbba......aba.
    //   abcbbba....abba.
    //   abccbbba..abbba.
    //   abcccbbbaabbbba.
    //   abccccbbbbbbbaa.
    //   abcccccbbbbbaa..
    //   .abcccccbbbaa...
    //   ..aabcccbbaa....
    //   ....aabbaa......
    //   ......aa........
    //   ................
    {
        0x0001, 0x0000, 0x0000, 0x0000,
        0x0011, 0x0000, 0x0000, 0x0000,
        0x0121, 0x0000, 0x0000, 0x0000,
        0x1221, 0x0000, 0x0000, 0x0100,
        0x2221, 0x0001, 0x0000, 0x0110,
        0x2221, 0x0012, 0x0000, 0x0121,
        0x2321, 0x0122, 0x1000, 0x0122,
        0x3321, 0x1222, 0x2100, 0x0122,
        0x3321, 0x2223, 0x2211, 0x0122,
        0x3321, 0x2233, 0x2222, 0x0112,
        0x3321, 0x2333, 0x2222, 0x0011,
        0x3210, 0x3333, 0x1222, 0x0001,
        0x1100, 0x3332, 0x1122, 0x0000,
        0x0000, 0x2211, 0x0011, 0x0000,
        0x0000, 0x1100, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtWingDown
    //   ................
    //   ................
    //   ................
    //   ................
    //   ................
    //   aa..............
    //   abaa............
    //   abbbaa.........a
    //   abcbbbaa......aa
    //   abccbbbbaa...aba
    //   abcccbbbbbaaabba
    //   .abccccbbbbbbbba
    //   ..abcccccbbbbbaa
    //   ...aabccccbbaa..
    //   .....aabbbaa....
    //   .......aaa......
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0011, 0x0000, 0x0000, 0x0000,
        0x1121, 0x0000, 0x0000, 0x0000,
        0x2221, 0x0011, 0x0000, 0x1000,
        0x2321, 0x1122, 0x0000, 0x1100,
        0x3321, 0x2222, 0x0011, 0x1210,
        0x3321, 0x2223, 0x1122, 0x1221,
        0x3210, 0x2333, 0x2222, 0x1222,
        0x2100, 0x3333, 0x2223, 0x1122,
        0x1000, 0x3321, 0x2233, 0x0011,
        0x0000, 0x2110, 0x1122, 0x0000,
        0x0000, 0x1000, 0x0011, 0x0000,
    },
    // kArtIconStaff
    //   ...........aaa..
    //   ..........adbba.
    //   ..........abdba.
    //   ...........abba.
    //   ..........aaaa..
    //   .........acca...
    //   ........acca....
    //   .......acca.....
    //   ......acca......
    //   .....acca.......
    //   ....acca........
    //   ...acca.........
    //   ..acca..........
    //   ..aaa...........
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x1000, 0x0011,
        0x0000, 0x0000, 0x4100, 0x0122,
        0x0000, 0x0000, 0x2100, 0x0124,
        0x0000, 0x0000, 0x1000, 0x0122,
        0x0000, 0x0000, 0x1100, 0x0011,
        0x0000, 0x0000, 0x3310, 0x0001,
        0x0000, 0x0000, 0x1331, 0x0000,
        0x0000, 0x1000, 0x0133, 0x0000,
        0x0000, 0x3100, 0x0013, 0x0000,
        0x0000, 0x3310, 0x0001, 0x0000,
        0x0000, 0x1331, 0x0000, 0x0000,
        0x1000, 0x0133, 0x0000, 0x0000,
        0x3100, 0x0013, 0x0000, 0x0000,
        0x1100, 0x0001, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtIconBody
    //   ................
    //   ...aaa....aaa...
    //   ..abba....abba..
    //   ..abbaaaaaabba..
    //   .abbbbddddbbbba.
    //   .abbbbbbbbbbbba.
    //   .aacbbbbbbbbcaa.
    //   ..acbbbbbbbbca..
    //   ..acbbbddbbbca..
    //   ..acbbbbbbbbca..
    //   ..acbbbbbbbbca..
    //   ..acbbbbbbbbca..
    //   ..acccbbbbccca..
    //   ..aaaaaaaaaaaa..
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x1000, 0x0011, 0x1100, 0x0001,
        0x2100, 0x0012, 0x2100, 0x0012,
        0x2100, 0x1112, 0x2111, 0x0012,
        0x2210, 0x4422, 0x2244, 0x0122,
        0x2210, 0x2222, 0x2222, 0x0122,
        0x3110, 0x2222, 0x2222, 0x0113,
        0x3100, 0x2222, 0x2222, 0x0013,
        0x3100, 0x4222, 0x2224, 0x0013,
        0x3100, 0x2222, 0x2222, 0x0013,
        0x3100, 0x2222, 0x2222, 0x0013,
        0x3100, 0x2222, 0x2222, 0x0013,
        0x3100, 0x2233, 0x3322, 0x0013,
        0x1100, 0x1111, 0x1111, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtIconHelm
    //   ................
    //   ................
    //   .....aaaaaa.....
    //   ....abbbbbba....
    //   ...abbddbbbba...
    //   ..abbdbbbbbbba..
    //   ..abbbbbbbbbba..
    //   ..abbbbbbbbbba..
    //   ..abaaaaaaaaba..
    //   ..aba.aaaa.aba..
    //   ..abaa.aa.aaba..
    //   ..acca....acca..
    //   ..acca....acca..
    //   ..aaaa....aaaa..
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x2221, 0x1222, 0x0000,
        0x1000, 0x4422, 0x2222, 0x0001,
        0x2100, 0x2242, 0x2222, 0x0012,
        0x2100, 0x2222, 0x2222, 0x0012,
        0x2100, 0x2222, 0x2222, 0x0012,
        0x2100, 0x1111, 0x1111, 0x0012,
        0x2100, 0x1101, 0x1011, 0x0012,
        0x2100, 0x1011, 0x1101, 0x0012,
        0x3100, 0x0013, 0x3100, 0x0013,
        0x3100, 0x0013, 0x3100, 0x0013,
        0x1100, 0x0011, 0x1100, 0x0011,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtIconRing
    //   ................
    //   ................
    //   ......aaaa......
    //   .....adbbda.....
    //   ......abba......
    //   .....aaaaaa.....
    //   ....acc..cca....
    //   ...acc....cca...
    //   ...ac......ca...
    //   ...ac......ca...
    //   ...acc....cca...
    //   ....acc..cca....
    //   .....aaaaaa.....
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1100, 0x0011, 0x0000,
        0x0000, 0x2410, 0x0142, 0x0000,
        0x0000, 0x2100, 0x0012, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x0331, 0x1330, 0x0000,
        0x1000, 0x0033, 0x3300, 0x0001,
        0x1000, 0x0003, 0x3000, 0x0001,
        0x1000, 0x0003, 0x3000, 0x0001,
        0x1000, 0x0033, 0x3300, 0x0001,
        0x0000, 0x0331, 0x1330, 0x0000,
        0x0000, 0x1110, 0x0111, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtIconRune
    //   ................
    //   ................
    //   ....aaaaaaaa....
    //   ...acccccccca...
    //   ...accbbbbcca...
    //   ...accbccccca...
    //   ...accbbbbcca...
    //   ...acccccbcca...
    //   ...accbbbbcca...
    //   ...acccccccca...
    //   ...acccccccca...
    //   ...acccccccca...
    //   ....aaaaaaaa....
    //   ................
    //   ................
    //   ................
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x1000, 0x3333, 0x3333, 0x0001,
        0x1000, 0x2233, 0x3322, 0x0001,
        0x1000, 0x3233, 0x3333, 0x0001,
        0x1000, 0x2233, 0x3322, 0x0001,
        0x1000, 0x3333, 0x3323, 0x0001,
        0x1000, 0x2233, 0x3322, 0x0001,
        0x1000, 0x3333, 0x3333, 0x0001,
        0x1000, 0x3333, 0x3333, 0x0001,
        0x1000, 0x3333, 0x3333, 0x0001,
        0x0000, 0x1111, 0x1111, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
    },
    // kArtSkelBack
    //   ................
    //   .....dddddd.....
    //   ....dddddddd....
    //   ....dddddddd....
    //   ....dddadddd....
    //   .....dddddd.....
    //   .......aa.......
    //   .dd.ddaaaadd.dd.
    //   c.d..dddaaddd.d.
    //   c.d..ddaaaadd.d.
    //   c.d..dddaaddd.d.
    //   ccd..ddaaaadd.d.
    //   .....dddaaddd...
    //   .....dd..dd.....
    //   .....dd..dd.....
    //   ....ddd..ddd....
    {
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x4440, 0x0444, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x0000, 0x4444, 0x4444, 0x0000,
        0x0000, 0x1444, 0x4444, 0x0000,
        0x0000, 0x4440, 0x0444, 0x0000,
        0x0000, 0x1000, 0x0001, 0x0000,
        0x0440, 0x1144, 0x4411, 0x0440,
        0x0403, 0x4440, 0x4411, 0x0404,
        0x0403, 0x1440, 0x4111, 0x0404,
        0x0403, 0x4440, 0x4411, 0x0404,
        0x0433, 0x1440, 0x4111, 0x0404,
        0x0000, 0x4440, 0x4411, 0x0004,
        0x0000, 0x0440, 0x0440, 0x0000,
        0x0000, 0x0440, 0x0440, 0x0000,
        0x0000, 0x0444, 0x4440, 0x0000,
    },
};
      // list ends with ch = 0

static const uint32_t kPalGoblin[PAL_N] = { COLOR_TETRIS_GRID, COLOR_DARK_GRAY, COLOR_GOLD, COLOR_OLIVE, COLOR_DARK_GRAY, COLOR_TETRIS_GRID, COLOR_LIGHT_GRAY, COLOR_WHITE };

static const uint32_t kPalBat[PAL_N] = { COLOR_GRAY, COLOR_CRIMSON, COLOR_DARK_PURPLE, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

// Allies (Necromancer summons, risen foes, Slime Crown slimes): every colour a shade of
// spectral violet, so red eyes and faces vanish and they read as "on your side".
static const uint32_t kP_ALLY[PAL_N] = { 0x002A1040UL, 0x00E0A0FFUL, 0x00B090F0UL, 0x008C5CD8UL,
                                         0x00C8A8FFUL, 0x005A2E90UL, 0x00D0B0FFUL, 0x007040B0UL };
static const uint32_t kPalSkel[PAL_N] = { COLOR_GRAY, COLOR_CRIMSON, COLOR_SILVER, COLOR_LIGHT_GRAY, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

// Lich Lord: a black cloak, b grey folds, c bone, d bright green eyes, e green magic, f staff
static const uint32_t kPalLich[PAL_N] = { 0x00141018UL, 0x003A3644UL, 0x00E6E0CCUL, 0x0040FF60UL,
                                          0x0020C040UL, COLOR_GRAY, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalBeholder[PAL_N] = { COLOR_BLACK, COLOR_TETRIS_GRID, COLOR_PURPLE, COLOR_CRIMSON, COLOR_WHITE, COLOR_DARK_PURPLE, COLOR_WHITE, COLOR_GOLD };

static const uint32_t kPalBeholderHot[PAL_N] = { COLOR_CRIMSON, COLOR_TETRIS_GRID, COLOR_GRAY, COLOR_KHAKI, COLOR_WHITE, COLOR_DARK_PURPLE, COLOR_PINK, COLOR_CRIMSON };

static const uint32_t kPalFlaskFire[PAL_N] = { COLOR_TETRIS_L_ORANGE, COLOR_TETRIS_GRID, COLOR_LIGHT_GRAY, COLOR_OLIVE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalFlaskFrost[PAL_N] = { COLOR_SKY_BLUE, COLOR_TETRIS_GRID, COLOR_LIGHT_GRAY, COLOR_OLIVE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalFlaskVenom[PAL_N] = { COLOR_GREEN, COLOR_TETRIS_GRID, COLOR_LIGHT_GRAY, COLOR_OLIVE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalFlaskMana[PAL_N] = { COLOR_BLUE, COLOR_NAVY, COLOR_WHITE, COLOR_GOLD, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalBolt[PAL_N] = { COLOR_CRIMSON, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalPuffFire[PAL_N] = { COLOR_BROWN, COLOR_TETRIS_L_ORANGE, COLOR_KHAKI, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalPuffFrost[PAL_N] = { COLOR_TEAL, COLOR_SKY_BLUE, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalPuffVenom[PAL_N] = { COLOR_DARK_GRAY, COLOR_OLIVE, COLOR_MINT, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };

static const uint32_t kPalPuffArcane[PAL_N] = { COLOR_GRAY, COLOR_VIOLET, COLOR_WHITE, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA, COLOR_MAGENTA };


// Ancient Dragon parts: a outline, b scales, c belly / wing membrane, d horns + claws, e chest core, f eyes, g fire
static const uint32_t kPalDragon[PAL_N] = { 0x002A0A10UL, 0x008A1A20UL, 0x00D08040UL, 0x00E8E0C0UL, 0x00601010UL, COLOR_GOLD, COLOR_ORANGE, COLOR_MAGENTA };
// exposed weak spot: darker scales, the chest core blazes
static const uint32_t kPalDragonCore[PAL_N] = { 0x002A0A10UL, 0x005A1018UL, 0x00A06030UL, 0x00E8E0C0UL, 0x00FFF060UL, COLOR_WHITE, COLOR_ORANGE, COLOR_MAGENTA };

// breath / fireball wind-up: glowing hot
static const uint32_t kPalDragonHot[PAL_N] = { 0x002A0A10UL, 0x00B02020UL, 0x00E09050UL, 0x00FFFFFFUL, COLOR_ORANGE, COLOR_WHITE, 0x00FFE040UL, COLOR_MAGENTA };


const uint32_t *const rc_kPal[PT_COUNT] =
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
    kP_MERCHANT, kP_ARCHER, kP_KEY, kP_HEART, kP_ALTAR, kP_CHEST_GOOD,
    kP_GOLEM_EARTH, kP_ROCK, kP_SPIKE, kP_MARK_EARTH, kP_MARK_BLUE, kP_MARK_WHITE, kP_MARK_GREEN,
    kP_BARREL, kP_BARREL_G, kP_BONES, kP_CRYSTAL, kP_CANDLE, kPalDragonCore, kP_ALLY
};


// Sprite tint: 0 none, 1 white (hit), 2 orange (burning), 3 ice blue (frozen), 4 gold (Golden Cataclysm)
const uint32_t rc_kTint[5] = { 0, COLOR_WHITE, COLOR_TETRIS_L_ORANGE, COLOR_SKY_BLUE, COLOR_GOLD };


// tile 6 = the Earth Golem's seal walls (packed earth)
const uint32_t rc_kWallBase[TILE_KINDS] =
    { 0, COLOR_GRAY, COLOR_GRAY, COLOR_BROWN, COLOR_ORANGE, COLOR_TURQUOISE, 0x006B4A2AUL, COLOR_GRAY };

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

// rc_kWpnArm (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .....kssssssssk.kk..
//   .....ksssssssskkssk.
//   .....kssssssssqkssk.
//   .....kssssssssqkkk..
//   ....kkqqqqqqqqkk....
//   ..kccccccccccck.....
//   .kccdccccccccdck....
//   kccdcccccccccccdck..
const uint16_t rc_kWpnArm[40] =
{
    0x0000, 0x2210, 0x2222, 0x0122, 0x0011,
    0x0000, 0x2210, 0x2222, 0x1122, 0x0122,
    0x0000, 0x2210, 0x2222, 0x1322, 0x0122,
    0x0000, 0x2210, 0x2222, 0x1322, 0x0011,
    0x0000, 0x3311, 0x3333, 0x1133, 0x0000,
    0x4100, 0x4444, 0x4444, 0x0144, 0x0000,
    0x4410, 0x4445, 0x4444, 0x1454, 0x0000,
    0x5441, 0x4444, 0x4444, 0x5444, 0x0014,
};


// ---- 0: RAY WAND (cyan crystal on a slim silver wand)
// kArtRay (ASCII source: edit these rows, then run tools/wpnpack.py)
//   ....................
//   .........kk.........
//   ........kghk........
//   ........kggk........
//   ........kggk........
//   .........kk.........
//   .......kkwxkk.......
//   ........kwxk........
//   ........kwxk........
//   ........kwxk........
static const uint16_t kArtRay[50] =
{
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0110, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1981, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1881, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1881, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0110, 0x0000, 0x0000,
    0x0000, 0x1000, 0x1761, 0x0001, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
};


// ---- 1: PRISM STAFF (gold shaft, glass prism)
// kArtPrism (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........kk.........
//   ........kghk........
//   .......kgghgk.......
//   ......kgggghgk......
//   ......kkkkkkkk......
//   .......kkwxkk.......
//   ......kkkwxkkk......
//   ........kwxk........
//   ........kwxk........
//   ........kwxk........
static const uint16_t kArtPrism[50] =
{
    0x0000, 0x0000, 0x0110, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1981, 0x0000, 0x0000,
    0x0000, 0x1000, 0x8988, 0x0001, 0x0000,
    0x0000, 0x8100, 0x9888, 0x0018, 0x0000,
    0x0000, 0x1100, 0x1111, 0x0011, 0x0000,
    0x0000, 0x1000, 0x1761, 0x0001, 0x0000,
    0x0000, 0x1100, 0x1761, 0x0011, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
};


// ---- 2: LANCE (wooden haft, steel blade, red tassel)
// ---- 3: STORM ROD (steel fork with a spark between the prongs)
// kArtStorm (ASCII source: edit these rows, then run tools/wpnpack.py)
//   ......k......k......
//   ......kw....wk......
//   .......kw..wk.......
//   .......kwggwk.......
//   ........kwwk........
//   ........kxwk........
//   .......kkwxkk.......
//   ........kwxk........
//   ........kwxk........
//   ........kwxk........
static const uint16_t kArtStorm[50] =
{
    0x0000, 0x0100, 0x0000, 0x0010, 0x0000,
    0x0000, 0x6100, 0x0000, 0x0016, 0x0000,
    0x0000, 0x1000, 0x6006, 0x0001, 0x0000,
    0x0000, 0x1000, 0x6886, 0x0001, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1671, 0x0000, 0x0000,
    0x0000, 0x1000, 0x1761, 0x0001, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
};


// ---- 4: SUNFIRE (golden staff, burning sun)
// kArtSun (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........ff.........
//   .......kkkkkk.......
//   ......kgghhggk......
//   ......kghhhhgk......
//   ......kgghhggk......
//   .......kkkkkk.......
//   ......kkkwxkkk......
//   ........kwxk........
//   ........kwxk........
//   ........kwxk........
static const uint16_t kArtSun[50] =
{
    0x0000, 0x0000, 0x0AA0, 0x0000, 0x0000,
    0x0000, 0x1000, 0x1111, 0x0001, 0x0000,
    0x0000, 0x8100, 0x8998, 0x0018, 0x0000,
    0x0000, 0x8100, 0x9999, 0x0018, 0x0000,
    0x0000, 0x8100, 0x8998, 0x0018, 0x0000,
    0x0000, 0x1000, 0x1111, 0x0001, 0x0000,
    0x0000, 0x1100, 0x1761, 0x0011, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
};


// ---- 5: FIRE FLASK (round bulb, burning wick)
// kArtFire (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........ff.........
//   ........fyyf........
//   ........krrk........
//   ........kwwk........
//   ........kwxk........
//   .......kwggxk.......
//   ......kwggghxk......
//   .....kwgggghgxk.....
//   ....kgggggghgggk....
//   ....kgggggggggxk....
static const uint16_t kArtFire[50] =
{
    0x0000, 0x0000, 0x0AA0, 0x0000, 0x0000,
    0x0000, 0x0000, 0xABBA, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1CC1, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1761, 0x0000, 0x0000,
    0x0000, 0x1000, 0x7886, 0x0001, 0x0000,
    0x0000, 0x6100, 0x9888, 0x0017, 0x0000,
    0x0000, 0x8610, 0x9888, 0x0178, 0x0000,
    0x0000, 0x8881, 0x9888, 0x1888, 0x0000,
    0x0000, 0x8881, 0x8888, 0x1788, 0x0000,
};


// ---- 6: FROST FLASK (tall vial, ice crystal stopper)
// kArtFrost (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........hh.........
//   ........hffh........
//   ........khhk........
//   ........kwwk........
//   .......kwggxk.......
//   .......kwgghk.......
//   .......kwggxk.......
//   .......kgghgk.......
//   .......kggggk.......
//   .......kggggk.......
static const uint16_t kArtFrost[50] =
{
    0x0000, 0x0000, 0x0990, 0x0000, 0x0000,
    0x0000, 0x0000, 0x9AA9, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1991, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0x1000, 0x7886, 0x0001, 0x0000,
    0x0000, 0x1000, 0x9886, 0x0001, 0x0000,
    0x0000, 0x1000, 0x7886, 0x0001, 0x0000,
    0x0000, 0x1000, 0x8988, 0x0001, 0x0000,
    0x0000, 0x1000, 0x8888, 0x0001, 0x0000,
    0x0000, 0x1000, 0x8888, 0x0001, 0x0000,
};


// ---- 7: VENOM FLASK (skull stopper, toxic liquid)
// kArtVenom (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .......kyyyyk.......
//   ......kyrkkryk......
//   .......kkyykk.......
//   ........kwwk........
//   .......kwggxk.......
//   ......kwgghgxk......
//   .....kwggggggxk.....
//   .....kwghggggxk.....
//   .....kggggggggk.....
//   ....kgghggggggxk....
static const uint16_t kArtVenom[50] =
{
    0x0000, 0x1000, 0xBBBB, 0x0001, 0x0000,
    0x0000, 0xB100, 0xC11C, 0x001B, 0x0000,
    0x0000, 0x1000, 0x1BB1, 0x0001, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0x1000, 0x7886, 0x0001, 0x0000,
    0x0000, 0x6100, 0x8988, 0x0017, 0x0000,
    0x0000, 0x8610, 0x8888, 0x0178, 0x0000,
    0x0000, 0x8610, 0x8889, 0x0178, 0x0000,
    0x0000, 0x8810, 0x8888, 0x0188, 0x0000,
    0x0000, 0x9881, 0x8888, 0x1788, 0x0000,
};


// ---- 8: ELIXIR (heart bottle with a gold ribbon)
// kArtElixir (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........kk.........
//   ........kyyk........
//   ........kwwk........
//   .......rrrrrr.......
//   .......kwggxk.......
//   .....kwggkkggxk.....
//   ....kwgggghgggxk....
//   ....kggggggggggk....
//   ....kkggggggggkk....
//   ......kkggggkk......
static const uint16_t kArtElixir[50] =
{
    0x0000, 0x0000, 0x0110, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1BB1, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0xC000, 0xCCCC, 0x000C, 0x0000,
    0x0000, 0x1000, 0x7886, 0x0001, 0x0000,
    0x0000, 0x8610, 0x8118, 0x0178, 0x0000,
    0x0000, 0x8861, 0x8988, 0x1788, 0x0000,
    0x0000, 0x8881, 0x8888, 0x1888, 0x0000,
    0x0000, 0x8811, 0x8888, 0x1188, 0x0000,
    0x0000, 0x1100, 0x8888, 0x0011, 0x0000,
};


// ---- 9: HASTE BREW (golden draught, lightning swirl)
// kArtHaste (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........kk.........
//   ........kyyk........
//   ........kwwk........
//   ........kwwk........
//   .......kwggxk.......
//   ......kwgyggxk......
//   .....kwggyyggxk.....
//   .....kwgggyggxk.....
//   ....kggggyyggggk....
//   ....kggggyggggxk....
static const uint16_t kArtHaste[50] =
{
    0x0000, 0x0000, 0x0110, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1BB1, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1661, 0x0000, 0x0000,
    0x0000, 0x1000, 0x7886, 0x0001, 0x0000,
    0x0000, 0x6100, 0x88B8, 0x0017, 0x0000,
    0x0000, 0x8610, 0x8BB8, 0x0178, 0x0000,
    0x0000, 0x8610, 0x8B88, 0x0178, 0x0000,
    0x0000, 0x8881, 0x8BB8, 0x1888, 0x0000,
    0x0000, 0x8881, 0x88B8, 0x1788, 0x0000,
};


// ---- 10: ARCANE TOME (open spellbook, glowing runes)
// kArtTome (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .....f........f.....
//   ...kkkkkkkkkkkkkk...
//   ...khhhhhxxhhhhhk...
//   ...khhgghxxhgghhk...
//   ...khhghhxxhhghhk...
//   ...khhgghxxhgghhk...
//   ...khhhhhxxhhhhhk...
//   ...kwwwwwxxwwwwwk...
//   ...kkkkkkxxkkkkkk...
//   ...kkkkkxxxxkkkkk...
static const uint16_t kArtTome[50] =
{
    0x0000, 0x00A0, 0x0000, 0x0A00, 0x0000,
    0x1000, 0x1111, 0x1111, 0x1111, 0x0001,
    0x1000, 0x9999, 0x9779, 0x9999, 0x0001,
    0x1000, 0x8899, 0x9779, 0x9988, 0x0001,
    0x1000, 0x9899, 0x9779, 0x9989, 0x0001,
    0x1000, 0x8899, 0x9779, 0x9988, 0x0001,
    0x1000, 0x9999, 0x9779, 0x9999, 0x0001,
    0x1000, 0x6666, 0x6776, 0x6666, 0x0001,
    0x1000, 0x1111, 0x1771, 0x1111, 0x0001,
    0x1000, 0x1111, 0x7777, 0x1111, 0x0001,
};


// ---- 11: WARP RUNE (glowing rune stone)
// kArtWarp (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........ff.........
//   ........kkkk........
//   .......kwwwwk.......
//   ......kwggggwk......
//   ......kwghhgwk......
//   ......kwghhgwk......
//   ......kwggggwk......
//   ......kwwwwwwk......
//   ......kxwwwwxk......
//   ......kkxwwxkk......
static const uint16_t kArtWarp[50] =
{
    0x0000, 0x0000, 0x0AA0, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1111, 0x0000, 0x0000,
    0x0000, 0x1000, 0x6666, 0x0001, 0x0000,
    0x0000, 0x6100, 0x8888, 0x0016, 0x0000,
    0x0000, 0x6100, 0x8998, 0x0016, 0x0000,
    0x0000, 0x6100, 0x8998, 0x0016, 0x0000,
    0x0000, 0x6100, 0x8888, 0x0016, 0x0000,
    0x0000, 0x6100, 0x6666, 0x0016, 0x0000,
    0x0000, 0x7100, 0x6666, 0x0017, 0x0000,
    0x0000, 0x1100, 0x7667, 0x0011, 0x0000,
};


// ---- 12: AEGIS CHARM (round shield medallion)
// kArtAegis (ASCII source: edit these rows, then run tools/wpnpack.py)
//   .........kk.........
//   ........kyyk........
//   .......kkyykk.......
//   ......kyyyyyyk......
//   .....kywwhhwwyk.....
//   .....kywwhhwwyk.....
//   .....kywhhhhwyk.....
//   .....kywwhhwwyk.....
//   .....kkywwwwykk.....
//   .....kkkywwykkk.....
static const uint16_t kArtAegis[50] =
{
    0x0000, 0x0000, 0x0110, 0x0000, 0x0000,
    0x0000, 0x0000, 0x1BB1, 0x0000, 0x0000,
    0x0000, 0x1000, 0x1BB1, 0x0001, 0x0000,
    0x0000, 0xB100, 0xBBBB, 0x001B, 0x0000,
    0x0000, 0x6B10, 0x6996, 0x01B6, 0x0000,
    0x0000, 0x6B10, 0x6996, 0x01B6, 0x0000,
    0x0000, 0x6B10, 0x9999, 0x01B6, 0x0000,
    0x0000, 0x6B10, 0x6996, 0x01B6, 0x0000,
    0x0000, 0xB110, 0x6666, 0x011B, 0x0000,
    0x0000, 0x1110, 0xB66B, 0x0111, 0x0000,
};


const uint16_t *const rc_kWpnArt[WPN_ARTS] =
{
    kArtRay, kArtPrism, kArtStorm, kArtSun,
    kArtFire, kArtFrost, kArtVenom, kArtElixir, kArtHaste,
    kArtTome, kArtWarp, kArtAegis
};


static const uint32_t kWpnPalRay[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_TURQUOISE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_GOLD,COLOR_CRIMSON);

static const uint32_t kWpnPalPrism[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_DARK_PURPLE,COLOR_DARK_GRAY,COLOR_CORAL,COLOR_OLIVE,COLOR_VIOLET,COLOR_WHITE,COLOR_TURQUOISE,COLOR_KHAKI,COLOR_CORAL);


static const uint32_t kWpnPalStorm[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_DARK_GRAY,COLOR_SILVER,COLOR_GRAY,COLOR_PLUM,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_KHAKI,COLOR_CORAL);

static const uint32_t kWpnPalSun[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_BROWN,COLOR_MAROON,COLOR_ORANGE,COLOR_OLIVE,COLOR_CORAL,COLOR_KHAKI,COLOR_TETRIS_L_ORANGE,COLOR_KHAKI,COLOR_CRIMSON);

static const uint32_t kWpnPalFire[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_BROWN,COLOR_MAROON,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_TETRIS_L_ORANGE,COLOR_KHAKI,COLOR_TETRIS_L_ORANGE,COLOR_GOLD,COLOR_OLIVE);

static const uint32_t kWpnPalFrost[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_SKY_BLUE);

static const uint32_t kWpnPalVenom[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_DARK_GRAY,COLOR_TETRIS_GRID,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_GREEN,COLOR_MINT,COLOR_MINT,COLOR_LIGHT_GRAY,COLOR_CRIMSON);

static const uint32_t kWpnPalElixir[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_DARK_GRAY,COLOR_TETRIS_GRID,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_CRIMSON,COLOR_PLUM,COLOR_CORAL,COLOR_OLIVE,COLOR_GOLD);

static const uint32_t kWpnPalHaste[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_OLIVE,COLOR_DARK_GRAY,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_GOLD,COLOR_KHAKI,COLOR_KHAKI,COLOR_WHITE,COLOR_ORANGE);

static const uint32_t kWpnPalTome[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_DARK_PURPLE,COLOR_DARK_GRAY,COLOR_DARK_PURPLE,COLOR_DARK_PURPLE,COLOR_VIOLET,COLOR_LIGHT_GRAY,COLOR_WHITE,COLOR_GOLD,COLOR_VIOLET);

static const uint32_t kWpnPalWarp[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_TETRIS_GRID,COLOR_TEAL,COLOR_CHARCOAL,COLOR_TURQUOISE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_TURQUOISE);

static const uint32_t kWpnPalAegis[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_CHARCOAL,COLOR_CHARCOAL,COLOR_GRAY,COLOR_CHARCOAL,COLOR_SKY_BLUE,COLOR_WHITE,COLOR_SKY_BLUE,COLOR_GOLD,COLOR_GOLD);


// Class staffs and the Mana Potion reuse the art above with their own colours.
//                                       outline           sleeve             sleeve shade      body              body shade        gem               highlight      flash                accent            accent 2
static const uint32_t kWpnPalInfernal[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_MAROON,COLOR_BLACK,COLOR_CRIMSON,COLOR_MAROON,COLOR_TETRIS_L_ORANGE,COLOR_GOLD,COLOR_ORANGE,COLOR_GOLD,COLOR_CRIMSON);
static const uint32_t kWpnPalThunder[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_NAVY,COLOR_CHARCOAL,COLOR_SILVER,COLOR_GRAY,COLOR_GOLD,COLOR_WHITE,COLOR_GOLD,COLOR_SKY_BLUE,COLOR_GOLD);
static const uint32_t kWpnPalGrave[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_CHARCOAL,COLOR_BLACK,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_MINT,COLOR_WHITE,COLOR_GREEN,COLOR_LIGHT_GRAY,COLOR_MINT);
static const uint32_t kWpnPalPEngine[WPN_SYMS] = WPAL(COLOR_TETRIS_GRID,COLOR_NAVY,COLOR_CHARCOAL,COLOR_SKY_BLUE,COLOR_TEAL,COLOR_VIOLET,COLOR_WHITE,COLOR_PINK,COLOR_TURQUOISE,COLOR_VIOLET);
static const uint32_t kWpnPalEldritch[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_DARK_PURPLE,COLOR_BLACK,COLOR_DARK_GRAY,COLOR_BLACK,COLOR_GREEN,COLOR_MINT,COLOR_GREEN,COLOR_DARK_PURPLE,COLOR_GREEN);
static const uint32_t kWpnPalGilded[WPN_SYMS] = WPAL(COLOR_BROWN,COLOR_MAROON,COLOR_BROWN,COLOR_GOLD,COLOR_ORANGE,COLOR_KHAKI,COLOR_WHITE,COLOR_GOLD,COLOR_CRIMSON,COLOR_GOLD);
static const uint32_t kWpnPalMana[WPN_SYMS] = WPAL(COLOR_BLACK,COLOR_DARK_GRAY,COLOR_TETRIS_GRID,COLOR_LIGHT_GRAY,COLOR_GRAY,COLOR_BLUE,COLOR_SKY_BLUE,COLOR_SKY_BLUE,COLOR_OLIVE,COLOR_GOLD);

const uint32_t *const rc_kWpnPal[WPN_PALS] =
{
    kWpnPalRay, kWpnPalPrism, kWpnPalStorm, kWpnPalSun,
    kWpnPalFire, kWpnPalFrost, kWpnPalVenom, kWpnPalElixir, kWpnPalHaste,
    kWpnPalTome, kWpnPalWarp, kWpnPalAegis,
    kWpnPalInfernal, kWpnPalThunder, kWpnPalGrave, kWpnPalPEngine, kWpnPalEldritch, kWpnPalGilded, kWpnPalMana
};
