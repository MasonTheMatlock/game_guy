//*****************************************************************************
//
// HAL_F28069_KITRONIX320X240_SSD2119_SPI.c - F2806x (TMS320F28069) HAL for
//     the Kitronix320x240x16 SSD2119 BoosterPack, 4-wire 8-bit SPI.
//
//*****************************************************************************

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_KITRONIX320X240_SSD2119_SPI.h"

//*****************************************************************************
//
//! Configures GPIO16/18 for SPI-A (SIMO/CLK) and GPIO17/19/44/3 as plain
//! GPIO outputs for D/C, CS, RESET and backlight enable.
//
//*****************************************************************************
void InitLCDInterfaceSPI(void)
{
    EALLOW;

    //
    // GPIO16 -> SPISIMOA, GPIO18 -> SPICLKA (mux = 1)
    //
    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO16 = 0;   // enable pull-up (TI default for SPI pins)
    GpioCtrlRegs.GPAPUD.bit.GPIO18 = 0;

    //
    // GPIO17 (D/C) and GPIO19 (CS) stay as GPIO -- driven manually so CS can
    // be held low across multi-byte commands/data and D/C toggled between
    // them, which the SSD2119 4-wire interface requires.
    //
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 0;
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO17 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO19 = 1;

    //
    // GPIO3 (backlight enable) - GPAMUX1
    //
    GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO3 = 1;

    //
    // GPIO44 (LCD reset) is on GPIO32-63, i.e. the GPB registers.
    //
    GpioCtrlRegs.GPBMUX1.bit.GPIO44 = 0;
    GpioCtrlRegs.GPBDIR.bit.GPIO44 = 1;

    EDIS;

    //
    // Enable the clock to the SPI-A peripheral.
    //
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 1;
    EDIS;

    //
    // Configure SPI-A as an 8-bit master.
    //
    // NOTE ON SPI MODE: the reference MSP430 HAL configured the USCI in
    // "data changes on first edge, captured on next" phase with the clock
    // idling high (CPOL = 1). The closest equivalent on the F2806x SPI
    // module is CLOCK POLARITY = 1 (SPICCR bit 6) with CLOCK PHASE = 0
    // (SPICTL bit 5). If the display does not respond, the SSD2119 is
    // generally forgiving about SPI mode -- try flipping SPICTL.bit.CLK_PHASE
    // to 1 as the next thing to test on a scope/logic analyzer.
    //
    SpiaRegs.SPICCR.bit.SPISWRESET = 0;   // hold SPI in reset while configuring
    SpiaRegs.SPICCR.bit.SPICHAR = 7;      // 8-bit character length (7 = 8 bits)
    SpiaRegs.SPICCR.bit.SPILBK = 0;       // no loopback
    SpiaRegs.SPICCR.bit.CLKPOLARITY = 1;

    SpiaRegs.SPICTL.bit.CLK_PHASE = 0;
    SpiaRegs.SPICTL.bit.MASTER_SLAVE = 1; // master mode
    SpiaRegs.SPICTL.bit.TALK = 1;         // enable transmitter
    SpiaRegs.SPICTL.bit.SPIINTENA = 0;    // polled, no interrupt

    SpiaRegs.SPIBRR = HAL_LCD_SPIBRR;

    SpiaRegs.SPIPRI.bit.FREE = 1;         // free-run in emulation halt

    SpiaRegs.SPIFFTX.all = 0xE040;   // SPIRST=1, SPIFFENA=1, TXFIFO=1
    SpiaRegs.SPIFFRX.all = 0x2044;   // RXFIFORESET=1, clear RX overflow/int flags
    SpiaRegs.SPIFFCT.all = 0x0;      // no inter-word FIFO transfer delay

    SpiaRegs.SPICCR.bit.SPISWRESET = 1;   // take SPI out of reset, start running
}

//*****************************************************************************
//
//! Initializes the display HAL: GPIO/SPI peripheral, then drives the panel
//! through its reset sequence. Mirrors HAL_LCD_initLCD() from the MSP430
//! reference HAL.
//
//*****************************************************************************
void HAL_LCD_initLCD(void)
{
    InitLCDInterfaceSPI();

    //
    // Default/idle state of the control lines: CS high (deselected),
    // D/C high (data), reset held low (asserted).
    //
    GpioDataRegs.GPASET.bit.GPIO19 = 1;    // CS idle high
    GpioDataRegs.GPASET.bit.GPIO17 = 1;    // D/C idle high (data)
    GpioDataRegs.GPBCLEAR.bit.GPIO44 = 1;  // hold LCD in reset

    //
    // Turn the backlight on.
    //
    GpioDataRegs.GPASET.bit.GPIO3 = 1;

    //
    // Hold reset low for 1ms.
    //
    DELAY_US(1000L);

    //
    // Release reset and wait for the panel to come out of it.
    //
    GpioDataRegs.GPBSET.bit.GPIO44 = 1;
    DELAY_US(1000L);
}

//*****************************************************************************
//
//! Waits for the current SPI-A transfer to complete (character in the
//! receive buffer means the transmit shift is done, since SPI is full
//! duplex on this part).
//
//*****************************************************************************
static inline void HAL_LCD_spiWaitDone(void)
{
    while(SpiaRegs.SPIFFRX.bit.RXFFST == 0)
    {
        ;
    }
}

static inline void HAL_LCD_spiSendByte(Uint16 byte)
{
    //
    // SPI-A shifts out from the MSBs of the 16-bit TX buffer, so an 8-bit
    // character must be left-justified.
    //
    SpiaRegs.SPITXBUF = (Uint16)(byte << 8);
    HAL_LCD_spiWaitDone();
    (void)SpiaRegs.SPIRXBUF;   // clear RX/INT_FLAG by reading
}

//*****************************************************************************
//
//! Writes a command byte to the SSD2119: D/C low, one byte out over SPI.
//
//*****************************************************************************
void HAL_LCD_writeCommand(Uint16 command)
{
    GpioDataRegs.GPACLEAR.bit.GPIO17 = 1;  // D/C low = command
    HAL_LCD_spiSendByte(command & 0xFF);
    GpioDataRegs.GPASET.bit.GPIO17 = 1;    // D/C high = data (default)
}

//*****************************************************************************
//
//! Writes a 16-bit data word to the SSD2119 as two 8-bit SPI transfers,
//! high byte first, matching the reference HAL's behaviour.
//
//*****************************************************************************
void HAL_LCD_writeData(Uint16 data)
{
    HAL_LCD_spiSendByte((Uint16)(data >> 8) & 0xFF);
    HAL_LCD_spiSendByte((Uint16)data & 0xFF);
}

//*****************************************************************************
//
//! Asserts (drives low) the LCD chip-select line.
//
//*****************************************************************************
void HAL_LCD_selectLCD(void)
{
    GpioDataRegs.GPACLEAR.bit.GPIO19 = 1;
}

//*****************************************************************************
//
//! Deasserts (drives high) the LCD chip-select line.
//
//*****************************************************************************
void HAL_LCD_deselectLCD(void)
{
    GpioDataRegs.GPASET.bit.GPIO19 = 1;
}

//*****************************************************************************
//
//! ~30ms delay, used once during panel init after leaving sleep mode.
//
//*****************************************************************************
void HAL_LCD_delay30ms(void)
{
    DELAY_US(30000L);
}
