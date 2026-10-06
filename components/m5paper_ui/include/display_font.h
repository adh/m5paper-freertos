#ifndef H__display_font__
#define H__display_font__

#include <stdint.h>

/* ANSI SGR sequences understood by the font-aware string APIs. */
#define DISPLAY_FONT_STYLE_RESET       "\x1b[0m"
#define DISPLAY_FONT_BOLD_ON           "\x1b[1m"
#define DISPLAY_FONT_BOLD_OFF          "\x1b[22m"
#define DISPLAY_FONT_ITALIC_ON         "\x1b[3m"
#define DISPLAY_FONT_ITALIC_OFF        "\x1b[23m"
#define DISPLAY_FONT_BOLD_ITALIC_ON    "\x1b[1;3m"

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

typedef enum display_font_style_e {
    DISPLAY_FONT_STYLE_REGULAR = 0,
    DISPLAY_FONT_STYLE_BOLD = 1,
    DISPLAY_FONT_STYLE_ITALIC = 2,
    DISPLAY_FONT_STYLE_BOLD_ITALIC = 3,
} display_font_style_t;

typedef struct display_font_s display_font_t;

typedef struct display_font_family_s {
    const display_font_t* regular;
    const display_font_t* bold;
    const display_font_t* italic;
    const display_font_t* bold_italic;
} display_font_family_t;

struct display_font_s {
    const display_font_glyph_t* glyphs;
    const uint8_t* bitmap;
    const display_font_family_t* family;
    uint16_t first_character;
    uint16_t glyph_count;
    uint8_t height;
    uint8_t baseline;
    uint8_t fallback_character;
    uint8_t style;
};

#endif
