// test_songs.c - the background songs the Test app's sound page cycles through.
// They live here because only the Test app lists them; each belongs to its own app.
#include "test_priv.h"
#include "../raycaster/rc_sound.h"
#include "../tetris/tetris_sound.h"

const Song *const songList[] =
{
    &song_corridor_run,
    &song_tetris,
};

const uint16_t numSongs = sizeof(songList) / sizeof(songList[0]);
