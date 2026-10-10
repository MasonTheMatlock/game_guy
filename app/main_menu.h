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

#endif // __MAIN_MENU_H__
