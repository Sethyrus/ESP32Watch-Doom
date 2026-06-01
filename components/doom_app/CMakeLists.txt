idf_component_register(
    SRCS
        "doom_app.c"
        "doom_port.c"
    INCLUDE_DIRS "include"
    REQUIRES
        doomgeneric
        esp_timer
        freertos
        heap
        log
    WHOLE_ARCHIVE
)
