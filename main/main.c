#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "it8951.h"
#include "m5paper.h"

static void draw_orientation_pattern(void) {
    it8951_device_info_t info = {0};
    it8951_get_system_info(&info);

    const uint16_t w = info.width;
    const uint16_t h = info.height;
    uint8_t* pixels = heap_caps_malloc((size_t)w * h, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pixels) {
        puts("framebuffer allocation failed");
        return;
    }

    memset(pixels, 0xFF, (size_t)w * h);

    for (uint16_t y = 0; y < h; ++y) {
        for (uint16_t x = 0; x < w; ++x) {
            pixels[(size_t)y * w + x] = (uint8_t)((x * 255u) / (w - 1));
        }
    }

    const uint16_t margin = 20;
    const uint16_t corner = 90;
    const uint16_t axis = 24;
    const uint16_t step = 36;

    for (uint16_t yy = margin; yy < margin + corner; ++yy) {
        for (uint16_t xx = margin; xx < margin + corner; ++xx) {
            pixels[(size_t)yy * w + xx] = 0x00;
        }
    }
    for (uint16_t yy = margin; yy < margin + corner; ++yy) {
        for (uint16_t xx = w - margin - corner; xx < w - margin; ++xx) {
            pixels[(size_t)yy * w + xx] = 0x55;
        }
    }
    for (uint16_t yy = h - margin - corner; yy < h - margin; ++yy) {
        for (uint16_t xx = margin; xx < margin + corner; ++xx) {
            pixels[(size_t)yy * w + xx] = 0xAA;
        }
    }
    for (uint16_t yy = h - margin - corner; yy < h - margin; ++yy) {
        for (uint16_t xx = w - margin - corner; xx < w - margin; ++xx) {
            pixels[(size_t)yy * w + xx] = 0xCC;
        }
    }

    for (uint16_t yy = margin + corner + 10; yy < margin + corner + 10 + axis; ++yy) {
        for (uint16_t xx = margin; xx < margin + (w / 3); ++xx) {
            pixels[(size_t)yy * w + xx] = 0x00;
        }
    }
    for (uint16_t yy = margin; yy < margin + (h / 3); ++yy) {
        for (uint16_t xx = margin + corner + 10; xx < margin + corner + 10 + axis; ++xx) {
            pixels[(size_t)yy * w + xx] = 0x00;
        }
    }

    for (uint16_t i = 0; i < 5; ++i) {
        uint16_t left = margin + 140 + (i * step);
        uint16_t top = margin + corner + 50 + (i * 10);
        uint8_t gray = (uint8_t)(0x20 + (i * 0x20));

        for (uint16_t yy = top; yy < top + 24; ++yy) {
            for (uint16_t xx = left; xx < left + 24; ++xx) {
                pixels[(size_t)yy * w + xx] = gray;
            }
        }
    }

    for (uint16_t i = 0; i < 5; ++i) {
        uint16_t left = margin + corner + 50 + (i * 10);
        uint16_t top = margin + 140 + (i * step);
        uint8_t gray = (uint8_t)(0x20 + (i * 0x20));

        for (uint16_t yy = top; yy < top + 24; ++yy) {
            for (uint16_t xx = left; xx < left + 24; ++xx) {
                pixels[(size_t)yy * w + xx] = gray;
            }
        }
    }

    for (uint16_t yy = (h / 2) - 30; yy < (h / 2) + 30; ++yy) {
        for (uint16_t xx = (w / 2) - 110; xx < (w / 2) + 110; ++xx) {
            pixels[(size_t)yy * w + xx] = 0x00;
        }
    }
    for (uint16_t yy = (h / 2) - 110; yy < (h / 2) + 110; ++yy) {
        for (uint16_t xx = (w / 2) - 20; xx < (w / 2) + 20; ++xx) {
            pixels[(size_t)yy * w + xx] = 0x77;
        }
    }

    it8951_blit_8bpp(0, 0, w, h, pixels);
    free(pixels);
}

void app_main(void)
{
    puts("hello world");

    m5paper_init();
    it8951_init(2300);
    draw_orientation_pattern();

}
