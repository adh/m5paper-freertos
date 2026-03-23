#ifndef H__input__
#define H__input__

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "gt911.h"

typedef enum input_event_type_e {
    INPUT_EVENT_TIMEOUT = 0,
    INPUT_EVENT_TOUCH_CHANGE,
    INPUT_EVENT_DIRECTIONAL_BUTTON,
} input_event_type_t;

typedef enum input_button_e {
    INPUT_BUTTON_UP = 0,
    INPUT_BUTTON_CENTER,
    INPUT_BUTTON_DOWN,
} input_button_t;

typedef struct input_button_event_s {
    input_button_t button;
    bool pressed;
    uint8_t pressed_mask;
} input_button_event_t;

typedef struct input_event_s {
    input_event_type_t type;
    union {
        gt911_touch_t touch;
        input_button_event_t button;
    };
} input_event_t;

esp_err_t input_init(void);
esp_err_t input_wait_for_event_or_timeout(input_event_t* event, uint32_t timeout_ms);
uint8_t input_button_mask(void);

#endif
