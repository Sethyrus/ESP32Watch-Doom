# Doom Port Research And Design

Investigacion y plan de implementacion para la rama `app/doom`. Este documento captura las decisiones tomadas antes de empezar a portar codigo y debe mantenerse actualizado durante el desarrollo.

## Objetivo

Valorar y planificar un port jugable de Doom para la placa Waveshare `ESP32-S3-Touch-AMOLED-2.06`, manteniendo el baseline del proyecto:

| Area | Decision base |
| --- | --- |
| Framework | `ESP-IDF 5.5.4` |
| Board layer | BSP oficial `waveshare/esp32_s3_touch_amoled_2_06` |
| MCU | `ESP32-S3R8`, dual-core LX7, 240 MHz |
| PSRAM | 8 MB octal, 80 MHz |
| Display | AMOLED 410 x 502, QSPI, SH8601, RGB565 |
| Storage inicial | microSD por BSP SDMMC 1-bit |
| Modo Doom inicial | Firmware standalone, no app dentro de shell LVGL |
| UI general | LVGL sigue siendo baseline para apps normales, pero Doom no lo usa en runtime MVP |
| Licencia motor | GPL aceptada para esta rama/firmware Doom |

El objetivo inicial no es un producto final. El primer hito es que el firmware standalone de Doom compile con `idf.py build`. Despues, la PoC medible debe cubrir arranque, WAD cargado, frames visibles, input minimo, FPS/heap registrados y sin panic.

## Decisiones Cerradas Para La Primera Implementacion

| Area | Decision | Razon |
| --- | --- | --- |
| Producto inicial | Firmware standalone de Doom | Maximiza rendimiento y reduce lifecycle/ownership con LVGL |
| Prioridad inmediata | Llegar a `idf.py build` OK | Permite iterar sobre errores reales antes de optimizar |
| Motor | `DoomGeneric` | Menor superficie de port: `DG_*`, timing, input, display y storage |
| Licencia | Aceptar GPL para esta rama | Es la via practica para usar DoomGeneric/Doom source |
| Assets | WAD aportado por usuario en microSD | Evita commitear assets y tocar particiones al inicio |
| Render | Directo SH8601/QSPI con `bsp_display_new()` | Evita LVGL/task/locks durante el bucle Doom |
| LVGL | No usar en runtime Doom MVP | Menos overhead y menos riesgo de threading |
| Audio | Deshabilitado inicialmente | Evita I2S/codec/mixer hasta tener video estable |
| Input MVP | Touch por zonas + `BOOT` | Hardware disponible sin accesorios externos |
| `PWR` | No usar en MVP | Pasa por AXP2101/PWRON y puede apagar la placa |

Regla de trabajo: si hay conflicto entre hacerlo perfecto y hacerlo compilar, primero compilar con stubs limpios y logs accionables. La funcionalidad se completa en fases posteriores sin esconder warnings ni desactivar diagnosticos globalmente.

## Plan Compile-First

Secuencia inicial de desarrollo, cuando se autorice empezar:

| Orden | Paso | Criterio de salida |
| --- | --- | --- |
| 1 | Importar `DoomGeneric` como componente GPL separado | Fuente exacta y licencia conservadas |
| 2 | Crear `components/doom_app` con HAL ESP32-S3/Waveshare | `DG_*` declaradas y enlazadas |
| 3 | Reemplazar demo LVGL por bootstrap standalone pequeno | `main/main.c` solo arranca Doom |
| 4 | Arrancar con display/input/audio stubs si hace falta | Primer `idf.py build` OK |
| 5 | Inicializar display directo con `bsp_display_new()` | Build OK y boot log claro |
| 6 | Montar SD con `bsp_sdcard_mount()` | `/sdcard/doom1.wad` validado o error claro |
| 7 | Dibujar `DG_DrawFrame()` en SH8601 | Doom visible o patron de diagnostico visible |
| 8 | Anadir input minimo touch + `BOOT` | Eventos `keydown/keyup` limpios |
| 9 | Medir FPS, heap y stack | Logs cada 5 s sin panic |

El desarrollo no debe tocar `partitions.csv` ni `sdkconfig.defaults` salvo que el build o el tamano final lo exijan y la razon quede documentada.

## Estado De Implementacion

Hito compile-first completado.

| Area | Estado |
| --- | --- |
| Motor | DoomGeneric importado en `components/doomgeneric` |
| Fuente exacta | `ozkl/doomgeneric` commit `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284` |
| Licencia | GPL-2.0 preservada en `components/doomgeneric/vendor/LICENSE` |
| App propia | `components/doom_app` con `doom_app_start()` y tarea FreeRTOS dedicada |
| Bootstrap | `main/main.c` arranca firmware standalone Doom en vez de demo LVGL |
| BSP/LVGL | Dependencias conservadas para la siguiente fase, pero no usadas en runtime Doom actual |
| Display | Stub: `DG_DrawFrame()` solo loguea FPS/heap si el motor corre |
| Storage | Stub: si `/sdcard/doom1.wad` no existe, no arranca motor y suspende la tarea |
| Input | Stub: `DG_GetKey()` no emite eventos todavia |
| Audio | Deshabilitado por argv con `-nosound -nomusic` |
| Build | `idf.py build` OK |
| Tamano binario | `0x7d630` bytes; slot app `8M`, `0x7829d0` bytes libres |
| Memoria tras build | DIRAM usada 273,567 bytes, quedan 68,193; `.ext_ram.bss` reportada como 84,992 bytes |

Parches locales aplicados al vendor:

| Archivo | Cambio | Motivo |
| --- | --- | --- |
| `doomgeneric.c` | `DG_ScreenBuffer` usa `heap_caps_malloc(... SPIRAM ...)` con fallback | Evitar consumir SRAM interna |
| `i_system.c` | Zone memory usa PSRAM con fallback; error GUI desktop desactivado en `ESP_PLATFORM` | `-mb 4` debe ir a PSRAM y no debe llamar `zenity/system()` |
| `r_plane.c` | `visplanes` usa `EXT_RAM_BSS_ATTR` | Resolver overflow DRAM manteniendo limite `MAXVISPLANES=128` |
| `CMakeLists.txt` vendor | Fuentes listadas explicitamente, `DOOMGENERIC_RESX=320`, `DOOMGENERIC_RESY=240` | Build reproducible y framebuffer 320 x 240 |
| `CMakeLists.txt` vendor | `-Wno-error` solo para warnings concretos de third-party | Mantener `-Werror` en codigo propio sin usar `-w` global |

Advertencia: el build todavia muestra warnings del codigo third-party DoomGeneric. No bloquean el firmware porque estan limitados al componente vendor. Antes de endurecer esta rama, decidir si conviene parchearlos uno a uno o mantenerlos documentados.

## Estado Local Relevante

Datos ya documentados en este repo:

| Recurso | Dato relevante |
| --- | --- |
| `docs/HARDWARE.md` | Pines, display, SD, touch, IMU, audio, PMU y BSP |
| `docs/GOTCHAS.md` | LVGL no thread-safe, SH8601/CO5300, brillo QSPI, PSRAM, SD, PWR |
| `sdkconfig.defaults` | CPU 240 MHz, PSRAM octal 80 MHz, flash 16 MB, app 8 MB |
| `partitions.csv` | `factory` 8 MB, `storage` SPIFFS 7 MB |
| BSP display | `bsp_display_new()`, `bsp_display_start()`, `bsp_display_brightness_set()` |
| BSP SD | `bsp_sdcard_mount()` y mount `/sdcard` |

Punto pendiente detectado: `sdkconfig.defaults` fija `CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT=100`, pero el `sdkconfig` generado local visto durante la investigacion estaba en `40`. Para Doom se recomienda no depender de LVGL, pero conviene resolver la discrepancia cuando se regenere configuracion.

## Repos Evaluados

### `espressif/esp32-doom`

Repo: `https://github.com/espressif/esp32-doom`.

Resumen:

| Area | Observacion |
| --- | --- |
| Motor | PrBoom sobre Doom source |
| Estado | Proof-of-concept no soportada oficialmente |
| Hardware objetivo | ESP32 original con 4 MB flash + 4 MB PSRAM, tipo ESP-Wrover |
| Display | SPI ILI9341/ST7789, no SH8601 QSPI |
| Build | ESP-IDF antiguo con Makefiles, no CMake moderno |
| Assets | WAD en particion raw `wad` en `0x100000` |
| Input | Mando PS1/PS2 por GPIO |
| Audio | No soporta sonido ni musica |
| Saves | No soporta guardar/cargar partidas |
| Bugs conocidos | Menus pueden crashear por datos PrBoom no incluidos |

Particiones usadas por la PoC:

```csv
factory, app,  factory, 0x10000, 928k
wifidata,data, nvs,    0xFC000, 16K
wad,     66,    6,     0x100000, 3072K
```

Lecciones aprovechables:

| Pieza | Valor para este proyecto |
| --- | --- |
| `i_video.c` | Framebuffer 320 x 240, paleta 8-bit, conversion a RGB565 |
| `spi_lcd.c` | Pipeline con DMA, conversion por chunks y doble buffer |
| `i_system.c` | Acceso a WAD en particion raw y uso de `esp_partition_mmap()` |
| `i_sound.c` | Stubs de audio para primera PoC sin sonido |
| `app_main.c` | Doom en tarea FreeRTOS con stack grande |

Conclusion: no conviene portarlo literalmente. Sirve como referencia de tecnicas ESP32, memoria y WAD, pero arrastra demasiados supuestos viejos: ESP32 no S3, display SPI distinto, IDF antiguo, input externo y PrBoom con features incompletas.

### `ozkl/doomgeneric`

Repo: `https://github.com/ozkl/doomgeneric`.

Resumen:

| Area | Observacion |
| --- | --- |
| Motor | Doom source adaptado para facilitar ports |
| API de plataforma | 6 funciones: `DG_Init`, `DG_DrawFrame`, `DG_SleepMs`, `DG_GetTicksMs`, `DG_GetKey`, `DG_SetWindowTitle` |
| Loop | `doomgeneric_Create()` una vez y `doomgeneric_Tick()` en bucle |
| Audio | No trivial; README recomienda mirar SDL si se necesita sonido |
| Licencia | GPL-2.0 |

API minima de port:

```c
void DG_Init(void);
void DG_DrawFrame(void);
void DG_SleepMs(uint32_t ms);
uint32_t DG_GetTicksMs(void);
int DG_GetKey(int *pressed, unsigned char *key);
void DG_SetWindowTitle(const char *title);
```

Conclusion: es la mejor base para una primera PoC porque reduce el trabajo de integracion a display, storage, timing e input.

### `Komedenden/esp32-s3-doom-port`

Repo: `https://github.com/Komedenden/esp32-s3-doom-port`.

Resumen:

| Area | Observacion |
| --- | --- |
| Motor | DoomGeneric |
| Target | ESP32-S3 N16R8 |
| Display | ILI9341 por SPI |
| Storage | microSD por SDSPI, WAD en `/sdcard/doom1.wad` |
| Input | 6 botones GPIO |
| Framebuffer | PSRAM |
| Render | RGBA8888 a RGB565 por chunks de 40 lineas |
| Rendimiento reportado | 15 FPS, 104 KB SRAM, 4.7 MB PSRAM |

Arquitectura reportada por el repo:

```text
DG_ScreenBuffer (PSRAM, RGBA8888)
        |
        | CPU: color conversion + aspect correction
        v
line_buffers[0 or 1] (internal SRAM, RGB565, CHUNK_LINES lines)
        |
        | DMA -> SPI -> LCD
        v
       LCD
```

Conclusion: es una referencia muy util para CMake, DoomGeneric, SD y pipeline de render en ESP32-S3. No es copiable tal cual porque usa ILI9341 SPI, SDSPI compartida y GPIOs que no encajan con esta placa.

### Investigacion Adicional De Source Ports

Fuentes revisadas despues de la investigacion inicial:

| Fuente | Leccion util |
| --- | --- |
| ActuallyTaylor, `Porting Doom (Part 1)` | `DG_DrawFrame()` concentra la mayor parte del trabajo real: adaptar el framebuffer al display y generar eventos de input |
| Reddit guide de source ports | `GZDoom` es para PC/modding avanzado; para microcontrolador interesan bases mas pequenas tipo DoomGeneric/Chocolate/PrBoom |
| Mattias Gustavsson `doom-crt` | Buena filosofia para esta rama: cambios minimos, pocas dependencias, build simple y fuente facil de auditar |
| Doomworld `where to start...` | No empezar un source port desde cero; usar una base existente, compilar primero, entender subsistemas y modificar poco a poco |
| DoomWiki `Doom source code` | Subsistemas por prefijo: `I_*` plataforma, `W_*` WAD, `R_*` render, `P_*` juego, `Z_*` memoria |
| DoomWiki `Static limits` | Vanilla usa limites estaticos y zone memory; para MVP conviene evitar WADs/mods complejos |
| Retro-Go / PrBoom-go | Referencia de firmware de juegos en ESP32, pero demasiado grande para el primer port standalone de esta placa |

Conclusiones practicas:

| Tema | Decision para este proyecto |
| --- | --- |
| Base | Mantener `DoomGeneric` para la primera version funcional |
| Alcance | No intentar compatibilidad GZDoom/Boom/MBF ni mods modernos en MVP |
| Build | Evitar copiar proyectos que silencian warnings con `-w`; arreglar o aislar warnings reales |
| Runtime | Priorizar display directo, WAD por SD, audio off e input minimo |
| Roadmap | Separar `compila`, `arranca`, `dibuja`, `se controla`, `se optimiza` |

## Base Recomendada

Usar `DoomGeneric` como motor y crear una HAL propia para esta placa. La primera implementacion debe ser firmware standalone de Doom, no una app dentro de un shell, para maximizar rendimiento y minimizar dependencias activas.

No usar LVGL para el render de Doom en la primera PoC. Motivos:

| Motivo | Detalle |
| --- | --- |
| Overhead | Doom ya genera frames completos; LVGL aporta poco para el bucle principal |
| Threading | LVGL requiere lock y task propio; complica el frame pacing |
| Control | Render directo permite medir DMA, conversion y escalado sin widgets |
| Riesgo | Menos dependencias activas durante la PoC |

LVGL puede seguir siendo el shell de otras apps o un launcher futuro, pero la primera rama Doom debe tomar ownership claro de display/touch/SD desde `app_main()` y no arrancar el port LVGL.

## Arquitectura Objetivo Inicial

Estructura sugerida para el desarrollo:

```text
components/
|-- doom_app/
|   |-- CMakeLists.txt
|   |-- doom_app.c           # app init, argv y loop Doom
|   |-- doom_port.c          # DG_* requeridas por DoomGeneric
|   |-- doom_display.c       # SH8601/QSPI direct draw
|   |-- doom_input.c         # touch/BOOT a eventos Doom
|   |-- doom_storage.c       # WAD desde SD o particion raw futura
|   `-- doom_config.h        # resolucion, chunks, flags de PoC
|
`-- doomgeneric/             # codigo third-party GPL importado/vendor

main/
`-- main.c                   # bootstrap pequeno
```

Reglas:

| Regla | Motivo |
| --- | --- |
| No mezclar el motor Doom completo en `main/main.c` | Mantener entrada pequena y revisable |
| Mantener `doomgeneric` separado de `doom_app` | Separar third-party GPL de la HAL propia |
| Registrar fuente exacta del motor | Reproducibilidad/licencia |
| No usar `file(GLOB_RECURSE ...)` si se puede listar fuentes estables | Builds mas predecibles |
| No desactivar warnings globalmente con `-w` | Evitar ocultar problemas de port |

## Display Y Render

### Datos Del Panel

| Area | Valor |
| --- | --- |
| Resolucion fisica | 410 x 502 |
| Bus | QSPI sobre `SPI2_HOST` |
| Driver | `waveshare/esp_lcd_sh8601` via BSP |
| Pixel format | RGB565 |
| Clock BSP | 40 MHz |
| Brillo | comando QSPI `0x51`, no PWM |
| Offset BSP | `esp_lcd_panel_set_gap(panel_handle, 0x16, 0)` |
| Restriccion area | SH8601 necesita areas alineadas; el BSP redondea LVGL, render directo debe hacerlo tambien |

### Formato De Frame

Doom clasico renderiza internamente `320 x 200`. DoomGeneric upstream usa `640 x 400` por defecto, asi que el port fija explicitamente `DOOMGENERIC_RESX=320` y `DOOMGENERIC_RESY=240` al compilar para el MVP. El framebuffer 320 x 240 deja margen vertical para mantener aspecto 4:3 en pixeles cuadrados.

Motivos:

| Motivo | Detalle |
| --- | --- |
| Compatibilidad | Muchos ports embebidos trabajan alrededor de 320 px de ancho |
| Coste | 64,000 pixeles de motor; 76,800 pixeles si se expande a 320 x 240 |
| Memoria | 8-bit: 64 KB; RGBA8888 upstream: 256 KB; RGB565 output 320 x 240: 150 KB |
| Rendimiento | Permite medir FPS antes de escalar a panel completo |

Opciones de salida:

| Modo | Descripcion | Pros | Contras |
| --- | --- | --- | --- |
| 320 x 200 centrado | Dibujar el framebuffer nativo con barras verticales dentro del panel portrait | Menor coste, primer hito rapido | Aspecto no corregido |
| 320 x 240 centrado | Expandir verticalmente 320 x 200 a 4:3 dentro del panel portrait | Aspecto correcto y coste bajo | Requiere escalado simple |
| 410 x 307 escalado | Escalar 4:3 al ancho del panel portrait | Usa mejor el ancho | Conversion mas cara |
| 502 x 376 landscape logico | Rotar por software para usar la placa en horizontal | Mejor UX para Doom | Mas coste CPU y mas complejidad de coordenadas |
| Full panel 410 x 502 | Frame completo con barras/rotacion | Control total del output | Mayor transferencia y conversion |

Modo recomendado para el primer hito visible: aceptar `320 x 200 centrado` si desbloquea el primer frame, y pasar rapido a `320 x 240 centrado` con escalado vertical simple. Despues medir y pasar a `410 x 307`. Dejar `landscape` para una fase posterior.

### Pipeline De Render Recomendado

Primera version:

```text
Doom framebuffer in PSRAM
        |
        | CPU convert/scale chunk
        v
DMA chunk buffer in internal SRAM, RGB565 byte-swapped
        |
        | esp_lcd_panel_draw_bitmap()
        v
SH8601 QSPI panel
```

Reglas:

| Regla | Razon |
| --- | --- |
| Mantener framebuffer grande en PSRAM | Ahorrar SRAM interna |
| Usar buffers DMA pequenos en SRAM interna | Compatibilidad y latencia DMA |
| Convertir a RGB565 justo antes de enviar | El panel/BSP espera RGB565 |
| Validar byte order con patron de color | El BSP LVGL usa `swap_bytes=true` |
| Alinear areas SH8601 | Evitar corrupcion de refresco |
| No llamar APIs LVGL desde el loop Doom | Evitar lock/task contention |

Tamanos iniciales razonables:

| Buffer | Tamano aproximado |
| --- | --- |
| DoomGeneric 320 x 200 8-bit (`CMAP256`) | 64 KB nominal; upstream reserva mas si no se parchea |
| DoomGeneric 320 x 200 RGBA8888 | 256 KB |
| RGB565 320 x 240 output completo | 150 KB |
| RGB565 320 x 40 chunk | 25.6 KB |
| Dos chunks RGB565 320 x 40 | 51.2 KB |
| RGB565 410 x 40 chunk | 32.8 KB |
| Dos chunks RGB565 410 x 40 | 65.6 KB |

Si se usa DoomGeneric upstream sin `CMAP256`, el framebuffer sera RGBA8888 y asumible con 8 MB PSRAM. Si se consigue usar `CMAP256` de forma limpia, se reduce memoria y conversion, pero no debe bloquear el primer build.

Formato inicial elegido: compilar con framebuffer `320 x 240` y arrancar con `-gfxmode rgb565`, soportado por el commit importado de DoomGeneric. Esto reduce la conversion futura en `DG_DrawFrame()`, aunque upstream todavia reserva `DG_ScreenBuffer` como `RESX * RESY * 4`.

Gotcha: en DoomGeneric upstream, `doomgeneric_Create()` reserva `DG_ScreenBuffer` con `malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4)` antes de llamar a `DG_Init()`. Para firmware ESP32-S3 hay que controlar esa reserva con un parche minimo o wrapper para ubicar el framebuffer en PSRAM. No reasignar el puntero en `DG_Init()` sin evitar/freear la reserva previa.

## Storage Y WAD

Primera decision: cargar WAD desde microSD por BSP.

Motivos:

| Motivo | Detalle |
| --- | --- |
| No requiere tocar particiones | Evita redisenar flash antes de validar Doom |
| Permite WADs grandes | Shareware, Freedoom o WAD legal del usuario |
| Encaja con BSP | `bsp_sdcard_mount()` usa SDMMC 1-bit ya documentado |
| Iteracion rapida | Cambiar WAD sin reflashear firmware |

Ruta inicial propuesta:

```text
/sdcard/doom1.wad
```

Alternativas posteriores:

| Alternativa | Uso |
| --- | --- |
| SPIFFS `/spiffs` | Solo WAD pequeno o assets auxiliares; particion actual 7 MB |
| Particion raw `wad` | Mejor para mmap/flash, pero requiere redisenar `partitions.csv` |
| Flash 32 MB | Solo tras validar `esptool.py flash_id` en hardware real |

No commitear WADs al repo salvo que se haya revisado y documentado su licencia.

Argumentos iniciales propuestos para el arranque standalone:

```text
doom -iwad /sdcard/doom1.wad -mb 4 -nosound -nomusic -nogui -gfxmode rgb565
```

Notas:

| Parametro | Motivo |
| --- | --- |
| `-iwad /sdcard/doom1.wad` | WAD externo aportado por usuario |
| `-mb 4` | Zone memory inicial contenida; ajustar tras medir |
| `-nosound -nomusic` | Mantener audio fuera del MVP |
| `-nogui` | Evitar rutas desktop de error popup |
| `-gfxmode rgb565` | El commit importado lo soporta y reduce conversion futura |
| Resolucion | Fijar por CMake/defines: `DOOMGENERIC_RESX=320`, `DOOMGENERIC_RESY=240` |

## Input

El control es el mayor problema de producto.

Opciones iniciales:

| Input | Viabilidad | Notas |
| --- | --- | --- |
| Touch por zonas | Alta | Direccional virtual + fire/use/menu |
| `BOOT` GPIO0 | Alta | Boton fisico unico; activo bajo; mantener recovery |
| IMU QMI8658 | Media | Giro/strafe/aim experimental; usar `waveshare/qmi8658` |
| PWR | Baja | No usar en MVP; ruta AXP2101/PWRON y riesgo de apagado |
| Mando externo | Media | Requiere pads/expansion y pinout validado |

Mapping minimo de PoC:

| Accion Doom | Input inicial sugerido |
| --- | --- |
| Move forward/back | Touch zona superior/inferior |
| Turn left/right | Touch zona izquierda/derecha |
| Fire | `BOOT` activo bajo |
| Use/open | Zona touch central/inferior |
| Menu/escape | Zona touch superior mantenida o combinacion simple |

Si no se usa LVGL, el touch debe leerse mediante driver `esp_lcd_touch`/BSP touch sin depender del input device LVGL. Hay que definir si Doom toma ownership del touch durante su ejecucion.

Para DoomGeneric, el input debe emitirse como eventos de transicion (`keydown`/`keyup`), no como niveles repetidos indefinidamente. La HAL de input debe guardar el estado anterior de cada zona/boton y devolver un evento por llamada a `DG_GetKey()`.

Prioridad practica: si touch bloquea el primer build, aceptar temporalmente stubs de input o solo `BOOT`; completar touch inmediatamente despues del primer `idf.py build` OK.

## Audio

No incluir audio en la primera PoC.

Motivos:

| Motivo | Detalle |
| --- | --- |
| Riesgo tecnico | Doom sound mixing + I2S + codec agrega otra dimension de bugs |
| Performance | Audio compite por CPU y memoria |
| BSP disponible | Se puede integrar despues por ES8311/I2S cuando video sea estable |
| Referencias | `esp32-doom` stubea sonido y musica |

Fase posterior:

| Audio | Ruta posible |
| --- | --- |
| SFX mono | Mezclador software a 11025/22050 Hz hacia ES8311 |
| Musica | Posponer; MIDI/MUS es mucho mas caro |
| Volumen | BSP codec + preferencias NVS |

## Tasking Y Timing

Modelo inicial recomendado:

| Tarea | Core sugerido | Prioridad | Notas |
| --- | --- | --- | --- |
| Doom loop | Core 1 | Media/alta | `doomgeneric_Tick()` + render |
| Display DMA callback | ISR/driver | N/A | Senal por semaforo si se usa async |
| Input polling | Mismo loop o tarea baja | Baja/media | Evitar locks largos |

Empezar con una sola tarea Doom que haga tick, input y draw. Separar display task solo si las mediciones muestran beneficio.

Stack inicial a probar: 16 KB a 24 KB para la tarea Doom. `esp32-doom` usa 22480 bytes para `doomEngineTask`; usarlo como referencia inicial y medir high-water mark.

Timing:

| Funcion | Implementacion propuesta |
| --- | --- |
| `DG_GetTicksMs()` | `esp_timer_get_time() / 1000` |
| `DG_SleepMs()` | `vTaskDelay(pdMS_TO_TICKS(ms))` |
| FPS log | Cada 5 s con heap interna/PSRAM |

## Memoria Y Config

Config local favorable:

| Config | Estado esperado |
| --- | --- |
| `CONFIG_SPIRAM=y` | Requerido |
| `CONFIG_SPIRAM_MODE_OCT=y` | Requerido para esta placa |
| `CONFIG_SPIRAM_SPEED_80M=y` | Requerido para rendimiento |
| `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` | Ya en defaults |
| `CONFIG_SPIRAM_RODATA=y` | Ya en defaults |
| `CONFIG_COMPILER_OPTIMIZATION_PERF=y` | Recomendado |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y` | Recomendado |

Mediciones obligatorias para cada hito:

| Medicion | API/comando |
| --- | --- |
| FPS | contador en `DG_DrawFrame()` |
| Heap interna libre/minima | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` |
| PSRAM libre/minima | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` |
| Stack libre | `uxTaskGetStackHighWaterMark()` |
| Tamano firmware | `idf.py size` despues de build |

Riesgos de memoria/build especificos:

| Riesgo | Mitigacion inicial |
| --- | --- |
| `DG_ScreenBuffer` reservado con `malloc()` normal | Parche minimo para `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` o wrapper controlado |
| Zone memory demasiado grande | Empezar con `-mb 4` y subir solo si WAD/engine lo requiere |
| App supera slot `factory` 8 MB | Medir con `idf.py size`; cambiar particiones solo si falla o queda margen insuficiente |
| Buffers DMA en PSRAM | Mantener chunks DMA en `MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL` |
| Warnings masivos de third-party | Arreglar/aislar flags por componente; no usar `-w` global como solucion permanente |

## Particiones

No cambiar `partitions.csv` en la primera PoC si el WAD viene de SD.

Si se decide WAD en flash:

| Paso | Motivo |
| --- | --- |
| Validar flash real con `esptool.py flash_id` | El baseline usa 16 MB, la placa parece 32 MB |
| Decidir OTA/no OTA | Cambia todo el layout |
| Crear particion `wad` raw o FAT/SPIFFS | El motor necesita acceso a WAD |
| Ajustar app slot | DoomGeneric/PrBoom puede crecer bastante |
| Documentar offsets/tamanos | Evitar layouts copiados sin razon |

Layout posible sin OTA en 16 MB, solo si se abandona el SPIFFS actual:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, ,        8M,
wad,      0x42, 0x06,    ,        7M,
```

Esto es solo una idea de analisis, no una decision. Para WADs grandes, SD o flash 32 MB es mejor.

## Licencias Y Assets

Riesgo importante: el motor Doom source, PrBoom y DoomGeneric son GPL. Si se integra el motor en el firmware y se distribuye, hay obligaciones GPL para la parte derivada y potencialmente para el firmware combinado.

Reglas iniciales:

| Regla | Motivo |
| --- | --- |
| No commitear WAD comercial | Assets comerciales no son libres |
| No commitear WAD sin licencia revisada | Evitar deuda legal |
| Tratar DoomGeneric/PrBoom como GPL | Definir compatibilidad del proyecto antes de distribuir |
| Documentar fuente exacta del motor importado | Reproducibilidad y cumplimiento |
| Mantener COPYING/LICENSE del motor | Requisito basico |

Opciones de assets:

| Asset | Uso |
| --- | --- |
| `doom1.wad` shareware | Bueno para pruebas si lo aporta el usuario |
| Freedoom | Alternativa libre para contenido, licencia BSD-like |
| `doom1-cut.wad` de Espressif | Referencia historica; revisar licencia antes de usar/commitear |

## Roadmap De Desarrollo

### Fase 0: Documentacion Y Decisiones

Objetivo: dejar cerrado el alcance antes de tocar codigo.

| Paso | Resultado |
| --- | --- |
| Documentar firmware standalone | Decision cerrada |
| Documentar aceptacion GPL en rama Doom | Decision cerrada |
| Documentar WAD externo por SD | Decision cerrada |
| Documentar no uso de `PWR` en MVP | Decision cerrada |
| Documentar prioridad compile-first | Decision cerrada |

Estado: completado en este documento antes del primer cambio de firmware.

### Fase 1: Build Skeleton

Objetivo: importar el motor y conseguir el primer `idf.py build` OK aunque la funcionalidad este stubeada.

| Paso | Resultado |
| --- | --- |
| Importar `DoomGeneric` como componente separado | Fuentes GPL y licencia presentes |
| Elegir commit/tag exacto de DoomGeneric | URL/commit documentado |
| Crear `components/doom_app` | HAL propia enlazada |
| Implementar `DG_*` minimas | Sin undefined symbols |
| Excluir plataformas no ESP32 (`SDL`, `X11`, `Win32`, etc.) | Build no arrastra dependencias desktop |
| Sustituir demo LVGL por bootstrap standalone | `main/main.c` pequeno |
| Ejecutar build | `idf.py build` OK |

Criterio: compila. No exige todavia display visible ni WAD real si los stubs son necesarios para superar la primera integracion.

### Fase 2: Hardware Init Standalone

Objetivo: inicializar display, SD, touch y `BOOT` sin LVGL.

| Paso | Resultado |
| --- | --- |
| Display con `bsp_display_new()` | Panel inicializado y brillo ajustado |
| SD con `bsp_sdcard_mount()` | `/sdcard` montado |
| Touch con `bsp_touch_new()` | Lectura directa por `esp_lcd_touch_*` |
| `BOOT` GPIO0 input pull-up | Estado activo bajo leido |
| Logs de init claros | Fallos de hardware diagnosticables |

Criterio: build OK y boot log interpretable en hardware.

### Fase 3: Primer Frame Visible

Objetivo: mostrar Doom o, si falla el motor, un patron RGB565 de diagnostico por la misma ruta de display.

| Paso | Resultado |
| --- | --- |
| Reservar framebuffer en PSRAM | Sin agotar SRAM interna |
| Usar framebuffer `rgb565` actual o `CMAP256` posterior | Formato entendido y documentado |
| Convertir/enviar chunks RGB565 | `esp_lcd_panel_draw_bitmap()` funciona |
| Aplicar byte swap | Colores correctos |
| Alinear area SH8601 | Sin corrupcion de refresco |
| Centrar `320 x 240` | Imagen visible en panel portrait |

Criterio: frame visible sin panic. FPS puede ser bajo en esta fase.

### Fase 4: WAD Y Loop Jugable Basico

Objetivo: cargar `/sdcard/doom1.wad`, entrar en el loop y avanzar frames.

| Paso | Resultado |
| --- | --- |
| Validar existencia de WAD | Error claro si falta |
| Arrancar `doomgeneric_Create()` con argv fijo | WAD cargado |
| Ejecutar `doomgeneric_Tick()` | Juego avanza |
| Log FPS/heap/stack cada 5 s | Rendimiento medido |
| Mantener audio off | Sin dependencia I2S/codec |

Criterio: Doom visible y avanzando, aunque input sea limitado.

### Fase 5: Input Minimo Funcional

Objetivo: poder navegar menu o jugar una escena simple con hardware disponible.

| Paso | Resultado |
| --- | --- |
| Touch por zonas | Forward/back/left/right/use/menu basicos |
| `BOOT` como fire/enter | Accion fisica inmediata |
| Eventos edge `keydown/keyup` | Sin repeticion accidental |
| Debounce simple para `BOOT` | Pulsaciones estables |
| UX documentada | Mapping reproducible |

Criterio: entrada usable sin accesorio externo. No se usa `PWR`.

### Fase 6: Rendimiento

Objetivo: mejorar comportamiento general sin cambiar de arquitectura.

| Idea | Beneficio | Riesgo |
| --- | --- | --- |
| Afinar chunk height | Mejor balance CPU/DMA | SRAM interna |
| Doble buffer DMA | Solapar conversion/envio | Complejidad y semaforos |
| Mantener `rgb565` end-to-end | Menos conversion | Depende de DoomGeneric/I_Video |
| Explorar `CMAP256` | Menos memoria | Cambios de paleta/video |
| Escalado `410 x 307` | Mejor uso de pantalla | Mas CPU |
| Landscape software | Mejor UX Doom | Rotacion/coste/input |
| WAD en mmap/particion raw | Menos overhead de FS | Redisenar particiones |

Criterio: FPS, heap y estabilidad mejoran con datos antes/despues.

### Fase 7: Polish Opcional

Objetivo: solo despues de video/input estable.

| Idea | Notas |
| --- | --- |
| SFX mono | ES8311, 11025/22050 Hz |
| Vibracion | GPIO18, validar hardware antes |
| Launcher LVGL futuro | Separar claramente ownership display/touch |
| Saves | Requiere filesystem y soporte motor |
| Seleccion de WAD | Menu o config posterior |

## Preguntas Abiertas

| Pregunta | Estado actual |
| --- | --- |
| GPL en esta rama/producto? | Aceptado para la rama Doom |
| Doom app o firmware dedicado? | Firmware standalone inicial |
| WAD desde SD o flash interna? | SD para MVP; flash interna posterior si hace falta |
| Objetivo de FPS minimo aceptable? | Pendiente de medir en hardware |
| Orientacion final portrait o landscape? | `320 x 240` portrait centrado para MVP; landscape posterior |
| Input principal? | Touch por zonas + `BOOT` para MVP |
| Audio en MVP? | No |

## Decision Inicial De Implementacion

Implementar primero:

| Decision | Valor |
| --- | --- |
| Modo | Firmware standalone |
| Primer exito | `idf.py build` OK |
| Motor | DoomGeneric |
| Assets | WAD aportado por usuario en microSD |
| Render | Directo SH8601/QSPI, sin LVGL |
| Resolucion | `320 x 240` centrado, Doom interno `320 x 200` aspect-correct |
| Audio | Deshabilitado con `-nosound -nomusic` |
| Input | Touch por zonas + `BOOT`; `PWR` excluido |
| Medicion | FPS, heap interna, PSRAM, stack, tamano firmware |

El orden practico queda invertido respecto a una PoC puramente grafica: se permite importar el motor para cerrar compilacion primero. Si el primer runtime no muestra imagen, volver a un patron RGB565 directo por la misma ruta SH8601 antes de depurar el motor.
