//#############################################################################
// FILE:   app/wason/wason_battle.c
// TITLE:  Wason Ware microgame BATTLE!  (Pokemon)
//
//   Public: ww_PkInit, ww_PkUpdate, ww_PkRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// BATTLE!  (Pokemon)  pick the right attack with the COLOR buttons: RED = FLAMETHROWER,
//          BLUE = HYDRO PUMP, GREEN = RAZOR LEAF.  Water > Fire > Grass > Water.  Land 3 hits.
// ============================================================================

static const char *const *const kPkArt[3] = { ww_kArtFire, ww_kArtWater, ww_kArtGrass };
static const int16_t  kPkCounter[3] = { 1, 2, 0 };          // fire<-water, water<-grass, grass<-fire
static const char    *kPkName[3]   = { "FIRE", "WATER", "GRASS" };
static int16_t pk_t, pk_hits, pk_shown, pk_drawn, pk_mv, pk_ax, pk_sx;
static bool    pk_anim, pk_sDr;

void ww_PkInit(uint16_t spd)
{
    pk_t = ww_Rnd(3);
    pk_hits = 0;
    pk_shown = pk_drawn = -1;
    pk_anim = pk_sDr = false;
}

int16_t ww_PkUpdate(void)
{
    int16_t p = -1;

    if(pk_anim)                                                 // the attack is flying
    {
        pk_ax += 10;
        if(pk_ax >= 104)
        {
            pk_anim = false;
            Buzzer_PlaySfx(&sfx_explosion);
            if(++pk_hits >= 3) return MG_WIN;
            pk_t = (pk_t + 1 + ww_Rnd(2)) % 3;
        }
        return MG_PLAY;
    }

    if(Ui_Pressed(UI_ICON_U))      p = 0;                                     // Flamethrower (fire move)
    else if(Ui_Pressed(UI_ICON_D)) p = 1;                                     // Hydro Pump   (water move)
    else if(Ui_Pressed(UI_ICON_R)) p = 2;                                     // Razor Leaf   (grass move)
    if(p < 0) return MG_PLAY;

    if(p != kPkCounter[pk_t]) return MG_LOSE;                   // not very effective...
    pk_anim = true;  pk_mv = p;  pk_ax = 40;
    Buzzer_PlaySfx(&sfx_laser);
    return MG_PLAY;
}

static void PkStream(int16_t x, int16_t mv)                     // 28x14 attack graphic
{
    if(mv == 1)      { ww_Box(x, 88, 28, 10, COLOR_BLUE);   ww_Box(x + 3, 90, 22, 3, COLOR_CYAN); }          // Hydro Pump
    else if(mv == 0) { ww_Box(x, 86, 28, 14, COLOR_ORANGE); ww_Box(x + 4, 89, 20, 8, COLOR_YELLOW);
                       ww_Box(x + 22, 91, 6, 4, COLOR_RED); }                                            // Flamethrower
    else             { ww_Box(x, 88, 8, 5, COLOR_LIME);  ww_Box(x + 10, 92, 8, 5, COLOR_GREEN);
                       ww_Box(x + 20, 87, 8, 5, COLOR_LIME); ww_Box(x + 4, 95, 8, 4, COLOR_GREEN); }        // Razor Leaf
}

void ww_PkRender(void)
{
    if(ww_first)
    {
        ww_Box(6, 168, 100, 56, COLOR_BLUE);
        ww_Box(110, 168, 100, 56, COLOR_RED);
        ww_Box(214, 168, 100, 56, COLOR_GREEN);
        ww_Text("< HYDRO",   56, 186, COLOR_WHITE);  ww_Text("PUMP",    56, 208, COLOR_WHITE);
        ww_Text("^ FLAME",  160, 186, COLOR_WHITE);   ww_Text("THROWER", 160, 208, COLOR_WHITE);
        ww_Text("RAZOR >",  264, 186, COLOR_WHITE);   ww_Text("LEAF",   264, 208, COLOR_WHITE);
        pk_shown = pk_drawn = -1;
        pk_sDr = false;
    }
    if(pk_drawn != pk_t)
    {
        ww_DrawArt(136, 56, kPkArt[pk_t], 16, 16, 3, PK_BG, true);                 // 48x48, opaque
        ww_Box(100, 112, 120, 24, PK_BG);
        ww_Text(kPkName[pk_t], 160, 124, COLOR_WHITE);
        pk_drawn = pk_t;
    }
    if(pk_anim)
    {
        PkStream(pk_ax, pk_mv);                                 // new first, then clear what it left
        if(pk_sDr) ww_EraseOutside(pk_sx, 86, 28, 14, pk_ax, 86, 28, 14, PK_BG);
        pk_sx = pk_ax;  pk_sDr = true;
    }
    else if(pk_sDr) { ww_Box(pk_sx, 86, 28, 14, PK_BG); pk_sDr = false; }
    if(pk_shown != pk_hits)
    {
        ww_Box(80, 12, 160, 10, COLOR_DARK_GRAY);
        ww_Box(80, 12, (160 * (3 - pk_hits)) / 3, 10, COLOR_LIME);
        pk_shown = pk_hits;
    }
}
