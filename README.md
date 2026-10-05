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

### Button

`main/widget_button.h` supplies a flat monochrome button class. It uses a white
background with black text and a 2 px black border normally, then inverts those
colors while captured touch input is inside its bounds. It calls `onclick` only
when the gesture is released inside the button:

```c
widget_button_t save_button;
widget_button_init(&save_button, (widget_rect_t){40, 80, 180, 64}, "SAVE",
                   save_clicked, save_context);
widget_window_add_child(&root_window, &save_button.window);
```

The background color, text color, border width, and text scale can be replaced with
`widget_button_set_style()`. Button press-state transitions call
`display_update_with_mode(DISPLAY_UPDATE_MODE_FAST)` immediately so the
pressed-state inversion is visible. The installed M841 LUT only changes pure
black and white reliably in this slot, so button colors are quantized to
controller values `0x00` and `0xF0`. Ordinary tree drawing remains batchable
by the application.

## E-paper update modes

`display_update()` remains the high-quality GC16 default.
`display_update_with_mode()` also exposes the M5Paper waveform slots DU, GL16,
GLR16, GLD16, FAST/DU4, and A2. Waveform contents depend on the installed LUT.
On the detected M841 LUT, slot 6 is the fastest interactive mode and should
only target black or white. `DISPLAY_UPDATE_MODE_DU4` remains an alias for
`DISPLAY_UPDATE_MODE_FAST` to match the naming used by M5Stack's driver.

Fast updates accumulate ghosting. The display layer remembers the union of
regions changed by DU, DU4, and A2 and automatically refreshes that area with
GC16 every 32 fast updates. Any GC16 update that covers the accumulated region
also resets the cleanup counter. IT8951 timing logs include the selected mode,
upload time, and panel update time.

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
