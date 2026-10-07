#ifndef H__widget_select__
#define H__widget_select__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display.h"
#include "display_font.h"
#include "widget.h"

#define WIDGET_SELECT_NONE ((size_t)-1)

typedef struct widget_select_s widget_select_t;
typedef void (*widget_select_onchange_t)(widget_select_t* select, size_t selected_index,
                                         void* context);

typedef struct widget_select_style_s {
    uint8_t background_color;
    uint8_t text_color;
    uint16_t border_width;
    uint16_t padding;
    uint16_t text_scale;
    uint16_t item_height;
    const display_font_t* font;
} widget_select_style_t;

struct widget_select_s {
    widget_window_t window;
    widget_window_t popup_window;
    const char* const* options;
    size_t option_count;
    size_t selected_index;
    const char* placeholder;
    widget_select_onchange_t onchange;
    void* onchange_context;
    widget_select_style_t style;

    display_pixmap_t popup_backdrop;
    widget_rect_t popup_frame;
    int pressed_index;
    bool pressed;
    bool popup_open;
    bool popup_press_armed;
};

extern const widget_select_style_t WIDGET_SELECT_DEFAULT_STYLE;

void widget_select_init(widget_select_t* select, widget_rect_t frame,
                        const char* const* options, size_t option_count,
                        size_t selected_index, widget_select_onchange_t onchange,
                        void* context);
void widget_select_set_style(widget_select_t* select, const widget_select_style_t* style);
void widget_select_set_placeholder(widget_select_t* select, const char* placeholder);
void widget_select_set_selected(widget_select_t* select, size_t selected_index);
size_t widget_select_selected(const widget_select_t* select);
bool widget_select_open(widget_select_t* select);
void widget_select_close(widget_select_t* select);
bool widget_select_is_open(const widget_select_t* select);
void widget_select_draw(widget_select_t* select);

#endif
