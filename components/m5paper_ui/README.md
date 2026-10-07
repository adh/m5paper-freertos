# m5paper_ui

`m5paper_ui` is an ESP-IDF component for the original M5Stack M5Paper
(ESP32). It contains:

- M5Paper power and battery support
- the IT8951 e-paper display driver and drawing API
- GT911 touch and physical-button input
- fixed and proportional bitmap fonts
- an allocation-free widget tree with button, checkbox, radio, select, and edit controls

## Add to a project

Copy this directory to `components/m5paper_ui` in an ESP-IDF project, or use
it as a Git dependency from an ESP-IDF Component Manager manifest:

```yaml
dependencies:
  m5paper_ui:
    git: https://github.com/adh/m5paper-freertos.git
    path: components/m5paper_ui
```

The component supports the `esp32` target and ESP-IDF 5.x. It currently uses
the M5Paper board's fixed GPIO assignments.

## Minimal application

```c
#include <stddef.h>

#include "display.h"
#include "input.h"
#include "m5paper.h"
#include "widget.h"

void app_main(void)
{
    m5paper_init();
    display_init(2300);
    input_init();

    widget_window_t root;
    widget_window_init(&root, NULL,
                       (widget_rect_t){0, 0, display_width(), display_height()},
                       NULL);
    widget_draw(&root);
    display_update();
}
```

Public headers are under `include/`. Bundled fonts are available through
`fonts/fonts.h`.
