#ifndef H__display_font__
#define H__display_font__

#include <stdint.h>

/*
 * Glyph bitmaps are stored top-to-bottom, with each row padded to a byte.
 * Pixels within a byte are most-significant-bit first.  The glyph's bearing
 * is measured from the pen position to the left edge of its bitmap.
 */
typedef struct display_font_glyph_s {
    uint32_t bitmap_offset;
    uint8_t width;
    uint8_t advance;
    int8_t bearing;
    uint8_t exists;
} display_font_glyph_t;

typedef struct display_font_s {
    const display_font_glyph_t* glyphs;
    const uint8_t* bitmap;
    uint16_t first_character;
    uint16_t glyph_count;
    uint8_t height;
    uint8_t baseline;
    uint8_t fallback_character;
} display_font_t;

#endif
