//#############################################################################
// FILE:   drivers/buzzer.c
// TITLE:  Buzzer driver - implementation (see buzzer.h)
//
//   TONE GENERATION (ePWM4A on GPIO6)
//     ePWM4 free-runs in up-count mode. The output is SET at TBCTR = 0 and
//     CLEARED at CMPA = TBPRD / 2, giving a 50% square wave:
//
//         frequency = TBCLK / (TBPRD + 1)
//         TBCLK     = SYSCLKOUT / (HSPCLKDIV * CLKDIV) = SYSCLKOUT / 8
//
//     /8 keeps TBPRD inside 16 bits for notes down to ~170 Hz and still gives
//     fine pitch resolution at the top end. SYSCLKOUT comes from CPU_RATE (the
//     same constant the joystick timer uses), so no clock is hard-coded here.
//     Silence = output forced low (AQCSFRC), so the transistor is fully off.
//
//   SONG ENGINE (CPU Timer 1, 1 ms tick)
//     Music and effects each have a "voice" that counts down the current note.
//     The effect voice owns the pin while it plays, but the music voice keeps
//     advancing underneath, so the song continues where it would have been.
//#############################################################################

#include "drivers.h"            // device headers, CPU_RATE -> TICKS_PER_MS

//*****************************************************************************
// Clocks
//*****************************************************************************
#define TBCLK_DIV     8UL                               // CLKDIV = /8, HSPCLKDIV = /1
#define SYSCLK_HZ     (TICKS_PER_MS * 1000UL)           // TICKS_PER_MS: see joystick.h
#define TBCLK_HZ      (SYSCLK_HZ / TBCLK_DIV)

interrupt void Buzzer_ISR(void);

//*****************************************************************************
// Voice: one stream of notes (the music, or a sound effect)
//*****************************************************************************
typedef struct
{
    const Song *song;
    uint16_t    idx;        // current note
    uint16_t    msLeft;     // ms remaining in the current note
    uint16_t    loop;       // 1 = restart at the end
    uint16_t    active;     // 1 = playing
} Voice;

static Voice    s_music;
static Voice    s_sfx;
static uint16_t s_musicPaused = 0;
static uint16_t s_lastFreq    = 0xFFFF;                 // forces the first update

//*****************************************************************************
// Output: point the ePWM at a frequency (0 = silent). Skips the register
// writes when nothing changed.
//*****************************************************************************
static void SetOutput(uint16_t freq)
{
    uint32_t period;

    if(freq == s_lastFreq) return;
    s_lastFreq = freq;

    if(freq == 0)
    {
        EPwm4Regs.AQCSFRC.bit.CSFA = 1;                 // force output low
    }
    else
    {
        period = (TBCLK_HZ / freq) - 1;
        if(period > 65535UL) period = 65535UL;          // clamp very low notes

        EPwm4Regs.TBPRD          = (uint16_t)period;    // shadowed: loads at next TBCTR = 0
        EPwm4Regs.CMPA.half.CMPA = (uint16_t)(period >> 1);     // 50% duty
        EPwm4Regs.AQCSFRC.bit.CSFA = 0;                 // release: PWM drives the pin
    }
}

//*****************************************************************************
// Voice helpers
//*****************************************************************************
static void VoiceLoad(Voice *v)
{
    const Note *n = &v->song->notes[v->idx];

    v->msLeft = (uint16_t)(n->eighths * v->song->eighthMs);
    if(v->msLeft == 0) v->msLeft = 1;                   // guard zero-length notes
}

static void VoiceStart(Voice *v, const Song *song, uint16_t loop)
{
    v->song   = song;
    v->idx    = 0;
    v->loop   = loop;
    v->active = 1;
    VoiceLoad(v);
}

// Advance one millisecond; returns the frequency this voice wants right now.
static uint16_t VoiceTick(Voice *v)
{
    uint16_t freq;

    if(!v->active) return 0;

    freq = v->song->notes[v->idx].freq;
    if(v->msLeft <= v->song->gapMs) freq = 0;           // silent gap at the end of the note

    if(--v->msLeft == 0)
    {
        if(++v->idx >= v->song->count)
        {
            if(v->loop) v->idx = 0;
            else        v->active = 0;                  // one-shot finished
        }
        if(v->active) VoiceLoad(v);
    }
    return freq;
}

//*****************************************************************************
// Setup
//*****************************************************************************
void Buzzer_Init(void)
{
    EALLOW;

    // GPIO6 -> ePWM4A. Call after InitGpio() so this setting wins.
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1;                 // 1 = EPWM4A
    GpioCtrlRegs.GPAPUD.bit.GPIO6  = 1;                 // pull-up off (output pin)

    // ePWM4: InitSysCtrl() already enabled its clock; set it again to be safe
    // and pause the time bases while configuring.
    SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 1;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC  = 0;

    EPwm4Regs.TBCTL.bit.CTRMODE   = 0;                  // up-count
    EPwm4Regs.TBCTL.bit.HSPCLKDIV = 0;                  // /1
    EPwm4Regs.TBCTL.bit.CLKDIV    = 3;                  // /8
    EPwm4Regs.TBCTL.bit.PHSEN     = 0;                  // no phase loading
    EPwm4Regs.TBCTL.bit.PRDLD     = 0;                  // TBPRD shadowed
    EPwm4Regs.TBCTL.bit.SYNCOSEL  = 3;                  // no sync output
    EPwm4Regs.TBCTL.bit.FREE_SOFT = 3;                  // free-run in emulation
    EPwm4Regs.TBPHS.half.TBPHS    = 0;
    EPwm4Regs.TBCTR               = 0;
    EPwm4Regs.TBPRD               = 10000;              // placeholder until the first note

    EPwm4Regs.CMPCTL.bit.SHDWAMODE = 0;                 // CMPA shadowed
    EPwm4Regs.CMPCTL.bit.LOADAMODE = 0;                 // load at TBCTR = 0
    EPwm4Regs.CMPA.half.CMPA       = 5000;

    EPwm4Regs.AQCTLA.bit.ZRO = 2;                       // set high at counter = 0
    EPwm4Regs.AQCTLA.bit.CAU = 1;                       // clear low at CMPA (up-count)
    EPwm4Regs.AQSFRC.bit.RLDCSF = 3;                    // software force acts immediately
    EPwm4Regs.AQCSFRC.bit.CSFA  = 1;                    // start silent (forced low)

    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;              // start the time base

    // CPU Timer 1: 1 ms tick -> INT13. Set up by hand because InitCpuTimers()
    // would also restart Timer 0, which the joystick driver uses as its clock.
    CpuTimer1Regs.TCR.bit.TSS  = 1;                     // stop
    CpuTimer1Regs.PRD.all      = TICKS_PER_MS - 1;      // 1 ms
    CpuTimer1Regs.TPR.all      = 0;                     // no prescale
    CpuTimer1Regs.TPRH.all     = 0;
    CpuTimer1Regs.TCR.bit.TRB  = 1;                     // reload counter from PRD
    CpuTimer1Regs.TCR.bit.SOFT = 1;
    CpuTimer1Regs.TCR.bit.FREE = 1;                     // keep running while halted
    CpuTimer1Regs.TCR.bit.TIE  = 1;                     // interrupt on timeout

    PieVectTable.TINT1 = &Buzzer_ISR;                   // Timer 1 vector (not PIE-routed)
    IER |= M_INT13;

    EDIS;

    CpuTimer1Regs.TCR.bit.TSS = 0;                      // start
}

//*****************************************************************************
// Public API. Interrupts are briefly off so the ISR never sees half-updated
// state. (Call these after EINT, i.e. from the app layer.)
//*****************************************************************************
void Buzzer_PlaySong(const Song *song, uint16_t loop)
{
    DINT;
    VoiceStart(&s_music, song, loop);
    EINT;
}

void Buzzer_PlaySfx(const Song *sfx)
{
    DINT;
    VoiceStart(&s_sfx, sfx, 0);
    EINT;
}

void Buzzer_Stop(void)
{
    DINT;
    s_music.active = 0;
    s_sfx.active   = 0;
    EINT;
}

void Buzzer_PauseMusic(uint16_t pause)
{
    s_musicPaused = pause;
}

uint16_t Buzzer_IsPlaying(void)
{
    return (s_music.active || s_sfx.active);
}

//*****************************************************************************
// 1 ms sequencer tick: a few counters and, at most, one frequency change.
// Timer 1 goes straight to the CPU (INT13), so there is no PIE acknowledge.
//*****************************************************************************
interrupt void Buzzer_ISR(void)
{
    uint16_t musicFreq = 0;
    uint16_t out;

    // The music keeps its place in time even while an effect covers it.
    if(!s_musicPaused) musicFreq = VoiceTick(&s_music);

    if(s_sfx.active) out = VoiceTick(&s_sfx);           // effect owns the pin
    else             out = musicFreq;

    SetOutput(out);
}
