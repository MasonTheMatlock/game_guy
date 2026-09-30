#include "drivers.h"


// Define global calibration trackers with default midpoints
volatile uint16_t center_x = 2048;
volatile uint16_t center_y = 2048;

// ---- REGISTER LEVEL HARDWARE CONFIGURATION CHANNELS ----

void Init_Joystick_ADC(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1; 
    //ADC callibration
    (*Device_cal)();                     
    
    
    AdcRegs.ADCCTL1.bit.ADCBGPWD   = 1;   // Power up Internal Bandgap
    AdcRegs.ADCCTL1.bit.ADCREFPWD  = 1;   // Power up Reference Cascades
    AdcRegs.ADCCTL1.bit.ADCPWDN    = 1;   // Power up Core Logic Circuitry
    AdcRegs.ADCCTL1.bit.ADCENABLE  = 1;   // Enable output streaming
    AdcRegs.ADCCTL1.bit.ADCREFSEL  = 0;   // Select Internal Reference engine

    DELAY_US(1000); 

    // SOC0 Configuration -> VRx on J7-63 / ADCINB7
    AdcRegs.ADCSOC0CTL.bit.CHSEL   = 15;  
    AdcRegs.ADCSOC0CTL.bit.TRIGSEL = 0;   
    AdcRegs.ADCSOC0CTL.bit.ACQPS   = 6;   

    // SOC1 Configuration -> VRy on J7-64 / ADCINB4
    AdcRegs.ADCSOC1CTL.bit.CHSEL   = 12;  
    AdcRegs.ADCSOC1CTL.bit.TRIGSEL = 0;   
    AdcRegs.ADCSOC1CTL.bit.ACQPS   = 6;   

    // Setup ADCINTFLG tracking connections
    AdcRegs.INTSEL1N2.bit.INT1SEL  = 1;   
    AdcRegs.INTSEL1N2.bit.INT1E    = 1;   
    AdcRegs.INTSEL1N2.bit.INT1CONT = 0;   
    EDIS;
}

void Init_Switch_GPIO(void)
{
    EALLOW;
    // Set Mux setting to 0
    GpioCtrlRegs.GPBMUX2.bit.GPIO55 = 0;   
    
    // Define directional parameter as INPUT mode
    GpioCtrlRegs.GPBDIR.bit.GPIO55  = 0;   
    
    // Enable internal pull-up resistor
    GpioCtrlRegs.GPBPUD.bit.GPIO55  = 0;   

    // Set digital input qualification
    GpioCtrlRegs.GPBQSEL2.bit.GPIO55 = 0;  
    EDIS;
}

void Read_Joystick(uint16_t *x_val, uint16_t *y_val, uint16_t *sw_val)
{
    AdcRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; 
    AdcRegs.ADCSOCFRC1.all = 0x0003;      

    while(AdcRegs.ADCINTFLG.bit.ADCINT1 == 0) {} 

    *x_val  = AdcResult.ADCRESULT0; 
    *y_val  = AdcResult.ADCRESULT1; 
    
    // Extract digital bit state from Bank B Data register (GPIO55)
    *sw_val = GpioDataRegs.GPBDAT.bit.GPIO55; 
}

void Calibrate_Joystick(void)
{
    uint32_t sum_x = 0; uint32_t sum_y = 0;
    uint16_t dummy_sw, tx, ty;
    int i;
    for(i = 0; i < 10; i++) {
        Read_Joystick(&tx, &ty, &dummy_sw);
        sum_x += tx; sum_y += ty;
        DELAY_US(2000);
    }
    center_x = (uint16_t)(sum_x / 10);
    center_y = (uint16_t)(sum_y / 10);
}
