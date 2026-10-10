//#############################################################################
// FILE:   app/raycaster.h
// TITLE:  Raycaster - public descriptor and tuning constants
//#############################################################################

#ifndef __RAYCASTER_H__
#define __RAYCASTER_H__

#include "apps.h"

extern const App g_raycasterApp;            // app/raycaster.c

#define RC_FRAME_US    16000UL          // pause after each frame (~60 fps
                                        //   before drawing time is added)

#endif // __RAYCASTER_H__
