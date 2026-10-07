#ifndef H__widget_edit__
#define H__widget_edit__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "display_font.h"
#include "widget.h"
#include "widget_button.h"

#define WIDGET_EDIT_MAX_LENGTH 63
#define WIDGET_EDIT_KEY_COUNT 32

typedef struct widget_edit_s widget_edit_t;
typedef struct widget_edit_key_s widget_edit_key_t;
typedef void (*widget_edit_onchange_t)(widget_edit_t* edit, const char* text, void* context);

typedef struct widget_edit_style_s {
    uint8_t background_color;
    uint8_t text_color;
    uint16_t border_width;
    uint16_t padding;
    uint16_t text_scale;
    const display_font_t* font;
} widget_edit_style_t;

struct widget_edit_key_s {
    widget_button_t button;
    widget_edit_t* edit;
    uint8_t action;
    char value;
    char label[8];
};

struct widget_edit_s {
    widget_window_t window;
    widget_window_t editor_window;
    widget_button_t cancel_button;
    widget_edit_key_t keys[WIDGET_EDIT_KEY_COUNT];
    const char* title;
    const char* placeholder;
    widget_edit_onchange_t onchange;
    void* onchange_context;
    widget_edit_style_t style;
    char text[WIDGET_EDIT_MAX_LENGTH + 1];
    char draft[WIDGET_EDIT_MAX_LENGTH + 1];
    size_t draft_length;
    bool pressed;
    bool editor_open;
    bool uppercase;
    bool symbols;
};

extern const widget_edit_style_t WIDGET_EDIT_DEFAULT_STYLE;

void widget_edit_init(widget_edit_t* edit, widget_rect_t frame, const char* initial_text,
                      widget_edit_onchange_t onchange, void* context);
void widget_edit_set_style(widget_edit_t* edit, const widget_edit_style_t* style);
void widget_edit_set_title(widget_edit_t* edit, const char* title);
void widget_edit_set_placeholder(widget_edit_t* edit, const char* placeholder);
void widget_edit_set_text(widget_edit_t* edit, const char* text);
const char* widget_edit_text(const widget_edit_t* edit);
bool widget_edit_open(widget_edit_t* edit);
void widget_edit_close(widget_edit_t* edit, bool accept);
bool widget_edit_is_open(const widget_edit_t* edit);
void widget_edit_draw(widget_edit_t* edit);

#endif
