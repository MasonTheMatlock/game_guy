#include "settings.h"  

//*****************************************************************************
// Game state
//   TODO: Allow brightness and sound to be changed in this app 
//         plus whatever else I can think of!
//*****************************************************************************

//*****************************************************************************
// init: runs once when the app starts. Reset game state and draw anything
// that never changes. Today that is just the placeholder screen.
//*****************************************************************************
static void settings_Init(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    // Same title style as the other screens: centered text over a divider line.
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Place Holder",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);

    Graphics_drawStringCentered(&g_sContext, (int16_t *)"COMING SOON",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 110, OPAQUE_TEXT);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 214, OPAQUE_TEXT);

    
}


static AppStatus settings_Update(void)
{
    

    return APP_CONTINUE;
}

//*****************************************************************************
// render: once per frame, AFTER update. Draw the current state of the game.
//*****************************************************************************
static void settings_Render(void)
{
    // TODO: cast the rays and draw the 3D view.
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//   Already declared in apps.h (extern const App g_raycasterApp) and listed in
//   g_apps[] in main.c.
//*****************************************************************************
const App g_settingsApp =
{
    "Settings",                         // name (first letter = menu tile icon)
    "Change User Settings",             // hint shown under the menu
    COLOR_DARK_GRAY,                          // menu tile color
    RC_FRAME_US,                        // frame period
    settings_Init, settings_Update, settings_Render,
    NULL,                               // no exit() cleanup needed
    NULL                                // not a legacy app
};
