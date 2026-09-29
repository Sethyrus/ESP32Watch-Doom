# AGENTS.md

## Project Shape
- ESP-IDF C firmware `ESP32WatchDoom`: standalone Doom port on DoomGeneric. Entry point `app_main()` in `main/main.c`; ESP32 HAL in `components/doom_app/`; engine vendored in `components/doomgeneric/vendor` (GPL-2.0).
- Target hardware is Waveshare `ESP32-S3-Touch-AMOLED-2.06` (ESP32-S3R8, AMOLED 410x502 QSPI via SH8601 BSP driver, FT3168 touch, AXP2101 PMU, ES8311 speaker, microSD).
- Stack: `ESP-IDF 5.5.4` + `waveshare/esp32_s3_touch_amoled_2_06` BSP + `watch_board` from ESP32Watch-core. No LVGL at runtime. Do not migrate to ESP-IDF 6.x unless explicitly requested.
- BOOT and PWR come from `watch_buttons.h` in `watch_board` (https://github.com/Sethyrus/ESP32Watch-core), pinned by tag in `main/idf_component.yml`. Fix hardware-level bugs there.
- Port decisions, status and references: `docs/DOOM_PORT.md`. Keep vendor patches minimal and documented there; `components/doomgeneric/README.md` lists what was pruned from upstream.
- Never commit WAD files or game assets. `wad/` is ignored except `wad/README.md`.
- Durable config lives in `sdkconfig.defaults`, `partitions.csv`, component manifests and `dependencies.lock`. `sdkconfig`, `build/` and `managed_components/` are generated.

## Commands
- Source ESP-IDF: `source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"`.
- First setup: `idf.py set-target esp32s3`. Verification: `idf.py build` (works with or without a WAD in `wad/`).
- Flash and monitor: `idf.py -p <PORT> flash monitor` (macOS port looks like `/dev/tty.usbmodem*` and changes with the USB socket; `idf.py` auto-detects it if `-p` is omitted).
- No test, lint or format targets are configured; do not invent them.

## Critical Hardware Notes
- `partitions.csv`: `factory` 3 MB + `storage` FATFS 12 MB for the embedded WAD. BSP SPIFFS label is moved to `spiffs` so `bsp_spiffs_mount()` never touches `storage`.
- Reuse `bsp_i2c_get_handle()` for devices on the shared I2C bus; never create a second master bus on the same port.
- BOOT is GPIO0, active low. PWR is AXP2101 `PWRON` (short press via INTSTS2 IRQ); holding it ~6 s powers off the board.
- Button convention: BOOT = accept/primary action, PWR short press = back/menu. See "Convencion De Botones" in core `docs/ARCHITECTURE.md`.
- microSD uses BSP SDMMC 1-bit (`CLK GPIO2`, `CMD GPIO1`, `D0 GPIO3`).
