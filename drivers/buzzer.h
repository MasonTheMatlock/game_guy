//#############################################################################
// FILE:   drivers/buzzer.h
// TITLE:  Buzzer driver - square-wave tones from ePWM4A, plus a tiny song player
//
//   Hardware: passive buzzer + NPN transistor. The transistor base (through
//             a ~1k resistor) connects to GPIO6 (J8:80), which is also ePWM4A.
//
//   The ePWM peripheral makes the square wave, so the CPU does no work while a
//   note plays. A 1 ms interrupt on CPU Timer 1 only steps through the song
//   and loads the next frequency when a note changes.
//
//   RESOURCES THIS DRIVER OWNS
//     GPIO6, ePWM4, CPU Timer 1 (INT13).
//     CPU Timer 0 is NOT touched: the joystick driver uses it as its clock.
//
//   Usage (apps normally just include apps.h):
//     Buzzer_PlaySong(&song_tetris, 1);   // looping background music
//     Buzzer_PlaySfx(&sfx_coin);          // effect on top of the music; the song
//                                         // keeps its place in time underneath
//     Buzzer_Stop();
//#############################################################################

#ifndef __BUZZER_H__
#define __BUZZER_H__

#include <stdint.h>

//*****************************************************************************
// Note frequencies in Hz (rounded)
//*****************************************************************************
#define N_REST  0
#define N_C4    262
#define N_D4    294
#define N_E4    330
#define N_F4    349
#define N_G4    392
#define N_G4S   415
#define N_A4    440
#define N_B4    494
#define N_C5    523
#define N_D5    587
#define N_E5    659
#define N_F5    698
#define N_G5    784
#define N_G5S   831
#define N_A5    880
#define N_B5    988
#define N_C6    1047
#define N_E6    1319

//*****************************************************************************
// Durations, in "eighths" (how long an eighth lasts is set per song)
//*****************************************************************************
#define D_E     1       // eighth
#define D_Q     2       // quarter
#define D_QD    3       // dotted quarter
#define D_H     4       // half
#define D_W     8       // whole

//*****************************************************************************
// Data types
//*****************************************************************************
typedef struct
{
    uint16_t freq;          // Hz, N_REST (0) = silence
    uint16_t eighths;       // length in eighth-note units
} Note;

typedef struct
{
    const Note *notes;
    uint16_t    count;      // number of notes in the table
    uint16_t    eighthMs;   // length of one eighth in ms (the tempo)
    uint16_t    gapMs;      // silent gap at the end of every note (staccato)
    const char *name;       // shown in the Settings app
} Song;

//*****************************************************************************
// API
//*****************************************************************************
// Call once from main(), after InitGpio(). Remuxes GPIO6 to ePWM4A, starts the
// 1 ms Timer 1 interrupt. The caller must enable interrupts afterwards (EINT).
void     Buzzer_Init(void);

void     Buzzer_PlaySong(const Song *song, uint16_t loop);  // start background music
void     Buzzer_PlaySfx(const Song *sfx);                   // effect (restarts if playing)
void     Buzzer_Stop(void);                                 // music + effect off
void     Buzzer_PauseMusic(uint16_t pause);                 // 1 = freeze music, 0 = resume
uint16_t Buzzer_IsPlaying(void);                            // 1 if music or an effect is active

#endif // __BUZZER_H__
