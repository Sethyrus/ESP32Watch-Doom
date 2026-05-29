# AGENTS.md

## Project Shape
- This is an ESP-IDF C firmware project named `ESP32S3Watch`; the app entrypoint is `app_main()` in `main/main.c`.
- Target hardware is Waveshare `ESP32-S3-Touch-AMOLED-2.06` with ESP32-S3R8, AMOLED 410x502 QSPI, FT3168 touch, QMI8658 IMU, PCF85063 RTC, AXP2101 PMU, ES8311/ES7210 audio, and microSD.
- Baseline stack is `ESP-IDF 5.5.4 + LVGL 9 + waveshare/esp32_s3_touch_amoled_2_06` BSP. Do not migrate to ESP-IDF 6.x or ESP-Brookesia unless explicitly requested.
- Keep `main` small. Add new `main` sources in `main/CMakeLists.txt`, or create ESP-IDF components for reusable code.
- Durable project config lives in `sdkconfig.defaults`, `partitions.csv`, and component manifests. `sdkconfig`, `build/`, and `managed_components/` are generated/local.

## Commands
- Source ESP-IDF before running IDF commands in a plain shell: `source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"`.
- First setup or fresh config: `idf.py set-target esp32s3`.
- Build/primary verification: `idf.py build`.
- Flash and monitor with the checked-in VS Code UART setting when applicable: `idf.py -p /dev/tty.usbmodem21301 flash monitor`.
- No repo-local test, lint, or format targets are configured; do not invent npm/PlatformIO/pytest commands for this repo.

## Generated And Local Files
- `build/` and `sdkconfig` are git-ignored. Treat them as local/generated state, not durable project source.
- If a Kconfig change must survive a clean checkout, add/update `sdkconfig.defaults` or intentionally change the tracking policy; editing only `sdkconfig` is not enough.
- `build/compile_commands.json` is the clangd source configured by `.vscode/settings.json`; regenerate it with `idf.py build` after source or config changes.

## Hardware And BSP Notes
- Prefer the Waveshare BSP for display/touch/brightness/audio/SD/I2C. It already handles the QSPI display init, LVGL port and touch input device.
- The wiki and Arduino examples mention display controller `CO5300`, but the official ESP-IDF BSP uses `waveshare/esp_lcd_sh8601`. Treat the BSP as source of truth for ESP-IDF work.
- Display brightness is controlled by command `0x51` over QSPI (`0x00..0xFF`), exposed as `bsp_display_brightness_set(percent)`.
- LVGL is not thread-safe. Wrap all `lv_*` calls made outside LVGL callbacks/tasks with `bsp_display_lock()` and `bsp_display_unlock()`.
- The BSP declares no IMU or button support. Use `waveshare/qmi8658` for IMU and custom code for BOOT GPIO0, RTC PCF85063, and PMU AXP2101.
- QMI8658 default accel units are milli-g. If using screen physics, normalize and map axes as `screen_x = -accelY / 1000.0f`, `screen_y = accelX / 1000.0f` unless m/s2 mode is enabled.

## Documentation Map
- `README.md`: project overview and quick start.
- `docs/HARDWARE.md`: board parts, pins, buses, sensors, APIs and addresses.
- `docs/SETUP.md`: ESP-IDF 5.5.4 setup, build/flash, config files and dependencies.
- `docs/GOTCHAS.md`: known pitfalls and implementation cautions.
- `docs/ARCHITECTURE.md`: LVGL+BSP decision, component layout and Brookesia criteria.
