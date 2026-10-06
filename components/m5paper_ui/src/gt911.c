#include "gt911.h"

#include <string.h>

#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"

static const char* TAG = "gt911";

#define GT911_I2C_PORT I2C_NUM_0
#define GT911_I2C_SDA_PIN 21
#define GT911_I2C_SCL_PIN 22
#define GT911_INTERRUPT_PIN 36
#define GT911_I2C_CLOCK_HZ 400000
#define GT911_I2C_TIMEOUT_MS 100

#define GT911_REG_PRODUCT_ID 0x8140
#define GT911_REG_STATUS 0x814E
#define GT911_REG_POINT1 0x8150

static const uint8_t s_candidate_addresses[] = {0x14, 0x5D};

static bool s_i2c_ready;
static uint8_t s_i2c_address;
static bool s_interrupt_ready;
static bool s_installed_isr_service;

static TickType_t gt911_timeout_ticks(void) {
    return pdMS_TO_TICKS(GT911_I2C_TIMEOUT_MS);
}

static esp_err_t gt911_read(uint16_t reg, void* data, size_t len) {
    uint8_t reg_buf[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF),
    };

    return i2c_master_write_read_device(
        GT911_I2C_PORT,
        s_i2c_address,
        reg_buf,
        sizeof(reg_buf),
        data,
        len,
        gt911_timeout_ticks());
}

static esp_err_t gt911_write(uint16_t reg, const void* data, size_t len) {
    uint8_t buffer[18];

    if (len > (sizeof(buffer) - 2)) {
        return ESP_ERR_INVALID_SIZE;
    }

    buffer[0] = (uint8_t)(reg >> 8);
    buffer[1] = (uint8_t)(reg & 0xFF);
    if (len > 0 && data != NULL) {
        memcpy(&buffer[2], data, len);
    }

    return i2c_master_write_to_device(
        GT911_I2C_PORT,
        s_i2c_address,
        buffer,
        len + 2,
        gt911_timeout_ticks());
}

static esp_err_t gt911_probe_address(uint8_t address) {
    uint8_t product_id[4];
    esp_err_t err;

    s_i2c_address = address;
    err = gt911_read(GT911_REG_PRODUCT_ID, product_id, sizeof(product_id));
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "Detected GT911 at 0x%02X, product id %.4s", address, product_id);
    return ESP_OK;
}

esp_err_t gt911_init(void) {
    if (s_i2c_ready) {
        return ESP_OK;
    }

    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GT911_I2C_SDA_PIN,
        .scl_io_num = GT911_I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = GT911_I2C_CLOCK_HZ,
        .clk_flags = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(GT911_I2C_PORT, &config), TAG, "configure i2c");
    ESP_RETURN_ON_ERROR(i2c_driver_install(GT911_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0), TAG, "install i2c driver");

    esp_err_t last_error = ESP_FAIL;
    for (size_t i = 0; i < sizeof(s_candidate_addresses); ++i) {
        const esp_err_t err = gt911_probe_address(s_candidate_addresses[i]);
        if (err == ESP_OK) {
            s_i2c_ready = true;
            break;
        }
        last_error = err;
    }

    if (!s_i2c_ready) {
        i2c_driver_delete(GT911_I2C_PORT);
        s_i2c_address = 0;
        ESP_LOGE(TAG, "Failed to detect GT911 on internal i2c bus");
        return last_error;
    }

    const gpio_config_t interrupt_config = {
        .pin_bit_mask = 1ULL << GT911_INTERRUPT_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&interrupt_config), TAG, "configure interrupt pin");
    return ESP_OK;
}

esp_err_t gt911_get_touch(gt911_touch_t* touch) {
    ESP_RETURN_ON_FALSE(touch != NULL, ESP_ERR_INVALID_ARG, TAG, "touch buffer is null");
    ESP_RETURN_ON_FALSE(s_i2c_ready, ESP_ERR_INVALID_STATE, TAG, "driver not initialized");

    memset(touch, 0, sizeof(*touch));

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(gt911_read(GT911_REG_STATUS, &status, sizeof(status)), TAG, "read status");

    if ((status & 0x80) == 0) {
        return ESP_OK;
    }

    touch->points = status & 0x0F;
    if (touch->points == 0) {
        const uint8_t clear = 0;
        return gt911_write(GT911_REG_STATUS, &clear, sizeof(clear));
    }

    uint8_t point_data[8];
    esp_err_t err = gt911_read(GT911_REG_POINT1, point_data, sizeof(point_data));
    if (err != ESP_OK) {
        return err;
    }

    touch->touched = true;
    touch->track_id = 0;
    touch->x = (uint16_t)point_data[0] | ((uint16_t)point_data[1] << 8);
    touch->y = (uint16_t)point_data[2] | ((uint16_t)point_data[3] << 8);
    touch->size = (uint16_t)point_data[4] | ((uint16_t)point_data[5] << 8);

    const uint8_t clear = 0;
    return gt911_write(GT911_REG_STATUS, &clear, sizeof(clear));
}

esp_err_t gt911_interrupt_init(gpio_isr_t isr_handler, void* arg) {
    ESP_RETURN_ON_FALSE(isr_handler != NULL, ESP_ERR_INVALID_ARG, TAG, "isr handler is null");
    ESP_RETURN_ON_FALSE(s_i2c_ready, ESP_ERR_INVALID_STATE, TAG, "driver not initialized");

    if (!s_installed_isr_service) {
        const esp_err_t err = gpio_install_isr_service(0);
        if (err == ESP_OK) {
            s_installed_isr_service = true;
        } else if (err != ESP_ERR_INVALID_STATE) {
            return err;
        }
    }

    ESP_RETURN_ON_ERROR(gpio_set_intr_type((gpio_num_t)GT911_INTERRUPT_PIN, GPIO_INTR_NEGEDGE), TAG, "set interrupt type");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add((gpio_num_t)GT911_INTERRUPT_PIN, isr_handler, arg), TAG, "add interrupt handler");
    ESP_RETURN_ON_ERROR(gpio_intr_enable((gpio_num_t)GT911_INTERRUPT_PIN), TAG, "enable interrupt");

    s_interrupt_ready = true;
    return ESP_OK;
}

esp_err_t gt911_interrupt_deinit(void) {
    if (!s_interrupt_ready) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(gpio_intr_disable((gpio_num_t)GT911_INTERRUPT_PIN), TAG, "disable interrupt");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_remove((gpio_num_t)GT911_INTERRUPT_PIN), TAG, "remove interrupt handler");
    ESP_RETURN_ON_ERROR(gpio_set_intr_type((gpio_num_t)GT911_INTERRUPT_PIN, GPIO_INTR_DISABLE), TAG, "clear interrupt type");

    s_interrupt_ready = false;

    if (s_installed_isr_service) {
        gpio_uninstall_isr_service();
        s_installed_isr_service = false;
    }

    return ESP_OK;
}
