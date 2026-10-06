#ifndef H__widget_radio_button__
#define H__widget_radio_button__

#include <stdbool.h>
#include <stdint.h>

#include "display_font.h"
#include "widget.h"

typedef struct widget_radio_button_s widget_radio_button_t;

typedef struct widget_radio_group_s {
    widget_radio_button_t* selected;
} widget_radio_group_t;

typedef void (*widget_radio_button_onchange_t)(widget_radio_button_t* button, void* context);

typedef struct widget_radio_button_style_s {
    uint8_t background_color;
    uint8_t text_color;
    uint16_t border_width;
    uint16_t indicator_size;
    uint16_t spacing;
    uint16_t text_scale;
    const display_font_t* font;
} widget_radio_button_style_t;

struct widget_radio_button_s {
    widget_window_t window;
    widget_radio_group_t* group;
    const char* label;
    widget_radio_button_onchange_t onchange;
    void* onchange_context;
    widget_radio_button_style_t style;
    bool pressed;
};

extern const widget_radio_button_style_t WIDGET_RADIO_BUTTON_DEFAULT_STYLE;

void widget_radio_group_init(widget_radio_group_t* group);
void widget_radio_button_init(widget_radio_button_t* button, widget_radio_group_t* group,
                              widget_rect_t frame, const char* label, bool selected,
                              widget_radio_button_onchange_t onchange, void* context);
void widget_radio_button_set_label(widget_radio_button_t* button, const char* label);
void widget_radio_button_set_style(widget_radio_button_t* button,
                                   const widget_radio_button_style_t* style);
void widget_radio_button_set_selected(widget_radio_button_t* button, bool selected);
bool widget_radio_button_is_selected(const widget_radio_button_t* button);
void widget_radio_button_draw(widget_radio_button_t* button);

#endif
