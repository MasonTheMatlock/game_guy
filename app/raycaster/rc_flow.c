// rc_flow.c - run flow: waves, doors, keys, secret doors, the Dragon Den, the player.
#include "rc_priv.h"

static uint16_t s_dragonWaves;          // endless Dragon waves started so far
static uint16_t s_hazardTick;           // special-room hazard timer
static uint16_t s_waveSfx;


//*****************************************************************************
// Run / level flow
//*****************************************************************************
void rc_ResetRun(void)
{
    uint16_t i;

    rc_hp = rc_maxHp = HP_START;
    rc_gold = 0;
    rc_kills = 0;
    for(i = 0; i < ST_COUNT; i++) rc_lvStat[i] = 0;
    for(i = 0; i < RUNE_SLOTS; i++) rc_rune[i] = RUNE_NONE;
    rc_classSeen = 0;
    rc_staffOwned = 1;
    rc_itemOwned  = 0;
    rc_equip = 0;
    for(i = 0; i < NUM_SLOT; i++) { rc_cd[i] = 0; rc_cdMax[i] = 0; }
    rc_haste = rc_aegis = rc_momentum = 0;
    rc_leechCnt = 0;
    rc_xp = 0;
    rc_xpLv = 1;
    rc_lvPending = 0;
    rc_mana = MANA_BASE;
    rc_manaAcc = rc_manaFlash = 0;
    rc_manaShown = -1;
    rc_keys = 0;
    rc_keyTaken = 0;
    rc_pSlow = rc_nullify = rc_pull = 0;
    rc_miniKills = rc_miniAlive = rc_miniPending = 0;
    rc_miniBag = 0;
    rc_lastMini = 0xFFFFu;
    rc_dragonKills = 0;
    rc_dragonAlive = 0;
    rc_escortLeft = 0;
    s_dragonWaves = 0;
    rc_lastTheme = 0xFFFFu;
    rc_treasureProp = -1;
    rc_treasureOpen = 0;
    rc_playerRoom = RM_S;
    rc_seed = Rand16();
    rc_ApplyTheme(TH_WARRENS);
    rc_wLoaded = 0xFFFFu;
}


uint16_t rc_ManaMax(void)
{
    return (uint16_t)(MANA_BASE + rc_StatPct(ST_MANA));
}


uint16_t rc_MaxHpCap(void)
{
    return HP_CAP;
}


// Wave theme: the fixed opening for waves 1..10, then a random theme whose
// enemies are all unlocked, never the same one twice in a row.
static uint16_t PickTheme(uint16_t w)
{
    uint16_t pool[NUM_THEMES], n = 0, i, k, ok;

    if(w <= 10u) return 0xFFu;
    for(i = 0; i < NUM_THEMES; i++)
    {
        if(i == rc_lastTheme) continue;
        ok = 1;
        for(k = 0; k < rc_kThemes[i].n; k++)
        {
            uint16_t u, found = 0;

            for(u = 0; u < NUM_ORDINARY && u < w; u++)
                if(rc_kUnlock[u] == rc_kThemes[i].t[k]) found = 1;
            if(!found) ok = 0;
        }
        if(ok) pool[n++] = i;
    }
    if(n == 0u) return 0u;
    k = pool[RAND(n)];
    rc_lastTheme = k;
    return k;
}


// Treasure wave: one chest in a random reachable chamber (not necessarily the player's).
static void PlaceTreasure(void)
{
    uint16_t cand[RC_MAX_SPAWNS], n = 0, i, k;
    int16_t  p = rc_treasureProp;

    rc_EnsureFlow();
    for(i = 0; i < rc_spCount; i++)
    {
        uint16_t cell = (uint16_t)(((uint16_t)(rc_spY[i] >> 10) << RC_MAP_SHIFT) + (uint16_t)(rc_spX[i] >> 10));

        if(rc_spRoom[i] == RM_DEN) continue;
        if(rc_flow[cell] == 0xFFFFu) continue;
        if(DistP(rc_spX[i], rc_spY[i], rc_px, rc_py) < 1500) continue;
        cand[n++] = i;
    }
    if(n == 0u) return;
    k = cand[RAND(n)];

    if(p < 0) p = rc_AddProp(PR_CHEST, 0, rc_spX[k], rc_spY[k]);
    if(p < 0) return;
    rc_prop[p].x = rc_spX[k];
    rc_prop[p].y = rc_spY[k];
    rc_prop[p].room = rc_spRoom[k];
    rc_prop[p].kind = PR_CHEST;
    rc_prop[p].sub  = RAND(3);              // regular, Mimic or cursed, equal chance
    rc_prop[p].used = 0;
    rc_prop[p].alive = 1;
    rc_treasureProp = p;
    rc_treasureOpen = 1;
}


static void StartWave(uint16_t w)
{
    LevelDef *o = &rc_lv;
    bool      endless = (rc_dragonKills > 0u);
    uint16_t  t;

    rc_wave     = w;
    o->dragon   = (endless && (w % RC_DRAGON_EVERY) == 0u) ? 1u : 0u;
    o->treasure = (!o->dragon && (w % RC_TREASURE_EVERY) == 0u) ? 1u : 0u;
    o->mini     = (!o->dragon && (w % RC_MINI_EVERY) == 0u) ? 1u : 0u;
    o->theme    = PickTheme(w);

    t = (uint16_t)(8u + 3u * (w - 1u));
    if(t > 70u) t = 70u;
    o->total = (o->dragon || o->treasure) ? 0u : t;

    t = (uint16_t)(4u + (w - 1u) / 2u);
    if(t > RC_ALIVE_CAP) t = RC_ALIVE_CAP;
    o->maxAlive = t;
    o->gap      = SPAWN_GAP_FRAMES;

    rc_toSpawn     = o->total;
    rc_spawnTimer  = 45;
    rc_roundGap    = 0;
    rc_miniPending = o->mini;

    if(o->treasure) PlaceTreasure();
    if(o->dragon)
    {
        int32_t x, y;

        s_dragonWaves++;
        rc_escortLeft  = (s_dragonWaves > 5u) ? 5u : s_dragonWaves;
        rc_escortTimer = 300;
        if(rc_PickMarker(true, &x, &y) && rc_SpawnEnemy(ET_DRAGON, x, y) != NULL)
        {
            rc_dragonAlive = 1;
            rc_RefreshTheme();
        }
    }

    // vendors restock every three waves (rc.md 11.1)
    if(((w - 1u) % 3u) == 0u)
    {
        uint16_t i;
        for(i = 0; i < rc_nProp; i++)
            if(rc_prop[i].kind == PR_VENDOR) rc_prop[i].used = 0;
    }

    rc_hud |= HUD_TOPR | HUD_LEFT | HUD_HINT;
    if(w > 1u) rc_Sfx(&sfx_level_up);
}


// New run: load the map, build the maze, lock every door, start wave 1.
void rc_StartLevel(void)
{
    uint16_t i;

    rc_LoadMap();
    rc_BuildMaze();

    for(i = 0; i < RC_MAX_ENEMIES; i++) rc_en[i].alive = 0;
    for(i = 0; i < RC_MAX_OBJ; i++)     rc_obj[i].alive = 0;
    for(i = 0; i <= RC_NUM_DOORS; i++)  rc_doorBought[i] = false;
    rc_doorsBought = 0;
    rc_denState    = DEN_SEALED;
    rc_iaKind      = IA_NONE;
    rc_iaShown     = 0xFFFFu;
    rc_flowTx      = -1;
    rc_alive       = 0;
    s_hazardTick   = 0;

    StartWave(1);
    rc_spawnTimer = 90;

    rc_ang    = 768;                        // face north, into the Goblin Warrens
    rc_invuln = 60;
    rc_fxTimer = 0;
    rc_stepAcc = 0;

    rc_mode      = MODE_INTRO;
    rc_timer     = 75;
    rc_banner    = BAN_LEVEL;
    rc_dirtyFull = true;
}


//*****************************************************************************
// Interaction (stick button): doors, secret doors, the maze exit, chests,
// vendors and altars. The nearest thing in front of the player wins.
//*****************************************************************************
static bool InFront(int32_t x, int32_t y, int16_t range, int16_t *dOut)
{
    int16_t dx = RD(x - rc_px), dy = RD(y - rc_py);
    int16_t d  = Dist10(dx, dy);
    int32_t fwd;

    if(d >= range) return false;
    fwd = (((int32_t)dx * COS(rc_ang)) + ((int32_t)dy * SIN(rc_ang))) >> 14;
    if(d > 500 && fwd * 10L < (int32_t)d * 6L) return false;       // roughly in front
    *dOut = d;
    return true;
}


static void UpdateInteract(void)
{
    uint16_t k, kind = IA_NONE, idx = 0, key;
    int16_t  best = 32767, d;

    for(k = 1; k <= RC_NUM_DOORS; k++)
    {
        if(rc_doorBought[k]) continue;
        if(InFront(rc_doorCx[k], rc_doorCy[k], DOOR_USE_DIST, &d) && d < best)
        { best = d; kind = IA_DOOR; idx = k; }
    }

    for(k = 0; k < rc_nProp; k++)
    {
        const Prop *p = &rc_prop[k];
        uint16_t    ia;

        if(!p->alive || p->used) continue;
        switch(p->kind)
        {
            case PR_CHEST:        ia = IA_CHEST;  break;
            case PR_VENDOR:       ia = IA_VENDOR; break;
            case PR_ALTAR_SHRINE: ia = IA_SHRINE; break;
            case PR_ALTAR_LAB:    ia = IA_LAB;    break;
            default:              continue;
        }
        if(InFront(p->x, p->y, INTERACT_DIST, &d) && d < best)
        { best = d; kind = ia; idx = k; }
    }

    // Secret doors and the maze exit: the wall cell just in front of the player.
    {
        int32_t  fx = rc_px + (((int32_t)COS(rc_ang) * 900L) >> 14);
        int32_t  fy = rc_py + (((int32_t)SIN(rc_ang) * 900L) >> 14);
        uint16_t tx = (uint16_t)(fx >> 10), ty = (uint16_t)(fy >> 10);

        if(tx < RC_MAP_W && ty < RC_MAP_H && rc_tile[(ty << RC_MAP_SHIFT) + tx] != 0u)
        {
            char ch = rc_kMap[ty][tx];

            if(ch == 's' && best > 900)
            { kind = IA_SECRET; idx = (uint16_t)((ty << RC_MAP_SHIFT) + tx); }
            else if(ch == 'x' && rc_playerRoom == RM_MAZE && best > 900)       // one way: only from inside
            { kind = IA_EXIT; idx = (uint16_t)((ty << RC_MAP_SHIFT) + tx); }
        }
    }

    rc_iaKind = kind;
    rc_iaIdx  = idx;
    key = (uint16_t)((kind << 8) | (idx & 0xFFu));
    if(kind == IA_DOOR) key ^= (rc_gold >= rc_DoorCost()) ? 0x80u : 0u;
    if(kind == IA_SECRET) key ^= rc_keys ? 0x80u : 0u;
    if(key != rc_iaShown)
    {
        rc_iaShown = key;
        rc_hud    |= HUD_HINT;
    }
}


static void BuyDoor(uint16_t k)
{
    uint16_t cost = rc_DoorCost();

    if(rc_gold < cost) { rc_Sfx(&sfx_menu_bump); return; }

    rc_gold -= cost;
    rc_doorsBought++;
    rc_doorBought[k] = true;
    rc_SetTiles((char)('0' + k), 0);           // the doorway is now open floor
    rc_iaShown = 0xFFFFu;
    rc_hud    |= HUD_GOLD | HUD_HINT;
    rc_Sfx(&sfx_level_up);
}


// Open a secret door with a key: the cell and any 's' neighbours (the Vault door is two thick).
static void OpenSecret(uint16_t cell)
{
    uint16_t tx = cell & (RC_MAP_W - 1u), ty = cell >> RC_MAP_SHIFT, i;

    if(rc_keys == 0u) { rc_Sfx(&sfx_menu_bump); rc_hud |= HUD_HINT; return; }
    rc_keys--;
    rc_tile[cell] = 0;
    for(i = 0; i < 4u; i++)
    {
        int16_t nx = (int16_t)(tx + rc_kNbDx[i]), ny = (int16_t)(ty + rc_kNbDy[i]);

        if((uint16_t)nx < RC_MAP_W && (uint16_t)ny < RC_MAP_H && rc_kMap[ny][nx] == 's')
            rc_tile[((uint16_t)ny << RC_MAP_SHIFT) + (uint16_t)nx] = 0;
    }
    rc_flowTx = -1;
    rc_viewDirty = 2;
    rc_iaShown = 0xFFFFu;
    rc_hud |= HUD_TOPR | HUD_HINT;
    rc_Sfx(&sfx_powerup);
}


static void Interact(void)
{
    switch(rc_iaKind)
    {
        case IA_DOOR:   BuyDoor(rc_iaIdx); break;
        case IA_SECRET: OpenSecret(rc_iaIdx); break;
        case IA_EXIT:
            rc_tile[rc_iaIdx] = 0;
            rc_flowTx = -1;
            rc_viewDirty = 2;
            rc_Sfx(&sfx_menu_select);
            break;
        case IA_CHEST:  rc_OpenChest(rc_iaIdx); break;
        case IA_VENDOR: rc_EnterCards(CS_VENDOR); break;       // rc_iaIdx = the vendor's prop
        case IA_SHRINE: rc_EnterCards(CS_SHRINE); break;
        case IA_LAB:    rc_EnterCards(CS_LAB); break;
        default: break;
    }
    rc_iaShown = 0xFFFFu;
    rc_hud |= HUD_HINT;
}


// Keys are picked up by walking over them (carry limit 3).
static void PickupKeys(void)
{
    uint16_t k;

    for(k = 0; k < rc_nProp; k++)
    {
        Prop *p = &rc_prop[k];

        if(!p->alive || p->used || p->kind != PR_KEY) continue;
        if(DistP(p->x, p->y, rc_px, rc_py) >= 520) continue;
        if(rc_keys >= KEY_CARRY_MAX) continue;
        rc_keys++;
        rc_keyTaken |= (uint16_t)(1u << p->sub);
        p->used = 1;
        rc_hud |= HUD_TOPR;
        rc_Sfx(&sfx_coin);
    }
}


//*****************************************************************************
// The Dragon Den (rc.md 15.1): all eight doors and three mini-bosses open it,
// but only between waves. Walking in starts the fight and seals the gates.
//*****************************************************************************
static void SetGates(bool open)
{
    rc_SetTiles('D', open ? 0u : TILE_METAL_DOOR);
}


// Never close a gate on top of the player: true if the player is clear of both gates.
static bool GatesClear(void)
{
    int16_t tx = (int16_t)(rc_px >> 10), ty = (int16_t)(rc_py >> 10);

    if(tx < 27 || tx > 33) return true;
    return !((ty >= 16 && ty <= 18) || (ty >= 32 && ty <= 34));
}


// Well inside the Den (one tile in from its walls), so the sealed gate cannot touch the player.
static bool DeepInDen(void)
{
    int16_t tx = (int16_t)(rc_px >> 10), ty = (int16_t)(rc_py >> 10);

    return tx > RC_DEN_X0 && tx < RC_DEN_X1 && ty > RC_DEN_Y0 && ty < RC_DEN_Y1;
}


static void UpdateDen(void)
{
    bool unlocked = (rc_doorsBought >= RC_NUM_DOORS) && (rc_miniKills >= RC_DRAGON_MINIS);

    if(rc_denState == DEN_FIGHT && !rc_dragonAlive)     // the Dragon is dead: the gates unseal
    {
        SetGates(true);
        rc_denState = DEN_OPEN;
        rc_hud |= HUD_HINT;
    }
    if(rc_dragonKills > 0u) return;             // endless mode: the gates stay open

    switch(rc_denState)
    {
        case DEN_SEALED:
            if(unlocked && rc_roundGap) { SetGates(true); rc_denState = DEN_OPEN; rc_Sfx(&sfx_powerup); rc_hud |= HUD_HINT; }
            break;

        case DEN_OPEN:
            if(rc_playerRoom == RM_DEN && DeepInDen())
            {
                int32_t x, y;

                SetGates(false);                // no retreat
                rc_denState = DEN_FIGHT;
                rc_roundGap = 0;
                rc_toSpawn  = 0;
                rc_miniPending = 0;
                rc_lv.treasure = 0;
                rc_lv.mini  = 0;
                rc_lv.dragon = 1;
                rc_escortLeft = 0;              // the first Dragon fights alone
                if(rc_PickMarker(true, &x, &y) && rc_SpawnEnemy(ET_DRAGON, x, y) != NULL)
                {
                    rc_dragonAlive = 1;
                    rc_RefreshTheme();
                }
                rc_hud |= HUD_ALL;
                rc_Sfx(&sfx_game_over);
            }
            else if(rc_roundGap == 0u && rc_playerRoom != RM_DEN && GatesClear())   // a wave started: shut until the next break
            {
                SetGates(false);
                rc_denState = DEN_SEALED;
                rc_hud |= HUD_HINT;
            }
            break;

        default:                                // DEN_FIGHT: no retreat until it dies
            break;
    }
}


//*****************************************************************************
// Special-room hazards: they hurt the player only (rc.md 4.8).
//*****************************************************************************
static void RoomHazards(void)
{
    uint16_t area;

    if(++s_hazardTick < 110u) return;
    s_hazardTick = 0;
    switch(rc_playerRoom)
    {
        case RM_VAULT: area = AR_SLIME;  break;
        case RM_LAB:   area = AR_ARCANE; break;
        case RM_MAZE:  area = AR_BLAST;  break;
        default: return;
    }
    rc_MakeWarn(rc_px + (int32_t)RAND(900) - 450, rc_py + (int32_t)RAND(900) - 450, 520, 40, area, 70);
}


//*****************************************************************************
// Wave clear
//*****************************************************************************
static bool WaveCleared(void)
{
    if(rc_denState == DEN_FIGHT) return false;  // the first Dragon: handled by UpdateDen
    if(rc_lv.dragon)
        return !rc_dragonAlive && rc_escortLeft == 0u && rc_miniAlive == 0u;
    if(rc_lv.treasure && !rc_lv.mini)
        return rc_treasureOpen == 0u;
    return rc_toSpawn == 0u && rc_CountRegular() == 0u && !rc_miniPending && rc_miniAlive == 0u;
}


static void OnWaveClear(void)
{
    rc_GainXp((uint16_t)(5u + 2u * rc_wave));   // wave-clear bonus grows with the wave
    if((rc_wave % 5u) == 0u) rc_lvPending++;    // milestone: a guaranteed level-up every 5th wave
    rc_roundGap = ROUND_GAP_FRAMES;
    rc_hud     |= HUD_LEFT | HUD_TOPR;
    s_waveSfx   = 1;
}


//*****************************************************************************
// One simulation tick of play
//*****************************************************************************
void rc_UpdatePlay(void)
{
    bool     moved = false;
    int16_t  jx, jy, fwd, str, c, s, mx, my, sp;
    int32_t  nx, ny;
    uint16_t i, room, pct;

    if(rc_invuln)  rc_invuln--;
    if(rc_fxTimer) rc_fxTimer--;
    if(rc_sfxHold) rc_sfxHold--;
    if(rc_stepBlock) rc_stepBlock--;
    if(rc_momentum) rc_momentum--;
    rc_portalPhase++;                           // key bob / pulse animation
    if(rc_pSlow)   { rc_pSlow--;   if(!rc_pSlow)   rc_hud |= HUD_HINT; }
    if(rc_nullify) { rc_nullify--; if(!rc_nullify) rc_hud |= HUD_HINT; }

    for(i = 0; i < NUM_SLOT; i++)
    {
        if(rc_cd[i]) rc_cd[i]--;
        if(rc_haste && rc_cd[i]) rc_cd[i]--;
    }
    if(rc_haste) { rc_haste--; if(!rc_haste) rc_hud |= HUD_HINT; }
    if(rc_toastT) { rc_toastT--; if(!rc_toastT) rc_hud |= HUD_HINT; }
    // rc_aegis is a one-hit shield (rc.md 10.2): it lasts until it blocks a hit.

    if(g_dpad.dPressed) rc_CycleEquip();

    if(g_dpad.l) { rc_ang -= RC_TURN_SPEED; moved = true; }
    if(g_dpad.r) { rc_ang += RC_TURN_SPEED; moved = true; }
    rc_ang &= RC_ANGLE_MASK;

    jx = g_joy.dx;
    jy = RC_INVERT_Y ? g_joy.dy : (int16_t)-g_joy.dy;
    if(Abs16(jx) < RC_STICK_DEAD) jx = 0;
    if(Abs16(jy) < RC_STICK_DEAD) jy = 0;

    if(jx || jy)
    {
        pct = (uint16_t)(100u + rc_StatPct(ST_MOVE));
        if(rc_HasRune(RU_HASTE)) pct += 12u;
        if(rc_momentum) pct += 30u;
        sp  = (int16_t)(((int32_t)RC_MOVE_SPEED * pct) / 100L);
        if(rc_pSlow) sp = (int16_t)(sp >> 1);
        fwd = (int16_t)(((int32_t)jy * sp * 41L) >> 12);
        str = (int16_t)(((int32_t)jx * sp * 41L) >> 12);
        c   = COS(rc_ang);
        s   = SIN(rc_ang);
        mx  = (int16_t)((((int32_t)fwd * c) - ((int32_t)str * s)) >> 14);
        my  = (int16_t)((((int32_t)fwd * s) + ((int32_t)str * c)) >> 14);

        nx = rc_px + mx;
        if(!Blocked(nx, rc_py, RC_PLAYER_R)) { rc_px = nx; moved = true; rc_stepAcc += (uint16_t)Abs16(mx); }
        ny = rc_py + my;
        if(!Blocked(rc_px, ny, RC_PLAYER_R)) { rc_py = ny; moved = true; rc_stepAcc += (uint16_t)Abs16(my); }
    }

#if RC_FOOTSTEPS
    if(rc_stepAcc >= RC_STEP_DIST)
    {
        rc_stepAcc -= RC_STEP_DIST;
        if(rc_stepAcc >= RC_STEP_DIST) rc_stepAcc = 0;
        if(rc_mode == MODE_PLAY && rc_stepBlock == 0)
            Buzzer_PlaySfx(rc_stepAlt ? &sfx_step_b : &sfx_step_a);
        rc_stepAlt ^= 1u;
    }
#endif

    if(rc_pull)                                 // the Beholder's gravity eye
    {
        int16_t gx = RD(rc_pullX - rc_px), gy = RD(rc_pullY - rc_py), gd = Dist10(gx, gy);

        rc_pull--;
        if(gd > 600)
        {
            rc_PlayerPush((int16_t)(((int32_t)gx * 24L) / gd), (int16_t)(((int32_t)gy * 24L) / gd));
            moved = true;
        }
    }

    // Mana refills while the stick is released.
    if(jx || jy)
        rc_manaAcc = 0;
    else if(!rc_nullify && rc_mana < rc_ManaMax())
    {
        uint16_t need;

        pct  = (uint16_t)(100u + rc_StatPct(ST_REGEN) + (rc_HasRune(RU_HERMIT) ? 50u : 0u));
        need = (uint16_t)((MANA_REGEN_FRAMES * 100u) / pct);
        if(need < 2u) need = 2u;
        if(++rc_manaAcc >= need) { rc_manaAcc = 0; rc_mana++; }
    }

    // Which room are we in? (doorways keep the previous room)
    room = rc_RoomAt(rc_px, rc_py);
    if(room != RM_NONE && room != rc_playerRoom)
    {
        rc_playerRoom = room;
        rc_RefreshTheme();
        rc_hud |= HUD_HINT;
    }

    PickupKeys();
    UpdateInteract();
    if(g_joy.btnPressed && rc_iaKind != IA_NONE) Interact();
    if(rc_mode != MODE_PLAY) return;            // a card screen opened

    if(g_dpad.u) rc_UseEquipped();

    rc_EnsureFlow();
    rc_UpdateObjects();
    rc_UpdateEnemies();
    rc_TrySpawn();
    RoomHazards();
    UpdateDen();

    if(rc_mode == MODE_PLAY)
    {
        if(rc_roundGap == 0u)
        {
            if(WaveCleared()) OnWaveClear();
        }
        else if(rc_denState != DEN_FIGHT && --rc_roundGap == 0u)
            StartWave((uint16_t)(rc_wave + 1u));
        else if((rc_roundGap % 30u) == 0u)
            rc_hud |= HUD_LEFT;
    }
    if(s_waveSfx) { s_waveSfx = 0; rc_Sfx(&sfx_level_up); }

    if(moved || rc_alive || rc_fxTimer || rc_haste)
        rc_viewDirty = 2;
    else
    {
        for(i = 0; i < RC_MAX_OBJ; i++)
            if(rc_obj[i].alive) { rc_viewDirty = 2; break; }
    }
}
