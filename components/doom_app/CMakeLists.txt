idf_component_register(
    SRCS
        "doom_app.c"
        "doom_port.c"
        "i_sound_esp32.c"
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
        fatfs
        esp_codec_dev
    WHOLE_ARCHIVE
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CMAP256
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=240
    FEATURE_SOUND=1
)

# --- WAD Embedding Logic ---
set(WAD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/wad")
if(EXISTS "${WAD_SOURCE_DIR}/doom.wad")
    set(WAD_FILE "${WAD_SOURCE_DIR}/doom.wad")
elseif(EXISTS "${WAD_SOURCE_DIR}/doom1.wad")
    set(WAD_FILE "${WAD_SOURCE_DIR}/doom1.wad")
endif()

if(WAD_FILE)
    message(STATUS "DOOM WAD Detected: ${WAD_FILE}. Embedding into FATFS partition 'storage'")
    set(WAD_STAGING_DIR "${CMAKE_BINARY_DIR}/wad_staging")
    file(REMOVE_RECURSE "${WAD_STAGING_DIR}")
    file(MAKE_DIRECTORY "${WAD_STAGING_DIR}")
    file(COPY "${WAD_FILE}" DESTINATION "${WAD_STAGING_DIR}")

    fatfs_create_partition_image(storage "${WAD_STAGING_DIR}" FLASH_IN_PROJECT)

    target_compile_definitions(${COMPONENT_LIB} PUBLIC DOOM_APP_EMBED_WAD=1)
endif()
