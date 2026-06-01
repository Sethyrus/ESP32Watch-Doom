#include "doom_app.h"

#include <sys/stat.h>

#include "doomgeneric.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DOOM_APP_TASK_STACK_BYTES (24 * 1024)
#define DOOM_APP_TASK_PRIORITY 5
#define DOOM_APP_TASK_CORE 1
#define DOOM_APP_WAD_PATH "/sdcard/doom1.wad"

static const char *TAG = "doom_app";

static void doom_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Doom standalone task started");
    ESP_LOGW(TAG, "Display, touch, and SD mount are compile-first stubs");

    struct stat wad_stat;
    if (stat(DOOM_APP_WAD_PATH, &wad_stat) != 0) {
        ESP_LOGW(TAG, "WAD not found at %s; Doom engine start deferred", DOOM_APP_WAD_PATH);
        ESP_LOGW(TAG, "Next phase will mount SD before this check");
        vTaskSuspend(NULL);
    }

    char *argv[] = {
        "doom",
        "-iwad", DOOM_APP_WAD_PATH,
        "-mb", "4",
        "-nosound",
        "-nomusic",
        "-nogui",
        "-gfxmode", "rgb565",
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
