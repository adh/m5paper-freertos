#include "it8951.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_log_buffer.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char* TAG = "it8951";

#define ESP_ERROR_ASSERT(x)                                                                                    \
    do {                                                                                                       \
        if (unlikely(!(x))) {                                                                                  \
            printf("ESP_ERROR_ASSERT failed");                                                                 \
            printf(" at %p\n", __builtin_return_address(0));                                                   \
            printf("file: \"%s\" line %d\nfunc: %s\nexpression: %s\n", __FILE__, __LINE__, __ASSERT_FUNC, #x); \
            abort();                                                                                           \
        }                                                                                                      \
    } while (0)

#define IT8951_MOSI_PIN 12
#define IT8951_MISO_PIN 13
#define IT8951_SCLK_PIN 14
#define IT8951_CSEL_PIN 15
#define IT8951_DISPLAY_READY_PIN 27

#define IT8951_SPI_HOST 2

// Built in I80 Command Code
#define IT8951_TCON_SYS_RUN 0x0001
#define IT8951_TCON_STANDBY 0x0002
#define IT8951_TCON_SLEEP 0x0003
#define IT8951_TCON_REG_RD 0x0010
#define IT8951_TCON_REG_WR 0x0011

#define IT8951_TCON_MEM_BST_RD_T 0x0012
#define IT8951_TCON_MEM_BST_RD_S 0x0013
#define IT8951_TCON_MEM_BST_WR 0x0014
#define IT8951_TCON_MEM_BST_END 0x0015

#define IT8951_TCON_LD_IMG 0x0020
#define IT8951_TCON_LD_IMG_AREA 0x0021
#define IT8951_TCON_LD_IMG_END 0x0022

// I80 User defined command code
#define USDEF_I80_CMD_DPY_AREA 0x0034
#define USDEF_I80_CMD_GET_DEV_INFO 0x0302
#define USDEF_I80_CMD_DPY_BUF_AREA 0x0037
#define USDEF_I80_CMD_VCOM 0x0039

#define IT8951_ENDIAN_LITTLE 0
#define IT8951_ENDIAN_BIG 1
#define IT8951_PANEL_ROTATION IT8951_ROTATE_0
#define IT8951_MODE_INIT 0
#define IT8951_MODE_DU 1
#define IT8951_MODE_GC16 2

// Register Base Address
#define DISPLAY_REG_BASE 0x1000  // Register RW access

// Base Address of Basic LUT Registers
#define LUT0EWHR (DISPLAY_REG_BASE + 0x00)   // LUT0 Engine Width Height Reg
#define LUT0XYR (DISPLAY_REG_BASE + 0x40)    // LUT0 XY Reg
#define LUT0BADDR (DISPLAY_REG_BASE + 0x80)  // LUT0 Base Address Reg
#define LUT0MFN (DISPLAY_REG_BASE + 0xC0)    // LUT0 Mode and Frame number Reg
#define LUT01AF (DISPLAY_REG_BASE + 0x114)   // LUT0 and LUT1 Active Flag Reg

// Update Parameter Setting Register
#define UP0SR (DISPLAY_REG_BASE + 0x134)      // Update Parameter0 Setting Reg
#define UP1SR (DISPLAY_REG_BASE + 0x138)      // Update Parameter1 Setting Reg
#define LUT0ABFRV (DISPLAY_REG_BASE + 0x13C)  // LUT0 Alpha blend and Fill rectangle Value
#define UPBBADDR (DISPLAY_REG_BASE + 0x17C)   // Update Buffer Base Address
#define LUT0IMXY (DISPLAY_REG_BASE + 0x180)   // LUT0 Image buffer X/Y offset Reg
#define LUTAFSR (DISPLAY_REG_BASE + 0x224)    // LUT Status Reg (status of All LUT Engines)
#define BGVR (DISPLAY_REG_BASE + 0x250)       // Bitmap (1bpp) image color table

// System Registers
#define SYS_REG_BASE 0x0000

// Address of System Registers
#define I80CPCR (SYS_REG_BASE + 0x04)

// Memory Converter Registers
#define MCSR_BASE_ADDR 0x0200
#define MCSR (MCSR_BASE_ADDR + 0x0000)
#define LISAR (MCSR_BASE_ADDR + 0x0008)

static spi_device_handle_t spi;

static size_t buffer_len;
static uint8_t* buffer0;
static uint8_t* buffer1;
static uint8_t* framebuffer;
static size_t framebuffer_len;

static void spi_setup(int clock_speed_hz) {
    if (!spi) {
        spi_bus_config_t bus_config = {
            .mosi_io_num = IT8951_MOSI_PIN,
            .miso_io_num = IT8951_MISO_PIN,
            .sclk_io_num = IT8951_SCLK_PIN,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        };

        ESP_ERROR_CHECK(spi_bus_initialize(IT8951_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));
    } else {
        spi_bus_remove_device(spi);
    }

    spi_device_interface_config_t device_interface_config = {
        .clock_speed_hz = clock_speed_hz,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    ESP_ERROR_CHECK(spi_bus_add_device(IT8951_SPI_HOST, &device_interface_config, &spi));

    int freq_khz;
    ESP_ERROR_CHECK(spi_device_get_actual_freq(spi, &freq_khz));
    ESP_LOGI(TAG, "SPI device frequency %d KHz", freq_khz);
    ESP_ERROR_ASSERT(freq_khz * 1000 <= device_interface_config.clock_speed_hz);

    if (buffer0) {
        return;
    }

    size_t bus_max_transfer_sz;
    ESP_ERROR_CHECK(spi_bus_get_max_transaction_len(IT8951_SPI_HOST, &bus_max_transfer_sz));

    buffer_len = bus_max_transfer_sz < 2048 ? bus_max_transfer_sz : 2048;

    ESP_LOGI(TAG, "Allocating %d bytes for xfer buffers (max %d)", buffer_len, bus_max_transfer_sz);

    buffer0 = (uint8_t*)heap_caps_malloc(buffer_len, MALLOC_CAP_DMA);
    ESP_ERROR_ASSERT(buffer0);
    buffer1 = (uint8_t*)heap_caps_malloc(buffer_len, MALLOC_CAP_DMA);
    ESP_ERROR_ASSERT(buffer1);
}

static void transaction_start() { 
    gpio_set_level((gpio_num_t)IT8951_CSEL_PIN, 0); 
}

static void transaction_end() { 
    gpio_set_level((gpio_num_t)IT8951_CSEL_PIN, 1); 
}

static uint8_t read_byte() {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 8,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));

    return t.rx_data[0];
}

static uint16_t read_word() {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_RXDATA,
        .length = 16,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));

    return (uint16_t)t.rx_data[0] << 8 | t.rx_data[1];
}

static void read_array(uint8_t* data, size_t len, bool swap) {
    spi_transaction_t t = {
        .length = 8 * len,
        .rx_buffer = data,
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));

    if (swap) {
        for (size_t i = 0; i < len; i += 2) {
            uint8_t tmp = data[i];
            data[i] = data[i + 1];
            data[i + 1] = tmp;
        }
    }
}

static void write_byte(uint8_t value) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = {value},
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

static void write_word(uint16_t value) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
        .tx_data = {(uint8_t)(value >> 8), (uint8_t)(value)},
    };

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

static void write_array(uint8_t* data, size_t len, bool swap) {
    spi_transaction_t t = {
        .length = 8 * len,
        .tx_buffer = data,
    };

    if (swap) {
        for (size_t i = 0; i < len; i += 2) {
            uint8_t tmp = data[i];
            data[i] = data[i + 1];
            data[i + 1] = tmp;
        }
    }

    ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
}

static void delay(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static uint32_t millis() { return esp_timer_get_time() / 1000; }
static void wait_until_idle(){
    if (gpio_get_level((gpio_num_t)IT8951_DISPLAY_READY_PIN)) {
        return;
    }

    const uint32_t start = millis();
    while (!gpio_get_level((gpio_num_t)IT8951_DISPLAY_READY_PIN)) {
        ESP_ERROR_ASSERT(millis() - start < 3000);

        delay(20);
    }
}

static uint16_t read_data_word() {
    transaction_start();

    wait_until_idle();
    write_word(0x1000);
    wait_until_idle();
    read_word();  // Skip a word.
    wait_until_idle();
    uint16_t result = read_word();

    transaction_end();

    return result;
}

void read_data(uint8_t* data, size_t len) {
    transaction_start();

    wait_until_idle();
    write_word(0x1000);
    wait_until_idle();
    read_word();  // Skip a word.
    wait_until_idle();
    read_array(data, len, true);

    transaction_end();
}

void write_command(uint16_t command) {
    transaction_start();

    wait_until_idle();
    write_word(0x6000);
    wait_until_idle();
    write_word(command);

    transaction_end();
}

void write_data_word(uint16_t data) {
    transaction_start();

    wait_until_idle();
    write_word(0x0000);
    wait_until_idle();
    write_word(data);

    transaction_end();
}

void write_data(uint8_t* data, size_t len) {
    transaction_start();

    wait_until_idle();
    write_word(0x0000);
    wait_until_idle();
    write_array(data, len, true);

    transaction_end();
}

uint16_t read_reg(uint16_t reg) {
    transaction_start();

    write_command(IT8951_TCON_REG_RD);
    write_data_word(reg);
    uint16_t result = read_data_word();

    transaction_end();

    return result;
}

void write_reg(uint16_t reg, uint16_t value) {
    transaction_start();

    write_command(IT8951_TCON_REG_WR);
    write_data_word(reg);
    write_data_word(value);

    transaction_end();
}

void it8951_set_system_run() { 
    ESP_LOGI(TAG, "Start ePD controller");
    write_command(IT8951_TCON_SYS_RUN); 
}

void it8951_set_sleep() { 
    write_command(IT8951_TCON_SLEEP); 
}

void it8951_get_system_info(it8951_device_info_t *device_info) {
    ESP_LOGI(TAG, "Read device info");

    write_command(USDEF_I80_CMD_GET_DEV_INFO);
    read_data((uint8_t*)device_info, sizeof(it8951_device_info_t));

    ESP_LOG_BUFFER_HEXDUMP(TAG, (uint8_t*)device_info, sizeof(it8951_device_info_t), ESP_LOG_INFO);

    ESP_LOGI(TAG, "Panel(W,H) = (%d,%d)", device_info->width, device_info->height);
    ESP_LOGI(TAG, "Memory Address = %X", device_info->memory_address_low | (device_info->memory_address_heigh << 16));
    ESP_LOGI(TAG, "FW Version = %s", (uint8_t*)device_info->firmware_version);
    ESP_LOGI(TAG, "LUT Version = %s", (uint8_t*)device_info->lut_version);
}

uint16_t it8951_get_vcom() {
    write_command(USDEF_I80_CMD_VCOM);
    write_data_word(0x0000);
    return read_data_word();
}

void it8951_set_vcom(uint16_t vcom) {
    ESP_LOGI(TAG, "Set vcom to %d", vcom);
    write_command(USDEF_I80_CMD_VCOM);
    write_data_word(0x0001);
    write_data_word(vcom);
}

static it8951_device_info_t device_info;

static void framebuffer_init(void) {
    if (framebuffer) {
        return;
    }

    framebuffer_len = (size_t)device_info.width * device_info.height;
    ESP_LOGI(TAG, "Allocating %d bytes for framebuffer", framebuffer_len);

    framebuffer = heap_caps_malloc(framebuffer_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_ASSERT(framebuffer);
}

uint16_t it8951_width(void) {
    return device_info.width;
}

uint16_t it8951_height(void) {
    return device_info.height;
}

uint8_t* it8951_framebuffer(void) {
    ESP_ERROR_ASSERT(framebuffer);
    return framebuffer;
}

static void controller_init(uint16_t vcomm){

    transaction_end();
    it8951_set_system_run();
    it8951_get_system_info(&device_info);
    framebuffer_init();

    write_reg(I80CPCR, 0x0001);
    it8951_set_vcom(vcomm);

}

static void gpio_init(){
    ESP_LOGI(TAG, "GPIO setup");
    gpio_config_t o_conf = {
        .pin_bit_mask = 1ull << IT8951_CSEL_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&o_conf));

    gpio_config_t i_conf = {
        .pin_bit_mask = 1ull << IT8951_DISPLAY_READY_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&i_conf));
    delay(1000);
}

static void set_target_memory_address(uint32_t dev_address){
    uint16_t h = (uint16_t)((dev_address >> 16) & 0x0000FFFF);
    uint16_t l = (uint16_t)(dev_address & 0x0000FFFF);

    write_reg(LISAR + 2, h);
    write_reg(LISAR, l);    
}

static void set_target_area(it8951_area_t* area){
    const uint16_t load_image_arg =
        (IT8951_ENDIAN_LITTLE << 8) | (IT8951_8BPP << 4) | IT8951_PANEL_ROTATION;

    write_command(IT8951_TCON_LD_IMG_AREA);
    write_data_word(load_image_arg);
    write_data_word(area->x);
    write_data_word(area->y);
    write_data_word(area->w);
    write_data_word(area->h);
}

static void normalize_area_even_width(it8951_area_t* area) {
    if ((area->w & 1u) == 0u || area->w == 0u) {
        return;
    }

    if ((uint32_t)area->x + area->w < device_info.width) {
        area->w += 1;
        return;
    }

    if (area->x > 0) {
        area->x -= 1;
        area->w += 1;
    }
}

uint32_t it8951_get_vram_base(){
    return device_info.memory_address_low | (device_info.memory_address_heigh << 16);
}

static void log_upload_stats(const it8951_area_t* area, int64_t upload_start_us, int64_t update_start_us) {
    const uint32_t pixels = (uint32_t)area->w * area->h;
    const int64_t upload_time_us = update_start_us - upload_start_us;
    const int64_t update_time_us = esp_timer_get_time() - update_start_us;

    ESP_LOGI(TAG,
             "Upload area x=%u y=%u w=%u h=%u pixels=%" PRIu32 " upload=%.2f ms update=%.2f ms",
             area->x,
             area->y,
             area->w,
             area->h,
             pixels,
             (double)upload_time_us / 1000.0,
             (double)update_time_us / 1000.0);
}

static void upload_area_solid(const it8951_area_t* area, uint8_t gray, int mode) {
    size_t remaining = (size_t)area->w * area->h;
    const int64_t upload_start_us = esp_timer_get_time();

    ESP_ERROR_ASSERT(buffer0);
    memset(buffer0, gray, buffer_len);

    set_target_memory_address(it8951_get_vram_base());
    set_target_area((it8951_area_t*)area);
    while (remaining) {
        size_t chunk = remaining < buffer_len ? remaining : buffer_len;
        write_data(buffer0, chunk);
        remaining -= chunk;
    }
    write_command(IT8951_TCON_LD_IMG_END);
    const int64_t update_start_us = esp_timer_get_time();
    it8951_update_area((it8951_area_t*)area, mode);
    it8951_wait_display_ready();
    log_upload_stats(area, upload_start_us, update_start_us);
}

static void upload_area_8bpp(const it8951_area_t* area, const uint8_t* pixels, int mode) {
    size_t remaining = (size_t)area->w * area->h;
    const int64_t upload_start_us = esp_timer_get_time();

    ESP_ERROR_ASSERT(pixels);

    set_target_memory_address(it8951_get_vram_base());
    set_target_area((it8951_area_t*)area);

    while (remaining) {
        size_t chunk = remaining < buffer_len ? remaining : buffer_len;
        memcpy(buffer0, pixels, chunk);
        write_data(buffer0, chunk);
        pixels += chunk;
        remaining -= chunk;
    }

    write_command(IT8951_TCON_LD_IMG_END);
    const int64_t update_start_us = esp_timer_get_time();
    it8951_update_area((it8951_area_t*)area, mode);
    it8951_wait_display_ready();
    log_upload_stats(area, upload_start_us, update_start_us);
}

static void upload_area_8bpp_stride(const it8951_area_t* area, const uint8_t* pixels, uint16_t stride, int mode) {
    ESP_ERROR_ASSERT(pixels);
    ESP_ERROR_ASSERT(stride >= area->w);
    const int64_t upload_start_us = esp_timer_get_time();

    set_target_memory_address(it8951_get_vram_base());
    set_target_area((it8951_area_t*)area);

    for (uint16_t row = 0; row < area->h; ++row) {
        const uint8_t* row_pixels = pixels + ((size_t)row * stride);
        size_t remaining = area->w;

        while (remaining) {
            size_t chunk = remaining < buffer_len ? remaining : buffer_len;
            memcpy(buffer0, row_pixels, chunk);
            write_data(buffer0, chunk);
            row_pixels += chunk;
            remaining -= chunk;
        }
    }

    write_command(IT8951_TCON_LD_IMG_END);
    const int64_t update_start_us = esp_timer_get_time();
    it8951_update_area((it8951_area_t*)area, mode);
    it8951_wait_display_ready();
    log_upload_stats(area, upload_start_us, update_start_us);
}

void it8951_clear_screen(){
    it8951_area_t area = {
        .x = 0,
        .y = 0,
        .w = device_info.width,
        .h = device_info.height
    };

    upload_area_solid(&area, 0xFF, IT8951_MODE_INIT);
}

void it8951_update_area(it8951_area_t* area, int mode){
    write_command(USDEF_I80_CMD_DPY_AREA);
    write_data_word(area->x);
    write_data_word(area->y);
    write_data_word(area->w);
    write_data_word(area->h);
    write_data_word((uint16_t)mode);
}

void it8951_wait_display_ready(){
    uint32_t start = millis();

    while (true){
        if (!read_reg(LUTAFSR)){
            return;
        }

        if (millis() - start > 5000){
            ESP_LOGE(TAG, "Controller is busy for longer than 5s");
            esp_restart();
        }
        delay(20);
    }
}

void it8951_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t gray){
    ESP_ERROR_ASSERT(x < device_info.width);
    ESP_ERROR_ASSERT(y < device_info.height);

    if (x + w > device_info.width) {
        w = device_info.width - x;
    }
    if (y + h > device_info.height) {
        h = device_info.height - y;
    }

    if (!w || !h) {
        return;
    }

    it8951_area_t area = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };

    normalize_area_even_width(&area);
    upload_area_solid(&area, gray, IT8951_MODE_GC16);
}

void it8951_blit_8bpp(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* pixels) {
    ESP_ERROR_ASSERT(x < device_info.width);
    ESP_ERROR_ASSERT(y < device_info.height);

    if (x + w > device_info.width) {
        w = device_info.width - x;
    }
    if (y + h > device_info.height) {
        h = device_info.height - y;
    }

    if (!w || !h) {
        return;
    }

    it8951_area_t area = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };

    normalize_area_even_width(&area);
    upload_area_8bpp(&area, pixels, IT8951_MODE_GC16);
}

void it8951_blit_8bpp_stride(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* pixels, uint16_t stride) {
    ESP_ERROR_ASSERT(x < device_info.width);
    ESP_ERROR_ASSERT(y < device_info.height);

    if (x + w > device_info.width) {
        w = device_info.width - x;
    }
    if (y + h > device_info.height) {
        h = device_info.height - y;
    }

    if (!w || !h) {
        return;
    }

    it8951_area_t area = {
        .x = x,
        .y = y,
        .w = w,
        .h = h,
    };

    normalize_area_even_width(&area);
    upload_area_8bpp_stride(&area, pixels, stride, IT8951_MODE_GC16);
}

void it8951_init(uint16_t vcomm){
    gpio_init();

    ESP_LOGI(TAG, "SPI setup");
    spi_setup(SPI_MASTER_FREQ_10M);

    ESP_LOGI(TAG, "Initialization sequence start");
    controller_init(vcomm);
}
