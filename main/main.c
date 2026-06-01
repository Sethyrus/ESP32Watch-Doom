
#include "doom_app.h"

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "ESP32S3Watch";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting standalone Doom firmware");

    esp_err_t err = doom_app_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Doom app: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Doom app task created");
}
