//#############################################################################
// FILE:   app/ui.h
// TITLE:  Shared UI: colored button symbols, joystick icons, hint bars and
//         THE control rules every app follows.
//
//  HARDWARE
//     4 colored buttons in a diamond (the old "D-pad"), a joystick with a
//     push button, and (later) a Start button.
//
//                 [RED]                  stick   = MOVE
//        [YELLOW]       [GREEN]          stick click = same as BLUE
//                 [BLUE]                 [START] (grey) = reserved, no hardware yet
//
//  THE RULES (one set, every app, no exceptions)
//     MOVE     the STICK. Menus step on a stick tilt, games move with it.
//     SELECT   BLUE  (or a stick click)  OK / confirm / choose / start
//     JUMP     BLUE  (or a stick click)  jump is the same button as select,
//                                        the "primary action" of the console
//     BACK     RED                       cancel / back / leave a screen
//     ACT2     YELLOW                    secondary action (rotate, swap, turn left ...)
//     ACT3     GREEN                     tertiary action  (turn right, ...)
//     PAUSE    hold the stick click (apps.c does this, apps never do)
//     START    grey, reserved
//
//  Inside a game RED has no "back" to do (hold the stick to pause), so a game
//  may use RED as one more action; the instruction screen always shows what
//  each colored button does with the real colored symbols.
//
//  CHANGING A COLOR OR A ROLE: edit the two tables in this file, nothing else.
//     UI_COL_*      what color each physical button is
//     UI_BTN_*_ROLE which physical button plays which role
//
//  Call the Ui_* input functions from update() only. They read g_dpad/g_joy,
//  which the app loop has already refreshed this frame.
//#############################################################################

#ifndef __UI_H__
#define __UI_H__

#include "apps.h"

//*****************************************************************************
// 1. Colors of the physical buttons (0xRRGGBB). Change here if your caps differ.
//    Physical names: U/L/R/D are the old D-pad positions (top, left, right, bottom).
//*****************************************************************************
#define UI_COL_U        0xE53030UL      // top      RED
#define UI_COL_L        0xF5C400UL      // left     YELLOW
#define UI_COL_R        0x2FB344UL      // right    GREEN
#define UI_COL_D        0x2F6FEDUL     // bottom   BLUE
#define UI_COL_START    0x808080UL      // grey (reserved)
#define UI_COL_RIM      0xE8E8E8UL      // thin outline so a glyph shows on any background

//*****************************************************************************
// 2. Icons. The first four are the physical buttons.
//*****************************************************************************
typedef enum
{
    UI_ICON_U = 0,                      // top button    (red)
    UI_ICON_L,                          // left button   (yellow)
    UI_ICON_R,                          // right button  (green)
    UI_ICON_D,                          // bottom button (blue)
    UI_ICON_START,                      // start (grey pill)
    UI_ICON_STICK,                      // the joystick, tilt = MOVE
    UI_ICON_STICK_CLICK,                // the joystick push button
    UI_ICON_NONE                        // label only, no symbol
} UiIcon;

// Roles -> physical button (see THE RULES above).
#define UI_ICON_SELECT  UI_ICON_D       // blue
#define UI_ICON_JUMP    UI_ICON_D       // blue
#define UI_ICON_BACK    UI_ICON_U       // red
#define UI_ICON_ACT2    UI_ICON_L       // yellow
#define UI_ICON_ACT3    UI_ICON_R       // green
#define UI_ICON_MOVE    UI_ICON_STICK

// 1 = the four colored buttons ALSO move in games (old behaviour), 0 = move is
// the stick only (the rule). Menus never navigate with the buttons either way.
#define UI_BUTTONS_ALSO_MOVE   0

//*****************************************************************************
// 3. Input (edges, use in update() only)
//*****************************************************************************
bool    Ui_Select(void);                // BLUE pressed, or stick click
bool    Ui_Jump(void);                  // same as Ui_Select(): the primary action
bool    Ui_Back(void);                  // RED pressed
bool    Ui_Act2(void);                  // YELLOW pressed
bool    Ui_Act3(void);                  // GREEN pressed
bool    Ui_AnyPress(void);              // any colored button or the stick click
bool    Ui_Pressed(UiIcon icon);        // that ONE physical button pressed this frame (UI_ICON_U/L/R/D)
bool    Ui_Held(UiIcon icon);           // that ONE physical button is being held
bool    Ui_SelectHeld(void);            // BLUE or the stick click is being held (nitro, auto-jump, drop)

int16_t Ui_NavX(void);                  // stick tilted left(-1)/right(+1) on THIS frame (edge)
int16_t Ui_NavY(void);                  // stick tilted up(-1)/down(+1) on THIS frame (edge)

int16_t Ui_MoveX(void);                 // held: -1 / 0 / +1  (stick)
int16_t Ui_MoveY(void);                 // held: -1 up / 0 / +1 down (stick)

//*****************************************************************************
// 4. Symbols
//   (cx,cy) is the CENTER, r the radius in pixels (7 for text lines, 11+ for
//   instruction screens). The drawing background is not touched outside the
//   symbol, so it works on any screen.
//*****************************************************************************
uint32_t Ui_IconColor(UiIcon icon);                 // the button's color
int16_t  Ui_IconWidth(UiIcon icon, int16_t r);      // pixels the symbol covers
void     Ui_DrawIcon(UiIcon icon, int16_t cx, int16_t cy, int16_t r);

//*****************************************************************************
// 5. Hint bar: a row of  [symbol] label  pairs, centered, drawn in the small font
//   Example:   static const UiHint k[] = { {UI_ICON_MOVE,"Pick"}, {UI_ICON_SELECT,"OK"},
//                                          {UI_ICON_BACK,"Back"} };
//              Ui_DrawHints(k, 3, 226);
//   y is the vertical CENTER of the bar. The bar clears its own strip first.
//*****************************************************************************
typedef struct
{
    UiIcon      icon;
    const char *label;
} UiHint;

#define UI_ARRAY_LEN(a)   ((uint16_t)(sizeof(a) / sizeof((a)[0])))

void Ui_DrawHints(const UiHint *h, uint16_t n, int16_t y, uint32_t bg);

// The bars used by almost every screen (so they look the same everywhere).
void Ui_HintsMenu(int16_t y, uint32_t bg);      // MOVE pick  SELECT ok  BACK back
void Ui_HintsAnyKey(int16_t y, uint32_t bg);    // SELECT back ("press the blue button to leave")

// "Press <symbol> to <verb>" as one centered line, for instruction screens.
// icon = the button, verb e.g. "jump". r = symbol radius.
void Ui_DrawPressLine(UiIcon icon, const char *verb, int16_t y, int16_t r, uint32_t fg);

#endif // __UI_H__
