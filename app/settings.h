//#############################################################################
// FILE:   app/settings.h
// TITLE:  Settings app - public descriptor
//#############################################################################

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "apps.h"

extern const App g_settingsApp;             // app/settings.c

// Set by the runner (apps.c) before launching Settings on top of a running app.
extern bool g_settingsOverlay;              // true = opened by holding the stick
// Set by Settings on exit: true = user chose "Main Menu", false = resume.
extern bool g_settingsToMenu;

#endif // __SETTINGS_H__
