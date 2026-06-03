#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "doomgeneric.h"
#include "doom_port.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp/display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DOOM_PORT_FRAME_W DOOMGENERIC_RESX
#define DOOM_PORT_FRAME_H DOOMGENERIC_RESY
#define DOOM_PORT_CHUNK_LINES 20
#define DOOM_PORT_CENTER_X (((BSP_LCD_H_RES - DOOM_PORT_FRAME_W) / 2) & ~1)
#define DOOM_PORT_CENTER_Y (((BSP_LCD_V_RES - DOOM_PORT_FRAME_H) / 2) & ~1)

static const char *TAG = "doom_port";
static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_draw_buffers[2];
static uint8_t s_next_draw_buffer;

static uint16_t rgb565_swap(uint16_t color)
{
    return (uint16_t)((color << 8) | (color >> 8));
}

static esp_err_t ensure_draw_buffers(void)
{
    if (s_draw_buffers[0] != NULL && s_draw_buffers[1] != NULL) {
        return ESP_OK;
    }

    const size_t buffer_pixels = BSP_LCD_H_RES * DOOM_PORT_CHUNK_LINES;
    const size_t buffer_bytes = buffer_pixels * sizeof(uint16_t);

    for (int i = 0; i < 2; i++) {
        if (s_draw_buffers[i] == NULL) {
            s_draw_buffers[i] = heap_caps_malloc(buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
            if (s_draw_buffers[i] == NULL) {
                ESP_LOGE(TAG, "Failed to allocate %u-byte DMA draw buffer %d", (unsigned)buffer_bytes, i);
                return ESP_ERR_NO_MEM;
            }
        }
    }

    return ESP_OK;
}

static uint16_t *next_draw_buffer(void)
{
    uint16_t *buffer = s_draw_buffers[s_next_draw_buffer];
    s_next_draw_buffer ^= 1;
    return buffer;
}

void doom_port_set_panel(esp_lcd_panel_handle_t panel)
{
    s_panel = panel;
}

esp_err_t doom_port_draw_diagnostic_frame(void)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ensure_draw_buffers();
    if (err != ESP_OK) {
        return err;
    }

    static const uint16_t colors[] = {
        0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F, 0x07FF,
    };
    const int color_count = sizeof(colors) / sizeof(colors[0]);

    for (int y = 0; y < BSP_LCD_V_RES; y += DOOM_PORT_CHUNK_LINES) {
        int lines = BSP_LCD_V_RES - y;
        if (lines > DOOM_PORT_CHUNK_LINES) {
            lines = DOOM_PORT_CHUNK_LINES;
        }

        uint16_t *draw_buffer = next_draw_buffer();

        for (int row = 0; row < lines; row++) {
            uint16_t *dst = draw_buffer + row * BSP_LCD_H_RES;
            for (int x = 0; x < BSP_LCD_H_RES; x++) {
                int bar = (x * color_count) / BSP_LCD_H_RES;
                uint16_t color = colors[bar];

                if (((x / 20) + ((y + row) / 20)) & 1) {
                    color = (uint16_t)(color & 0x7BEF);
                }

                dst[x] = rgb565_swap(color);
            }
        }

        err = esp_lcd_panel_draw_bitmap(s_panel, 0, y, BSP_LCD_H_RES, y + lines, draw_buffer);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t draw_doom_frame(void)
{
    if (s_panel == NULL || DG_ScreenBuffer == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ensure_draw_buffers();
    if (err != ESP_OK) {
        return err;
    }

    const uint16_t *src_frame = (const uint16_t *)DG_ScreenBuffer;

    for (int y = 0; y < DOOM_PORT_FRAME_H; y += DOOM_PORT_CHUNK_LINES) {
        int lines = DOOM_PORT_FRAME_H - y;
        if (lines > DOOM_PORT_CHUNK_LINES) {
            lines = DOOM_PORT_CHUNK_LINES;
        }

        uint16_t *draw_buffer = next_draw_buffer();

        for (int row = 0; row < lines; row++) {
            const uint16_t *src = src_frame + (y + row) * DOOM_PORT_FRAME_W;
            uint16_t *dst = draw_buffer + row * DOOM_PORT_FRAME_W;

            for (int x = 0; x < DOOM_PORT_FRAME_W; x++) {
                dst[x] = rgb565_swap(src[x]);
            }
        }

        err = esp_lcd_panel_draw_bitmap(s_panel,
                                        DOOM_PORT_CENTER_X,
                                        DOOM_PORT_CENTER_Y + y,
                                        DOOM_PORT_CENTER_X + DOOM_PORT_FRAME_W,
                                        DOOM_PORT_CENTER_Y + y + lines,
                                        draw_buffer);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

void DG_Init(void)
{
    ESP_LOGI(TAG, "DG_Init: display %s", s_panel ? "ready" : "not initialized");
}

void DG_DrawFrame(void)
{
    static uint32_t frame_count;
    static int64_t last_log_us;

    esp_err_t err = draw_doom_frame();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Draw frame skipped: %s", esp_err_to_name(err));
        DG_SleepMs(1000);
        return;
    }

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
