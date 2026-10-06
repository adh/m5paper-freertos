idf_component_register(
    SRCS
        "src/display.c"
        "src/fonts.c"
        "src/gt911.c"
        "src/input.c"
        "src/it8951.c"
        "src/m5paper.c"
        "src/widget.c"
        "src/widget_button.c"
        "src/widget_checkbox.c"
        "src/widget_radio_button.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES driver
    PRIV_REQUIRES esp_adc esp_hw_support esp_timer
)
