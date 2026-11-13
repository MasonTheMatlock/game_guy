
//#############################################################################
// FILE:   app/wason/wason_sound.c
// TITLE:  WASON WARE music and voice effects
//
//   Original arcade-style theme and Undertale-inspired dialogue blips.
//   Designed for a single passive PWM buzzer.
//#############################################################################

#include "wason_sound.h"

//*****************************************************************************
// WASON WARE THEME
// Fast, goofy, frantic arcade music.
//
// A, A, B, A
// Eighth note = 150ms (200 BPM quarter-note tempo)
//*****************************************************************************

#define WASON_A \
    {N_E5,D_E},{N_G5,D_E},{N_E5,D_E},{N_C5,D_E}, \
    {N_D5,D_E},{N_C5,D_E},{N_B4,D_E},{N_G4,D_E}, \
    {N_A4,D_E},{N_C5,D_E},{N_E5,D_E},{N_G5,D_E}, \
    {N_A5,D_Q},{N_G5,D_E},{N_E5,D_E}

#define WASON_B \
    {N_C5,D_E},{N_E5,D_E},{N_G5,D_E},{N_A5,D_E}, \
    {N_G5,D_E},{N_E5,D_E},{N_D5,D_E},{N_B4,D_E}, \
    {N_C5,D_E},{N_D5,D_E},{N_E5,D_E},{N_G5,D_E}, \
    {N_E5,D_Q},{N_C5,D_E},{N_A4,D_E}

static const Note s_wasonNotes[] =
{
    WASON_A,
    WASON_A,
    WASON_B,
    WASON_A,
    {N_REST,D_Q}
};

const Song song_wasonware =
{
    s_wasonNotes,
    sizeof(s_wasonNotes) / sizeof(s_wasonNotes[0]),
    150,                 // Eighth note duration (ms)
    12,                  // Small separation between notes
    "Wason Ware"
};


//*****************************************************************************
// WASON TALK
// Undertale-style synthesized dialogue.
//
// Think of this as a little cartoon character speaking.
// Rapid pitch changes simulate syllables and vocal inflection.
//*****************************************************************************

static const Note n_wasonTalk[] =
{
    {N_C5,1},
    {N_E5,1},
    {N_D5,1},
    {N_G5,2},

    {N_E5,1},
    {N_D5,1},
    {N_C5,2},

    {N_G4,1},
    {N_C5,3}
};

SFX(sfx_wason_talk, n_wasonTalk, 24, 4, "Wason Talk");



//*****************************************************************************
// WASON DIALOGUE VOICE
// Short Undertale-style character blips.
// One blip is played for each displayed character.
//*****************************************************************************

static const Note n_wasonVoice1[] = {{N_C5,1}};
static const Note n_wasonVoice2[] = {{N_D5,1}};
static const Note n_wasonVoice3[] = {{N_E5,1}};
static const Note n_wasonVoice4[] = {{N_G5,1}};

SFX(sfx_wason_voice1, n_wasonVoice1, 18, 0, "Wason Voice 1");
SFX(sfx_wason_voice2, n_wasonVoice2, 18, 0, "Wason Voice 2");
SFX(sfx_wason_voice3, n_wasonVoice3, 18, 0, "Wason Voice 3");
SFX(sfx_wason_voice4, n_wasonVoice4, 18, 0, "Wason Voice 4");

void WasonVoice_Tick(uint16_t character)
{
    static const Song *voices[] =
    {
        &sfx_wason_voice1,
        &sfx_wason_voice2,
        &sfx_wason_voice3,
        &sfx_wason_voice4
    };

    // Don't make spaces or punctuation beep.
    if(character == ' ' || character == '\n' ||
       character == '.' || character == ',' ||
       character == '!' || character == '?')
        return;

    Buzzer_PlaySfx(voices[character & 3]);
}
