# Architecture

Este documento define la arquitectura base actual y los criterios para evolucionarla.

## Decision Actual

Usar `ESP-IDF 5.5.4 + LVGL + Waveshare BSP` como base principal.

Motivos:

- Es la ruta mas directa para validar display, touch, brillo, SD, audio e I2C.
- El BSP oficial ya encapsula la parte delicada del panel QSPI y touch.
- Reduce dependencias y complejidad frente a ESP-Brookesia.
- Permite construir UI propia sin adoptar un launcher o lifecycle de telefono.
- Encaja mejor con ideas tipo reloj, Poketch, demos hardware y juegos simples.

## Por Que No Brookesia Ahora

ESP-Brookesia aporta valor real, pero no para el baseline minimo.

Usarlo ahora introduciria:

- Framework de sistema y apps con lifecycle propio.
- Dependencias extra como Boost y componentes Brookesia.
- Registro de apps por constructores estaticos y posible necesidad de `WHOLE_ARCHIVE`.
- Mas reglas de navegacion, status bar, recents y gestos.
- Mayor superficie de problemas antes de validar la placa.

Brookesia se considerara si el producto necesita:

- Launcher multi-app tipo telefono.
- Apps aisladas con lifecycle formal.
- Integracion fuerte con SquareLine.
- Servicios de audio/video/AI de Espressif.
- Experiencia mas cercana a un sistema operativo movil.

## Estructura Recomendada

Mantener `main` pequeno. Cuando una pieza sea reutilizable o crezca, moverla a un componente.

Estructura inicial:

```text
main/
├── main.c
└── idf_component.yml
```

Estructura sugerida al crecer:

```text
components/
├── board_services/      # wrappers para IMU, RTC, PMU, storage, audio
├── ui_shell/            # navegacion global, tema, pantallas base
└── apps/                # apps o demos si se adopta arquitectura modular
main/
└── main.c               # solo bootstrap
```

Si se usa `components/apps/*`, actualizar el `CMakeLists.txt` raiz con `EXTRA_COMPONENT_DIRS`.

## Reglas LVGL

- Crear y modificar objetos LVGL solo con `bsp_display_lock()` tomado.
- No hacer trabajo pesado dentro de callbacks de UI.
- Si una tarea FreeRTOS procesa sensores, que publique estado y haga update UI bajo lock.
- Evitar buffers grandes en RAM interna; usar PSRAM cuando aplique.

## Hardware Services

La app no deberia hablar directamente con todos los registros de hardware una vez que crezca. Encapsular:

- `imu_service`: init QMI8658, calibracion, ejes de pantalla, filtros.
- `rtc_service`: hora/fecha PCF85063, fallback SNTP si aparece Wi-Fi.
- `power_service`: AXP2101, voltajes, bateria, PWR key.
- `storage_service`: NVS, SPIFFS y SD.
- `audio_service`: speaker/mic sobre BSP codec APIs.

## Configuracion Durable

Todo cambio de Kconfig debe ir a `sdkconfig.defaults`. Toda decision de particiones debe ir a `partitions.csv`.

No depender de `sdkconfig` para estado del proyecto.

## DESIGN.md

No hay `DESIGN.md` aun porque todavia no hay un producto final cerrado. Si el proyecto se fija como Poketch, reloj, launcher, Doom watch u otra direccion, crear un `DESIGN.md` de producto con UX, apps, navegacion y alcance.
