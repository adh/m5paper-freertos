#include "m5paper.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task.h"

static const char* TAG = "m5paper";

#define M5STACK_MAIN_PWR_PIN 2
#define M5STACK_EXT_PWR_PIN 5
#define M5STACK_EPD_PWR_PIN 23

void m5paper_enable_main_power(){
    ESP_LOGI(TAG, "Enable main power");
    gpio_set_level(M5STACK_MAIN_PWR_PIN, 1);
}
void m5paper_enable_ext_power(){
    ESP_LOGI(TAG, "Enable ext power");
    gpio_set_level(M5STACK_EXT_PWR_PIN, 1);
}
void m5paper_enable_epd_power(){
    ESP_LOGI(TAG, "Enable ePD power");
    gpio_set_level(M5STACK_EPD_PWR_PIN, 1);
}

static void delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void m5paper_init(){
    ESP_LOGI(TAG, "Platform init");

    ESP_LOGI(TAG, "Configure power control GPIOs");
    gpio_config_t i_conf = {
        .pin_bit_mask = (1ull << M5STACK_MAIN_PWR_PIN) | (1ull << M5STACK_EXT_PWR_PIN) | (1ull << M5STACK_EPD_PWR_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&i_conf));

    m5paper_enable_main_power();
    m5paper_enable_ext_power();
    m5paper_enable_epd_power();
    delay(500);
}