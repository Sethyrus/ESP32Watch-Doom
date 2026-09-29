
#include "doom_app.h"

#include "esp_err.h"
#include "esp_log.h"
#include "watch_launcher.h"

static const char *TAG = "ESP32WatchDoom";

void app_main(void)
{
    // Launcher mode: any reset from here on (Quit Game restarts) returns to the launcher.
    watch_launcher_boot_once();

    ESP_LOGI(TAG, "Starting Doom firmware");

    esp_err_t err = doom_app_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Doom app: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Doom app task created");
}
