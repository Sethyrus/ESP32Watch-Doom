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
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DOOM_PORT_FRAME_W DOOMGENERIC_RESX
#define DOOM_PORT_FRAME_H DOOMGENERIC_RESY
#define DOOM_PORT_CHUNK_LINES 20
#define DOOM_PORT_LANDSCAPE 1
#define DOOM_PORT_LANDSCAPE_CLOCKWISE 1
#define DOOM_PORT_LOGICAL_W BSP_LCD_V_RES
#define DOOM_PORT_LOGICAL_H BSP_LCD_H_RES
#define DOOM_PORT_GAME_W BSP_LCD_V_RES
#define DOOM_PORT_GAME_H 376
#define DOOM_PORT_GAME_X ((DOOM_PORT_LOGICAL_W - DOOM_PORT_GAME_W) / 2)
#define DOOM_PORT_GAME_Y ((DOOM_PORT_LOGICAL_H - DOOM_PORT_GAME_H) / 2)
#define DOOM_PORT_BOOT_GPIO GPIO_NUM_0
#define DOOM_PORT_BOOT_DEBOUNCE_US 30000
#define DOOM_PORT_EVENT_QUEUE_LEN 32
#define DOOM_PORT_TOUCH_CENTER_W 160
#define DOOM_PORT_TOUCH_CENTER_H 130
#define DOOM_PORT_TOUCH_CENTER_X_MIN ((DOOM_PORT_LOGICAL_W - DOOM_PORT_TOUCH_CENTER_W) / 2)
#define DOOM_PORT_TOUCH_CENTER_X_MAX (DOOM_PORT_TOUCH_CENTER_X_MIN + DOOM_PORT_TOUCH_CENTER_W)
#define DOOM_PORT_TOUCH_CENTER_Y_MIN ((DOOM_PORT_LOGICAL_H - DOOM_PORT_TOUCH_CENTER_H) / 2)
#define DOOM_PORT_TOUCH_CENTER_Y_MAX (DOOM_PORT_TOUCH_CENTER_Y_MIN + DOOM_PORT_TOUCH_CENTER_H)
#define DOOM_PORT_TOUCH_MENU_CORNER_PX 90

#define AXP2101_ADDR 0x34
#define AXP2101_INTEN2 0x41
#define AXP2101_INTSTS2 0x49
#define AXP2101_PKEY_SHORT_IRQ_BIT (1 << 3)

static const char *TAG = "doom_port";
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static i2c_master_dev_handle_t s_axp2101_dev = NULL;
static uint16_t *s_draw_buffers[2];
static uint8_t s_next_draw_buffer;
static uint16_t s_palette_swapped[256];
static bool s_palette_ready;

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
static bool s_landscape_map_ready;
static int16_t s_landscape_src_x_by_phys_y[BSP_LCD_V_RES];
static int16_t s_landscape_src_y_by_phys_x[BSP_LCD_H_RES];

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

static void physical_to_logical(int phys_x, int phys_y, int *logical_x, int *logical_y)
{
#if DOOM_PORT_LANDSCAPE_CLOCKWISE
    *logical_x = phys_y;
    *logical_y = BSP_LCD_H_RES - 1 - phys_x;
#else
    *logical_x = BSP_LCD_V_RES - 1 - phys_y;
    *logical_y = phys_x;
#endif
}

static void ensure_landscape_map(void)
{
    if (s_landscape_map_ready) {
        return;
    }

    for (int phys_y = 0; phys_y < BSP_LCD_V_RES; phys_y++) {
        int logical_x;
        int logical_y;
        physical_to_logical(0, phys_y, &logical_x, &logical_y);

        if (logical_x < DOOM_PORT_GAME_X || logical_x >= DOOM_PORT_GAME_X + DOOM_PORT_GAME_W) {
            s_landscape_src_x_by_phys_y[phys_y] = -1;
        } else {
            s_landscape_src_x_by_phys_y[phys_y] = (int16_t)(((logical_x - DOOM_PORT_GAME_X) * DOOM_PORT_FRAME_W) / DOOM_PORT_GAME_W);
        }
    }

    for (int phys_x = 0; phys_x < BSP_LCD_H_RES; phys_x++) {
        int logical_x;
        int logical_y;
        physical_to_logical(phys_x, 0, &logical_x, &logical_y);

        if (logical_y < DOOM_PORT_GAME_Y || logical_y >= DOOM_PORT_GAME_Y + DOOM_PORT_GAME_H) {
            s_landscape_src_y_by_phys_x[phys_x] = -1;
        } else {
            s_landscape_src_y_by_phys_x[phys_x] = (int16_t)(((logical_y - DOOM_PORT_GAME_Y) * DOOM_PORT_FRAME_H) / DOOM_PORT_GAME_H);
        }
    }

    s_landscape_map_ready = true;
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

    // Enable AXP2101 short press interrupt (INTEN2 bit 5)
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    if (i2c_bus) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = AXP2101_ADDR,
            .scl_speed_hz = 400000,
        };
        esp_err_t err_add = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &s_axp2101_dev);
        if (err_add == ESP_OK && s_axp2101_dev != NULL) {
            uint8_t enable_data[2] = {AXP2101_INTEN2, 0x00};
            // Read current INTEN2
            i2c_master_transmit_receive(s_axp2101_dev, &enable_data[0], 1, &enable_data[1], 1, -1);
            // Set bit 5
            enable_data[1] |= AXP2101_PKEY_SHORT_IRQ_BIT;
            i2c_master_transmit(s_axp2101_dev, enable_data, 2, -1);
        }
    }

    ESP_LOGI(TAG, "Input map: landscape touch center=use/enter, edges=move/turn; BOOT=fire/enter; PWR=menu");
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
    int logical_x;
    int logical_y;
    physical_to_logical(x, y, &logical_x, &logical_y);

    if (logical_x >= DOOM_PORT_TOUCH_CENTER_X_MIN && logical_x <= DOOM_PORT_TOUCH_CENTER_X_MAX &&
        logical_y >= DOOM_PORT_TOUCH_CENTER_Y_MIN && logical_y <= DOOM_PORT_TOUCH_CENTER_Y_MAX) {
        desired[DOOM_INPUT_USE] = true;
        desired[DOOM_INPUT_ENTER] = true;
        return;
    }

    if (logical_y < DOOM_PORT_LOGICAL_H / 3) {
        desired[DOOM_INPUT_UP] = true;
    } else if (logical_y > (DOOM_PORT_LOGICAL_H * 2) / 3) {
        desired[DOOM_INPUT_DOWN] = true;
    } else if (logical_x < DOOM_PORT_LOGICAL_W / 2) {
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

    // Check AXP2101 for PWR button short press
    if (s_axp2101_dev) {
        uint8_t reg = AXP2101_INTSTS2;
        uint8_t status = 0;
        esp_err_t err = i2c_master_transmit_receive(s_axp2101_dev, &reg, 1, &status, 1, pdMS_TO_TICKS(5));
        if (err == ESP_OK && (status & AXP2101_PKEY_SHORT_IRQ_BIT)) {
            desired[DOOM_INPUT_ESCAPE] = true;
            // Clear interrupt
            uint8_t clear_data[2] = {AXP2101_INTSTS2, AXP2101_PKEY_SHORT_IRQ_BIT};
            i2c_master_transmit(s_axp2101_dev, clear_data, 2, pdMS_TO_TICKS(5));
        }
    }

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
    ensure_landscape_map();

    if (palette_changed || !s_palette_ready) {
        for (int i = 0; i < 256; i++) {
            s_palette_swapped[i] = rgb565_swap(rgb565_from_color(colors[i]));
        }
        palette_changed = false;
        s_palette_ready = true;
    }

    for (int y = 0; y < BSP_LCD_V_RES; y += DOOM_PORT_CHUNK_LINES) {
        int lines = BSP_LCD_V_RES - y;
        if (lines > DOOM_PORT_CHUNK_LINES) {
            lines = DOOM_PORT_CHUNK_LINES;
        }

        uint16_t *draw_buffer = next_draw_buffer();

        for (int row = 0; row < lines; row++) {
            int16_t src_x = s_landscape_src_x_by_phys_y[y + row];
            uint16_t *dst = draw_buffer + row * BSP_LCD_H_RES;

            for (int x = 0; x < BSP_LCD_H_RES; x++) {
                int16_t src_y = s_landscape_src_y_by_phys_x[x];
                if (src_x < 0 || src_y < 0) {
                    dst[x] = 0;
                    continue;
                }

                dst[x] = s_palette_swapped[src_frame[src_y * DOOM_PORT_FRAME_W + src_x]];
            }
        }

        err = esp_lcd_panel_draw_bitmap(s_panel,
                                        0,
                                        y,
                                        BSP_LCD_H_RES,
                                        y + lines,
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
