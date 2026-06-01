#include <inttypes.h>
#include <stdint.h>

#include "doomgeneric.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "doom_port";

void DG_Init(void)
{
    ESP_LOGI(TAG, "DG_Init stub: display init pending");
}

void DG_DrawFrame(void)
{
    static uint32_t frame_count;
    static int64_t last_log_us;

    frame_count++;

    int64_t now_us = esp_timer_get_time();
    if (now_us - last_log_us >= 5000000) {
        uint32_t fps = 0;

        if (last_log_us != 0) {
            fps = (uint32_t)((frame_count * 1000000ULL) / (now_us - last_log_us));
        }

        ESP_LOGI(TAG,
                 "DG_DrawFrame stub: frames=%" PRIu32 " fps=%" PRIu32 " internal=%u psram=%u",
                 frame_count,
                 fps,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

        frame_count = 0;
        last_log_us = now_us;
    }
}

void DG_SleepMs(uint32_t ms)
{
    if (ms == 0) {
        taskYIELD();
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(ms));
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    (void)pressed;
    (void)key;
    return 0;
}

void DG_SetWindowTitle(const char *title)
{
    ESP_LOGI(TAG, "Doom title: %s", title ? title : "(null)");
}
