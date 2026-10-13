//#############################################################################
// FILE:   app/songs.h
// TITLE:  Song and sound-effect data (no hardware access - see drivers/buzzer.c)
//
//   Apps use these through apps.h:
//       Buzzer_PlaySong(&song_tetris, 1);
//       Buzzer_PlaySfx(&sfx_coin);
//
//   To add a song: define it in songs.c, declare it here, and (if you want
//   the Settings app to cycle to it) add it to songList[] in songs.c.
//#############################################################################

#ifndef __SONGS_H__
#define __SONGS_H__

#include "drivers/buzzer.h"

extern const Song song_corridor_run;    // original dungeon-crawler theme (raycaster)
extern const Song song_tetris;          // Korobeiniki, the traditional folk melody
extern const Song sfx_coin;             // coin-collect pickup (one-shot)

extern const Song *const songList[];    // every background song, in Settings order
extern const uint16_t    numSongs;

#endif // __SONGS_H__
