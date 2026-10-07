#include "widget_select.h"

#include <limits.h>
#include <stddef.h>

#include "fonts/fonts.h"

const widget_select_style_t WIDGET_SELECT_DEFAULT_STYLE = {
    .background_color = 0xF0,
    .text_color = 0x00,
    .border_width = 3,
    .padding = 12,
    .text_scale = 1,
    .item_height = 58,
    .font = &font_swiss20,
};

static widget_select_t* select_from_window(widget_window_t* window) {
    return window ? (widget_select_t*)window->context : NULL;
}

static uint8_t fast_gray(uint8_t gray) {
    return gray < 0x80 ? 0x00 : 0xF0;
}

static bool contains_local_point(const widget_select_t* select, int x, int y) {
    return select && x >= 0 && y >= 0 &&
           x < select->window.frame.w && y < select->window.frame.h;
}

static void draw_fitted_text(const widget_select_t* select, int x, int y, int w, int h,
                             const char* text, uint8_t color) {
    if (!text || !*text || w <= 0 || h <= 0 || select->style.text_scale == 0) return;

    uint16_t scale = select->style.text_scale;
    while (scale > 0 &&
           (display_measure_string(select->style.font, text, scale) > w ||
            display_measure_string_height(select->style.font, scale) > h)) {
        --scale;
    }
    if (scale == 0) return;

    const int text_height = display_measure_string_height(select->style.font, scale);
    display_draw_string_with_font(x, y + (h - text_height) / 2, text, select->style.font,
                                  DISPLAY_ROTATE_0, scale, color);
}

static void draw_select_class(widget_window_t* window) {
    widget_select_t* select = select_from_window(window);
    if (!select || window->frame.w <= 0 || window->frame.h <= 0) return;

    const widget_rect_t frame = widget_window_absolute_frame(window);
    const uint8_t normal_background = fast_gray(select->style.background_color);
    const uint8_t normal_text = fast_gray(select->style.text_color);
    const uint8_t background = select->pressed ? normal_text : normal_background;
    const uint8_t foreground = select->pressed ? normal_background : normal_text;
    int border_width = select->style.border_width;
    const int max_border = (frame.w < frame.h ? frame.w : frame.h) / 2;
    if (border_width > max_border) border_width = max_border;

    display_fill_rect(frame.x, frame.y, frame.w, frame.h, background);
    display_stroke_rect(frame.x, frame.y, frame.w, frame.h, border_width, foreground);

    const int arrow_width = 34;
    const int content_x = frame.x + border_width + select->style.padding;
    const int content_w = frame.w - border_width * 2 - select->style.padding * 2 - arrow_width;
    const char* text = select->placeholder;
    if (select->selected_index < select->option_count && select->options) {
        text = select->options[select->selected_index];
    }
    draw_fitted_text(select, content_x, frame.y + border_width, content_w,
                     frame.h - border_width * 2, text, foreground);

    const int arrow_x = frame.x + frame.w - border_width - select->style.padding - 10;
    const int arrow_y = frame.y + frame.h / 2;
    const uint16_t arrow_stroke = border_width > 1 ? border_width : 2;
    if (select->popup_open) {
        display_draw_line(arrow_x - 8, arrow_y + 4, arrow_x, arrow_y - 4,
                          arrow_stroke, foreground);
        display_draw_line(arrow_x, arrow_y - 4, arrow_x + 8, arrow_y + 4,
                          arrow_stroke, foreground);
    } else {
        display_draw_line(arrow_x - 8, arrow_y - 4, arrow_x, arrow_y + 4,
                          arrow_stroke, foreground);
        display_draw_line(arrow_x, arrow_y + 4, arrow_x + 8, arrow_y - 4,
                          arrow_stroke, foreground);
    }
}

static void draw_popup_class(widget_window_t* window) {
    widget_select_t* select = select_from_window(window);
    if (!select || !select->popup_open) return;

    const widget_rect_t frame = select->popup_frame;
    const uint8_t background = fast_gray(select->style.background_color);
    const uint8_t foreground = fast_gray(select->style.text_color);
    const int border_width = select->style.border_width;
    const int item_height = select->style.item_height;

    display_fill_rect(frame.x, frame.y, frame.w, frame.h, background);
    display_stroke_rect(frame.x, frame.y, frame.w, frame.h, border_width, foreground);
    for (size_t i = 0; i < select->option_count; ++i) {
        const int item_y = frame.y + border_width + (int)i * item_height;
        const bool highlighted = i == select->selected_index || (int)i == select->pressed_index;
        const uint8_t item_background = highlighted ? foreground : background;
        const uint8_t item_text = highlighted ? background : foreground;
        display_fill_rect(frame.x + border_width, item_y,
                          frame.w - border_width * 2, item_height, item_background);
        draw_fitted_text(select, frame.x + border_width + select->style.padding, item_y,
                         frame.w - border_width * 2 - select->style.padding * 2,
                         item_height, select->options ? select->options[i] : NULL, item_text);
        if (i + 1 < select->option_count) {
            display_fill_rect(frame.x + border_width, item_y + item_height - 1,
                              frame.w - border_width * 2, 1, foreground);
        }
    }
}

static int popup_item_at(const widget_select_t* select, uint16_t x, uint16_t y) {
    const widget_rect_t frame = select->popup_frame;
    const int border = select->style.border_width;
    if ((int)x < frame.x + border || (int)x >= frame.x + frame.w - border ||
        (int)y < frame.y + border || (int)y >= frame.y + frame.h - border) {
        return -1;
    }
    const int index = ((int)y - frame.y - border) / select->style.item_height;
    return index >= 0 && (size_t)index < select->option_count ? index : -1;
}

static void redraw_popup(widget_select_t* select) {
    widget_draw(&select->popup_window);
    display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
}

static bool popup_event_class(widget_window_t* window, const input_event_t* event) {
    widget_select_t* select = select_from_window(window);
    if (!select || !select->popup_open) return false;
    if (event->type != INPUT_EVENT_TOUCH_PRESS &&
        event->type != INPUT_EVENT_TOUCH_MOVE &&
        event->type != INPUT_EVENT_TOUCH_RELEASE) {
        return false;
    }

    const int item = popup_item_at(select, event->touch.x, event->touch.y);
    if (event->type == INPUT_EVENT_TOUCH_PRESS) {
        select->popup_press_armed = item >= 0;
        select->pressed_index = item;
        if (item >= 0) redraw_popup(select);
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_MOVE) {
        const int next = select->popup_press_armed ? item : -1;
        if (next != select->pressed_index) {
            select->pressed_index = next;
            redraw_popup(select);
        }
        return true;
    }

    const bool changed = select->popup_press_armed && item >= 0 &&
                         (size_t)item != select->selected_index;
    if (select->popup_press_armed && item >= 0) select->selected_index = (size_t)item;
    select->popup_press_armed = false;
    widget_select_close(select);
    if (changed && select->onchange) {
        select->onchange(select, select->selected_index, select->onchange_context);
    }
    return true;
}

static bool select_event_class(widget_window_t* window, const input_event_t* event) {
    widget_select_t* select = select_from_window(window);
    int x;
    int y;
    if (!select || !widget_event_local_point(window, event, &x, &y)) return false;

    const bool inside = contains_local_point(select, x, y);
    if (event->type == INPUT_EVENT_TOUCH_PRESS) {
        select->pressed = true;
        widget_select_draw(select);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_MOVE) {
        if (select->pressed != inside) {
            select->pressed = inside;
            widget_select_draw(select);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_RELEASE) {
        const bool activate = select->pressed && inside;
        select->pressed = false;
        if (!activate || !widget_select_open(select)) {
            widget_select_draw(select);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }
    return false;
}

static const widget_class_t s_select_class = {
    .draw = draw_select_class,
    .event = select_event_class,
};

static const widget_class_t s_popup_class = {
    .draw = draw_popup_class,
    .event = popup_event_class,
};

void widget_select_init(widget_select_t* select, widget_rect_t frame,
                        const char* const* options, size_t option_count,
                        size_t selected_index, widget_select_onchange_t onchange,
                        void* context) {
    if (!select) return;
    *select = (widget_select_t){
        .options = options,
        .option_count = option_count,
        .selected_index = selected_index < option_count ? selected_index : WIDGET_SELECT_NONE,
        .placeholder = "Select...",
        .onchange = onchange,
        .onchange_context = context,
        .style = WIDGET_SELECT_DEFAULT_STYLE,
        .pressed_index = -1,
    };
    widget_window_init(&select->window, &s_select_class, frame, select);
    widget_window_init(&select->popup_window, &s_popup_class, (widget_rect_t){0}, select);
}

void widget_select_set_style(widget_select_t* select, const widget_select_style_t* style) {
    if (select && style && !select->popup_open) select->style = *style;
}

void widget_select_set_placeholder(widget_select_t* select, const char* placeholder) {
    if (select) select->placeholder = placeholder;
}

void widget_select_set_selected(widget_select_t* select, size_t selected_index) {
    if (!select) return;
    select->selected_index = selected_index < select->option_count
        ? selected_index
        : WIDGET_SELECT_NONE;
}

size_t widget_select_selected(const widget_select_t* select) {
    return select ? select->selected_index : WIDGET_SELECT_NONE;
}

bool widget_select_open(widget_select_t* select) {
    if (!select || select->popup_open || !select->window.parent || !select->options ||
        select->option_count == 0 || select->style.item_height == 0 ||
        select->option_count > (size_t)INT_MAX / select->style.item_height) {
        return false;
    }

    widget_window_t* parent = select->window.parent;
    const widget_rect_t parent_frame = widget_window_absolute_frame(parent);
    const widget_rect_t select_frame = widget_window_absolute_frame(&select->window);
    const int border = select->style.border_width;
    const int popup_height = border * 2 + (int)select->option_count * select->style.item_height;
    if (popup_height > parent_frame.h || select_frame.w <= 0) return false;

    widget_rect_t popup = {
        .x = select_frame.x,
        .y = select_frame.y + select_frame.h,
        .w = select_frame.w,
        .h = popup_height,
    };
    if (popup.w > parent_frame.w) popup.w = parent_frame.w;
    if (popup.x + popup.w > parent_frame.x + parent_frame.w) {
        popup.x = parent_frame.x + parent_frame.w - popup.w;
    }
    if (popup.x < parent_frame.x) popup.x = parent_frame.x;
    if (popup.y + popup.h > parent_frame.y + parent_frame.h) {
        popup.y = select_frame.y - popup.h;
    }
    if (popup.y < parent_frame.y) return false;

    if (!display_pixmap_get(&select->popup_backdrop,
                            popup.x, popup.y, popup.w, popup.h)) {
        return false;
    }

    select->popup_frame = popup;
    select->popup_open = true;
    select->pressed_index = -1;
    select->popup_press_armed = false;
    select->popup_window.frame = (widget_rect_t){0, 0, parent->frame.w, parent->frame.h};
    if (!widget_window_add_child(parent, &select->popup_window)) {
        select->popup_open = false;
        display_pixmap_free(&select->popup_backdrop);
        return false;
    }

    widget_select_draw(select);
    widget_draw(&select->popup_window);
    display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
    return true;
}

void widget_select_close(widget_select_t* select) {
    if (!select || !select->popup_open) return;

    widget_window_remove(&select->popup_window);
    display_pixmap_blit(select->popup_frame.x, select->popup_frame.y,
                        &select->popup_backdrop, DISPLAY_ROTATE_0, 1, -1);
    display_pixmap_free(&select->popup_backdrop);
    select->popup_open = false;
    select->pressed_index = -1;
    select->popup_press_armed = false;
    widget_select_draw(select);
    display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
}

bool widget_select_is_open(const widget_select_t* select) {
    return select && select->popup_open;
}

void widget_select_draw(widget_select_t* select) {
    if (select) widget_draw(&select->window);
}
