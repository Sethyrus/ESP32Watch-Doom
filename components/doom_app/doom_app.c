#include "doom_app.h"
#include "doom_port.h"

#include <sys/stat.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "bsp/touch.h"
#include "doomgeneric.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DOOM_APP_TASK_STACK_BYTES (24 * 1024)
#define DOOM_APP_TASK_PRIORITY 5
#define DOOM_APP_TASK_CORE 1
#define DOOM_APP_WAD_PATH "/sdcard/doom1.wad"
#define DOOM_APP_DISPLAY_CHUNK_LINES 20

static const char *TAG = "doom_app";
static bool s_display_ready;
static bool s_sd_mounted;
static esp_lcd_touch_handle_t s_touch;

static esp_err_t doom_app_init_display(void)
{
    ESP_LOGI(TAG, "Initializing standalone display path");

    esp_lcd_panel_handle_t panel = NULL;
    const bsp_display_config_t display_config = {
        .max_transfer_sz = BSP_LCD_H_RES * DOOM_APP_DISPLAY_CHUNK_LINES * BSP_LCD_BITS_PER_PIXEL / 8,
    };

    esp_err_t err = bsp_display_new(&display_config, &panel, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_new failed: %s", esp_err_to_name(err));
        return err;
    }

    doom_port_set_panel(panel);

    err = bsp_display_brightness_set(80);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_display_brightness_set failed: %s", esp_err_to_name(err));
        return err;
    }

    err = doom_port_draw_diagnostic_frame();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Diagnostic display draw failed: %s", esp_err_to_name(err));
        return err;
    }

    s_display_ready = true;
    ESP_LOGI(TAG, "Display initialized; diagnostic color bars shown");
    return ESP_OK;
}

static void doom_app_mount_sd(void)
{
    ESP_LOGI(TAG, "Mounting SD card at %s", BSP_SD_MOUNT_POINT);

    esp_err_t err = bsp_sdcard_mount();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(err));
        return;
    }

    s_sd_mounted = true;
    if (bsp_sdcard != NULL) {
        ESP_LOGI(TAG, "SD mounted: %s", bsp_sdcard->cid.name);
    } else {
        ESP_LOGI(TAG, "SD mounted");
    }
}

static void doom_app_init_input(void)
{
    esp_err_t err = doom_port_init_input();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BOOT input init failed: %s", esp_err_to_name(err));
    }

    err = bsp_touch_new(NULL, &s_touch);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed: %s", esp_err_to_name(err));
        ESP_LOGW(TAG, "Continuing with BOOT-only input");
        return;
    }

    doom_port_set_touch(s_touch);
    ESP_LOGI(TAG, "Input initialized: touch zones + BOOT GPIO0");
}

static void doom_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Doom standalone task started");

    if (doom_app_init_display() != ESP_OK) {
        ESP_LOGE(TAG, "Display initialization failed; suspending Doom task");
        vTaskSuspend(NULL);
    }

    doom_app_init_input();

    doom_app_mount_sd();

    struct stat wad_stat;
    if (stat(DOOM_APP_WAD_PATH, &wad_stat) != 0) {
        ESP_LOGW(TAG, "WAD not found at %s; Doom engine start deferred", DOOM_APP_WAD_PATH);
        if (!s_sd_mounted) {
            ESP_LOGW(TAG, "SD is not mounted; insert/format card or check hardware before retrying");
        }
        if (s_display_ready) {
            ESP_LOGW(TAG, "Display remains on diagnostic color bars while waiting for next firmware iteration");
        }
        vTaskSuspend(NULL);
    }

    char *argv[] = {
        "doom",
        "-iwad", DOOM_APP_WAD_PATH,
        "-mb", "4",
        "-nosound",
        "-nomusic",
        "-nogui",
    };
    const int argc = sizeof(argv) / sizeof(argv[0]);

    ESP_LOGI(TAG, "Starting DoomGeneric with %s", DOOM_APP_WAD_PATH);
    doomgeneric_Create(argc, argv);

    while (true) {
        doomgeneric_Tick();
    }
}

esp_err_t doom_app_start(void)
{
    BaseType_t created = xTaskCreatePinnedToCore(
        doom_task,
        "doom",
        DOOM_APP_TASK_STACK_BYTES,
        NULL,
        DOOM_APP_TASK_PRIORITY,
        NULL,
        DOOM_APP_TASK_CORE);

    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Doom task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
