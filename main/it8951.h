#ifndef H__it8951__
#define H__it8951__

#include <stdint.h>

// Rotate mode
#define IT8951_ROTATE_0   0
#define IT8951_ROTATE_90  1
#define IT8951_ROTATE_180 2
#define IT8951_ROTATE_270 3

// Pixel mode (Bit per Pixel)
#define IT8951_2BPP 0
#define IT8951_3BPP 1
#define IT8951_4BPP 2
#define IT8951_8BPP 3

typedef struct it8951_device_info_s {
    uint16_t width;
    uint16_t height;
    uint16_t memory_address_low;
    uint16_t memory_address_heigh;
    uint8_t firmware_version[16];
    uint8_t lut_version[16];
} it8951_device_info_t;

typedef struct it8951_area_s {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} it8951_area_t;

void it8951_init(uint16_t vcomm);
void it8951_get_system_info(it8951_device_info_t *device_info);
void it8951_clear_screen(void);
void it8951_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t gray);
void it8951_update_area(it8951_area_t* area, int mode);
void it8951_wait_display_ready(void);

#endif
