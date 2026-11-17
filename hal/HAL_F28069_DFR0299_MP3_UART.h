//*****************************************************************************
//
// HAL_F28069_DFR0299_MP3_UART.h - UART HAL for the DFRobot DFR0299
//     (DFPlayer Mini) MP3 module on the LAUNCHXL-F28069M, using SCI-B.
//
//   Module pin      LaunchPad pin            Notes
//   -----------     ---------------------    --------------------------------
//   VCC             5V (or 3.3V-5V)          5V gives the cleanest audio/power
//   GND             GND                      MUST share ground with LaunchPad
//   RX  (module)    J5.47 = GPIO14 (SCITXDB, mux 2)   1k series resistor
//   TX  (module)    J5.49 = GPIO23 (SCIRXDB, mux 3)   module TX is 3.3V, safe
//   SPK1 / SPK2     speaker (<3W) or DAC_L/R to an amplifier
//
// SD card: FAT32, files in an "MP3" folder named 0001.mp3, 0002.mp3, ...
//
//*****************************************************************************

#ifndef __HAL_F28069_DFR0299_MP3_UART_H__
#define __HAL_F28069_DFR0299_MP3_UART_H__

#include "F2806x_Device.h"

#ifdef __cplusplus
extern "C" {
#endif

// SYSCLKOUT as configured by InitSysCtrl() (Hz). THIS MUST BE THE REAL VALUE:
// if it is wrong, the baud rate is wrong and the module ignores every frame.
#define HAL_MP3_SYSCLKOUT_FREQUENCY   90000000UL

// OPTIONAL: define your board's oscillator (Hz) to compute SYSCLKOUT from the
// PLL registers instead of trusting the constant above. The LAUNCHXL-F28069M
// has a 20 MHz crystal, a controlCARD has 10 MHz. Uncomment ONE line:
// #define HAL_MP3_OSCCLK_HZ          20000000UL
// #define HAL_MP3_OSCCLK_HZ          10000000UL

// DFPlayer Mini is fixed at 9600 baud, 8N1.
#define HAL_MP3_BAUD                  9600UL

#define MP3_TX_GPIO     14      // J5.47 GPIO14 -> SCITXDB (mux 2) -> module RX
#define MP3_RX_GPIO     23      // J5.49 GPIO23 <- SCIRXDB (mux 3) <- module TX

// Configure SCI-B (clock, pins, baud, FIFOs). Call after InitSysCtrl().
// LSPCLK is read from SysCtrlRegs.LOSPCP at call time, so the LCD HAL's
// LOSPCP change is handled as long as it happens BEFORE this call.
extern void HAL_MP3_init(void);

// Blocking send of one byte.
extern void HAL_MP3_sendByte(Uint16 byte);

// Number of bytes waiting in the RX FIFO (0..16). Clears error flags.
extern Uint16 HAL_MP3_rxCount(void);

// Pop one byte from the RX FIFO (only call when HAL_MP3_rxCount() > 0).
extern Uint16 HAL_MP3_readByte(void);

// Drop everything in the RX FIFO and clear any RX error / overflow.
extern void HAL_MP3_flushRx(void);

extern void HAL_MP3_delayMs(Uint16 ms);

// How many times an RX framing/overrun error or FIFO overflow was cleared.
// Nonzero with garbage bytes means the baud rate is wrong.
extern Uint16 HAL_MP3_getRxErrors(void);

// BRR value programmed into SCIHBAUD/SCILBAUD (for on-screen debugging).
// Expected 1171 (0x0493) for LSPCLK = 90 MHz, 9600 baud.
extern Uint16 HAL_MP3_getBrr(void);

// Wiring self-test. Unplug both module wires, jumper J5.47 to J5.49, then
// call this after HAL_MP3_init(). Returns 1 if a byte sent on TX comes back
// on RX, 0 otherwise.
extern Uint16 HAL_MP3_loopbackTest(void);

// Sends 0x55 forever for scope/logic-analyzer baud verification (104 us/bit).
extern void HAL_MP3_txTest(void);

#ifdef __cplusplus
}
#endif

#endif // __HAL_F28069_DFR0299_MP3_UART_H__
