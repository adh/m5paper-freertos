#include "widget_button.h"

#include <stddef.h>
#include <string.h>

#include "display.h"

const widget_button_style_t WIDGET_BUTTON_DEFAULT_STYLE = {
    .background_color = 0xF0,
    .text_color = 0x00,
    .border_width = 4,
    .text_scale = 2,
};

static widget_button_t* widget_button_from_window(widget_window_t* window) {
    return window ? (widget_button_t*)window->context : NULL;
}

static uint8_t widget_button_fast_gray(uint8_t gray) {
    return gray < 0x80 ? 0x00 : 0xF0;
}

static bool widget_button_contains_local_point(const widget_button_t* button, int x, int y) {
    return button && x >= 0 && y >= 0 &&
           x < button->window.frame.w && y < button->window.frame.h;
}

static void widget_button_draw_class(widget_window_t* window) {
    widget_button_t* button = widget_button_from_window(window);
    if (!button || window->frame.w <= 0 || window->frame.h <= 0) {
        return;
    }

    const widget_rect_t frame = widget_window_absolute_frame(window);
    const uint8_t normal_background = widget_button_fast_gray(button->style.background_color);
    const uint8_t normal_text = widget_button_fast_gray(button->style.text_color);
    const uint8_t background = button->pressed ? normal_text : normal_background;
    const uint8_t text = button->pressed ? normal_background : normal_text;
    int border_width = button->style.border_width;
    const int max_border = (frame.w < frame.h ? frame.w : frame.h) / 2;
    if (border_width > max_border) {
        border_width = max_border;
    }

    display_fill_rect(frame.x, frame.y, frame.w, frame.h, background);
    display_stroke_rect(frame.x, frame.y, frame.w, frame.h, border_width, text);

    if (!button->label || button->style.text_scale == 0) {
        return;
    }

    const size_t label_length = strlen(button->label);
    const int available_width = frame.w - (border_width * 2);
    const int available_height = frame.h - (border_width * 2);
    uint16_t text_scale = button->style.text_scale;
    while (text_scale > 0 &&
           (label_length * 8 * text_scale > (size_t)(available_width > 0 ? available_width : 0) ||
            16 * text_scale > available_height)) {
        --text_scale;
    }
    if (text_scale == 0) {
        return;
    }

    const int text_width = (int)(label_length * 8 * text_scale);
    const int text_height = 16 * text_scale;
    const int text_x = frame.x + (frame.w - text_width) / 2;
    const int text_y = frame.y + (frame.h - text_height) / 2;
    display_draw_string(text_x, text_y, button->label, DISPLAY_ROTATE_0,
                        text_scale, text);
}

static bool widget_button_event_class(widget_window_t* window, const input_event_t* event) {
    widget_button_t* button = widget_button_from_window(window);
    int x;
    int y;
    if (!button || !widget_event_local_point(window, event, &x, &y)) {
        return false;
    }

    const bool inside = widget_button_contains_local_point(button, x, y);
    if (event->type == INPUT_EVENT_TOUCH_PRESS) {
        button->pressed = true;
        widget_button_draw(button);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        return true;
    }

    if (event->type == INPUT_EVENT_TOUCH_MOVE) {
        if (button->pressed != inside) {
            button->pressed = inside;
            widget_button_draw(button);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }

    if (event->type == INPUT_EVENT_TOUCH_RELEASE) {
        const bool clicked = button->pressed && inside;
        button->pressed = false;
        widget_button_draw(button);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        if (clicked && button->onclick) {
            button->onclick(button, button->onclick_context);
        }
        return true;
    }

    return false;
}

static const widget_class_t s_widget_button_class = {
    .draw = widget_button_draw_class,
    .event = widget_button_event_class,
};

void widget_button_init(widget_button_t* button, widget_rect_t frame, const char* label,
                        widget_button_onclick_t onclick, void* context) {
    if (!button) {
        return;
    }

    *button = (widget_button_t){
        .label = label,
        .onclick = onclick,
        .onclick_context = context,
        .style = WIDGET_BUTTON_DEFAULT_STYLE,
    };
    widget_window_init(&button->window, &s_widget_button_class, frame, button);
}

void widget_button_set_label(widget_button_t* button, const char* label) {
    if (button) {
        button->label = label;
    }
}

void widget_button_set_style(widget_button_t* button, const widget_button_style_t* style) {
    if (button && style) {
        button->style = *style;
    }
}

void widget_button_draw(widget_button_t* button) {
    if (button) {
        widget_draw(&button->window);
    }
}
