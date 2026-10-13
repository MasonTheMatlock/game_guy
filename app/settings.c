#include "settings.h"

//*****************************************************************************
// Settings app: sound test for now
//   D-pad C : next song (previews it, looping)
//   D-pad D : play the coin sound over the music
//   TODO: brightness and whatever else I can think of!
//*****************************************************************************
static uint16_t s_songIdx = 0;          // survives between launches
static bool     s_redraw;               // song name needs redrawing

static void DrawSongName(void)
{
    // Clear the old name first: the new one may be shorter.
    Graphics_FillRect(10, 72, SCREEN_W - 11, 98, COLOR_BLACK);

    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)songList[s_songIdx]->name,
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 85, OPAQUE_TEXT);
}

//*****************************************************************************
// init: runs once when the app starts. Reset state and draw the screen.
//*****************************************************************************
static void settings_Init(void)
{
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    // Same title style as the other screens: centered text over a divider line.
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Settings",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 15, OPAQUE_TEXT);
    Graphics_drawLine(&g_sContext, 10, 30, SCREEN_W - 11, 30);

    Graphics_drawStringCentered(&g_sContext, (int16_t *)"SOUND TEST",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 55, OPAQUE_TEXT);

    Graphics_setForegroundColor(&g_sContext, COLOR_GRAY);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"C = next song",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 140, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"D = coin sound",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 165, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"Hold btn = menu",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 214, OPAQUE_TEXT);

    Buzzer_PlaySong(songList[s_songIdx], 1);
    s_redraw = true;
}

//*****************************************************************************
// update: once per frame, BEFORE render. Logic only.
//*****************************************************************************
static AppStatus settings_Update(void)
{
    if(g_dpad.cPressed)
    {
        if(++s_songIdx >= numSongs) s_songIdx = 0;      // wrap to the first song
        Buzzer_PlaySong(songList[s_songIdx], 1);
        s_redraw = true;
    }

    if(g_dpad.dPressed)
    {
        Buzzer_PlaySfx(&sfx_coin);                      // the song keeps its place underneath
    }

    return APP_CONTINUE;
}

//*****************************************************************************
// render: once per frame, AFTER update. Drawing only.
//*****************************************************************************
static void settings_Render(void)
{
    if(s_redraw)
    {
        DrawSongName();
        s_redraw = false;
    }
}

//*****************************************************************************
// The app descriptor: the ONLY public symbol in this file.
//   Already declared in apps.h and listed in g_apps[] in app/apps.c.
//*****************************************************************************
const App g_settingsApp =
{
    "Settings",                         // name (first letter = menu tile icon)
    "Change User Settings",             // hint shown under the menu
    COLOR_DARK_GRAY,                    // menu tile color
    RC_FRAME_US,                        // frame period
    settings_Init, settings_Update, settings_Render,
    NULL,                               // no exit() cleanup needed (app loop stops the music)
    NULL                                // not a legacy app
};
