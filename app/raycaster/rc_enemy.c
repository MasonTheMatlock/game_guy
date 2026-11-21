// rc_enemy.c - enemy AI: path-finding, movement helpers and per-type behaviour.
#include "rc_priv.h"



static void BuildFlow(void)
{
    uint16_t i, head = 0, tail = 0, cell, nb;
    int16_t tx = (int16_t)(rc_px >> 10);
    int16_t ty = (int16_t)(rc_py >> 10);
    int16_t x, y, nx, ny;

    for(i = 0; i < RC_MAP_CELLS; i++) rc_flow[i] = 0xFFFFu;
    rc_flowTx = tx;
    rc_flowTy = ty;
    if((uint16_t)tx >= RC_MAP_W || (uint16_t)ty >= RC_MAP_H) return;

    cell = (uint16_t)(((uint16_t)ty << RC_MAP_SHIFT) + (uint16_t)tx);
    rc_flow[cell] = 0;
    rc_flowQ[tail++] = cell;

    /* Queue BFS avoids the old repeated full-map scans; important on 32x32. */
    while(head < tail)
    {
        cell = rc_flowQ[head++];
        x = (int16_t)(cell & (RC_MAP_W - 1u));
        y = (int16_t)(cell >> RC_MAP_SHIFT);
        for(i = 0; i < 4; i++)
        {
            nx = (int16_t)(x + rc_kNbDx[i]);
            ny = (int16_t)(y + rc_kNbDy[i]);
            if((uint16_t)nx >= RC_MAP_W || (uint16_t)ny >= RC_MAP_H) continue;
            if(Solid(nx, ny)) continue;
            nb = (uint16_t)(((uint16_t)ny << RC_MAP_SHIFT) + (uint16_t)nx);
            if(rc_flow[nb] != 0xFFFFu) continue;
            rc_flow[nb] = (uint16_t)(rc_flow[cell] + 1u);
            if(tail < RC_MAP_CELLS) rc_flowQ[tail++] = nb;
        }
    }
}


void rc_EnsureFlow(void)
{
    if((int16_t)(rc_px >> 10) != rc_flowTx || (int16_t)(rc_py >> 10) != rc_flowTy)
        BuildFlow();
}


// Is the straight line between two points free of walls?
bool rc_CanSee(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int16_t dx = RD(x1 - x0);
    int16_t dy = RD(y1 - y0);
    int16_t n  = (int16_t)(Dist10(dx, dy) / 320);
    int16_t i;

    for(i = 1; i < n; i++)
        if(Blocked(x0 + ((int32_t)dx * i) / n, y0 + ((int32_t)dy * i) / n, 220))
            return false;
    return true;
}


// Where an enemy at (ex, ey) should head, as a vector (dx, dy).
static void SteerTarget(int32_t ex, int32_t ey, int16_t *tdx, int16_t *tdy)
{
    int16_t  etx = (int16_t)(ex >> 10);
    int16_t  ety = (int16_t)(ey >> 10);
    int16_t  nx, ny;
    uint16_t d, i;

    *tdx = RD(rc_px - ex);                              // default: straight at the player
    *tdy = RD(rc_py - ey);

    if((uint16_t)etx >= RC_MAP_W || (uint16_t)ety >= RC_MAP_H) return;
    d = rc_flow[((uint16_t)ety << RC_MAP_SHIFT) + (uint16_t)etx];
    if(d <= 1u || d == 0xFFFFu) return;                 // next to the player / no route
    // Close by: walk straight if the player is in sight. Far away the flow field alone
    // is used (a sight check over a long distance costs dozens of wall tests per frame).
    if(d < 14u && Dist10(*tdx, *tdy) < RC_SEE_DIST && rc_CanSee(ex, ey, rc_px, rc_py)) return;

    for(i = 0; i < 4; i++)                              // step to the tile that is closer
    {
        nx = (int16_t)(etx + rc_kNbDx[i]);
        ny = (int16_t)(ety + rc_kNbDy[i]);
        if((uint16_t)nx >= RC_MAP_W || (uint16_t)ny >= RC_MAP_H) continue;
        if(rc_flow[((uint16_t)ny << RC_MAP_SHIFT) + (uint16_t)nx] == (uint16_t)(d - 1u))
        {
            *tdx = RD(TILE_C(nx) - ex);
            *tdy = RD(TILE_C(ny) - ey);
            return;
        }
    }
}


//*****************************************************************************
// Enemy helpers
//*****************************************************************************

// Slide an enemy by (mx, my); walls stop each axis separately. Counts walk-animation steps.
static void Slide(Enemy *e, int16_t mx, int16_t my)
{
    int32_t nx = e->x + mx;
    int32_t ny = e->y + my;
    bool    moved = false;

    if(!Blocked(nx, e->y, 230)) { e->x = nx; moved = true; }
    if(!Blocked(e->x, ny, 230)) { e->y = ny; moved = true; }
    if(moved) e->an++;
}


// Move at speed sp along the vector (vx, vy).
static void MoveAlong(Enemy *e, int16_t vx, int16_t vy, int16_t sp)
{
    int16_t d = Dist10(vx, vy);

    if(d < 1) return;
    Slide(e, (int16_t)(((int32_t)vx * sp) / d), (int16_t)(((int32_t)vy * sp) / d));
}


// Walk toward the player (straight if visible, else along the flow field).
// side = +1 / -1 adds a sideways drift when close (goblins surround you).
static void Chase(Enemy *e, int16_t sp, int16_t side, int16_t dx, int16_t dy, int16_t dist)
{
    int16_t sdx, sdy, sd, mx, my;

    SteerTarget(e->x, e->y, &sdx, &sdy);
    sd = Dist10(sdx, sdy);
    if(sd < 1) sd = 1;
    mx = (int16_t)(((int32_t)sdx * sp) / sd);
    my = (int16_t)(((int32_t)sdy * sp) / sd);
    if(side && dist > 0 && dist < 3400)
    {
        mx = (int16_t)(mx + (int16_t)(((int32_t)(-dy) * (sp / 3) * side) / dist));
        my = (int16_t)(my + (int16_t)(((int32_t)( dx) * (sp / 3) * side) / dist));
    }
    Slide(e, mx, my);
}


// Ranged spacing: close in beyond `far`, back off inside `near`, drift sideways between.
static void KeepRange(Enemy *e, int16_t sp, int16_t neer, int16_t faar, int16_t sgn,
                      int16_t dx, int16_t dy, int16_t dist)
{
    if(dist < 1) return;
    if(dist > faar)
        Chase(e, sp, 0, dx, dy, dist);
    else if(dist < neer)
        Slide(e, (int16_t)(-(((int32_t)dx * (sp + (sp >> 1))) / dist)),
                 (int16_t)(-(((int32_t)dy * (sp + (sp >> 1))) / dist)));
    else
        Slide(e, (int16_t)(sgn * (((int32_t)-dy * (sp >> 1)) / dist)),
                 (int16_t)(sgn * (((int32_t) dx * (sp >> 1)) / dist)));
}


static int16_t SgnOf(const Enemy *e)
{
    return (int16_t)((e->an & 64u) ? 1 : -1);        // flips slowly so strafing changes side
}


// A random valid spot on a ring around (cx, cy). Returns false if no free spot was found.
bool rc_TeleportEnemy(Enemy *e, int32_t cx, int32_t cy, int16_t rMin, int16_t rMax)
{
    uint16_t k;
    int16_t  a, r;
    int32_t  nx, ny;

    for(k = 0; k < 10; k++)
    {
        a  = (int16_t)RAND(RC_ANGLES);
        r  = (int16_t)(rMin + (int16_t)RAND((uint16_t)(rMax - rMin + 1)));
        nx = cx + (((int32_t)COS(a) * r) >> 14);
        ny = cy + (((int32_t)SIN(a) * r) >> 14);
        if(Blocked(nx, ny, 260)) continue;
        if(DistP(nx, ny, rc_px, rc_py) < 900) continue;
        rc_AddSpark(e->x, e->y, 6, 10, PT_PUFF_ARCANE, 8);
        e->x = nx;
        e->y = ny;
        rc_AddSpark(nx, ny, 6, 10, PT_PUFF_ARCANE, 8);
        return true;
    }
    return false;
}


// Fire one projectile at the player. angleOff rotates the aim (angle units, 1024 = full turn).
void rc_ShootBolt(const Enemy *e, int16_t angleOff, uint16_t kind, int16_t speed, uint16_t life)
{
    int16_t dx   = RD(rc_px - e->x);
    int16_t dy   = RD(rc_py - e->y);
    int16_t dist = Dist10(dx, dy);
    int16_t vx, vy, c, s;
    Obj    *o;

    if(dist < 300) return;
    vx = (int16_t)(((int32_t)dx * speed) / dist);
    vy = (int16_t)(((int32_t)dy * speed) / dist);
    if(angleOff)
    {
        c = COS(angleOff);
        s = SIN(angleOff);
        {
            int16_t rx = (int16_t)((((int32_t)vx * c) - ((int32_t)vy * s)) >> 14);
            int16_t ry = (int16_t)((((int32_t)vx * s) + ((int32_t)vy * c)) >> 14);
            vx = rx;
            vy = ry;
        }
    }
    o = rc_NewObj(OB_BOLT);
    if(o == NULL) return;
    o->x    = e->x;
    o->y    = e->y;
    o->vx   = vx;
    o->vy   = vy;
    o->z    = (kind == BK_FLAME) ? 4 : 6;
    o->life = life;
    o->aux  = kind;
    // damage in half hearts: arrows, webs, ice and sparks are small hits, the rest big
    o->sc   = (kind == BK_ARROW || kind == BK_ICE || kind == BK_SPARK || kind == BK_WEB) ? DMG_SMALL : DMG_BIG;
}


static uint16_t SummonCount(uint16_t type)
{
    uint16_t i, n = 0;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
        if(rc_en[i].alive && rc_en[i].sum && rc_en[i].type == type) n++;
    return n;
}


// Summon n minions of `type` around the master (they die with it).
static void SummonAround(const Enemy *m, uint16_t type, uint16_t n)
{
    uint16_t k;

    for(k = 0; k < n; k++)
    {
        int32_t sx = m->x + ((k == 0) ? 520L : (k == 1) ? -520L : 0L);
        int32_t sy = m->y + ((k == 2) ? 520L : (k == 3) ? -520L : 0L);
        Enemy  *p;

        if(rc_alive >= RC_MAX_ENEMIES - 2) break;   // keep room for the wave
        if(Blocked(sx, sy, 230)) { sx = m->x; sy = m->y; }
        p = rc_SpawnEnemy(type, sx, sy);
        if(p == NULL) break;
        p->sum = 1;
        rc_AddSpark(sx, sy, 6, 10, PT_PUFF_ARCANE, 10);
    }
    rc_SfxSoft(&sfx_powerup);
}


void rc_BeholderShoot(const Enemy *e)               // Watcher laser (name kept)
{
    int16_t k, kmax = (rc_wave >= 9) ? 1 : 0;        // deeper: three-way fan

    for(k = -kmax; k <= kmax; k++)
        rc_ShootBolt(e, (int16_t)(k * 36), BK_LASER, RC_BOLT_SPEED, 130);
    rc_SfxSoft(&sfx_laser);
}


void rc_DragonBreath(const Enemy *e)
{
    int16_t k;

    for(k = -1; k <= 1; k++)
        rc_ShootBolt(e, (int16_t)(k * 33), BK_FLAME, 68, 115);
    rc_SfxSoft(&sfx_laser);
}


uint16_t rc_BeholderCadence(void)
{
    uint16_t cut = (uint16_t)(2u * rc_wave);
    if(cut > 45u) cut = 45u;
    return (uint16_t)(100u - cut);                      // frames between shots
}


// Which sprite frame / palette an enemy is drawn with (used by the renderer).
void rc_EnemyLook(const Enemy *e, uint16_t *shape, uint16_t *pal)
{
    const EnemyDef *d = &rc_kEnemy[e->type];

    *shape = d->shape;
    *pal   = d->pal;
    if(d->shapeB && ((e->an >> 3) & 1u)) *shape = d->shapeB;

    switch(e->type)
    {
        case ET_WATCHER:
            if(e->st == 1 || e->aux + 25u >= rc_BeholderCadence()) *pal = PT_BEHOLDER_HOT;
            break;

        case ET_GHOST:
            if(e->st == 1) { *shape = SH_GHOST_FADE; *pal = PT_GHOST_FADE; }
            break;

        case ET_BOMBER:
            if(e->st == 1 && (e->tm & 2u)) *pal = PT_BOMBER_LIT;
            break;

        case ET_MIMIC:
            if(e->st != 0) { *shape = SH_MIMIC; *pal = PT_MIMIC; }
            break;

        case ET_GOLEM:
            *pal = (uint16_t)(PT_GOLEM_FIRE + e->aux2 % 3u);
            if(e->st == 1 || e->st == 2 || e->st == 4) *shape = SH_GOLEM_B;
            break;

        case ET_EYEKING:
            if(e->st >= 1 && e->st <= 3)                // charging / firing: eye shows the element
            {
                static const uint16_t kEye[6] = { PT_EYEKING_FIRE, PT_EYEKING_ICE, PT_EYEKING_BOLT,
                                                  PT_EYEKING_GRAV, PT_EYEKING_NULL, PT_EYEKING };
                *pal = kEye[e->aux2 % 6u];
            }
            break;

        case ET_DRAGON:
            *pal = (e->aux >= 48u && e->aux <= 86u) ? PT_DRAGON_HOT : PT_DRAGON;
            if(e->aux >= 112u && e->aux <= 150u) *shape = SH_DRAGON_ATTACK;
            break;

        default:
            break;
    }
}


// Shove the player (collision checked per axis).
void rc_PlayerPush(int16_t dx, int16_t dy)
{
    if(!Blocked(rc_px + dx, rc_py, RC_PLAYER_R)) rc_px += dx;
    if(!Blocked(rc_px, rc_py + dy, RC_PLAYER_R)) rc_py += dy;
    rc_flowTx = -1;
}
//*****************************************************************************
// Per-type behaviour. Each returns true if touching the player hurts.
//*****************************************************************************
static uint16_t Phase(const Enemy *e)               // 0 = healthy, 1 = below 2/3, 2 = below 1/3
{
    if((int32_t)e->hp * 3L > (int32_t)e->mhp * 2L) return 0u;
    if((int32_t)e->hp * 3L > (int32_t)e->mhp)      return 1u;
    return 2u;
}


// Aim a straight dash at the player (speed 4x walking).
static void AimCharge(Enemy *e, int16_t sp, int16_t dx, int16_t dy)
{
    int16_t d = Dist10(dx, dy);

    if(d < 1) d = 1;
    e->tx = (int16_t)(((int32_t)dx * sp * 4L) / d);
    e->ty = (int16_t)(((int32_t)dy * sp * 4L) / d);
}


static bool ChargeStep(Enemy *e)
{
    int32_t nx = e->x + e->tx;
    int32_t ny = e->y + e->ty;

    if(Blocked(nx, ny, 230)) return false;          // stops at a wall
    e->x = nx;
    e->y = ny;
    e->an++;
    return true;
}


static void BehindPlayer(int32_t *bx, int32_t *by)   // a spot behind the player's back
{
    *bx = rc_px - (((int32_t)COS(rc_ang) * 1400L) >> 14);
    *by = rc_py - (((int32_t)SIN(rc_ang) * 1400L) >> 14);
}


static bool ThinkHobgob(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    switch(e->st)
    {
        case 0:                                     // walk, now and then charge
            Chase(e, sp, 0, dx, dy, dist);
            if(e->tm) e->tm--;
            else if(dist > 1500 && dist < 4200 && rc_CanSee(e->x, e->y, rc_px, rc_py))
            {
                e->st = 1;
                e->tm = 24;                         // telegraph ("!" frames)
            }
            else e->tm = 20;
            break;
        case 1:
            if(--e->tm == 0) { AimCharge(e, sp, dx, dy); e->st = 2; e->tm = 14; }
            break;
        case 2:
            if(!ChargeStep(e) || --e->tm == 0) { e->st = 3; e->tm = 30; }
            break;
        default:                                    // recovery: shield down, takes full damage
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)(100u + RAND(120)); }
            break;
    }
    return true;
}


static bool ThinkBat(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    int16_t sdx, sdy, off, c, s, vx, vy;

    (void)dx; (void)dy;
    if(e->tm == 0)                                  // pick a new angle of attack
    {
        e->tm = (uint16_t)(18u + RAND(24));
        e->tx = (int16_t)((RAND(2) ? 1 : -1) * (int16_t)(60u + RAND(180)));
    }
    e->tm--;

    SteerTarget(e->x, e->y, &sdx, &sdy);
    off = (dist < 1100) ? (int16_t)(e->tx >> 2) : e->tx;     // close in: fly straighter
    c = COS(off);
    s = SIN(off);
    vx = (int16_t)((((int32_t)sdx * c) - ((int32_t)sdy * s)) >> 14);
    vy = (int16_t)((((int32_t)sdx * s) + ((int32_t)sdy * c)) >> 14);
    MoveAlong(e, vx, vy, sp);
    return true;
}


static bool ThinkWatcher(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    KeepRange(e, sp, 2600, 3800, SgnOf(e), dx, dy, dist);

    if(e->st == 1)                                  // gaze: eye glows, then weakens your magic
    {
        if(--e->tm == 0)
        {
            e->st = 0;
            if(dist < 5200 && rc_CanSee(e->x, e->y, rc_px, rc_py)) { rc_nullify = 150; rc_hud |= HUD_HINT; }
        }
        return true;
    }
    if(++e->aux >= rc_BeholderCadence())
    {
        e->aux = 0;
        if((++e->cnt % 6u) == 0) { e->st = 1; e->tm = 28; }
        else                    rc_BeholderShoot(e);
    }
    return true;
}


static bool ThinkLich(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t p = Phase(e);

    if(p) sp = (int16_t)(sp + (sp >> 2));
    switch(e->st)
    {
        case 0:
            Chase(e, sp, 0, dx, dy, dist);
            if(++e->aux >= (uint16_t)(180u - 35u * p))          // summon skeletons
            {
                e->aux = 0;
                if(SummonCount(ET_SKELETON) + SummonCount(ET_ARCHER) < 4u)     // at most 4 raised
                    SummonAround(e, (RAND(4) == 0u) ? ET_ARCHER : ET_SKELETON, 1u);
            }
            if(++e->aux2 >= (uint16_t)(80u - 10u * p))          // soul bolts (every 3rd drains)
            {
                e->aux2 = 0;
                rc_ShootBolt(e, 0, ((++e->cnt % 3u) == 0) ? BK_DRAIN : BK_ORB, 62, 130);
                rc_SfxSoft(&sfx_laser);
            }
            if(e->tm) e->tm--;
            else if(p == 2 && (e->cnt & 1u))                    // Death Nova
            {
                e->st = 3;
                e->tm = 40;
                rc_MakeWarn(e->x, e->y, 2200, 40, AR_SLIME, 0);
            }
            else { e->st = 1; e->tm = 24; }                     // charge
            break;
        case 1:
            if(--e->tm == 0) { AimCharge(e, sp, dx, dy); e->st = 2; e->tm = 16; }
            break;
        case 2:
            if(!ChargeStep(e) || --e->tm == 0) { e->st = 4; e->tm = 30; }
            break;
        case 3:
            if(--e->tm == 0) { rc_MakeWave(e->x, e->y, 50, 3600, WV_NOVA); e->st = 4; e->tm = 40; e->cnt++; }
            break;
        default:
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)(200u - 30u * p + RAND(60)); }
            break;
    }
    return true;
}


static bool ThinkDragon(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t p   = Phase(e);
    uint16_t len = (uint16_t)(180u - 20u * p);

    if(p) sp = (int16_t)(sp + (sp >> 2));                       // enraged
    if(++e->aux >= len) { e->aux = 0; e->cnt++; }

    if(e->aux == 58u || e->aux == 72u) rc_DragonBreath(e);      // two fire volleys
    if(p >= 1u && e->aux == 120u && dist < 2800 && dist > 0)    // wing gust
        rc_PlayerPush((int16_t)(((int32_t)dx * 650L) / dist), (int16_t)(((int32_t)dy * 650L) / dist));
    if(p == 2u && e->aux == 90u)                                // phase 3: inferno
    {
        rc_MakeWarn(rc_px, rc_py, 900, 40, AR_FIRE, 150);
        rc_MakeWarn(rc_px + 1500L, rc_py - 700L, 900, 40, AR_FIRE, 150);
        rc_MakeWarn(rc_px - 1300L, rc_py + 900L, 900, 40, AR_FIRE, 150);
    }

    if(dist > 200) Chase(e, sp, 0, dx, dy, dist);
    if(e->aux == 132u && dist < 2050) rc_HurtPlayer(DMG_BOSS_HEAVY);   // tail-whip impact frame
    return false;
}


static bool ThinkSlime(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    Chase(e, sp, 0, dx, dy, dist);
    if(e->type == ET_SLIME && ++e->aux >= 45u)                  // leaves a damaging puddle
    {
        Obj *o = rc_NewObj(OB_AREA);

        e->aux = 0;
        if(o != NULL) { o->x = e->x; o->y = e->y; o->r = 420; o->life = 150; o->aux = AR_SLIME; }
    }
    return true;
}


static bool ThinkSpider(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t cad = (rc_wave > 10u) ? 65u : (uint16_t)(85u - 2u * rc_wave);

    KeepRange(e, sp, 2600, 4200, SgnOf(e), dx, dy, dist);
    if(++e->aux >= cad && dist < 5400 && rc_CanSee(e->x, e->y, rc_px, rc_py))
    {
        e->aux = 0;
        rc_ShootBolt(e, 0, BK_WEB, 66, 110);
        rc_SfxSoft(&sfx_laser);
    }
    return true;
}


static bool ThinkGhost(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    int32_t bx, by;
    int16_t vx, vy, d;

    switch(e->st)
    {
        case 0:
            Chase(e, sp, 0, dx, dy, dist);
            if(e->tm) e->tm--; else { e->st = 1; e->tm = 40; }
            return true;
        case 1:                                     // intangible: glides through walls
            BehindPlayer(&bx, &by);
            vx = RD(bx - e->x);
            vy = RD(by - e->y);
            d  = Dist10(vx, vy);
            if(d > 1)
            {
                e->x += ((int32_t)vx * sp * 2L) / d;
                e->y += ((int32_t)vy * sp * 2L) / d;
                e->an++;
            }
            if(--e->tm == 0) e->st = 2;
            return false;
        case 2:                                     // reappear behind the player
            BehindPlayer(&bx, &by);
            if(!rc_TeleportEnemy(e, bx, by, 0, 500))
                (void)rc_TeleportEnemy(e, rc_px, rc_py, 2200, 3600);
            e->st = 3;
            e->tm = 15;
            return true;
        default:
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)(90u + RAND(70)); }
            return true;
    }
}


static bool ThinkBomber(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    if(e->st == 0)
    {
        Chase(e, sp, 0, dx, dy, dist);
        if(dist < 950 && rc_CanSee(e->x, e->y, rc_px, rc_py)) { e->st = 1; e->tm = 16; }   // fuse lit
    }
    else if(--e->tm == 0)
    {
        rc_Explode(e->x, e->y);
        e->alive = 0;
        if(rc_alive) rc_alive--;
    }
    return false;
}


static bool ThinkMimic(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    if(e->st == 0)                                  // disguised as a chest
    {
        if(dist < 1500 || e->flash) { e->st = 1; e->tm = 12; rc_Sfx(&sfx_hit); }
        return false;
    }
    if(e->st == 1) { if(--e->tm == 0) e->st = 2; return true; }
    Chase(e, sp, 0, dx, dy, dist);
    return true;
}


static bool ThinkSlimeKing(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t p = Phase(e), k, a;

    if(!(e->cnt & 0x100u) && (int32_t)e->hp * 2L < (int32_t)e->mhp)     // splits in two at half health
    {
        e->cnt |= 0x100u;
        for(k = 0; k < 2; k++)
        {
            Enemy *s = rc_SpawnEnemy(ET_SLIME_S, e->x + (k ? 600L : -600L), e->y);
            if(s != NULL) s->tm = 0;
        }
    }
    switch(e->st)
    {
        case 0:
            Chase(e, (int16_t)(p == 2u ? sp + (sp >> 1) : sp), 0, dx, dy, dist);
            if(e->tm) { e->tm--; break; }
            a = e->cnt & 3u;
            e->cnt = (uint16_t)((e->cnt & 0x100u) | ((a + 1u) % 3u));
            if(a == 0u)                             // ground slam
            {
                e->st = 1;
                e->tm = 40;
                rc_MakeWarn(e->x, e->y, 1900, 40, AR_SLIME, 0);
            }
            else if(a == 1u)                        // toxic rain
            {
                e->st = 5;
                e->tm = 40;
                for(k = 0; k < 5; k++)
                {
                    int32_t rx = rc_px + (int32_t)RAND(4000) - 2000L;
                    int32_t ry = rc_py + (int32_t)RAND(4000) - 2000L;

                    if(!Blocked(rx, ry, 100)) rc_MakeWarn(rx, ry, 650, 40, AR_SLIME, 170);
                }
            }
            else { e->st = 2; e->tm = 24; }         // royal bounce
            break;
        case 1:
            if(--e->tm == 0) { rc_MakeWave(e->x, e->y, 45, 3000, WV_SLIME); e->st = 4; e->tm = 34; }
            break;
        case 2:
            if(--e->tm == 0) { AimCharge(e, sp, dx, dy); e->st = 3; e->tm = 16; }
            break;
        case 3:
            if(!ChargeStep(e) || --e->tm == 0) { e->st = 4; e->tm = 30; }
            break;
        case 5:
            if(--e->tm == 0) { e->st = 4; e->tm = 30; }
            break;
        default:
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)((p == 2u ? 60u : 95u) + RAND(40)); }
            break;
    }
    return true;
}


static bool ThinkGolem(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t p = Phase(e);

    if(p == 2u) sp = (int16_t)(sp * 2);             // Overload: faster at low health
    switch(e->st)
    {
        case 0:
            Chase(e, sp, 0, dx, dy, dist);
            if(p >= 1u && !(e->cnt & 1u))    { e->cnt |= 1u; e->st = 5; e->tm = 90; break; }   // Fortification
            if(p == 2u && !(e->cnt & 2u))    { e->cnt |= 2u; e->st = 5; e->tm = 90; break; }
            if(e->tm) { e->tm--; break; }
            if(dist < 1500)      { e->st = 1; e->tm = 30; }                                     // Iron Fist
            else if(dist > 2300) { e->st = 2; e->tm = 36; rc_MakeWarn(e->x, e->y, 2000, 36, AR_SLIME, 0); }  // Earthquake
            else e->tm = 10;
            break;
        case 1:
            if(--e->tm == 0)
            {
                if(dist < 1750)
                {
                    rc_HurtPlayer(DMG_BOSS_HEAVY);
                    if(e->aux2 % 3u == 1u) { rc_pSlow = 90; rc_hud |= HUD_HINT; }   // ice fist slows
                }
                rc_AddSpark(e->x, e->y, 2, 14, PT_PUFF_FIRE, 8);
                e->aux2++;
                e->st = 4;
                e->tm = 70;                         // weak point is exposed
            }
            break;
        case 2:
            if(--e->tm == 0)
            {
                uint16_t el = e->aux2 % 3u;

                rc_MakeWave(e->x, e->y, (int16_t)(el == 2u ? 60 : 40), 4800,
                            (uint16_t)(el == 0u ? WV_FIRE : el == 1u ? WV_ICE : WV_STORM));
                if(el == 0u) rc_MakeWarn(rc_px, rc_py, 800, 30, AR_FIRE, 130);
                e->aux2++;
                e->st = 6;
                e->tm = 50;
            }
            break;
        default:                                    // 4 exposed, 5 fortified, 6 recovering
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)(60u + RAND(40)); }
            break;
    }
    return true;
}


static bool ThinkVampire(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    bool frenzy = ((int32_t)e->hp * 5L < (int32_t)e->mhp * 2L);          // below 40%
    int32_t bx, by;

    if(frenzy) sp = (int16_t)(sp + sp / 3);
    switch(e->st)
    {
        case 0:
            KeepRange(e, sp, 2400, 3600, SgnOf(e), dx, dy, dist);
            if(e->tm) { e->tm--; break; }
            e->aux2 = RAND(3);
            if(e->aux2 == 2u && SummonCount(ET_BAT) >= 4u) e->aux2 = 0;
            e->st = 1;
            e->tm = 20;
            break;
        case 1:
            if(--e->tm == 0)
            {
                if(e->aux2 == 0u)                   // blood drain
                {
                    rc_ShootBolt(e, 0, BK_DRAIN, 75, 120);
                    if(frenzy) { rc_ShootBolt(e, 30, BK_DRAIN, 75, 120); rc_ShootBolt(e, -30, BK_DRAIN, 75, 120); }
                }
                else if(e->aux2 == 1u)              // shadow step, then strike
                {
                    BehindPlayer(&bx, &by);
                    (void)rc_TeleportEnemy(e, bx, by, 0, 500);
                    rc_ShootBolt(e, 0, BK_DRAIN, 75, 120);
                }
                else SummonAround(e, ET_BAT, 3u);   // bat swarm
                rc_SfxSoft(&sfx_laser);
                e->st = 2;
                e->tm = frenzy ? 40u : 70u;
            }
            break;
        default:
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)((frenzy ? 30u : 60u) + RAND(40)); }
            break;
    }
    return true;
}


static bool ThinkEye(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t p = Phase(e), n, a;
    int16_t  off;

    switch(e->st)
    {
        case 0:
            KeepRange(e, (int16_t)(sp + (int16_t)(p * 3u)), 3200, 4800, SgnOf(e), dx, dy, dist);
            if(e->tm) { e->tm--; break; }
            n = (p == 0u) ? 3u : (p == 1u) ? 5u : 6u;
            a = RAND(n);
            if(a == e->aux2) a = (a + 1u) % n;
            e->aux2 = a;
            e->st = 1;
            e->tm = (uint16_t)(26u - 6u * p);
            break;
        case 1:                                     // eye glows in the colour of the attack
            if(--e->tm != 0) break;
            switch(e->aux2)
            {
                case 0:                             // fire eye: three-way fan
                    rc_ShootBolt(e, -38, BK_FIRE, 70, 120);
                    rc_ShootBolt(e,   0, BK_FIRE, 70, 120);
                    rc_ShootBolt(e,  38, BK_FIRE, 70, 120);
                    break;
                case 1:                             // ice eye: slows
                    rc_ShootBolt(e, 0, BK_ICE, 66, 130);
                    break;
                case 2:                             // lightning eye: sweeping bolts
                    e->st = 2;
                    e->tm = 18;
                    return true;
                case 3:                             // gravity eye: pulls you in
                    rc_pull  = 70;
                    rc_pullX = e->x;
                    rc_pullY = e->y;
                    break;
                case 4:                             // nullifying gaze
                    if(rc_CanSee(e->x, e->y, rc_px, rc_py)) { rc_nullify = 180; rc_hud |= HUD_HINT; }
                    break;
                default:                            // eye barrage
                    for(off = -64; off <= 64; off += 32) rc_ShootBolt(e, off, BK_ORB, 58, 130);
                    break;
            }
            rc_SfxSoft(&sfx_laser);
            e->st = 3;
            e->tm = 12;
            break;
        case 2:
            if((e->tm % 3u) == 0u)
                rc_ShootBolt(e, (int16_t)(-40 + (int16_t)((18u - e->tm) * 80u / 18u)), BK_SPARK, 85, 80);
            if(--e->tm == 0) { e->st = 3; e->tm = 12; }
            break;
        default:
            if(--e->tm == 0) { e->st = 0; e->tm = (uint16_t)(110u - 25u * p + RAND(30)); }
            break;
    }
    return true;
}




//*****************************************************************************
// Skeleton Archer (rc.md 13.1): keeps its distance and fires arrows.
//*****************************************************************************
static bool ThinkArcher(Enemy *e, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    switch(e->st)
    {
        case 0:
            KeepRange(e, sp, 3000, 4600, SgnOf(e), dx, dy, dist);
            if(e->tm) { e->tm--; break; }
            if(dist < 7000 && rc_CanSee(e->x, e->y, rc_px, rc_py)) { e->st = 1; e->tm = 16; }   // draw the bow
            else e->tm = 20;
            break;
        case 1:
            if(--e->tm == 0)
            {
                rc_ShootBolt(e, 0, BK_ARROW, 88, 120);
                rc_SfxSoft(&sfx_laser);
                e->st = 0;
                e->tm = (uint16_t)(70u + RAND(40));
            }
            break;
        default:
            e->st = 0;
            break;
    }
    return true;
}


//*****************************************************************************
// Goblin (rc.md 13.3): with a Hobgoblin nearby, goblins hold position behind
// its shield and then slip round the sides to surround the player.
//*****************************************************************************
static void ThinkGoblin(Enemy *e, uint16_t idx, int16_t sp, int16_t dx, int16_t dy, int16_t dist)
{
    uint16_t j, best = 0xFFFFu;
    int16_t  bestD = 6144, d;
    int16_t  side = (int16_t)((idx & 1u) ? 1 : -1);

    if(dist > 2200)
        for(j = 0; j < RC_MAX_ENEMIES; j++)
        {
            const Enemy *h = &rc_en[j];

            if(!h->alive || h->type != ET_HOBGOB) continue;
            d = DistP(h->x, h->y, e->x, e->y);
            if(d < bestD) { bestD = d; best = j; }
        }

    if(best != 0xFFFFu)
    {
        const Enemy *h = &rc_en[best];
        int16_t hx = RD(h->x - rc_px), hy = RD(h->y - rc_py), hd = Dist10(hx, hy);

        if(hd > 1)
        {
            // a spot 0.9 tiles behind the shield (seen from the player), fanned to one side
            int32_t tx = h->x + ((int32_t)hx * 900L) / hd + ((int32_t)(-hy) * 500L * side) / hd;
            int32_t ty = h->y + ((int32_t)hy * 900L) / hd + ((int32_t)( hx) * 500L * side) / hd;
            int16_t vx = RD(tx - e->x), vy = RD(ty - e->y);

            if(Dist10(vx, vy) > 300) { MoveAlong(e, vx, vy, sp); return; }
            if(hd > 2600) return;                   // hold behind the shield until it closes in
        }
    }
    Chase(e, sp, side, dx, dy, dist);
}


//*****************************************************************************
// The update loop: every enemy thinks every frame (rc.md 16.1).
//*****************************************************************************
void rc_UpdateEnemies(void)
{
    uint16_t i, j;
    int16_t  dx, dy, dist, sp;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        Enemy          *e = &rc_en[i];
        const EnemyDef *d;
        bool            hurts = true;

        if(!e->alive) continue;
        d = &rc_kEnemy[e->type];

        if(e->flash) e->flash--;

        if(e->burn)                                     // burning: 1 damage every 16 frames
        {
            e->burn--;
            if((e->burn & 15u) == 0)
            {
                rc_DamageEnemyEx(e, 1, 0, 0, true);
                if(!e->alive) continue;
            }
        }

        if(e->freeze) { e->freeze--; continue; }        // frozen solid: no move, no attack

        sp = d->speed;
        if(e->slow) { e->slow--; sp >>= 1; }

        dx   = RD(rc_px - e->x);
        dy   = RD(rc_py - e->y);
        dist = Dist10(dx, dy);

        switch(e->type)
        {
            case ET_GOBLIN:    ThinkGoblin(e, i, sp, dx, dy, dist);         break;
            case ET_HOBGOB:    hurts = ThinkHobgob(e, sp, dx, dy, dist);    break;
            case ET_BAT:       hurts = ThinkBat(e, sp, dx, dy, dist);       break;
            case ET_WATCHER:   hurts = ThinkWatcher(e, sp, dx, dy, dist);   break;
            case ET_LICH:      hurts = ThinkLich(e, sp, dx, dy, dist);      break;
            case ET_DRAGON:    hurts = ThinkDragon(e, sp, dx, dy, dist);    break;
            case ET_SLIME:
            case ET_SLIME_S:   hurts = ThinkSlime(e, sp, dx, dy, dist);     break;
            case ET_SPIDER:    hurts = ThinkSpider(e, sp, dx, dy, dist);    break;
            case ET_GHOST:     hurts = ThinkGhost(e, sp, dx, dy, dist);     break;
            case ET_BOMBER:    hurts = ThinkBomber(e, sp, dx, dy, dist);    break;
            case ET_ARCHER:    hurts = ThinkArcher(e, sp, dx, dy, dist);    break;
            case ET_MIMIC:     hurts = ThinkMimic(e, sp, dx, dy, dist);     break;
            case ET_SLIMEKING: hurts = ThinkSlimeKing(e, sp, dx, dy, dist); break;
            case ET_GOLEM:     hurts = ThinkGolem(e, sp, dx, dy, dist);     break;
            case ET_VAMPIRE:   hurts = ThinkVampire(e, sp, dx, dy, dist);   break;
            case ET_EYEKING:   hurts = ThinkEye(e, sp, dx, dy, dist);       break;
            default:           Chase(e, sp, 0, dx, dy, dist);              break;   // skeleton
        }

        if(e->alive && hurts && dist < d->reach)
            rc_HurtPlayer((e->type == ET_HOBGOB && e->st == 2u) ? DMG_BIG : d->touch);
    }

    // Keep them from stacking into one blob.
    for(i = 0; i + 1 < RC_MAX_ENEMIES; i++)
    {
        if(!rc_en[i].alive) continue;
        for(j = i + 1; j < RC_MAX_ENEMIES; j++)
        {
            if(!rc_en[j].alive) continue;
            dx = RD(rc_en[i].x - rc_en[j].x);
            dy = RD(rc_en[i].y - rc_en[j].y);
            if(Abs16(dx) < 300 && Abs16(dy) < 300)
            {
                int16_t px = (dx >= 0) ? 5 : -5;
                int16_t py = (dy >= 0) ? 5 : -5;

                if(!Blocked(rc_en[i].x + px, rc_en[i].y, 230)) rc_en[i].x += px;
                if(!Blocked(rc_en[i].x, rc_en[i].y + py, 230)) rc_en[i].y += py;
                if(!Blocked(rc_en[j].x - px, rc_en[j].y, 230)) rc_en[j].x -= px;
                if(!Blocked(rc_en[j].x, rc_en[j].y - py, 230)) rc_en[j].y -= py;
            }
        }
    }
}
