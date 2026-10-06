#include "widget_radio_button.h"

#include <stddef.h>

#include "display.h"
#include "fonts/fonts.h"

const widget_radio_button_style_t WIDGET_RADIO_BUTTON_DEFAULT_STYLE = {
    .background_color = 0xF0,
    .text_color = 0x00,
    .border_width = 3,
    .indicator_size = 38,
    .spacing = 14,
    .text_scale = 1,
    .font = &font_swiss20,
};

static widget_radio_button_t* radio_button_from_window(widget_window_t* window) {
    return window ? (widget_radio_button_t*)window->context : NULL;
}

static uint8_t fast_gray(uint8_t gray) {
    return gray < 0x80 ? 0x00 : 0xF0;
}

static bool contains_local_point(const widget_radio_button_t* button, int x, int y) {
    return button && x >= 0 && y >= 0 &&
           x < button->window.frame.w && y < button->window.frame.h;
}

static void draw_radio_button_class(widget_window_t* window) {
    widget_radio_button_t* button = radio_button_from_window(window);
    if (!button || window->frame.w <= 0 || window->frame.h <= 0) {
        return;
    }

    const widget_rect_t frame = widget_window_absolute_frame(window);
    const uint8_t normal_background = fast_gray(button->style.background_color);
    const uint8_t normal_text = fast_gray(button->style.text_color);
    const uint8_t background = button->pressed ? normal_text : normal_background;
    const uint8_t foreground = button->pressed ? normal_background : normal_text;
    int indicator_size = button->style.indicator_size;
    if (indicator_size > frame.h - 4) indicator_size = frame.h - 4;
    if (indicator_size < 3) indicator_size = 3;
    int border_width = button->style.border_width;
    const int radius = (indicator_size - 2) / 2;
    if (border_width > radius) border_width = radius;

    display_fill_rect(frame.x, frame.y, frame.w, frame.h, background);

    const int indicator_x = frame.x + 2;
    const int center_x = indicator_x + indicator_size / 2;
    const int center_y = frame.y + frame.h / 2;
    display_draw_ellipse(center_x, center_y, radius, radius, border_width, foreground);
    if (widget_radio_button_is_selected(button)) {
        const uint16_t dot_radius = radius > 5 ? radius / 2 : radius;
        display_draw_ellipse(center_x, center_y, dot_radius, dot_radius,
                             dot_radius, foreground);
    }

    if (!button->label || button->style.text_scale == 0) {
        return;
    }

    const int text_x = indicator_x + indicator_size + button->style.spacing;
    const int available_width = frame.x + frame.w - text_x;
    uint16_t text_scale = button->style.text_scale;
    while (text_scale > 0 &&
           (display_measure_string(button->style.font, button->label, text_scale) >
                available_width ||
            display_measure_string_height(button->style.font, text_scale) > frame.h)) {
        --text_scale;
    }
    if (text_scale == 0) {
        return;
    }

    const int text_height = display_measure_string_height(button->style.font, text_scale);
    display_draw_string_with_font(text_x, frame.y + (frame.h - text_height) / 2,
                                  button->label, button->style.font,
                                  DISPLAY_ROTATE_0, text_scale, foreground);
}

static bool radio_button_event_class(widget_window_t* window, const input_event_t* event) {
    widget_radio_button_t* button = radio_button_from_window(window);
    int x;
    int y;
    if (!button || !widget_event_local_point(window, event, &x, &y)) {
        return false;
    }

    const bool inside = contains_local_point(button, x, y);
    if (event->type == INPUT_EVENT_TOUCH_PRESS) {
        button->pressed = true;
        widget_radio_button_draw(button);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_MOVE) {
        if (button->pressed != inside) {
            button->pressed = inside;
            widget_radio_button_draw(button);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_RELEASE) {
        const bool activate = button->pressed && inside;
        widget_radio_button_t* previous = button->group ? button->group->selected : NULL;
        const bool changed = activate && previous != button;
        button->pressed = false;
        if (changed) button->group->selected = button;
        if (changed && previous) widget_radio_button_draw(previous);
        widget_radio_button_draw(button);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        if (changed && button->onchange) {
            button->onchange(button, button->onchange_context);
        }
        return true;
    }
    return false;
}

static const widget_class_t s_radio_button_class = {
    .draw = draw_radio_button_class,
    .event = radio_button_event_class,
};

void widget_radio_group_init(widget_radio_group_t* group) {
    if (group) group->selected = NULL;
}

void widget_radio_button_init(widget_radio_button_t* button, widget_radio_group_t* group,
                              widget_rect_t frame, const char* label, bool selected,
                              widget_radio_button_onchange_t onchange, void* context) {
    if (!button || !group) return;
    *button = (widget_radio_button_t){
        .group = group,
        .label = label,
        .onchange = onchange,
        .onchange_context = context,
        .style = WIDGET_RADIO_BUTTON_DEFAULT_STYLE,
    };
    widget_window_init(&button->window, &s_radio_button_class, frame, button);
    if (selected) group->selected = button;
}

void widget_radio_button_set_label(widget_radio_button_t* button, const char* label) {
    if (button) button->label = label;
}

void widget_radio_button_set_style(widget_radio_button_t* button,
                                   const widget_radio_button_style_t* style) {
    if (button && style) button->style = *style;
}

void widget_radio_button_set_selected(widget_radio_button_t* button, bool selected) {
    if (!button || !button->group) return;
    if (selected) {
        button->group->selected = button;
    } else if (button->group->selected == button) {
        button->group->selected = NULL;
    }
}

bool widget_radio_button_is_selected(const widget_radio_button_t* button) {
    return button && button->group && button->group->selected == button;
}

void widget_radio_button_draw(widget_radio_button_t* button) {
    if (button) widget_draw(&button->window);
}
