//#############################################################################
// FILE:   app/wason/wason_beat.c
// TITLE:  Wason Ware microgame BEAT!  (Melatonin)
//
//   Public: ww_MlInit, ww_MlUpdate, ww_MlRender  (declared in wason_priv.h,
//   listed in the table in wason_table.c). Everything else here is static.
//#############################################################################
#include "wason_priv.h"

// ============================================================================
// BEAT!  (Melatonin)  rhythm. "Melatonin Groove" plays (songs.c: 28 bars, one
//        song unit = an eighth note = 150 ms, so a quarter-note beat is 300 ms).
//        Each note flies in and lands on the ring exactly when that note of the
//        song sounds - press any button ON it. Notes are placed from the song's
//        own clock, not frame counts, so they stay in time however fast the
//        screen is. Hit 6 of 8.
//
//        kMlUnit[] is the chart: the song-unit number of each note to hit. Unit n
//        sounds n * 150 ms after the song starts (unit 8 = the G on bar 2 beat 1).
//        To re-chart, pick any non-rest notes of song_melatonin.
// ============================================================================
#define ML_Z        70
#define ML_N        8
#define ML_UNIT_MS  150                     // = song_melatonin.eighthMs
#define ML_WIN_MS   100                     // +/- this counts as "on the beat"
#define ML_NEED     6

static const int16_t kMlUnit[ML_N] = { 8, 10, 13, 16, 18, 21, 24, 26 };

static int16_t ml_res[ML_N], ml_px[ML_N], ml_hits, ml_miss, ml_flash, ml_v, ml_bdr;
static uint32_t ml_t0;
static bool    ml_started, ml_dr[ML_N];

static int32_t MlNow(void)                  // ms since the song started (0 until it has)
{
    return ml_started ? (int32_t)MsSince(ml_t0) : 0;
}

static int16_t MlX(int16_t i, int32_t now)  // note x: 0.2 px per ms of lead time
{
    int32_t lead = (int32_t)kMlUnit[i] * ML_UNIT_MS - now;
    int32_t x = ML_Z + (lead * 13) / 64;
    return (x > 340) ? 340 : (int16_t)x;
}

void ww_MlInit(uint16_t spd)
{
    int16_t i;

    ml_hits = ml_miss = ml_flash = 0;
    ml_bdr = -1;
    ml_started = false;
    for(i = 0; i < ML_N; i++) { ml_res[i] = 0; ml_px[i] = 340; ml_dr[i] = false; }
}

int16_t ww_MlUpdate(void)
{
    int16_t i, best = -1, bd = 9999, d, resolved = 0;
    int32_t now;

    if(!ml_started && ww_frame >= 2)                             // start the music after the first screen is up
    {
        Buzzer_PlaySong(&song_wasonware, 1);
        ml_t0 = TimerNow();
        ml_started = true;
    }
    if(!ml_started) return MG_PLAY;
    now = MlNow();
    if(ml_flash > 0) ml_flash--;

    for(i = 0; i < ML_N; i++)
    {
        d = (int16_t)(now - (int32_t)kMlUnit[i] * ML_UNIT_MS);          // + = note is late
        if(ml_res[i] == 0 && d > ML_WIN_MS)
        {
            ml_res[i] = 2;
            if(++ml_miss >= ML_N - ML_NEED + 1) return MG_LOSE;
        }
        if(ml_res[i] != 0) resolved++;
    }

    if(ww_AnyPress())
    {
        for(i = 0; i < ML_N; i++)
        {
            d = ww_Abs16((int16_t)(now - (int32_t)kMlUnit[i] * ML_UNIT_MS));
            if(ml_res[i] == 0 && d <= ML_WIN_MS && d < bd) { bd = d; best = i; }
        }
        if(best >= 0)
        {
            ml_res[best] = 1;  resolved++;
            ml_flash = 6;
            if(++ml_hits >= ML_NEED) return MG_WIN;
        }
    }
    if(resolved >= ML_N) return (ml_hits >= ML_NEED) ? MG_WIN : MG_LOSE;
    return MG_PLAY;
}

static void MlRing(uint32_t c)
{
    ww_Box(ML_Z - 22, 93, 44, 3, c);   ww_Box(ML_Z - 22, 134, 44, 3, c);
    ww_Box(ML_Z - 22, 93, 3, 44, c);   ww_Box(ML_Z + 19, 93, 3, 44, c);
}

void ww_MlRender(void)
{
    int16_t  i, x, b;
    int32_t  now = MlNow();
    bool     nearRing = false;
    static bool flashDr = false;

    if(ww_first)
    {
        ww_Text("PRESS ON THE NOTE", 160, 206, COLOR_GRAY);
        MlRing(COLOR_WHITE);  flashDr = false;  ml_bdr = -1;
    }

    for(i = 0; i < ML_N; i++)
    {
        x = MlX(i, now);
        if(ml_res[i] == 0 && x < 340)
        {
            if(ml_dr[i]) ww_MoveArt(ml_px[i] - 12, 103, x - 12, 103, ww_kArtNote, 12, 12, 2, ML_BG);
            else         ww_DrawArt(x - 12, 103, ww_kArtNote, 12, 12, 2, ML_BG, true);
            ml_px[i] = x;  ml_dr[i] = true;
            if(ww_Abs16(x - ML_Z) < 40) nearRing = true;
        }
        else if(ml_dr[i])
        {
            ww_Box(ml_px[i] - 12, 103, 24, 24, ML_BG);
            ml_dr[i] = false;
            nearRing = true;
        }
    }
    if(nearRing || (ml_flash > 0) != flashDr)                   // ring only when a note touched it / flash changed
    {
        MlRing(ml_flash ? COLOR_LIME : COLOR_WHITE);
        flashDr = (ml_flash > 0);
    }

    if(ml_started)                                              // 4 beat lights show where you are in the bar
    {
        b = (int16_t)((now / (ML_UNIT_MS * 2)) & 3);                   // quarter-note beat within the bar
        if(b != ml_bdr)
        {
            for(i = 0; i < 4; i++) ww_Box(110 + i * 28, 170, 20, 12, i == b ? COLOR_YELLOW : 0x4A3A70UL);
            ml_bdr = b;
        }
    }
}
