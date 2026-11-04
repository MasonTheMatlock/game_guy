//#############################################################################
// FILE:   app/wason.h
// TITLE:  Wason Ware - a WarioWare-style microgame marathon (public header)
//
//   Every few seconds a new 3-5 second microgame flashes up with a one-word
//   command. Do it (or survive it) before the fuse burns out. Lose a game and
//   you lose a life; win and the pace picks up every WW_PER_LEVEL games.
//
//   Microgames (23), each a nod to a game you know:
//     CLIMB!  Donkey Kong      BATTLE!  Pokemon (Hydro Pump / Flamethrower / Razor Leaf)
//     FLIP!   Pokemon Pinball  RUN!     Sonic            PLANT!   Plants vs Zombies
//     BLAST!  Metroid (stick aims the arm cannon)         SLICE!   Fruit Ninja
//     RACE!   Cruis'n USA      SURF!    Subway Surfers   CALL IT! Helldivers 2
//     BREAK!  Brick Breaker    FIT IT!  Tetris           CHOMP!   Pac-Man
//     CROSS!  Frogger          FLAP!    Flappy Bird      JUMP!    Chrome Dino
//     STOMP!  Super Mario      DASH!    Geometry Dash    LAUNCH!  Angry Birds
//     BEAT!   Melatonin        SORT!    Recycler         RETURN!  Pong
//     SHOOT!  Galaga
//
//   INTRO: pressing PLAY first shows an angry purple Wario telling you "You'll
//   never get to 100!" (typed out, talk blips, flapping mouth). His picture is
//   isolated in TauntDrawFace()/TauntDrawMouth() in wason.c so a real image can
//   replace the pixel art later. Any button skips it.
//
//   BEAT! plays song_melatonin (songs.c): the dreamy funk loop, 150 ms per
//   eighth-note unit. Its notes are placed from the song's own clock (kMlUnit in
//   wason.c lists which song units to hit), so they land on the music however
//   fast the screen is.
//
//   DISPLAY NOTES (LAUNCHXL-F28069M -> 320x240 BoosterPack, ~250k pixels/s):
//     * each MicroGame has a render window in kGames[]; (0,0) inside a game is the
//       window's top-left and drawing is clipped to it. Smaller window = cheaper.
//     * art sprites: DrawArt()/MoveArt() draw opaque pixel-art with merged runs.
//     * moving things: draw the NEW image first, then EraseOutside() only the part
//       of the old one that was vacated. Never erase-then-redraw (that flickers).
//
//   MENU: title screen = PLAY (the marathon) or PRACTICE (pick any microgame from
//   a scrolling list, L/R sets the speed level; no lives, no score, and after
//   each try you land back on the list). New microgames appear in it automatically.
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
