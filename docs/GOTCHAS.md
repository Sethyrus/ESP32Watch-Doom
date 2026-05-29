# Gotchas

Problemas conocidos y decisiones que deben recordarse antes de tocar el firmware.

## No Editar `sdkconfig` Para Cambios Duraderos

`sdkconfig` es generado y esta git-ignored. Si un cambio debe sobrevivir a un checkout limpio, ponerlo en `sdkconfig.defaults`, `partitions.csv` o el manifest correspondiente.

## ESP-IDF 5.5.4 Es La Base

El repo esta alineado con `ESP-IDF 5.5.4`. Evitar migrar a `6.x` sin una razon concreta, porque Brookesia, BSPs y ejemplos oficiales estan mas alineados con ramas `5.x`.

## Mantener Compatibles LVGL Y esp_lvgl_port

El BSP depende de `espressif/esp_lvgl_port`. La combinacion resuelta y verificada en este repo es `esp_lvgl_port 2.8.0~1` + `lvgl 9.3.0`. El intento con `lvgl 9.2.0` fallo por simbolos esperados por el port, como `LV_COLOR_FORMAT_RGB565_SWAPPED`.

## PSRAM Es Obligatoria Para UI Real

La placa tiene 8 MB de PSRAM octal. El `sdkconfig` generado anterior no la tenia activa. LVGL, buffers de display, audio/video y demos grandes necesitan PSRAM.

Config minima esperada:

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

## Flash: Wiki 32 MB, Ejemplos 16 MB

La wiki anuncia 32 MB de flash, pero los ejemplos ESP-IDF oficiales de Waveshare usan `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`.

Este repo arranca con 16 MB por compatibilidad. Para usar 32 MB, verificar primero la placa real con `esptool.py flash_id`.

Aunque `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y`, el comando final de esptool puede mostrar `--flash_mode dio`. En ESP-IDF 5.5 esto es normal para QIO/QOUT: esptool flashea el bootloader en modo DIO y el bootloader cambia a quad durante la inicializacion.

## CO5300 Vs SH8601

La wiki y ejemplos Arduino mencionan `CO5300`. El BSP ESP-IDF oficial usa `waveshare/esp_lcd_sh8601` y comandos SH8601.

Decision: para ESP-IDF, usar BSP oficial y tratar `SH8601` como fuente practica de verdad.

## Brillo No Es PWM

El BSP tiene comentarios antiguos que hablan de PWM, pero en esta placa el brillo se controla enviando comando QSPI `0x51` con parametro `0x00..0xFF`.

API BSP recomendada:

```c
bsp_display_brightness_set(75);
bsp_display_backlight_on();
bsp_display_backlight_off();
```

`bsp_display_start()` inicializa el brillo al 100%. Si la app quiere otro brillo inicial, llamarlo justo despues de arrancar display.

## Caveats BSP v1.0.6

El BSP tiene comentarios heredados de otros paneles/placas. Priorizar el codigo real y no la prosa del header.

- `bsp_display_start_with_config()` recibe `bsp_display_cfg_t`, pero el codigo actual calcula el buffer LVGL desde Kconfig (`CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT` o full-screen si avoid-tear), no desde todos los campos del struct.
- Las opciones/ayudas Kconfig mencionan RGB LCD y LEDC PWM, pero esta placa usa panel QSPI `SH8601` y brillo por comando `0x51`.
- El header I2C menciona dispositivos QMA7981/OV2640, pero en esta placa los dispositivos relevantes son FT3168, QMI8658, PCF85063, AXP2101 y codecs.

## LVGL No Es Thread-Safe

Toda llamada `lv_*` hecha fuera del task interno de LVGL debe estar protegida:

```c
if (bsp_display_lock(0)) {
    lv_label_set_text(label, "Hello");
    bsp_display_unlock();
}
```

No actualizar widgets desde tareas FreeRTOS sin lock.

## Touch Ya Lo Registra El BSP

`bsp_display_start()` inicializa display, touch y el input device LVGL. No crear otro driver touch salvo que se este reemplazando el BSP.

## QMI8658: Unidades Y Ejes

Por defecto el driver `waveshare/qmi8658` devuelve aceleracion en milli-g. Dividir entre `1000.0f` para obtener g.

Si se llama `qmi8658_set_accel_unit_mps2(&dev, true)`, ya no dividir por 1000; los datos estan en m/s2.

Mapeo probado para pantalla:

```c
float screen_x = -data.accelY / 1000.0f;
float screen_y =  data.accelX / 1000.0f;
```

Centralizar el mapeo en una sola funcion para evitar aplicar doble inversion en juegos o fisicas.

## BSP No Expone IMU, RTC Ni PMU

Aunque la placa los tiene, `BSP_CAPS_IMU` y `BSP_CAPS_BUTTONS` son 0. Para IMU usar `waveshare/qmi8658`; para RTC/PMU crear componente propio o portar lo minimo de los ejemplos oficiales. Para botones, `BOOT` es GPIO0, pero `PWR` aparece en wiki como `EXIO6`, no como GPIO directo del ESP32-S3.

## PWR Puede Apagar La Placa

El boton `PWR` tiene comportamiento de alimentacion ademas de posible input de usuario.

- Pulsado unos 6 s en encendido apaga la placa.
- Pulsacion en apagado enciende la placa.
- En runtime la wiki dice que se lee por `EXIO6` con nivel alto al pulsar.
- No implementar long-press de app cercano a 6 s sin gestionar el riesgo de apagado.

## AXP2101: Porcentaje De Bateria No Lineal

La wiki avisa que el porcentaje estimado puede fluctuar, especialmente con cargador conectado, cambios de carga o envejecimiento de bateria. Preferir voltaje y tendencia para decisiones importantes.

El ejemplo ESP-IDF oficial llama `PMU.disableTSPinMeasure()` porque la placa no tiene medida de temperatura de bateria por TS; dejar esa deteccion activa puede causar carga anomala.

## Seguridad De Bateria Y Agua

La wiki incluye advertencias de LiPo que deben respetarse en cualquier app que gestione energia:

- Usar bateria compatible, segura y con proteccion; evitar baterias/cargadores baratos o de baja calidad.
- No invertir polaridad al cargar/descargar.
- Evitar humedad, altas temperaturas, golpes, sobrecarga y sobredescarga.
- Para almacenamiento largo, retirar la bateria y evitar dejarla en estado de carga muy bajo.
- Reemplazar baterias envejecidas al final de su vida util o tras unos dos anos.
- La placa no es waterproof; mantenerla seca.

## microSD: GPIO17 Es Solo Referencia Arduino

La wiki/ejemplos Arduino etiquetan la TF card como SPI con `CS GPIO17`, `DI/MOSI GPIO1`, `DO/MISO GPIO3`, `SCK GPIO2`. En ESP-IDF el BSP usa SDMMC 1-bit con `CLK GPIO2`, `CMD GPIO1`, `D0 GPIO3`, sin CS ni card-detect.

Decision: para ESP-IDF usar `bsp_sdcard_mount()` y no configurar `GPIO17` salvo que se porte deliberadamente un driver SPI-style y se valide en hardware.

## Factory Firmware Puede Asumir 32 MB

El repo oficial contiene firmware factory/test para self-check. Uno de los bins vistos mide unos 29 MB, asi que no encaja con la config baseline de 16 MB de este repo. No flashear factory bins grandes sin confirmar flash real y offsets.

## Arduino LVGL Menos Fluido Que ESP-IDF

La wiki indica que los ejemplos LVGL en Arduino son menos fluidos porque la ruta Arduino TFT/DMA acelera menos. Los ejemplos ESP-IDF usan configuraciones de buffering/anti-tearing mas adecuadas. No extrapolar rendimiento Arduino al baseline ESP-IDF.

## Brookesia No Es Baseline

ESP-Brookesia es util si se necesita launcher tipo telefono, lifecycle de apps, SquareLine o servicios/AI. Para una base de reloj, Poketch, juegos simples o validacion hardware, LVGL+BSP reduce dependencias y riesgo.

## Linker Y Registro Estatico

Si en el futuro se implementa un sistema de apps con auto-registro por constructores estaticos, cada componente de app debe usar `WHOLE_ARCHIVE` en `idf_component_register(...)`. Si no, el linker puede eliminar los registros.

## Arduino No Es Fuente Principal

Los ejemplos Arduino son utiles para entender sensores y comportamiento, pero este repo es ESP-IDF. Para pines, display y touch, preferir el BSP ESP-IDF oficial.

## Recovery

Si el firmware crashea y el USB no responde, mantener `BOOT` y encender/resetear para entrar en modo descarga antes de flashear de nuevo.

Notas de FAQ:

- Si el flasheo falla porque el monitor ocupa el puerto, cerrar monitor y reintentar.
- Si la placa entra en modo descarga forzado, puede no salir automaticamente tras flashear; apagar y reiniciar.
- Si el monitor queda en `waiting for download...`, volver a alimentar/reiniciar la placa.
- Para volver a encender tras apagado completo, la FAQ indica mantener `PWR` al menos 6 s y luego pulsar `PWR` otra vez.
