#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_video.h"
#include "doom_port.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
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
#define DOOM_PORT_BOOT_GPIO GPIO_NUM_0
#define DOOM_PORT_BOOT_DEBOUNCE_US 30000
#define DOOM_PORT_EVENT_QUEUE_LEN 32
#define DOOM_PORT_TOUCH_CENTER_X_MIN 140
#define DOOM_PORT_TOUCH_CENTER_X_MAX 270
#define DOOM_PORT_TOUCH_CENTER_Y_MIN 200
#define DOOM_PORT_TOUCH_CENTER_Y_MAX 330
#define DOOM_PORT_TOUCH_MENU_CORNER_PX 90

static const char *TAG = "doom_port";
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static uint16_t *s_draw_buffers[2];
static uint8_t s_next_draw_buffer;

typedef enum {
    DOOM_INPUT_UP,
    DOOM_INPUT_DOWN,
    DOOM_INPUT_LEFT,
    DOOM_INPUT_RIGHT,
    DOOM_INPUT_FIRE,
    DOOM_INPUT_USE,
    DOOM_INPUT_ENTER,
    DOOM_INPUT_ESCAPE,
    DOOM_INPUT_COUNT,
} doom_input_id_t;

typedef struct {
    int pressed;
    unsigned char key;
} doom_input_event_t;

static const unsigned char s_input_keys[DOOM_INPUT_COUNT] = {
    [DOOM_INPUT_UP] = KEY_UPARROW,
    [DOOM_INPUT_DOWN] = KEY_DOWNARROW,
    [DOOM_INPUT_LEFT] = KEY_LEFTARROW,
    [DOOM_INPUT_RIGHT] = KEY_RIGHTARROW,
    [DOOM_INPUT_FIRE] = KEY_FIRE,
    [DOOM_INPUT_USE] = KEY_USE,
    [DOOM_INPUT_ENTER] = KEY_ENTER,
    [DOOM_INPUT_ESCAPE] = KEY_ESCAPE,
};

static bool s_input_down[DOOM_INPUT_COUNT];
static doom_input_event_t s_input_events[DOOM_PORT_EVENT_QUEUE_LEN];
static uint8_t s_input_event_head;
static uint8_t s_input_event_tail;
static bool s_boot_raw;
static bool s_boot_stable;
static int64_t s_boot_last_change_us;
static bool s_input_ready;

static uint16_t rgb565_swap(uint16_t color)
{
    return (uint16_t)((color << 8) | (color >> 8));
}

static uint16_t rgb565_from_color(struct color color)
{
    return (uint16_t)(((uint16_t)(color.r & 0xF8) << 8) |
                      ((uint16_t)(color.g & 0xFC) << 3) |
                      ((uint16_t)color.b >> 3));
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

void doom_port_set_touch(esp_lcd_touch_handle_t touch)
{
    s_touch = touch;
}

esp_err_t doom_port_init_input(void)
{
    const gpio_config_t boot_config = {
        .pin_bit_mask = 1ULL << DOOM_PORT_BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&boot_config);
    if (err != ESP_OK) {
        return err;
    }

    s_boot_raw = gpio_get_level(DOOM_PORT_BOOT_GPIO) == 0;
    s_boot_stable = s_boot_raw;
    s_boot_last_change_us = esp_timer_get_time();
    s_input_ready = true;

    ESP_LOGI(TAG, "Input map: BOOT=fire/enter, touch top-left=menu, center=use/enter, edges=move/turn");
    return ESP_OK;
}

static bool queue_input_event(int pressed, unsigned char key)
{
    uint8_t next_head = (uint8_t)((s_input_event_head + 1) % DOOM_PORT_EVENT_QUEUE_LEN);
    if (next_head == s_input_event_tail) {
        ESP_LOGW(TAG, "Input event queue full; dropping key %u", (unsigned)key);
        return false;
    }

    s_input_events[s_input_event_head].pressed = pressed;
    s_input_events[s_input_event_head].key = key;
    s_input_event_head = next_head;
    return true;
}

static bool dequeue_input_event(doom_input_event_t *event)
{
    if (s_input_event_tail == s_input_event_head) {
        return false;
    }

    *event = s_input_events[s_input_event_tail];
    s_input_event_tail = (uint8_t)((s_input_event_tail + 1) % DOOM_PORT_EVENT_QUEUE_LEN);
    return true;
}

static void update_input_key(doom_input_id_t input, bool desired_down)
{
    if (s_input_down[input] == desired_down) {
        return;
    }

    if (queue_input_event(desired_down ? 1 : 0, s_input_keys[input])) {
        s_input_down[input] = desired_down;
    }
}

static bool poll_boot_button(void)
{
    if (!s_input_ready) {
        return false;
    }

    bool raw_pressed = gpio_get_level(DOOM_PORT_BOOT_GPIO) == 0;
    int64_t now_us = esp_timer_get_time();

    if (raw_pressed != s_boot_raw) {
        s_boot_raw = raw_pressed;
        s_boot_last_change_us = now_us;
    }

    if ((now_us - s_boot_last_change_us) >= DOOM_PORT_BOOT_DEBOUNCE_US) {
        s_boot_stable = s_boot_raw;
    }

    return s_boot_stable;
}

static bool poll_touch_point(uint16_t *x, uint16_t *y)
{
    if (s_touch == NULL) {
        return false;
    }

    esp_err_t err = esp_lcd_touch_read_data(s_touch);
    if (err != ESP_OK) {
        static int64_t last_warn_us;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_warn_us > 2000000) {
            ESP_LOGW(TAG, "Touch read failed: %s", esp_err_to_name(err));
            last_warn_us = now_us;
        }
        return false;
    }

    esp_lcd_touch_point_data_t point;
    uint8_t point_count = 0;
    err = esp_lcd_touch_get_data(s_touch, &point, &point_count, 1);
    if (err != ESP_OK || point_count == 0) {
        return false;
    }

    *x = point.x;
    *y = point.y;
    return true;
}

static void apply_touch_mapping(bool desired[DOOM_INPUT_COUNT], uint16_t x, uint16_t y)
{
    if (x < DOOM_PORT_TOUCH_MENU_CORNER_PX && y < DOOM_PORT_TOUCH_MENU_CORNER_PX) {
        desired[DOOM_INPUT_ESCAPE] = true;
        return;
    }

    if (x >= DOOM_PORT_TOUCH_CENTER_X_MIN && x <= DOOM_PORT_TOUCH_CENTER_X_MAX &&
        y >= DOOM_PORT_TOUCH_CENTER_Y_MIN && y <= DOOM_PORT_TOUCH_CENTER_Y_MAX) {
        desired[DOOM_INPUT_USE] = true;
        desired[DOOM_INPUT_ENTER] = true;
        return;
    }

    if (y < BSP_LCD_V_RES / 3) {
        desired[DOOM_INPUT_UP] = true;
    } else if (y > (BSP_LCD_V_RES * 2) / 3) {
        desired[DOOM_INPUT_DOWN] = true;
    } else if (x < BSP_LCD_H_RES / 2) {
        desired[DOOM_INPUT_LEFT] = true;
    } else {
        desired[DOOM_INPUT_RIGHT] = true;
    }
}

static void poll_input(void)
{
    bool desired[DOOM_INPUT_COUNT] = {0};

    bool boot_pressed = poll_boot_button();
    desired[DOOM_INPUT_FIRE] = boot_pressed;
    desired[DOOM_INPUT_ENTER] = boot_pressed;

    uint16_t x = 0;
    uint16_t y = 0;
    if (poll_touch_point(&x, &y)) {
        apply_touch_mapping(desired, x, y);
    }

    for (doom_input_id_t input = 0; input < DOOM_INPUT_COUNT; input++) {
        update_input_key(input, desired[input]);
    }
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

static esp_err_t fill_screen_rgb565(uint16_t color)
{
    if (s_panel == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = ensure_draw_buffers();
    if (err != ESP_OK) {
        return err;
    }

    const uint16_t panel_color = rgb565_swap(color);

    for (int y = 0; y < BSP_LCD_V_RES; y += DOOM_PORT_CHUNK_LINES) {
        int lines = BSP_LCD_V_RES - y;
        if (lines > DOOM_PORT_CHUNK_LINES) {
            lines = DOOM_PORT_CHUNK_LINES;
        }

        uint16_t *draw_buffer = next_draw_buffer();
        for (int i = 0; i < BSP_LCD_H_RES * lines; i++) {
            draw_buffer[i] = panel_color;
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

    const uint8_t *src_frame = (const uint8_t *)DG_ScreenBuffer;

    for (int y = 0; y < DOOM_PORT_FRAME_H; y += DOOM_PORT_CHUNK_LINES) {
        int lines = DOOM_PORT_FRAME_H - y;
        if (lines > DOOM_PORT_CHUNK_LINES) {
            lines = DOOM_PORT_CHUNK_LINES;
        }

        uint16_t *draw_buffer = next_draw_buffer();

        for (int row = 0; row < lines; row++) {
            const uint8_t *src = src_frame + (y + row) * DOOM_PORT_FRAME_W;
            uint16_t *dst = draw_buffer + row * DOOM_PORT_FRAME_W;

            for (int x = 0; x < DOOM_PORT_FRAME_W; x++) {
                dst[x] = rgb565_swap(rgb565_from_color(colors[src[x]]));
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

    if (s_panel != NULL) {
        esp_err_t err = fill_screen_rgb565(0x0000);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to clear diagnostic frame: %s", esp_err_to_name(err));
        }
    }
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
                 "DG_DrawFrame: frames=%" PRIu32 " fps=%" PRIu32 " internal=%u psram=%u",
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
    poll_input();

    doom_input_event_t event;
    if (!dequeue_input_event(&event)) {
        return 0;
    }

    *pressed = event.pressed;
    *key = event.key;
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    ESP_LOGI(TAG, "Doom title: %s", title ? title : "(null)");
}
