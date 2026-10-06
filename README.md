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

## Proportional fonts

The BDF-like JSON files in `main/fonts` are compiled into row-packed, 1-bit C
data. The generated header exports `font_eurex24i`, `font_swiss20`, and
`font_swiss20b` as `display_font_t` values. Drawing honors each glyph's advance
and left bearing, including at all four display rotations:

```c
#include "fonts/fonts.h"

const char* label = "Proportional text";
int width = display_measure_string(&font_swiss20, label, 1);
int height = display_measure_string_height(&font_swiss20, 1);
display_draw_string_with_font((display_width() - width) / 2, 40, label,
                              &font_swiss20, DISPLAY_ROTATE_0, 1, 0x00);
```

Coordinates identify the top of the font raster. `height` and `baseline` are
available on `display_font_t` for line layout. Text is byte-oriented because
the source fonts use 8-bit character slots; an absent slot is rendered as `?`.
The original `display_draw_character()` and `display_draw_string()` functions
continue to use the fixed 8x16 font.

Regenerate the C data after editing or adding a JSON font with:

```sh
tools/font_json_to_c.py main/fonts/*.json \
  --header main/fonts/fonts.h --source main/fonts/fonts.c
```

Pass the same arguments with `--check` in CI to verify that the checked-in C
files match their JSON sources. The font-aware drawing and measurement APIs
accept `NULL` as the font to select the legacy fixed 8x16 font.

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

The background color, text color, border width, text scale, and optional
proportional font can be replaced with `widget_button_set_style()`. Leave the
style's `font` member as `NULL` to use the fixed 8x16 font. Button press-state
transitions call `display_update_with_mode(DISPLAY_UPDATE_MODE_FAST)`
immediately so the pressed-state inversion is visible. The installed M841 LUT
only changes pure black and white reliably in this slot, so button colors are
quantized to controller values `0x00` and `0xF0`. Ordinary tree drawing remains
batchable by the application.

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
