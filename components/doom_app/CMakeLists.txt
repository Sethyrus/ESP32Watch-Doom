idf_component_register(
    SRCS
        "doom_app.c"
        "doom_port.c"
        "i_sound_esp32.c"
    INCLUDE_DIRS "include"
    REQUIRES
        doomgeneric
        watch_board
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
# CONFIG_DOOM_EMBED_WAD (menuconfig > Doom) decides; without an embedded WAD the
# firmware loads it from the microSD.
set(WAD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/wad")
if(EXISTS "${WAD_SOURCE_DIR}/doom.wad")
    set(WAD_FILE "${WAD_SOURCE_DIR}/doom.wad")
elseif(EXISTS "${WAD_SOURCE_DIR}/doom1.wad")
    set(WAD_FILE "${WAD_SOURCE_DIR}/doom1.wad")
endif()

if(WAD_FILE AND NOT CONFIG_DOOM_EMBED_WAD)
    message(STATUS "DOOM WAD ${WAD_FILE} not embedded (CONFIG_DOOM_EMBED_WAD=n): loading it from the microSD")
endif()

if(WAD_FILE AND CONFIG_DOOM_EMBED_WAD)
    message(STATUS "DOOM WAD Detected: ${WAD_FILE}. Embedding into FATFS partition 'storage'")
    set(WAD_STAGING_DIR "${CMAKE_BINARY_DIR}/wad_staging")
    file(REMOVE_RECURSE "${WAD_STAGING_DIR}")
    file(MAKE_DIRECTORY "${WAD_STAGING_DIR}")
    file(COPY "${WAD_FILE}" DESTINATION "${WAD_STAGING_DIR}")

    fatfs_create_partition_image(storage "${WAD_STAGING_DIR}" FLASH_IN_PROJECT)

    target_compile_definitions(${COMPONENT_LIB} PUBLIC DOOM_APP_EMBED_WAD=1)
else()
    # A stale image from an earlier build must not be flashed (ESP32Watch-Launcher
    # flashes build/storage.bin whenever it exists).
    file(REMOVE "${CMAKE_BINARY_DIR}/storage.bin")
endif()
