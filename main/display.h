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

void display_init(uint16_t vcomm);
uint16_t display_width(void);
uint16_t display_height(void);
uint8_t* display_framebuffer(void);
void display_clear(uint8_t gray);
void display_fill_rect(int x, int y, int w, int h, uint8_t gray);
void display_stroke_rect(int x, int y, int w, int h, uint16_t thickness, uint8_t gray);
void display_draw_line(int x0, int y0, int x1, int y1, uint16_t thickness, uint8_t gray);
uint16_t display_xpm3_width(const char* const* xpm);
uint16_t display_xpm3_height(const char* const* xpm);
void display_xpm3_draw(int x, int y, const char* const* xpm);
void display_xpm3_draw_scaled(int x, int y, const char* const* xpm, uint16_t scale);
bool display_damaged(display_rect_t* rect);
bool display_update(void);

#endif
