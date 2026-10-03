//#############################################################################
// FILE:   app/main_menu.c
// TITLE:  Main menu - pick a game with the joystick
//
// WHAT IT DOES
//   Draws one button per entry in g_apps[] and lets the player highlight one
//   with the joystick (up/down) and choose it with the button.
//
// DESIGN NOTE
//   This file does NOT know which games exist. It reads names, hints and
//   colors from the g_apps[] table (defined in main.c) and returns the INDEX
//   of the chosen entry. Launching the game is main.c's job. That keeps the
//   menu tiny and means adding a game never requires editing this file.
//
// CONTROLS
//   stick up/down : move the highlight (wraps around)
//   button        : start the highlighted game
//#############################################################################

#include "apps.h"

//*****************************************************************************
// Layout (pixels). Screen is 320 x 240.
//
//     y =  15   "SELECT GAME"        title
//     y =  30   ----------------     divider line
//     y =  52   [   button 0   ]     MENU_FIRST_Y
//     y = 112   [   button 1   ]     MENU_FIRST_Y + MENU_SPACING
//     y = 186   control hint         for the highlighted item
//     y = 214   "Hold btn = menu"    reminder of how to leave a game
//
// The hint area starts at y = 176, so this layout holds MENU_MAX_ITEMS = 2
// buttons. A 3rd entry needs smaller buttons/spacing or a scrolling list.
//*****************************************************************************
#define MENU_MAX_ITEMS     2
#define MENU_BOX_X0        60
#define MENU_BOX_X1        259
#define MENU_BOX_H         45
#define MENU_FIRST_Y       52
#define MENU_SPACING       60
#define MENU_HINT_AREA_Y   176          // everything below this is the hint area
#define MENU_HINT_Y        186
#define MENU_REMINDER_Y    214
#define MENU_LOOP_US       15000UL      // poll the stick ~60 times a second

//extern const Graphics_Font g_sFontCmss20b;


//*****************************************************************************
// Drawing
//*****************************************************************************

// Draw button number i. The selected button is filled with its own color
// (black text); the others are a gray outline (white text).
static void Menu_DrawItem(int16_t i, bool selected)
{
    Graphics_Rectangle box;
    int16_t y = MENU_FIRST_Y + i * MENU_SPACING;

    box.xMin = MENU_BOX_X0;
    box.xMax = MENU_BOX_X1;
    box.yMin = y;
    box.yMax = y + MENU_BOX_H - 1;

    if(selected)
    {
        Graphics_FillRect(box.xMin, box.yMin, box.xMax, box.yMax, g_apps[i].color);
        Graphics_setForegroundColor(&g_sContext, COLOR_BLACK);
    }
    else
    {
        Graphics_FillRect(box.xMin, box.yMin, box.xMax, box.yMax, COLOR_BLACK);   // erase old highlight
        Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
        Graphics_drawRectangle(&g_sContext, &box);
        Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    }

    // TRANSPARENT_TEXT draws only the letters, so the fill color shows through.
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_apps[i].name,
                                AUTO_STRING_LENGTH,
                                (MENU_BOX_X0 + MENU_BOX_X1) / 2,
                                y + MENU_BOX_H / 2,
                                TRANSPARENT_TEXT);
}

// Draw the whole menu. full = true also clears the screen and draws the
// title (used once on entry); full = false only redraws what a selection
// change affects (buttons + hint text), which is faster and flicker-free.
static void Menu_Draw(int16_t sel, int16_t count, bool full)
{
    int16_t i;

    if(full)
    {
        Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
        Graphics_setFont(&g_sContext, &g_sFontCmss20b);
        Graphics_clearDisplay(&g_sContext);

        Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
        Graphics_drawStringCentered(&g_sContext, (int16_t *)"SELECT GAME",
                                    AUTO_STRING_LENGTH, 159, 15, OPAQUE_TEXT);
        Graphics_drawLine(&g_sContext, 10, 30, 309, 30);
    }

    for(i = 0; i < count; i++) Menu_DrawItem(i, (i == sel));

    // Hint area: erase it first, because new text can be shorter than the old.
    Graphics_FillRect(0, MENU_HINT_AREA_Y, SCREEN_W - 1, SCREEN_H - 1, COLOR_BLACK);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)g_apps[sel].hint,
                                AUTO_STRING_LENGTH, 159, MENU_HINT_Y,
                                TRANSPARENT_TEXT);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, 159, MENU_REMINDER_Y,
                                TRANSPARENT_TEXT);
}



// Shows the menu and blocks until the player presses the button.
// Returns the index into g_apps[] of the chosen entry.
int16_t Menu_Run(void)
{
    static int16_t sel = 0;             // 'static' = remembers the last choice
                                        // between visits to the menu
    int16_t count = (int16_t)g_appCount;

    if(count > MENU_MAX_ITEMS) count = MENU_MAX_ITEMS;   // all that fits on screen
    if(sel >= count) sel = 0;

    Menu_Draw(sel, count, true);

    while(1)
    {
        Joy_Update();

       
        if(g_joy.edgeY != 0)
        {
            sel += g_joy.edgeY;                     // +1 = stick down
            if(sel < 0)      sel = count - 1;       // wrap around the ends
            if(sel >= count) sel = 0;
            Menu_Draw(sel, count, false);
        }

        if(g_joy.btnPressed) return sel;

        DELAY_US(MENU_LOOP_US);
    }
}
