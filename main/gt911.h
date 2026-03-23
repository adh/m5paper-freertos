#ifndef H__gt911__
#define H__gt911__

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct gt911_touch_s {
    bool touched;
    uint8_t points;
    uint8_t track_id;
    uint16_t x;
    uint16_t y;
    uint16_t size;
} gt911_touch_t;

esp_err_t gt911_init(void);
esp_err_t gt911_get_touch(gt911_touch_t* touch);
esp_err_t gt911_interrupt_init(gpio_isr_t isr_handler, void* arg);
esp_err_t gt911_interrupt_deinit(void);

#endif
