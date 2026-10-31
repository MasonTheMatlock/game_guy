#include "images.h"
#include <stdint.h>

extern const uint16_t g_sGameGuySplash4bpp[];
extern const uint16_t g_sGameGuySplashPalette[16];
extern const uint16_t g_sGameGuySplashWidth;
extern const uint16_t g_sGameGuySplashHeight;

#define SPLASH_SCALE 2u

void Splash_Show(void)
{
    uint16_t x, y;
    uint16_t screenWidth = Graphics_getDisplayWidth(&g_sContext);
    uint16_t screenHeight = Graphics_getDisplayHeight(&g_sContext);
    const Graphics_Display *display = g_sContext.display;
    uint16_t startX = (screenWidth - (g_sGameGuySplashWidth * SPLASH_SCALE)) / 2u;
    uint16_t startY = (screenHeight - (g_sGameGuySplashHeight * SPLASH_SCALE)) / 2u;

    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_clearDisplay(&g_sContext);

    for (y = 0; y < g_sGameGuySplashHeight; ++y)
    {
        for (x = 0; x < g_sGameGuySplashWidth; ++x)
        {
            uint32_t pixelIndex = (uint32_t)y * g_sGameGuySplashWidth + x;
            uint16_t packed = g_sGameGuySplash4bpp[pixelIndex >> 1];
            uint16_t colorIndex = (pixelIndex & 1u) ? (packed & 0x0Fu) : (packed >> 4);
            uint16_t color = g_sGameGuySplashPalette[colorIndex];
            int16_t sx = (int16_t)(startX + x * SPLASH_SCALE);
            int16_t sy = (int16_t)(startY + y * SPLASH_SCALE);
            display->callPixelDraw(display->displayData, sx, sy, color);
            display->callPixelDraw(display->displayData, sx + 1, sy, color);
            display->callPixelDraw(display->displayData, sx, sy + 1, color);
            display->callPixelDraw(display->displayData, sx + 1, sy + 1, color);
        }
    }

    Graphics_flushBuffer(&g_sContext);
    {
        uint16_t i;
        for (i = 0; i < 200; ++i) DELAY_US(10000);
    }
}
