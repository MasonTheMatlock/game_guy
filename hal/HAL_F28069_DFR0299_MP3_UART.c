//*****************************************************************************
//
// HAL_F28069_DFR0299_MP3_UART.c - SCI-B UART HAL for the DFR0299 / DFPlayer
//     Mini on the LAUNCHXL-F28069M. See the .h for wiring.
//
//*****************************************************************************

#include "F2806x_Device.h"
#include "F2806x_Examples.h"
#include "HAL_F28069_DFR0299_MP3_UART.h"

static Uint16 g_rxErrors = 0;          // times an RX error / overflow was cleared
static Uint16 g_brr = 0;               // BRR value actually programmed

Uint16 HAL_MP3_getRxErrors(void)
{
    return g_rxErrors;
}

Uint16 HAL_MP3_getBrr(void)
{
    return g_brr;
}

// SYSCLKOUT in Hz. If HAL_MP3_OSCCLK_HZ is defined in the .h it is computed
// from the live PLL registers, otherwise the fixed constant is used.
static Uint32 HAL_MP3_sysclk(void)
{
#ifdef HAL_MP3_OSCCLK_HZ
    Uint32 pll = SysCtrlRegs.PLLCR.bit.DIV;
    Uint16 divsel = SysCtrlRegs.PLLSTS.bit.DIVSEL;
    Uint32 div = (divsel == 3) ? 1UL : ((divsel == 2) ? 2UL : 4UL);
    Uint32 clkin = (pll == 0) ? HAL_MP3_OSCCLK_HZ
                              : (HAL_MP3_OSCCLK_HZ * pll);
    return clkin / div;
#else
    return HAL_MP3_SYSCLKOUT_FREQUENCY;
#endif
}

void HAL_MP3_init(void)
{
    Uint32 lspclk;
    Uint32 sysclk = HAL_MP3_sysclk();
    Uint32 brr;

    // LSPCLK = SYSCLKOUT / (2 * LOSPCP), or SYSCLKOUT when LOSPCP == 0.
    if(SysCtrlRegs.LOSPCP.bit.LSPCLK == 0)
    {
        lspclk = sysclk;
    }
    else
    {
        lspclk = sysclk / (2UL * SysCtrlRegs.LOSPCP.bit.LSPCLK);
    }

    // SCI baud = LSPCLK / ((BRR + 1) * 8), rounded to nearest.
    brr = ((lspclk + (4UL * HAL_MP3_BAUD)) / (8UL * HAL_MP3_BAUD)) - 1UL;
    g_brr = (Uint16)brr;

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.SCIBENCLK = 1;
    __asm(" NOP");
    __asm(" NOP");

    // GPIO23 (J5.49) = SCIRXDB (mux 3): pull-up, async input
    GpioCtrlRegs.GPAPUD.bit.GPIO23 = 0;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO23 = 3;
    GpioCtrlRegs.GPAMUX2.bit.GPIO23 = 3;

    // GPIO14 (J5.47) = SCITXDB (mux 2)
    GpioCtrlRegs.GPAPUD.bit.GPIO14 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO14 = 2;
    EDIS;

    ScibRegs.SCICCR.all = 0x0007;          // 1 stop bit, no parity, 8 data bits
    ScibRegs.SCICTL1.all = 0x0003;         // TX/RX enabled, SW reset held
    ScibRegs.SCICTL2.all = 0x0000;         // no interrupts
    ScibRegs.SCIHBAUD = (Uint16)((brr >> 8) & 0xFF);
    ScibRegs.SCILBAUD = (Uint16)(brr & 0xFF);

    ScibRegs.SCIFFTX.all = 0xE040;         // FIFO enabled, TX FIFO reset
    ScibRegs.SCIFFRX.all = 0x2044;         // RX FIFO reset, no interrupts
    ScibRegs.SCIFFCT.all = 0x0000;

    ScibRegs.SCICTL1.all = 0x0023;         // release SW reset
}

void HAL_MP3_sendByte(Uint16 byte)
{
    Uint32 timeout = 100000UL;
    while((ScibRegs.SCIFFTX.bit.TXFFST >= 16) && (--timeout > 0))
    {
        ;
    }
    ScibRegs.SCITXBUF = byte & 0xFF;
}

Uint16 HAL_MP3_rxCount(void)
{
    // FIX: do NOT throw away the FIFO on an error. Count it, clear the error
    // flags, and still report whatever bytes are waiting. Otherwise one
    // framing error (e.g. wrong baud) hides every byte from the parser.
    if(ScibRegs.SCIRXST.bit.RXERROR)
    {
        g_rxErrors++;
        ScibRegs.SCICTL1.all = 0x0003;     // pulse SW reset to clear FE/OE/BRKDT
        ScibRegs.SCICTL1.all = 0x0023;
    }
    if(ScibRegs.SCIFFRX.bit.RXFFOVF)
    {
        g_rxErrors++;
        ScibRegs.SCIFFRX.all = 0x6044;     // RXFFOVRCLR=1 (bit 14), keep FIFO enabled
        ScibRegs.SCIFFRX.all = 0x2044;
    }
    return ScibRegs.SCIFFRX.bit.RXFFST;
}

Uint16 HAL_MP3_readByte(void)
{
    return ScibRegs.SCIRXBUF.all & 0xFF;
}

void HAL_MP3_flushRx(void)
{
    // Full-register writes preserve TXENA (bit 1) and RXENA (bit 0)
    ScibRegs.SCICTL1.all = 0x0003;         // Hold SW reset, keep TX/RX enabled
    ScibRegs.SCICTL1.all = 0x0023;         // Release SW reset, keep TX/RX enabled

    // FIX: bit 14 (RXFFOVRCLR) must be written 1 to clear the overflow flag.
    // The old 0x0044 never cleared it, so after one overflow the flag stuck.
    ScibRegs.SCIFFRX.all = 0x4044;         // clear overflow, FIFO held in reset
    ScibRegs.SCIFFRX.all = 0x2044;         // release RX FIFO reset
}

void HAL_MP3_delayMs(Uint16 ms)
{
    while(ms--)
    {
        DELAY_US(1000L);
    }
}

Uint16 HAL_MP3_loopbackTest(void)
{
    Uint16 ok = 0;

    HAL_MP3_flushRx();
    HAL_MP3_sendByte(0x55);
    HAL_MP3_delayMs(10);

    if(HAL_MP3_rxCount() > 0)
    {
        ok = (HAL_MP3_readByte() == 0x55) ? 1 : 0;
    }
    return ok;
}

// Scope / logic-analyzer test: sends 0x55 forever on J5.47. A correct 9600
// baud line shows a square wave with 104 us per bit (period ~208 us).
// Never returns.
void HAL_MP3_txTest(void)
{
    for(;;)
    {
        HAL_MP3_sendByte(0x55);
    }
}
