//#############################################################################
// FILE:   app/wason/wason_priv.h
// TITLE:  Wason Ware - private header shared by the modules of this app
//
//   FILES (app/wason/)
//     wason.h           public: g_wasonwareApp, tuning constants
//     wason.c           framework: title / taunt / pick list / intro / play /
//                       result / game over, fuse bar, app callbacks + descriptor
//     wason_table.c     ww_games[]: one line per microgame
//     wason_helpers.c   Box / Slide / input / text / pixel-art helpers (ww_*)
//     wason_art.c       sprite tables (ww_kArt*)
//     wason_<name>.c    one file per microgame (CLIMB!, BATTLE!, ...)
//
//   HOW TO ADD A MICROGAME
//     1. new file wason_<name>.c: XxInit / XxUpdate / XxRender (keep all of its
//        state static), export the three callbacks with a ww_ prefix
//     2. declare the three callbacks in section 4 below
//     3. add its background color to section 3 and one line to ww_games[]
//        in wason_table.c
//#############################################################################
#ifndef WASON_PRIV_H
#define WASON_PRIV_H

#include "wason.h"
#include "wason_sound.h"

//*****************************************************************************
// 1. Layout and shared types
//*****************************************************************************
#define FIELD_W     320
#define FIELD_H     230                     // playfield; the fuse bar is below
#define BAR_Y0      232
#define BAR_Y1      239

enum { MG_PLAY = 0, MG_WIN = 1, MG_LOSE = 2 };

typedef struct
{
    const char *cmd;                        // one-word command, shown big
    const char *inspo;                      // "inspired by" tag, shown small
    uint16_t    frames;                     // time limit (frames)
    uint32_t    bg;                         // background color
    int16_t     wx, wy, ww, wh;             // render window (screen coords); smaller = cheaper to repaint
    uint16_t    winOnTimeout;               // 1 = "survive" game, 0 = "do it" game
    void      (*init)(uint16_t spd);
    int16_t   (*update)(void);
    void      (*render)(void);
} MicroGame;

extern bool     ww_first;                   // true on the first render of a screen
extern uint16_t ww_frame;                   // frames since the microgame started

extern const MicroGame ww_games[];          // wason_table.c
extern const uint16_t  ww_numGames;
#define NUM_GAMES   (ww_numGames)

//*****************************************************************************
// 2. Helpers (wason_helpers.c)
//*****************************************************************************
extern int16_t ww_wx, ww_wy, ww_ww, ww_wh;  // current render window (set by the framework)

int16_t ww_Min16(int16_t a, int16_t b);
int16_t ww_Max16(int16_t a, int16_t b);
int16_t ww_Abs16(int16_t a);
int16_t ww_Sgn16(int16_t a);
int16_t ww_Clamp16(int16_t v, int16_t lo, int16_t hi);
int16_t ww_Rnd(int16_t n);
void ww_Box(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t c);
bool ww_Hit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                int16_t bx, int16_t by, int16_t bw, int16_t bh);
void ww_Disc(int16_t x, int16_t y, int16_t r, uint32_t c);
void ww_SlideX(int16_t y, int16_t h, int16_t x0, int16_t x1, int16_t w,
                   uint32_t c, uint32_t bg);
void ww_SlideY(int16_t x, int16_t w, int16_t y0, int16_t y1, int16_t h,
                   uint32_t c, uint32_t bg);
int16_t ww_HeldX(void);
int16_t ww_HeldY(void);
bool ww_EdgeL(void);
bool ww_EdgeR(void);
bool ww_EdgeU(void);
bool ww_EdgeD(void);
bool ww_AnyPress(void);
int16_t ww_PaddleSpeed(int16_t maxSpeed);
void ww_Text(const char *s, int16_t x, int16_t y, uint32_t c);
void ww_BoldText(const char *s, int16_t x, int16_t y, uint32_t c);
void ww_Label(char *buf, const char *label, uint32_t v);
void ww_Arrow(int16_t cx, int16_t cy, int16_t dir, int16_t u, uint32_t c);
void ww_EraseOutside(int16_t ox, int16_t oy, int16_t ow, int16_t oh,
                         int16_t nx, int16_t ny, int16_t nw, int16_t nh, uint32_t bg);
uint32_t ww_ArtColor(char ch, uint32_t bg);
bool ww_SameRow(const char *a, const char *b, int16_t w);
void ww_DrawArt(int16_t x, int16_t y, const char *const *rows, int16_t w, int16_t h,
                    int16_t sc, uint32_t bg, bool opaque);
void ww_MoveArt(int16_t ox, int16_t oy, int16_t nx, int16_t ny, const char *const *rows,
                    int16_t w, int16_t h, int16_t sc, uint32_t bg);

//*****************************************************************************
// 3. Microgame background colors (the table in wason_table.c uses them)
//*****************************************************************************
#define DK_BG    0x101030UL
#define PK_BG    0x203040UL
#define PB_BG    0x14143CUL
#define SO_BG    0x4FA5F5UL
#define PZ_BG0   0x4CAF50UL
#define PZ_BG1   0x3E9A42UL
#define MT_BG    0x14001EUL
#define FN_BG    0x1A1A2EUL
#define RS_SKY   0x6EC6FFUL
#define SF_SKY   0x87CEEBUL
#define HD_BG    0x0B1220UL
#define BK_BG    0x101828UL
#define TT_BG    0x0A0A20UL
#define PM_BG    COLOR_BLACK
#define FR_BG    0x303030UL
#define FB_BG    0x4EC0CAUL
#define DN_BG    0xF7F7F7UL
#define MR_BG    0x5C94FCUL
#define GD_BG    0x1A0B3BUL
#define BD_BG    0x87CEEBUL
#define ML_BG    0x2B1B4DUL
#define RC_BG    0x203038UL
#define PG_BG    0x000000UL
#define GL_BG    COLOR_BLACK

//*****************************************************************************
// 4. Microgame callbacks (one wason_<name>.c each)
//*****************************************************************************
void    ww_DkInit(uint16_t spd);   int16_t ww_DkUpdate(void);   void ww_DkRender(void);   // CLIMB! Donkey Kong
void    ww_PkInit(uint16_t spd);   int16_t ww_PkUpdate(void);   void ww_PkRender(void);   // BATTLE! Pokemon
void    ww_PbInit(uint16_t spd);   int16_t ww_PbUpdate(void);   void ww_PbRender(void);   // FLIP! Pokemon Pinball
void    ww_SoInit(uint16_t spd);   int16_t ww_SoUpdate(void);   void ww_SoRender(void);   // RUN! Sonic
void    ww_PzInit(uint16_t spd);   int16_t ww_PzUpdate(void);   void ww_PzRender(void);   // PLANT! Plants vs Zombies
void    ww_MtInit(uint16_t spd);   int16_t ww_MtUpdate(void);   void ww_MtRender(void);   // BLAST! Metroid
void    ww_FnInit(uint16_t spd);   int16_t ww_FnUpdate(void);   void ww_FnRender(void);   // SLICE! Fruit Ninja
void    ww_RsInit(uint16_t spd);   int16_t ww_RsUpdate(void);   void ww_RsRender(void);   // RACE! + SURF!
void    ww_SfInit(uint16_t spd);   int16_t ww_SfUpdate(void);   void ww_SfRender(void);   // RACE! + SURF!
void    ww_HdInit(uint16_t spd);   int16_t ww_HdUpdate(void);   void ww_HdRender(void);   // CALL IT! Helldivers 2
void    ww_BkInit(uint16_t spd);   int16_t ww_BkUpdate(void);   void ww_BkRender(void);   // BREAK! Brick Breaker
void    ww_TtInit(uint16_t spd);   int16_t ww_TtUpdate(void);   void ww_TtRender(void);   // FIT IT! Tetris
void    ww_PmInit(uint16_t spd);   int16_t ww_PmUpdate(void);   void ww_PmRender(void);   // CHOMP! Pac-Man
void    ww_FrInit(uint16_t spd);   int16_t ww_FrUpdate(void);   void ww_FrRender(void);   // CROSS! Frogger
void    ww_FbInit(uint16_t spd);   int16_t ww_FbUpdate(void);   void ww_FbRender(void);   // FLAP! Flappy Bird
void    ww_DnInit(uint16_t spd);   int16_t ww_DnUpdate(void);   void ww_DnRender(void);   // JUMP! Chrome Dino
void    ww_MrInit(uint16_t spd);   int16_t ww_MrUpdate(void);   void ww_MrRender(void);   // STOMP! Super Mario
void    ww_GdInit(uint16_t spd);   int16_t ww_GdUpdate(void);   void ww_GdRender(void);   // DASH! Geometry Dash
void    ww_BdInit(uint16_t spd);   int16_t ww_BdUpdate(void);   void ww_BdRender(void);   // LAUNCH! Angry Birds
void    ww_MlInit(uint16_t spd);   int16_t ww_MlUpdate(void);   void ww_MlRender(void);   // BEAT! Melatonin
void    ww_RcInit(uint16_t spd);   int16_t ww_RcUpdate(void);   void ww_RcRender(void);   // SORT! Recycler
void    ww_PgInit(uint16_t spd);   int16_t ww_PgUpdate(void);   void ww_PgRender(void);   // RETURN! Pong
void    ww_GlInit(uint16_t spd);   int16_t ww_GlUpdate(void);   void ww_GlRender(void);   // SHOOT! Galaga

//*****************************************************************************
// 5. Pixel art (wason_art.c)
//*****************************************************************************
extern const char *const ww_kArtFire[16];
extern const char *const ww_kArtWater[16];
extern const char *const ww_kArtGrass[16];
extern const char *const ww_kArtSonic0[20];
extern const char *const ww_kArtSonic1[20];
extern const char *const ww_kArtSonic2[20];
extern const char *const ww_kArtSpin[16];
extern const char *const ww_kArtMotobug[12];
extern const char *const ww_kArtRing[10];
extern const char *const ww_kArtSpikes[10];
extern const char *const ww_kArtSpring[10];
extern const char *const ww_kArtSunflower[20];
extern const char *const ww_kArtPalm[30];
extern const char *const ww_kArtOrange[16];
extern const char *const ww_kArtApple[16];
extern const char *const ww_kArtLime[16];
extern const char *const ww_kArtBomb[16];
extern const char *const ww_kArtPeashooter[16];
extern const char *const ww_kArtZombie[22];
extern const char *const ww_kArtPea[6];
extern const char *const ww_kArtRun0[22];
extern const char *const ww_kArtRun1[22];
extern const char *const ww_kArtBall[12];
extern const char *const ww_kArtBallHit[12];
extern const char *const ww_kArtPikachu[16];
extern const char *const ww_kArtDiglett[12];
extern const char *const ww_kArtGdSpike[12];
extern const char *const ww_kArtNote[12];
extern const char *const ww_kArtMetroid[14];
extern const char *const ww_kArtSamus[22];
extern const char *const ww_kArtReo[10];
extern const char *const ww_kArtWario[24];
extern const char *const ww_kArtMouthC[5];
extern const char *const ww_kArtMouthO[5];

//*****************************************************************************
// 6. Sounds this app plays that songs.h may not declare (defined in songs.c /
//    tetris_sound.c). Repeating an extern is harmless if songs.h has them too.
//*****************************************************************************
extern const Song song_melatonin;           // BEAT!  (songs.c)
extern const Song sfx_talk;                 // taunt blips (songs.c)
extern const Song sfx_tetris_say;           // FIT IT! (tetris_sound.c)

#endif // WASON_PRIV_H
