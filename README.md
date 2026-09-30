# ESP32Watch-Doom

Port de **Doom** (sobre [doomgeneric](https://github.com/ozkl/doomgeneric)) para la Waveshare **ESP32-S3-Touch-AMOLED-2.06**: render directo al AMOLED en horizontal (502x376), efectos de sonido por el altavoz y controles por tactil y botones.

> **Antes de compilar: el WAD.** Este repo no incluye datos del juego. Necesitas un WAD legal (`doom1.wad` shareware o Freedoom) embebido al compilar o en la microSD. Instrucciones en [wad/README.md](wad/README.md).

## Controles

El reloj se sostiene en horizontal.

| Accion | Control |
| --- | --- |
| Avanzar / retroceder | Tocar zona superior / inferior |
| Girar | Tocar zona izquierda / derecha (franja media) |
| Usar / abrir, aceptar en menu | Tocar el centro |
| Disparar, aceptar en menu, confirmar Si/No | `BOOT` |
| Menu / pausa, responder No | Pulsacion corta de `PWR` |

Sigue la [convencion de botones](https://github.com/Sethyrus/ESP32Watch-core/blob/main/docs/ARCHITECTURE.md#convencion-de-botones) comun (`BOOT` = aceptar, `PWR` = atras/menu). No mantener `PWR` unos 6 s: apaga la placa. `Quit Game` → Si reinicia el reloj.

## Compilar y flashear

Requiere `ESP-IDF 5.5.4` (ver [SETUP](https://github.com/Sethyrus/ESP32Watch-core/blob/main/docs/SETUP.md)).

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor   # p. ej. /dev/tty.usbmodem1101; sin -p lo autodetecta
```

El puerto puede variar. Particiones: la tabla comun de [ESP32Watch-Launcher](https://github.com/Sethyrus/ESP32Watch-Launcher), con `storage` FAT de 16 MB para el WAD embebido. Sin WAD embebido (ninguno en `wad/`, o `CONFIG_DOOM_EMBED_WAD=n` en menuconfig > Doom), el firmware busca el WAD en la SD.

Para tenerlo junto a las demas apps y elegirlo desde un menu de arranque, grabarlo con el launcher (`./flash_all.sh`). "Quit Game" reinicia y vuelve al launcher; en standalone la app ocupa `factory` y reinicia Doom.

## Estado

Funciona: arranque, render, input tactil + BOOT + PWR, SFX, guardado en SD. Musica deshabilitada (`-nomusic`). Estado, medidas y decisiones en [docs/DOOM_PORT.md](docs/DOOM_PORT.md).

## Estructura

| Ruta | Contenido |
| --- | --- |
| `main/main.c` | Arranque. |
| `components/doom_app/` | Capa ESP32: display, input, storage, audio (`doom_port.c`, `i_sound_esp32.c`). |
| `components/doomgeneric/` | Motor doomgeneric vendorizado (solo lo que se compila; ver su README). |
| `wad/` | Donde poner el WAD para embeberlo (no versionado). |
| `docs/DOOM_PORT.md` | Investigacion, arquitectura, estado y referencias. |

Los botones vienen del componente `watch_board` de [ESP32Watch-core](https://github.com/Sethyrus/ESP32Watch-core), donde tambien esta la documentacion de hardware de la placa.

## Licencia

GPL-2.0 (ver [LICENSE](LICENSE)), porque incluye el codigo de Doom/doomgeneric. Los datos del juego (WAD) no forman parte del repo ni de las releases.

Doom es una marca de id Software. Este proyecto no esta afiliado a id Software ni a ZeniMax/Bethesda.
