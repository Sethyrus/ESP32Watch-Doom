idf_component_register(
    SRCS
        "doom_app.c"
        "doom_port.c"
    INCLUDE_DIRS "include"
    REQUIRES
        doomgeneric
        esp_driver_gpio
        esp_timer
        esp_lcd
        espressif__esp_lcd_touch
        freertos
        heap
        log
        waveshare__esp32_s3_touch_amoled_2_06
    WHOLE_ARCHIVE
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CMAP256
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=240
)
