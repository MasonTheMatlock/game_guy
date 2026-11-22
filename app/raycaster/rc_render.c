// rc_render.c - the 3D view: strip diffing, sprites, wall art, the held item.
#include "rc_priv.h"



static uint32_t IdColor(uint16_t id)
{
    if(id == ID_CEIL)  return rc_colCeil;
    if(id == ID_FLOOR) return rc_colFloor;
    if(id >= ID_THEME0 && id < ID_THEME0 + 16u)
    {
        id -= ID_THEME0;
        return rc_themeCol[id >> 3][(id >> 1) & 3u][id & 1u];
    }
    /* Wall art: gold trim, banner cloth, door frame, ember and flame. */
    if(id >= ID_BANNER0 && id < ID_BANNER0 + 8u)
    {
        static const uint32_t gold[4] = { 0x00DDB43Au, 0x00AC8C2Du, 0x00A4852Bu, 0x00806721u };
        id -= ID_BANNER0;
        return (id < 4u) ? gold[id] : rc_bannerCol[id - 4u];
    }
    if(id == ID_DOOR_FRAME) return 0x002B1A0Fu;
    if(id == ID_EMBER)      return 0x00E0501Au;
    if(id == ID_FLAME)      return 0x00FFD060u;
    /* Tiny procedural door textures: no bitmap assets or extra flash use. */
    if(id >= ID_TEX_WOOD0 && id <= 60u)
    {
        static const uint32_t wood[3] = { 0x00734A25u, 0x00905B2Du, 0x004A2D19u };
        return wood[id - ID_TEX_WOOD0];
    }
    if(id >= ID_TEX_MET0 && id <= 63u)
    {
        static const uint32_t steel[3] = { 0x006A747Cu, 0x0099A3AAu, 0x00343C43u };
        return steel[id - ID_TEX_MET0];
    }
    if(id < ID_SPR)  { id -= 2u;       return rc_wallCol[id >> 3][(id >> 1) & 3u][id & 1u]; }
    if(id < ID_TINT) return rc_sprRgb[id - ID_SPR];
    if(id < ID_WPN)  return rc_kTint[id - ID_TINT];
    return rc_wPal[id - ID_WPN];
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
        rc_sl[c][0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
        rc_sn[c]    = 1;
    }
    for(c = 0; c < RC_COLS; c++)
    {
        rc_rngT[c] = RC_VIEW_H;  rc_rngB[c] = -1;
        rc_prvT[c] = RC_VIEW_H;  rc_prvB[c] = -1;
    }
}


// Remember that rows y0..y1 of column c hold beam pixels this frame
// (they are repainted next frame because the LCD no longer matches the model).
static void AddRange(int16_t c, int16_t y0, int16_t y1)
{
    if(y0 < 0) y0 = 0;
    if(y1 > RC_VIEW_H - 1) y1 = RC_VIEW_H - 1;
    if(y0 < rc_rngT[c]) rc_rngT[c] = y0;
    if(y1 > rc_rngB[c]) rc_rngB[c] = y1;
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
    uint16_t *ol = rc_sl[si];
    uint16_t  on = rc_sn[si];
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
        rc_sn[si] = nn;
    }
    else                                    // too busy to remember: the tail repaints next frame
    {
        for(k = 0; k < RC_MAXR - 1; k++) ol[k] = nl[k];
        ol[RC_MAXR - 1] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
        rc_sn[si] = RC_MAXR;
        g_rcTrunc++;
    }
}


// Project one world point into the sprite list (sorted far to near).
static void ProjectOne(int32_t wx, int32_t wy, uint16_t shape, uint16_t pal,
                       uint16_t scale, uint16_t zoff, uint16_t tint)
{
    Spr     *sp;
    int16_t  rx, ry, along, side, ratio, theta, cx, x0, x1, lo, hi;
    uint16_t perp8, lineH, size, k;
    int16_t  floorY, bottom;

    if(rc_nSpr >= RC_SPR_CAP) return;

    rx    = RD(wx - rc_px);
    ry    = RD(wy - rc_py);
    along = (int16_t)(((int32_t)rx * rc_projC + (int32_t)ry * rc_projS) >> 14);
    if(along < 150) return;                                 // behind / too close

    side  = (int16_t)(((int32_t)ry * rc_projC - (int32_t)rx * rc_projS) >> 14);
    ratio = (int16_t)(((int32_t)side * 256L) / along);
    theta = (ratio < 0) ? -ratio : ratio;
    if(theta > 256) return;                                 // way outside the view
    theta = rc_atan[theta];
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

    sp = &rc_spr[rc_nSpr];
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
    k = rc_nSpr++;
    while(k > 0 && rc_spr[k - 1].dist < rc_spr[k].dist)
    {
        Spr tmp = rc_spr[k - 1];
        rc_spr[k - 1] = rc_spr[k];
        rc_spr[k] = tmp;
        k--;
    }
}


// How an enemy projectile (OB_BOLT, flavour in aux) is drawn: shape, palette, scale.
static const uint16_t kBoltLook[BK_COUNT][3] =
{
    { SH_BOLT, PT_BOLT,       3 },      // BK_LASER  (Watcher)
    { SH_PUFF, PT_PUFF_FIRE,  4 },      // BK_FLAME  (Dragon)
    { SH_ORB,  PT_ORB_PURPLE, 5 },      // BK_ORB    (Necromancer, Lich, Beholder)
    { SH_ORB,  PT_ORB_BLOOD,  5 },      // BK_DRAIN  (Vampire Lord)
    { SH_WEB,  PT_WEB,        6 },      // BK_WEB    (Spider)
    { SH_ORB,  PT_ORB_ICE,    5 },      // BK_ICE    (Beholder ice eye)
    { SH_ORB,  PT_ORB_FIRE,   5 },      // BK_FIRE   (Beholder fire eye)
    { SH_ORB,  PT_ORB_BOLT,   4 },      // BK_SPARK  (Beholder lightning eye)
    { SH_BOLT, PT_SKELETON,   3 },      // BK_ARROW  (Skeleton Archer)
    { SH_ORB,  PT_ROCK,       6 },      // BK_ROCK   (Earth Golem)
    { SH_ORB,  PT_ORB_GREEN,  5 },      // BK_DEATH  (Lich Lord)
    { SH_ORB,  PT_SLIME,      5 },      // BK_GLOB   (Slime King)
    { SH_ORB,  PT_ORB_FIRE,   8 }       // BK_FIREBALL (Dragon, Fire Golem)
};

static uint16_t DecalScale(int16_t r)           // ring / puddle of radius r (Q10) -> sprite scale
{
    uint16_t sc = (uint16_t)(r >> 5);
    return (sc < 4u) ? 4u : (sc > 150u) ? 150u : sc;
}


// Turn every enemy and object into a screen-space sprite.
static void ProjectSprites(void)
{
    uint16_t i;

    rc_nSpr  = 0;
    rc_projC = COS(rc_ang);
    rc_projS = SIN(rc_ang);

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        const Enemy    *e = &rc_en[i];
        const EnemyDef *d;
        uint16_t        shape, pal, tint;

        if(!e->alive || (e->type == ET_SLIMEKING && rc_Untargetable(e))) continue;     // split king is hidden
        d = &rc_kEnemy[e->type];
        rc_EnemyLook(e, &shape, &pal);
        tint = e->flash ? 1u : (e->freeze && rc_GoldStatues()) ? 4u : e->freeze ? 3u :
               (e->burn && (e->burn & 8u)) ? 2u : 0u;
        ProjectOne(e->x, e->y, shape, pal, d->scale, d->zoff, tint);
    }

    for(i = 0; i < RC_MAX_OBJ; i++)
    {
        const Obj *o = &rc_obj[i];

        if(!o->alive) continue;
        switch(o->kind)
        {
            case OB_FLASK:
                ProjectOne(o->x, o->y, SH_FLASK, o->pal, 4, (uint16_t)o->z, 0);
                break;

            case OB_BOLT:
                ProjectOne(o->x, o->y, kBoltLook[o->aux][0], kBoltLook[o->aux][1],
                           kBoltLook[o->aux][2], (uint16_t)o->z, 0);
                break;

            case OB_MANA:
                if(o->life - o->age < 90u && (o->age & 4u)) break;      // blink before it vanishes
                ProjectOne(o->x, o->y, SH_FLASK, PT_FLASK_MANA, 5, (uint16_t)o->z, 0);
                break;

            case OB_WARN:
                if(o->age & 4u) break;                                  // blinking warning ring
                ProjectOne(o->x, o->y, SH_WARN, PT_WARN, DecalScale(o->r), 0, 0);
                break;

            case OB_AREA:
                if(o->life - o->age < 30u && (o->age & 4u)) break;      // fading out
                ProjectOne(o->x, o->y, SH_PUDDLE,
                           (o->aux == AR_FIRE) ? PT_PUDDLE_FIRE : (o->aux == AR_BLAST) ? PT_PUDDLE_BLAST :
                           (o->aux == AR_WEB)  ? PT_PUDDLE_WEB  : (o->aux == AR_ARCANE) ? PT_WAVE_NOVA :
                           PT_PUDDLE_SLIME,
                           DecalScale(o->r), 0, 0);
                break;

            case OB_WAVE:
                ProjectOne(o->x, o->y, SH_WAVE, PT_WAVE_SLIME + o->aux, DecalScale(o->r), 0, 0);
                break;

            case OB_SING:                               // Eldritch Singularity: a pulsing dark orb
                ProjectOne(o->x, o->y, SH_ORB, PT_WAVE_NOVA, (uint16_t)(14 + ((o->age >> 2) & 3u)), 6, 0);
                break;

            case OB_HEART:
                if(o->life - o->age < 90u && (o->age & 4u)) break;
                ProjectOne(o->x, o->y, SH_HEART, PT_HEART, 6, (uint16_t)(o->z + 1), 0);
                break;

            default:                                    // cloud / spark
                ProjectOne(o->x, o->y, SH_PUFF, o->pal,
                           (uint16_t)(o->sc + ((o->age >> 1) & 3u)),
                           (uint16_t)o->z, 0);
                break;
        }
    }

    // Floor marks: flat decals that flash faster as the hit gets close.
    for(i = 0; i < rc_nMark; i++)
    {
        const Mark *m = &rc_mark[i];
        uint16_t    half = (m->life > 20u) ? 4u : 2u;

        if(!(m->flags & MK_STEADY) && ((m->life / half) & 1u)) continue;
        if(m->flags & MK_SPIKE) ProjectOne(TILE_C(m->tx), TILE_C(m->ty), SH_SPIKE, m->pal, 10, 0, 0);
        else                    ProjectOne(TILE_C(m->tx), TILE_C(m->ty), SH_PUDDLE, m->pal, 30, 0, 0);
    }

    // Props: chests, vendors, keys and altars (a Mimic chest looks like a regular one).
    for(i = 0; i < rc_nProp; i++)
    {
        const Prop *p = &rc_prop[i];

        if(!p->alive) continue;
        switch(p->kind)
        {
            case PR_CHEST:
                if(p->used) break;
                ProjectOne(p->x, p->y, SH_CHEST,
                           (p->sub == CH_CURSED) ? PT_CHEST_CURSED : (p->sub == CH_GOOD) ? PT_CHEST_GOOD : PT_CHEST,
                           10, 0, 0);
                break;
            case PR_VENDOR:
                ProjectOne(p->x, p->y, SH_MERCHANT, PT_MERCHANT, 15, 0, 0);
                break;
            case PR_KEY:
                if(p->used) break;
                ProjectOne(p->x, p->y, SH_KEY, PT_KEY, 6, (uint16_t)((rc_portalPhase >> 3) & 1u), 0);
                break;
            case PR_ALTAR_SHRINE:
            case PR_ALTAR_LAB:
                ProjectOne(p->x, p->y, SH_ORB, p->used ? PT_PUFF_ARCANE : PT_ALTAR, 8, 4, 0);
                break;
            default:
                break;
        }
    }
}


// Paint one sprite's pixel column (the one under screen x = xc) into a strip.
// The 16 art rows are scanned here, so a column may hold any number of colour runs.
static void PaintSlice(uint16_t *nl, uint16_t *nn, const Spr *sp, int16_t xc)
{
    const uint16_t *art = rc_kArt[sp->shape];
    uint16_t u, y, y1, id, px, sh, wi;
    int16_t  xs = (int16_t)(xc - sp->x0);

    if(xs < 0) xs = 0;
    u = (uint16_t)(((int32_t)xs * sp->uStep) >> 8);
    if(u > SPR_N - 1) u = SPR_N - 1;
    wi = (uint16_t)(u >> 2);                            // which word of a row holds this column
    sh = (uint16_t)((u & 3u) << 2);                     // and which nibble of it

    y = 0;
    while(y < SPR_N)
    {
        int16_t py0, py1;

        px = (uint16_t)((art[(y << 2) + wi] >> sh) & 15u);
        if(px == 0) { y++; continue; }

        y1 = y;
        while(y1 + 1 < SPR_N && (uint16_t)((art[((y1 + 1) << 2) + wi] >> sh) & 15u) == px) y1++;

        py0 = (int16_t)(sp->top + (int16_t)(((int32_t)y * sp->size) >> 4));
        py1 = (int16_t)(sp->top + (int16_t)(((int32_t)(y1 + 1) * sp->size) >> 4) - 1);
        if(py0 < 0) py0 = 0;
        if(py1 > RC_VIEW_H - 1) py1 = RC_VIEW_H - 1;
        if(py1 >= py0)
        {
            id = sp->flash ? (uint16_t)(ID_TINT + sp->flash)
                           : rc_palId[sp->type][px - 1u];
            ListPaint(nl, nn, RC_MAXN, py0, py1, id);
        }
        y = y1 + 1;
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
// Cell code (0..12) of held-art cell (x, y): rows 0..9 are the item, 10..17 the shared hand.
static uint16_t WpnCell(const uint16_t *art, uint16_t x, uint16_t y)
{
    const uint16_t *row = (y < WPN_ART_ROWS) ? &art[y * 5u] : &rc_kWpnArm[(y - WPN_ART_ROWS) * 5u];
    return (uint16_t)((row[x >> 2] >> ((x & 3u) << 2)) & 15u);
}


// Build the held art of item `it` (a staff or consumable): its art rows and its palette.
static void WpnBuild(uint16_t it)
{
    uint16_t k, y, y1, n, i, ch;
    const uint16_t *art = rc_kWpnArt[rc_kItems[it].art];
    const uint32_t *pal = rc_kWpnPal[rc_kItems[it].pal];

    for(i = 0; i < WPN_SYMS; i++) rc_wPal[i] = pal[i];

    for(k = 0; k < WPN_W; k++)                          // vertical runs per art column
    {
        n = 0;
        y = 0;
        while(y < WPN_H)
        {
            ch = WpnCell(art, k, y);
            if(ch == 0u) { y++; continue; }               // transparent

            y1 = y;
            while(y1 + 1 < WPN_H && WpnCell(art, k, y1 + 1) == ch) y1++;

            if(n < WPN_MAXRUNS)
                rc_wRun[k][n++] = (uint16_t)(y | (y1 << 5) | (ch << 10));
            y = y1 + 1;
        }
        rc_wRunN[k] = n;
    }
    rc_wLoaded = it;
}


// Paint the held item's art column ak (0..WPN_W-1) into a strip list.
static void PaintWeapon(uint16_t *nl, uint16_t *nn, uint16_t ak)
{
    uint16_t r, v;

    for(r = 0; r < rc_wRunN[ak]; r++)
    {
        v = rc_wRun[ak][r];
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

    if(rc_fxTimer == 4)                              // muzzle flash
    {
        Graphics_FillRect(tx - 4, ty - 4, tx + 4, ty + 4, rc_fxColor);
        Graphics_FillRect(tx - 2, ty - 2, tx + 2, ty + 2, C_WHITE);
        MarkRect(tx - 4, ty - 4, tx + 4, ty + 4);
    }

    for(b = 0; b < rc_fxN; b++)
    {
        ex = rc_fxX[b];

        if(rc_fxTimer >= 2)                          // bolt in flight
        {
            t0 = (rc_fxTimer == 4) ?   0u : (rc_fxTimer == 3) ?  80u : 170u;
            t1 = (rc_fxTimer == 4) ? 100u : (rc_fxTimer == 3) ? 190u : 256u;

            for(k = 0; k < 3; k++)
            {
                t  = t0 + ((t1 - t0) * k) / 2u;
                px = (int16_t)(tx + (int16_t)(((int32_t)(ex - tx) * (int32_t)t) >> 8));
                py = (int16_t)(ty + (int16_t)(((int32_t)(cy - ty) * (int32_t)t) >> 8));
                FxDot(px, py, rc_fxColor);
            }
        }

        if(rc_fxTimer <= 2)                          // impact burst
        {
            int16_t r = (rc_fxTimer == 2) ? 6 : 3;

            Graphics_FillRect(ex - r, cy - 1, ex + r, cy + 1, rc_fxColor);
            Graphics_FillRect(ex - 1, cy - r, ex + 1, cy + r, rc_fxColor);
            Graphics_FillRect(ex - 1, cy - 1, ex + 1, cy + 1, C_WHITE);
            MarkRect(ex - r, cy - r, ex + r, cy + r);
        }
    }
}


// Where a ray hit the wall: u = distance along the wall face, v = the wall's own tile row / column.
static void WallUV(int16_t ang, uint16_t dist, uint16_t side, uint16_t *u, uint16_t *v)
{
    int32_t hx = rc_px + (((int32_t)COS(ang) * (int32_t)dist) >> 12);
    int32_t hy = rc_py + (((int32_t)SIN(ang) * (int32_t)dist) >> 12);

    if(side == 0) { *u = (uint16_t)hy & 0x7FFFu; *v = (uint16_t)(hx >> 10) & 63u; }
    else          { *u = (uint16_t)hx & 0x7FFFu; *v = (uint16_t)(hy >> 10) & 63u; }
}


// Paint stone blocks (running bond) plus the current boss theme's pattern over a plain wall strip.
// Only close walls get the pattern; far walls stay flat so they don't shimmer.
static void WallDecor(uint16_t *nl, uint16_t *nn, int16_t top, uint16_t lineH,
                      uint16_t u, uint16_t v, uint16_t shade, uint16_t side)
{
    uint16_t mortar = (uint16_t)(ID_THEME0 + shade * 2u + side);
    uint16_t accent = (uint16_t)(ID_THEME0 + 8u + shade * 2u + side);
    uint16_t blk0   = (uint16_t)((u >> 9) * 5u + v * 13u + side * 3u);
    uint16_t blk1   = (uint16_t)(((u + 256u) >> 9) * 7u + v * 11u + side * 5u);
    int16_t  mid    = (int16_t)(top + (int16_t)(lineH >> 1));
    int16_t  bot    = (int16_t)(top + (int16_t)lineH - 1);
    int16_t  seam   = (lineH >= 60u) ? 1 : 0;           // seam is 1 or 2 rows thick
    int16_t  y0, y1;
    uint16_t pu = (uint16_t)(u & 63u), q = (uint16_t)((u >> 6) + v * 3u);

#if !RC_WALL_DECOR
    (void)nl; (void)nn; (void)top; (void)lineH; (void)u; (void)v; (void)shade; (void)side;
    return;
#endif
    if(shade > 1u || lineH < 40u) return;               // far away: plain colour (thin lines would only shimmer)

    // moss / stained blocks, then the vertical seams, then the horizontal seam
    if((blk0 % 9u) == 0u) ListPaint(nl, nn, RC_MAXN, top, (int16_t)(mid - seam - 1), accent);
    if((blk1 % 9u) == 4u) ListPaint(nl, nn, RC_MAXN, (int16_t)(mid + 1), bot, accent);
    if((u & 511u) < 52u)                 ListPaint(nl, nn, RC_MAXN, top, (int16_t)(mid - seam - 1), mortar);
    if(((u + 256u) & 511u) < 52u)        ListPaint(nl, nn, RC_MAXN, (int16_t)(mid + 1), bot, mortar);
    ListPaint(nl, nn, RC_MAXN, (int16_t)(mid - seam), (int16_t)(mid + seam), mortar);

    switch(rc_kTheme[rc_theme].pat)
    {
        case PAT_DRIPS:                                 // slime drips from the top
            if((q & 3u) == 0u && pu < 20u)
            {
                y1 = (int16_t)(top + (int16_t)(((uint32_t)lineH * (22u + (q * 7u) % 40u)) / 100u));
                ListPaint(nl, nn, RC_MAXN, top, y1, accent);
            }
            break;

        case PAT_CRACKS:                                // glowing cracks
        case PAT_LAVA:                                  // lava veins
            if(((q % ((rc_kTheme[rc_theme].pat == PAT_LAVA) ? 3u : 5u)) == 0u) && pu < 9u)
            {
                y0 = (int16_t)(top + (int16_t)(((uint32_t)lineH * ((pu < 4u) ? 12u : 24u)) / 100u));
                y1 = (int16_t)(top + (int16_t)(((uint32_t)lineH * ((pu < 4u) ? 80u : 68u)) / 100u));
                ListPaint(nl, nn, RC_MAXN, y0, y1, accent);
            }
            break;

        case PAT_VEINS:                                 // blood veins
            if(((q % 6u) == 1u) && pu < 7u)
                ListPaint(nl, nn, RC_MAXN, top, bot, accent);
            break;

        case PAT_EYES:                                  // watching eyes / stars
            if((blk0 % 4u) == 1u && (u & 511u) > 190u && (u & 511u) < 322u)
            {
                y0 = (int16_t)(top + (int16_t)(((uint32_t)lineH * 22u) / 100u));
                y1 = (int16_t)(top + (int16_t)(((uint32_t)lineH * 38u) / 100u));
                ListPaint(nl, nn, RC_MAXN, y0, y1, accent);
            }
            break;

        case PAT_BANDS:                                 // bone bands
            y0 = (int16_t)(top + (int16_t)(((uint32_t)lineH * 12u) / 100u));
            ListPaint(nl, nn, RC_MAXN, y0, (int16_t)(y0 + 1 + seam), accent);
            y0 = (int16_t)(top + (int16_t)(((uint32_t)lineH * 82u) / 100u));
            ListPaint(nl, nn, RC_MAXN, y0, (int16_t)(y0 + 1 + seam), accent);
            break;

        default:
            break;
    }
}


// ==== WALL ART BEGIN =========================================================
// Pillars get a hanging banner (three emblems) or a wall torch, chosen by the pillar's map cell so
// all four faces of one pillar match. Doors are arched, bound with iron bands and studs; the
// Dragon Den door has a glowing dragon eye. Everything is a few ListPaint calls per strip, drawn
// only on close walls (far walls stay flat so thin detail would not shimmer). All art is
// left/right symmetric, so it looks right from either side.
//
// Speed notes (C28x has no fast divide): every percentage is turned into a row with one multiply
// and a shift, and every proportional size uses a 16-bit reciprocal (MULQ16). All intermediate
// values fit in 16 bits except where a uint32_t is spelled out, because int is 16 bits here.
#if RC_WALL_ART
#define FR(pct)       ((int16_t)(top + (int16_t)(((uint32_t)(uint16_t)(lineH * (uint16_t)(pct)) * 5243UL) >> 19)))
#define MULQ16(a, m)  ((uint16_t)(((uint32_t)(a) * (m)) >> 16))
#define LPT(y0, y1, id)  ListPaint(nl, nn, RC_MAXN, (y0), (y1), (id))
#define ID_STEEL_DK   ((uint16_t)(ID_TEX_MET0 + 2u))
#define ID_STEEL_LT   ((uint16_t)(ID_TEX_MET0 + 1u))
#define ID_BRASS      ((uint16_t)ID_BANNER0)
#endif


// Where along its door the ray hit, and how wide the door is. A door is a run of identical map
// characters along the face the ray hit (a 1-tile hallway door gives 1024, a 3-tile door 3072).
// Cheap: at most a few map reads, and only for door strips.
static void DoorSpan(uint16_t side, uint16_t u, uint16_t *loc, uint16_t *span)
{
    int16_t  cx = rc_rayCx, cy = rc_rayCy, p;
    uint16_t k = 0, a = 0;
    char     ch;

    *loc  = (uint16_t)(u & 1023u);
    *span = 1024u;
    if((uint16_t)cx >= RC_MAP_W || (uint16_t)cy >= RC_MAP_H) return;
    ch = rc_MapCh(cx, cy);

    if(side == 1u)                                          // a y-face: the door runs along x
    {
        for(p = (int16_t)(cx - 1); p >= 0 && rc_MapCh(p, cy) == ch; p--) k++;
        for(p = (int16_t)(cx + 1); p < RC_MAP_W && rc_MapCh(p, cy) == ch; p++) a++;
    }
    else                                                    // an x-face: the door runs along y
    {
        for(p = (int16_t)(cy - 1); p >= 0 && rc_MapCh(cx, p) == ch; p--) k++;
        for(p = (int16_t)(cy + 1); p < RC_MAP_H && rc_MapCh(cx, p) == ch; p++) a++;
    }
    *loc  = (uint16_t)(k * 1024u + (u & 1023u));
    *span = (uint16_t)((k + 1u + a) * 1024u);
}


#if RC_WALL_ART
// Secret doors (map 's') wear a narrow dark banner with a small brass padlock: subtle,
// only drawn up close, and centred on the tile face so it reads from either side.
static void SecretArt(uint16_t *nl, uint16_t *nn, int16_t top, uint16_t lineH,
                      uint16_t u, uint16_t shade, uint16_t side)
{
    uint16_t uu  = (uint16_t)(u & 1023u);
    uint16_t dx  = (uu >= 512u) ? (uint16_t)(uu - 512u) : (uint16_t)(512u - uu);
    uint16_t ink = (uint16_t)(ID_THEME0 + shade * 2u + side);          // the theme's mortar: a dark cloth

    if(shade > RC_ART_MAX_SHADE || lineH < 40u || dx >= 150u) return;
    LPT(FR(18), FR(70) - 1, ink);                                       // banner
    if(dx < 46u)  LPT(FR(34), FR(41) - 1, ID_STEEL_DK);                 // shackle
    if(dx >= 26u && dx < 46u) LPT(FR(41), FR(44) - 1, ID_STEEL_DK);
    if(dx < 56u)  LPT(FR(44), FR(58) - 1, ID_BRASS);                    // lock body
    if(dx < 10u)  LPT(FR(48), FR(54) - 1, ID_DOOR_FRAME);               // keyhole
}


static void PillarArt(uint16_t *nl, uint16_t *nn, int16_t top, uint16_t lineH,
                      uint16_t u, uint16_t shade, uint16_t side)
{
    uint16_t uu     = (uint16_t)(u & 1023u);
    uint16_t dx     = (uu >= 512u) ? (uint16_t)(uu - 512u) : (uint16_t)(512u - uu);
    uint16_t gold   = (uint16_t)(ID_BANNER0 + shade * 2u + side);
    uint16_t cloth  = (uint16_t)(ID_BANNER0 + 4u + shade * 2u + side);
    uint16_t ink    = (uint16_t)(ID_THEME0 + 8u + shade * 2u + side);       // theme accent
    uint16_t design = (uint16_t)(((uint16_t)rc_rayCx * 3u + (uint16_t)rc_rayCy * 5u) & 3u);
    uint16_t bF, hh;

    if(shade > RC_ART_MAX_SHADE || lineH < 40u) return;

    if(design == 3u)                                    // wall torch
    {
        if(dx < 20u) LPT(FR(46), FR(70) - 1, ID_STEEL_DK);              // iron post
        if(dx < 56u) LPT(FR(58), FR(65) - 1, ID_STEEL_DK);              // cup
        if(dx < 52u)
        {
            hh = (uint16_t)((52u - dx) >> 1);                           // 26 * (52 - dx) / 52
            LPT(FR(48u - hh), FR(58) - 1, ID_EMBER);                    // flame
        }
        if(dx < 26u)
        {
            hh = MULQ16(26u - dx, 45371u);                              // 18 * (26 - dx) / 26
            LPT(FR(53u - hh), FR(58) - 1, ID_FLAME);                    // hot core
        }
        return;
    }

    if(dx < 290u) LPT(FR(5), FR(8) - 1, ID_STEEL_DK);                   // hanging rod
    if(dx >= 260u) return;

    bF = (uint16_t)(62u + MULQ16(260u - dx, 5041u));                    // pointed bottom edge
    LPT(FR(8), FR(bF) - 1, cloth);
    if(dx >= 226u)
        LPT(FR(8), FR(bF) - 1, gold);                                   // side trim
    else
    {
        LPT(FR(8), FR(11) - 1, gold);                                   // top trim
        LPT(FR(bF - 3u), FR(bF) - 1, gold);                             // trim along the point
    }

    switch(design)
    {
        case 0u:                                                        // sword
            if(dx < 20u) LPT(FR(20), FR(66) - 1, ink);                  // blade and grip
            if(dx < 70u) LPT(FR(51), FR(56) - 1, ink);                  // cross guard
            if(dx < 28u) LPT(FR(66), FR(71) - 1, ink);                  // pommel
            break;

        case 1u:                                                        // diamond
            if(dx < 84u)
            {
                hh = MULQ16(84u - dx, 14043u);                          // 18 * (84 - dx) / 84
                LPT(FR(40u - hh), FR(41u + hh) - 1, ink);
            }
            break;

        default:                                                        // rune cross
            if(dx < 22u) LPT(FR(22), FR(60) - 1, ink);
            if(dx < 70u) LPT(FR(32), FR(39) - 1, ink);
            break;
    }
}


// Rounded top: how many percent of the door height the stone frame cuts off at distance dx
// from the door's centre (0 in the middle, about 24 at the corners of a 3-tile door).
static uint16_t ArchCut(uint16_t dx)
{
    uint32_t t, c;

    if(dx <= 700u) return 0u;
    t = (uint32_t)(dx - 700u);
    c = (t * t * 36UL) >> 20;                               // t * t / 29127
    return (c > 30UL) ? 30u : (uint16_t)c;
}


// Wooden door: stone arch, dark frame, two iron bands with brass studs, brass handle plates and,
// on a double door, a centre seam. Planks (the base colour) come from the caller.
static void DoorWood(uint16_t *nl, uint16_t *nn, int16_t top, uint16_t lineH,
                     uint16_t L, uint16_t span, uint16_t shade, uint16_t side)
{
    uint16_t mid    = (uint16_t)(span >> 1);
    uint16_t dx     = (L >= mid) ? (uint16_t)(L - mid) : (uint16_t)(mid - L);
    uint16_t cut    = ArchCut(dx), dr, k;
    bool     dbl    = (span > 1536u);                       // double door, or a single narrow leaf
    int16_t  bot    = (int16_t)(top + (int16_t)lineH - 1);

    if(cut) LPT(top, FR(cut) - 1, (uint16_t)ID_WALL(1u, shade, side));  // stone beside the arch
    LPT(FR(cut), FR(cut + 4u) - 1, ID_DOOR_FRAME);                      // arch rim

    if(L < 100u || L > (uint16_t)(span - 100u) || (dbl && dx < 30u))    // side posts, centre seam
    {
        LPT(FR(cut + 4u), bot, ID_DOOR_FRAME);
        return;
    }

    LPT(FR(22), FR(29) - 1, ID_STEEL_DK);                               // iron bands
    LPT(FR(72), FR(79) - 1, ID_STEEL_DK);
    if((L & 255u) >= 96u && (L & 255u) < 160u)                          // brass studs
    {
        LPT(FR(24), FR(27) - 1, ID_BRASS);
        LPT(FR(74), FR(77) - 1, ID_BRASS);
    }

    for(k = 0; k < (dbl ? 2u : 1u); k++)                                // handle plates
    {
        uint16_t c = !dbl ? (uint16_t)(span - 230u)
                          : (k == 0u) ? (uint16_t)(mid - 190u) : (uint16_t)(mid + 190u);

        dr = (L >= c) ? (uint16_t)(L - c) : (uint16_t)(c - L);
        if(dr >= 44u) continue;
        LPT(FR(44), FR(64) - 1, ID_BRASS);                              // brass plate
        if(dr < 16u) LPT(FR(50), FR(58) - 1, ID_DOOR_FRAME);            // with a dark hole
    }
}


// Dragon Den door: riveted steel with a glowing dragon eye; the centre seam is its pupil.
static void DoorSteel(uint16_t *nl, uint16_t *nn, int16_t top, uint16_t lineH,
                      uint16_t L, uint16_t span, uint16_t shade, uint16_t side)
{
    uint16_t mid = (uint16_t)(span >> 1);
    uint16_t dx  = (L >= mid) ? (uint16_t)(L - mid) : (uint16_t)(mid - L);
    uint16_t cut = ArchCut(dx), hh;
    int16_t  bot = (int16_t)(top + (int16_t)lineH - 1);

    if(cut) LPT(top, FR(cut) - 1, (uint16_t)ID_WALL(1u, shade, side));
    LPT(FR(cut), FR(cut + 4u) - 1, ID_STEEL_DK);

    if(L < 100u || L > (uint16_t)(span - 100u))
    {
        LPT(FR(cut + 4u), bot, ID_STEEL_DK);
        return;
    }

    LPT(FR(14), FR(21) - 1, ID_STEEL_DK);                               // heavy cross bars
    LPT(FR(79), FR(86) - 1, ID_STEEL_DK);
    if((L & 255u) >= 96u && (L & 255u) < 160u)
        LPT(FR(16), FR(19) - 1, ID_STEEL_LT);                           // rivets
    if(dx < 14u) LPT(FR(cut + 4u), bot, ID_DOOR_FRAME);                 // centre seam

    if(dx < 300u)                                                       // the eye
    {
        hh = MULQ16(300u - dx, 8738u);                                  // 40 * (300 - dx) / 300
        LPT(FR(50u - hh), FR(51u + hh) - 1, ID_EMBER);
        if(dx < 110u)
        {
            hh = MULQ16(110u - dx, 13107u);                             // 22 * (110 - dx) / 110
            LPT(FR(50u - hh), FR(51u + hh) - 1, ID_FLAME);
        }
        if(dx < 22u) LPT(FR(34), FR(66) - 1, ID_DOOR_FRAME);            // slit pupil
    }
}
#endif
// ==== WALL ART END ===========================================================


void rc_RenderView(void)
{
    uint16_t c, k, tile, side, dist, perp, lineH, shade, h, nStrips, nn, si, wallId;
    uint16_t dL = 0, dSpan = 0;
    bool     isDoor = false;
    int16_t  top, bot, sx, w, ak;
    uint16_t nl[RC_MAXN];
    int16_t  cxp = RC_VIEW_X0 + RC_VIEW_W / 2;
    int16_t  cyp = RC_VIEW_Y0 + RC_HORIZON;

    if(rc_invalidate)
    {
        InvalidateView();
        rc_invalidate = false;
    }

    if(rc_wLoaded != rc_SelItem())
    {
        WpnBuild(rc_SelItem());          // new art AND palette: repaint the strips behind the hand
        for(c = WPN_C0; c <= WPN_C1; c++)
            for(h = 0; h < 1; h++)
            {
                si = StripIndex(c, h);
                rc_sl[si][0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
                rc_sn[si]    = 1;
            }
    }

    // Beam overlay bookkeeping: last frame's overlay rows must be repainted.
    for(c = 0; c < RC_COLS; c++)
    {
        rc_prvT[c] = rc_rngT[c];  rc_prvB[c] = rc_rngB[c];
        rc_rngT[c] = RC_VIEW_H;  rc_rngB[c] = -1;
    }

    ProjectSprites();

    for(c = 0; c < RC_COLS; c++)
    {
        dist = rc_RC_CastRay(rc_ang + rc_colOff[c], &tile, &side);
        perp = (uint16_t)(((uint32_t)dist * (uint16_t)rc_colCos[c]) >> 14);
        if(perp < RC_MIN_DIST) perp = RC_MIN_DIST;

        lineH = RC_PROJ / perp;
        lineH &= ~1u;                                   // even heights: fewer changed rows
        if(lineH > RC_VIEW_H) lineH = RC_VIEW_H;
        if(lineH < 2)         lineH = 2;

        top = (int16_t)((RC_VIEW_H - lineH) >> 1);
        bot = (int16_t)(top + lineH - 1);

        shade  = (perp < 512) ? 0 : (perp < 1024) ? 1 : (perp < 1792) ? 2 : 3;
        if(tile == TILE_PORTAL && rc_portalPhase) tile = TILE_PORTAL + 1u;   // pulse

        /* Door faces: planks / steel panels plus the art above. The pattern is tied to the position
         * along the door (not the screen column), so it moves with the door when you turn. Far away
         * the door is one flat colour, because thin detail would only shimmer. */
        isDoor = (tile == TILE_DOOR || tile == TILE_METAL_DOOR);
        if(isDoor)
        {
            uint16_t du, dv;

            WallUV((int16_t)(rc_ang + rc_colOff[c]), dist, side, &du, &dv);
            (void)dv;
            DoorSpan(side, du, &dL, &dSpan);
            if(tile == TILE_DOOR)
                wallId = (uint16_t)(ID_TEX_WOOD0 + ((shade > 1u) ? 1u :
                                     ((dL & 127u) < 14u) ? 2u : (((dL >> 7) & 1u) ? 0u : 1u)));
            else
                wallId = (uint16_t)(ID_TEX_MET0 + ((shade > 1u) ? 0u :
                                     ((dL & 255u) < 18u) ? 2u : (((dL & 255u) >= 128u && (dL & 255u) < 146u) ? 1u : 0u)));
        }
        else
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

            if(tile == TILE_WALL_STONE)
            {
                uint16_t wu, wv;

                WallUV((int16_t)(rc_ang + rc_colOff[c]), dist, side, &wu, &wv);
                WallDecor(nl, &nn, top, lineH, wu, wv, shade, side);
#if RC_WALL_ART
                if((uint16_t)rc_rayCx < RC_MAP_W && (uint16_t)rc_rayCy < RC_MAP_H &&
                   rc_MapCh((uint16_t)rc_rayCx, (uint16_t)rc_rayCy) == 's')
                    SecretArt(nl, &nn, top, lineH, wu, shade, side);
#endif
            }
#if RC_WALL_ART
            else if(isDoor && shade <= RC_ART_MAX_SHADE && lineH >= 40u)
            {
                if(tile == TILE_DOOR) DoorWood(nl, &nn, top, lineH, dL, dSpan, shade, side);
                else                  DoorSteel(nl, &nn, top, lineH, dL, dSpan, shade, side);
            }
            else if(tile == 2u && shade <= RC_ART_MAX_SHADE && lineH >= 40u)   // pillar
            {
                uint16_t pu, pv;

                WallUV((int16_t)(rc_ang + rc_colOff[c]), dist, side, &pu, &pv);
                (void)pv;
                PillarArt(nl, &nn, top, lineH, pu, shade, side);
            }
#endif

            // sprites, far to near, hidden behind nearer wall
            for(k = 0; k < rc_nSpr; k++)
                if(c >= rc_spr[k].c0 && c <= rc_spr[k].c1 && rc_spr[k].dist < perp)
                    PaintSlice(nl, &nn, &rc_spr[k], (int16_t)(sx + (w >> 1)));

            // held item on top of everything
            if((int16_t)c >= WPN_C0 && (int16_t)c <= WPN_C1)
            {
                ak = (int16_t)((int16_t)c - WPN_C0);        // art column == strip
                if(ak < WPN_W) PaintWeapon(nl, &nn, (uint16_t)ak);
            }

            // beam pixels from last frame are not in the model: repaint them
            if(rc_prvT[c] <= rc_prvB[c])
            {
                uint16_t *ol = rc_sl[si];
                uint16_t  on = rc_sn[si];

                if(on == 0 || !ListPaint(ol, &on, RC_MAXR, rc_prvT[c], rc_prvB[c], ID_UNKNOWN))
                {
                    ol[0] = RUN_MAKE(RC_VIEW_H - 1, ID_UNKNOWN);
                    on = 1;
                }
                rc_sn[si] = on;
            }

            EmitStrip(si, sx, w, nl, nn);
        }
    }

    // Overlays: staff bolts, then the crosshair.
    if(rc_fxTimer) DrawFx();

    if(rc_mode == MODE_PLAY)
    {
        Graphics_FillRect(cxp - 5, cyp, cxp + 5, cyp, COLOR_SILVER);
        Graphics_FillRect(cxp, cyp - 5, cxp, cyp + 5, COLOR_SILVER);
    }
}
