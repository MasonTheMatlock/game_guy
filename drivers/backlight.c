//#############################################################################
// FILE:   drivers/backlight.c
// TITLE:  Backlight driver - implementation (see backlight.h)
//
//   The ePWM free-runs in up-count mode: output SET at TBCTR = 0, CLEARED at
//   the compare value, so the high time (backlight on) is CMP counts of
//   PERIOD. A compare above TBPRD never matches = constant high = 100%.
//   The compare register is shadowed (loads at TBCTR = 0), so a level change
//   takes effect cleanly at the next period; one 16-bit write, no DINT needed.
//#############################################################################

#include "drivers.h"            // device headers, TICKS_PER_MS
#include "backlight.h"

//*****************************************************************************
// Tuning
//*****************************************************************************
// PWM frequency. 20 kHz is above hearing (no coil whine) and far above
// flicker. If low levels look uneven, try something lower (e.g. 5000UL).
#define BACKLIGHT_PWM_HZ    20000UL

#define SYSCLK_HZ           (TICKS_PER_MS * 1000UL)     // ePWM runs at SYSCLK (dividers /1)

// Duty per level in percent. Eyes are roughly logarithmic, so the steps grow
// geometrically instead of evenly. Level 0 stays visible. Tune by eye.
static const uint16_t s_dutyPct[BACKLIGHT_NUM_LEVELS] =
{
    12, 25, 37, 50, 62, 75, 87, 100
};

//*****************************************************************************
// Which ePWM / pin
//*****************************************************************************
#if BACKLIGHT_PIN == BACKLIGHT_PIN_J4_37            // GPIO3 -> ePWM2B
    #define BL_PWM              EPwm2Regs
    #define BL_SET_CMP(v)       (EPwm2Regs.CMPB = (v))
#elif BACKLIGHT_PIN == BACKLIGHT_PIN_J4_40          // GPIO0 -> ePWM1A
    #define BL_PWM              EPwm1Regs
    #define BL_SET_CMP(v)       (EPwm1Regs.CMPA.half.CMPA = (v))
#else
    #error "BACKLIGHT_PIN must be BACKLIGHT_PIN_J4_37 or BACKLIGHT_PIN_J4_40"
#endif

static uint32_t Period(void)                        // TBCLK counts per PWM period
{
    uint32_t p = SYSCLK_HZ / BACKLIGHT_PWM_HZ;

    if(p > 65535UL) p = 65535UL;                    // TBPRD is 16 bits
    return p;
}

static volatile uint16_t s_level = BACKLIGHT_NUM_LEVELS - 1;    // full by default

static void ApplyLevel(uint16_t level)
{
    uint32_t period = Period();
    uint32_t cmp;

    if(level >= BACKLIGHT_NUM_LEVELS) level = BACKLIGHT_NUM_LEVELS - 1;
    s_level = level;

    if(s_dutyPct[level] >= 100)
        cmp = period;                               // > TBPRD: never clears, stays high
    else
    {
        cmp = (period * (uint32_t)s_dutyPct[level]) / 100UL;
        if(cmp < 1UL) cmp = 1UL;
    }

    BL_SET_CMP((uint16_t)cmp);                      // shadowed: loads at next TBCTR = 0
}

//*****************************************************************************
// Setup
//*****************************************************************************
void Backlight_Init(void)
{
    EALLOW;

    // Pin -> ePWM output, clock on. Call after InitGpio() so this wins.
#if BACKLIGHT_PIN == BACKLIGHT_PIN_J4_37
    GpioCtrlRegs.GPAPUD.bit.GPIO3  = 1;             // pull-up off (output pin)
    GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;             // 1 = EPWM2B
    SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 1;
#else
    GpioCtrlRegs.GPAPUD.bit.GPIO0  = 1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;             // 1 = EPWM1A
    SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1;
#endif

    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;          // pause the time bases while configuring

    BL_PWM.TBCTL.bit.CTRMODE   = 0;                 // up-count
    BL_PWM.TBCTL.bit.HSPCLKDIV = 0;                 // /1
    BL_PWM.TBCTL.bit.CLKDIV    = 0;                 // /1
    BL_PWM.TBCTL.bit.PHSEN     = 0;
    BL_PWM.TBCTL.bit.PRDLD     = 0;                 // TBPRD shadowed
    BL_PWM.TBCTL.bit.SYNCOSEL  = 3;                 // no sync output
    BL_PWM.TBCTL.bit.FREE_SOFT = 3;                 // free-run in emulation
    BL_PWM.TBPHS.half.TBPHS    = 0;
    BL_PWM.TBCTR               = 0;
    BL_PWM.TBPRD               = (uint16_t)(Period() - 1UL);

#if BACKLIGHT_PIN == BACKLIGHT_PIN_J4_37
    BL_PWM.CMPCTL.bit.SHDWBMODE = 0;                // CMPB shadowed
    BL_PWM.CMPCTL.bit.LOADBMODE = 0;                // load at TBCTR = 0
    BL_PWM.AQCTLB.bit.ZRO = 2;                      // set high at counter = 0
    BL_PWM.AQCTLB.bit.CBU = 1;                      // clear low at CMPB (up-count)
#else
    BL_PWM.CMPCTL.bit.SHDWAMODE = 0;                // CMPA shadowed
    BL_PWM.CMPCTL.bit.LOADAMODE = 0;                // load at TBCTR = 0
    BL_PWM.AQCTLA.bit.ZRO = 2;                      // set high at counter = 0
    BL_PWM.AQCTLA.bit.CAU = 1;                      // clear low at CMPA (up-count)
#endif

    ApplyLevel(s_level);

    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;          // start the time base

    EDIS;
}

//*****************************************************************************
// Public API
//*****************************************************************************
void Backlight_SetLevel(uint16_t level)
{
    ApplyLevel(level);
}

uint16_t Backlight_GetLevel(void)
{
    return s_level;
}
