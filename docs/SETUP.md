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
- BSP I2C: port 1, 400 kHz.
- Mounts BSP: SPIFFS `/spiffs`, SD `/sdcard`.

Nota sobre flash: la wiki y el esquematico indican 32 MB (`GD25Q256EYIGR`), pero los ejemplos ESP-IDF oficiales Waveshare usan 16 MB. Este repo arranca con 16 MB por compatibilidad con esos ejemplos. Si se quiere usar todo el flash, verificar primero con `esptool.py flash_id` y cambiar a `CONFIG_ESPTOOLPY_FLASHSIZE_32MB=y`.

## Particiones

La tabla actual es single-factory y no tiene OTA slots:

| Particion | Tipo | Tamano | Uso |
| --- | --- | --- | --- |
| `nvs` | data/nvs | `0x6000` | Config pequena, calibraciones, preferencias. |
| `phy_init` | data/phy | `0x1000` | Datos PHY ESP-IDF. |
| `factory` | app/factory | `8M` | Firmware principal. |
| `storage` | data/spiffs | `7M` | SPIFFS montado por BSP como `/spiffs`. |

No hay particion de coredump ni OTA. Si se necesita OTA, crash dumps persistentes o assets grandes en flash, redisenar `partitions.csv` antes de construir muchas apps.

## VS Code

La extension ESP-IDF debe apuntar a:

```text
/Users/alex/.espressif/v5.5.4/esp-idf
```

`clangd` usa `build/compile_commands.json`. Si cambian fuentes, dependencias o config, regenerar con:

```sh
idf.py build
```

## Devcontainer

El devcontainer usa la imagen Docker `espressif/idf:v5.5.4` para mantener la misma version que el baseline local. Evitar tags flotantes tipo `release-v5.5` si se necesita reproducibilidad exacta.

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

## Ejemplos Oficiales Waveshare

El repo oficial de Waveshare contiene ejemplos ESP-IDF bajo `examples/ESP-IDF-v5.4.2`. Se usan como referencia, pero este proyecto queda fijado en ESP-IDF `5.5.4`.

| Ejemplo | Valor para este repo |
| --- | --- |
| `01_AXP2101` | Referencia PMU/bateria/carga con XPowersLib. |
| `02_lvgl_demo_v9` | Referencia LVGL+BSP, particiones 8M app + 7M SPIFFS. |
| `03_esp-brookesia` | Referencia futura si se adopta framework de apps. |
| `04_Immersive_block` | Referencia IMU QMI8658 + fisicas LVGL. |
| `05_Spec_Analyzer` | Referencia microfonos/audio capture + visualizacion. |
| `06_videoplayer` | Referencia AVI desde TF con audio; requiere assets en SD. |

No copiar ejemplos completos al repo salvo que se porten conscientemente. Preferir extraer drivers o patrones minimos.

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

Si sigue fallando por cache de build, borrar `build/` manualmente o desde el IDE. Si se sospecha resolucion vieja de componentes, borrar tambien `managed_components/` y regenerar. `dependencies.lock` solo debe actualizarse si se aceptan nuevas versiones resueltas; no borrarlo como limpieza rutinaria aunque algunos textos de Waveshare lo sugieran para demos aisladas. No borrar cambios fuente.
