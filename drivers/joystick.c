#include "drivers.h"

#define ADC_TIMEOUT   20000U      // safety limit for the conversion wait loop
#define CAL_SAMPLES   16
#define CAL_MIN       1500        // calibration sanity window
#define CAL_MAX       2600

// Calibrated center (defined here, declared extern in drivers.h)
uint16_t center_x = 2048;
uint16_t center_y = 2048;

Joy g_joy;

static int16_t  g_btnState;          // debounced button state (1 = pressed)
static int16_t  g_btnLongFired;
static uint32_t g_btnChangeTick;

// Last good reading, returned if a conversion ever times out
static uint16_t g_lastX = 2048;
static uint16_t g_lastY = 2048;

// ---- REGISTER LEVEL HARDWARE CONFIGURATION ----------------------------------

void Init_Joystick_ADC(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
    (*Device_cal)();

    // Grouped sub-bitfields inside the main ADCCTL1 register structure
    AdcRegs.ADCCTL1.bit.ADCBGPWD   = 1;   // Power up Internal Bandgap
    AdcRegs.ADCCTL1.bit.ADCREFPWD  = 1;   // Power up Reference Cascades
    AdcRegs.ADCCTL1.bit.ADCPWDN    = 1;   // Power up Core Logic Circuitry
    AdcRegs.ADCCTL1.bit.ADCENABLE  = 1;   // Enable output streaming
    AdcRegs.ADCCTL1.bit.ADCREFSEL  = 0;   // Select Internal Reference engine

    DELAY_US(1000);

    // ACQPS = sample window in ADC clocks (+1). A joystick pot is a fairly
    // high source impedance, so 6 (the TI example value) is short; a longer
    // window gives cleaner, less cross-coupled readings. Range is 6..63.

    // SOC0 Configuration -> VRx on J7-63 / ADCINB7
    AdcRegs.ADCSOC0CTL.bit.CHSEL   = 15;
    AdcRegs.ADCSOC0CTL.bit.TRIGSEL = 0;   // software trigger
    AdcRegs.ADCSOC0CTL.bit.ACQPS   = 32;

    // SOC1 Configuration -> VRy on J7-64 / ADCINB4
    AdcRegs.ADCSOC1CTL.bit.CHSEL   = 12;
    AdcRegs.ADCSOC1CTL.bit.TRIGSEL = 0;
    AdcRegs.ADCSOC1CTL.bit.ACQPS   = 32;

    // ADCINT1 fires when SOC1 (the last one forced) finishes
    AdcRegs.INTSEL1N2.bit.INT1SEL  = 1;
    AdcRegs.INTSEL1N2.bit.INT1E    = 1;
    AdcRegs.INTSEL1N2.bit.INT1CONT = 0;
    EDIS;
}

void Init_Switch_GPIO(void)
{
    EALLOW;
    GpioCtrlRegs.GPBMUX2.bit.GPIO55  = 0;   // standard digital GPIO
    GpioCtrlRegs.GPBDIR.bit.GPIO55   = 0;   // input
    GpioCtrlRegs.GPBPUD.bit.GPIO55   = 0;   // internal pull-up enabled (button = active low)
    GpioCtrlRegs.GPBQSEL2.bit.GPIO55 = 0;   // synchronous sampling
    EDIS;
}

// Free-running 32-bit down-counter at SYSCLK, used for timing (no interrupts)
void Timer_Init(void)
{
    CpuTimer0Regs.TCR.bit.TSS = 1;          // stop
    CpuTimer0Regs.PRD.all     = 0xFFFFFFFFUL;
    CpuTimer0Regs.TPR.all     = 0;          // no prescale
    CpuTimer0Regs.TPRH.all    = 0;
    CpuTimer0Regs.TCR.bit.TRB = 1;          // reload counter from PRD
    CpuTimer0Regs.TCR.bit.TIE = 0;          // no interrupt
    CpuTimer0Regs.TCR.bit.TSS = 0;          // start
}

uint32_t TimerNow(void)
{
    return CpuTimer0Regs.TIM.all;
}

uint32_t MsSince(uint32_t startTick)
{
    return (startTick - CpuTimer0Regs.TIM.all) / TICKS_PER_MS;   // counts down
}

// ---- RAW READ AND CALIBRATION -----------------------------------------------

// Reads both axes JOY_SAMPLES times and averages. The switch is returned raw
// (1 = released, 0 = pressed). If the ADC ever fails to finish, the last good
// values are returned instead of hanging forever.
void Read_Joystick(uint16_t *x_val, uint16_t *y_val, uint16_t *sw_val)
{
    uint32_t sumX = 0, sumY = 0;
    uint16_t guard;
    int16_t  n, ok = 1;

    for(n = 0; n < JOY_SAMPLES; n++)
    {
        AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1;
        AdcRegs.ADCSOCFRC1.all = 0x0003;      // force SOC0 and SOC1

        guard = 0;
        while(AdcRegs.ADCINTFLG.bit.ADCINT1 == 0)
        {
            if(++guard >= ADC_TIMEOUT) break;
        }
        if(AdcRegs.ADCINTFLG.bit.ADCINT1 == 0) { ok = 0; break; }

        sumX += AdcResult.ADCRESULT0;
        sumY += AdcResult.ADCRESULT1;
    }

    if(ok)
    {
        g_lastX = (uint16_t)(sumX / JOY_SAMPLES);
        g_lastY = (uint16_t)(sumY / JOY_SAMPLES);
    }

    *x_val  = g_lastX;
    *y_val  = g_lastY;
    *sw_val = GpioDataRegs.GPBDAT.bit.GPIO55;
}

// Averages the resting position. Returns 1 if plausible, 0 if it fell back to
// 2048 (stick was pushed during power-up, or VRx/VRy are not wired correctly).
int16_t Calibrate_Joystick(void)
{
    uint32_t sumX = 0, sumY = 0;
    uint16_t sw, tx, ty, cx, cy;
    int16_t  i;

    Read_Joystick(&tx, &ty, &sw);             // discard the first conversion

    for(i = 0; i < CAL_SAMPLES; i++)
    {
        Read_Joystick(&tx, &ty, &sw);
        sumX += tx;
        sumY += ty;
        DELAY_US(2000);
    }

    cx = (uint16_t)(sumX / CAL_SAMPLES);
    cy = (uint16_t)(sumY / CAL_SAMPLES);

    if(cx < CAL_MIN || cx > CAL_MAX || cy < CAL_MIN || cy > CAL_MAX)
    {
        center_x = 2048;
        center_y = 2048;
        return 0;
    }

    center_x = cx;
    center_y = cy;
    return 1;
}

int16_t Joystick_Init(void)
{
    int16_t ok;

    Init_Switch_GPIO();
    Init_Joystick_ADC();
    Timer_Init();

    ok = Calibrate_Joystick();

    // Start from the real button state so a button held at boot causes no
    // phantom press or long-press
    g_btnState      = (GpioDataRegs.GPBDAT.bit.GPIO55 == 0) ? 1 : 0;
    g_btnLongFired  = 1;
    g_btnChangeTick = TimerNow();

    return ok;
}

// ---- PROCESSED INPUT --------------------------------------------------------

// -100..100 after removing the deadzone; reaches 100 at JOY_FULLSCALE
static int16_t JoyScale(int16_t v)
{
    int16_t a = (v < 0) ? -v : v;
    int32_t s;

    if(a <= JOY_DEADZONE) return 0;

    s = ((int32_t)(a - JOY_DEADZONE) * 100L) / (JOY_FULLSCALE - JOY_DEADZONE);
    if(s > 100) s = 100;

    return (v < 0) ? -(int16_t)s : (int16_t)s;
}

// Digital direction with hysteresis: turns on past ENGAGE, off below RELEASE
static int16_t JoyDir(int16_t v, int16_t prev)
{
    if(prev > 0)
    {
        if(v >  JOY_DIR_RELEASE) return 1;
    }
    else if(prev < 0)
    {
        if(v < -JOY_DIR_RELEASE) return -1;
    }

    if(v >  JOY_DIR_ENGAGE) return 1;
    if(v < -JOY_DIR_ENGAGE) return -1;
    return 0;
}

void Joy_Update(void)
{
    uint16_t rx, ry, rsw;
    int16_t  vx, vy, sw;
    int16_t  prevDirX = g_joy.dirX;
    int16_t  prevDirY = g_joy.dirY;

    Read_Joystick(&rx, &ry, &rsw);

    vx = (int16_t)rx - (int16_t)center_x;
    vy = (int16_t)ry - (int16_t)center_y;

    g_joy.dx = JoyScale(vx);
    g_joy.dy = JoyScale(vy);

    g_joy.dirX = JoyDir(vx, prevDirX);
    g_joy.dirY = JoyDir(vy, prevDirY);

    g_joy.edgeX = (g_joy.dirX != 0 && g_joy.dirX != prevDirX) ? g_joy.dirX : 0;
    g_joy.edgeY = (g_joy.dirY != 0 && g_joy.dirY != prevDirY) ? g_joy.dirY : 0;

    // Button (active low) with time-based debounce
    sw = (rsw == 0) ? 1 : 0;
    g_joy.btnPressed = 0;
    g_joy.btnLong    = 0;

    if(sw != g_btnState && MsSince(g_btnChangeTick) >= BTN_DEBOUNCE_MS)
    {
        g_btnState      = sw;
        g_btnChangeTick = TimerNow();
        if(sw)
        {
            g_joy.btnPressed = 1;
            g_btnLongFired   = 0;
        }
    }
    g_joy.btn = g_btnState;

    if(g_btnState && !g_btnLongFired && MsSince(g_btnChangeTick) >= LONG_PRESS_MS)
    {
        g_joy.btnLong  = 1;
        g_btnLongFired = 1;
    }
}

void Joy_WaitRelease(void)
{
    do
    {
        Joy_Update();
        DELAY_US(5000);
    } while(g_joy.btn);

    Joy_Update();
}
