# M5Paper FreeRTOS UI

This repository contains the reusable `m5paper_ui` ESP-IDF component and a
demo application that exercises it on the original M5Stack M5Paper.

## Reusable component

The component lives in `components/m5paper_ui`; the project in `main` is only
an example consumer. To use it in another ESP-IDF 5.x project, copy the
component directory into that project's `components` directory, or reference
this repository and component path from `idf_component.yml`:

```yaml
dependencies:
  m5paper_ui:
    git: https://github.com/adh/m5paper-freertos.git
    path: components/m5paper_ui
```

The component targets `esp32` and currently uses the original M5Paper's fixed
GPIO assignments. See
[`components/m5paper_ui/README.md`](components/m5paper_ui/README.md) for its
contents and a minimal initialization example.

## Demo application

The application opens on a menu instead of combining every test on one screen.
Its focused screens are:

- **UI widgets** — buttons, independent checkboxes, and an exclusive radio group.
- **Primitives** — grayscale shapes, lines, ellipses, and bundled fonts.
- **Touch** — a finger-drawing surface using fast monochrome partial updates.
- **Animation** — a moving pixmap demonstrating GC16 damage-region updates.
- **System** — live battery voltage, display dimensions, and IT8951 firmware/LUT.

Every demonstration has a touch Back button. Pressing the physical center
button also returns directly to the menu. Static screens wait indefinitely for
input, while Animation and System request their own event-loop timeouts.

## Widget tree

`components/m5paper_ui/include/widget.h` provides an allocation-free window
tree. Applications own the `widget_window_t` objects, attach them with
`widget_window_add_child()`, and give each window a `widget_class_t` containing
optional draw and event callbacks. Window frames are relative to their parents;
later children are in front of earlier children.

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

The BDF-like JSON files in `components/m5paper_ui/fonts` are compiled into
row-packed, 1-bit C data. The generated header exports `font_eurex24i`,
`font_swiss20`, and `font_swiss20b` as `display_font_t` values. Drawing honors
each glyph's advance and left bearing, including at all four display rotations:

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

Related regular, bold, italic, and bold-italic faces are exposed as generated
`display_font_family_t` values such as `font_family_swiss20`. Font-aware string
drawing and measurement recognize a small ANSI SGR subset and select the
corresponding face automatically:

```c
display_draw_string_with_font(24, 120,
    "Status: " DISPLAY_FONT_BOLD_ON "bold" DISPLAY_FONT_BOLD_OFF ", "
    DISPLAY_FONT_ITALIC_ON "italic" DISPLAY_FONT_STYLE_RESET,
    &font_swiss20, DISPLAY_ROTATE_0, 1, 0x00);
```

Supported parameters are `0` (reset), `1`/`22` (bold on/off), and `3`/`23`
(italic on/off); combined sequences such as `\x1b[1;3m` are accepted. When a
family lacks the requested combined face, drawing falls back to the closest
available face. Escape bytes do not contribute to `display_measure_string()`.
The corresponding string-literal macros are declared in `display_font.h`,
including `DISPLAY_FONT_BOLD_ITALIC_ON` for the combined form.

Regenerate the C data after editing or adding a JSON font with:

```sh
tools/font_json_to_c.py components/m5paper_ui/fonts/*.json \
  --header components/m5paper_ui/include/fonts/fonts.h \
  --source components/m5paper_ui/src/fonts.c
```

Pass the same arguments with `--check` in CI to verify that the checked-in C
files match their JSON sources. The font-aware drawing and measurement APIs
accept `NULL` as the font to select the legacy fixed 8x16 font.

### Button

`components/m5paper_ui/include/widget_button.h` supplies a flat monochrome
button class. It uses a white background with black text and a 2 px black
border normally, then inverts those colors while captured touch input is inside
its bounds. It calls `onclick` only when the gesture is released inside the
button:

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

### Checkbox

`components/m5paper_ui/include/widget_checkbox.h` supplies an independently
toggled checkbox. Its whole window is the touch target, and `onchange` receives
the new checked state after a successful release:

```c
widget_checkbox_t wifi_checkbox;
widget_checkbox_init(&wifi_checkbox, (widget_rect_t){40, 80, 300, 58},
                     "Wi-Fi", true, wifi_changed, context);
widget_window_add_child(&root_window, &wifi_checkbox.window);
```

`widget_checkbox_set_checked()` changes the model without invoking the callback
or refreshing the display, allowing callers to batch programmatic changes.

### Radio button

Radio buttons share a caller-owned `widget_radio_group_t`. The group points to
the selected member, so selecting a new member automatically clears and redraws
the previous selection:

```c
widget_radio_group_t size_group;
widget_radio_button_t small_button;
widget_radio_button_t large_button;

widget_radio_group_init(&size_group);
widget_radio_button_init(&small_button, &size_group,
    (widget_rect_t){40, 80, 300, 58}, "Small", true, size_changed, context);
widget_radio_button_init(&large_button, &size_group,
    (widget_rect_t){40, 145, 300, 58}, "Large", false, size_changed, context);
```

Checkboxes and radio buttons use the same monochrome press inversion and fast
partial refresh as ordinary buttons. Their style structures control colors,
indicator size and border, label spacing, font, and text scale.

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

