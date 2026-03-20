#include "display.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"
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

typedef struct pixmap_size_s {
    int width;
    int height;
} pixmap_size_t;

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

static pixmap_size_t rotated_pixmap_size(const display_pixmap_t* pixmap, display_rotation_t rotation) {
    pixmap_size_t size = {
        .width = pixmap ? pixmap->width : 0,
        .height = pixmap ? pixmap->height : 0,
    };

    if (rotation == DISPLAY_ROTATE_90 || rotation == DISPLAY_ROTATE_270) {
        const int tmp = size.width;
        size.width = size.height;
        size.height = tmp;
    }

    return size;
}

static uint8_t pixmap_sample_rotated(const display_pixmap_t* pixmap, int rx, int ry, display_rotation_t rotation) {
    int src_x = rx;
    int src_y = ry;

    switch (rotation) {
        case DISPLAY_ROTATE_0:
            break;
        case DISPLAY_ROTATE_90:
            src_x = ry;
            src_y = pixmap->height - 1 - rx;
            break;
        case DISPLAY_ROTATE_180:
            src_x = pixmap->width - 1 - rx;
            src_y = pixmap->height - 1 - ry;
            break;
        case DISPLAY_ROTATE_270:
            src_x = pixmap->width - 1 - ry;
            src_y = rx;
            break;
        default:
            break;
    }

    return pixmap->pixels[(size_t)src_y * pixmap->width + src_x];
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

static void set_pixel_unchecked(int x, int y, uint8_t gray) {
    if (x < 0 || y < 0 || x >= s_width || y >= s_height) {
        return;
    }

    s_framebuffer[(size_t)y * s_width + x] = gray;
}

static void draw_glyph_pixel(int x, int y, int gx, int gy, display_rotation_t rotation, uint16_t scale, uint8_t gray) {
    int rx = gx;
    int ry = gy;

    switch (rotation) {
        case DISPLAY_ROTATE_0:
            break;
        case DISPLAY_ROTATE_90:
            rx = 15 - gy;
            ry = gx;
            break;
        case DISPLAY_ROTATE_180:
            rx = 7 - gx;
            ry = 15 - gy;
            break;
        case DISPLAY_ROTATE_270:
            rx = gy;
            ry = 7 - gx;
            break;
        default:
            break;
    }

    for (uint16_t sy = 0; sy < scale; ++sy) {
        for (uint16_t sx = 0; sx < scale; ++sx) {
            set_pixel_unchecked(x + rx * (int)scale + sx, y + ry * (int)scale + sy, gray);
        }
    }
}

static bool point_in_roundrect_local(int px, int py, int w, int h, int radius) {
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (px < 0 || py < 0 || px >= w || py >= h) {
        return false;
    }

    if (radius <= 0) {
        return true;
    }

    const int inner_left = radius;
    const int inner_right = w - radius;
    const int inner_top = radius;
    const int inner_bottom = h - radius;

    if ((px >= inner_left && px < inner_right) || (py >= inner_top && py < inner_bottom)) {
        return true;
    }

    const int corner_cx = px < inner_left ? radius - 1 : w - radius;
    const int corner_cy = py < inner_top ? radius - 1 : h - radius;
    const int dx2 = (px * 2 + 1) - (corner_cx * 2 + 1);
    const int dy2 = (py * 2 + 1) - (corner_cy * 2 + 1);
    const int dist4 = dx2 * dx2 + dy2 * dy2;
    const int radius4 = radius * radius * 4;

    return dist4 <= radius4;
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

void display_draw_roundrect(int x, int y, int w, int h, uint16_t radius, uint16_t thickness, uint8_t gray) {
    if (thickness == 0 || w <= 0 || h <= 0) {
        return;
    }

    const int max_radius = (w < h ? w : h) / 2;
    const int outer_radius = radius > (uint16_t)max_radius ? max_radius : (int)radius;
    const int inner_w = w - ((int)thickness * 2);
    const int inner_h = h - ((int)thickness * 2);

    if (outer_radius <= 0) {
        display_stroke_rect(x, y, w, h, thickness, gray);
        return;
    }

    if (inner_w <= 0 || inner_h <= 0) {
        for (int yy = 0; yy < h; ++yy) {
            for (int xx = 0; xx < w; ++xx) {
                if (point_in_roundrect_local(xx, yy, w, h, outer_radius)) {
                    set_pixel_unchecked(x + xx, y + yy, gray);
                }
            }
        }
        mark_damage(x, y, w, h);
        return;
    }

    int inner_radius = outer_radius - (int)thickness;
    const int inner_max_radius = (inner_w < inner_h ? inner_w : inner_h) / 2;
    if (inner_radius > inner_max_radius) {
        inner_radius = inner_max_radius;
    }

    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            if (!point_in_roundrect_local(xx, yy, w, h, outer_radius)) {
                continue;
            }

            if (point_in_roundrect_local(xx - (int)thickness, yy - (int)thickness, inner_w, inner_h, inner_radius)) {
                continue;
            }

            set_pixel_unchecked(x + xx, y + yy, gray);
        }
    }

    mark_damage(x, y, w, h);
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

void display_draw_ellipse(int cx, int cy, uint16_t rx, uint16_t ry, uint16_t thickness, uint8_t gray) {
    if (thickness == 0 || rx == 0 || ry == 0) {
        return;
    }

    const int inner_rx = (int)rx - (int)thickness;
    const int inner_ry = (int)ry - (int)thickness;
    const int64_t outer_bound = (int64_t)rx * rx * ry * ry * 4;
    const int64_t inner_bound = (inner_rx > 0 && inner_ry > 0)
        ? (int64_t)inner_rx * inner_rx * inner_ry * inner_ry * 4
        : -1;

    for (int y = -(int)ry; y <= (int)ry; ++y) {
        for (int x = -(int)rx; x <= (int)rx; ++x) {
            const int x2 = x * 2 + (x >= 0 ? 1 : -1);
            const int y2 = y * 2 + (y >= 0 ? 1 : -1);
            const int64_t outer = (int64_t)x2 * x2 * ry * ry + (int64_t)y2 * y2 * rx * rx;
            if (outer > outer_bound) {
                continue;
            }

            if (inner_bound >= 0) {
                const int64_t inner = (int64_t)x2 * x2 * inner_ry * inner_ry + (int64_t)y2 * y2 * inner_rx * inner_rx;
                if (inner < inner_bound) {
                    continue;
                }
            }

            set_pixel_unchecked(cx + x, cy + y, gray);
        }
    }

    mark_damage(cx - rx, cy - ry, rx * 2 + 1, ry * 2 + 1);
}

void display_draw_character(int x, int y, char c, display_rotation_t rotation, uint16_t scale, uint8_t gray) {
    if (scale == 0) {
        return;
    }

    const unsigned char* glyph = font8x16[(unsigned char)c];
    for (int gy = 0; gy < 16; ++gy) {
        const unsigned char row = glyph[gy];
        for (int gx = 0; gx < 8; ++gx) {
            if ((row & (0x80u >> gx)) == 0) {
                continue;
            }

            draw_glyph_pixel(x, y, gx, gy, rotation, scale, gray);
        }
    }

    if (rotation == DISPLAY_ROTATE_0 || rotation == DISPLAY_ROTATE_180) {
        mark_damage(x, y, 8 * scale, 16 * scale);
    } else {
        mark_damage(x, y, 16 * scale, 8 * scale);
    }
}

void display_draw_string(int x, int y, const char* text, display_rotation_t rotation, uint16_t scale, uint8_t gray) {
    if (!text || scale == 0) {
        return;
    }

    int cursor_x = x;
    int cursor_y = y;
    const int advance = 8 * (int)scale;

    while (*text) {
        display_draw_character(cursor_x, cursor_y, *text, rotation, scale, gray);

        switch (rotation) {
            case DISPLAY_ROTATE_0:
                cursor_x += advance;
                break;
            case DISPLAY_ROTATE_90:
                cursor_y += advance;
                break;
            case DISPLAY_ROTATE_180:
                cursor_x -= advance;
                break;
            case DISPLAY_ROTATE_270:
                cursor_y -= advance;
                break;
            default:
                cursor_x += advance;
                break;
        }

        ++text;
    }
}

void display_pixmap_free(display_pixmap_t* pixmap) {
    if (!pixmap) {
        return;
    }

    free(pixmap->pixels);
    pixmap->pixels = NULL;
    pixmap->width = 0;
    pixmap->height = 0;
}

bool display_pixmap_blit(int x, int y, const display_pixmap_t* pixmap, display_rotation_t rotation, uint16_t scale, int transparent_color) {
    if (!pixmap || !pixmap->pixels || pixmap->width == 0 || pixmap->height == 0 || scale == 0) {
        return false;
    }

    const pixmap_size_t rotated = rotated_pixmap_size(pixmap, rotation);
    bool touched = false;

    for (int ry = 0; ry < rotated.height; ++ry) {
        for (int rx = 0; rx < rotated.width; ++rx) {
            const uint8_t gray = pixmap_sample_rotated(pixmap, rx, ry, rotation);
            if (transparent_color >= 0 && gray == (uint8_t)transparent_color) {
                continue;
            }

            for (uint16_t sy = 0; sy < scale; ++sy) {
                const int dst_y = y + (ry * (int)scale) + sy;
                if (dst_y < 0 || dst_y >= s_height) {
                    continue;
                }

                for (uint16_t sx = 0; sx < scale; ++sx) {
                    const int dst_x = x + (rx * (int)scale) + sx;
                    if (dst_x < 0 || dst_x >= s_width) {
                        continue;
                    }

                    s_framebuffer[(size_t)dst_y * s_width + dst_x] = gray;
                    touched = true;
                }
            }
        }
    }

    if (touched) {
        mark_damage(x, y, rotated.width * scale, rotated.height * scale);
    }

    return touched;
}

bool display_pixmap_from_xbm3(display_pixmap_t* pixmap, const char* const* xpm) {
    xpm3_header_t header;
    if (!pixmap || !parse_xpm3_header(xpm, &header)) {
        return false;
    }

    xpm3_color_t* colors = calloc((size_t)header.colors, sizeof(*colors));
    uint8_t* pixels = malloc((size_t)header.width * header.height);
    if (!colors || !pixels) {
        free(colors);
        free(pixels);
        return false;
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

    for (int yy = 0; ok && yy < header.height; ++yy) {
        const char* row = xpm[1 + header.colors + yy];
        if (!row || (int)strlen(row) < header.width * header.cpp) {
            ok = false;
            break;
        }

        for (int xx = 0; xx < header.width; ++xx) {
            const xpm3_color_t* color = find_xpm3_color(colors, header.colors, row + ((size_t)xx * header.cpp), header.cpp);
            if (!color) {
                ok = false;
                break;
            }

            pixels[(size_t)yy * header.width + xx] = color->transparent ? 0xFF : color->gray;
        }
    }

    free(colors);

    if (!ok) {
        free(pixels);
        return false;
    }

    display_pixmap_free(pixmap);
    pixmap->width = (uint16_t)header.width;
    pixmap->height = (uint16_t)header.height;
    pixmap->pixels = pixels;
    return true;
}

bool display_pixmap_get(display_pixmap_t* pixmap, int x, int y, int w, int h) {
    if (!pixmap || !clip_rect(&x, &y, &w, &h)) {
        return false;
    }

    uint8_t* pixels = malloc((size_t)w * h);
    if (!pixels) {
        return false;
    }

    for (int yy = 0; yy < h; ++yy) {
        memcpy(&pixels[(size_t)yy * w], &s_framebuffer[(size_t)(y + yy) * s_width + x], (size_t)w);
    }

    display_pixmap_free(pixmap);
    pixmap->width = (uint16_t)w;
    pixmap->height = (uint16_t)h;
    pixmap->pixels = pixels;
    return true;
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
    display_pixmap_t pixmap = {0};
    if (!display_pixmap_from_xbm3(&pixmap, xpm)) {
        ESP_LOGW(TAG, "Invalid XPM3 pixmap");
        return;
    }

    display_pixmap_blit(x, y, &pixmap, DISPLAY_ROTATE_0, scale, 0xFF);
    display_pixmap_free(&pixmap);
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
