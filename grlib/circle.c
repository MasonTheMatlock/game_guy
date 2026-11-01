//#############################################################################
// FILE:   grlib/circle.c
// TITLE:  Graphics_drawCircle() and Graphics_fillCircle()
//
//   grlib.h declares both, but the TI grlib source for them was missing from
//   this project, so any call failed to link ("unresolved symbol
//   _Graphics_fillCircle"). Drop this file next to line.c / rectangle.c.
//
//   Integer midpoint circle (no multiply/divide in the loop, no float).
//   Everything is clipped to context->clipRegion, so partly off-screen circles
//   are safe. Uses the foreground color of the context, like the other
//   grlib shapes.
//#############################################################################

#include "grlib.h"

// Plot one pixel if it is inside the clipping region.
static void PlotClipped(const Graphics_Context *context, int32_t x, int32_t y)
{
    if(x < context->clipRegion.xMin || x > context->clipRegion.xMax ||
       y < context->clipRegion.yMin || y > context->clipRegion.yMax)
    {
        return;
    }
    Graphics_drawPixel(context, (uint16_t)x, (uint16_t)y);
}

//*****************************************************************************
//! Draws the outline of a circle (1 pixel wide).
//!
//! \param context  drawing context (foreground color is used)
//! \param x, y     center
//! \param lRadius  radius in pixels (negative = nothing drawn)
//*****************************************************************************
void Graphics_drawCircle(const Graphics_Context *context, int32_t x, int32_t y,
                         int32_t lRadius)
{
    int32_t a = 0;
    int32_t b = lRadius;
    int32_t d = 3 - 2 * lRadius;

    if(lRadius < 0) return;

    while(a <= b)
    {
        PlotClipped(context, x + a, y + b);
        PlotClipped(context, x - a, y + b);
        PlotClipped(context, x + a, y - b);
        PlotClipped(context, x - a, y - b);
        PlotClipped(context, x + b, y + a);
        PlotClipped(context, x - b, y + a);
        PlotClipped(context, x + b, y - a);
        PlotClipped(context, x - b, y - a);

        if(d < 0)
        {
            d += 4 * a + 6;
        }
        else
        {
            d += 4 * (a - b) + 10;
            b--;
        }
        a++;
    }
}

//*****************************************************************************
//! Draws a filled circle, as horizontal spans (fast on the SPI LCD).
//!
//! Same parameters as Graphics_drawCircle().
//*****************************************************************************
void Graphics_fillCircle(const Graphics_Context *context, int32_t x, int32_t y,
                         int32_t lRadius)
{
    int32_t a = 0;
    int32_t b = lRadius;
    int32_t d = 3 - 2 * lRadius;

    if(lRadius < 0) return;

    while(a <= b)
    {
        Graphics_drawLineH(context, x - b, x + b, y + a);
        if(a != 0) Graphics_drawLineH(context, x - b, x + b, y - a);

        // The two long spans at the top and bottom, drawn once b is about to
        // shrink (otherwise they would be redrawn on every step).
        if(d >= 0 || a == b)
        {
            if(a != b)
            {
                Graphics_drawLineH(context, x - a, x + a, y + b);
                Graphics_drawLineH(context, x - a, x + a, y - b);
            }
        }

        if(d < 0)
        {
            d += 4 * a + 6;
        }
        else
        {
            d += 4 * (a - b) + 10;
            b--;
        }
        a++;
    }
}
