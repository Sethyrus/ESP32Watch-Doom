# Setup And Build

Entorno recomendado para este repo: `ESP-IDF 5.5.4` con target `esp32s3`.

## Rutas Locales

Instalacion ESP-IDF verificada:

```sh
/Users/alex/.espressif/v5.5.4/esp-idf
```

Activacion del entorno:

```sh
source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"
```

Comprobar version:

```sh
idf.py --version
```

## Build

```sh
source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

## Flash Y Monitor

Puerto VS Code actual:

```sh
/dev/tty.usbmodem21301
```

Comando:

```sh
idf.py -p /dev/tty.usbmodem21301 flash monitor
```

Salir de monitor: `Ctrl+]`.

## Archivos De Configuracion

| Archivo | Tipo | Regla |
| --- | --- | --- |
| `sdkconfig.defaults` | Fuente durable | Editar aqui cambios de Kconfig que deben sobrevivir. |
| `sdkconfig` | Generado/local | No editar para cambios duraderos; esta git-ignored. |
| `partitions.csv` | Fuente durable | Tabla de particiones del proyecto. |
| `dependencies.lock` | Fuente durable generada | Bloquea versiones resueltas por ESP Component Manager. Actualizar al cambiar manifests. |
| `managed_components/` | Generado | Lo crea ESP Component Manager; esta git-ignored. |
| `build/` | Generado | Lo crea `idf.py build`; esta git-ignored. |

## Config Base Actual

Decisiones del baseline:

- Target: `esp32s3`.
- ESP-IDF: `5.5.4`.
- Flash mode: `QIO`.
- Flash size configurado: `16MB`.
- PSRAM: habilitada, octal, 80 MHz.
- CPU: 240 MHz.
- FreeRTOS tick: 1000 Hz.
- LVGL: v9.3.0 por manifest, con malloc/string/sprintf de libc.
- BSP: `waveshare/esp32_s3_touch_amoled_2_06`.

Nota sobre flash: la wiki indica 32 MB, pero los ejemplos ESP-IDF oficiales Waveshare usan 16 MB. Este repo arranca con 16 MB por compatibilidad con esos ejemplos. Si se quiere usar todo el flash, verificar primero con `esptool.py flash_id` y cambiar a `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y`.

## VS Code

La extension ESP-IDF debe apuntar a:

```text
/Users/alex/.espressif/v5.5.4/esp-idf
```

`clangd` usa `build/compile_commands.json`. Si cambian fuentes, dependencias o config, regenerar con:

```sh
idf.py build
```

## Dependencias

El componente `main` declara dependencias en `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=5.5,<5.6"
  waveshare/esp32_s3_touch_amoled_2_06: "^1.0.6"
  lvgl/lvgl:
    version: "9.3.0"
    public: true
```

Para anadir perifericos:

- IMU: anadir `waveshare/qmi8658`.
- RTC/PMU: preferir componentes separados o driver propio minimo; no mezclar todo en `main/main.c`.

## Problemas Frecuentes

`idf.py` no existe:

```sh
source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"
```

Build usa ESP-IDF incorrecto:

```sh
idf.py --version
```

Si aparece `v6.0.1`, revisar VS Code y shell.

Falla por componentes antiguos o cacheados:

```sh
idf.py reconfigure
```

Si sigue fallando por cache de build, borrar `build/` manualmente o desde el IDE. No borrar cambios fuente.
