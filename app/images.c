#include "images.h"

extern const int scale = 2;
// This must be defined by your converted image asset.
extern const Graphics_Image g_sGameGuySplash;

// ============================================================================
// STEP 1: NATIVE 16-BIT BIT MASK TRANSLATION FOR C2000 ARCHITECTURE
// ============================================================================

// Unpack channels from raw 16-bit word without byte-boundary assumptions
#define UNPACK_RAW_R(pixel16)   (((pixel16) >> 11) & 0x1F)
#define UNPACK_RAW_G(pixel16)   (((pixel16) >> 5)  & 0x3F)
#define UNPACK_RAW_B(pixel16)   ((pixel16) & 0x1F)

// Target packing configurations
#define PACK_NATIVE_RGB565(r,g,b)  ((((uint16_t)(r) & 0x1F) << 11) | \
                                    (((uint16_t)(g) & 0x3F) << 5)  | \
                                    (((uint16_t)(b) & 0x1F)))

#define PACK_NATIVE_BGR565(r,g,b)  ((((uint16_t)(b) & 0x1F) << 11) | \
                                    (((uint16_t)(g) & 0x3F) << 5)  | \
                                    (((uint16_t)(r) & 0x1F)))

// ============================================================================
// STEP 2: CUSTOM C2000 RENDER LOOP TO BYPASS GRLIB POINTER CORRUPTION
// ============================================================================
void Display_RenderSplashC2000(const Graphics_Image *pImage, int16_t xStart, int16_t yStart)
{
    uint32_t i;
    uint32_t totalPixels = (uint32_t)pImage->xSize * (uint32_t)pImage->ySize;
    const uint16_t *pData = pImage->pPixel;
    const Graphics_Display *pDpy = g_sContext.display;

    for(i = 0; i < totalPixels; i++)
    {
        uint16_t rawPixel = pData[i];
        
        // Extract channels safely using 16-bit masks
        uint16_t r = UNPACK_RAW_R(rawPixel);
        uint16_t g = UNPACK_RAW_G(rawPixel);
        uint16_t b = UNPACK_RAW_B(rawPixel);
        
        // Profiles (Keep Profile 1 unless your colors are flipped)
        uint16_t nativeColor = PACK_NATIVE_RGB565(r, g, b);
        // uint16_t nativeColor = PACK_NATIVE_BGR565(r, g, b);

        // Find the base source coordinates of the original image
        int16_t origX = i % pImage->xSize;
        int16_t origY = i / pImage->xSize;

        // Multiply coordinates by 2 to find the new scaled anchor position on screen
        int16_t scaledX = xStart + (origX * 2);
        int16_t scaledY = yStart + (origY * 2);

        // Render a 2x2 block of physical pixels for this single source pixel
        // Top-Left Pixel
        pDpy->callPixelDraw(pDpy->displayData, scaledX,     scaledY,     nativeColor);
        // Top-Right Pixel
        pDpy->callPixelDraw(pDpy->displayData, scaledX + 1, scaledY,     nativeColor);
        // Bottom-Left Pixel
        pDpy->callPixelDraw(pDpy->displayData, scaledX,     scaledY + 1, nativeColor);
        // Bottom-Right Pixel
        pDpy->callPixelDraw(pDpy->displayData, scaledX + 1, scaledY + 1, nativeColor);
    }
}


// ============================================================================

void Splash_Show(void)
{
    int16_t x;
    int16_t y;
    uint16_t screenWidth;
    uint16_t screenHeight;

    screenWidth = Graphics_getDisplayWidth(&g_sContext);
    screenHeight = Graphics_getDisplayHeight(&g_sContext);

    // Clear the LCD to black.
    Graphics_setBackgroundColor(&g_sContext, COLOR_BLACK);
    Graphics_setForegroundColor(&g_sContext, COLOR_WHITE);
    Graphics_clearDisplay(&g_sContext);

    // Center the splash image.
    x = (screenWidth - Graphics_getImageWidth(&g_sGameGuySplash)*scale) / 2;
    y = (screenHeight - Graphics_getImageHeight(&g_sGameGuySplash)*scale) / 2;

    // USE THE NEW NATIVE RENDER LOOP INSTEAD OF Graphics_drawImage
    Display_RenderSplashC2000(&g_sGameGuySplash, x, y);

    Graphics_flushBuffer(&g_sContext);

    // Hold the splash screen for approximately 2 seconds.
    {
        uint16_t i;
        for (i = 0; i < 200; i++)
        {
            DELAY_US(10000);
        }
    }
}
