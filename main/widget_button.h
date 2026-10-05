#ifndef H__widget_button__
#define H__widget_button__

#include <stdint.h>

#include "widget.h"

typedef struct widget_button_s widget_button_t;
typedef void (*widget_button_onclick_t)(widget_button_t* button, void* context);

typedef struct widget_button_style_s {
    uint8_t background_color;
    uint8_t text_color;
    uint16_t border_width;
    uint16_t text_scale;
} widget_button_style_t;

struct widget_button_s {
    widget_window_t window;
    const char* label;
    widget_button_onclick_t onclick;
    void* onclick_context;
    widget_button_style_t style;
    bool pressed;
};

extern const widget_button_style_t WIDGET_BUTTON_DEFAULT_STYLE;

void widget_button_init(widget_button_t* button, widget_rect_t frame, const char* label,
                        widget_button_onclick_t onclick, void* context);
void widget_button_set_label(widget_button_t* button, const char* label);
void widget_button_set_style(widget_button_t* button, const widget_button_style_t* style);
void widget_button_draw(widget_button_t* button);

#endif
