// rc_cards.c - card screens (level-up, vendor, shrine, rune altar), game over,
// high scores, and the title screens.
#include "rc_priv.h"

static Card     s_saved[CARDS_MAX];         // level-up cards kept while choosing a rune slot
static uint16_t s_savedN;
static uint16_t s_vendor;                   // 0 = west corridor, 1 = east corridor
static uint16_t s_stock[2][CARDS_MAX];      // vendor stock: slot index (staff / item), 0xFFFF = none
static uint16_t s_stockSold[2];
static uint16_t s_stockBlock[2] = { 0xFFFFu, 0xFFFFu };
static uint16_t s_prop;                     // the prop that opened the screen
static uint16_t s_overT;                    // game-over timer
static uint16_t s_survived;


//*****************************************************************************
// Banners
//*****************************************************************************
void rc_DrawBanner(void)
{
    char    buf[24];
    int16_t n;
    const char *sub;

    Graphics_FillRect(50, 84, SCREEN_W - 51, 140, C_BLACK);
    Graphics_FillRect(50, 84, SCREEN_W - 51, 85, C_GOLD);
    Graphics_FillRect(50, 139, SCREEN_W - 51, 140, C_GOLD);

    n = rc_Cat(buf, 0, "WAVE ", 5);
    rc_Itoa(buf + n, rc_wave);
    rc_DrawText(buf, SCREEN_W / 2, 105, rc_lv.dragon ? C_RED : C_WHITE);
    if(rc_lv.dragon)                         sub = "The Dragon returns";
    else if(rc_lv.treasure && rc_lv.mini)    sub = "Mini-boss and treasure";
    else if(rc_lv.treasure)                  sub = "Treasure wave";
    else if(rc_lv.theme == 0xFFu)            sub = rc_kOpening[rc_wave - 1u].name;
    else                                     sub = rc_kThemes[rc_lv.theme].name;
    rc_DrawText(sub, SCREEN_W / 2, 126, C_GRAY);
}


//*****************************************************************************
// Card contents
//*****************************************************************************
static bool StatAvailable(uint16_t st)
{
    if(rc_lvStat[st] >= STAT_MAX_LV) return false;
    if(st == ST_HP && rc_maxHp + HP_UNITS_PER_HEART > rc_MaxHpCap()) return false;
    return true;
}


static bool RuneAvailable(uint16_t r)
{
    if(rc_HasRune(r)) return false;
    if(rc_kRune[r].cls != 0u && !rc_classSeen) return false;    // class runes after a class (Pass 2)
    return true;
}


// n distinct random runes into rc_card[first..].
static void RollRunes(uint16_t first, uint16_t n)
{
    uint16_t pool[RU_COUNT], m = 0, i, k;

    for(i = 0; i < RU_COUNT; i++)
        if(RuneAvailable(i)) pool[m++] = i;
    for(i = 0; i < n; i++)
    {
        Card *c = &rc_card[first + i];

        c->sold = 0;
        if(m == 0u) { c->kind = CK_NONE; continue; }
        k = RAND(m);
        c->kind = CK_RUNE;
        c->idx  = pool[k];
        pool[k] = pool[--m];
    }
}


static void RollLevel(void)
{
    uint16_t pool[ST_COUNT], m = 0, i;

    for(i = 0; i < ST_COUNT; i++)
        if(StatAvailable(i)) pool[m++] = i;
    rc_card[0].sold = 0;
    if(m) { rc_card[0].kind = CK_STAT; rc_card[0].idx = pool[RAND(m)]; }
    else  rc_card[0].kind = CK_NONE;
    RollRunes(1, 2);                            // one stat upgrade and two runes (rc.md 6.1)
    rc_nCard = 3;
}


// Vendor stock (placeholder gear) refreshes every three waves.
static void VendorStock(void)
{
    uint16_t block = (uint16_t)((rc_wave - 1u) / 3u), pool[NUM_SLOT], m = 0, i, k;

    if(s_stockBlock[s_vendor] == block) return;
    s_stockBlock[s_vendor] = block;
    s_stockSold[s_vendor]  = 0;
    for(i = 1; i < NUM_SLOT; i++)
        if(i != NUM_STAFF + IT_ELIXIR) pool[m++] = i;
    s_stock[s_vendor][0] = NUM_STAFF + IT_ELIXIR;   // every vendor always has healing
    for(i = 1; i < CARDS_MAX; i++)
    {
        k = RAND(m);
        s_stock[s_vendor][i] = pool[k];
        pool[k] = pool[--m];
    }
}


static void SetupScreen(void)
{
    uint16_t i;

    rc_cardSel = 0;
    rc_cardMsg = 0;
    switch(rc_cardScreen)
    {
        case CS_LEVEL:
            RollLevel();
            break;

        case CS_VENDOR:
            VendorStock();
            for(i = 0; i < CARDS_MAX; i++)
            {
                uint16_t sl = s_stock[s_vendor][i];

                rc_card[i].kind = (sl < NUM_STAFF) ? CK_STAFF : CK_ITEM;
                rc_card[i].idx  = (sl < NUM_STAFF) ? sl : (uint16_t)(sl - NUM_STAFF);
                rc_card[i].sold = (s_stockSold[s_vendor] >> i) & 1u;
            }
            rc_nCard = CARDS_MAX;
            break;

        case CS_SHRINE:
            rc_card[0].kind = CK_BLESS_HP;   rc_card[0].idx = 0; rc_card[0].sold = 0;
            rc_card[1].kind = CK_BLESS_GOLD; rc_card[1].idx = 0; rc_card[1].sold = 0;
            rc_nCard = 2;
            break;

        case CS_LAB:
            RollRunes(0, 3);                    // the Astral Laboratory guarantees a rune
            rc_nCard = 3;
            break;

        default:                                // CS_SWAP: pick the rune to replace
            for(i = 0; i < RUNE_SLOTS; i++)
            {
                rc_card[i].kind = CK_SLOT;
                rc_card[i].idx  = i;
                rc_card[i].sold = 0;
            }
            rc_nCard = RUNE_SLOTS;
            break;
    }
    rc_cardDirty = CD_ALL;
}


static uint16_t RerollCost(void)
{
    return (rc_rerolls == 0u) ? 0u : (uint16_t)(5u * rc_rerolls);       // free, then 5, 10, 15 ...
}


static uint16_t CardCost(const Card *c)
{
    if(c->kind == CK_STAFF) return rc_kStaffCost[c->idx];
    if(c->kind == CK_ITEM)  return rc_kItemCost[c->idx];
    return 0;
}


static bool CardOwned(const Card *c)
{
    if(c->kind == CK_STAFF) return (rc_staffOwned & (1u << c->idx)) != 0;
    if(c->kind == CK_ITEM)  return (rc_itemOwned & (1u << c->idx)) != 0;
    return false;
}


//*****************************************************************************
// Drawing
//*****************************************************************************
static void CardBox(uint16_t i, int16_t *x0, int16_t *w)
{
    *w  = (int16_t)((310 - 5 * ((int16_t)rc_nCard - 1)) / (int16_t)rc_nCard);
    *x0 = (int16_t)(CARD_X0 + (int16_t)i * (*w + CARD_GAP));
}


static void DrawLines(const char *s, int16_t cx, int16_t y, uint32_t fg, uint32_t bg)
{
    char    line[16];
    uint16_t k, n = 0;

    for(k = 0; ; k++)
    {
        char ch = s[k];

        if(ch == '|' || ch == 0)
        {
            line[n] = 0;
            rc_DrawSmall(line, cx, y, fg, bg);
            y = (int16_t)(y + 15);
            n = 0;
            if(ch == 0) break;
        }
        else if(n < 14) line[n++] = ch;
    }
}


static void DrawCard(uint16_t i)
{
    int16_t  x0, w, x1, y0 = CARD_Y0, y1 = CARD_Y0 + CARD_H - 1, cx;
    bool     sel = (rc_cardSel == i);
    uint32_t fill = sel ? COLOR_CHARCOAL : COLOR_TETRIS_GRID, band = COLOR_CHARCOAL;
    const Card *c = &rc_card[i];
    const char *kind = "", *name = "", *desc = "";
    char     foot[16];
    uint32_t footC = C_GRAY;
    int16_t  n;

    CardBox(i, &x0, &w);
    x1 = (int16_t)(x0 + w - 1);
    cx = (int16_t)(x0 + w / 2);
    foot[0] = 0;

    Graphics_FillRect(x0, y0, x1, y1, sel ? C_CYAN : COLOR_CHARCOAL);
    Graphics_FillRect(x0 + 2, y0 + 2, x1 - 2, y1 - 2, fill);

    switch(c->kind)
    {
        case CK_STAT:
            kind = "UPGRADE"; band = COLOR_OLIVE;
            name = rc_kStat[c->idx].name; desc = rc_kStat[c->idx].desc;
            n = rc_Cat(foot, 0, "Lv ", 3);
            n += rc_Itoa(foot + n, (uint16_t)(rc_lvStat[c->idx] + 1u));
            n = rc_Cat(foot, n, "/", 1);
            rc_Itoa(foot + n, STAT_MAX_LV);
            footC = C_GREEN;
            break;
        case CK_RUNE:
            kind = rc_kRune[c->idx].cls ? "CLASS RUNE" : "RUNE"; band = COLOR_PURPLE;
            name = rc_kRune[c->idx].name; desc = rc_kRune[c->idx].desc;
            break;
        case CK_STAFF:
        case CK_ITEM:
            kind = (c->kind == CK_STAFF) ? "STAFF" : "MAGIC"; band = COLOR_TEAL;
            name = (c->kind == CK_STAFF) ? rc_kStaff[c->idx].name : rc_kItem[c->idx].name;
            desc = (c->kind == CK_STAFF) ? rc_kStaffDesc[c->idx] : rc_kItemDesc[c->idx];
            if(c->sold)            rc_Cat(foot, 0, "SOLD", 4);
            else if(CardOwned(c))  rc_Cat(foot, 0, "OWNED", 5);
            else
            {
                n = rc_Itoa(foot, CardCost(c));
                rc_Cat(foot, n, "g", 1);
                footC = (rc_gold >= CardCost(c)) ? C_GREEN : C_BAD;
            }
            break;
        case CK_BLESS_HP:
            kind = "BLESSING"; band = COLOR_MAROON;
            name = "Vigor"; desc = "+1 max heart|and full|health";
            break;
        case CK_BLESS_GOLD:
            kind = "BLESSING"; band = COLOR_OLIVE;
            name = "Riches"; desc = "+120 gold";
            break;
        case CK_SLOT:
            kind = "REPLACE"; band = COLOR_MAROON;
            name = rc_kRune[rc_rune[c->idx]].name; desc = rc_kRune[rc_rune[c->idx]].desc;
            break;
        default:
            rc_DrawSmall("none left", cx, (int16_t)(y0 + CARD_H / 2), C_DKGRAY, fill);
            return;
    }

    Graphics_FillRect(x0 + 2, y0 + 2, x1 - 2, y0 + 17, band);
    rc_DrawSmall(kind, cx, (int16_t)(y0 + 10), C_WHITE, band);
    rc_DrawSmall(name, cx, (int16_t)(y0 + 32), C_WHITE, fill);
    DrawLines(desc, cx, (int16_t)(y0 + 54), C_GRAY, fill);
    if(foot[0]) rc_DrawTextEx(foot, cx, (int16_t)(y1 - 16), footC, fill, false);
}


static void DrawReroll(void)
{
    char     buf[18];
    int16_t  n;
    bool     sel = (rc_cardSel == rc_nCard);
    uint32_t fill = sel ? COLOR_CHARCOAL : COLOR_TETRIS_GRID;
    uint16_t cost = RerollCost();

    Graphics_FillRect(RR_X0, RR_Y0, RR_X1, RR_Y1, sel ? C_CYAN : COLOR_CHARCOAL);
    Graphics_FillRect(RR_X0 + 2, RR_Y0 + 2, RR_X1 - 2, RR_Y1 - 2, fill);
    n = rc_Cat(buf, 0, "REROLL  ", 8);
    if(cost == 0u) rc_Cat(buf, n, "FREE", 4);
    else { n += rc_Itoa(buf + n, cost); rc_Cat(buf, n, "g", 1); }
    rc_DrawTextEx(buf, (RR_X0 + RR_X1) / 2, (RR_Y0 + RR_Y1) / 2,
                  (rc_gold >= cost) ? C_WHITE : C_BAD, fill, false);
}


static void DrawCardsGold(void)
{
    char    buf[16];
    int16_t n;

    Graphics_FillRect(190, 2, SCREEN_W - 1, 28, C_BLACK);
    n = rc_Cat(buf, 0, "G ", 2);
    rc_Itoa(buf + n, rc_gold);
    rc_DrawText(buf, 275, 15, C_GOLD);
}


static void DrawCardMsg(void)
{
    Graphics_FillRect(172, RR_Y0, SCREEN_W - 1, RR_Y1, C_BLACK);
    rc_DrawText(rc_kMsgText[rc_cardMsg], 245, (RR_Y0 + RR_Y1) / 2,
                (rc_cardMsg == 1 || rc_cardMsg == 4 || rc_cardMsg == 6) ? C_GREEN : C_BAD);
}


void rc_DrawCardsFull(void)
{
    char    buf[24];
    int16_t n;
    static const UiHint kPick[]  = { { UI_ICON_L, "" }, { UI_ICON_R, "Pick" }, { UI_ICON_U, "Choose" } };
    static const UiHint kShop[]  = { { UI_ICON_L, "" }, { UI_ICON_R, "Pick" }, { UI_ICON_U, "Buy" }, { UI_ICON_D, "Leave" } };
    static const UiHint kSwap[]  = { { UI_ICON_L, "" }, { UI_ICON_R, "Pick" }, { UI_ICON_U, "Replace" }, { UI_ICON_D, "Refuse" } };
    static const UiHint kAltar[] = { { UI_ICON_L, "" }, { UI_ICON_R, "Pick" }, { UI_ICON_U, "Choose" }, { UI_ICON_D, "Leave" } };

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    switch(rc_cardScreen)
    {
        case CS_LEVEL:
            n = rc_Cat(buf, 0, "LEVEL UP!  Lv ", 14);
            rc_Itoa(buf + n, rc_xpLv);
            rc_DrawText(buf, 105, 15, C_XP);
            if(rc_lvPending > 1u)
            {
                n = rc_Cat(buf, 0, "x", 1);
                rc_Itoa(buf + n, rc_lvPending);
                rc_DrawSmall(buf, 215, 15, C_GRAY, C_BLACK);
            }
            Ui_DrawHints(kPick, UI_ARRAY_LEN(kPick), 228, C_BLACK);
            break;
        case CS_VENDOR:
            rc_DrawText("VENDOR", 105, 15, C_GOLD);
            Ui_DrawHints(kShop, UI_ARRAY_LEN(kShop), 228, C_BLACK);
            break;
        case CS_SHRINE:
            rc_DrawText("HAUNTED SHRINE", 105, 15, C_XP);
            Ui_DrawHints(kAltar, UI_ARRAY_LEN(kAltar), 228, C_BLACK);
            break;
        case CS_LAB:
            rc_DrawText("ASTRAL ALTAR", 105, 15, C_CYAN);
            Ui_DrawHints(kAltar, UI_ARRAY_LEN(kAltar), 228, C_BLACK);
            break;
        default:
            rc_DrawText("RUNE SLOTS FULL", 105, 15, C_BAD);
            Ui_DrawHints(kSwap, UI_ARRAY_LEN(kSwap), 228, C_BLACK);
            break;
    }

    Graphics_setForegroundColor(&g_sContext, C_GRAY);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);
    rc_cardDirty = CD_ALL;
}


void rc_CardsRender(void)
{
    uint16_t i;

    for(i = 0; i < rc_nCard; i++)
        if(rc_cardDirty & (1u << i)) DrawCard(i);
    if((rc_cardDirty & CD_REROLL) && rc_cardScreen == CS_LEVEL) DrawReroll();
    if(rc_cardDirty & CD_GOLD) DrawCardsGold();
    if(rc_cardDirty & CD_MSG)  DrawCardMsg();
    rc_cardDirty = 0;
}


//*****************************************************************************
// Entering / leaving
//*****************************************************************************
void rc_EnterCards(uint16_t screen)
{
    if(screen == CS_VENDOR || screen == CS_SHRINE || screen == CS_LAB)
    {
        s_prop   = rc_iaIdx;
        s_vendor = (rc_prop[s_prop].room == RM_EC) ? 1u : 0u;
    }
    rc_cardScreen = screen;
    rc_mode       = MODE_CARDS;
    rc_dirtyFull  = true;
    rc_rerolls    = 0;
    rc_stX = rc_stY = 1;
    SetupScreen();
    rc_Sfx(&sfx_level_up);
}


static void BackToPlay(void)
{
    rc_mode      = MODE_PLAY;
    rc_dirtyFull = true;
}


// A level-up is done: next pending level-up, or back to the game.
static void FinishLevel(void)
{
    if(rc_lvPending) rc_lvPending--;
    if(rc_lvPending)
    {
        rc_cardScreen = CS_LEVEL;
        rc_rerolls    = 0;
        rc_dirtyFull  = true;
        SetupScreen();
    }
    else BackToPlay();
}


// Equip a rune. Returns false if all four slots are full (the swap screen opens).
static bool GiveRune(uint16_t r)
{
    uint16_t i;

    for(i = 0; i < RUNE_SLOTS; i++)
        if(rc_rune[i] == RUNE_NONE)
        {
            rc_rune[i] = r;
            if(r == RU_VITAL && rc_maxHp + HP_UNITS_PER_HEART <= rc_MaxHpCap())
            {
                rc_maxHp = (uint16_t)(rc_maxHp + HP_UNITS_PER_HEART);
                rc_HealPlayer(HP_UNITS_PER_HEART);
            }
            return true;
        }
    return false;
}


static void RemoveRune(uint16_t slot)
{
    if(rc_rune[slot] == RU_VITAL && rc_maxHp > HP_UNITS_PER_HEART)
    {
        rc_maxHp = (uint16_t)(rc_maxHp - HP_UNITS_PER_HEART);
        if(rc_hp > rc_maxHp) rc_hp = rc_maxHp;
    }
    rc_rune[slot] = RUNE_NONE;
}


static uint16_t s_swapFrom;                 // screen that asked for the swap

static void AskSwap(uint16_t rune)
{
    uint16_t i;

    s_swapFrom  = rc_cardScreen;
    rc_swapRune = rune;
    for(i = 0; i < rc_nCard; i++) s_saved[i] = rc_card[i];
    s_savedN = rc_nCard;
    rc_cardScreen = CS_SWAP;
    rc_dirtyFull  = true;
    SetupScreen();
    rc_cardMsg = 5;
}


static void ReturnFromSwap(void)
{
    uint16_t i;

    rc_cardScreen = s_swapFrom;
    for(i = 0; i < s_savedN; i++) rc_card[i] = s_saved[i];
    rc_nCard     = s_savedN;
    rc_cardSel   = 0;
    rc_dirtyFull = true;
}


// The player chose a rune that is now equipped: finish whatever offered it.
static void RuneTaken(uint16_t from)
{
    if(from == CS_LAB) { rc_prop[s_prop].used = 1; BackToPlay(); }
    else               FinishLevel();
}


static void Choose(uint16_t i)
{
    Card *c = &rc_card[i];

    rc_cardDirty |= CD_MSG;
    if(c->kind == CK_NONE || c->sold) { rc_cardMsg = 3; rc_Sfx(&sfx_menu_bump); return; }

    switch(c->kind)
    {
        case CK_STAT:
            rc_lvStat[c->idx]++;
            if(c->idx == ST_HP)
            {
                rc_maxHp = (uint16_t)(rc_maxHp + HP_UNITS_PER_HEART);
                rc_HealPlayer(HP_UNITS_PER_HEART);
            }
            if(c->idx == ST_MANA) rc_mana = (uint16_t)(rc_mana + rc_kStat[ST_MANA].step);
            rc_manaShown = -1;
            rc_Sfx(&sfx_coin);
            FinishLevel();
            return;

        case CK_RUNE:
            if(!GiveRune(c->idx)) { AskSwap(c->idx); rc_Sfx(&sfx_menu_bump); return; }
            rc_Sfx(&sfx_coin);
            RuneTaken(rc_cardScreen);
            return;

        case CK_SLOT:
            RemoveRune(c->idx);
            (void)GiveRune(rc_swapRune);
            rc_swapRune = RUNE_NONE;
            rc_Sfx(&sfx_coin);
            RuneTaken(s_swapFrom);
            return;

        case CK_STAFF:
        case CK_ITEM:
            if(CardOwned(c)) { rc_cardMsg = 3; rc_Sfx(&sfx_menu_bump); return; }
            if(rc_gold < CardCost(c)) { rc_cardMsg = 2; rc_Sfx(&sfx_menu_bump); return; }
            rc_gold -= CardCost(c);
            if(c->kind == CK_STAFF) { rc_staffOwned |= (uint16_t)(1u << c->idx); rc_equip = c->idx; }
            else                    { rc_itemOwned  |= (uint16_t)(1u << c->idx); rc_equip = (uint16_t)(NUM_STAFF + c->idx); }
            c->sold = 1;
            s_stockSold[s_vendor] |= (uint16_t)(1u << i);
            rc_cardMsg    = 1;
            rc_cardDirty |= CD_GOLD | 0x0Fu;
            rc_Sfx(&sfx_coin);
            return;

        case CK_BLESS_HP:
            if(rc_maxHp + HP_UNITS_PER_HEART <= rc_MaxHpCap()) rc_maxHp = (uint16_t)(rc_maxHp + HP_UNITS_PER_HEART);
            rc_HealPlayer(rc_maxHp);
            rc_prop[s_prop].used = 1;
            rc_Sfx(&sfx_powerup);
            BackToPlay();
            return;

        default:                                // CK_BLESS_GOLD
            rc_GainGold(120);
            rc_prop[s_prop].used = 1;
            rc_Sfx(&sfx_coin);
            BackToPlay();
            return;
    }
}


static void Reroll(void)
{
    uint16_t cost = RerollCost();

    rc_cardDirty |= CD_MSG;
    if(rc_gold < cost) { rc_cardMsg = 2; rc_Sfx(&sfx_menu_bump); return; }
    rc_gold -= cost;
    rc_cardMsg = (cost == 0u) ? 6u : 4u;
    rc_rerolls++;
    RollLevel();
    rc_cardDirty = CD_ALL;
    rc_Sfx(&sfx_menu_select);
}


void rc_UpdateCards(void)
{
    int16_t  sx = rc_StickEdge(g_joy.dx, &rc_stX);
    uint16_t old = rc_cardSel;
    uint16_t last = (rc_cardScreen == CS_LEVEL) ? rc_nCard : (uint16_t)(rc_nCard - 1u);   // + reroll button

    (void)rc_StickEdge(g_joy.dy, &rc_stY);

    if(g_dpad.dPressed)
    {
        uint16_t i, any = 0;

        for(i = 0; i < rc_nCard; i++) if(rc_card[i].kind != CK_NONE) any = 1;
        rc_Sfx(&sfx_menu_back);
        if(rc_cardScreen == CS_SWAP)       { rc_swapRune = RUNE_NONE; ReturnFromSwap(); }     // refuse the new rune
        else if(rc_cardScreen != CS_LEVEL) BackToPlay();
        else if(!any)                      FinishLevel();     // nothing left to offer: skip
        return;                                               // otherwise a level-up must be taken
    }

    if(g_dpad.lPressed || sx < 0)      rc_cardSel = (rc_cardSel == 0u) ? last : (uint16_t)(rc_cardSel - 1u);
    else if(g_dpad.rPressed || sx > 0) rc_cardSel = (rc_cardSel >= last) ? 0u : (uint16_t)(rc_cardSel + 1u);

    if(rc_cardSel != old)
    {
        rc_cardDirty |= (old >= rc_nCard ? CD_REROLL : (1u << old)) |
                        (rc_cardSel >= rc_nCard ? CD_REROLL : (1u << rc_cardSel));
        rc_Sfx(&sfx_menu_move);
    }

    if(g_dpad.uPressed || g_joy.btnPressed)
    {
        if(rc_cardSel < rc_nCard) Choose(rc_cardSel);
        else                      Reroll();
    }
}


//*****************************************************************************
// Game over, high scores, name entry (rc.md 5.5)
//*****************************************************************************
void rc_LoadScores(void)
{
    uint16_t i, k;

    if(!Memory_Read(MEM_SLOT_RC_SCORES, &rc_scores, sizeof(rc_scores)))
        for(i = 0; i < RC_NUM_SCORES; i++) rc_scores.wave[i] = 0;
    for(i = 0; i < RC_NUM_SCORES; i++)
        if(rc_scores.wave[i] == 0u)                 // empty entries are the default WASON / 15
        {
            static const char kDef[RC_NAME_LEN] = { 'W', 'A', 'S', 'O', 'N' };
            rc_scores.wave[i] = 15;
            for(k = 0; k < RC_NAME_LEN; k++) rc_scores.c[i][k] = (uint16_t)kDef[k];
        }
}


void rc_EnterOver(void)
{
    uint16_t i;

    rc_mode      = MODE_OVER;
    rc_dirtyFull = true;
    s_overT      = 90;                              // 3 s of run stats
    s_survived   = rc_roundGap ? rc_wave : (uint16_t)(rc_wave - 1u);
    rc_LoadScores();
    rc_scoreRank = 0xFFFFu;
    for(i = 0; i < RC_NUM_SCORES; i++)
        if(s_survived > rc_scores.wave[i]) { rc_scoreRank = i; break; }    // must beat an entry
    rc_Sfx(&sfx_game_over);
}


void rc_DrawEndScreen(void)
{
    char    buf[28];
    int16_t n, i, k;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    if(rc_mode == MODE_OVER)
    {
        rc_DrawText("GAME OVER", SCREEN_W / 2, 45, C_RED);
        n = rc_Cat(buf, 0, "Waves survived ", 15);
        rc_Itoa(buf + n, s_survived);
        rc_DrawText(buf, SCREEN_W / 2, 85, C_WHITE);
        n = rc_Cat(buf, 0, "Level ", 6);
        rc_Itoa(buf + n, rc_xpLv);
        rc_DrawText(buf, SCREEN_W / 2, 112, C_XP);
        n = rc_Cat(buf, 0, "Kills ", 6);
        rc_Itoa(buf + n, rc_kills);
        rc_DrawText(buf, SCREEN_W / 2, 139, C_GRAY);
        n = rc_Cat(buf, 0, "Mini-bosses ", 12);
        rc_Itoa(buf + n, rc_miniKills);
        rc_DrawText(buf, SCREEN_W / 2, 166, C_GOLD);
        if(rc_dragonKills)
        {
            n = rc_Cat(buf, 0, "Dragons slain ", 14);
            rc_Itoa(buf + n, rc_dragonKills);
            rc_DrawText(buf, SCREEN_W / 2, 193, C_RED);
        }
        return;
    }

    if(rc_mode == MODE_NAME)
    {
        rc_DrawText("NEW HIGH SCORE!", SCREEN_W / 2, 40, C_GOLD);
        n = rc_Cat(buf, 0, "Wave ", 5);
        rc_Itoa(buf + n, s_survived);
        rc_DrawText(buf, SCREEN_W / 2, 70, C_WHITE);
        {
            static const UiHint kN[] = { { UI_ICON_U, "" }, { UI_ICON_D, "Letter" },
                                         { UI_ICON_R, "Next" }, { UI_ICON_STICK_CLICK, "Done" } };
            Ui_DrawHints(kN, UI_ARRAY_LEN(kN), 226, C_BLACK);
        }
        return;
    }

    // MODE_SCORES
    rc_DrawText("HIGH SCORES", SCREEN_W / 2, 28, C_GOLD);
    for(i = 0; i < RC_NUM_SCORES; i++)
    {
        uint32_t c = ((uint16_t)i == rc_scoreRank) ? C_GOLD : C_WHITE;

        n = rc_Itoa(buf, (uint16_t)(i + 1));
        n = rc_Cat(buf, n, ".  ", 3);
        for(k = 0; k < RC_NAME_LEN; k++) buf[n++] = (char)rc_scores.c[i][k];
        n = rc_Cat(buf, n, "   wave ", 8);
        rc_Itoa(buf + n, rc_scores.wave[i]);
        rc_DrawText(buf, SCREEN_W / 2, (int16_t)(66 + 28 * i), c);
    }
    Graphics_FillRect(110, 200, 210, 222, C_CYAN);
    Graphics_FillRect(112, 202, 208, 220, COLOR_CHARCOAL);
    rc_DrawTextEx("Main Menu", SCREEN_W / 2, 211, C_WHITE, COLOR_CHARCOAL, false);
}


static void DrawName(void)
{
    uint16_t k;

    for(k = 0; k < RC_NAME_LEN; k++)
    {
        int16_t  x = (int16_t)(100 + 26 * k);
        char     s[2];
        bool     cur = (k == rc_namePos);

        s[0] = (char)rc_nameChar[k];
        s[1] = 0;
        Graphics_FillRect(x - 11, 105, x + 11, 140, cur ? C_CYAN : COLOR_CHARCOAL);
        Graphics_FillRect(x - 9, 107, x + 9, 138, C_BLACK);
        rc_DrawText(s, x, 122, cur ? C_GOLD : C_WHITE);
    }
}


void rc_OverRender(void)
{
    if(rc_mode == MODE_NAME && rc_cardDirty) { DrawName(); rc_cardDirty = 0; }
}


static uint16_t NextChar(uint16_t ch, int16_t d)        // A..Z then 0..9
{
    static const char kSet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int16_t i;

    for(i = 0; i < 36; i++) if((uint16_t)kSet[i] == ch) break;
    if(i >= 36) i = 0;
    i = (int16_t)((i + d + 36) % 36);
    return (uint16_t)kSet[i];
}


// Returns true when the player leaves to the main menu.
bool rc_UpdateOverScreens(void)
{
    uint16_t i, k;

    if(rc_mode == MODE_OVER)
    {
        if(s_overT) s_overT--;
        if(s_overT == 0u || ((g_dpad.dPressed || g_joy.btnPressed) && s_overT < 60u))
        {
            if(rc_scoreRank != 0xFFFFu)
            {
                rc_mode = MODE_NAME;
                for(k = 0; k < RC_NAME_LEN; k++) rc_nameChar[k] = 'A';
                rc_namePos = 0;
                rc_cardDirty = 1;
            }
            else rc_mode = MODE_SCORES;
            rc_dirtyFull = true;
        }
        return false;
    }

    if(rc_mode == MODE_NAME)
    {
        if(g_dpad.uPressed) { rc_nameChar[rc_namePos] = NextChar(rc_nameChar[rc_namePos], 1);  rc_cardDirty = 1; rc_Sfx(&sfx_menu_move); }
        if(g_dpad.dPressed) { rc_nameChar[rc_namePos] = NextChar(rc_nameChar[rc_namePos], -1); rc_cardDirty = 1; rc_Sfx(&sfx_menu_move); }
        if(g_dpad.rPressed && rc_namePos + 1u < RC_NAME_LEN) { rc_namePos++; rc_cardDirty = 1; rc_Sfx(&sfx_menu_move); }
        if(g_dpad.lPressed && rc_namePos > 0u)               { rc_namePos--; rc_cardDirty = 1; rc_Sfx(&sfx_menu_move); }
        if(g_joy.btnPressed)
        {
            uint16_t r = rc_scoreRank;

            for(i = RC_NUM_SCORES - 1u; i > r; i--)             // push the lower scores down
            {
                rc_scores.wave[i] = rc_scores.wave[i - 1u];
                for(k = 0; k < RC_NAME_LEN; k++) rc_scores.c[i][k] = rc_scores.c[i - 1u][k];
            }
            rc_scores.wave[r] = s_survived;
            for(k = 0; k < RC_NAME_LEN; k++) rc_scores.c[r][k] = rc_nameChar[k];
            Memory_Write(MEM_SLOT_RC_SCORES, &rc_scores, sizeof(rc_scores));
            Memory_Flush();                                     // blocks briefly: a user action, not per frame
            rc_mode = MODE_SCORES;
            rc_dirtyFull = true;
            rc_Sfx(&sfx_menu_select);
        }
        return false;
    }

    // MODE_SCORES: the only button is Main Menu
    return (g_joy.btnPressed || g_dpad.uPressed || g_dpad.dPressed) != 0;
}


//*****************************************************************************
// Title screen, How to Play, and the opening letter
//*****************************************************************************
#define TT_PARCH     0x00D8BF84UL            // yellowed parchment
#define TT_PARCH_DK  0x00BFA064UL            // stains
#define TT_INK       0x003A2412UL
#define TT_SEAL      0x00861414UL

static uint16_t s_ttSel;                     // 0 = START, 1 = HOW TO PLAY
static uint16_t s_ttDirty;                   // 1 = menu needs redrawing
static uint16_t s_ttChars;                   // letter characters shown so far
static uint16_t s_ttSub;                     // tick fraction for the typing speed

// 5x7 block letters for the title (one row per entry, bit 4 = leftmost pixel)
static const uint16_t kGlyph[7][7] =
{
    { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 },      // R
    { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },      // A
    { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 },      // Y
    { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E },      // C
    { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E },      // S
    { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },      // T
    { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F }       // E
};
static const uint16_t kTitleWord[9] = { 0, 1, 2, 3, 1, 4, 5, 6, 0 };     // R A Y C A S T E R

static const char *const kLetter[] =
{
    "Dear Ray Caster,",
    "",
    "Congratulations on your hiring.",
    "The dungeon beneath this office",
    "has grown... lively. Our last six",
    "wizards left no forwarding address.",
    "",
    "Your duties are simple: clear the",
    "rooms, buy the doors, and do NOT",
    "wake the Dragon. (You will.)",
    "",
    "Pay: gold, glory, and whatever",
    "is in the chests. Mind the mimics."
};
#define LETTER_LINES   13
#define LETTER_X       56
#define LETTER_Y0      28
#define LETTER_DY      12

static uint16_t LetterTotal(void)
{
    uint16_t i, n = 0, k;

    for(i = 0; i < LETTER_LINES; i++)
    {
        for(k = 0; kLetter[i][k]; k++) {}
        n = (uint16_t)(n + k + 1u);                 // + one pause per line
    }
    return n;
}


static void DrawLeft(const char *s, uint16_t len, int16_t x, int16_t y, uint32_t fg, uint32_t bg)
{
    if(len == 0) return;
    Graphics_setFont(&g_sContext, &g_sFontCmss12b);
    Graphics_setForegroundColor(&g_sContext, fg);
    Graphics_setBackgroundColor(&g_sContext, bg);
    Graphics_drawString(&g_sContext, (int16_t *)s, len, x, y, OPAQUE_TEXT);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
}


static void DrawMenu(void)
{
    uint16_t i;
    static const char *const kItem[2] = { "START", "HOW TO PLAY" };
    char     buf[20];
    int16_t  n;

    Graphics_FillRect(60, 138, 260, 200, C_BLACK);
    for(i = 0; i < 2; i++)
    {
        bool sel = (s_ttSel == i);

        n = 0;
        if(sel) n = rc_Cat(buf, n, "> ", 2);
        n = rc_Cat(buf, n, kItem[i], 16);
        if(sel) rc_Cat(buf, n, " <", 2);
        rc_DrawText(buf, SCREEN_W / 2, (int16_t)(152 + 30 * i), sel ? C_GOLD : C_GRAY);
    }
}


static void DrawTitle(void)
{
    uint16_t w, r, cx, x0 = 25, y0 = 40;
    int16_t  bx;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);

    // rays fanning up from the wand
    for(bx = 20; bx <= 300; bx += 70)
    {
        Graphics_setForegroundColor(&g_sContext, (bx == 160) ? C_CYAN : COLOR_TEAL);
        Graphics_drawLine(&g_sContext, 160, 236, bx, 110);
    }
    Graphics_fillCircle(&g_sContext, 160, 238, 5);

    // block-letter title with a drop shadow
    for(w = 0; w < 9; w++)
        for(r = 0; r < 7; r++)
            for(cx = 0; cx < 5; cx++)
                if(kGlyph[kTitleWord[w]][r] & (0x10u >> cx))
                {
                    int16_t px = (int16_t)(x0 + (w * 6u + cx) * 5u);
                    int16_t py = (int16_t)(y0 + r * 5u);

                    Graphics_FillRect(px + 3, py + 3, px + 7, py + 7, COLOR_MAROON);
                    Graphics_FillRect(px, py, px + 4, py + 4, (r < 3) ? C_GOLD : C_RED);
                }
    rc_DrawSmall("A dungeon of rays and bad decisions", SCREEN_W / 2, 100, C_GRAY, C_BLACK);

    DrawMenu();
    {
        static const UiHint k[] =
        {
            { UI_ICON_U, "" }, { UI_ICON_D, "Choose" },
            { UI_ICON_R, "" }, { UI_ICON_STICK_CLICK, "Select" }
        };
        Ui_DrawHints(k, UI_ARRAY_LEN(k), 226, C_BLACK);
    }
}


static void DrawHowTo(void)
{
    static const char *const kLine[] =
    {
        "walk and strafe",
        "turn",
        "cast / use item",
        "swap staff / item",
        "buy, open, trade, use keys",
        "",
        "Survive the waves, buy all eight doors,",
        "beat three mini-bosses, then step into",
        "the Dragon Den. Keys open secret doors."
    };
    uint16_t i;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);
    rc_DrawText("HOW TO PLAY", SCREEN_W / 2, 22, C_GOLD);
    Graphics_setForegroundColor(&g_sContext, C_GRAY);
    Graphics_drawLine(&g_sContext, 20, 40, SCREEN_W - 21, 40);
    for(i = 0; i < 9; i++)
        if(kLine[i][0])
        {
            uint16_t k;

            for(k = 0; kLine[i][k]; k++) {}
            DrawLeft(kLine[i], k, (i < 5) ? 96 : 24, (int16_t)(60 + 18 * i), (i < 5) ? C_WHITE : C_GRAY, C_BLACK);
        }

    // the real symbols in one column, one per control (the original buttons)
    Ui_DrawIcon(UI_ICON_STICK,       40, 67, 7);
    Ui_DrawIcon(UI_ICON_L,           32, 85, 7);                // turn: left + right
    Ui_DrawIcon(UI_ICON_R,           52, 85, 7);
    Ui_DrawIcon(UI_ICON_U,           40, 103, 7);
    Ui_DrawIcon(UI_ICON_D,           40, 121, 7);
    Ui_DrawIcon(UI_ICON_STICK_CLICK, 40, 139, 7);
    {
        static const UiHint k[] = { { UI_ICON_L, "Back" } };
        Ui_DrawHints(k, UI_ARRAY_LEN(k), 226, C_BLACK);
    }
}


// Yellowed, damaged parchment on black.
static void DrawParchment(void)
{
    uint16_t i;

    Graphics_FillRect(0, 0, SCREEN_W - 1, SCREEN_H - 1, C_BLACK);
    Graphics_FillRect(36, 10, 284, 229, TT_PARCH);

    for(i = 0; i < 9; i++)                              // stains and age
        Graphics_FillRect((int16_t)(44 + (i * 53u) % 200u), (int16_t)(16 + (i * 37u) % 190u),
                          (int16_t)(60 + (i * 53u) % 200u), (int16_t)(24 + (i * 37u) % 190u), TT_PARCH_DK);
    Graphics_setForegroundColor(&g_sContext, TT_PARCH_DK);
    Graphics_drawCircle(&g_sContext, 236, 112, 16);     // a coffee ring
    Graphics_drawCircle(&g_sContext, 236, 112, 15);

    for(i = 0; i < 12; i++)                             // torn, ragged edges
    {
        int16_t t = (int16_t)(2u + (i * 5u) % 7u);

        Graphics_FillRect((int16_t)(36 + (i * 23u) % 240u), 10, (int16_t)(40 + (i * 23u) % 240u), (int16_t)(10 + t), C_BLACK);
        Graphics_FillRect((int16_t)(36 + (i * 29u) % 240u), (int16_t)(229 - t), (int16_t)(41 + (i * 29u) % 240u), 229, C_BLACK);
        Graphics_FillRect(36, (int16_t)(12 + (i * 19u) % 210u), (int16_t)(36 + t), (int16_t)(16 + (i * 19u) % 210u), C_BLACK);
        Graphics_FillRect((int16_t)(284 - t), (int16_t)(12 + (i * 17u) % 210u), 284, (int16_t)(15 + (i * 17u) % 210u), C_BLACK);
    }
    Graphics_FillRect(250, 10, 284, 22, C_BLACK);       // a burnt corner
    Graphics_FillRect(262, 22, 284, 34, C_BLACK);
    Graphics_FillRect(250, 22, 261, 26, TT_INK);
    Graphics_FillRect(36, 218, 52, 229, C_BLACK);       // a chewed corner
}


static void DrawLetterText(uint16_t shown)
{
    uint16_t i, k, left = shown;

    for(i = 0; i < LETTER_LINES && left > 0; i++)
    {
        for(k = 0; kLetter[i][k]; k++) {}
        if(left >= k + 1u)
        {
            if(k) DrawLeft(kLetter[i], k, LETTER_X, (int16_t)(LETTER_Y0 + LETTER_DY * i), TT_INK, TT_PARCH);
            left = (uint16_t)(left - k - 1u);
        }
        else
        {
            if(left) DrawLeft(kLetter[i], left, LETTER_X, (int16_t)(LETTER_Y0 + LETTER_DY * i), TT_INK, TT_PARCH);
            left = 0;
        }
    }
}


static void DrawLetterEnd(void)
{
    rc_DrawTextEx("YOUR SHIFT BEGINS NOW.", SCREEN_W / 2, 194, 0x00A01010UL, TT_PARCH, true);
    DrawLeft("- Dungeon Management", 20, 150, 214, TT_INK, TT_PARCH);
    Graphics_setForegroundColor(&g_sContext, TT_SEAL);                // dark red wax seal
    Graphics_fillCircle(&g_sContext, 92, 212, 15);
    Graphics_setForegroundColor(&g_sContext, COLOR_MAROON);
    Graphics_fillCircle(&g_sContext, 92, 212, 12);
    Graphics_setForegroundColor(&g_sContext, TT_SEAL);
    Graphics_fillCircle(&g_sContext, 92, 212, 9);
    rc_DrawTextEx("R", 92, 212, 0x00B03030UL, TT_SEAL, true);
}


void rc_EnterTitle(void)
{
    rc_mode      = MODE_TITLE;
    s_ttSel      = 0;
    s_ttDirty    = 0;
    rc_stX = rc_stY = 1;                              // ignore a stick that is already pushed
    rc_dirtyFull = true;
}


void rc_DrawTitleFull(void)
{
    if(rc_mode == MODE_TITLE)       DrawTitle();
    else if(rc_mode == MODE_HOWTO)  DrawHowTo();
    else
    {
        DrawParchment();
        DrawLetterText(s_ttChars);
        if(s_ttChars >= LetterTotal()) DrawLetterEnd();
    }
}


// Incremental drawing: the menu cursor, and the letter typing itself out.
void rc_TitleRender(void)
{
    static uint16_t shownChars;

    if(rc_mode == MODE_TITLE)
    {
        if(s_ttDirty) { DrawMenu(); s_ttDirty = 0; }
    }
    else if(rc_mode == MODE_LETTER)
    {
        if(s_ttChars < shownChars) shownChars = 0;        // a new letter
        if(s_ttChars != shownChars)
        {
            DrawLetterText(s_ttChars);
            if(s_ttChars >= LetterTotal() && shownChars < LetterTotal()) DrawLetterEnd();
            shownChars = s_ttChars;
        }
    }
}


bool rc_UpdateTitle(uint16_t ticks)
{
    int16_t sy  = rc_StickEdge(g_joy.dy, &rc_stY);
    bool    go  = (g_joy.btnPressed != 0);
    uint16_t total;

    (void)rc_StickEdge(g_joy.dx, &rc_stX);

    if(rc_mode == MODE_TITLE)
    {
        if(g_dpad.uPressed || g_dpad.dPressed || sy != 0)
        {
            s_ttSel ^= 1u;
            s_ttDirty = 1;
            rc_Sfx(&sfx_menu_move);
        }
        if(go || g_dpad.rPressed)
        {
            rc_Sfx(&sfx_menu_select);
            rc_dirtyFull = true;
            if(s_ttSel == 0) { rc_mode = MODE_LETTER; s_ttChars = 0; s_ttSub = 0; }
            else               rc_mode = MODE_HOWTO;
        }
    }
    else if(rc_mode == MODE_HOWTO)
    {
        if(go || g_dpad.lPressed || g_dpad.dPressed)
        {
            rc_Sfx(&sfx_menu_back);
            rc_mode      = MODE_TITLE;
            rc_dirtyFull = true;
        }
    }
    else                                                    // MODE_LETTER
    {
        total = LetterTotal();
        if(s_ttChars < total)
        {
            s_ttSub = (uint16_t)(s_ttSub + ticks);          // about 45 characters a second
            while(s_ttSub >= 2u) { s_ttSub = (uint16_t)(s_ttSub - 2u); if(s_ttChars < total) s_ttChars++; }
            if(go || g_dpad.uPressed) s_ttChars = total;    // skip the typing
        }
        else if(go || g_dpad.uPressed || g_dpad.rPressed)
        {
            rc_Sfx(&sfx_menu_select);
            return true;                                    // enter the dungeon
        }
    }
    return false;
}
