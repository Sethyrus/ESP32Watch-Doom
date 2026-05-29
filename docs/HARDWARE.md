# Hardware Reference

Referencia tecnica de la placa Waveshare `ESP32-S3-Touch-AMOLED-2.06` para este proyecto.

Fuentes usadas: wiki oficial Waveshare, repo oficial `waveshareteam/ESP32-S3-Touch-AMOLED-2.06`, componentes del ESP Component Registry, BSP `waveshare/esp32_s3_touch_amoled_2_06` v1.0.6 y proyecto previo local `MyESP32S3Watch`.

## Resumen De Placa

| Bloque | Modelo / dato | Notas |
| --- | --- | --- |
| MCU | `ESP32-S3R8` | Dual-core LX7, hasta 240 MHz. |
| PSRAM | 8 MB octal | Necesaria para LVGL fluido y buffers de display. |
| Flash | 32 MB segun wiki | Los ejemplos ESP-IDF oficiales usan config de 16 MB. |
| Display | AMOLED 2.06", 410 x 502 | QSPI, 16-bit RGB565 en BSP. |
| Touch | `FT3168` | I2C; BSP usa driver compatible `FT5x06`. |
| IMU | `QMI8658` | Acelerometro + giroscopio 6 ejes, I2C. |
| RTC | `PCF85063` | I2C, alimentado por bateria via PMU. |
| PMU | `AXP2101` | Gestion de bateria/carga/voltajes, I2C. |
| Audio out | `ES8311` | Codec para speaker, I2C control + I2S data. |
| Audio in | `ES7210` | ADC/microfono, I2C control + I2S data. |
| Storage | microSD | SDMMC 1-bit segun BSP. |

## BSP Oficial

Componente recomendado: `waveshare/esp32_s3_touch_amoled_2_06`.

Version observada: `1.0.6`.

Capacidades declaradas por el BSP:

| Capacidad | Valor |
| --- | --- |
| Display | `BSP_CAPS_DISPLAY 1` |
| Touch | `BSP_CAPS_TOUCH 1` |
| Buttons | `BSP_CAPS_BUTTONS 0` |
| Audio | `BSP_CAPS_AUDIO 1` |
| Speaker | `BSP_CAPS_AUDIO_SPEAKER 1` |
| Mic | `BSP_CAPS_AUDIO_MIC 1` |
| SD card | `BSP_CAPS_SDCARD 1` |
| IMU | `BSP_CAPS_IMU 0` |

El BSP cubre display, touch, brillo, I2C, audio, SPIFFS y SD. No cubre IMU, RTC, PMU ni botones como APIs de alto nivel.

## Pines Principales

| Funcion | Pin ESP32-S3 | Fuente |
| --- | --- | --- |
| I2C SDA | GPIO15 | BSP |
| I2C SCL | GPIO14 | BSP |
| LCD CS | GPIO12 | BSP |
| LCD PCLK/SCLK | GPIO11 | BSP |
| LCD DATA0 | GPIO4 | BSP |
| LCD DATA1 | GPIO5 | BSP |
| LCD DATA2 | GPIO6 | BSP |
| LCD DATA3 | GPIO7 | BSP |
| LCD RST | GPIO8 | BSP |
| Touch RST | GPIO9 | BSP |
| Touch INT | GPIO38 | BSP |
| SD D0 | GPIO3 | BSP |
| SD CMD | GPIO1 | BSP |
| SD CLK | GPIO2 | BSP |
| I2S MCLK | GPIO16 | BSP |
| I2S SCLK/BCLK | GPIO41 | BSP |
| I2S LCLK/WS | GPIO45 | BSP |
| I2S DOUT | GPIO40 | BSP |
| I2S DSIN | GPIO42 | BSP |
| Speaker amp enable | GPIO46 | BSP |
| BOOT button | GPIO0 | ESP32-S3 convention / ejemplo oficial |

## I2C Devices

Todos comparten `SDA=GPIO15` y `SCL=GPIO14` en el bus del BSP.

| Dispositivo | Modelo | Direccion | Estado |
| --- | --- | --- | --- |
| Touch | `FT3168` / driver `FT5x06` | `0x38` | BSP lo inicializa con LVGL. |
| IMU | `QMI8658` | `0x6A` o `0x6B` | Usar `QMI8658_ADDRESS_HIGH` (`0x6B`) como en el proyecto previo. |
| RTC | `PCF85063` | tipica `0x51` | No expuesto por BSP. |
| PMU | `AXP2101` | tipica `0x34` | No expuesto por BSP; ejemplo oficial porta XPowersLib. |
| Speaker codec | `ES8311` | `0x30` en `esp_codec_dev` | BSP lo usa en `bsp_audio_codec_speaker_init()`. |
| Mic codec | `ES7210` | default del componente codec | BSP lo usa en `bsp_audio_codec_microphone_init()`. |

Antes de desarrollar drivers propios para RTC/PMU, conviene hacer un I2C scan en hardware real y anotar los resultados aqui.

## Display AMOLED

Datos validados:

- Resolucion BSP: `BSP_LCD_H_RES=410`, `BSP_LCD_V_RES=502`.
- Bus: QSPI por `SPI2_HOST`.
- Formato BSP: RGB565, `LV_COLOR_FORMAT_RGB565` en LVGL 9.
- Backlight real: no hay pin PWM; el brillo se controla con comando QSPI `0x51` y parametro `0x00..0xFF`.
- API BSP: `bsp_display_start()`, `bsp_display_start_with_config()`, `bsp_display_backlight_on()`, `bsp_display_backlight_off()`, `bsp_display_brightness_set(percent)`.

Gotcha de controlador:

- La wiki y ejemplos Arduino hablan de `CO5300`.
- El BSP oficial usa `waveshare/esp_lcd_sh8601` y comandos SH8601.
- Para ESP-IDF, usar el BSP oficial como fuente de verdad practica.

Gotcha de areas LVGL:

- El driver necesita redondear areas a coordenadas pares/impares para refresco correcto.
- El BSP ya instala `rounder_event_cb`; no duplicarlo en la app salvo que se use display sin BSP.

## Touch FT3168

El touch se inicializa automaticamente al llamar `bsp_display_start()` porque el BSP registra el input device de LVGL.

APIs utiles:

```c
lv_display_t *display = bsp_display_start();
lv_indev_t *touch = bsp_display_get_input_dev();
```

No hace falta leer el touch manualmente para widgets LVGL normales. LVGL recibira eventos de puntero por el input device registrado por el BSP.

## IMU QMI8658

Componente recomendado: `waveshare/qmi8658`.

Uso base con BSP:

```c
i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
qmi8658_dev_t dev;
ESP_ERROR_CHECK(qmi8658_init(&dev, bus, QMI8658_ADDRESS_HIGH));
ESP_ERROR_CHECK(qmi8658_set_accel_range(&dev, QMI8658_ACCEL_RANGE_8G));
ESP_ERROR_CHECK(qmi8658_set_accel_odr(&dev, QMI8658_ACCEL_ODR_500HZ));
```

Unidades:

- Por defecto, `qmi8658_read_sensor_data()` devuelve acelerometro en milli-g.
- `1000.0` equivale a 1 g.
- Si se llama `qmi8658_set_accel_unit_mps2(&dev, true)`, devuelve m/s2.

Mapeo de ejes probado para coordenadas de pantalla:

```c
qmi8658_data_t data;
qmi8658_read_sensor_data(&dev, &data);

float screen_x = -data.accelY / 1000.0f;
float screen_y =  data.accelX / 1000.0f;
```

Calibracion recomendada:

- Tomar 100 a 200 muestras al arrancar con el dispositivo quieto.
- Aplicar el mismo mapeo de ejes durante el promedio.
- Restar bias en lecturas futuras.
- Usar deadzone pequena despues de normalizar, por ejemplo `0.015f` a `0.05f` segun la app.

## RTC PCF85063

La wiki identifica `PCF85063` y enlaza datasheet `PCF85063A`.

Estado actual:

- No esta expuesto por el BSP oficial.
- Los ejemplos Arduino usan `SensorPCF85063`/SensorLib.
- Para ESP-IDF se puede crear un driver minimo por I2C si solo necesitamos fecha/hora.

Usos previstos:

- Reloj persistente sin Wi-Fi.
- Calendario, alarmas y sleep/wake si se anade gestion de PMU/RTC.

## PMU AXP2101

La wiki identifica `AXP2101` para energia, carga y bateria.

Datos que puede reportar segun wiki/ejemplos:

- Temperatura de chip.
- Estado de carga/descarga/standby.
- VBUS presente y estado de cargador.
- Voltaje de bateria.
- Voltaje VBUS.
- Voltaje de sistema.
- Porcentaje estimado de bateria.

Gotcha:

- El porcentaje de bateria del AXP2101 se basa en voltaje y no es lineal.
- En cargas, descargas y cambios de carga puede fluctuar mucho.
- Para decisiones de autonomia, preferir voltaje y tendencia antes que porcentaje instantaneo.

Estado actual:

- No esta expuesto por el BSP oficial.
- El ejemplo ESP-IDF oficial `01_AXP2101` porta `XPowersLib`.
- Si se integra, hacerlo como componente separado o driver minimo propio.

## Audio

APIs BSP:

```c
ESP_ERROR_CHECK(bsp_audio_init(NULL));
esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
```

Pines I2S relevantes:

| Senal | Pin |
| --- | --- |
| MCLK | GPIO16 |
| BCLK/SCLK | GPIO41 |
| WS/LCLK | GPIO45 |
| DOUT | GPIO40 |
| DIN/DSIN | GPIO42 |
| Amp enable | GPIO46 |

## microSD

APIs BSP:

```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE *f = fopen(BSP_SD_MOUNT_POINT "/file.txt", "r");
```

Pines:

| Senal | Pin |
| --- | --- |
| D0 | GPIO3 |
| CMD | GPIO1 |
| CLK | GPIO2 |

El BSP configura SDMMC 1-bit. No hay pin de card-detect declarado.

## Botones Y Recuperacion

- El BSP declara `BSP_CAPS_BUTTONS 0`.
- `BOOT` esta en `GPIO0` y puede usarse como input propio si la app lo necesita.
- El boton `PWR` esta relacionado con la PMU; tratarlo via `AXP2101` cuando se integre energia.
- Si un firmware deja la placa en crash y USB no responde, la FAQ recomienda mantener `BOOT` y encender para forzar modo descarga.
