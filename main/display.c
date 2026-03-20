#include "display.h"

#include <ctype.h>
#include <stdio.h>
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

typedef struct xpm3_header_s {
    int width;
    int height;
    int colors;
    int cpp;
} xpm3_header_t;

typedef struct xpm3_color_s {
    const char* key;
    uint8_t gray;
    bool transparent;
} xpm3_color_t;

static bool parse_xpm3_header(const char* const* xpm, xpm3_header_t* header) {
    if (!xpm || !xpm[0] || !header) {
        return false;
    }

    return sscanf(xpm[0], "%d %d %d %d", &header->width, &header->height, &header->colors, &header->cpp) == 4 &&
           header->width > 0 && header->height > 0 && header->colors > 0 && header->cpp > 0;
}

static int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static bool parse_hex_component(const char* text, int digits, uint8_t* value) {
    int raw = 0;

    for (int i = 0; i < digits; ++i) {
        const int nibble = hex_digit_value(text[i]);
        if (nibble < 0) {
            return false;
        }
        raw = (raw << 4) | nibble;
    }

    const int max = (1 << (digits * 4)) - 1;
    *value = (uint8_t)((raw * 255 + (max / 2)) / max);
    return true;
}

static bool parse_xpm3_color_value(const char* value, uint8_t* gray, bool* transparent) {
    if (!value || !gray || !transparent) {
        return false;
    }

    if (strcasecmp(value, "none") == 0) {
        *transparent = true;
        *gray = 0xFF;
        return true;
    }

    *transparent = false;

    if (value[0] == '#') {
        const size_t digits = strlen(value + 1);
        if (digits == 0 || (digits % 3) != 0 || digits > 12) {
            return false;
        }

        const int per_channel = (int)(digits / 3);
        uint8_t r;
        uint8_t g;
        uint8_t b;

        if (!parse_hex_component(value + 1, per_channel, &r) ||
            !parse_hex_component(value + 1 + per_channel, per_channel, &g) ||
            !parse_hex_component(value + 1 + (per_channel * 2), per_channel, &b)) {
            return false;
        }

        *gray = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
        return true;
    }

    if (strcasecmp(value, "black") == 0) {
        *gray = 0x00;
        return true;
    }
    if (strcasecmp(value, "white") == 0) {
        *gray = 0xFF;
        return true;
    }
    if (strcasecmp(value, "gray") == 0 || strcasecmp(value, "grey") == 0) {
        *gray = 0x80;
        return true;
    }

    if (strncasecmp(value, "gray", 4) == 0 || strncasecmp(value, "grey", 4) == 0) {
        char* end = NULL;
        long percent = strtol(value + 4, &end, 10);
        if (end != value + 4 && *end == '\0' && percent >= 0 && percent <= 100) {
            *gray = (uint8_t)((percent * 255) / 100);
            return true;
        }
    }

    return false;
}

static const char* find_xpm3_color_token(const char* line) {
    const char* p = line;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (!*p) {
            break;
        }

        const char* key = p;
        while (*p && !isspace((unsigned char)*p)) {
            ++p;
        }
        const size_t key_len = (size_t)(p - key);

        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }

        if (key_len == 1 && (*key == 'c' || *key == 'g' || *key == 'm')) {
            return p;
        }

        while (*p && !isspace((unsigned char)*p)) {
            ++p;
        }
    }

    return NULL;
}

static const xpm3_color_t* find_xpm3_color(const xpm3_color_t* colors, int count, const char* key, int cpp) {
    for (int i = 0; i < count; ++i) {
        if (strncmp(colors[i].key, key, (size_t)cpp) == 0) {
            return &colors[i];
        }
    }

    return NULL;
}

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

uint16_t display_xpm3_width(const char* const* xpm) {
    xpm3_header_t header;
    return parse_xpm3_header(xpm, &header) ? (uint16_t)header.width : 0;
}

uint16_t display_xpm3_height(const char* const* xpm) {
    xpm3_header_t header;
    return parse_xpm3_header(xpm, &header) ? (uint16_t)header.height : 0;
}

void display_xpm3_draw(int x, int y, const char* const* xpm) {
    display_xpm3_draw_scaled(x, y, xpm, 1);
}

void display_xpm3_draw_scaled(int x, int y, const char* const* xpm, uint16_t scale) {
    xpm3_header_t header;
    if (scale == 0 || !parse_xpm3_header(xpm, &header)) {
        ESP_LOGW(TAG, "Invalid XPM3 header");
        return;
    }

    xpm3_color_t* colors = calloc((size_t)header.colors, sizeof(*colors));
    if (!colors) {
        ESP_LOGE(TAG, "Failed to allocate XPM3 color table");
        return;
    }

    bool ok = true;
    for (int i = 0; i < header.colors; ++i) {
        const char* line = xpm[1 + i];
        if (!line || (int)strlen(line) < header.cpp) {
            ok = false;
            break;
        }

        colors[i].key = line;

        const char* value = find_xpm3_color_token(line + header.cpp);
        if (!parse_xpm3_color_value(value, &colors[i].gray, &colors[i].transparent)) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        ESP_LOGW(TAG, "Invalid XPM3 palette");
        free(colors);
        return;
    }

    bool touched = false;
    for (int yy = 0; yy < header.height; ++yy) {
        const char* row = xpm[1 + header.colors + yy];
        if (!row) {
            break;
        }

        for (int xx = 0; xx < header.width; ++xx) {
            const int dst_x = x + xx;
            const int dst_y = y + yy;
            if (dst_x < 0 || dst_y < 0 || dst_x >= s_width || dst_y >= s_height) {
                continue;
            }

            const char* key = row + ((size_t)xx * header.cpp);
            const xpm3_color_t* color = find_xpm3_color(colors, header.colors, key, header.cpp);
            if (!color || color->transparent) {
                continue;
            }

            for (uint16_t sy = 0; sy < scale; ++sy) {
                const int scaled_y = y + (yy * (int)scale) + sy;
                if (scaled_y < 0 || scaled_y >= s_height) {
                    continue;
                }

                for (uint16_t sx = 0; sx < scale; ++sx) {
                    const int scaled_x = x + (xx * (int)scale) + sx;
                    if (scaled_x < 0 || scaled_x >= s_width) {
                        continue;
                    }

                    s_framebuffer[(size_t)scaled_y * s_width + scaled_x] = color->gray;
                    touched = true;
                }
            }
        }
    }

    if (touched) {
        mark_damage(x, y, header.width * scale, header.height * scale);
    }

    free(colors);
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
