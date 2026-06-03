idf_component_register(
    SRCS
        "doom_app.c"
        "doom_port.c"
    INCLUDE_DIRS "include"
    REQUIRES
        doomgeneric
        esp_timer
        esp_lcd
        freertos
        heap
        log
        waveshare__esp32_s3_touch_amoled_2_06
    WHOLE_ARCHIVE
)
