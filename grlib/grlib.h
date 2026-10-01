/* --COPYRIGHT--,BSD
 * Copyright (c) 2014, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/

#include <stdint.h>
#include <stdbool.h>

#ifndef __GRLIB_H__
#define __GRLIB_H__

//*****************************************************************************
//
// TMS320C28x (C2000) compatibility note:
//
// The C28x core has no native 8-bit-wide storage -- "char" is 16 bits on
// this architecture. Since the C standard requires int16_t/uint16_t to be
// *exactly* 8 bits, the TI C2000 compiler's <stdint.h> correctly omits
// them (it does provide the 16/32-bit types, which map onto native int/
// long). grlib was originally written for 8-bit-char MCUs (MSP430,
// Stellaris/Tiva) and uses uint16_t/int16_t for small fields (font format,
// bits-per-pixel, etc.), so we substitute a 16-bit stand-in here purely so
// this unmodified grlib source compiles on C2000. These fields only ever
// hold small values, so the extra width is harmless.
//
//*****************************************************************************
/*
#if defined(_TMS320C28X) || defined(__TMS320C28XX__) || defined(__TMS320C28X__)
typedef char            int16_t;
typedef unsigned char   uint16_t;
#endif
*/

#define NDEBUG
#include "assert.h"

//*****************************************************************************
//
//! \addtogroup primitives_api
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
// Make sure min and max are defined.
//
//*****************************************************************************
#ifndef min
#define min(a, b)               (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)               (((a) < (b)) ? (b) : (a))
#endif

//*****************************************************************************
//
//! This structure defines the characteristics of a Bitmap Image
//
//*****************************************************************************
typedef struct Graphics_Image
{
    uint16_t bPP;	             //!< Bits per pixel and Compressed/Uncompressed
    uint16_t xSize;              //!< xSize
    uint16_t ySize;              //!< ySize
    uint16_t numColors;          //!< Number of Colors in Palette
    const uint32_t  * pPalette;  //!< Pointer to Palette
    const uint16_t * pPixel;      //!< Pointer to pixel data;
} Graphics_Image;

//*****************************************************************************
//
//! This structure defines the extents of a rectangle.  All points greater than
//! or equal to the minimum and less than or equal to the maximum are part of
//! the rectangle.
//
//*****************************************************************************
typedef struct Graphics_Rectangle
{
	int16_t xMin;  			//!< The minimum X coordinate of the rectangle.
    int16_t yMin;			//!< The minimum Y coordinate of the rectangle.
    int16_t xMax;			//!< The maximum X coordinate of the rectangle.
    int16_t yMax;			//!< The maximum Y coordinate of the rectangle.
} Graphics_Rectangle;


//*****************************************************************************
//
//! This structure defines the characteristics of a display driver.
//
//*****************************************************************************
typedef struct Graphics_Display
{
    int32_t  size;				//!< The size of this structure.
    void *displayData;			//!< A pointer to display driver-specific data.
    uint16_t width;				//!< The width of this display.
    uint16_t heigth;			//!< The height of this display.
    void (*callPixelDraw)(void *displayData, int16_t x, int16_t y,
    		uint16_t value);	//!< A pointer to the function to draw a pixel on this display.
    void (*callPixelDrawMultiple)(void *displayData, int16_t x, int16_t y,
    		int16_t x0, int16_t count, int16_t bPP, const uint16_t *data,
    		const uint32_t *pucPalette);	//!< A pointer to the function to draw multiple pixels on this display.
    void (*callLineDrawH)(void *displayData, int16_t x1, int16_t x2, int16_t y,
    		uint16_t value);	//!< A pointer to the function to draw a horizontal line on this display.
    void (*callLineDrawV)(void *displayData, int16_t x, int16_t y1,
    		int16_t y2, uint16_t value); //!< A pointer to the function to draw a vertical line on this display.
    void (*callRectFill)(void *displayData, const Graphics_Rectangle *rect,
    		uint16_t value);	//!< A pointer to the function to draw a filled rectangle on this display.
    uint32_t (*callColorTranslate)(void *displayData, uint32_t  value);	//!< A pointer to the function to translate 24-bit RGB colors to display-specific colors.
    void (*callFlush)(void *displayData); //!< A pointer to the function to flush any cached drawing operations on this display.
    void (*callClearDisplay)(void *displayData, uint16_t value); //!<  A pointer to the function to clears Display. Contents of display buffer unmodified
} Graphics_Display;

//*****************************************************************************
//
//! This structure describes a font used for drawing text onto the screen.
//
//*****************************************************************************
typedef struct Graphics_Font
{
    uint16_t format;		//!< The format of the font.  Can be one of FONT_FMT_UNCOMPRESSED or FONT_FMT_PIXEL_RLE.
    uint16_t maxWidth;	//!< The maximum width of a character; this is the width of the widest character in the font, though any individual character may be narrower than this width.
    uint16_t height;		//!< The height of the character cell; this may be taller than the font data for the characters (to provide inter-line spacing).
    uint16_t baseline;	//!< The offset between the top of the character cell and the baseline of  the glyph.  The baseline is the bottom row of a capital letter, below which only the descenders of the lower case letters occur.
    uint16_t offset[96];//!< The offset within data to the data for each character in the font.
    const uint16_t *data;//!< A pointer to the data for the font.
} Graphics_Font;

//*****************************************************************************
//
//! This is a newer version of the structure which describes a font used
//! for drawing text onto the screen.  This variant allows a font to contain an
//! arbitrary, contiguous block of codepoints from the 256 basic characters in
//! an ISO8859-n font and allows support for accented characters in Western
//! European languages and any left-to-right typeface supported by an ISO8859
//! variant. Fonts encoded in this format may be used interchangeably with the
//! original fonts merely by casting the structure pointer when calling any
//! function or macro which expects a font pointer as a parameter.
//
//*****************************************************************************
typedef struct Graphics_FontEx
{
    uint16_t format;			//!< The format of the font.  Can be one of FONT_FMT_EX_UNCOMPRESSED or FONT_FMT_EX_PIXEL_RLE.
    uint16_t maxWidth;		//!< The maximum width of a character; this is the width of the widest character in the font, though any individual character may be narrower than this width.
    uint16_t height;			//!< The height of the character cell; this may be taller than the font data for the characters (to provide inter-line spacing).
    uint16_t baseline;		//!< The offset between the top of the character cell and the baseline of the glyph.  The baseline is the bottom row of a capital letter, below which only the descenders of the lower case letters occur.
    uint16_t first;		//!< The codepoint number representing the first character encoded in the font.
    uint16_t last;			//!< The codepoint number representing the last character encoded in the font.
    const uint16_t *offset;	//!< A pointer to a table containing the offset within data to the data for each character in the font.
    const uint16_t *data;	//!< A pointer to the data for the font.
} Graphics_FontEx;

//*****************************************************************************
//
//! This structure defines a drawing context to be used to draw onto the
//! screen.  Multiple drawing contexts may exist at any time.
//
//*****************************************************************************
typedef struct Graphics_Context
{
    int32_t  size;						//!< The size of this structure.
    const Graphics_Display *display;	//!< The screen onto which drawing operations are performed.
    Graphics_Rectangle clipRegion;		//!< The clipping region to be used when drawing onto the screen.
    uint32_t  foreground;				//!< The color used to draw primitives onto the screen.
    uint32_t  background;				//!< The background color used to draw primitives onto the screen.
    const Graphics_Font *font;			//!< The font used to render text onto the screen.
} Graphics_Context;

//*****************************************************************************
//
// Deprecated struct names.  These definitions ensure backwards compatibility
// but new code should avoid using deprecated struct names since these will
// be removed at some point in the future.
//
//*****************************************************************************
#define FONT_FMT_UNCOMPRESSED			GRAPHICS_FONT_FMT_UNCOMPRESSED
#define FONT_FMT_PIXEL_RLE				GRAPHICS_FONT_FMT_PIXEL_RLE
#define FONT_EX_MARKER					GRAPHICS_FONT_EX_MARKER
#define FONT_FMT_EX_UNCOMPRESSED		GRAPHICS_FONT_FMT_EX_UNCOMPRESSED
#define FONT_FMT_EX_PIXEL_RLE			GRAPHICS_FONT_FMT_EX_PIXEL_RLE
#define AUTO_STRING_LENGTH				GRAPHICS_AUTO_STRING_LENGTH
#define OPAQUE_TEXT						GRAPHICS_OPAQUE_TEXT
#define TRANSPARENT_TEXT				GRAPHICS_TRANSPARENT_TEXT
#define IMAGE_FMT_1BPP_UNCOMP			GRAPHICS_IMAGE_FMT_1BPP_UNCOMP
#define IMAGE_FMT_2BPP_UNCOMP			GRAPHICS_IMAGE_FMT_2BPP_UNCOMP
#define IMAGE_FMT_4BPP_UNCOMP			GRAPHICS_IMAGE_FMT_4BPP_UNCOMP
#define IMAGE_FMT_8BPP_UNCOMP			GRAPHICS_IMAGE_FMT_8BPP_UNCOMP
#define IMAGE_FMT_1BPP_COMP_RLE4		GRAPHICS_IMAGE_FMT_1BPP_COMP_RLE4
#define IMAGE_FMT_2BPP_UNCOMP			GRAPHICS_IMAGE_FMT_2BPP_UNCOMP
#define IMAGE_FMT_4BPP_COMP_RLE4		GRAPHICS_IMAGE_FMT_4BPP_COMP_RLE4
#define IMAGE_FMT_1BPP_COMP_RLE8		GRAPHICS_IMAGE_FMT_1BPP_COMP_RLE8
#define IMAGE_FMT_2BPP_COMP_RLE8 		GRAPHICS_IMAGE_FMT_2BPP_COMP_RLE8
#define IMAGE_FMT_4BPP_COMP_RLE8 		GRAPHICS_IMAGE_FMT_4BPP_COMP_RLE8
#define IMAGE_FMT_8BPP_COMP_RLE8		GRAPHICS_IMAGE_FMT_8BPP_COMP_RLE8
#define IMAGE_FMT_8BPP_COMP_RLEBLEND	GRAPHICS_IMAGE_FMT_8BPP_COMP_RLEBLEND
#define tFontEx 								Graphics_FontEx
#define tFont 									Graphics_Font
#define tDisplay 								Graphics_Display
#define tRectangle 								Graphics_Rectangle
#define tImage 									Graphics_Image
#define tContext  								Graphics_Context
#define sXMax									xMax
#define sXMin									xMin
#define sYMax									yMax
#define sYMin									yMin

//*****************************************************************************
//
// Deprecated function names.  These definitions ensure backwards compatibility
// but new code should avoid using deprecated function names since these will
// be removed at some point in the future.
//
//*****************************************************************************
#define GrCircleDraw						Graphics_drawCircle
#define GrCircleFill						Graphics_fillCircle
#define GrContextClipRegionSet				Graphics_setClipRegion
#define GrContextInit						Graphics_initContext
#define GrImageDraw							Graphics_drawImage
#define GrLineDraw							Graphics_drawLine
#define GrLineDrawH							Graphics_drawLineH
#define GrLineDrawV							Graphics_drawLineV
#define GrRectDraw							Graphics_drawRectangle
#define GrRectFill							Graphics_fillRectangle
#define GrStringDraw						Graphics_drawString
#define GrStringWidthGet					Graphics_getStringWidth
#define GrRectOverlapCheck					Graphics_isOverlappingRectangle
#define GrRectIntersectGet					Graphics_getRectangleIntersection
#define GrContextBackgroundSet			   	Graphics_setBackgroundColor
#define GrContextBackgroundSetTranslated   Graphics_setBackgroundColorTranslated
#define GrContextDpyWidthGet			   	Graphics_getDisplayWidth
#define GrContextDpyHeightGet				Graphics_getDisplayHeight
#define GrContextFontSet					Graphics_setFont
#define GrContextForegroundSet				Graphics_setForegroundColor
#define GrContextForegroundSetTranslated  	Graphics_setForegroundColorTranslated
#define GrFlush								Graphics_flushBuffer
#define GrClearDisplay						Graphics_clearDisplay
#define GrFontBaselineGet					Graphics_getFontBaseline
#define GrFontHeightGet						Graphics_getFontHeight
#define GrFontMaxWidthGet					Graphics_getFontMaxWidth
#define GrImageColorsGet					Graphics_getImageColors
#define GrImageHeightGet					Graphics_getImageHeight
#define GrImageWidthGet						Graphics_getImageWidth
#define GrOffScreen1BPPSize					Graphics_getOffscreen1BppImageSize
#define GrOffScreen4BPPSize					Graphics_getOffscreen4BppImageSize
#define GrOffScreen8BPPSize					Graphics_getOffscreen8BppImageSize
#define GrPixelDraw							Graphics_drawPixel
#define GrStringBaselineGet					Graphics_getStringBaseline
#define GrStringDrawCentered				Graphics_drawStringCentered
#define GrStringHeightGet					Graphics_getStringHeight
#define GrStringMaxWidthGet					Graphics_getStringMaxWidth
#define DpyColorTranslate					Graphics_translateColorOnDisplay
#define DpyFlush							Graphics_flushOnDisplay
#define DpyClearDisplay						Graphics_clearDisplayOnDisplay
#define DpyHeightGet						Graphics_getHeightOfDisplay
#define DpyLineDrawH						Graphics_drawHorizontalLineOnDisplay
#define DpyLineDrawV						Graphics_drawVerticalLineOnDisplay
#define DpyPixelDraw						Graphics_drawPixelOnDisplay
#define DpyPixelDrawMultiple				Graphics_drawMultiplePixelsOnDisplay
#define DpyRectFill							Graphics_fillRectangleOnDisplay
#define DpyWidthGet							Graphics_getWidthOfDisplay
#define GrRectContainsPoint					Graphics_isPointWithinRectangle

//*****************************************************************************
//
//! Indicates that the font data is stored in an uncompressed format.
//
//*****************************************************************************
#define GRAPHICS_FONT_FMT_UNCOMPRESSED   0x00

//*****************************************************************************
//
//! Indicates that the font data is stored using a pixel-based RLE format.
//
//*****************************************************************************
#define GRAPHICS_FONT_FMT_PIXEL_RLE      0x01

//*****************************************************************************
//
//! A marker used in the format field of a font to indicates that the font
//! data is stored using the new tFontEx structure.
//
//*****************************************************************************
#define GRAPHICS_FONT_EX_MARKER          0x80

//*****************************************************************************
//
//! Indicates that the font data is stored in an uncompressed format and uses
//! the tFontEx structure format.
//
//*****************************************************************************
#define GRAPHICS_FONT_FMT_EX_UNCOMPRESSED   (FONT_FMT_UNCOMPRESSED | FONT_EX_MARKER)

//*****************************************************************************
//
//! Indicates that the font data is stored using a pixel-based RLE format and
//! uses the tFontEx structure format.
//
//*****************************************************************************
#define GRAPHICS_FONT_FMT_EX_PIXEL_RLE      (FONT_FMT_PIXEL_RLE | FONT_EX_MARKER)

//*****************************************************************************
//
//! Value to automatically draw the entire length of the string
//! (subject to clipping)
//
//*****************************************************************************
#define GRAPHICS_AUTO_STRING_LENGTH     -1

//*****************************************************************************
//
//! Value to draw text opaque
//! The text foreground and background are drawn together
//
//*****************************************************************************
#define GRAPHICS_OPAQUE_TEXT     1

//*****************************************************************************
//
//! Value to draw text transparently
//! The text only (no background) is drawn
//
//*****************************************************************************
#define GRAPHICS_TRANSPARENT_TEXT     0

//*****************************************************************************
//
//! Indicates that the image data is not compressed and represents each pixel
//! with a single bit.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_1BPP_UNCOMP   0x01

//*****************************************************************************
//
//! Indicates that the image data is not compressed and represents each pixel
//! with two bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_2BPP_UNCOMP   0x02

//*****************************************************************************
//
//! Indicates that the image data is not compressed and represents each pixel
//! with four bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_4BPP_UNCOMP   0x04

//*****************************************************************************
//
//! Indicates that the image data is not compressed and represents each pixel
//! with eight bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_8BPP_UNCOMP   0x08

//*****************************************************************************
//
//! Indicates that the image data is compressed with 4 bit Run Length Encoding
//! and represents each pixel with a single bit.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_1BPP_COMP_RLE4     0x41

//*****************************************************************************
//
//! Indicates that the image data is compressed with 4 bit Run Length Encoding 
//! and represents each pixel with two bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_2BPP_COMP_RLE4     0x42

//*****************************************************************************
//
//! Indicates that the image data is compressed with 4 bit Run Length Encoding 
//! and represents each pixel with four bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_4BPP_COMP_RLE4     0x44

//*****************************************************************************
//
//! Indicates that the image data is compressed with 8 bit Run Length Encoding 
//! and represents each pixel with a single bit.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_1BPP_COMP_RLE8     0x81

//*****************************************************************************
//
//! Indicates that the image data is compressed with 8 bit Run Length Encoding
//! and represents each pixel with two bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_2BPP_COMP_RLE8     0x82

//*****************************************************************************
//
//! Indicates that the image data is compressed with 8 bit Run Length Encoding
//! and represents each pixel with four bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_4BPP_COMP_RLE8     0x84

//*****************************************************************************
//
//! Indicates that the image data is compressed with 8 bit Run Length Encoding
//! and represents each pixel with eight bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_8BPP_COMP_RLE8     0x88

//*****************************************************************************
//
//! Indicates that the image data is compressed and represents each pixel with
//! info bits.
//
//*****************************************************************************
#define GRAPHICS_IMAGE_FMT_8BPP_COMP_RLEBLEND     0x28

//*****************************************************************************
//
// Corrected TI grlib Color Definitions (24-bit 0x00RRGGBB Format)
//
//*****************************************************************************
// Primary & System Colors
#define COLOR_BLACK           0x00000000
#define COLOR_WHITE           0x00FFFFFF
#define COLOR_RED             0x00FF0000
#define COLOR_GREEN           0x0000FF00
#define COLOR_BLUE            0x000000FF

// Common UI & Secondary Colors
#define COLOR_YELLOW          0x00FFFF00
#define COLOR_CYAN            0x0000FFFF
#define COLOR_MAGENTA         0x00FF00FF
#define COLOR_ORANGE          0x00FFA500
#define COLOR_PINK            0x00FFC0CB

// Grayscale Spectrum
#define COLOR_LIGHT_GRAY      0x00D3D3D3
#define COLOR_SILVER          0x00C0C0C0
#define COLOR_GRAY            0x00808080
#define COLOR_DARK_GRAY       0x00404040
#define COLOR_CHARCOAL        0x0036454F

// Deep & Earthy Tones
#define COLOR_NAVY            0x00000080
#define COLOR_MAROON          0x00800000
#define COLOR_PURPLE          0x00800080
#define COLOR_DARK_PURPLE     0x004B0082
#define COLOR_BROWN           0x00A52A2A
#define COLOR_OLIVE           0x00808000

// Vibrant Accent Colors
#define COLOR_LIME            0x0000FF00
#define COLOR_TEAL            0x00008080
#define COLOR_GOLD            0x00FFD700
#define COLOR_VIOLET          0x00EE82EE
#define COLOR_INDIGO          0x004B0082
#define COLOR_TURQUOISE       0x0040E0D0
#define COLOR_CORAL           0x00FF7F50
#define COLOR_CRIMSON         0x00DC143C
#define COLOR_KHAKI           0x00F0E68C
#define COLOR_PLUM            0x00DDA0DD
#define COLOR_SKY_BLUE        0x0087CEEB
#define COLOR_MINT            0x0098FF98

// Official Tetris Tetromino Colors (TI grlib 24-bit)
#define COLOR_TETRIS_I_CYAN    0x0000FFFF  // Cyan (I-Piece)
#define COLOR_TETRIS_O_YELLOW  0x00FFFF00  // Yellow (O-Piece)
#define COLOR_TETRIS_T_PURPLE  0x00800080  // Purple (T-Piece)
#define COLOR_TETRIS_S_GREEN   0x0000FF00  // Green (S-Piece)
#define COLOR_TETRIS_Z_RED     0x00FF0000  // Red (Z-Piece)
#define COLOR_TETRIS_J_BLUE    0x000000FF  // Blue (J-Piece)
#define COLOR_TETRIS_L_ORANGE  0x00FF7F00  // Orange (L-Piece)

// Additional Gameplay UI Colors
#define COLOR_TETRIS_GHOST     0x00404040  // Dark Gray for piece shadow
#define COLOR_TETRIS_GRID      0x00202020  // Very dim gray for background grid


//*****************************************************************************
//
// Masks and shifts to aid in color format translation by drivers.
//
//*****************************************************************************
#define ClrRedMask              0x00FF0000
#define ClrRedShift             16
#define ClrGreenMask            0x0000FF00
#define ClrGreenShift           8
#define ClrBlueMask             0x000000FF
#define ClrBlueShift            0

//*****************************************************************************
//
// Prototypes for the predefined fonts in the graphics library.  ..Cm.. is the
// computer modern font, which is a serif font.  ..Cmsc.. is the computer
// modern small-caps font, which is also a serif font.  ..Cmss.. is the
// computer modern sans-serif font.
//
//*****************************************************************************
extern const Graphics_Font g_sFontCm12;
extern const Graphics_Font g_sFontCm12b;
extern const Graphics_Font g_sFontCm12i;
extern const Graphics_Font g_sFontCm14;
extern const Graphics_Font g_sFontCm14b;
extern const Graphics_Font g_sFontCm14i;
extern const Graphics_Font g_sFontCm16;
extern const Graphics_Font g_sFontCm16b;
extern const Graphics_Font g_sFontCm16i;
extern const Graphics_Font g_sFontCm18;
extern const Graphics_Font g_sFontCm18b;
extern const Graphics_Font g_sFontCm18i;
extern const Graphics_Font g_sFontCm20;
extern const Graphics_Font g_sFontCm20b;
extern const Graphics_Font g_sFontCm20i;
extern const Graphics_Font g_sFontCm22;
extern const Graphics_Font g_sFontCm22b;
extern const Graphics_Font g_sFontCm22i;
extern const Graphics_Font g_sFontCm24;
extern const Graphics_Font g_sFontCm24b;
extern const Graphics_Font g_sFontCm24i;
extern const Graphics_Font g_sFontCm26;
extern const Graphics_Font g_sFontCm26b;
extern const Graphics_Font g_sFontCm26i;
extern const Graphics_Font g_sFontCm28;
extern const Graphics_Font g_sFontCm28b;
extern const Graphics_Font g_sFontCm28i;
extern const Graphics_Font g_sFontCm30;
extern const Graphics_Font g_sFontCm30b;
extern const Graphics_Font g_sFontCm30i;
extern const Graphics_Font g_sFontCm32;
extern const Graphics_Font g_sFontCm32b;
extern const Graphics_Font g_sFontCm32i;
extern const Graphics_Font g_sFontCm34;
extern const Graphics_Font g_sFontCm34b;
extern const Graphics_Font g_sFontCm34i;
extern const Graphics_Font g_sFontCm36;
extern const Graphics_Font g_sFontCm36b;
extern const Graphics_Font g_sFontCm36i;
extern const Graphics_Font g_sFontCm38;
extern const Graphics_Font g_sFontCm38b;
extern const Graphics_Font g_sFontCm38i;
extern const Graphics_Font g_sFontCm40;
extern const Graphics_Font g_sFontCm40b;
extern const Graphics_Font g_sFontCm40i;
extern const Graphics_Font g_sFontCm42;
extern const Graphics_Font g_sFontCm42b;
extern const Graphics_Font g_sFontCm42i;
extern const Graphics_Font g_sFontCm44;
extern const Graphics_Font g_sFontCm44b;
extern const Graphics_Font g_sFontCm44i;
extern const Graphics_Font g_sFontCm46;
extern const Graphics_Font g_sFontCm46b;
extern const Graphics_Font g_sFontCm46i;
extern const Graphics_Font g_sFontCm48;
extern const Graphics_Font g_sFontCm48b;
extern const Graphics_Font g_sFontCm48i;
extern const Graphics_Font g_sFontCmsc12;
extern const Graphics_Font g_sFontCmsc14;
extern const Graphics_Font g_sFontCmsc16;
extern const Graphics_Font g_sFontCmsc18;
extern const Graphics_Font g_sFontCmsc20;
extern const Graphics_Font g_sFontCmsc22;
extern const Graphics_Font g_sFontCmsc24;
extern const Graphics_Font g_sFontCmsc26;
extern const Graphics_Font g_sFontCmsc28;
extern const Graphics_Font g_sFontCmsc30;
extern const Graphics_Font g_sFontCmsc32;
extern const Graphics_Font g_sFontCmsc34;
extern const Graphics_Font g_sFontCmsc36;
extern const Graphics_Font g_sFontCmsc38;
extern const Graphics_Font g_sFontCmsc40;
extern const Graphics_Font g_sFontCmsc42;
extern const Graphics_Font g_sFontCmsc44;
extern const Graphics_Font g_sFontCmsc46;
extern const Graphics_Font g_sFontCmsc48;
extern const Graphics_Font g_sFontCmss12;
extern const Graphics_Font g_sFontCmss12b;
extern const Graphics_Font g_sFontCmss12i;
extern const Graphics_Font g_sFontCmss14;
extern const Graphics_Font g_sFontCmss14b;
extern const Graphics_Font g_sFontCmss14i;
extern const Graphics_Font g_sFontCmss16;
extern const Graphics_Font g_sFontCmss16b;
extern const Graphics_Font g_sFontCmss16i;
extern const Graphics_Font g_sFontCmss18;
extern const Graphics_Font g_sFontCmss18b;
extern const Graphics_Font g_sFontCmss18i;
extern const Graphics_Font g_sFontCmss20;
extern const Graphics_Font g_sFontCmss20b;
extern const Graphics_Font g_sFontCmss20i;
extern const Graphics_Font g_sFontCmss22;
extern const Graphics_Font g_sFontCmss22b;
extern const Graphics_Font g_sFontCmss22i;
extern const Graphics_Font g_sFontCmss24;
extern const Graphics_Font g_sFontCmss24b;
extern const Graphics_Font g_sFontCmss24i;
extern const Graphics_Font g_sFontCmss26;
extern const Graphics_Font g_sFontCmss26b;
extern const Graphics_Font g_sFontCmss26i;
extern const Graphics_Font g_sFontCmss28;
extern const Graphics_Font g_sFontCmss28b;
extern const Graphics_Font g_sFontCmss28i;
extern const Graphics_Font g_sFontCmss30;
extern const Graphics_Font g_sFontCmss30b;
extern const Graphics_Font g_sFontCmss30i;
extern const Graphics_Font g_sFontCmss32;
extern const Graphics_Font g_sFontCmss32b;
extern const Graphics_Font g_sFontCmss32i;
extern const Graphics_Font g_sFontCmss34;
extern const Graphics_Font g_sFontCmss34b;
extern const Graphics_Font g_sFontCmss34i;
extern const Graphics_Font g_sFontCmss36;
extern const Graphics_Font g_sFontCmss36b;
extern const Graphics_Font g_sFontCmss36i;
extern const Graphics_Font g_sFontCmss38;
extern const Graphics_Font g_sFontCmss38b;
extern const Graphics_Font g_sFontCmss38i;
extern const Graphics_Font g_sFontCmss40;
extern const Graphics_Font g_sFontCmss40b;
extern const Graphics_Font g_sFontCmss40i;
extern const Graphics_Font g_sFontCmss42;
extern const Graphics_Font g_sFontCmss42b;
extern const Graphics_Font g_sFontCmss42i;
extern const Graphics_Font g_sFontCmss44;
extern const Graphics_Font g_sFontCmss44b;
extern const Graphics_Font g_sFontCmss44i;
extern const Graphics_Font g_sFontCmss46;
extern const Graphics_Font g_sFontCmss46b;
extern const Graphics_Font g_sFontCmss46i;
extern const Graphics_Font g_sFontCmss48;
extern const Graphics_Font g_sFontCmss48b;
extern const Graphics_Font g_sFontCmss48i;
extern const Graphics_Font g_sFontCmtt12;
extern const Graphics_Font g_sFontCmtt14;
extern const Graphics_Font g_sFontCmtt16;
extern const Graphics_Font g_sFontCmtt18;
extern const Graphics_Font g_sFontCmtt20;
extern const Graphics_Font g_sFontCmtt22;
extern const Graphics_Font g_sFontCmtt24;
extern const Graphics_Font g_sFontCmtt26;
extern const Graphics_Font g_sFontCmtt28;
extern const Graphics_Font g_sFontCmtt30;
extern const Graphics_Font g_sFontCmtt32;
extern const Graphics_Font g_sFontCmtt34;
extern const Graphics_Font g_sFontCmtt36;
extern const Graphics_Font g_sFontCmtt38;
extern const Graphics_Font g_sFontCmtt40;
extern const Graphics_Font g_sFontCmtt42;
extern const Graphics_Font g_sFontCmtt44;
extern const Graphics_Font g_sFontCmtt46;
extern const Graphics_Font g_sFontCmtt48;
extern const Graphics_Font g_sFontFixed6x8;

//*****************************************************************************
//
// Language identifiers supported by the string table processing functions.
//
//*****************************************************************************
#define GrLangZhPRC             0x0804      // Chinese (PRC)
#define GrLangZhTW              0x0404      // Chinese (Taiwan)
#define GrLangEnUS              0x0409      // English (United States)
#define GrLangEnUK              0x0809      // English (United Kingdom)
#define GrLangEnAUS             0x0C09      // English (Australia)
#define GrLangEnCA              0x1009      // English (Canada)
#define GrLangEnNZ              0x1409      // English (New Zealand)
#define GrLangFr                0x040C      // French (Standard)
#define GrLangDe                0x0407      // German (Standard)
#define GrLangHi                0x0439      // Hindi
#define GrLangIt                0x0410      // Italian (Standard)
#define GrLangJp                0x0411      // Japanese
#define GrLangKo                0x0412      // Korean
#define GrLangEsMX              0x080A      // Spanish (Mexico)
#define GrLangEsSP              0x0C0A      // Spanish (Spain)
#define GrLangSwKE              0x0441      // Swahili (Kenya)
#define GrLangUrIN              0x0820      // Urdu (India)
#define GrLangUrPK              0x0420      // Urdu (Pakistan)



//*****************************************************************************
//
// Prototypes for the graphics library functions.
//
//*****************************************************************************

//*****************************************************************************
//
//! Draws a circle.
//!
//! \param context is a pointer to the drawing context to use.
//! \param x is the X coordinate of the center of the circle.
//! \param y is the Y coordinate of the center of the circle.
//! \param radius is the radius of the circle.
//!
//! This function draws a circle, utilizing the Bresenham circle drawing
//! algorithm.  The extent of the circle is from \e x - \e radius to \e x +
//! \e radius and \e y - \e radius to \e y + \e radius, inclusive.
//!
//! \return None.
//
//*****************************************************************************
extern void Graphics_drawCircle(const Graphics_Context *context, int32_t  x,
		int32_t  y, int32_t  lRadius);
extern void Graphics_fillCircle(const Graphics_Context *context, int32_t  x,
		int32_t  y, int32_t  lRadius);
extern void Graphics_setClipRegion(Graphics_Context *context,
		Graphics_Rectangle *rect);
extern void Graphics_initContext(Graphics_Context *context,
		const Graphics_Display *display);
extern void Graphics_drawImage(const Graphics_Context *context,
                        const Graphics_Image *pBitmap, int16_t x, int16_t y);
extern void Graphics_drawLine(const Graphics_Context *context, int32_t  x1,
		int32_t  y1, int32_t  x2, int32_t  y2);
extern void Graphics_drawLineH(const Graphics_Context *context, int32_t  x1,
		int32_t  x2, int32_t  y);
extern void Graphics_drawLineV(const Graphics_Context *context, int32_t  x,
		int32_t  y1, int32_t  y2);
extern void Graphics_drawRectangle(const Graphics_Context *context,
		const Graphics_Rectangle *rect);
extern void Graphics_fillRectangle(const Graphics_Context *context,
		const Graphics_Rectangle *rect);
extern void Graphics_drawString(const Graphics_Context *context, int16_t *string,
       int32_t  lLength, int32_t  x, int32_t  y, bool  opaque);
extern int32_t  Graphics_getStringWidth(const Graphics_Context *context,
		const int16_t *string, int32_t  lLength);
extern int32_t  Graphics_isOverlappingRectangle(Graphics_Rectangle *psRect1,
		Graphics_Rectangle *psRect2);
extern int32_t  Graphics_getRectangleIntersection(Graphics_Rectangle *psRect1,
		Graphics_Rectangle *psRect2, Graphics_Rectangle *psIntersect);
extern void Graphics_setBackgroundColor(Graphics_Context *context,
		int32_t value);
extern uint16_t Graphics_getDisplayWidth(Graphics_Context *context);
extern uint16_t Graphics_getDisplayHeight(Graphics_Context *context);
extern void Graphics_setFont(Graphics_Context *context,
		const Graphics_Font *font);
extern uint16_t Graphics_getFontBaseline(const Graphics_Font *font);
extern void Graphics_setForegroundColor(Graphics_Context *context,
		int32_t value);
extern void Graphics_setForegroundColorTranslated(Graphics_Context *context,
		int32_t value);
extern uint16_t Graphics_getFontHeight(const Graphics_Font *font);
extern uint16_t Graphics_getFontMaxWidth(const Graphics_Font *font);
extern uint16_t Graphics_getImageColors(const Graphics_Image *image);
extern uint16_t Graphics_getImageHeight(const Graphics_Image *image);
extern uint16_t Graphics_getImageWidth(const Graphics_Image *image);
extern uint32_t Graphics_getOffscreen1BppImageSize(uint16_t width,
		uint16_t height);
extern uint32_t Graphics_getOffscreen4BppImageSize(uint16_t width,
		uint16_t height);
extern uint32_t Graphics_getOffScreen8BPPSize(uint16_t width, uint16_t height);
extern void  Graphics_drawStringCentered(const Graphics_Context *context,
		int16_t *string, int32_t  length, int32_t  x, int32_t  y,
		bool  opaque);
extern uint16_t Graphics_getStringHeight(const Graphics_Context *context);
extern uint16_t Graphics_getStringMaxWidth(const Graphics_Context *context);
extern uint16_t Graphics_getStringBaseline(const Graphics_Context *context);
extern uint32_t Graphics_translateColorOnDisplay(const Graphics_Display *display,
		uint32_t value);
extern void Graphics_drawHorizontalLineOnDisplay(
		const Graphics_Display *display, uint16_t x1, uint16_t  x2, uint16_t  y,
		uint32_t value);
extern void Graphics_drawVerticalLineOnDisplay(const Graphics_Display *display,
		uint16_t x, uint16_t y1, uint16_t y2, uint16_t value);
extern void Graphics_fillRectangleOnDisplay(const Graphics_Display *display,
		const Graphics_Rectangle *rect, uint16_t value);
extern void Graphics_flushOnDisplay(const Graphics_Display *display);
extern void Graphics_drawPixel(const Graphics_Context *context, uint16_t x,
		uint16_t y);
extern void Graphics_clearDisplay(const Graphics_Context *context);
extern uint16_t Graphics_getHeightOfDisplay(const Graphics_Display *display);
extern void Graphics_flushBuffer(const Graphics_Context *context);
extern uint16_t Graphics_getWidthOfDisplay(const Graphics_Display *display);
extern bool Graphics_isPointWithinRectangle(const Graphics_Rectangle *rect,
		uint16_t x, uint16_t y);
extern void Graphics_drawPixelOnDisplay(const Graphics_Display *display,
		uint16_t x, uint16_t y, uint16_t value);
extern void Graphics_clearDisplayOnDisplay(const Graphics_Display *display,
		uint16_t value);
extern void Graphics_drawMultiplePixelsOnDisplay(
		const Graphics_Display *display, uint16_t x, uint16_t y, uint16_t x0,
		uint16_t  count, uint16_t bPP, const uint16_t *data,
		const uint32_t *pucPalette);
extern void Graphics_initOffscreen1BppImage(Graphics_Display *display,
        uint16_t *image, int32_t width, int32_t height);
extern void Graphics_initOffscreen4BppImage(Graphics_Display *display,
        uint16_t *image, int32_t width, int32_t height);
extern void Graphics_setOffscreen4BppPalette(Graphics_Display *display,
        uint32_t *ppalette, uint32_t offset, uint32_t count);
extern void Graphics_initOffscreen8BppImage(Graphics_Display *display,
        uint16_t *image, int32_t width, int32_t height);
extern void Graphics_setOffscreen8BppPalette(Graphics_Display *display,
        uint32_t *ppalette, uint32_t offset, uint32_t count);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

#endif // __GRLIB_H__
