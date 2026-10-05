#include "widget.h"

#include <stddef.h>

static widget_window_t* widget_root(widget_window_t* window) {
    while (window && window->parent) {
        window = window->parent;
    }
    return window;
}

static bool widget_is_ancestor(const widget_window_t* ancestor, const widget_window_t* window) {
    while (window) {
        if (window == ancestor) {
            return true;
        }
        window = window->parent;
    }
    return false;
}

void widget_window_init(widget_window_t* window, const widget_class_t* window_class,
                        widget_rect_t frame, void* context) {
    if (!window) {
        return;
    }

    *window = (widget_window_t){
        .window_class = window_class,
        .frame = frame,
        .context = context,
        .visible = true,
        .enabled = true,
    };
}

bool widget_window_add_child(widget_window_t* parent, widget_window_t* child) {
    if (!parent || !child || parent == child || widget_is_ancestor(child, parent)) {
        return false;
    }

    widget_window_remove(child);
    child->parent = parent;
    child->previous_sibling = parent->last_child;
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
    return true;
}

void widget_window_remove(widget_window_t* window) {
    if (!window || !window->parent) {
        return;
    }

    widget_window_t* root = widget_root(window);
    if (root->touch_capture && widget_is_ancestor(window, root->touch_capture)) {
        root->touch_capture = NULL;
    }

    widget_window_t* parent = window->parent;
    if (window->previous_sibling) {
        window->previous_sibling->next_sibling = window->next_sibling;
    } else {
        parent->first_child = window->next_sibling;
    }
    if (window->next_sibling) {
        window->next_sibling->previous_sibling = window->previous_sibling;
    } else {
        parent->last_child = window->previous_sibling;
    }

    window->parent = NULL;
    window->previous_sibling = NULL;
    window->next_sibling = NULL;
}

void widget_window_set_visible(widget_window_t* window, bool visible) {
    if (!window) {
        return;
    }
    window->visible = visible;
    if (!visible) {
        widget_window_t* root = widget_root(window);
        if (root->touch_capture && widget_is_ancestor(window, root->touch_capture)) {
            root->touch_capture = NULL;
        }
    }
}

void widget_window_set_enabled(widget_window_t* window, bool enabled) {
    if (!window) {
        return;
    }
    window->enabled = enabled;
    if (!enabled) {
        widget_window_t* root = widget_root(window);
        if (root->touch_capture && widget_is_ancestor(window, root->touch_capture)) {
            root->touch_capture = NULL;
        }
    }
}

widget_rect_t widget_window_absolute_frame(const widget_window_t* window) {
    widget_rect_t frame = {0};
    if (!window) {
        return frame;
    }

    frame = window->frame;
    for (const widget_window_t* parent = window->parent; parent; parent = parent->parent) {
        frame.x += parent->frame.x;
        frame.y += parent->frame.y;
    }
    return frame;
}

bool widget_event_local_point(const widget_window_t* window, const input_event_t* event,
                              int* x, int* y) {
    if (!window || !event || !x || !y ||
        (event->type != INPUT_EVENT_TOUCH_PRESS &&
         event->type != INPUT_EVENT_TOUCH_MOVE &&
         event->type != INPUT_EVENT_TOUCH_RELEASE)) {
        return false;
    }

    const widget_rect_t frame = widget_window_absolute_frame(window);
    *x = (int)event->touch.x - frame.x;
    *y = (int)event->touch.y - frame.y;
    return true;
}

static bool widget_rect_contains(widget_rect_t rect, int x, int y) {
    return rect.w > 0 && rect.h > 0 &&
           x >= rect.x && y >= rect.y &&
           x < rect.x + rect.w && y < rect.y + rect.h;
}

widget_window_t* widget_window_hit_test(widget_window_t* root, int x, int y) {
    if (!root || !root->visible || !root->enabled ||
        !widget_rect_contains(widget_window_absolute_frame(root), x, y)) {
        return NULL;
    }

    for (widget_window_t* child = root->last_child; child; child = child->previous_sibling) {
        widget_window_t* hit = widget_window_hit_test(child, x, y);
        if (hit) {
            return hit;
        }
    }
    return root;
}

void widget_draw(widget_window_t* root) {
    if (!root || !root->visible) {
        return;
    }

    if (root->window_class && root->window_class->draw) {
        root->window_class->draw(root);
    }
    for (widget_window_t* child = root->first_child; child; child = child->next_sibling) {
        widget_draw(child);
    }
}

static widget_window_t* widget_bubble_event(widget_window_t* root, widget_window_t* target,
                                             const input_event_t* event) {
    while (target) {
        widget_window_t* parent = target->parent;
        if (target->visible && target->enabled && target->window_class &&
            target->window_class->event && target->window_class->event(target, event)) {
            return target;
        }
        if (target == root) {
            break;
        }
        target = parent;
    }
    return NULL;
}

bool widget_dispatch_event(widget_window_t* root, const input_event_t* event) {
    if (!root || !event || !root->visible || !root->enabled) {
        return false;
    }

    widget_window_t* handled_by = NULL;
    switch (event->type) {
        case INPUT_EVENT_TOUCH_PRESS:
            root->touch_capture = NULL;
            handled_by = widget_bubble_event(root,
                widget_window_hit_test(root, event->touch.x, event->touch.y), event);
            if (handled_by && widget_is_ancestor(root, handled_by)) {
                root->touch_capture = handled_by;
            }
            break;

        case INPUT_EVENT_TOUCH_MOVE:
            if (root->touch_capture) {
                handled_by = widget_bubble_event(root, root->touch_capture, event);
            } else {
                handled_by = widget_bubble_event(root,
                    widget_window_hit_test(root, event->touch.x, event->touch.y), event);
            }
            break;

        case INPUT_EVENT_TOUCH_RELEASE:
            if (root->touch_capture) {
                handled_by = widget_bubble_event(root, root->touch_capture, event);
            } else {
                handled_by = widget_bubble_event(root,
                    widget_window_hit_test(root, event->touch.x, event->touch.y), event);
            }
            root->touch_capture = NULL;
            break;

        default:
            handled_by = widget_bubble_event(root, root, event);
            break;
    }

    return handled_by != NULL;
}
