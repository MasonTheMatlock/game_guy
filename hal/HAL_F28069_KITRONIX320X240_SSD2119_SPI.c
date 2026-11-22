//*****************************************************************************
//
// HAL_F28069_KITRONIX320X240_SSD2119_SPI.c - F2806x (TMS320F28069) HAL for
//     the Kitronix320x240x16 SSD2119 BoosterPack, 4-wire 8-bit SPI.
//     Pin mapping is for the BOOSTXL-K350QVG-S1 plugged DIRECTLY into the
//     LAUNCHXL-F28069M (BoosterPack site 1, J1/J2/J3/J4).
//
//*****************************************************************************

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"

// Reset pin lives in GPIO32-47 (GPBMUX1 / GPB registers). To move it, change
// LCD_RESET_GPIO in the .h (must stay within 32..47 for this code).
#define LCD_RESET_MASK   (1UL << (LCD_RESET_GPIO - 32))

//*****************************************************************************
//
//! Configures GPIO16/18 for SPI-A (SIMO/CLK) and the remaining LCD signals
//! as plain GPIO outputs:
//!   D/C  = GPIO22  (J1.8)
//!   CS   = GPIO50  (J2.13)
//!   RST  = LCD_RESET_GPIO (J4.32, see .h)
//!   BKLT = GPIO0   (J4.40)
//
//*****************************************************************************
void InitLCDInterfaceSPI(void)
{
    EALLOW;
    SysCtrlRegs.LOSPCP.all = 0x0000;       // LSPCLK = SYSCLKOUT/1 = 90 MHz
    SpiaRegs.SPIBRR = 0x0003; // Sets SPI clock to 22.5 MHz (assuming LSPCLK is 90 MHz)


    // GPIO16 -> SPISIMOA (J2.15), GPIO18 -> SPICLKA (J1.7), mux = 1
    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;

    // D/C: GPIO22 (J1.8), GPAMUX2
    GpioCtrlRegs.GPAMUX2.bit.GPIO22 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO22 = 1;

    // CS: GPIO50 (J2.13), GPBMUX2
    GpioCtrlRegs.GPBMUX2.bit.GPIO50 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO50 = 1;

    // RESET: J4.32 is the LaunchPad's "DAC1" net, a PWM-filter output.
    // The driving GPIO is set by LCD_RESET_GPIO (GPIO32-47 -> GPBMUX1).
    GpioCtrlRegs.GPBMUX1.all &= ~(3UL << ((LCD_RESET_GPIO - 32) * 2));
    GpioCtrlRegs.GPBDIR.all  |= LCD_RESET_MASK;

    // Backlight: GPIO0 (J4.40), GPAMUX1
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO0 = 1;

    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 1;
    EDIS;

    SpiaRegs.SPICCR.bit.SPISWRESET = 0;
    SpiaRegs.SPICCR.bit.SPICHAR = 7;       // 8-bit characters
    SpiaRegs.SPICCR.bit.SPILBK = 0;
    SpiaRegs.SPICCR.bit.CLKPOLARITY = 1;

    SpiaRegs.SPICTL.bit.CLK_PHASE = 0;
    SpiaRegs.SPICTL.bit.MASTER_SLAVE = 1;
    SpiaRegs.SPICTL.bit.TALK = 1;
    SpiaRegs.SPICTL.bit.SPIINTENA = 0;

    SpiaRegs.SPIBRR = HAL_LCD_SPIBRR;
    SpiaRegs.SPIPRI.bit.FREE = 1;

    SpiaRegs.SPIFFTX.all = 0xE040;
    SpiaRegs.SPIFFRX.all = 0x2044;
    SpiaRegs.SPIFFCT.all = 0x0;

    SpiaRegs.SPICCR.bit.SPISWRESET = 1;
}

//*****************************************************************************
//
//! Initializes the display HAL: GPIO/SPI peripheral, then the reset sequence.
//
//*****************************************************************************
void HAL_LCD_initLCD(void)
{
    InitLCDInterfaceSPI();

    GpioDataRegs.GPBSET.bit.GPIO50 = 1;    // CS idle high
    GpioDataRegs.GPASET.bit.GPIO22 = 1;    // D/C idle high (data)
    GpioDataRegs.GPBCLEAR.all = LCD_RESET_MASK;   // hold LCD in reset

    GpioDataRegs.GPASET.bit.GPIO0 = 1;     // backlight on

    DELAY_US(1000L);                       // reset low for 1ms

    GpioDataRegs.GPBSET.all = LCD_RESET_MASK;     // release reset
    DELAY_US(1000L);
}

//*****************************************************************************
//
// Streaming SPI. Bytes are queued into the 4-deep TX FIFO without waiting for
// each one to finish, so the wire runs back to back at the full 22.5 MHz.
// Every byte sent also clocks one byte into the RX FIFO; s_inFlight counts the
// bytes sent but not yet echoed back. HAL_LCD_flush() waits until that count is
// zero, i.e. the last bit has left the shift register. D/C and CS may only
// change after a flush (the LCD samples D/C on the last bit of each byte).
// All of it runs from RAM (.TI.ramfunc): no flash wait states per byte.
//
//*****************************************************************************
#pragma CODE_SECTION(HAL_LCD_flush,          ".TI.ramfunc")
#pragma CODE_SECTION(HAL_LCD_writeCommand,   ".TI.ramfunc")
#pragma CODE_SECTION(HAL_LCD_writeData,      ".TI.ramfunc")
#pragma CODE_SECTION(HAL_LCD_writeDataRepeat, ".TI.ramfunc")
#pragma CODE_SECTION(HAL_LCD_selectLCD,      ".TI.ramfunc")
#pragma CODE_SECTION(HAL_LCD_deselectLCD,    ".TI.ramfunc")

static Uint16 s_inFlight;               // bytes queued but not yet fully shifted out

// Collect the echo bytes that have arrived (keeps the RX FIFO from overflowing).
static inline void HAL_LCD_spiReap(void)
{
    while(SpiaRegs.SPIFFRX.bit.RXFFST != 0)
    {
        (void)SpiaRegs.SPIRXBUF;
        s_inFlight--;
    }
}

static inline void HAL_LCD_spiPut(Uint16 byte)
{
    while(SpiaRegs.SPIFFTX.bit.TXFFST >= 4)     // FIFO full: let the wire catch up
    {
        HAL_LCD_spiReap();
    }
    SpiaRegs.SPITXBUF = (Uint16)(byte << 8);    // 8-bit characters are left-justified
    s_inFlight++;
    HAL_LCD_spiReap();
}

// Wait until every queued byte is out on the wire. The loop limit only guards
// against a lost echo byte (it cannot normally happen): ~50x a byte time.
void HAL_LCD_flush(void)
{
    Uint16 guard = 0;

    while(s_inFlight != 0 && guard < 2000)
    {
        if(SpiaRegs.SPIFFRX.bit.RXFFST != 0) { (void)SpiaRegs.SPIRXBUF; s_inFlight--; guard = 0; }
        else guard++;
    }
    s_inFlight = 0;
}

void HAL_LCD_writeCommand(Uint16 command)
{
    HAL_LCD_flush();                            // earlier data must finish with D/C high
    GpioDataRegs.GPACLEAR.bit.GPIO22 = 1;       // D/C low = command
    HAL_LCD_spiPut(command & 0xFF);
    HAL_LCD_flush();
    GpioDataRegs.GPASET.bit.GPIO22 = 1;         // D/C high = data
}

void HAL_LCD_writeData(Uint16 data)
{
    HAL_LCD_spiPut((data >> 8) & 0xFF);
    HAL_LCD_spiPut(data & 0xFF);
}

// The same 16-bit value count times (rectangle fills): the hot loop of the display.
void HAL_LCD_writeDataRepeat(Uint16 data, Uint32 count)
{
    Uint16 hi = (data >> 8) & 0xFF, lo = data & 0xFF;

    while(count--)
    {
        HAL_LCD_spiPut(hi);
        HAL_LCD_spiPut(lo);
    }
}

void HAL_LCD_selectLCD(void)
{
    GpioDataRegs.GPBCLEAR.bit.GPIO50 = 1;
}

void HAL_LCD_deselectLCD(void)
{
    HAL_LCD_flush();                            // never cut CS while bits are on the wire
    GpioDataRegs.GPBSET.bit.GPIO50 = 1;
}

void HAL_LCD_delay30ms(void)
{
    DELAY_US(30000L);
}
