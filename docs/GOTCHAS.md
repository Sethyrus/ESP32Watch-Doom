# Gotchas

Problemas conocidos y decisiones que deben recordarse antes de tocar el firmware.

## No Editar `sdkconfig` Para Cambios Duraderos

`sdkconfig` es generado y esta git-ignored. Si un cambio debe sobrevivir a un checkout limpio, ponerlo en `sdkconfig.defaults`, `partitions.csv` o el manifest correspondiente.

## ESP-IDF 5.5.4 Es La Base

El repo esta alineado con `ESP-IDF 5.5.4`. Evitar migrar a `6.x` sin una razon concreta, porque Brookesia, BSPs y ejemplos oficiales estan mas alineados con ramas `5.x`.

## Mantener Compatibles LVGL Y esp_lvgl_port

El BSP depende de `espressif/esp_lvgl_port`. Versiones recientes del port esperan simbolos de LVGL 9.3+, como `LV_COLOR_FORMAT_RGB565_SWAPPED`. Por eso el manifest fija `lvgl/lvgl` en `9.3.0` en vez de `9.2.0`.

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

Aunque la placa los tiene, `BSP_CAPS_IMU` y `BSP_CAPS_BUTTONS` son 0. Para IMU usar `waveshare/qmi8658`; para RTC/PMU crear componente propio o portar lo minimo de los ejemplos oficiales.

## AXP2101: Porcentaje De Bateria No Lineal

La wiki avisa que el porcentaje estimado puede fluctuar, especialmente con cargador conectado, cambios de carga o envejecimiento de bateria. Preferir voltaje y tendencia para decisiones importantes.

## Brookesia No Es Baseline

ESP-Brookesia es util si se necesita launcher tipo telefono, lifecycle de apps, SquareLine o servicios/AI. Para una base de reloj, Poketch, juegos simples o validacion hardware, LVGL+BSP reduce dependencias y riesgo.

## Linker Y Registro Estatico

Si en el futuro se implementa un sistema de apps con auto-registro por constructores estaticos, cada componente de app debe usar `WHOLE_ARCHIVE` en `idf_component_register(...)`. Si no, el linker puede eliminar los registros.

## Arduino No Es Fuente Principal

Los ejemplos Arduino son utiles para entender sensores y comportamiento, pero este repo es ESP-IDF. Para pines, display y touch, preferir el BSP ESP-IDF oficial.

## Recovery

Si el firmware crashea y el USB no responde, mantener `BOOT` y encender/resetear para entrar en modo descarga antes de flashear de nuevo.
