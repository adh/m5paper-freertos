#include <inttypes.h>
#include <stdio.h>
#include "display.h"
#include "m5paper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "main";

typedef struct demo_box_s {
    int x;
    int y;
    int vx;
    int vy;
    int w;
    int h;
} demo_box_t;

static void draw_static_demo(void) {
    const uint16_t w = display_width();
    const uint16_t h = display_height();
    const int split_x = w / 2;

    ESP_LOGI(TAG, "Draw static demo");

    display_clear(0xF4);
    display_fill_rect(0, 0, split_x, h, 0xEE);
    display_stroke_rect(18, 18, split_x - 36, h - 36, 4, 0x20);
    display_fill_rect(36, 42, 80, 50, 0x10);
    display_fill_rect(132, 42, 120, 50, 0x90);
    display_stroke_rect(36, 112, 216, 96, 5, 0x40);
    display_stroke_rect(70, 146, 148, 28, 2, 0x00);
    display_draw_line(32, h - 180, split_x - 32, h - 180, 3, 0x10);
    display_draw_line(48, h - 72, split_x - 60, h - 220, 4, 0x70);
    display_draw_line(60, h - 220, split_x - 48, h - 72, 2, 0xA0);

    display_update();
}

static void draw_box(const demo_box_t* box, uint8_t fill, uint8_t stroke) {
    display_fill_rect(box->x, box->y, box->w, box->h, fill);
    display_stroke_rect(box->x, box->y, box->w, box->h, 4, stroke);
    display_draw_line(box->x, box->y, box->x + box->w - 1, box->y + box->h - 1, 2, stroke);
    display_draw_line(box->x + box->w - 1, box->y, box->x, box->y + box->h - 1, 2, stroke);
}

static void step_box(demo_box_t* box) {
    const int min_x = (int)(display_width() / 2) + 24;
    const int max_x = (int)display_width() - box->w - 24;
    const int min_y = 24;
    const int max_y = (int)display_height() - box->h - 24;

    box->x += box->vx;
    box->y += box->vy;

    if (box->x <= min_x || box->x >= max_x) {
        box->vx = -box->vx;
        if (box->x < min_x) {
            box->x = min_x;
        }
        if (box->x > max_x) {
            box->x = max_x;
        }
    }

    if (box->y <= min_y || box->y >= max_y) {
        box->vy = -box->vy;
        if (box->y < min_y) {
            box->y = min_y;
        }
        if (box->y > max_y) {
            box->y = max_y;
        }
    }
}

static void update_box_frame(uint32_t frame, demo_box_t* box) {
    ESP_LOGI(TAG, "Draw demo frame %" PRIu32, frame);

    display_fill_rect(box->x, box->y, box->w, box->h, 0xF4);
    step_box(box);
    draw_box(box, (uint8_t)(0x30 + ((frame % 5) * 0x18)), 0x00);
    display_update();
}

void app_main(void)
{
    ESP_LOGI(TAG, "hello world");

    m5paper_init();
    display_init(2300);
    draw_static_demo();
    uint32_t frame = 0;
    demo_box_t box = {
        .x = (int)(display_width() / 2) + 36,
        .y = 44,
        .vx = 26,
        .vy = 18,
        .w = 120,
        .h = 84,
    };

    draw_box(&box, 0x30, 0x00);
    display_update();

    while (1) {
        float battery_voltage = 0.0f;
        if (m5paper_battery_voltage(&battery_voltage)) {
            ESP_LOGI(TAG, "Battery voltage: %.3f V", battery_voltage);
        } else {
            ESP_LOGW(TAG, "Battery voltage read failed");
        }

        update_box_frame(frame++, &box);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }   

}
