#include <inttypes.h>
#include <stdio.h>
#include "display.h"
#include "m5paper.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "main";
static const uint16_t TEST_SPRITE_SCALE = 8;

typedef struct demo_box_s {
    int x;
    int y;
    int vx;
    int vy;
    int w;
    int h;
} demo_sprite_t;

static display_pixmap_t s_test_sprite;


static const char* const s_test_pixmap[] = {
    "24 20 4 1",
    ". c None",
    "X c #000000",
    "o c #666666",
    "+ c #DDDDDD",
    "........................",
    "..........XXXX..........",
    "........XXXXXXXX........",
    ".......XXooooooXX.......",
    "......XXo++++++oXX......",
    ".....XXo++XXXX++oXX.....",
    "....XXo++XXXXXX++oXX....",
    "...XXo++XX++++XX++oXX...",
    "...XXo++XX++++XX++oXX...",
    "...XXo++XXXXXXXX++oXX...",
    "...XXo++++++++++++oXX...",
    "....XXo++XXXXXX++oXX....",
    ".....XXo++XXXX++oXX.....",
    "......XXo++++++oXX......",
    ".......XXooooooXX.......",
    "........XXXXXXXX........",
    ".......XX++XX++XX.......",
    "......XX++XXXX++XX......",
    "......XX++X..X++XX......",
    "......XXXXXXXXXXXX......",
    NULL,
};

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
    display_draw_roundrect(278, 42, 140, 86, 18, 5, 0x30);
    display_draw_ellipse(348, 176, 62, 36, 4, 0x70);
    display_draw_line(32, h - 180, split_x - 32, h - 180, 3, 0x10);
    display_draw_line(48, h - 72, split_x - 60, h - 220, 4, 0x70);
    display_draw_line(60, h - 220, split_x - 48, h - 72, 2, 0xA0);

    display_update();
}

static void draw_sprite(const demo_sprite_t* sprite) {
    display_pixmap_blit(sprite->x, sprite->y, &s_test_sprite, DISPLAY_ROTATE_0, TEST_SPRITE_SCALE, 0xFF);
}

static void step_sprite(demo_sprite_t* sprite) {
    const int min_x = (int)(display_width() / 2) + 24;
    const int max_x = (int)display_width() - sprite->w - 24;
    const int min_y = 24;
    const int max_y = (int)display_height() - sprite->h - 24;

    sprite->x += sprite->vx;
    sprite->y += sprite->vy;

    if (sprite->x <= min_x || sprite->x >= max_x) {
        sprite->vx = -sprite->vx;
        if (sprite->x < min_x) {
            sprite->x = min_x;
        }
        if (sprite->x > max_x) {
            sprite->x = max_x;
        }
    }

    if (sprite->y <= min_y || sprite->y >= max_y) {
        sprite->vy = -sprite->vy;
        if (sprite->y < min_y) {
            sprite->y = min_y;
        }
        if (sprite->y > max_y) {
            sprite->y = max_y;
        }
    }
}

static void update_sprite_frame(uint32_t frame, demo_sprite_t* sprite) {
    ESP_LOGI(TAG, "Draw demo frame %" PRIu32, frame);

    display_fill_rect(sprite->x, sprite->y, sprite->w, sprite->h, 0xF4);
    step_sprite(sprite);
    draw_sprite(sprite);
    display_update();
}

void app_main(void)
{
    ESP_LOGI(TAG, "hello world");

    m5paper_init();
    display_init(2300);
    draw_static_demo();
    if (!display_pixmap_from_xbm3(&s_test_sprite, s_test_pixmap)) {
        ESP_LOGE(TAG, "Failed to decode test pixmap");
        return;
    }
    uint32_t frame = 0;
    demo_sprite_t sprite = {
        .x = (int)(display_width() / 2) + 36,
        .y = 44,
        .vx = 26,
        .vy = 18,
        .w = s_test_sprite.width * TEST_SPRITE_SCALE,
        .h = s_test_sprite.height * TEST_SPRITE_SCALE,
    };

    draw_sprite(&sprite);
    display_update();

    while (1) {
        float battery_voltage = 0.0f;
        if (m5paper_battery_voltage(&battery_voltage)) {
            ESP_LOGI(TAG, "Battery voltage: %.3f V", battery_voltage);
        } else {
            ESP_LOGW(TAG, "Battery voltage read failed");
        }

        update_sprite_frame(frame++, &sprite);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }   

}
