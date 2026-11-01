//#############################################################################
// FILE:   app/wason.h
// TITLE:  Wason Ware - a WarioWare-style microgame marathon (public header)
//
//   Every few seconds a new 3-5 second microgame flashes up with a one-word
//   command. Do it (or survive it) before the fuse burns out. Lose a game and
//   you lose a life; win and the pace picks up every WW_PER_LEVEL games.
//
//   Microgames (22), each a nod to a game you know:
//     CLIMB!  Donkey Kong      BATTLE!  Pokemon          FLIP!    Pokemon Pinball
//     RUN!    Sonic            BLAST!   Metroid          SLICE!   Fruit Ninja
//     RACE!   Hydro Thunder    TURN!    Temple Run       CALL IT! Helldivers 2
//     BREAK!  Brick Breaker    FIT IT!  Tetris           CHOMP!   Pac-Man
//     CROSS!  Frogger          FLAP!    Flappy Bird      JUMP!    Chrome Dino
//     STOMP!  Super Mario      DASH!    Geometry Dash    LAUNCH!  Angry Birds
//     BEAT!   Melatonin        SORT!    Recycler         RETURN!  Pong
//     SHOOT!  Galaga
//
//   HOW TO ADD A MICROGAME (all inside app/wasonware.c)
//     1. Write  static void XxInit(uint16_t spd)   reset state; spd = 0,1,2..
//                                                  (grows every WW_PER_LEVEL games)
//               static int16_t XxUpdate(void)      logic only -> MG_PLAY/MG_WIN/MG_LOSE
//               static void XxRender(void)         drawing only. When s_first is
//                                                  true, draw EVERYTHING (the
//                                                  background was just filled).
//     2. Add one MicroGame line to kGames[] (command word, inspiration tag,
//        frames, background color, win-on-timeout flag, the 3 callbacks).
//   The bag-shuffle, fuse timer, lives, speed-up and high score are automatic.
//#############################################################################

#ifndef __WASON_H__
#define __WASON_H__

#include "apps.h"

extern const App g_wasonwareApp;            // app/wasonware.c

#define WW_FRAME_US     16000UL             // pause after each frame (~60 fps max)
#define WW_LIVES        4                   // lives per run
#define WW_PER_LEVEL    4                   // microgames won/lost per speed-up
#define WW_INTRO_FRAMES 48                  // command-word screen length
#define WW_RESULT_FRAMES 42                 // NICE!/OOPS! banner length

#endif // __WASON_H__
