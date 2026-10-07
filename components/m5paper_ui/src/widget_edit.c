#include "widget_edit.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "fonts/fonts.h"

#define EDIT_FIELD_X 24
#define EDIT_FIELD_Y 100
#define EDIT_FIELD_HEIGHT 76
#define EDIT_KEYBOARD_TOP 580
#define EDIT_KEY_HEIGHT 78
#define EDIT_KEY_GAP 8

enum edit_key_action_e {
    EDIT_KEY_CHARACTER = 0,
    EDIT_KEY_SHIFT,
    EDIT_KEY_MODE,
    EDIT_KEY_SPACE,
    EDIT_KEY_BACKSPACE,
    EDIT_KEY_DONE,
};

const widget_edit_style_t WIDGET_EDIT_DEFAULT_STYLE = {
    .background_color = 0xF0,
    .text_color = 0x00,
    .border_width = 3,
    .padding = 12,
    .text_scale = 1,
    .font = &font_swiss20,
};

static widget_edit_t* edit_from_window(widget_window_t* window) {
    return window ? (widget_edit_t*)window->context : NULL;
}

static uint8_t fast_gray(uint8_t gray) {
    return gray < 0x80 ? 0x00 : 0xF0;
}

static void copy_text(char* destination, const char* source) {
    if (!source) source = "";
    snprintf(destination, WIDGET_EDIT_MAX_LENGTH + 1, "%s", source);
}

static void draw_fitted_text(const widget_edit_t* edit, int x, int y, int w, int h,
                             const char* text, uint8_t color) {
    if (!text || !*text || w <= 0 || h <= 0 || edit->style.text_scale == 0) return;

    uint16_t scale = edit->style.text_scale;
    while (scale > 0 &&
           (display_measure_string(edit->style.font, text, scale) > w ||
            display_measure_string_height(edit->style.font, scale) > h)) {
        --scale;
    }
    if (scale == 0) return;

    const int text_height = display_measure_string_height(edit->style.font, scale);
    display_draw_string_with_font(x, y + (h - text_height) / 2, text, edit->style.font,
                                  DISPLAY_ROTATE_0, scale, color);
}

static void draw_edit_class(widget_window_t* window) {
    widget_edit_t* edit = edit_from_window(window);
    if (!edit || window->frame.w <= 0 || window->frame.h <= 0) return;

    const widget_rect_t frame = widget_window_absolute_frame(window);
    const uint8_t normal_background = fast_gray(edit->style.background_color);
    const uint8_t normal_text = fast_gray(edit->style.text_color);
    const uint8_t background = edit->pressed ? normal_text : normal_background;
    const uint8_t foreground = edit->pressed ? normal_background : normal_text;
    int border_width = edit->style.border_width;
    const int max_border = (frame.w < frame.h ? frame.w : frame.h) / 2;
    if (border_width > max_border) border_width = max_border;

    display_fill_rect(frame.x, frame.y, frame.w, frame.h, background);
    display_stroke_rect(frame.x, frame.y, frame.w, frame.h, border_width, foreground);
    const char* text = edit->text[0] ? edit->text : edit->placeholder;
    draw_fitted_text(edit, frame.x + border_width + edit->style.padding,
                     frame.y + border_width,
                     frame.w - border_width * 2 - edit->style.padding * 2,
                     frame.h - border_width * 2, text, foreground);
}

static const char* visible_draft(const widget_edit_t* edit, int available_width) {
    const char* text = edit->draft;
    while (*text && display_measure_string(edit->style.font, text,
                                            edit->style.text_scale) > available_width) {
        ++text;
    }
    return text;
}

static void draw_draft_field(widget_edit_t* edit) {
    const widget_rect_t editor = widget_window_absolute_frame(&edit->editor_window);
    const int x = editor.x + EDIT_FIELD_X;
    const int y = editor.y + EDIT_FIELD_Y;
    const int w = editor.w - EDIT_FIELD_X * 2;
    const int border = edit->style.border_width;
    const uint8_t background = fast_gray(edit->style.background_color);
    const uint8_t foreground = fast_gray(edit->style.text_color);
    const int content_x = x + border + edit->style.padding;
    const int content_w = w - border * 2 - edit->style.padding * 2 - 4;

    display_fill_rect(x, y, w, EDIT_FIELD_HEIGHT, background);
    display_stroke_rect(x, y, w, EDIT_FIELD_HEIGHT, border, foreground);
    const char* visible = visible_draft(edit, content_w);
    draw_fitted_text(edit, content_x, y + border, content_w,
                     EDIT_FIELD_HEIGHT - border * 2, visible, foreground);
    int cursor_x = content_x + display_measure_string(edit->style.font, visible,
                                                       edit->style.text_scale);
    if (cursor_x > x + w - border - 3) cursor_x = x + w - border - 3;
    display_fill_rect(cursor_x, y + 14, 3, EDIT_FIELD_HEIGHT - 28, foreground);
}

static void draw_editor_class(widget_window_t* window) {
    widget_edit_t* edit = edit_from_window(window);
    if (!edit) return;

    const widget_rect_t frame = widget_window_absolute_frame(window);
    display_fill_rect(frame.x, frame.y, frame.w, frame.h,
                      fast_gray(edit->style.background_color));
    display_draw_string_with_font(frame.x + 24, frame.y + 22,
                                  edit->title ? edit->title : "Edit value",
                                  &font_eurex24i, DISPLAY_ROTATE_0, 1,
                                  fast_gray(edit->style.text_color));
    draw_draft_field(edit);
}

static bool editor_event_class(widget_window_t* window, const input_event_t* event) {
    (void)window;
    return event->type == INPUT_EVENT_TOUCH_PRESS ||
           event->type == INPUT_EVENT_TOUCH_MOVE ||
           event->type == INPUT_EVENT_TOUCH_RELEASE;
}

static bool edit_event_class(widget_window_t* window, const input_event_t* event) {
    widget_edit_t* edit = edit_from_window(window);
    int x;
    int y;
    if (!edit || !widget_event_local_point(window, event, &x, &y)) return false;

    const bool inside = x >= 0 && y >= 0 && x < window->frame.w && y < window->frame.h;
    if (event->type == INPUT_EVENT_TOUCH_PRESS) {
        edit->pressed = true;
        widget_edit_draw(edit);
        display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_MOVE) {
        if (edit->pressed != inside) {
            edit->pressed = inside;
            widget_edit_draw(edit);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }
    if (event->type == INPUT_EVENT_TOUCH_RELEASE) {
        const bool activate = edit->pressed && inside;
        edit->pressed = false;
        if (!activate || !widget_edit_open(edit)) {
            widget_edit_draw(edit);
            display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
        }
        return true;
    }
    return false;
}

static const widget_class_t s_edit_class = {
    .draw = draw_edit_class,
    .event = edit_event_class,
};

static const widget_class_t s_editor_class = {
    .draw = draw_editor_class,
    .event = editor_event_class,
};

static void set_key(widget_edit_key_t* key, widget_rect_t frame, const char* label,
                    uint8_t action, char value, bool visible) {
    key->button.window.frame = frame;
    key->action = action;
    key->value = value;
    snprintf(key->label, sizeof(key->label), "%s", label ? label : "");
    widget_button_set_label(&key->button, key->label);
    widget_window_set_visible(&key->button.window, visible);
}

static int configure_character_row(widget_edit_t* edit, int key_index, int y,
                                   const char* characters, int x, int width) {
    const int count = (int)strlen(characters);
    const int gap = 4;
    const int key_width = (width - gap * (count - 1)) / count;
    for (int i = 0; i < count; ++i) {
        char value = characters[i];
        if (edit->uppercase) value = (char)toupper((unsigned char)value);
        char label[2] = {value, '\0'};
        set_key(&edit->keys[key_index++],
                (widget_rect_t){x + i * (key_width + gap), y, key_width, EDIT_KEY_HEIGHT},
                label, EDIT_KEY_CHARACTER, value, true);
    }
    return key_index;
}

static int configure_special_row(widget_edit_t* edit, int key_index, int y,
                                 const char* characters, const char* mode_label,
                                 uint8_t mode_action) {
    const int character_count = (int)strlen(characters);
    const int count = character_count + 2;
    const int x = 10;
    const int width = 520;
    const int gap = 4;
    const int key_width = (width - gap * (count - 1)) / count;
    set_key(&edit->keys[key_index++], (widget_rect_t){x, y, key_width, EDIT_KEY_HEIGHT},
            mode_label, mode_action, 0, true);
    for (int i = 0; i < character_count; ++i) {
        char value = characters[i];
        if (edit->uppercase) value = (char)toupper((unsigned char)value);
        char label[2] = {value, '\0'};
        set_key(&edit->keys[key_index++],
                (widget_rect_t){x + (i + 1) * (key_width + gap), y,
                                key_width, EDIT_KEY_HEIGHT},
                label, EDIT_KEY_CHARACTER, value, true);
    }
    set_key(&edit->keys[key_index++],
            (widget_rect_t){x + (count - 1) * (key_width + gap), y,
                            key_width, EDIT_KEY_HEIGHT},
            "DEL", EDIT_KEY_BACKSPACE, 0, true);
    return key_index;
}

static int configure_symbol_row(widget_edit_t* edit, int key_index, int y,
                                const char* characters) {
    const int character_count = (int)strlen(characters);
    const int count = character_count + 1;
    const int x = 10;
    const int width = 520;
    const int gap = 4;
    const int key_width = (width - gap * (count - 1)) / count;
    for (int i = 0; i < character_count; ++i) {
        char label[2] = {characters[i], '\0'};
        set_key(&edit->keys[key_index++],
                (widget_rect_t){x + i * (key_width + gap), y,
                                key_width, EDIT_KEY_HEIGHT},
                label, EDIT_KEY_CHARACTER, characters[i], true);
    }
    set_key(&edit->keys[key_index++],
            (widget_rect_t){x + (count - 1) * (key_width + gap), y,
                            key_width, EDIT_KEY_HEIGHT},
            "DEL", EDIT_KEY_BACKSPACE, 0, true);
    return key_index;
}

static void configure_keyboard(widget_edit_t* edit) {
    int key_index = 0;
    const int row1_y = EDIT_KEYBOARD_TOP;
    const int row2_y = row1_y + EDIT_KEY_HEIGHT + EDIT_KEY_GAP;
    const int row3_y = row2_y + EDIT_KEY_HEIGHT + EDIT_KEY_GAP;
    const int row4_y = row3_y + EDIT_KEY_HEIGHT + EDIT_KEY_GAP;

    if (edit->symbols) {
        key_index = configure_character_row(edit, key_index, row1_y,
                                            "1234567890", 10, 520);
        key_index = configure_character_row(edit, key_index, row2_y,
                                            "-/:;()$&@", 28, 484);
        key_index = configure_symbol_row(edit, key_index, row3_y, ".,?!#+=\"");
    } else {
        key_index = configure_character_row(edit, key_index, row1_y,
                                            "qwertyuiop", 10, 520);
        key_index = configure_character_row(edit, key_index, row2_y,
                                            "asdfghjkl", 28, 484);
        key_index = configure_special_row(edit, key_index, row3_y,
                                          "zxcvbnm", "SHIFT", EDIT_KEY_SHIFT);
    }

    set_key(&edit->keys[key_index++], (widget_rect_t){10, row4_y, 82, EDIT_KEY_HEIGHT},
            edit->symbols ? "ABC" : "123", EDIT_KEY_MODE, 0, true);
    set_key(&edit->keys[key_index++], (widget_rect_t){98, row4_y, 250, EDIT_KEY_HEIGHT},
            "Space", EDIT_KEY_SPACE, 0, true);
    set_key(&edit->keys[key_index++], (widget_rect_t){354, row4_y, 62, EDIT_KEY_HEIGHT},
            ".", EDIT_KEY_CHARACTER, '.', true);
    set_key(&edit->keys[key_index++], (widget_rect_t){422, row4_y, 108, EDIT_KEY_HEIGHT},
            "Done", EDIT_KEY_DONE, 0, true);

    while (key_index < WIDGET_EDIT_KEY_COUNT) {
        widget_window_set_visible(&edit->keys[key_index++].button.window, false);
    }
}

static void redraw_keyboard(widget_edit_t* edit) {
    const widget_rect_t editor = widget_window_absolute_frame(&edit->editor_window);
    configure_keyboard(edit);
    display_fill_rect(editor.x, editor.y + EDIT_KEYBOARD_TOP,
                      editor.w, editor.h - EDIT_KEYBOARD_TOP,
                      fast_gray(edit->style.background_color));
    for (size_t i = 0; i < WIDGET_EDIT_KEY_COUNT; ++i) {
        widget_edit_key_t* key = &edit->keys[i];
        if (key->button.window.visible) widget_button_draw(&key->button);
    }
    display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
}

static void redraw_draft(widget_edit_t* edit) {
    draw_draft_field(edit);
    display_update_with_mode(DISPLAY_UPDATE_MODE_FAST);
}

static void cancel_clicked(widget_button_t* button, void* context) {
    (void)button;
    widget_edit_close((widget_edit_t*)context, false);
}

static void key_clicked(widget_button_t* button, void* context) {
    (void)button;
    widget_edit_key_t* key = (widget_edit_key_t*)context;
    widget_edit_t* edit = key->edit;
    switch (key->action) {
        case EDIT_KEY_CHARACTER:
            if (edit->draft_length < WIDGET_EDIT_MAX_LENGTH) {
                edit->draft[edit->draft_length++] = key->value;
                edit->draft[edit->draft_length] = '\0';
                redraw_draft(edit);
            }
            break;
        case EDIT_KEY_SPACE:
            if (edit->draft_length < WIDGET_EDIT_MAX_LENGTH) {
                edit->draft[edit->draft_length++] = ' ';
                edit->draft[edit->draft_length] = '\0';
                redraw_draft(edit);
            }
            break;
        case EDIT_KEY_BACKSPACE:
            if (edit->draft_length > 0) {
                edit->draft[--edit->draft_length] = '\0';
                redraw_draft(edit);
            }
            break;
        case EDIT_KEY_SHIFT:
            edit->uppercase = !edit->uppercase;
            redraw_keyboard(edit);
            break;
        case EDIT_KEY_MODE:
            edit->symbols = !edit->symbols;
            edit->uppercase = false;
            redraw_keyboard(edit);
            break;
        case EDIT_KEY_DONE:
            widget_edit_close(edit, true);
            break;
        default:
            break;
    }
}

void widget_edit_init(widget_edit_t* edit, widget_rect_t frame, const char* initial_text,
                      widget_edit_onchange_t onchange, void* context) {
    if (!edit) return;
    *edit = (widget_edit_t){
        .title = "Edit value",
        .placeholder = "Tap to edit",
        .onchange = onchange,
        .onchange_context = context,
        .style = WIDGET_EDIT_DEFAULT_STYLE,
    };
    copy_text(edit->text, initial_text);
    widget_window_init(&edit->window, &s_edit_class, frame, edit);
    widget_window_init(&edit->editor_window, &s_editor_class, (widget_rect_t){0}, edit);
    widget_button_init(&edit->cancel_button, (widget_rect_t){366, 12, 150, 52},
                       "Cancel", cancel_clicked, edit);
    widget_window_add_child(&edit->editor_window, &edit->cancel_button.window);
    for (size_t i = 0; i < WIDGET_EDIT_KEY_COUNT; ++i) {
        widget_edit_key_t* key = &edit->keys[i];
        key->edit = edit;
        widget_button_init(&key->button, (widget_rect_t){0}, "", key_clicked, key);
        widget_window_add_child(&edit->editor_window, &key->button.window);
    }
    configure_keyboard(edit);
}

void widget_edit_set_style(widget_edit_t* edit, const widget_edit_style_t* style) {
    if (edit && style && !edit->editor_open) edit->style = *style;
}

void widget_edit_set_title(widget_edit_t* edit, const char* title) {
    if (edit) edit->title = title;
}

void widget_edit_set_placeholder(widget_edit_t* edit, const char* placeholder) {
    if (edit) edit->placeholder = placeholder;
}

void widget_edit_set_text(widget_edit_t* edit, const char* text) {
    if (edit && !edit->editor_open) copy_text(edit->text, text);
}

const char* widget_edit_text(const widget_edit_t* edit) {
    return edit ? edit->text : "";
}

bool widget_edit_open(widget_edit_t* edit) {
    if (!edit || edit->editor_open || !edit->window.parent) return false;

    widget_window_t* parent = edit->window.parent;
    copy_text(edit->draft, edit->text);
    edit->draft_length = strlen(edit->draft);
    edit->uppercase = false;
    edit->symbols = false;
    configure_keyboard(edit);
    edit->editor_window.frame = (widget_rect_t){0, 0, parent->frame.w, parent->frame.h};
    edit->editor_open = true;
    if (!widget_window_add_child(parent, &edit->editor_window)) {
        edit->editor_open = false;
        return false;
    }
    widget_draw(&edit->editor_window);
    display_update();
    return true;
}

void widget_edit_close(widget_edit_t* edit, bool accept) {
    if (!edit || !edit->editor_open) return;

    const bool changed = accept && strcmp(edit->text, edit->draft) != 0;
    if (accept) copy_text(edit->text, edit->draft);
    widget_window_t* parent = edit->editor_window.parent;
    widget_window_remove(&edit->editor_window);
    edit->editor_open = false;
    edit->pressed = false;
    if (parent) widget_draw(parent);
    display_update();
    if (changed && edit->onchange) {
        edit->onchange(edit, edit->text, edit->onchange_context);
    }
}

bool widget_edit_is_open(const widget_edit_t* edit) {
    return edit && edit->editor_open;
}

void widget_edit_draw(widget_edit_t* edit) {
    if (edit) widget_draw(&edit->window);
}
