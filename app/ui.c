//#############################################################################
// FILE:   app/ui.c
// TITLE:  Shared UI implementation (see ui.h for the rules)
//
//   C28x notes: int/char are 16 bits, so only int16_t/uint16_t/uint32_t are used
//   and widths from grlib (int32_t) are narrowed on purpose.
//#############################################################################

#include "apps.h"
#include "ui.h"

#define UI_ARROW_DARK    0x202020UL
#define UI_STICK_RING    0x909090UL
#define UI_STICK_BASE    0x1C1C1CUL
#define UI_STICK_BALL    0xD0D0D0UL
#define UI_HINT_GAP      12             // pixels between two hint pairs
#define UI_ICON_TEXT_GAP 4              // symbol -> its label

//*****************************************************************************
// Tiny drawing helpers
//*****************************************************************************
static void Disc(int16_t cx, int16_t cy, int16_t r, uint32_t c)
{
    if(r < 0) return;
    Graphics_setForegroundColor(&g_sContext, c);
    Graphics_fillCircle(&g_sContext, cx, cy, r);
}

// Solid triangle, h rows tall, pointing dir (0 up, 1 left, 2 right, 3 down),
// centered on (cx,cy). One FillRect per row; h is small (2..8).
static void Tri(int16_t cx, int16_t cy, int16_t h, UiIcon dir, uint32_t c)
{
    int16_t i, t = h / 2;

    for(i = 0; i < h; i++)
    {
        switch(dir)
        {
            case UI_ICON_U: Graphics_FillRect(cx - i, cy - t + i, cx + i, cy - t + i, c); break;
            case UI_ICON_D: Graphics_FillRect(cx - i, cy + t - i, cx + i, cy + t - i, c); break;
            case UI_ICON_L: Graphics_FillRect(cx - t + i, cy - i, cx - t + i, cy + i, c); break;
            default:        Graphics_FillRect(cx + t - i, cy - i, cx + t - i, cy + i, c); break;
        }
    }
}

//*****************************************************************************
// Symbols
//*****************************************************************************
uint32_t Ui_IconColor(UiIcon icon)
{
    switch(icon)
    {
        case UI_ICON_U:     return UI_COL_U;
        case UI_ICON_L:     return UI_COL_L;
        case UI_ICON_R:     return UI_COL_R;
        case UI_ICON_D:     return UI_COL_D;
        case UI_ICON_START: return UI_COL_START;
        default:            return UI_STICK_BALL;
    }
}

int16_t Ui_IconWidth(UiIcon icon, int16_t r)
{
    if(icon == UI_ICON_NONE)  return 0;
    if(icon == UI_ICON_START) return (int16_t)(3 * r + 1);
    return (int16_t)(2 * r + 1);
}

static void DrawButton(UiIcon icon, int16_t cx, int16_t cy, int16_t r)
{
    int16_t  h   = (int16_t)(r / 2 + 1);
    uint32_t ink = (icon == UI_ICON_L) ? UI_ARROW_DARK : COLOR_WHITE;   // yellow needs a dark arrow

    Disc(cx, cy, r,     UI_COL_RIM);
    Disc(cx, cy, r - 1, Ui_IconColor(icon));
    if(r >= 6) Tri(cx, cy, h, icon, ink);                               // arrow = where it sits on the pad
}

static void DrawStart(int16_t cx, int16_t cy, int16_t r)
{
    int16_t half = (int16_t)(r / 2);

    Disc(cx - half, cy, r, UI_COL_RIM);
    Disc(cx + half, cy, r, UI_COL_RIM);
    Graphics_FillRect(cx - half, cy - r, cx + half, cy + r, UI_COL_RIM);

    Disc(cx - half, cy, r - 1, UI_COL_START);
    Disc(cx + half, cy, r - 1, UI_COL_START);
    Graphics_FillRect(cx - half, cy - r + 1, cx + half, cy + r - 1, UI_COL_START);

    if(r >= 6) Tri(cx, cy, (int16_t)(r / 2 + 1), UI_ICON_R, COLOR_WHITE);
}

// Joystick seen from above: a dark base ring with four direction ticks and a
// light ball. With `click` the ball is a push button (dark dip + down arrow).
static void DrawStick(int16_t cx, int16_t cy, int16_t r, bool click)
{
    int16_t br = (int16_t)(click ? (r * 3) / 4 : r / 2 + 1);
    int16_t t  = (int16_t)(r - 2);

    Disc(cx, cy, r,     UI_STICK_RING);
    Disc(cx, cy, r - 1, UI_STICK_BASE);

    if(!click && r >= 6)                                        // MOVE: four ticks around the ball
    {
        Tri(cx,     cy - t, 3, UI_ICON_U, COLOR_WHITE);
        Tri(cx,     cy + t, 3, UI_ICON_D, COLOR_WHITE);
        Tri(cx - t, cy,     3, UI_ICON_L, COLOR_WHITE);
        Tri(cx + t, cy,     3, UI_ICON_R, COLOR_WHITE);
    }

    Disc(cx, cy, br, UI_STICK_BALL);

    if(click)
    {
        Disc(cx, cy, (int16_t)(br - 1), 0x505050UL);            // the dip
        if(r >= 6) Tri(cx, cy, (int16_t)(br / 2 + 2), UI_ICON_D, COLOR_WHITE);
    }
    else if(br >= 3)
    {
        Disc((int16_t)(cx - br / 3), (int16_t)(cy - br / 3), (int16_t)(br / 4 > 0 ? br / 4 : 1), COLOR_WHITE);
    }
}

void Ui_DrawIcon(UiIcon icon, int16_t cx, int16_t cy, int16_t r)
{
    switch(icon)
    {
        case UI_ICON_U:
        case UI_ICON_L:
        case UI_ICON_R:
        case UI_ICON_D:           DrawButton(icon, cx, cy, r);  break;
        case UI_ICON_START:       DrawStart(cx, cy, r);         break;
        case UI_ICON_STICK:       DrawStick(cx, cy, r, false);  break;
        case UI_ICON_STICK_CLICK: DrawStick(cx, cy, r, true);   break;
        default:                  break;
    }
}

//*****************************************************************************
// Hint bar
//*****************************************************************************
static int16_t TextW(const char *s)
{
    return (int16_t)Graphics_getStringWidth(&g_sContext, (const int16_t *)s, AUTO_STRING_LENGTH);
}

static void TextAt(const char *s, int16_t x, int16_t yCenter, uint32_t c, const Graphics_Font *f)
{
    int16_t top = (int16_t)(yCenter - (int16_t)Graphics_getFontHeight(f) / 2);

    Graphics_setForegroundColor(&g_sContext, c);
    Graphics_drawString(&g_sContext, (int16_t *)s, AUTO_STRING_LENGTH, x, top, TRANSPARENT_TEXT);
}

void Ui_DrawHints(const UiHint *h, uint16_t n, int16_t y, uint32_t bg)
{
    const Graphics_Font *oldFont = g_sContext.font;
    uint32_t             oldFg   = g_sContext.foreground;
    const int16_t        r       = 7;
    int16_t              total   = 0, x, w;
    uint16_t             i;

    Graphics_setFont(&g_sContext, &g_sFontCmss12b);

    for(i = 0; i < n; i++)
    {
        w = Ui_IconWidth(h[i].icon, r);
        if(w) w += UI_ICON_TEXT_GAP;
        total += w + TextW(h[i].label);
        if(i + 1 < n) total += UI_HINT_GAP;
    }

    Graphics_FillRect(0, y - r - 2, SCREEN_W - 1, y + r + 2, bg);   // clear our own strip

    x = (int16_t)((SCREEN_W - total) / 2);
    if(x < 2) x = 2;

    for(i = 0; i < n; i++)
    {
        w = Ui_IconWidth(h[i].icon, r);
        if(w)
        {
            Ui_DrawIcon(h[i].icon, (int16_t)(x + w / 2), y, r);
            x += w + UI_ICON_TEXT_GAP;
        }
        TextAt(h[i].label, x, y, COLOR_WHITE, &g_sFontCmss12b);
        x += TextW(h[i].label) + UI_HINT_GAP;
    }

    Graphics_setFont(&g_sContext, oldFont);                 // callers keep their font
    Graphics_setForegroundColor(&g_sContext, oldFg);
}

void Ui_HintsMenu(int16_t y, uint32_t bg)
{
    static const UiHint k[] =
    {
        { UI_ICON_MOVE,   "Pick" },
        { UI_ICON_SELECT, "OK"   },
        { UI_ICON_BACK,   "Back" }
    };
    Ui_DrawHints(k, UI_ARRAY_LEN(k), y, bg);
}

void Ui_HintsAnyKey(int16_t y, uint32_t bg)
{
    static const UiHint k[] = { { UI_ICON_SELECT, "Back" } };
    Ui_DrawHints(k, UI_ARRAY_LEN(k), y, bg);
}

// [symbol] verb, centered, big font (instruction screens).
void Ui_DrawPressLine(UiIcon icon, const char *verb, int16_t y, int16_t r, uint32_t fg)
{
    const Graphics_Font *oldFont = g_sContext.font;
    uint32_t             oldFg   = g_sContext.foreground;
    int16_t              iw      = Ui_IconWidth(icon, r);
    int16_t              tw, x;

    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    tw = TextW(verb);
    x  = (int16_t)((SCREEN_W - (iw + (iw ? 8 : 0) + tw)) / 2);
    if(x < 2) x = 2;

    if(iw)
    {
        Ui_DrawIcon(icon, (int16_t)(x + iw / 2), y, r);
        x += iw + 8;
    }
    TextAt(verb, x, y, fg, &g_sFontCmss20b);

    Graphics_setFont(&g_sContext, oldFont);
    Graphics_setForegroundColor(&g_sContext, oldFg);
}

//*****************************************************************************
// Input rules (edges). One place maps ROLES -> physical buttons.
//*****************************************************************************
bool Ui_Pressed(UiIcon icon)
{
    switch(icon)
    {
        case UI_ICON_U: return g_dpad.uPressed != 0;
        case UI_ICON_L: return g_dpad.lPressed != 0;
        case UI_ICON_R: return g_dpad.rPressed != 0;
        case UI_ICON_D: return g_dpad.dPressed != 0;
        default:        return false;
    }
}

bool Ui_Held(UiIcon icon)
{
    switch(icon)
    {
        case UI_ICON_U: return g_dpad.u != 0;
        case UI_ICON_L: return g_dpad.l != 0;
        case UI_ICON_R: return g_dpad.r != 0;
        case UI_ICON_D: return g_dpad.d != 0;
        default:        return false;
    }
}

bool Ui_Select(void) { return Ui_Pressed(UI_ICON_SELECT) || g_joy.btnPressed; }
bool Ui_Jump(void)   { return Ui_Pressed(UI_ICON_JUMP)   || g_joy.btnPressed; }
bool Ui_Back(void)   { return Ui_Pressed(UI_ICON_BACK); }
bool Ui_Act2(void)   { return Ui_Pressed(UI_ICON_ACT2); }
bool Ui_Act3(void)   { return Ui_Pressed(UI_ICON_ACT3); }

bool Ui_SelectHeld(void)
{
    return g_joy.btn || g_dpad.d;           // BLUE position (see UI_ICON_SELECT)
}

bool Ui_AnyPress(void)
{
    return g_joy.btnPressed || g_dpad.uPressed || g_dpad.lPressed ||
           g_dpad.rPressed  || g_dpad.dPressed;
}

int16_t Ui_NavX(void)
{
    return g_joy.edgeX;
}

int16_t Ui_NavY(void)
{
    return g_joy.edgeY;
}

int16_t Ui_MoveX(void)
{
    if(g_joy.dirX) return g_joy.dirX;
#if UI_BUTTONS_ALSO_MOVE
    if(g_dpad.l) return -1;
    if(g_dpad.r) return  1;
#endif
    return 0;
}

int16_t Ui_MoveY(void)
{
    if(g_joy.dirY) return g_joy.dirY;
#if UI_BUTTONS_ALSO_MOVE
    if(g_dpad.u) return -1;
    if(g_dpad.d) return  1;
#endif
    return 0;
}
