#ifndef H__display__
#define H__display__

#include <stdbool.h>
#include <stdint.h>

typedef struct display_rect_s {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} display_rect_t;

typedef struct display_pixmap_s {
    uint16_t width;
    uint16_t height;
    uint8_t* pixels;
} display_pixmap_t;

typedef enum display_rotation_e {
    DISPLAY_ROTATE_0 = 0,
    DISPLAY_ROTATE_90 = 1,
    DISPLAY_ROTATE_180 = 2,
    DISPLAY_ROTATE_270 = 3,
} display_rotation_t;

void display_init(uint16_t vcomm);
uint16_t display_width(void);
uint16_t display_height(void);
uint8_t* display_framebuffer(void);
void display_clear(uint8_t gray);
void display_fill_rect(int x, int y, int w, int h, uint8_t gray);
void display_stroke_rect(int x, int y, int w, int h, uint16_t thickness, uint8_t gray);
void display_draw_roundrect(int x, int y, int w, int h, uint16_t radius, uint16_t thickness, uint8_t gray);
void display_draw_line(int x0, int y0, int x1, int y1, uint16_t thickness, uint8_t gray);
void display_draw_ellipse(int cx, int cy, uint16_t rx, uint16_t ry, uint16_t thickness, uint8_t gray);
void display_pixmap_free(display_pixmap_t* pixmap);
bool display_pixmap_blit(int x, int y, const display_pixmap_t* pixmap, display_rotation_t rotation, uint16_t scale, int transparent_color);
bool display_pixmap_from_xbm3(display_pixmap_t* pixmap, const char* const* xpm);
bool display_pixmap_get(display_pixmap_t* pixmap, int x, int y, int w, int h);
uint16_t display_xpm3_width(const char* const* xpm);
uint16_t display_xpm3_height(const char* const* xpm);
void display_xpm3_draw(int x, int y, const char* const* xpm);
void display_xpm3_draw_scaled(int x, int y, const char* const* xpm, uint16_t scale);
bool display_damaged(display_rect_t* rect);
bool display_update(void);

#endif
