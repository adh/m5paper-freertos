#ifndef H__widget__
#define H__widget__

#include <stdbool.h>

#include "input.h"

typedef struct widget_window_s widget_window_t;

typedef struct widget_rect_s {
    int x;
    int y;
    int w;
    int h;
} widget_rect_t;

typedef struct widget_class_s {
    void (*draw)(widget_window_t* window);
    bool (*event)(widget_window_t* window, const input_event_t* event);
} widget_class_t;

struct widget_window_s {
    const widget_class_t* window_class;
    widget_rect_t frame;
    void* context;

    widget_window_t* parent;
    widget_window_t* first_child;
    widget_window_t* last_child;
    widget_window_t* previous_sibling;
    widget_window_t* next_sibling;

    bool visible;
    bool enabled;

    /* Private dispatcher state. Only meaningful on a root window. */
    widget_window_t* touch_capture;
};

void widget_window_init(widget_window_t* window, const widget_class_t* window_class,
                        widget_rect_t frame, void* context);
bool widget_window_add_child(widget_window_t* parent, widget_window_t* child);
void widget_window_remove(widget_window_t* window);
void widget_window_set_visible(widget_window_t* window, bool visible);
void widget_window_set_enabled(widget_window_t* window, bool enabled);
widget_rect_t widget_window_absolute_frame(const widget_window_t* window);
bool widget_event_local_point(const widget_window_t* window, const input_event_t* event,
                              int* x, int* y);
widget_window_t* widget_window_hit_test(widget_window_t* root, int x, int y);
void widget_draw(widget_window_t* root);
bool widget_dispatch_event(widget_window_t* root, const input_event_t* event);

#endif
