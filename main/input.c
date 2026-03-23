#include "input.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "input";

#define INPUT_TOUCH_INTERRUPT_PIN GPIO_NUM_36
#define INPUT_BUTTON_UP_PIN GPIO_NUM_37
#define INPUT_BUTTON_CENTER_PIN GPIO_NUM_38
#define INPUT_BUTTON_DOWN_PIN GPIO_NUM_39
static bool s_initialized;
static gt911_touch_t s_last_touch;
static uint8_t s_last_button_mask;

static const gpio_num_t s_button_pins[] = {
    INPUT_BUTTON_UP_PIN,
    INPUT_BUTTON_CENTER_PIN,
    INPUT_BUTTON_DOWN_PIN,
};

static uint8_t input_button_mask_read(void) {
    uint8_t mask = 0;

    for (size_t i = 0; i < sizeof(s_button_pins) / sizeof(s_button_pins[0]); ++i) {
        if (gpio_get_level(s_button_pins[i]) == 0) {
            mask |= (uint8_t)(1u << i);
        }
    }

    return mask;
}

uint8_t input_button_mask(void) {
    return input_button_mask_read();
}

static bool input_touch_same_state(const gt911_touch_t* a, const gt911_touch_t* b) {
    if (a->touched != b->touched) {
        return false;
    }

    if (!a->touched) {
        return true;
    }

    return a->x == b->x &&
           a->y == b->y &&
           a->points == b->points;
}

static bool input_touch_release_resolved(gt911_touch_t* touch) {
    const TickType_t sample_delay = pdMS_TO_TICKS(15);
    const int sample_count = 3;

    for (int i = 0; i < sample_count; ++i) {
        vTaskDelay(sample_delay);
        if (gt911_get_touch(touch) != ESP_OK) {
            return false;
        }
        if (touch->touched) {
            return false;
        }
    }

    return true;
}

static bool input_pop_touch_change(input_event_t* event) {
    gt911_touch_t touch;
    if (gt911_get_touch(&touch) != ESP_OK) {
        return false;
    }

    if (!touch.touched && s_last_touch.touched) {
        if (!input_touch_release_resolved(&touch)) {
            if (touch.touched && !input_touch_same_state(&touch, &s_last_touch)) {
                s_last_touch = touch;
                memset(event, 0, sizeof(*event));
                event->type = INPUT_EVENT_TOUCH_MOVE;
                event->touch = touch;
                return true;
            }
            return false;
        }
    }

    if (input_touch_same_state(&touch, &s_last_touch)) {
        return false;
    }

    const bool was_touched = s_last_touch.touched;
    s_last_touch = touch;
    memset(event, 0, sizeof(*event));
    if (touch.touched) {
        event->type = was_touched ? INPUT_EVENT_TOUCH_MOVE : INPUT_EVENT_TOUCH_PRESS;
    } else {
        event->type = INPUT_EVENT_TOUCH_RELEASE;
    }
    event->touch = touch;
    return true;
}

static bool input_pop_button_change(input_event_t* event) {
    const uint8_t current_mask = input_button_mask_read();
    const uint8_t changed_mask = current_mask ^ s_last_button_mask;

    if (changed_mask == 0) {
        return false;
    }

    const uint8_t bit = (uint8_t)(changed_mask & (uint8_t)(-((int8_t)changed_mask)));
    input_button_t button = INPUT_BUTTON_UP;
    if (bit == (1u << 1)) {
        button = INPUT_BUTTON_CENTER;
    } else if (bit == (1u << 2)) {
        button = INPUT_BUTTON_DOWN;
    }

    s_last_button_mask = current_mask;

    memset(event, 0, sizeof(*event));
    event->type = INPUT_EVENT_DIRECTIONAL_BUTTON;
    event->button.button = button;
    event->button.pressed = (current_mask & bit) != 0;
    event->button.pressed_mask = current_mask;
    return true;
}

static bool input_pop_pending_event(input_event_t* event) {
    return input_pop_touch_change(event) || input_pop_button_change(event);
}

static void input_enable_sleep_wakeup(void) {
    ESP_ERROR_CHECK(gpio_wakeup_enable(INPUT_TOUCH_INTERRUPT_PIN, GPIO_INTR_LOW_LEVEL));
    for (size_t i = 0; i < sizeof(s_button_pins) / sizeof(s_button_pins[0]); ++i) {
        ESP_ERROR_CHECK(gpio_wakeup_enable(s_button_pins[i], GPIO_INTR_LOW_LEVEL));
    }
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
}

esp_err_t input_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(gt911_init(), TAG, "initialize gt911");

    const gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << INPUT_BUTTON_UP_PIN) | (1ULL << INPUT_BUTTON_CENTER_PIN) | (1ULL << INPUT_BUTTON_DOWN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&button_config), TAG, "configure buttons");

    for (size_t i = 0; i < sizeof(s_button_pins) / sizeof(s_button_pins[0]); ++i) {
        ESP_RETURN_ON_ERROR(rtc_gpio_pullup_en(s_button_pins[i]), TAG, "enable button rtc pullup");
        ESP_RETURN_ON_ERROR(rtc_gpio_pulldown_dis(s_button_pins[i]), TAG, "disable button rtc pulldown");
    }

    memset(&s_last_touch, 0, sizeof(s_last_touch));
    gt911_get_touch(&s_last_touch);
    s_last_button_mask = input_button_mask_read();
    input_enable_sleep_wakeup();
    s_initialized = true;

    ESP_LOGI(TAG, "Input initialized");
    return ESP_OK;
}

esp_err_t input_wait_for_event_or_timeout(input_event_t* event, uint32_t timeout_ms) {
    ESP_RETURN_ON_FALSE(event != NULL, ESP_ERR_INVALID_ARG, TAG, "event buffer is null");
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "input not initialized");

    memset(event, 0, sizeof(*event));

    if (input_pop_pending_event(event)) {
        return ESP_OK;
    }

    if (timeout_ms == 0) {
        event->type = INPUT_EVENT_TIMEOUT;
        return ESP_OK;
    }

    const bool wait_forever = timeout_ms == UINT32_MAX;
    int64_t remaining_us = wait_forever ? -1 : ((int64_t)timeout_ms * 1000);

    while (1) {
        if (!wait_forever && remaining_us <= 0) {
            event->type = INPUT_EVENT_TIMEOUT;
            return ESP_OK;
        }

        if (wait_forever) {
            const esp_err_t err = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                return err;
            }
        } else {
            const int64_t sleep_us = remaining_us;
            ESP_RETURN_ON_ERROR(esp_sleep_enable_timer_wakeup((uint64_t)sleep_us), TAG, "enable timeout wakeup");
        }

        const int64_t start_us = esp_timer_get_time();
        ESP_RETURN_ON_ERROR(esp_light_sleep_start(), TAG, "enter light sleep");
        const int64_t elapsed_us = esp_timer_get_time() - start_us;

        if (!wait_forever) {
            remaining_us -= elapsed_us;
        }

        if (input_pop_pending_event(event)) {
            return ESP_OK;
        }

        if (!wait_forever && remaining_us <= 0) {
            event->type = INPUT_EVENT_TIMEOUT;
            return ESP_OK;
        }
    }
}
