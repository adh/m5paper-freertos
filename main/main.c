#include <stdio.h>
#include <string.h>
#include "it8951.h"
#include "m5paper.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "main";


static void draw_orientation_pattern(void) {
    const uint16_t w = it8951_width();
    const uint16_t h = it8951_height();
    uint8_t* pixels = it8951_framebuffer();

    ESP_LOGI(TAG, "Draw orientation pattern");

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
}

static void draw_solid_color(uint8_t gray) {
    const uint16_t w = it8951_width();
    const uint16_t h = it8951_height();

    ESP_LOGI(TAG, "Draw solid color: %02X", gray);

    it8951_fill_rect(0, 0, w, h, gray);
}

static void draw_pixels() {
        const uint16_t w = it8951_width();
        const uint16_t h = it8951_height();
        uint8_t* pixels = it8951_framebuffer();
    
        ESP_LOGI(TAG, "Draw pixels");
    
        for (uint16_t y = 0; y < h; ++y) {
            for (uint16_t x = 0; x < w; ++x) {
                pixels[(size_t)y * w + x] = (uint8_t)(((x + y) / 2 * 255u) / ((w + h) / 2 - 1));
            }
        }
    
        it8951_blit_8bpp(0, 0, w, h, pixels);
}

static void draw_mandelbrot() {
    const uint16_t w = it8951_width();
    const uint16_t h = it8951_height();
    uint8_t* pixels = it8951_framebuffer();

    ESP_LOGI(TAG, "Draw Mandelbrot set");

    it8951_fill_rect(0, 0, w, h, 0xff);
    it8951_fill_rect(210, 250, h + 20, 40, 0x80);

    for (uint16_t y = 0; y < h; ++y) {
        ESP_LOGI(TAG, "Drawing line %d/%d", y + 1, h);
        for (uint16_t x = 0; x < w; ++x) {
            float zx = 0.0;
            float zy = 0.0;
            float cx = ((float)x / w) * 3.5f - 2.5f;
            float cy = ((float)y / h) * 2.0f - 1.0f;

            uint8_t iter = 0;
            while (zx * zx + zy * zy < 4.0f && iter < 255) {
                float tmp = zx * zx - zy * zy + cx;
                zy = 2.0f * zx * zy + cy;
                zx = tmp;
                iter++;
            }

            pixels[(size_t)y * w + x] = iter;
        }
        if (y % 40 == 0) {
            it8951_fill_rect(210 + y, 250, 40, 40, 0x00);
        }

    }

    it8951_blit_8bpp(0, 0, w, h, pixels);
}

void app_main(void)
{
    ESP_LOGI(TAG, "hello world");

    m5paper_init();
    it8951_init(2300);
    while (1) {
        float battery_voltage = 0.0f;
        if (m5paper_battery_voltage(&battery_voltage)) {
            ESP_LOGI(TAG, "Battery voltage: %.3f V", battery_voltage);
        } else {
            ESP_LOGW(TAG, "Battery voltage read failed");
        }

        draw_orientation_pattern();
        vTaskDelay(pdMS_TO_TICKS(2000));
        draw_solid_color(0x80);
        vTaskDelay(pdMS_TO_TICKS(2000));
        draw_pixels();
        vTaskDelay(pdMS_TO_TICKS(2000));
        draw_mandelbrot();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }   

}
