// menu_settings.c - the launcher's own settings page (pause menu > Menu Settings)
//   Menu Color     appearance: the launcher's background colour
// Pure data: settings.c draws and runs it (see AppSettings in apps.h).
#include "menu_priv.h"

static uint32_t    BgColorAt(uint16_t i) { return Menu_BgColor(i); }
static const char *BgNameAt(uint16_t i)  { return Menu_BgName(i); }
static uint16_t    BgGet(void)           { return Menu_GetBg(); }
static void        BgSet(uint16_t i)     { Menu_SetBg(i); }            // RAM only; settings.c flushes

static const SettingItem s_items[] =
{
    {   "Menu Color", SET_COLOR,
        NULL, NULL,
        MENU_NUM_BGS, BgColorAt, BgNameAt, BgGet, BgSet, "SELECT GAME",
        NULL, NULL, NULL, false, NULL },

};

const AppSettings g_menuSettings = { "Menu Settings", s_items, sizeof(s_items) / sizeof(s_items[0]) };
