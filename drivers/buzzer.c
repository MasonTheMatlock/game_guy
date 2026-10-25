//#############################################################################
// FILE:   drivers/buzzer.c
// TITLE:  Buzzer driver - implementation (see buzzer.h)
//
//   TONE + VOLUME (ePWM4A on GPIO6)
//     ePWM4 does NOT run at the note frequency. It free-runs as a fast CARRIER
//     (CARRIER_HZ, above hearing). Each carrier period an interrupt decides
//     whether the pin should be in the "on" or "off" half of the tone:
//
//         tone phase += step                (32-bit phase accumulator)
//         phase MSB = 1 -> "on" half   -> carrier pulse width = volume
//         phase MSB = 0 -> "off" half  -> output held low
//
//     So the audible waveform is always a 50% square wave at the note
//     frequency (same harmonics at every volume, so the pitch does not change),
//     and volume is the duty of the inaudible carrier inside the "on" half.
//     The buzzer's own mechanics low-pass the carrier, exactly like a class-D
//     amplifier. Same trick as putting a resistor in series, done digitally.
//
//     Carrier action qualifier: LOW at TBCTR = 0, HIGH at CMPA. High time is
//     (period - CMPA); CMPA > TBPRD never matches, so the pin just stays low.
//
//     The note edges land on carrier boundaries, so they have +-1/CARRIER_HZ of
//     jitter (25 us at 40 kHz). The pitch averages exactly right.
//
//   SONG ENGINE (CPU Timer 1, 1 ms tick)
//     Music and effects each have a "voice" that counts down the current note.
//     The effect voice owns the pin while it plays, but the music voice keeps
//     advancing underneath, so the song continues where it would have been.
//     The 1 ms tick only sets the note's phase step; the carrier ISR does the rest.
//#############################################################################

#include "drivers.h"            // device headers, CPU_RATE -> TICKS_PER_MS

//*****************************************************************************
// Clocks
//*****************************************************************************
#define TBCLK_DIV       1UL                             // CLKDIV = /1, HSPCLKDIV = /1
#define SYSCLK_HZ       (TICKS_PER_MS * 1000UL)         // TICKS_PER_MS: see joystick.h
#define TBCLK_HZ        (SYSCLK_HZ / TBCLK_DIV)

#define CARRIER_HZ      40000UL                         // above hearing; raise it if you
                                                        // can still hear a whine
#define CARRIER_PERIOD  (TBCLK_HZ / CARRIER_HZ)         // TBCLK counts per carrier period
#define CARRIER_ACTUAL  (TBCLK_HZ / CARRIER_PERIOD)     // real carrier rate after rounding

// Phase accumulator step per Hz: phase is 32 bits, one wrap = one tone period.
//   step = freq * 2^32 / CARRIER_ACTUAL
#define STEP_PER_HZ     (0xFFFFFFFFUL / CARRIER_ACTUAL)

interrupt void Buzzer_ISR(void);                        // 1 ms sequencer (Timer 1)
interrupt void Buzzer_PwmISR(void);                     // carrier tick (ePWM4, PIE 3.4)

//*****************************************************************************
// Volume: 5 levels, ~6 dB apart. The loudest level is a carrier duty of ~100%
// (a plain 50% square wave); each step down halves the carrier duty, which
// halves the amplitude the buzzer sees. Q16 fraction of the carrier period.
// Level 0 is quiet, not silent.
//*****************************************************************************
static const uint16_t s_ampQ16[BUZZER_NUM_VOLUMES] =
{
     0,      // level 0: -24 dB (6.25%)
     8192,      // level 1: -18 dB (12.5%)
    16384,      // level 2: -12 dB (25%)
    32768,      // level 3:  -6 dB (50%)
    65535       // level 4:   0 dB (~100%)
};

static uint16_t          s_cmpTab[BUZZER_NUM_VOLUMES];  // CMPA for the "on" half, per level
static volatile uint16_t s_volume = BUZZER_NUM_VOLUMES - 1;     // loudest by default
static volatile uint16_t s_cmpOn;                       // CMPA now used for the "on" half
static const    uint16_t s_cmpOff = (uint16_t)CARRIER_PERIOD;   // > TBPRD: never matches, pin low

static volatile uint32_t s_step  = 0;                   // tone phase step; 0 = silent
static uint32_t          s_phase = 0;

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
// Output: set the note frequency (0 = silent). Only the phase step changes;
// the carrier ISR turns it into pin activity. The 1 ms ISR and the carrier ISR
// never nest (INTM is set inside both), so the 32-bit write can't be torn.
//*****************************************************************************
static void SetOutput(uint16_t freq)
{
    if(freq == s_lastFreq) return;
    s_lastFreq = freq;

    s_step = (freq == 0) ? 0UL : (STEP_PER_HZ * (uint32_t)freq);
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
    uint16_t i;
    uint32_t width;

    // Per-level CMPA: high time = period - CMPA, kept to 1 .. period-1 counts so
    // the compare never lands on the TBCTR = 0 clear.
    for(i = 0; i < BUZZER_NUM_VOLUMES; i++)
    {
        width = (CARRIER_PERIOD * (uint32_t)s_ampQ16[i]) >> 16;
        if(width < 1UL)                    width = 1UL;
        if(width > CARRIER_PERIOD - 1UL)   width = CARRIER_PERIOD - 1UL;
        s_cmpTab[i] = (uint16_t)(CARRIER_PERIOD - width);
    }
    s_cmpOn = s_cmpTab[s_volume];

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
    EPwm4Regs.TBCTL.bit.CLKDIV    = 0;                  // /1
    EPwm4Regs.TBCTL.bit.PHSEN     = 0;                  // no phase loading
    EPwm4Regs.TBCTL.bit.PRDLD     = 0;                  // TBPRD shadowed
    EPwm4Regs.TBCTL.bit.SYNCOSEL  = 3;                  // no sync output
    EPwm4Regs.TBCTL.bit.FREE_SOFT = 3;                  // free-run in emulation
    EPwm4Regs.TBPHS.half.TBPHS    = 0;
    EPwm4Regs.TBCTR               = 0;
    EPwm4Regs.TBPRD               = (uint16_t)(CARRIER_PERIOD - 1UL);   // fixed carrier

    EPwm4Regs.CMPCTL.bit.SHDWAMODE = 0;                 // CMPA shadowed
    EPwm4Regs.CMPCTL.bit.LOADAMODE = 0;                 // load at TBCTR = 0
    EPwm4Regs.CMPA.half.CMPA       = s_cmpOff;          // start silent

    EPwm4Regs.AQCTLA.bit.ZRO = 1;                       // clear low at counter = 0
    EPwm4Regs.AQCTLA.bit.CAU = 2;                       // set high at CMPA (up-count)
    EPwm4Regs.AQSFRC.bit.RLDCSF = 3;
    EPwm4Regs.AQCSFRC.bit.CSFA  = 0;                    // no software force; AQ drives the pin

    // Interrupt every carrier period (at TBCTR = 0) -> PIE group 3, INT4.
    EPwm4Regs.ETSEL.bit.INTSEL = 1;                     // event: TBCTR = 0
    EPwm4Regs.ETPS.bit.INTPRD  = 1;                     // every event
    EPwm4Regs.ETSEL.bit.INTEN  = 1;
    PieVectTable.EPWM4_INT = &Buzzer_PwmISR;
    PieCtrlRegs.PIEIER3.bit.INTx4 = 1;

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
    IER |= (M_INT13 | M_INT3);                          // INT3 = ePWM group

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

void Buzzer_SetVolume(uint16_t level)
{
    if(level >= BUZZER_NUM_VOLUMES) level = BUZZER_NUM_VOLUMES - 1;
    s_volume = level;
    s_cmpOn  = s_cmpTab[level];                         // 16-bit write: atomic; the carrier
                                                        // ISR uses it from its next period
}

uint16_t Buzzer_GetVolume(void)
{
    return s_volume;
}

// Hard-silence the buzzer pin (ePWM4A forced low) / release it. Used around
// flash erase/program, when interrupts are off and the carrier ISR cannot
// update CMPA: without this the pin can be stuck on its last carrier pulse.
void Buzzer_Mute(uint16_t on)
{
    EALLOW;
    EPwm4Regs.AQCSFRC.bit.CSFA = on ? 1 : 0;            // 1 = force low, 0 = AQ drives the pin
    EDIS;
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
// 1 ms sequencer tick: a few counters and, at most, one phase-step change.
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

//*****************************************************************************
// Carrier tick (every CARRIER_PERIOD): advance the tone phase and pick the
// compare value for the NEXT carrier period (CMPA is shadowed, loads at 0).
//   on half  -> CMPA = s_cmpOn  (carrier pulse, width = volume)
//   off half -> CMPA = s_cmpOff (never matches, pin stays low)
// Keep this short: it runs CARRIER_HZ times a second.
//*****************************************************************************
interrupt void Buzzer_PwmISR(void)
{
    s_phase += s_step;

    if(s_step != 0UL && (s_phase & 0x80000000UL))
        EPwm4Regs.CMPA.half.CMPA = s_cmpOn;
    else
        EPwm4Regs.CMPA.half.CMPA = s_cmpOff;

    EPwm4Regs.ETCLR.bit.INT = 1;                        // clear the ePWM flag
    PieCtrlRegs.PIEACK.all  = PIEACK_GROUP3;            // let group 3 fire again
}
