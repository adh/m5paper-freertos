#include <stdint.h>

#include "demo.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "input.h"
#include "m5paper.h"
#include "widget.h"

static const char* TAG = "main";
static widget_window_t s_root_window;

void app_main(void) {
    ESP_LOGI(TAG, "M5Paper widget demos");
    m5paper_init();
    display_init(2300);
    display_set_rotation(DISPLAY_ROTATE_270);
    demo_init(&s_root_window);

    const esp_err_t input_err = input_init();
    if (input_err != ESP_OK) {
        ESP_LOGW(TAG, "Input init failed: %s", esp_err_to_name(input_err));
    }

    while (1) {
        input_event_t event;
        const esp_err_t event_err = input_wait_for_event_or_timeout(
            &event, demo_event_timeout_ms());
        if (event_err != ESP_OK) {
            ESP_LOGW(TAG, "Input wait failed: %s", esp_err_to_name(event_err));
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        widget_dispatch_event(&s_root_window, &event);
    }
}
