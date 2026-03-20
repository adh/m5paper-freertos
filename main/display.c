#include "display.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "it8951.h"

static const char* TAG = "display";

typedef struct damage_state_s {
    bool dirty;
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
} damage_state_t;

static uint8_t* s_framebuffer;
static uint16_t s_width;
static uint16_t s_height;
static damage_state_t s_damage;

static bool clip_rect(int* x, int* y, int* w, int* h) {
    int x0 = *x;
    int y0 = *y;
    int x1 = x0 + *w;
    int y1 = y0 + *h;

    if (x1 <= 0 || y1 <= 0 || x0 >= s_width || y0 >= s_height) {
        return false;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > s_width) {
        x1 = s_width;
    }
    if (y1 > s_height) {
        y1 = s_height;
    }

    *x = x0;
    *y = y0;
    *w = x1 - x0;
    *h = y1 - y0;
    return *w > 0 && *h > 0;
}

static void mark_damage(int x, int y, int w, int h) {
    if (!clip_rect(&x, &y, &w, &h)) {
        return;
    }

    const uint16_t x1 = (uint16_t)(x + w - 1);
    const uint16_t y1 = (uint16_t)(y + h - 1);

    if (!s_damage.dirty) {
        s_damage.dirty = true;
        s_damage.x0 = (uint16_t)x;
        s_damage.y0 = (uint16_t)y;
        s_damage.x1 = x1;
        s_damage.y1 = y1;
        return;
    }

    if (x < s_damage.x0) {
        s_damage.x0 = (uint16_t)x;
    }
    if (y < s_damage.y0) {
        s_damage.y0 = (uint16_t)y;
    }
    if (x1 > s_damage.x1) {
        s_damage.x1 = x1;
    }
    if (y1 > s_damage.y1) {
        s_damage.y1 = y1;
    }
}

void display_init(uint16_t vcomm) {
    it8951_init(vcomm);
    s_width = it8951_width();
    s_height = it8951_height();
    s_framebuffer = it8951_framebuffer();
    memset(s_framebuffer, 0xFF, (size_t)s_width * s_height);
    s_damage.dirty = false;
}

uint16_t display_width(void) {
    return s_width;
}

uint16_t display_height(void) {
    return s_height;
}

uint8_t* display_framebuffer(void) {
    return s_framebuffer;
}

void display_clear(uint8_t gray) {
    memset(s_framebuffer, gray, (size_t)s_width * s_height);
    mark_damage(0, 0, s_width, s_height);
}

void display_fill_rect(int x, int y, int w, int h, uint8_t gray) {
    if (!clip_rect(&x, &y, &w, &h)) {
        return;
    }

    for (int yy = y; yy < y + h; ++yy) {
        memset(&s_framebuffer[(size_t)yy * s_width + x], gray, (size_t)w);
    }

    mark_damage(x, y, w, h);
}

void display_stroke_rect(int x, int y, int w, int h, uint16_t thickness, uint8_t gray) {
    if (thickness == 0 || w <= 0 || h <= 0) {
        return;
    }

    if ((int)thickness * 2 >= w || (int)thickness * 2 >= h) {
        display_fill_rect(x, y, w, h, gray);
        return;
    }

    display_fill_rect(x, y, w, thickness, gray);
    display_fill_rect(x, y + h - thickness, w, thickness, gray);
    display_fill_rect(x, y + thickness, thickness, h - ((int)thickness * 2), gray);
    display_fill_rect(x + w - thickness, y + thickness, thickness, h - ((int)thickness * 2), gray);
}

void display_draw_line(int x0, int y0, int x1, int y1, uint16_t thickness, uint8_t gray) {
    if (thickness == 0) {
        return;
    }

    const int radius = (int)(thickness - 1) / 2;
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        display_fill_rect(x0 - radius, y0 - radius, thickness, thickness, gray);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

bool display_damaged(display_rect_t* rect) {
    if (!s_damage.dirty) {
        return false;
    }

    if (rect) {
        rect->x = s_damage.x0;
        rect->y = s_damage.y0;
        rect->w = s_damage.x1 - s_damage.x0 + 1;
        rect->h = s_damage.y1 - s_damage.y0 + 1;
    }

    return true;
}

bool display_update(void) {
    display_rect_t rect;

    if (!display_damaged(&rect)) {
        ESP_LOGI(TAG, "No damage to update");
        return false;
    }

    ESP_LOGI(TAG, "Update damaged region x=%u y=%u w=%u h=%u", rect.x, rect.y, rect.w, rect.h);
    it8951_blit_8bpp_stride(rect.x, rect.y, rect.w, rect.h, s_framebuffer + ((size_t)rect.y * s_width + rect.x), s_width);
    s_damage.dirty = false;
    return true;
}
