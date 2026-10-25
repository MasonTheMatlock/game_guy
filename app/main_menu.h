//#############################################################################
// FILE:   app/main_menu.h
// TITLE:  Main menu - public descriptor
//
// The menu is an app like any other, except it is not listed in g_apps[]
// (it is not a tile) and the runner starts it first and after every app.
//#############################################################################

#ifndef __MAIN_MENU_H__
#define __MAIN_MENU_H__

#include "apps.h"

extern const App g_menuApp;             // app/main_menu.c

// ---- Background color (picked in Settings > Apps > Menu) -------------------
// The menu has MENU_NUM_BGS preset background colors. Only the palette INDEX
// is saved (SAVE_SLOT_MENU), so the colors themselves can be tweaked later.
#define MENU_NUM_BGS   8

uint32_t    Menu_BgColor(uint16_t idx);     // 0xRRGGBB of palette entry idx
const char *Menu_BgName(uint16_t idx);      // "Black", "Navy", ...
uint16_t    Menu_GetBg(void);               // saved index (0 if never saved)
void        Menu_SetBg(uint16_t idx);       // RAM only; caller does Save_Flush()

#endif // __MAIN_MENU_H__
