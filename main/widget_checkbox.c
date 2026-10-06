#include "widget_checkbox.h"

#include <stddef.h>

#include "display.h"
#include "fonts/fonts.h"

const widget_checkbox_style_t WIDGET_CHECKBOX_DEFAULT_STYLE = {
    .background_color = 0xF0,
    .text_color = 0x00,
    .border_width = 3,
    .indicator_size = 38,
    .spacing = 14,
    .text_scale = 1,
    .font = &font_swiss20,
};

static widget_checkbox_t* widget_checkbox_from_window(widget_window_t* window) {
    return window ? (widget_checkbox_t*)window->context : NULL;
}

static uint8_t fast_gray(uint8_t gray) {
    return gray < 0x80 ? 0x00 : 0xF0;
}

static bool contains_local_point(const widget_checkbox_t* checkbox, int x, int y) {
    return checkbox && x >= 0 && y >= 0 &&
           x < checkbox->window.frame.w && y < checkbox->window.frame.h;
}

static void draw_checkbox_class(widget_window_t* window) {
    widget_checkbox_t* checkbox = widget_checkbox_from_window(window);
    if (!checkbox || window->frame.w <= 0 || window->frame.h <= 0) {
        return;
    }

    const widget_rect_t frame = widget_window_absolute_frame(window);
    const uint8_t normal_background = fast_gray(checkbox->style.background_color);
    const uint8_t normal_text = fast_gray(checkbox->style.text_color);
    const uint8_t background = checkbox->pressed ? normal_text : normal_background;
    const uint8_t foreground = checkbox->pressed ? normal_background : normal_text;
    int indicator_size = checkbox->style.indicator_size;
    if (indicator_size > frame.h - 4) indicator_size = frame.h - 4;
    if (indicator_size < 1) indicator_size = 1;
    int border_width = checkbox->style.border_width;
    if (border_width * 2 > indicator_size) border_width = indicator_size / 2;

    display_fill_rect(frame.x, frame.y, frame.w, frame.h, background);

    const int indicator_x = frame.x + 2;
    const int indicator_y = frame.y + (frame.h - indicator_size) / 2;
    display_stroke_rect(indicator_x, indicator_y, indicator_size, indicator_size,
                        border_width, foreground);
    if (checkbox->checked) {
        int inset = indicator_size / 4;
        if (inset < border_width + 1) inset = border_width + 1;
        const int far_edge = indicator_size - inset - 1;
        const uint16_t mark_width = border_width > 2 ? border_width : 3;
        if (far_edge >= inset) {
            display_draw_line(indicator_x + inset, indicator_y + inset,
                              indicator_x + far_edge, indicator_y + far_edge,
                              mark_width, foreground);
            display_draw_line(indicator_x + far_edge, indicator_y + inset,
                              indicator_x + inset, indicator_y + far_edge,
                              mark_width, foreground);
        }
    }

    if (!checkbox->label || checkbox->style.text_scale == 0) {
        return;
    }

    const int text_x = indicator_x + indicator_size + checkbox->style.spacing;
    const int available_width = frame.x + frame.w - text_x;
    uint16_t text_scale = checkbox->style.text_scale;
    while (text_scale > 0 &&
           (display_measure_string(checkbox->style.font, checkbox->label, text_scale) >
                available_width ||
            display_measure_string_height(checkbox->style.font, text_scale) > frame.h)) {
        --text_scale;
    }
    if (text_scale == 0) {
        return;
    }

    const int text_height = display_measure_string_height(checkbox->style.font, text_scale);
    display_draw_string_with_font(text_x, frame.y + (frame.h - text_height) / 2,
                                  checkbox->label, checkbox->style.font,
                                  DISPLAY_ROTATE_0, text_scale, foreground);
}

static bool checkbox_event_class(widget_window_t* window, const input_event_t* event) {
    widget_checkbox_t* checkbox = widget_checkbox_from_window(window);
    int x;
    int y;
    if (!checkbox || !widget_event_local_point(window, event, &x, &y)) {
        return false;
    }

    const bool inside = contains_local_point(checkbox, x, y);
    if (event->type == INPUT_EVENT_TOUCH_PRESS) {
        checkbox->pressed = true;
        widget_checkbox_draw(checkbox);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_MOVE) {
        if (checkbox->pressed != inside) {
            checkbox->pressed = inside;
            widget_checkbox_draw(checkbox);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_RELEASE) {
        const bool changed = checkbox->pressed && inside;
        checkbox->pressed = false;
        if (changed) checkbox->checked = !checkbox->checked;
        widget_checkbox_draw(checkbox);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        if (changed && checkbox->onchange) {
            checkbox->onchange(checkbox, checkbox->checked, checkbox->onchange_context);
        }
        return true;
    }
    return false;
}

static const widget_class_t s_checkbox_class = {
    .draw = draw_checkbox_class,
    .event = checkbox_event_class,
};

void widget_checkbox_init(widget_checkbox_t* checkbox, widget_rect_t frame, const char* label,
                          bool checked, widget_checkbox_onchange_t onchange, void* context) {
    if (!checkbox) return;
    *checkbox = (widget_checkbox_t){
        .label = label,
        .onchange = onchange,
        .onchange_context = context,
        .style = WIDGET_CHECKBOX_DEFAULT_STYLE,
        .checked = checked,
    };
    widget_window_init(&checkbox->window, &s_checkbox_class, frame, checkbox);
}

void widget_checkbox_set_label(widget_checkbox_t* checkbox, const char* label) {
    if (checkbox) checkbox->label = label;
}

void widget_checkbox_set_style(widget_checkbox_t* checkbox,
                               const widget_checkbox_style_t* style) {
    if (checkbox && style) checkbox->style = *style;
}

void widget_checkbox_set_checked(widget_checkbox_t* checkbox, bool checked) {
    if (checkbox) checkbox->checked = checked;
}

bool widget_checkbox_is_checked(const widget_checkbox_t* checkbox) {
    return checkbox && checkbox->checked;
}

void widget_checkbox_draw(widget_checkbox_t* checkbox) {
    if (checkbox) widget_draw(&checkbox->window);
}
