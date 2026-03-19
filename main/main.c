#include <stdio.h>
#include "it8951.h"
#include "m5paper.h"

static void draw_orientation_pattern(void) {
    it8951_device_info_t info = {0};
    it8951_get_system_info(&info);

    const uint16_t w = info.width;
    const uint16_t h = info.height;

    const uint16_t margin = 20;
    const uint16_t corner = 90;
    const uint16_t axis = 24;
    const uint16_t step = 36;

    it8951_clear_screen();

    // Distinct corner blocks: black, dark gray, light gray, mid gray.
    it8951_fill_rect(margin, margin, corner, corner, 0x00);
    it8951_fill_rect(w - margin - corner, margin, corner, corner, 0x55);
    it8951_fill_rect(margin, h - margin - corner, corner, corner, 0xAA);
    it8951_fill_rect(w - margin - corner, h - margin - corner, corner, corner, 0xCC);

    // Long top bar and left bar identify the logical origin corner.
    it8951_fill_rect(margin, margin + corner + 10, w / 3, axis, 0x00);
    it8951_fill_rect(margin + corner + 10, margin, axis, h / 3, 0x00);

    // Stepped marker along +X direction.
    for (uint16_t i = 0; i < 5; ++i) {
        it8951_fill_rect(
            margin + 140 + (i * step),
            margin + corner + 50 + (i * 10),
            24,
            24,
            0x20 + (i * 0x20));
    }

    // Stepped marker along +Y direction.
    for (uint16_t i = 0; i < 5; ++i) {
        it8951_fill_rect(
            margin + corner + 50 + (i * 10),
            margin + 140 + (i * step),
            24,
            24,
            0x20 + (i * 0x20));
    }

    // Off-center center marker to expose mirroring/rotation.
    it8951_fill_rect((w / 2) - 110, (h / 2) - 30, 220, 60, 0x00);
    it8951_fill_rect((w / 2) - 20, (h / 2) - 110, 40, 220, 0x77);
}

void app_main(void)
{
    puts("hello world");

    m5paper_init();
    it8951_init(2300);
    draw_orientation_pattern();

}
