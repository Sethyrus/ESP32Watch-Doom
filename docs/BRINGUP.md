# Bring-Up Checklist

Checklist para validar la placa antes de construir apps grandes. Completar resultados reales aqui o en un log corto de hardware.

## Objetivo

Confirmar que el baseline `ESP-IDF 5.5.4 + LVGL 9.3.0 + Waveshare BSP` controla correctamente la placa real `ESP32-S3-Touch-AMOLED-2.06`.

## Preparacion

```sh
source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

Flash/monitor esperado:

```sh
idf.py -p /dev/tty.usbmodem21301 flash monitor
```

Salir de monitor: `Ctrl+]`.

## Validacion Inicial

| Paso | Esperado | Resultado |
| --- | --- | --- |
| Build limpio | `idf.py build` termina OK. | Pendiente hardware. |
| Flash | Escribe bootloader/app/partition table sin errores. | Pendiente hardware. |
| Boot log | Sin panic/reboot loop. | Pendiente hardware. |
| Display | UI `ESP32S3Watch` visible. | Pendiente hardware. |
| Brillo | Brillo cambia con `bsp_display_brightness_set(80)`. | Pendiente hardware. |
| Touch | Widgets LVGL responden si se anade boton/gesture de prueba. | Pendiente hardware. |
| Recovery | Mantener `BOOT` al alimentar permite volver a flashear si hay crash. | Pendiente hardware. |

## Flash Real

La wiki indica 32 MB, pero ejemplos oficiales usan 16 MB. Antes de usar particiones de 32 MB:

```sh
esptool.py -p /dev/tty.usbmodem21301 flash_id
```

Anotar aqui:

| Dato | Resultado |
| --- | --- |
| Manufacturer | Pendiente |
| Device | Pendiente |
| Detected flash size | Pendiente |

No cambiar `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y` hasta confirmar la placa real.

## I2C Scan

Bus esperado: `SDA=GPIO15`, `SCL=GPIO14`, `400 kHz`, port `1` por BSP.

Direcciones esperadas:

| Dispositivo | Direccion esperada | Resultado |
| --- | --- | --- |
| FT3168 touch | `0x38` | Pendiente |
| QMI8658 IMU | `0x6B` preferida, `0x6A` posible | Pendiente |
| PCF85063 RTC | `0x51` tipica | Pendiente |
| AXP2101 PMU | `0x34` | Pendiente |
| ES8311 speaker codec | `0x30` | Pendiente |
| ES7210 mic codec | Verificar | Pendiente |

Si falta un dispositivo, comprobar alimentacion/PMU antes de asumir fallo del sensor.

## Display Y LVGL

Validar:

| Area | Esperado |
| --- | --- |
| Resolucion | `410 x 502`. |
| Orientacion | Portrait por BSP; rotacion solo si la app lo decide. |
| Color | RGB565 correcto, sin swap visual. |
| Refresco | Sin areas corruptas; el BSP ya redondea invalidate areas. |
| Locking | Toda modificacion desde tareas FreeRTOS usa `bsp_display_lock()`. |
| Brillo | 0 apaga/dim, 100 maximo; control por comando `0x51`, no PWM. |

## Touch

Validar con una pantalla simple que muestre coordenadas o cambie un boton LVGL.

| Area | Esperado |
| --- | --- |
| Init | `bsp_display_start()` registra input device. |
| Coordenadas | Rango aproximado `0..409`, `0..501`. |
| Gestos | No bloquearlos con contenedores clickables innecesarios. |
| Reset/INT | `RST GPIO9`, `INT GPIO38` por BSP. |

## IMU QMI8658

Integracion recomendada cuando se pase de demo de pantalla:

```c
i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
qmi8658_dev_t dev;
ESP_ERROR_CHECK(qmi8658_init(&dev, bus, QMI8658_ADDRESS_HIGH));
ESP_ERROR_CHECK(qmi8658_set_accel_range(&dev, QMI8658_ACCEL_RANGE_8G));
ESP_ERROR_CHECK(qmi8658_set_accel_odr(&dev, QMI8658_ACCEL_ODR_500HZ));
```

Validar:

| Area | Esperado |
| --- | --- |
| Direccion | `QMI8658_ADDRESS_HIGH` (`0x6B`) funciona. |
| Unidades default | Accel en milli-g; `1000.0` aproximadamente 1 g. |
| Mapeo pantalla | `screen_x = -accelY / 1000.0f`, `screen_y = accelX / 1000.0f`. |
| Calibracion | 100-200 muestras quieto, aplicar mismo mapeo antes de promediar. |
| Deadzone | Empezar con `0.015f` a `0.05f` en g. |

Si se activa `qmi8658_set_accel_unit_mps2(&dev, true)`, no dividir por `1000.0f`.

## RTC PCF85063

Pendiente de driver propio/minimo. Validar primero por I2C scan.

| Area | Esperado |
| --- | --- |
| Direccion | `0x51` tipica. |
| Persistencia | Mantiene hora con bateria si PMU/RTC estan bien alimentados. |
| API futura | `rtc_service` o componente propio, no en `main.c`. |

## PMU AXP2101

Pendiente de wrapper propio o port minimo de XPowersLib.

Validar:

| Area | Esperado |
| --- | --- |
| Direccion | `0x34`. |
| Datos | Temperatura chip, charging, VBUS, battery voltage, system voltage. |
| Porcentaje | Solo orientativo; preferir voltage/tendencia. |
| TS pin | Llamar equivalente a `disableTSPinMeasure()` si se usa XPowersLib. |
| PKEY | Usar para `PWR` si se decide integrar power key. |

No implementar politica de sleep/bateria antes de entender bien PWR/PMU.

## Botones

| Boton | Ruta esperada | Validacion |
| --- | --- | --- |
| BOOT | `GPIO0`, bajo al pulsar | Debounce, click/long press, recovery. |
| PWR | `EXIO6`, alto al pulsar segun wiki | Validar via PMU/expander; no asumir GPIO10. |

Regla: no usar long press de `PWR` cercano a 6 s para funciones de app porque apaga la placa.

## microSD

Usar BSP ESP-IDF:

```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE *f = fopen(BSP_SD_MOUNT_POINT "/file.txt", "r");
```

Validar:

| Area | Esperado |
| --- | --- |
| Bus | SDMMC 1-bit. |
| Pines BSP | `CLK GPIO2`, `CMD GPIO1`, `D0 GPIO3`. |
| Mount | `/sdcard`. |
| Card detect | No declarado. |
| GPIO17 | Solo referencia Arduino/SPI-style, no usar sin validar. |

## Audio

Validar en fase propia porque audio toca I2S, codecs y amplificador.

| Area | Esperado |
| --- | --- |
| Speaker | `bsp_audio_codec_speaker_init()`, ES8311, amp `GPIO46`. |
| Mic | `bsp_audio_codec_microphone_init()`, ES7210. |
| I2S | MCLK `GPIO16`, BCLK `GPIO41`, WS `GPIO45`, DOUT `GPIO40`, DIN `GPIO42`. |
| Default BSP | Mono duplex, 16-bit, 22050 Hz si `bsp_audio_init(NULL)`. |

## Bateria Y Termica

Validar solo con bateria segura y compatible.

| Area | Esperado |
| --- | --- |
| Bateria recomendada | `4*27*28`, `400 mAh`, 3.7 V MX1.25. |
| Full brightness | Aproximadamente 1 h segun FAQ. |
| Pantalla apagada | Aproximadamente 3-4 h segun FAQ. |
| Low-power | Aproximadamente 6 h segun FAQ. |
| Temperatura | Wiki midio hasta 46 C con Wi-Fi STA/AP y carga; sin Wi-Fi/BLE aprox. 36 C. |

No asumir esas autonomias para la app final; medir consumo real.

## Criterio Para Pasar A Apps

Pasar de bring-up a arquitectura de apps solo cuando esten cerrados:

- Build limpio reproducible desde checkout.
- Display + brillo + touch OK.
- `flash_id` documentado.
- I2C scan documentado.
- Decision tomada para primer periferico extra: IMU, RTC, PMU, SD o audio.
- Si se usa bateria, comportamiento PWR/PMU validado.
