#include "m5paper.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task.h"

static const char* TAG = "m5paper";

#define M5STACK_MAIN_PWR_PIN 2
#define M5STACK_EXT_PWR_PIN 5
#define M5STACK_EPD_PWR_PIN 23
#define M5STACK_BATTERY_ADC_CHANNEL ADC1_CHANNEL_7
#define M5STACK_BATTERY_ADC_ATTEN ADC_ATTEN_DB_11
#define M5STACK_BATTERY_ADC_WIDTH ADC_WIDTH_BIT_12
#define M5STACK_BATTERY_DIVIDER_RATIO 2.0f

static esp_adc_cal_characteristics_t s_battery_adc_chars;
static bool s_battery_adc_ready;

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

static void m5paper_battery_init(void) {
    ESP_LOGI(TAG, "Configure battery ADC");

    ESP_ERROR_CHECK(adc1_config_width(M5STACK_BATTERY_ADC_WIDTH));
    ESP_ERROR_CHECK(adc1_config_channel_atten(M5STACK_BATTERY_ADC_CHANNEL, M5STACK_BATTERY_ADC_ATTEN));

    esp_adc_cal_value_t cal_source = esp_adc_cal_characterize(
        ADC_UNIT_1,
        M5STACK_BATTERY_ADC_ATTEN,
        M5STACK_BATTERY_ADC_WIDTH,
        1100,
        &s_battery_adc_chars);

    s_battery_adc_ready = true;
    ESP_LOGI(TAG, "Battery ADC ready (calibration source: %d)", cal_source);
}

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
    m5paper_battery_init();
    delay(500);
}

bool m5paper_battery_voltage(float* voltage) {
    if (!s_battery_adc_ready || voltage == NULL) {
        return false;
    }

    const int raw = adc1_get_raw(M5STACK_BATTERY_ADC_CHANNEL);
    if (raw < 0) {
        ESP_LOGW(TAG, "Battery ADC read failed: %d", raw);
        return false;
    }

    const uint32_t pin_mv = esp_adc_cal_raw_to_voltage((uint32_t)raw, &s_battery_adc_chars);
    *voltage = ((float)pin_mv * M5STACK_BATTERY_DIVIDER_RATIO) / 1000.0f;
    return true;
}
