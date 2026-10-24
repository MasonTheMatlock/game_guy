//#############################################################################
// FILE:   main.c
// TITLE:  game_guy - hardware boot, then hand off to the app loop
//
//   Board : TMS320F28069 (C2000 Piccolo)
//   Screen: BOOSTXL-K350QVG-S1, 320x240 Kitronix SSD2119 LCD over SPI
//   Input : analog joystick (VRx = ADCINB7, VRy = ADCINB4, button = GPIO55)
//           4-button D-pad  (A = GPIO25, B = GPIO52, C = GPIO53, D = GPIO56)
//   Audio : passive buzzer + NPN on GPIO6 / ePWM4A (J8:80), driven by CPU Timer 1
//
// main.c only boots the hardware. Everything above that lives in app/:
//   app/apps.h / apps.c   the app contract, shared services (g_sContext,
//                         Graphics_FillRect, Rand16), g_apps[], and
//                         App_Loop(): the one frame loop (menu -> app -> menu)
//   app/main_menu.h / .c  the menu
//   app/<name>.h / .c     one pair per app
//#############################################################################

#include <string.h>                         // memcpy (flash -> RAM, Release)
#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"
#include "kitronix320x240x16_ssd2119_spi.h"
#include "grlib/grlib.h"
#include "drivers/drivers.h"
#include "app/apps.h"

//*****************************************************************************
// Boot helpers
//*****************************************************************************
#define WARNING_HOLD_MS   2000

// Joystick_Init() returns 0 if calibration looked wrong. Say so on screen
// instead of failing silently.
//Also a good illustration of how to use inputs to output to the screen.
static void ShowCalibrationWarning(void)
{
    uint16_t i;

    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setFont(&g_sContext, &g_sFontCmss20b);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, COLOR_RED);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"JOYSTICK CALIBRATION",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 90, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int16_t *)"FAILED - CHECK STICK",
                                AUTO_STRING_LENGTH, SCREEN_W / 2, 120, OPAQUE_TEXT);

    for(i = 0; i < WARNING_HOLD_MS / 10; i++) DELAY_US(10000);
}

//*****************************************************************************
// main
//*****************************************************************************
void main(void)
{
    int16_t joystickOk;                     

#ifdef _RELEASE
    // Release (flash) build: copy time-critical code to RAM first.
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart,
           (Uint32)&RamfuncsLoadSize);
#endif

    // Step 1. Clocks, PLL, watchdog, GPIO.
    InitSysCtrl();
    InitGpio();

    // Step 2. Interrupts off, default vector table installed.
    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();

    // Step 3. Joystick before LCD (order proven on this board). Keep the
    // stick untouched at power-up.
    joystickOk = Joystick_Init();
    Dpad_Init();                            // uses the joystick's timer

    // Step 4. LCD + graphics context.
    Kitronix320x240x16_SSD2119Init();
    Graphics_initContext(&g_sContext, &g_sKitronix320x240x16_SSD2119);

    if(!joystickOk) ShowCalibrationWarning();

    // Step 5. Buzzer: GPIO6 -> ePWM4A, plus a 1 ms sequencer on CPU Timer 1.
    //         (CPU Timer 0 is the joystick's free-running clock; leave it alone.)
    //         Nothing earlier needed interrupts, so global interrupts are
    //         enabled here, last, once every vector is in place.
    Buzzer_Init();

    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 0;  // Force back to standard GPIO first
    GpioDataRegs.GPACLEAR.bit.GPIO0 = 1; // Clear any residual high latch state
    EDIS;

    Backlight_Init();

    Save_Init();                    // load saved scores + apply volume/brightness
    //Save_EraseAll();                // TEMPORARY: wipe old/garbage saves ONCE, then delete this line

    EINT;
    ERTM;

    Sfx_PlayWait(&sfx_boot_gbc);    // pick sfx_boot_dmg for the plain "ba-ding"

    // Step 6. Menu -> app -> menu -> ... (never returns)
    App_Loop();
}
