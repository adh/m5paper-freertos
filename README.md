# M5Paper FreeRTOS playground

## Widget tree

`main/widget.h` provides an allocation-free window tree. Applications own the
`widget_window_t` objects, attach them with `widget_window_add_child()`, and
give each window a `widget_class_t` containing optional draw and event
callbacks. Window frames are relative to their parents; later children are in
front of earlier children.

The application event loop is intentionally small:

```c
input_event_t event;
if (input_wait_for_event_or_timeout(&event, timeout_ms) == ESP_OK) {
    widget_dispatch_event(&root_window, &event);
}
```

Touch coordinates in `input_event_t` are display coordinates. The original
GT911 sample remains available as `event.touch.raw`. A press is hit-tested
against the deepest, topmost visible and enabled window. Events bubble toward
the root until handled, and the handler that accepts a press captures all move
and release events for that gesture. Directional-button and timeout events are
sent to the root. `widget_event_local_point()` converts a touch event to a
window's local coordinate system.

Call `widget_draw(&root_window)` to draw visible windows in back-to-front tree
order. Each class draws into the existing display framebuffer; `display_update()`
remains under application control so a caller can batch several changes into a
single e-paper refresh.

# Original ESP-IDF sample notes

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This is the simplest buildable example. The example is used by command `idf.py create-project`
that copies the project to user specified path and set it's name. For more information follow the [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)



## How to use example
We encourage the users to use the example as a template for the new projects.
A recommended way is to follow the instructions on a [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project).

## Example folder contents

The project **sample_project** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```
Additionally, the sample project contains Makefile and component.mk files, used for the legacy Make based build system. 
They are not used or needed when building with CMake and idf.py.
