// rc_logic.c - combat, progression, objects, chests, spawning.
#include "rc_priv.h"

char     rc_toast[32];
uint16_t rc_toastT;
static int16_t s_rewardProp = -1;           // mini-boss reward chest (one slot, reused)


void rc_Toast(const char *a, const char *b)
{
    int16_t n = rc_Cat(rc_toast, 0, a, 30);

    if(b) rc_Cat(rc_toast, n, b, (int16_t)(30 - n));
    rc_toastT = 75;
    rc_hud   |= HUD_HINT;
}


//*****************************************************************************
// Progression
//*****************************************************************************
uint16_t rc_XpNeed(void)
{
    return (uint16_t)(24u + 12u * rc_xpLv);             // rc.md 5.5
}


void rc_GainXp(uint16_t amt)
{
    uint16_t need;

    rc_xp += amt;
    for(;;)
    {
        need = rc_XpNeed();
        if(rc_xp < need) break;
        rc_xp -= need;
        rc_xpLv++;
        rc_lvPending++;
    }
    rc_hud |= HUD_XP | HUD_LEVEL;
}


// Total bonus of a stat after its soft cap: step% * soft[level].
uint16_t rc_StatPct(uint16_t st)
{
    return (uint16_t)(((uint32_t)rc_kStat[st].step * rc_kSoft[rc_lvStat[st]]) >> 8);
}


bool rc_HasRune(uint16_t r)
{
    uint16_t i;

    for(i = 0; i < RUNE_SLOTS; i++)
        if(rc_rune[i] == r) return true;
    return false;
}


void rc_GainGold(uint16_t base)
{
    uint32_t pct = 100u + rc_StatPct(ST_GOLD) + (rc_HasRune(RU_FORTUNE) ? 25u : 0u);
    uint32_t g   = ((uint32_t)base * pct + 50u) / 100u;

    rc_gold = (rc_gold + g > 9999u) ? 9999u : (uint16_t)(rc_gold + g);
    rc_hud |= HUD_GOLD;
}


//-----------------------------------------------------------------------------
// Equipment slots
//-----------------------------------------------------------------------------
static bool SlotOwned(uint16_t slot)
{
    if(slot < NUM_STAFF) return (rc_staffOwned & (1u << slot)) != 0;
    return (rc_itemOwned & (1u << (slot - NUM_STAFF))) != 0;
}

const char *rc_SlotName(uint16_t slot)
{
    return (slot < NUM_STAFF) ? rc_kStaff[slot].name : rc_kItem[slot - NUM_STAFF].name;
}

uint32_t rc_SlotColor(uint16_t slot)
{
    return (slot < NUM_STAFF) ? rc_kStaff[slot].color : rc_kItem[slot - NUM_STAFF].color;
}

uint16_t rc_SlotMana(uint16_t slot)
{
    uint16_t m = (slot < NUM_STAFF) ? rc_kStaffMana[slot] : rc_kItemMana[slot - NUM_STAFF];

    if(rc_HasRune(RU_CONSERVE)) m = (uint16_t)((m * 3u + 3u) >> 2);
    return m;
}


static uint16_t ScaleCd(uint16_t base)
{
    uint32_t pct = 100u;
    uint16_t a   = rc_StatPct(ST_ASPD);

    if(rc_HasRune(RU_SWIFT)) a += 15u;
    if(a > 60u) a = 60u;
    pct -= a;
    return (uint16_t)(((uint32_t)base * pct) / 100u);
}


static uint16_t StaffCooldown(uint16_t st)
{
    uint16_t cd = ScaleCd(rc_kStaff[st].cooldown);

    if(rc_haste) cd >>= 1;
    return (cd < 4u) ? 4u : cd;
}


void rc_CycleEquip(void)
{
    uint16_t i, slot = rc_equip;

    for(i = 1; i < NUM_SLOT; i++)
    {
        slot = (slot + 1u) % NUM_SLOT;
        if(SlotOwned(slot)) break;
    }

    if(slot != rc_equip && SlotOwned(slot))
    {
        rc_equip = slot;
        rc_hud  |= HUD_STAFF;
        rc_viewDirty = 2;
        rc_Sfx(&sfx_menu_move);
    }
}


//-----------------------------------------------------------------------------
// Player damage (half hearts)
//-----------------------------------------------------------------------------
void rc_HurtPlayer(uint16_t units)
{
    if(rc_invuln || rc_hp == 0 || units == 0) return;
    if(rc_aegis) { rc_aegis = 0; rc_invuln = 30; rc_hud |= HUD_HINT; rc_Sfx(&sfx_menu_select); return; }

    if(RAND(100) < rc_StatPct(ST_RES)) units--;         // resistance: block half a heart
    if(units == 0) { rc_invuln = 20; rc_SfxSoft(&sfx_blip); return; }

    rc_hp = (rc_hp > units) ? (uint16_t)(rc_hp - units) : 0u;
    rc_invuln = RC_INVULN_FRAMES;
    rc_hud   |= HUD_HEARTS;
    rc_viewDirty = 2;

    if(rc_hp == 0) rc_EnterOver();
    else           rc_Sfx(&sfx_hit);
}


void rc_HealPlayer(uint16_t units)
{
    rc_hp = (uint16_t)(rc_hp + units);
    if(rc_hp > rc_maxHp) rc_hp = rc_maxHp;
    rc_hud |= HUD_HEARTS;
}


// Walls follow the boss that is alive (the Dragon first, then the newest mini-boss),
// otherwise the theme of the room the player is in.
void rc_RefreshTheme(void)
{
    uint16_t i, t = 0xFFFFu;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        if(!rc_en[i].alive) continue;
        if(rc_en[i].type == ET_DRAGON) { t = TH_DRAGON; break; }
        if(RC_IS_MINI(rc_en[i].type))  t = rc_ThemeOfMini(rc_en[i].type);
    }
    if(t == 0xFFFFu)
        t = (rc_playerRoom < RM_COUNT) ? rc_kRoom[rc_playerRoom].theme : TH_BASEMENT;
    if(t != rc_theme) rc_ApplyTheme(t);
}


static void KillEnemy(Enemy *e)
{
    const EnemyDef *d = &rc_kEnemy[e->type];
    uint16_t i, g, xp;
    bool     boss = RC_IS_BOSS(e->type);
    int32_t  ex = e->x, ey = e->y;

    e->alive = 0;
    if(rc_alive) rc_alive--;

    g  = e->sum ? 1u : (uint16_t)d->gold;
    xp = e->sum ? 1u : (uint16_t)d->xp;
    rc_GainGold(g);
    rc_kills++;
    rc_hud |= HUD_LEFT;
    rc_GainXp(xp);
    rc_Sfx(&sfx_explosion);

    if(rc_HasRune(RU_LEECH) && ++rc_leechCnt >= 5u) { rc_leechCnt = 0; rc_HealPlayer(1); }
    if(rc_HasRune(RU_MOMENTUM)) rc_momentum = 90;

    if(!e->sum && !boss)
    {
        if(rc_hp < rc_maxHp && RAND(100) < HEART_DROP_PCT) rc_DropHeart(ex, ey);
        else if(RAND(100) < MANA_DROP_PCT)
        {
            Obj *o = rc_NewObj(OB_MANA);
            if(o) { o->x = ex; o->y = ey; o->pal = PT_FLASK_MANA; o->life = MANA_BOTTLE_LIFE; }
        }
    }

    if(e->type == ET_SLIME)                     // splits into two small slimes
    {
        for(i = 0; i < 2; i++)
        {
            Enemy *s2 = rc_SpawnEnemy(ET_SLIME_S, ex + (i ? 330L : -330L), ey);
            if(s2 != NULL && Blocked(s2->x, s2->y, 230)) { s2->x = ex; s2->y = ey; }
        }
    }
    else if(e->type == ET_BOMBER)
        rc_Explode(ex, ey);
    else if(e->type == ET_MIMIC && rc_treasureOpen == 2u)
        rc_treasureOpen = 0;                    // the treasure wave's Mimic is beaten

    if(RC_IS_MINI(e->type))                     // guaranteed heart + reward chest
    {
        rc_DropHeart(ex, ey);
        if(rc_miniAlive) rc_miniAlive--;
        rc_miniKills++;
        if(s_rewardProp < 0 || rc_prop[s_rewardProp].used)
        {
            if(s_rewardProp < 0) s_rewardProp = rc_AddProp(PR_CHEST, CH_GOOD, ex, ey);
            if(s_rewardProp >= 0)
            {
                Prop *p = &rc_prop[s_rewardProp];
                p->x = ex; p->y = ey; p->kind = PR_CHEST; p->sub = CH_GOOD;
                p->used = 0; p->alive = 1; p->room = rc_RoomAt(ex, ey);
            }
        }
        else rc_GainGold(60);                   // the last reward chest is still unopened
        rc_Toast(rc_kEnemyName[e->type], " defeated!");
        for(i = 0; i < RC_MAX_ENEMIES; i++)     // its summons vanish with it
            if(rc_en[i].alive && rc_en[i].sum)
            {
                rc_en[i].alive = 0;
                if(rc_alive) rc_alive--;
            }
    }

    if(e->type == ET_DRAGON)
    {
        rc_dragonAlive = 0;
        rc_dragonKills++;
        rc_GainGold(150);
        rc_DropHeart(ex, ey);
        rc_Toast("The Dragon falls!", NULL);
        for(i = 0; i < rc_nProp; i++)           // every chest refreshes after a Dragon (rc.md 15.4)
            if(rc_prop[i].kind == PR_CHEST && i != (uint16_t)rc_treasureProp && i != (uint16_t)s_rewardProp)
            {
                rc_prop[i].used  = 0;
                rc_prop[i].alive = 1;                   // an opened Mimic chest comes back too
                rc_prop[i].sub   = (rc_prop[i].sub == CH_GOOD) ? CH_GOOD : RAND(3);
            }
    }

    if(boss) rc_RefreshTheme();
}


void rc_HealEnemy(Enemy *e, int16_t amt)
{
    e->hp = (int16_t)(e->hp + amt);
    if(e->hp > e->mhp) e->hp = e->mhp;
    rc_hud |= HUD_LEFT;
}


// quiet = damage-over-time / splash tick: no blip.
void rc_DamageEnemyEx(Enemy *e, uint16_t dmg, int16_t c, int16_t s, bool quiet)
{
    (void)c; (void)s;
    if(e->type == ET_GHOST && e->st == 1) return;                       // intangible
    if(e->type == ET_HOBGOB && e->st != 3 && dmg > 1u) dmg--;           // shield (down while recovering)
    if(e->type == ET_GOLEM)
    {
        if(e->st == 5)      dmg = (uint16_t)((dmg + 1u) >> 1);
        else if(e->st == 4) dmg = (uint16_t)(dmg * 2u);
    }
    e->hp   -= (int16_t)dmg;
    e->flash = 3;
    rc_hud   |= HUD_LEFT;

    if(e->hp <= 0) { KillEnemy(e); return; }
    if(!quiet) rc_SfxSoft(&sfx_blip);
}


// Damage scaled by the Damage stat and Force, with a critical roll (1.5x).
static uint16_t RollDmg(uint16_t base, bool *crit)
{
    uint32_t pct = 100u + rc_StatPct(ST_DMG) + (rc_HasRune(RU_FORCE) ? 20u : 0u);
    uint32_t d   = ((uint32_t)base * pct + 50u) / 100u;

    *crit = (RAND(100) < (uint16_t)(RC_CRIT_BASE_PCT + rc_StatPct(ST_CRIT)));
    if(*crit) d = (d * 3u + 1u) >> 1;
    return (d < 1u) ? 1u : (uint16_t)d;
}

static uint16_t ScaledDmg(uint16_t base)
{
    bool c;
    return RollDmg(base, &c);
}


//-----------------------------------------------------------------------------
// Objects
//-----------------------------------------------------------------------------
Obj *rc_NewObj(uint16_t kind)
{
    uint16_t i, pick = 0xFFFFu, oldest = 0;
    Obj     *o;

    for(i = 0; i < RC_MAX_OBJ; i++)
        if(!rc_obj[i].alive) { pick = i; break; }

    if(pick == 0xFFFFu)                         // full: recycle a spark or the oldest cloud / area
    {
        for(i = 0; i < RC_MAX_OBJ; i++)
        {
            if(rc_obj[i].kind != OB_SPARK && rc_obj[i].kind != OB_CLOUD && rc_obj[i].kind != OB_AREA) continue;
            if(pick == 0xFFFFu || rc_obj[i].kind == OB_SPARK || rc_obj[i].age > oldest)
            {
                pick   = i;
                oldest = rc_obj[i].age;
                if(rc_obj[i].kind == OB_SPARK) break;
            }
        }
        if(pick == 0xFFFFu) return NULL;
    }

    o = &rc_obj[pick];
    o->x = o->y = 0;
    o->vx = o->vy = o->z = o->r = 0;
    o->kind = kind;
    o->pal = o->sc = o->life = o->age = o->aux = 0;
    o->alive = 1;
    return o;
}


void rc_DropHeart(int32_t x, int32_t y)
{
    Obj *o = rc_NewObj(OB_HEART);

    if(o == NULL) return;
    o->x = x;  o->y = y;
    o->pal = PT_HEART;
    o->life = 600;
}


void rc_AddSpark(int32_t x, int32_t y, int16_t z, uint16_t sc, uint16_t pal, uint16_t life)
{
    Obj *o = rc_NewObj(OB_SPARK);

    if(o == NULL) return;
    o->x = x;  o->y = y;  o->z = z;
    o->sc = sc;  o->pal = pal;  o->life = life;
}


static void Shatter(Obj *o)
{
    uint16_t kind = o->aux;
    int16_t  rad  = rc_kPotRadius[kind];
    uint16_t dmg  = ScaledDmg(rc_kPotDmg[kind]);
    uint16_t i;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        Enemy *e = &rc_en[i];

        if(!e->alive) continue;
        if(DistP(e->x, e->y, o->x, o->y) >= rad + (rc_kEnemy[e->type].radius >> 1)) continue;

        rc_DamageEnemyEx(e, dmg, 0, 0, true);
        if(!e->alive) continue;
        if(kind == PK_FIRE)       e->burn = 120;
        else if(kind == PK_FROST) { e->freeze = 60; e->slow = 220; }
        else                      e->slow = 60;
    }

    o->kind = OB_CLOUD;
    o->age  = 0;
    o->life = rc_kPotLife[kind];
    o->sc   = rc_kPotScale[kind];
    o->pal  = PT_PUFF_FIRE + kind;
    o->z    = 0;
    o->vx   = o->vy = 0;
    rc_Sfx(&sfx_explosion);
}


void rc_UpdateObjects(void)
{
    uint16_t i, j;
    int32_t  nx, ny;
    int16_t  dp;

    for(i = 0; i < RC_MAX_OBJ; i++)
    {
        Obj *o = &rc_obj[i];

        if(!o->alive) continue;
        dp = DistP(rc_px, rc_py, o->x, o->y);

        switch(o->kind)
        {
            case OB_FLASK:
                o->age++;
                nx = o->x + o->vx;
                ny = o->y + o->vy;
                if(Blocked(nx, ny, 60)) { Shatter(o); break; }
                o->x = nx;
                o->y = ny;
                o->z = (int16_t)(9 - (int16_t)((o->age * 7u) / o->life));
                for(j = 0; j < RC_MAX_ENEMIES; j++)
                {
                    if(!rc_en[j].alive) continue;
                    if(DistP(rc_en[j].x, rc_en[j].y, o->x, o->y) < rc_kEnemy[rc_en[j].type].radius + 150)
                    {
                        Shatter(o);
                        break;
                    }
                }
                if(o->kind == OB_FLASK && o->age >= o->life) Shatter(o);
                break;

            case OB_BOLT:                       // enemy projectile: dodge it
                nx = o->x + o->vx;
                ny = o->y + o->vy;
                if(Blocked(nx, ny, 80))
                {
                    if(o->aux == BK_WEB)
                    {
                        o->kind = OB_AREA;
                        o->aux  = AR_WEB;
                        o->r    = 450;
                        o->age  = 0;
                        o->life = 140;
                    }
                    else o->alive = 0;
                    break;
                }
                o->x = nx;
                o->y = ny;
                if(DistP(rc_px, rc_py, o->x, o->y) < ((o->aux == BK_FLAME) ? 360 : 300))
                {
                    o->alive = 0;
                    if(o->aux == BK_WEB)
                    {
                        rc_pSlow = 120;
                        rc_hud  |= HUD_HINT;
                    }
                    else
                    {
                        bool hit = (rc_invuln == 0 && rc_hp != 0 && rc_aegis == 0);

                        if(o->aux == BK_ICE) { rc_pSlow = 100; rc_hud |= HUD_HINT; }
                        if(o->aux == BK_DRAIN && hit)
                        {
                            for(j = 0; j < RC_MAX_ENEMIES; j++)
                                if(rc_en[j].alive && (rc_en[j].type == ET_VAMPIRE || rc_en[j].type == ET_LICH))
                                    rc_HealEnemy(&rc_en[j], 10);
                        }
                        rc_HurtPlayer(o->sc ? o->sc : DMG_SMALL);
                    }
                    break;
                }
                if(++o->age >= o->life) o->alive = 0;
                break;

            case OB_WARN:
                if(++o->age >= o->life)
                {
                    if(o->vx)
                    {
                        o->kind = OB_AREA;
                        o->life = (uint16_t)o->vx;
                        o->age  = 0;
                        o->vx   = 0;
                        if(o->aux == AR_BLAST) rc_Sfx(&sfx_explosion);
                    }
                    else o->alive = 0;
                }
                break;

            case OB_AREA:                       // floor hazard: hurts or slows the player only
                if(++o->age >= o->life) { o->alive = 0; break; }
                if(dp < o->r)
                {
                    if(o->aux == AR_WEB)
                    {
                        if(rc_pSlow < 20u) { rc_pSlow = 20; rc_hud |= HUD_HINT; }
                    }
                    else if((o->age % 12u) == 0) rc_HurtPlayer(DMG_SMALL);
                }
                break;

            case OB_WAVE:
                o->r = (int16_t)(o->r + o->vx);
                if(o->r >= o->vy) { o->alive = 0; break; }
                if(!o->sc && Abs16((int16_t)(dp - o->r)) < 260)
                {
                    o->sc = 1;
                    rc_HurtPlayer(DMG_BOSS);
                    if(o->aux == WV_ICE) { rc_pSlow = 100; rc_hud |= HUD_HINT; }
                }
                break;

            case OB_CLOUD:
                if(++o->age >= o->life) { o->alive = 0; break; }
                if(o->aux == PK_VENOM && (o->age % 12u) == 0)
                {
                    for(j = 0; j < RC_MAX_ENEMIES; j++)
                    {
                        Enemy *e = &rc_en[j];

                        if(!e->alive) continue;
                        if(DistP(e->x, e->y, o->x, o->y) >=
                           rc_kPotRadius[PK_VENOM] + (rc_kEnemy[e->type].radius >> 1))
                            continue;
                        rc_DamageEnemyEx(e, ScaledDmg(1), 0, 0, true);
                        if(e->alive) e->slow = 30;
                    }
                }
                break;

            case OB_MANA:                       // mana bottle (at full mana it stays on the floor)
                if(++o->age >= o->life) { o->alive = 0; break; }
                o->z = (int16_t)((o->age >> 3) & 1u);
                if(rc_mana < rc_ManaMax() && dp < MANA_PICKUP_R)
                {
                    rc_mana += MANA_BOTTLE;
                    if(rc_mana > rc_ManaMax()) rc_mana = rc_ManaMax();
                    o->alive = 0;
                    rc_Sfx(&sfx_powerup);
                }
                break;

            case OB_HEART:                      // health pickup: one full heart
                if(++o->age >= o->life) { o->alive = 0; break; }
                o->z = (int16_t)((o->age >> 3) & 1u);
                if(rc_hp < rc_maxHp && dp < MANA_PICKUP_R)
                {
                    rc_HealPlayer(HP_UNITS_PER_HEART);
                    o->alive = 0;
                    rc_Sfx(&sfx_powerup);
                }
                break;

            default:                            // OB_SPARK
                if(++o->age >= o->life) o->alive = 0;
                break;
        }
    }
}


//-----------------------------------------------------------------------------
// Staffs: hitscan beams. A Hobgoblin's shield stops the beam (it does not
// pierce through to the Goblins hiding behind it).
//-----------------------------------------------------------------------------
static void FireBeam(int16_t angle, uint16_t st)
{
    uint16_t tile, side;
    int16_t  c, s, best;
    int32_t  wall, bestAlong, along, perp;
    uint16_t hitMask = 0, left, i, dmg;
    int16_t  rx, ry;
    bool     crit;

    angle &= RC_ANGLE_MASK;
    wall = (int32_t)rc_RC_CastRay(angle, &tile, &side) << 2;
    c = COS(angle);
    s = SIN(angle);

    left = 1u + rc_kStaff[st].pierce;

    while(left--)
    {
        best      = -1;
        bestAlong = wall;

        for(i = 0; i < RC_MAX_ENEMIES; i++)
        {
            if(!rc_en[i].alive || (hitMask & (1u << i))) continue;
            if(rc_en[i].type == ET_GHOST && rc_en[i].st == 1u) continue;

            rx    = RD(rc_en[i].x - rc_px);
            ry    = RD(rc_en[i].y - rc_py);
            along = ((int32_t)rx * c + (int32_t)ry * s) >> 14;
            if(along <= 0 || along >= bestAlong) continue;

            perp = ((int32_t)ry * c - (int32_t)rx * s) >> 14;
            if(perp < 0) perp = -perp;
            if(perp > rc_kEnemy[rc_en[i].type].radius) continue;

            best      = (int16_t)i;
            bestAlong = along;
        }

        if(best < 0) break;

        hitMask |= (1u << best);
        dmg = RollDmg(rc_kStaff[st].dmg, &crit);
        if(crit) rc_AddSpark(rc_en[best].x, rc_en[best].y, 8, 6, PT_PUFF_ARCANE, 5);
        if(rc_en[best].type == ET_HOBGOB && rc_en[best].st != 3u) left = 0;     // shield blocks the beam
        rc_DamageEnemyEx(&rc_en[best], dmg, c, s, false);
    }
}


static void Fire(void)
{
    uint16_t     st = rc_equip;
    const Staff *sp = &rc_kStaff[st];
    uint16_t     i, n = sp->beams;
    int16_t      off, spread = (int16_t)sp->spread;

    if(rc_HasRune(RU_ECHOES) && RAND(100) < 20u && n < 5u) { n++; if(spread == 0) spread = 10; }

    rc_cdMax[st] = StaffCooldown(st);
    rc_cd[st]    = rc_cdMax[st];
    rc_fxN       = n;
    rc_fxColor   = sp->color;

    for(i = 0; i < n; i++)
    {
        off = (int16_t)((((int16_t)(2 * i) - (int16_t)(n - 1)) * spread) / 2);
        FireBeam(rc_ang + off, st);
        rc_fxX[i] = (int16_t)(RC_VIEW_X0 + RC_VIEW_W / 2 + off * RC_PX_PER_UNIT);
    }

    rc_fxTimer   = 4;
    rc_viewDirty = 2;
    if(rc_sfxHold == 0) Buzzer_PlaySfx(rc_kStaffSfx[st]);
    rc_stepBlock = 5;
}


//-----------------------------------------------------------------------------
// Magic items (placeholder: they become inventory consumables in Pass 2)
//-----------------------------------------------------------------------------
static bool ThrowFlask(uint16_t kind)
{
    int16_t c = COS(rc_ang);
    int16_t s = SIN(rc_ang);
    int32_t sx, sy;
    Obj    *o = rc_NewObj(OB_FLASK);

    if(o == NULL) return false;

    sx = rc_px + (((int32_t)c * 300L) >> 14);
    sy = rc_py + (((int32_t)s * 300L) >> 14);
    if(Blocked(sx, sy, 60)) { sx = rc_px; sy = rc_py; }

    o->x    = sx;
    o->y    = sy;
    o->vx   = (int16_t)(((int32_t)c * 110L) >> 14);
    o->vy   = (int16_t)(((int32_t)s * 110L) >> 14);
    o->z    = 9;
    o->aux  = kind;
    o->life = 42;
    o->pal  = PT_FLASK_FIRE + kind;
    Buzzer_PlaySfx(&sfx_jump);
    return true;
}


// Nearest visible enemies in front of the player (arc +-90 units, line of sight).
static uint16_t AcquireTargets(uint16_t maxN, uint16_t *out)
{
    int16_t  c = COS(rc_ang);
    int16_t  s = SIN(rc_ang);
    uint16_t cand[RC_MAX_ENEMIES], candD[RC_MAX_ENEMIES];
    uint16_t nc = 0, n = 0, i, k, tile, side;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        const Enemy *e = &rc_en[i];
        int16_t rx, ry, along, sd, ratio, theta;
        uint16_t wd;

        if(!e->alive) continue;
        if(e->type == ET_GHOST && e->st == 1u) continue;
        rx    = RD(e->x - rc_px);
        ry    = RD(e->y - rc_py);
        along = (int16_t)(((int32_t)rx * c + (int32_t)ry * s) >> 14);
        if(along < 200) continue;

        sd    = (int16_t)(((int32_t)ry * c - (int32_t)rx * s) >> 14);
        ratio = (int16_t)(((int32_t)sd * 256L) / along);
        theta = (ratio < 0) ? -ratio : ratio;
        if(theta > 256) continue;
        theta = rc_atan[theta];
        if(theta > 90) continue;
        if(ratio < 0) theta = -theta;

        wd = rc_RC_CastRay(rc_ang + theta, &tile, &side);
        if(wd < (uint16_t)(along >> 2)) continue;

        cand[nc]  = i;
        candD[nc] = (uint16_t)along;
        nc++;
    }

    while(n < maxN && nc > 0)
    {
        k = 0;
        for(i = 1; i < nc; i++)
            if(candD[i] < candD[k]) k = i;
        out[n++] = cand[k];
        nc--;
        cand[k]  = cand[nc];
        candD[k] = candD[nc];
    }
    return n;
}


static bool CastTome(void)
{
    uint16_t idx[3], n, i;
    uint16_t dmg = ScaledDmg(3);

    n = AcquireTargets(3, idx);
    if(n == 0) { rc_Sfx(&sfx_menu_bump); return false; }

    for(i = 0; i < n; i++)
    {
        Enemy *e = &rc_en[idx[i]];

        rc_AddSpark(e->x, e->y, 6, 8, PT_PUFF_ARCANE, 8);
        rc_DamageEnemyEx(e, dmg, 0, 0, false);
    }
    Buzzer_PlaySfx(&sfx_staff_prism);
    return true;
}


static void CastWarp(void)
{
    int16_t c = COS(rc_ang);
    int16_t s = SIN(rc_ang);
    int32_t nx, ny;
    int16_t i;

    for(i = 0; i < 24; i++)                     // up to 3 tiles, never into a wall
    {
        nx = rc_px + (((int32_t)c * 128L) >> 14);
        ny = rc_py + (((int32_t)s * 128L) >> 14);
        if(Blocked(nx, ny, RC_PLAYER_R)) break;
        rc_px = nx;
        rc_py = ny;
    }

    if(rc_invuln < 20) rc_invuln = 20;
    rc_stepAcc   = 0;
    rc_viewDirty = 2;
    Buzzer_PlaySfx(&sfx_staff_storm);
}


static bool UseItem(uint16_t it)
{
    switch(it)
    {
        case IT_FIRE:
        case IT_FROST:
        case IT_VENOM:
            return ThrowFlask(it);

        case IT_ELIXIR:                                 // restores all missing health
            if(rc_hp >= rc_maxHp) { rc_Sfx(&sfx_menu_bump); return false; }
            rc_HealPlayer(rc_maxHp);
            rc_Sfx(&sfx_powerup);
            return true;

        case IT_HASTE:
            rc_haste = 300;
            rc_hud  |= HUD_HINT;
            rc_Sfx(&sfx_powerup);
            return true;

        case IT_TOME:
            return CastTome();

        case IT_WARP:
            CastWarp();
            return true;

        default:                                        // IT_AEGIS: blocks the next hit
            if(rc_aegis) { rc_Sfx(&sfx_menu_bump); return false; }
            rc_aegis = 1;
            rc_hud  |= HUD_HINT;
            rc_Sfx(&sfx_menu_select);
            return true;
    }
}


void rc_UseEquipped(void)
{
    uint16_t slot = rc_equip;
    uint16_t it;
    uint16_t cost = rc_SlotMana(slot);

    if(rc_nullify) cost = (uint16_t)(cost * 2u);
    if(rc_cd[slot]) return;

    if(rc_mana < cost)
    {
        rc_cdMax[slot] = 10;
        rc_cd[slot]    = 10;
        rc_manaFlash   = 12;
        rc_Sfx(&sfx_menu_bump);
        return;
    }

    if(slot < NUM_STAFF)
    {
        Fire();
        rc_mana -= cost;
        return;
    }

    it = slot - NUM_STAFF;
    if(UseItem(it))
    {
        rc_mana       -= cost;
        rc_cdMax[slot] = ScaleCd(rc_kItem[it].cooldown);
        rc_cd[slot]    = rc_cdMax[slot];
    }
    else
        rc_cd[slot] = 10;
    rc_viewDirty = 2;
}


//-----------------------------------------------------------------------------
// Chests (rc.md 12). Placeholder loot: the legacy staffs and items (rc.md 20)
// until the Pass 2 catalog. Cursed chests will grant Mimic Cloth in Pass 2;
// for now they give loot and permanently cost one maximum heart.
//-----------------------------------------------------------------------------
static bool GiveGear(void)
{
    uint16_t pool[NUM_SLOT], n = 0, i, k;

    for(i = 1; i < NUM_SLOT; i++)
        if(!SlotOwned(i)) pool[n++] = i;
    if(n == 0u) return false;
    k = pool[RAND(n)];
    if(k < NUM_STAFF) rc_staffOwned |= (uint16_t)(1u << k);
    else              rc_itemOwned  |= (uint16_t)(1u << (k - NUM_STAFF));
    rc_equip = k;
    rc_hud  |= HUD_STAFF;
    rc_Toast("Found: ", rc_SlotName(k));
    return true;
}


void rc_OpenChest(uint16_t p)
{
    Prop *pr = &rc_prop[p];
    bool  treasure = ((int16_t)p == rc_treasureProp);

    if(pr->used) return;
    pr->used = 1;
    rc_AddSpark(pr->x, pr->y, 4, 14, PT_PUFF_ARCANE, 10);

    switch(pr->sub)
    {
        case CH_MIMIC:
        {
            Enemy *e = rc_SpawnEnemy(ET_MIMIC, pr->x, pr->y);

            if(e) { e->st = 1; e->tm = 12; }
            pr->alive = 0;
            if(treasure) rc_treasureOpen = 2;
            rc_Toast("It's a Mimic!", NULL);
            rc_Sfx(&sfx_hit);
            return;
        }

        case CH_CURSED:
            if(!GiveGear()) rc_GainGold(40);
            if(rc_maxHp > HP_UNITS_PER_HEART)
            {
                rc_maxHp = (uint16_t)(rc_maxHp - HP_UNITS_PER_HEART);
                if(rc_hp > rc_maxHp) rc_hp = rc_maxHp;
                rc_hud |= HUD_HEARTS;
            }
            rc_Toast("CURSED! -1 max heart", NULL);
            rc_Sfx(&sfx_hit);
            break;

        case CH_GOOD:
            (void)GiveGear();
            rc_GainGold(60);
            if(rc_toastT == 0u) rc_Toast("Treasure: gold!", NULL);
            rc_Sfx(&sfx_coin);
            break;

        default:                                        // regular
            if(RAND(4) == 0u || !GiveGear())
            {
                rc_GainGold(25);
                rc_Toast("Found gold", NULL);
            }
            rc_Sfx(&sfx_coin);
            break;
    }
    if(treasure) rc_treasureOpen = 0;
}


//-----------------------------------------------------------------------------
// Spawning
//-----------------------------------------------------------------------------
Enemy *rc_SpawnEnemy(uint16_t type, int32_t x, int32_t y)
{
    uint16_t i;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        Enemy *e = &rc_en[i];

        if(!e->alive)
        {
            int16_t hp = rc_kEnemy[type].hp;

            if(RC_IS_MINI(type))                        // each mini-boss beaten makes the next a bit tougher
            {
                uint16_t k = (rc_miniKills > 10u) ? 10u : rc_miniKills;
                hp = (int16_t)(((int32_t)hp * (10 + (int16_t)k)) / 10);
            }

            e->x = x;       e->y = y;
            e->hp = hp;     e->mhp = hp;
            e->type = type;
            e->flash = 0;   e->slow = 0;
            e->freeze = 0;  e->burn = 0;
            e->aux = 0;     e->aux2 = 0;
            e->st = 0;      e->an = 0;
            e->cnt = 0;     e->sum = 0;
            e->tx = 0;      e->ty = 0;
            e->tm = (type == ET_BAT) ? 0u : (uint16_t)(60u + RAND(120));
            e->alive = 1;
            rc_alive++;
            rc_hud |= HUD_LEFT;
            return e;
        }
    }
    return NULL;
}


uint16_t rc_CountType(uint16_t type)
{
    uint16_t i, n = 0;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
        if(rc_en[i].alive && rc_en[i].type == type) n++;
    return n;
}


uint16_t rc_CountBosses(void)
{
    uint16_t i, n = 0;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
        if(rc_en[i].alive && RC_IS_BOSS(rc_en[i].type)) n++;
    return n;
}


// Regular enemies that must die to clear the wave (not summons, bosses or Mimics).
uint16_t rc_CountRegular(void)
{
    uint16_t i, n = 0;

    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        const Enemy *e = &rc_en[i];

        if(e->alive && !e->sum && !RC_IS_BOSS(e->type) && e->type != ET_MIMIC) n++;
    }
    return n;
}


// Spawn point: a chamber marker the enemies can reach. The player's own chamber
// first; otherwise the chamber closest along the paths (rc.md 3.3). den = the Den markers.
bool rc_PickMarker(bool den, int32_t *x, int32_t *y)
{
    uint16_t i, bestRoom = RM_NONE, bestD = 0xFFFFu, cand[RC_MAX_SPAWNS], n = 0;

    rc_EnsureFlow();
    for(i = 0; i < rc_spCount; i++)
    {
        uint16_t cell = (uint16_t)(((uint16_t)(rc_spY[i] >> 10) << RC_MAP_SHIFT) + (uint16_t)(rc_spX[i] >> 10));
        uint16_t f    = rc_flow[cell];

        if((rc_spRoom[i] == RM_DEN) != den) continue;
        if(f == 0xFFFFu) continue;                      // behind a locked door
        if(DistP(rc_spX[i], rc_spY[i], rc_px, rc_py) <= SPAWN_MIN_DIST) continue;
        if(rc_spRoom[i] == rc_playerRoom) f = 0;        // the player's chamber wins
        if(f < bestD) { bestD = f; bestRoom = rc_spRoom[i]; }
    }
    if(bestRoom == RM_NONE) return false;

    for(i = 0; i < rc_spCount; i++)
    {
        uint16_t cell = (uint16_t)(((uint16_t)(rc_spY[i] >> 10) << RC_MAP_SHIFT) + (uint16_t)(rc_spX[i] >> 10));

        if(rc_spRoom[i] != bestRoom || rc_flow[cell] == 0xFFFFu) continue;
        if(DistP(rc_spX[i], rc_spY[i], rc_px, rc_py) <= SPAWN_MIN_DIST) continue;
        cand[n++] = i;
    }
    if(n == 0u) return false;
    i = cand[RAND(n)];
    *x = rc_spX[i];
    *y = rc_spY[i];
    return true;
}


static bool Unlocked(uint16_t type)
{
    uint16_t u;

    for(u = 0; u < NUM_ORDINARY && u < rc_wave; u++)
        if(rc_kUnlock[u] == type) return true;
    return false;
}


// About three quarters of a wave comes from its theme, the rest is filler (rc.md 13.2).
static uint16_t PickType(void)
{
    const WaveTheme *th = (rc_lv.theme == 0xFFu) ? &rc_kOpening[rc_wave - 1u] : &rc_kThemes[rc_lv.theme];
    uint16_t pool[3], n = 0, i, u;

    if(th->n && RAND(4) != 0u)
    {
        for(i = 0; i < th->n; i++)
            if(Unlocked(th->t[i])) pool[n++] = th->t[i];
        if(n) return pool[RAND(n)];
    }
    u = (rc_wave < NUM_ORDINARY) ? rc_wave : NUM_ORDINARY;
    return rc_kUnlock[RAND(u)];
}


// The first five mini-bosses are all different; after that, random but not twice in a row.
Enemy *rc_SpawnMini(void)
{
    int32_t  x, y;
    uint16_t k, n = 0, pool[RC_NUM_MINI], type;
    Enemy   *e;

    if(!rc_PickMarker(false, &x, &y)) return NULL;
    if(rc_miniBag != 0x1Fu)
    {
        for(k = 0; k < RC_NUM_MINI; k++)
            if(!(rc_miniBag & (1u << k))) pool[n++] = k;
    }
    else
    {
        for(k = 0; k < RC_NUM_MINI; k++)
            if(rc_kMini[k] != rc_lastMini) pool[n++] = k;
    }
    k = pool[RAND(n)];
    rc_miniBag |= (uint16_t)(1u << k);
    type = rc_kMini[k];
    rc_lastMini = type;

    e = rc_SpawnEnemy(type, x, y);
    if(e != NULL)
    {
        rc_miniAlive++;
        rc_RefreshTheme();
        rc_Toast(rc_kEnemyName[type], " appears!");
        rc_Sfx(&sfx_level_up);
    }
    return e;
}


void rc_TrySpawn(void)
{
    int32_t x, y;

    // Endless Dragon wave: escort mini-bosses arrive on a timer.
    if(rc_lv.dragon && rc_escortLeft && rc_escortTimer && --rc_escortTimer == 0u)
    {
        if(rc_alive < RC_MAX_ENEMIES - 2u && rc_SpawnMini() != NULL)
        {
            rc_escortLeft--;
            rc_escortTimer = RC_MINI_STAGGER;
        }
        else rc_escortTimer = 60;
    }

    if(rc_spawnTimer) { rc_spawnTimer--; return; }
    if(rc_roundGap || rc_denState == DEN_FIGHT) return;

    // The mini-boss joins midway through the wave (at once if the wave has no regulars).
    if(rc_miniPending && rc_toSpawn <= rc_lv.total / 2u)
    {
        if(rc_SpawnMini() != NULL) rc_miniPending = 0;
        rc_spawnTimer = 30;
        return;
    }

    if(!rc_toSpawn) return;
    if(rc_CountRegular() >= rc_lv.maxAlive || rc_alive >= RC_MAX_ENEMIES - 2u) return;
    if(!rc_PickMarker(false, &x, &y)) { rc_spawnTimer = 20; return; }
    (void)rc_SpawnEnemy(PickType(), x, y);
    rc_toSpawn--;
    rc_spawnTimer = rc_lv.gap;
}


//-----------------------------------------------------------------------------
// Hazards shared by several enemies
//-----------------------------------------------------------------------------
void rc_MakeWarn(int32_t x, int32_t y, int16_t r, uint16_t life, uint16_t area, uint16_t areaLife)
{
    Obj *o = rc_NewObj(OB_WARN);

    if(o == NULL) return;
    o->x    = x;
    o->y    = y;
    o->r    = r;
    o->life = life;
    o->aux  = area;
    o->vx   = (int16_t)areaLife;
}


void rc_MakeWave(int32_t x, int32_t y, int16_t speed, int16_t maxR, uint16_t flavour)
{
    Obj *o = rc_NewObj(OB_WAVE);

    if(o == NULL) return;
    o->x   = x;
    o->y   = y;
    o->r   = 200;
    o->vx  = speed;
    o->vy  = maxR;
    o->aux = flavour;
    o->sc  = 0;
    rc_Sfx(&sfx_explosion);
}


// Bomber blast: hurts the player (a big hit) and every enemy close by.
void rc_Explode(int32_t x, int32_t y)
{
    uint16_t i;
    Obj     *o;

    rc_AddSpark(x, y, 4, 20, PT_PUFF_FIRE, 12);
    rc_AddSpark(x + 300L, y, 6, 14, PT_PUFF_FIRE, 10);
    rc_AddSpark(x - 300L, y, 6, 14, PT_PUFF_FIRE, 10);
    if(DistP(rc_px, rc_py, x, y) < 1300) rc_HurtPlayer(DMG_BIG);
    for(i = 0; i < RC_MAX_ENEMIES; i++)
    {
        Enemy *e = &rc_en[i];

        if(!e->alive) continue;
        if(DistP(e->x, e->y, x, y) < 1300) rc_DamageEnemyEx(e, 4, 0, 0, true);
    }
    o = rc_NewObj(OB_AREA);
    if(o != NULL) { o->x = x; o->y = y; o->r = 800; o->life = 45; o->aux = AR_BLAST; }
    rc_Sfx(&sfx_explosion);
}


const int16_t rc_kNbDx[4] = { 1, -1, 0, 0 };
const int16_t rc_kNbDy[4] = { 0, 0, 1, -1 };
