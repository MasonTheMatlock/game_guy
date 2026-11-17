//*****************************************************************************
//
// mp3.c - Driver for the DFR0299 / DFPlayer Mini MP3 module.
//
// Frame format (10 bytes, 9600 8N1):
//   7E FF 06 CMD FB P1 P2 CSUM_H CSUM_L EF
//   FB = 1 asks the module for an ACK, 0 = no ack
//   CSUM = 0x10000 - (FF + 06 + CMD + FB + P1 + P2)
//
//*****************************************************************************

#include "F2806x_Device.h"
#include "HAL_F28069_DFR0299_MP3_UART.h"
#include "mp3.h"

// Module commands
#define CMD_NEXT            0x01
#define CMD_PREVIOUS        0x02
#define CMD_VOL_UP          0x04
#define CMD_VOL_DOWN        0x05
#define CMD_SET_VOLUME      0x06
#define CMD_SET_EQ          0x07
#define CMD_LOOP_TRACK      0x08
#define CMD_SELECT_DEVICE   0x09
#define CMD_RESET           0x0C
#define CMD_RESUME          0x0D
#define CMD_PAUSE           0x0E
#define CMD_PLAY_FOLDER     0x0F
#define CMD_LOOP_ALL        0x11
#define CMD_PLAY_MP3_FOLDER 0x12
#define CMD_STOP            0x16
#define CMD_RANDOM_ALL      0x18

// Messages the module sends on its own
#define MSG_CARD_INSERTED   0x3A
#define MSG_CARD_REMOVED    0x3B
#define MSG_TRACK_DONE_USB  0x3C
#define MSG_TRACK_DONE_TF   0x3D
#define MSG_INIT_DONE       0x3F
#define MSG_ERROR           0x40
#define MSG_ACK             0x41

#define DEVICE_TF_CARD      2

// Time to wait between commands. The module needs a short gap or it drops
// the next frame; 30 ms is plenty for everything except reset/device select.
#define MP3_CMD_GAP_MS      30

// Init options. Both default to 0 because many DFPlayer clones play fine
// without them and some stop responding after a reset or device select.
#define MP3_SEND_RESET      0
#define MP3_SELECT_TF       0

// Receive parser state
#define RX_START    0
#define RX_BODY     1

static Uint16 g_rxState = RX_START;
static Uint16 g_rxBuf[10];
static Uint16 g_rxIdx = 0;

static Uint16 g_volume = 0;
static Uint16 g_cardOnline = 0;
static Uint16 g_initDone = 0;
static Uint16 g_trackDone = 0;
static Uint16 g_error = MP3_ERR_NONE;
static Uint16 g_gotAnything = 0;
static Uint16 g_rxBytes = 0;       // raw bytes seen (even if not a valid frame)
static Uint16 g_lastCmd = 0;       // last valid frame from the module
static Uint16 g_lastParam = 0;

void MP3_sendCommand(Uint16 cmd, Uint16 param1, Uint16 param2)
{
    Uint16 sum;
    Uint16 csum;

    sum = (Uint16)(0xFF + 0x06 + cmd + 0x01 + param1 + param2);
    csum = (Uint16)(0 - sum);

    HAL_MP3_sendByte(0x7E);
    HAL_MP3_sendByte(0xFF);
    HAL_MP3_sendByte(0x06);
    HAL_MP3_sendByte(cmd);
    HAL_MP3_sendByte(0x01);                // ACK requested
    HAL_MP3_sendByte(param1);
    HAL_MP3_sendByte(param2);
    HAL_MP3_sendByte((csum >> 8) & 0xFF);
    HAL_MP3_sendByte(csum & 0xFF);
    HAL_MP3_sendByte(0xEF);

    HAL_MP3_delayMs(MP3_CMD_GAP_MS);
}

static void MP3_handleFrame(void)
{
    Uint16 cmd = g_rxBuf[3];
    Uint16 p2  = g_rxBuf[6];

    g_gotAnything = 1;
    g_lastCmd = cmd;
    g_lastParam = (Uint16)((g_rxBuf[5] << 8) | g_rxBuf[6]);

    switch(cmd)
    {
        case MSG_CARD_INSERTED:
            g_cardOnline = 1;
            break;
        case MSG_CARD_REMOVED:
            g_cardOnline = 0;
            break;
        case MSG_TRACK_DONE_TF:
        case MSG_TRACK_DONE_USB:
            g_trackDone = 1;
            break;
        case MSG_INIT_DONE:
            g_initDone = 1;
            // param low byte is a bitmask of online devices; bit 1 = TF card
            if(p2 & 0x02)
            {
                g_cardOnline = 1;
            }
            break;
        case MSG_ERROR:
            g_error = p2;
            break;
        default:
            break;
    }
}

void MP3_poll(void)
{
    Uint16 b;

    while(HAL_MP3_rxCount() > 0)
    {
        b = HAL_MP3_readByte();
        g_rxBytes++;

        if(g_rxState == RX_START)
        {
            if(b == 0x7E)
            {
                g_rxBuf[0] = b;
                g_rxIdx = 1;
                g_rxState = RX_BODY;
            }
        }
        else
        {
            g_rxBuf[g_rxIdx++] = b;

            if(g_rxIdx == 10)
            {
                if(g_rxBuf[9] == 0xEF)
                {
                    MP3_handleFrame();
                }
                g_rxState = RX_START;
                g_rxIdx = 0;
            }
        }
    }
}

Uint16 MP3_init(Uint16 initialVolume)
{
#if MP3_SEND_RESET
    Uint16 waited;
#endif

    HAL_MP3_init();

    g_rxState = RX_START;
    g_rxIdx = 0;
    g_cardOnline = 0;
    g_initDone = 0;
    g_trackDone = 0;
    g_error = MP3_ERR_NONE;
    g_gotAnything = 0;

    // The module takes ~1-2 s to power up and mount the card before it
    // listens to commands.
    HAL_MP3_delayMs(1500);
    HAL_MP3_flushRx();

#if MP3_SEND_RESET
    // Some clones misbehave after a reset command, so this is optional.
    MP3_sendCommand(CMD_RESET, 0, 0);

    // Wait up to 2 s for the "initialization complete" message.
    for(waited = 0; waited < 2000 && !g_initDone; waited += 10)
    {
        HAL_MP3_delayMs(10);
        MP3_poll();
    }
#endif

#if MP3_SELECT_TF
    MP3_sendCommand(CMD_SELECT_DEVICE, 0, DEVICE_TF_CARD);
    HAL_MP3_delayMs(200);                  // device select needs extra time
#endif

    MP3_setVolume(initialVolume);
    MP3_poll();

    return g_gotAnything;
}

void MP3_playTrack(Uint16 track)
{
    // 0x12 plays /MP3/0001.mp3 etc. (up to 9999), the most reliable mode.
    MP3_sendCommand(CMD_PLAY_MP3_FOLDER, (track >> 8) & 0xFF, track & 0xFF);
    g_trackDone = 0;
}

void MP3_playFolderTrack(Uint16 folder, Uint16 track)
{
    MP3_sendCommand(CMD_PLAY_FOLDER, folder & 0xFF, track & 0xFF);
    g_trackDone = 0;
}

void MP3_pause(void)    { MP3_sendCommand(CMD_PAUSE, 0, 0); }
void MP3_resume(void)   { MP3_sendCommand(CMD_RESUME, 0, 0); }
void MP3_stop(void)     { MP3_sendCommand(CMD_STOP, 0, 0); }
void MP3_next(void)     { MP3_sendCommand(CMD_NEXT, 0, 0); }
void MP3_previous(void) { MP3_sendCommand(CMD_PREVIOUS, 0, 0); }

void MP3_loopTrack(Uint16 track)
{
    MP3_sendCommand(CMD_LOOP_TRACK, (track >> 8) & 0xFF, track & 0xFF);
}

void MP3_loopAll(void)
{
    MP3_sendCommand(CMD_LOOP_ALL, 0, 1);
}

void MP3_randomAll(void)
{
    MP3_sendCommand(CMD_RANDOM_ALL, 0, 0);
}

void MP3_setVolume(Uint16 volume)
{
    if(volume > MP3_VOLUME_MAX)
    {
        volume = MP3_VOLUME_MAX;
    }
    g_volume = volume;
    MP3_sendCommand(CMD_SET_VOLUME, 0, volume);
}

void MP3_volumeUp(void)
{
    if(g_volume < MP3_VOLUME_MAX)
    {
        MP3_setVolume(g_volume + 1);
    }
}

void MP3_volumeDown(void)
{
    if(g_volume > 0)
    {
        MP3_setVolume(g_volume - 1);
    }
}

void MP3_setEQ(Uint16 eq)
{
    if(eq > MP3_EQ_BASS)
    {
        eq = MP3_EQ_NORMAL;
    }
    MP3_sendCommand(CMD_SET_EQ, 0, eq);
}

Uint16 MP3_getVolume(void)
{
    return g_volume;
}

// Link test: ask the module for its status. It always answers a query, so a
// reply proves commands reach it AND its replies reach us at the right baud.
Uint16 MP3_ping(void)
{
    Uint16 t;

    HAL_MP3_flushRx();
    g_rxState = RX_START;
    g_rxIdx = 0;
    g_gotAnything = 0;
    g_rxBytes = 0;

    MP3_sendCommand(0x42, 0, 0);            // query current status

    for(t = 0; t < 100 && !g_gotAnything; t++)
    {
        HAL_MP3_delayMs(5);
        MP3_poll();
    }
    return g_gotAnything;
}

Uint16 MP3_getRxBytes(void)  { return g_rxBytes; }
Uint16 MP3_getLastCmd(void)  { return g_lastCmd; }
Uint16 MP3_getLastParam(void){ return g_lastParam; }

Uint16 MP3_initDone(void)
{
    return g_initDone;
}

Uint16 MP3_cardOnline(void)
{
    return g_cardOnline;
}

Uint16 MP3_trackFinished(void)
{
    Uint16 r = g_trackDone;
    g_trackDone = 0;
    return r;
}

Uint16 MP3_getError(void)
{
    Uint16 e = g_error;
    g_error = MP3_ERR_NONE;
    return e;
}
