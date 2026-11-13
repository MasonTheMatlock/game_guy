
#ifndef WASON_SOUND_H
#define WASON_SOUND_H

#include "../songs.h"

// Helper macro for defining sound effects
#define SFX(sym, notes, unitMs, gap, label) \
    const Song sym = { notes, sizeof(notes)/sizeof(notes[0]), unitMs, gap, label }

extern const Song song_wasonware;

extern const Song sfx_wason_talk;
extern const Song sfx_wason_voice1;
extern const Song sfx_wason_voice2;
extern const Song sfx_wason_voice3;
extern const Song sfx_wason_voice4;

void WasonVoice_Tick(uint16_t character);

#endif
