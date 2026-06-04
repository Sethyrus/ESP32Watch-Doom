#include "doom_app.h"
#include "doom_port.h"

#include <sys/stat.h>
#include <unistd.h>

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "bsp/touch.h"
#include "doomgeneric.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_vfs_fat.h"

#include "esp_codec_dev.h"

#define DOOM_APP_TASK_STACK_BYTES (24 * 1024)
#define DOOM_APP_TASK_PRIORITY 5
#define DOOM_APP_TASK_CORE 1
#define DOOM_APP_DISPLAY_CHUNK_LINES 20

static const char *TAG = "doom_app";
static bool s_display_ready;
static bool s_sd_mounted;
static bool s_internal_mounted;
static esp_lcd_touch_handle_t s_touch;
static char s_wad_path[128] = "";

extern void doom_app_set_audio_codec(esp_codec_dev_handle_t codec);

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

static void doom_app_mount_internal(void)
{
#ifdef DOOM_APP_EMBED_WAD
    ESP_LOGI(TAG, "Mounting internal FATFS partition at /internal");
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 2,
        .format_if_mount_failed = false,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_ro("/internal", "storage", &mount_config);
    if (err == ESP_OK) {
        s_internal_mounted = true;
        ESP_LOGI(TAG, "Internal FATFS mounted successfully");
    } else {
        ESP_LOGW(TAG, "Failed to mount internal FATFS: %s", esp_err_to_name(err));
    }
#else
    ESP_LOGI(TAG, "No internal WAD embedded during build");
#endif
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

    // Initialize sound hardware via BSP
#ifdef FEATURE_SOUND
    esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
    if (spk == NULL) {
        ESP_LOGW(TAG, "Failed to initialize speaker hardware");
    } else {
        ESP_LOGI(TAG, "Speaker hardware initialized");

        esp_codec_dev_sample_info_t fs = {
            .sample_rate = 22050,
            .channel = 2,
            .bits_per_sample = 16,
        };
        esp_err_t err = esp_codec_dev_open(spk, &fs);
        if (err == ESP_CODEC_DEV_OK) {
            ESP_LOGI(TAG, "Audio codec opened successfully");
            // Keep output below max while debugging the Doom mixer.
            esp_codec_dev_set_out_vol(spk, 60);
            doom_app_set_audio_codec(spk);
        } else {
            ESP_LOGE(TAG, "Failed to open audio codec");
        }
    }
#endif

    doom_app_mount_internal();
    doom_app_mount_sd();

    if (s_sd_mounted) {
        if (chdir(BSP_SD_MOUNT_POINT) != 0) {
            ESP_LOGW(TAG, "Failed to chdir to %s", BSP_SD_MOUNT_POINT);
        } else {
            ESP_LOGI(TAG, "Working directory set to %s", BSP_SD_MOUNT_POINT);
        }
        ESP_LOGI(TAG, "Doom config and savegames use %s/", BSP_SD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "SD not mounted; Doom config/savegame persistence is unavailable");
    }

    // Search for WAD files
    const char *wad_candidates[] = {
        "/internal/doom.wad",
        "/internal/doom1.wad",
        "/sdcard/doom.wad",
        "/sdcard/doom1.wad"
    };
    int num_candidates = sizeof(wad_candidates) / sizeof(wad_candidates[0]);
    struct stat wad_stat;
    bool wad_found = false;

    for (int i = 0; i < num_candidates; i++) {
        if (stat(wad_candidates[i], &wad_stat) == 0) {
            strncpy(s_wad_path, wad_candidates[i], sizeof(s_wad_path) - 1);
            wad_found = true;
            break;
        }
    }

    if (!wad_found) {
        ESP_LOGW(TAG, "No WAD found! Doom engine start deferred");
        if (!s_sd_mounted && !s_internal_mounted) {
            ESP_LOGW(TAG, "Neither SD nor internal FATFS is mounted");
        }
        if (s_display_ready) {
            ESP_LOGW(TAG, "Display remains on diagnostic color bars");
        }
        vTaskSuspend(NULL);
    }

    char *argv[] = {
        "doom",
        "-iwad", s_wad_path,
        "-mb", "4",
        "-nomusic",
        "-nogui",
    };
    const int argc = sizeof(argv) / sizeof(argv[0]);

    ESP_LOGI(TAG, "Starting DoomGeneric with %s", s_wad_path);
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
