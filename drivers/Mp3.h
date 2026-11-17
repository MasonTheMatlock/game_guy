//*****************************************************************************
//
// mp3.h - Driver for the DFR0299 / DFPlayer Mini MP3 module.
//
// Talks the module's 10-byte serial protocol through the HAL in
// HAL_F28069_DFR0299_MP3_UART.c. Put your sounds on a FAT32 microSD card as
//   /MP3/0001.mp3, /MP3/0002.mp3, ...   (4-digit, zero padded)
// and play them with MP3_playTrack(1), MP3_playTrack(2), ...
//
// Typical use:
//     MP3_init(15);                  // once, after the HAL / clocks are up
//     MP3_playTrack(1);
//     while(1) { MP3_poll(); if(MP3_trackFinished()) MP3_playTrack(2); }
//
//*****************************************************************************

#ifndef __MP3_H__
#define __MP3_H__

#include "F2806x_Device.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_VOLUME_MAX      30      // module range is 0..30

// Equalizer presets for MP3_setEQ()
#define MP3_EQ_NORMAL       0
#define MP3_EQ_POP          1
#define MP3_EQ_ROCK         2
#define MP3_EQ_JAZZ         3
#define MP3_EQ_CLASSIC      4
#define MP3_EQ_BASS         5

// Error codes reported by the module (MP3_getError())
#define MP3_ERR_NONE        0
#define MP3_ERR_BUSY        1       // module is busy / still initializing
#define MP3_ERR_SLEEPING    2
#define MP3_ERR_RX          3       // serial frame error
#define MP3_ERR_CHECKSUM    4
#define MP3_ERR_NO_FILE     5       // track out of range
#define MP3_ERR_NOT_FOUND   6       // track not found on card
#define MP3_ERR_INSERTION   7       // bad insertion / advert in wrong state
#define MP3_ERR_SD          8       // SD card read failure
#define MP3_ERR_SLEEP       0x0A

// Initializes the HAL and the module (blocks ~2.5 s while it boots and the
// card mounts). Returns 1 if the module answered, 0 if nothing came back
// (check wiring, SD card, 1k resistor, GND). initialVolume is 0..30.
extern Uint16 MP3_init(Uint16 initialVolume);

// Call regularly from the main loop. Drains the serial FIFO and updates the
// status flags below. Never blocks.
extern void MP3_poll(void);

// Playback
extern void MP3_playTrack(Uint16 track);                    // /MP3/000N.mp3
extern void MP3_playFolderTrack(Uint16 folder, Uint16 track); // /01/001.mp3
extern void MP3_pause(void);
extern void MP3_resume(void);
extern void MP3_stop(void);
extern void MP3_next(void);
extern void MP3_previous(void);
extern void MP3_loopTrack(Uint16 track);                    // repeat one forever
extern void MP3_loopAll(void);                              // repeat whole card
extern void MP3_randomAll(void);

// Settings
extern void MP3_setVolume(Uint16 volume);                   // 0..30
extern void MP3_volumeUp(void);
extern void MP3_volumeDown(void);
extern void MP3_setEQ(Uint16 eq);                           // MP3_EQ_*
extern Uint16 MP3_getVolume(void);                          // last volume sent

// Status (updated by MP3_poll)
// Link diagnostics (see MP3_ping)
extern Uint16 MP3_ping(void);           // 1 if the module answered a status query
extern Uint16 MP3_getRxBytes(void);     // raw bytes received since the last ping
extern Uint16 MP3_getLastCmd(void);     // command byte of the last valid frame
extern Uint16 MP3_getLastParam(void);   // parameter of the last valid frame

extern Uint16 MP3_initDone(void);       // 1 if the module sent its init-complete message
extern Uint16 MP3_cardOnline(void);     // 1 once the SD card is reported
extern Uint16 MP3_trackFinished(void);  // 1 once per finished track (self-clears)
extern Uint16 MP3_getError(void);       // last error code, clears on read

// Low level: send any command (cmd, param high byte, param low byte)
extern void MP3_sendCommand(Uint16 cmd, Uint16 param1, Uint16 param2);

#ifdef __cplusplus
}
#endif

#endif // __MP3_H__
