#include "esp_err.h"
#include "esp_log.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "lvgl.h"

static const char *TAG = "ESP32S3Watch";

static void create_demo_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0b1020), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "ESP32S3Watch");
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_size(card, 340, 210);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x132238), LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x35d0ba), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 28, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x020617), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(card, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(card, LV_OPA_50, LV_PART_MAIN);
    lv_obj_center(card);

    lv_obj_t *status = lv_label_create(card);
    lv_label_set_text(status, "LVGL + Waveshare BSP");
    lv_obj_set_style_text_color(status, lv_color_hex(0x67e8f9), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(status, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 32);

    lv_obj_t *details = lv_label_create(card);
    lv_label_set_text(details, "ESP-IDF 5.5.4\n410 x 502 AMOLED\nTouch ready");
    lv_obj_set_style_text_color(details, lv_color_hex(0xcbd5e1), LV_PART_MAIN);
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(details, &lv_font_montserrat_16, LV_PART_MAIN);
#endif
    lv_obj_align(details, LV_ALIGN_CENTER, 0, 22);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, "Baseline hardware check");
    lv_obj_set_style_text_color(footer, lv_color_hex(0x94a3b8), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, LV_PART_MAIN);
#endif
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -42);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LVGL BSP demo");

    lv_display_t *display = bsp_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start display");
        return;
    }

    esp_err_t err = bsp_display_brightness_set(80);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set display brightness: %s", esp_err_to_name(err));
    }

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    create_demo_ui();

    bsp_display_unlock();

    ESP_LOGI(TAG, "LVGL BSP demo ready");
}
