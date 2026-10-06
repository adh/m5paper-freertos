#include "demo.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "esp_log.h"
#include "fonts/fonts.h"
#include "it8951.h"
#include "m5paper.h"
#include "widget_button.h"

static const char* TAG = "demo";

#define DEMO_HEADER_HEIGHT 76
#define DEMO_MARGIN 24
#define DEMO_BUTTON_HEIGHT 88
#define DEMO_BACK_WIDTH 150
#define DEMO_TOUCH_MARKER_CAPACITY 128
#define DEMO_TOUCH_MARKER_RADIUS 12
#define DEMO_ANIMATION_TIMEOUT_MS 250
#define DEMO_SYSTEM_TIMEOUT_MS 2000
#define DEMO_SPRITE_SCALE 8

typedef enum demo_screen_id_e {
    DEMO_SCREEN_MENU = 0,
    DEMO_SCREEN_PRIMITIVES,
    DEMO_SCREEN_TOUCH,
    DEMO_SCREEN_ANIMATION,
    DEMO_SCREEN_SYSTEM,
} demo_screen_id_t;

typedef struct demo_sprite_s {
    int x;
    int y;
    int vx;
    int vy;
    int w;
    int h;
} demo_sprite_t;

typedef struct demo_touch_marker_s {
    uint16_t x;
    uint16_t y;
} demo_touch_marker_t;

static widget_window_t* s_app_root;
static widget_window_t* s_active_screen;
static demo_screen_id_t s_active_screen_id;

static widget_window_t s_menu_screen;
static widget_window_t s_primitives_screen;
static widget_window_t s_touch_screen;
static widget_window_t s_touch_canvas;
static widget_window_t s_animation_screen;
static widget_window_t s_system_screen;

static widget_button_t s_menu_primitives_button;
static widget_button_t s_menu_touch_button;
static widget_button_t s_menu_animation_button;
static widget_button_t s_menu_system_button;
static widget_button_t s_primitives_back_button;
static widget_button_t s_touch_back_button;
static widget_button_t s_touch_clear_button;
static widget_button_t s_animation_back_button;
static widget_button_t s_animation_reset_button;
static widget_button_t s_system_back_button;
static widget_button_t s_system_refresh_button;

static display_pixmap_t s_sprite_pixmap;
static demo_sprite_t s_sprite;
static uint32_t s_animation_frame;
static demo_touch_marker_t s_touch_markers[DEMO_TOUCH_MARKER_CAPACITY];
static size_t s_touch_marker_count;

static const char* const s_test_pixmap[] = {
    "24 20 4 1",
    ". c None",
    "X c #000000",
    "o c #666666",
    "+ c #DDDDDD",
    "........................",
    "..........XXXX..........",
    "........XXXXXXXX........",
    ".......XXooooooXX.......",
    "......XXo++++++oXX......",
    ".....XXo++XXXX++oXX.....",
    "....XXo++XXXXXX++oXX....",
    "...XXo++XX++++XX++oXX...",
    "...XXo++XX++++XX++oXX...",
    "...XXo++XXXXXXXX++oXX...",
    "...XXo++++++++++++oXX...",
    "....XXo++XXXXXX++oXX....",
    ".....XXo++XXXX++oXX.....",
    "......XXo++++++oXX......",
    ".......XXooooooXX.......",
    "........XXXXXXXX........",
    ".......XX++XX++XX.......",
    "......XX++XXXX++XX......",
    "......XX++X..X++XX......",
    "......XXXXXXXXXXXX......",
    NULL,
};

static void demo_show_screen(demo_screen_id_t screen_id);

static void draw_page_background(const char* title) {
    const display_font_t* font = &font_swiss20b;
    display_clear(0xF0);
    if (strlen(title) <= 21){
        font = &font_eurex24i;
    }
    display_draw_string_with_font(DEMO_MARGIN, 22, title, font,
                                  DISPLAY_ROTATE_0, 2, 0x00);
    display_draw_line(DEMO_MARGIN, DEMO_HEADER_HEIGHT - 1,
                      display_width() - DEMO_MARGIN, DEMO_HEADER_HEIGHT - 1, 2, 0x00);
}

static void draw_touch_marker(uint16_t x, uint16_t y) {
    display_draw_ellipse(x, y, DEMO_TOUCH_MARKER_RADIUS, DEMO_TOUCH_MARKER_RADIUS, 2, 0x00);
    display_draw_line(x - 7, y, x + 7, y, 2, 0x00);
    display_draw_line(x, y - 7, x, y + 7, 2, 0x00);
}

static void draw_touch_markers(void) {
    for (size_t i = 0; i < s_touch_marker_count; ++i) {
        draw_touch_marker(s_touch_markers[i].x, s_touch_markers[i].y);
    }
}

static void append_touch_marker(uint16_t x, uint16_t y) {
    if (s_touch_marker_count == DEMO_TOUCH_MARKER_CAPACITY) {
        for (size_t i = 1; i < DEMO_TOUCH_MARKER_CAPACITY; ++i) {
            s_touch_markers[i - 1] = s_touch_markers[i];
        }
        --s_touch_marker_count;
    }
    s_touch_markers[s_touch_marker_count++] = (demo_touch_marker_t){.x = x, .y = y};
}

static void reset_sprite(void) {
    s_sprite = (demo_sprite_t){
        .x = DEMO_MARGIN,
        .y = DEMO_HEADER_HEIGHT + 60,
        .vx = 28,
        .vy = 20,
        .w = s_sprite_pixmap.width * DEMO_SPRITE_SCALE,
        .h = s_sprite_pixmap.height * DEMO_SPRITE_SCALE,
    };
    s_animation_frame = 0;
}

static void draw_sprite(void) {
    display_pixmap_blit(s_sprite.x, s_sprite.y, &s_sprite_pixmap,
                        DISPLAY_ROTATE_0, DEMO_SPRITE_SCALE, 0xFF);
}

static void step_sprite(void) {
    const int min_x = DEMO_MARGIN;
    const int max_x = display_width() - s_sprite.w - DEMO_MARGIN;
    const int min_y = DEMO_HEADER_HEIGHT + 60;
    const int max_y = display_height() - s_sprite.h - DEMO_MARGIN;

    s_sprite.x += s_sprite.vx;
    s_sprite.y += s_sprite.vy;
    if (s_sprite.x <= min_x || s_sprite.x >= max_x) {
        s_sprite.vx = -s_sprite.vx;
        if (s_sprite.x < min_x) s_sprite.x = min_x;
        if (s_sprite.x > max_x) s_sprite.x = max_x;
    }
    if (s_sprite.y <= min_y || s_sprite.y >= max_y) {
        s_sprite.vy = -s_sprite.vy;
        if (s_sprite.y < min_y) s_sprite.y = min_y;
        if (s_sprite.y > max_y) s_sprite.y = max_y;
    }
}

static void update_animation(void) {
    ESP_LOGI(TAG, "Animation frame %" PRIu32, s_animation_frame++);
    display_fill_rect(s_sprite.x, s_sprite.y, s_sprite.w, s_sprite.h, 0xF0);
    step_sprite();
    draw_sprite();
    display_update();
}

static void draw_system_values(void) {
    char text[64];
    float battery_voltage;
    const bool battery_ok = m5paper_battery_voltage(&battery_voltage);

    display_fill_rect(DEMO_MARGIN, 150, display_width() - DEMO_MARGIN * 2, 240, 0xF0);
    snprintf(text, sizeof(text), DISPLAY_FONT_BOLD_ON "Display:" DISPLAY_FONT_BOLD_OFF " %u x %u",
             display_width(), display_height());
    display_draw_string_with_font(DEMO_MARGIN, 154, text, &font_bigfnt,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    if (battery_ok) {
        snprintf(text, sizeof(text), DISPLAY_FONT_BOLD_ON "Battery:" DISPLAY_FONT_BOLD_OFF " "
                 DISPLAY_FONT_ITALIC_ON "%.3f V" DISPLAY_FONT_STYLE_RESET, battery_voltage);
    } else {
        snprintf(text, sizeof(text), DISPLAY_FONT_BOLD_ON "Battery:" DISPLAY_FONT_BOLD_OFF
                 DISPLAY_FONT_ITALIC_ON " unavailable" DISPLAY_FONT_STYLE_RESET);
    }
    display_draw_string_with_font(DEMO_MARGIN, 205, text, &font_bigfnt,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    snprintf(text, sizeof(text), DISPLAY_FONT_BOLD_ON "Firmware:" DISPLAY_FONT_BOLD_OFF " %.16s",
             it8951_firmware_version());
    display_draw_string_with_font(DEMO_MARGIN, 256, text, &font_bigfnt,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    snprintf(text, sizeof(text), DISPLAY_FONT_BOLD_ON "Panel LUT:" DISPLAY_FONT_BOLD_OFF " %.16s",
             it8951_lut_version());
    display_draw_string_with_font(DEMO_MARGIN, 307, text, &font_bigfnt,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    display_draw_string_with_font(DEMO_MARGIN, 358,
                                  DISPLAY_FONT_BOLD_ON "Fast update:" DISPLAY_FONT_BOLD_OFF
                                  " monochrome mode 6",
                                  &font_bigfnt, DISPLAY_ROTATE_0, 1, 0x00);
}

static void menu_draw(widget_window_t* window) {
    (void)window;
    draw_page_background("Widget demonstrations");
    display_draw_string_with_font(DEMO_MARGIN, 92, "Choose a focused test screen.",
                                  &font_swiss20, DISPLAY_ROTATE_0, 1, 0x00);
}

static void primitives_draw(widget_window_t* window) {
    (void)window;
    draw_page_background("Drawing primitives and fonts");
    display_fill_rect(40, 115, 90, 55, 0x00);
    display_fill_rect(155, 115, 130, 55, 0x90);
    display_stroke_rect(320, 110, 210, 76, 5, 0x40);
    display_draw_roundrect(570, 110, 150, 76, 18, 5, 0x30);
    display_draw_ellipse(820, 148, 72, 38, 4, 0x70);
    display_draw_line(42, 220, 280, 350, 5, 0x20);
    display_draw_line(42, 350, 280, 220, 2, 0x90);
    display_draw_string_with_font(340, 215, "MeepMeep!", &font_eurex24i,
                                  DISPLAY_ROTATE_0, 2, 0x00);
    display_draw_string_with_font(340, 285, "(define demo 1)", &font_bigfnt,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    display_draw_string_with_font(340, 355, "Swiss regular", &font_swiss20,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    display_draw_string_with_font(340, 405, "Swiss bold", &font_swiss20b,
                                  DISPLAY_ROTATE_0, 1, 0x00);
}

static void touch_draw(widget_window_t* window) {
    (void)window;
    draw_page_background("Touch input");
    display_draw_string_with_font(DEMO_MARGIN, 92, "Draw on the canvas with a finger.",
                                  &font_swiss20, DISPLAY_ROTATE_0, 1, 0x00);
}

static void touch_canvas_draw(widget_window_t* window) {
    (void)window;
    draw_touch_markers();
}

static bool touch_canvas_event(widget_window_t* window, const input_event_t* event) {
    (void)window;
    if (event->type == INPUT_EVENT_TOUCH_RELEASE) return true;
    if (event->type != INPUT_EVENT_TOUCH_PRESS && event->type != INPUT_EVENT_TOUCH_MOVE) {
        return false;
    }
    append_touch_marker(event->touch.x, event->touch.y);
    draw_touch_marker(event->touch.x, event->touch.y);
    display_update_with_mode(DISPLAY_UPDATE_MODE_DU);
    return true;
}

static void animation_draw(widget_window_t* window) {
    (void)window;
    draw_page_background("Partial-update animation");
    display_draw_string_with_font(DEMO_MARGIN, 92,
                                  "GC16 updates the sprite's changing region.",
                                  &font_swiss20, DISPLAY_ROTATE_0, 1, 0x00);
    draw_sprite();
}

static void system_draw(widget_window_t* window) {
    (void)window;
    draw_page_background("System information");
    display_draw_string_with_font(DEMO_MARGIN, 92,
                                  "Live values refresh every two seconds.", &font_swiss20,
                                  DISPLAY_ROTATE_0, 1, 0x00);
    draw_system_values();
}

static bool app_event(widget_window_t* window, const input_event_t* event) {
    (void)window;
    if (event->type == INPUT_EVENT_DIRECTIONAL_BUTTON) {
        if (event->button.pressed && event->button.button == INPUT_BUTTON_CENTER) {
            demo_show_screen(DEMO_SCREEN_MENU);
        }
        return true;
    }
    if (event->type != INPUT_EVENT_TIMEOUT) return false;
    if (s_active_screen_id == DEMO_SCREEN_ANIMATION) {
        update_animation();
    } else if (s_active_screen_id == DEMO_SCREEN_SYSTEM) {
        draw_system_values();
        display_update();
    }
    return true;
}

static const widget_class_t s_app_class = {.event = app_event};
static const widget_class_t s_menu_class = {.draw = menu_draw};
static const widget_class_t s_primitives_class = {.draw = primitives_draw};
static const widget_class_t s_touch_class = {.draw = touch_draw};
static const widget_class_t s_touch_canvas_class = {
    .draw = touch_canvas_draw,
    .event = touch_canvas_event,
};
static const widget_class_t s_animation_class = {.draw = animation_draw};
static const widget_class_t s_system_class = {.draw = system_draw};

static widget_window_t* screen_for_id(demo_screen_id_t screen_id) {
    switch (screen_id) {
        case DEMO_SCREEN_PRIMITIVES: return &s_primitives_screen;
        case DEMO_SCREEN_TOUCH: return &s_touch_screen;
        case DEMO_SCREEN_ANIMATION: return &s_animation_screen;
        case DEMO_SCREEN_SYSTEM: return &s_system_screen;
        default: return &s_menu_screen;
    }
}

static void demo_show_screen(demo_screen_id_t screen_id) {
    if (s_active_screen) widget_window_remove(s_active_screen);
    s_active_screen_id = screen_id;
    s_active_screen = screen_for_id(screen_id);
    if (screen_id == DEMO_SCREEN_ANIMATION) reset_sprite();
    widget_window_add_child(s_app_root, s_active_screen);
    widget_draw(s_app_root);
    display_update();
}

static void menu_button_clicked(widget_button_t* button, void* context) {
    (void)button;
    demo_show_screen((demo_screen_id_t)(intptr_t)context);
}

static void back_button_clicked(widget_button_t* button, void* context) {
    (void)button;
    (void)context;
    demo_show_screen(DEMO_SCREEN_MENU);
}

static void touch_clear_clicked(widget_button_t* button, void* context) {
    (void)button;
    (void)context;
    s_touch_marker_count = 0;
    widget_draw(&s_touch_screen);
    display_update();
}

static void animation_reset_clicked(widget_button_t* button, void* context) {
    (void)button;
    (void)context;
    reset_sprite();
    widget_draw(&s_animation_screen);
    display_update();
}

static void system_refresh_clicked(widget_button_t* button, void* context) {
    (void)button;
    (void)context;
    draw_system_values();
    display_update();
}

static void init_button(widget_button_t* button, widget_window_t* parent, widget_rect_t frame,
                        const char* label, widget_button_onclick_t onclick, void* context) {
    widget_button_init(button, frame, label, onclick, context);
    widget_window_add_child(parent, &button->window);
}

static void init_back_button(widget_button_t* button, widget_window_t* parent) {
    init_button(button, parent,
                (widget_rect_t){display_width() - DEMO_BACK_WIDTH - DEMO_MARGIN, 12,
                                DEMO_BACK_WIDTH, 52},
                "Back", back_button_clicked, NULL);
}

void demo_init(widget_window_t* root) {
    s_app_root = root;
    const widget_rect_t full_screen = {0, 0, display_width(), display_height()};
    widget_window_init(root, &s_app_class, full_screen, NULL);
    widget_window_init(&s_menu_screen, &s_menu_class, full_screen, NULL);
    widget_window_init(&s_primitives_screen, &s_primitives_class, full_screen, NULL);
    widget_window_init(&s_touch_screen, &s_touch_class, full_screen, NULL);
    widget_window_init(&s_animation_screen, &s_animation_class, full_screen, NULL);
    widget_window_init(&s_system_screen, &s_system_class, full_screen, NULL);

    const int button_width = (display_width() - DEMO_MARGIN * 3) / 2;
    init_button(&s_menu_primitives_button, &s_menu_screen,
                (widget_rect_t){DEMO_MARGIN, 155, button_width, DEMO_BUTTON_HEIGHT},
                "Primitives", menu_button_clicked, (void*)(intptr_t)DEMO_SCREEN_PRIMITIVES);
    init_button(&s_menu_touch_button, &s_menu_screen,
                (widget_rect_t){DEMO_MARGIN * 2 + button_width, 155, button_width, DEMO_BUTTON_HEIGHT},
                "Touch", menu_button_clicked, (void*)(intptr_t)DEMO_SCREEN_TOUCH);
    init_button(&s_menu_animation_button, &s_menu_screen,
                (widget_rect_t){DEMO_MARGIN, 285, button_width, DEMO_BUTTON_HEIGHT},
                "Animation", menu_button_clicked, (void*)(intptr_t)DEMO_SCREEN_ANIMATION);
    init_button(&s_menu_system_button, &s_menu_screen,
                (widget_rect_t){DEMO_MARGIN * 2 + button_width, 285, button_width, DEMO_BUTTON_HEIGHT},
                "System", menu_button_clicked, (void*)(intptr_t)DEMO_SCREEN_SYSTEM);

    init_back_button(&s_primitives_back_button, &s_primitives_screen);

    widget_window_init(&s_touch_canvas, &s_touch_canvas_class,
                       (widget_rect_t){0, DEMO_HEADER_HEIGHT, display_width(),
                                       display_height() - DEMO_HEADER_HEIGHT}, NULL);
    widget_window_add_child(&s_touch_screen, &s_touch_canvas);
    init_back_button(&s_touch_back_button, &s_touch_screen);
    init_button(&s_touch_clear_button, &s_touch_screen,
                (widget_rect_t){display_width() - 330, 12, 150, 52},
                "Clear", touch_clear_clicked, NULL);

    init_back_button(&s_animation_back_button, &s_animation_screen);
    init_button(&s_animation_reset_button, &s_animation_screen,
                (widget_rect_t){display_width() - 330, 12, 150, 52},
                "Reset", animation_reset_clicked, NULL);

    init_back_button(&s_system_back_button, &s_system_screen);
    init_button(&s_system_refresh_button, &s_system_screen,
                (widget_rect_t){display_width() - 330, 12, 150, 52},
                "Refresh", system_refresh_clicked, NULL);

    if (!display_pixmap_from_xbm3(&s_sprite_pixmap, s_test_pixmap)) {
        ESP_LOGE(TAG, "Failed to decode animation pixmap");
    }
    reset_sprite();
    demo_show_screen(DEMO_SCREEN_MENU);
}

uint32_t demo_event_timeout_ms(void) {
    if (s_active_screen_id == DEMO_SCREEN_ANIMATION) return DEMO_ANIMATION_TIMEOUT_MS;
    if (s_active_screen_id == DEMO_SCREEN_SYSTEM) return DEMO_SYSTEM_TIMEOUT_MS;
    return UINT32_MAX;
}
