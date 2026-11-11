//#############################################################################
// FILE:   app/songs.h
// TITLE:  Song and sound-effect data (no hardware access - see drivers/buzzer.c)
//
//   Apps use these through apps.h:
//       Buzzer_PlaySong(&song_tetris, 1);
//       Buzzer_PlaySfx(&sfx_coin);
//
//   Shared sounds live here. A sound that belongs to one app goes in that app's
//   folder (see app/tetris/tetris_sound.c). To let the Test app's sound page
//   cycle to a new song, add it to songList[] in app/test/test_songs.c.
//#############################################################################

#ifndef __SONGS_H__
#define __SONGS_H__

#include "drivers/buzzer.h"


// ---- UI ----
extern const Song sfx_menu_move;        // scroll tick
extern const Song sfx_menu_bump;        // hit the end of the list
extern const Song sfx_menu_select;      // open / confirm
extern const Song sfx_menu_back;        // back / cancel
extern const Song sfx_pause;            // opening the pause menu
extern const Song sfx_resume;           // returning to the game
// ---- Games ----
extern const Song sfx_blip;             // rotate / small move
extern const Song sfx_thud;             // piece lands / block hits
extern const Song sfx_jump;
extern const Song sfx_laser;
extern const Song sfx_powerup;
extern const Song sfx_hit;              // player hurt
extern const Song sfx_explosion;
extern const Song sfx_level_up;
extern const Song sfx_game_over;

//sound experiment
extern const Song sfx_boot_dmg;         // original Game Boy "ba-ding"
extern const Song sfx_boot_gbc;         // Game Boy Color style rising sparkle + ding
//og sounds
extern const Song sfx_coin;             // coin-collect pickup (one-shot)

extern const uint16_t    numSongs;

#endif // __SONGS_H__
