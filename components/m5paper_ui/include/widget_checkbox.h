#ifndef H__widget_checkbox__
#define H__widget_checkbox__

#include <stdbool.h>
#include <stdint.h>

#include "display_font.h"
#include "widget.h"

typedef struct widget_checkbox_s widget_checkbox_t;
typedef void (*widget_checkbox_onchange_t)(widget_checkbox_t* checkbox, bool checked,
                                           void* context);

typedef struct widget_checkbox_style_s {
    uint8_t background_color;
    uint8_t text_color;
    uint16_t border_width;
    uint16_t indicator_size;
    uint16_t spacing;
    uint16_t text_scale;
    const display_font_t* font;
} widget_checkbox_style_t;

struct widget_checkbox_s {
    widget_window_t window;
    const char* label;
    widget_checkbox_onchange_t onchange;
    void* onchange_context;
    widget_checkbox_style_t style;
    bool checked;
    bool pressed;
};

extern const widget_checkbox_style_t WIDGET_CHECKBOX_DEFAULT_STYLE;

void widget_checkbox_init(widget_checkbox_t* checkbox, widget_rect_t frame, const char* label,
                          bool checked, widget_checkbox_onchange_t onchange, void* context);
void widget_checkbox_set_label(widget_checkbox_t* checkbox, const char* label);
void widget_checkbox_set_style(widget_checkbox_t* checkbox,
                               const widget_checkbox_style_t* style);
void widget_checkbox_set_checked(widget_checkbox_t* checkbox, bool checked);
bool widget_checkbox_is_checked(const widget_checkbox_t* checkbox);
void widget_checkbox_draw(widget_checkbox_t* checkbox);

#endif
