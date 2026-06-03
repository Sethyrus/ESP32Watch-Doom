#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

void doom_port_set_panel(esp_lcd_panel_handle_t panel);
void doom_port_set_touch(esp_lcd_touch_handle_t touch);
esp_err_t doom_port_init_input(void);
esp_err_t doom_port_draw_diagnostic_frame(void);

#ifdef __cplusplus
}
#endif
