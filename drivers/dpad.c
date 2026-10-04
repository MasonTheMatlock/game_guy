//#############################################################################
// FILE:   dpad.c
// TITLE:  4-button D-pad driver (buttons A, B, C, D)
//
//   A = GPIO25     B = GPIO52     C = GPIO53     D = GPIO56
//
// Same idea as joystick.c: raw pin -> time-based debounce -> "held" and
// "pressed this poll" flags in g_dpad.
//#############################################################################

#include "drivers.h"

Dpad g_dpad;

#define DPAD_COUNT   4

static int16_t  g_dpadState[DPAD_COUNT];   // debounced state (1 = pressed)
static uint32_t g_dpadTick[DPAD_COUNT];    // time of the last accepted change
static int16_t  g_dpadAny;                 // any new press on the last update

// ---- REGISTER LEVEL HARDWARE CONFIGURATION ----------------------------------

void Init_Dpad_GPIO(void)
{
    EALLOW;

    // Button A
    GpioCtrlRegs.GPAMUX2.bit.GPIO25  = 0;   // 0 = plain GPIO
    GpioCtrlRegs.GPAQSEL2.bit.GPIO25 = 0;   // synchronous sampling
    GpioCtrlRegs.GPADIR.bit.GPIO25   = 0;   // 0 = input
    GpioCtrlRegs.GPAPUD.bit.GPIO25   = 0;   // 0 = pull-up enabled

    // Button B
    GpioCtrlRegs.GPBMUX2.bit.GPIO52  = 0;
    GpioCtrlRegs.GPBQSEL2.bit.GPIO52 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO52   = 0;
    GpioCtrlRegs.GPBPUD.bit.GPIO52   = 0;

    // Button C
    GpioCtrlRegs.GPBMUX2.bit.GPIO53  = 0;
    GpioCtrlRegs.GPBQSEL2.bit.GPIO53 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO53   = 0;
    GpioCtrlRegs.GPBPUD.bit.GPIO53   = 0;

    // Button D
    GpioCtrlRegs.GPBMUX2.bit.GPIO56  = 0;
    GpioCtrlRegs.GPBQSEL2.bit.GPIO56 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO56   = 0;
    GpioCtrlRegs.GPBPUD.bit.GPIO56   = 0;

    EDIS;
}

// ---- RAW READ ---------------------------------------------------------------

// Returns 1 if button i (0=A, 1=B, 2=C, 3=D) is pressed right now, undebounced
static int16_t Dpad_ReadRaw(int16_t i)
{
    uint16_t pin;

    switch(i)
    {
        case 0:  pin = GpioDataRegs.GPADAT.bit.GPIO25; break;
        case 1:  pin = GpioDataRegs.GPBDAT.bit.GPIO52; break;
        case 2:  pin = GpioDataRegs.GPBDAT.bit.GPIO53; break;
        default: pin = GpioDataRegs.GPBDAT.bit.GPIO56; break;
    }

#if DPAD_ACTIVE_LOW
    return (pin == 0) ? 1 : 0;
#else
    return (pin != 0) ? 1 : 0;
#endif
}

// ---- PUBLIC API -------------------------------------------------------------

void Dpad_Init(void)
{
    int16_t i;

    Init_Dpad_GPIO();
    DELAY_US(100);                          // let the pull-ups settle

    // Start from the real state so a button held at boot causes no phantom press
    for(i = 0; i < DPAD_COUNT; i++)
    {
        g_dpadState[i] = Dpad_ReadRaw(i);
        g_dpadTick[i]  = TimerNow();
    }
    g_dpadAny = 0;

    Dpad_Update();
}

// Debounce rule (same as the joystick button): a change is accepted only if
// the previous accepted change was at least BTN_DEBOUNCE_MS ago. The first
// press after a quiet period therefore goes through with zero added delay.
void Dpad_Update(void)
{
    int16_t i, raw;
    int16_t pressed[DPAD_COUNT] = {0, 0, 0, 0};

    g_dpadAny = 0;

    for(i = 0; i < DPAD_COUNT; i++)
    {
        raw = Dpad_ReadRaw(i);

        if(raw != g_dpadState[i] && MsSince(g_dpadTick[i]) >= BTN_DEBOUNCE_MS)
        {
            g_dpadState[i] = raw;
            g_dpadTick[i]  = TimerNow();
            if(raw)
            {
                pressed[i] = 1;
                g_dpadAny  = 1;
            }
        }
    }

    g_dpad.a = g_dpadState[0];   g_dpad.aPressed = pressed[0];
    g_dpad.b = g_dpadState[1];   g_dpad.bPressed = pressed[1];
    g_dpad.c = g_dpadState[2];   g_dpad.cPressed = pressed[2];
    g_dpad.d = g_dpadState[3];   g_dpad.dPressed = pressed[3];
}

int16_t Dpad_AnyPressed(void)
{
    return g_dpadAny;
}