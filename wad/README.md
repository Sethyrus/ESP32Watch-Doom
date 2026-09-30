# WAD (datos del juego)

El firmware incluye el motor, **no los datos del juego**. Hace falta un WAD que aportas tu. Este repo nunca incluye WADs: esta carpeta esta en `.gitignore` salvo este README.

## Donde conseguir un WAD legal

| WAD | Licencia | Notas |
| --- | --- | --- |
| `doom1.wad` (shareware) | Redistribuible sin modificar | Episodio 1 de Doom. Buscar "doom1.wad shareware" o sacarlo de la version shareware oficial. |
| [Freedoom](https://freedoom.github.io/) (`freedoom1.wad`, `freedoom2.wad`) | BSD-like, libre | Renombrar a `doom.wad` o `doom1.wad` (ver nombres abajo). Compatibilidad con este port pendiente de validar. |
| `doom.wad` / `doom2.wad` comerciales | Propietaria | Solo si tienes el juego (Steam/GOG). No redistribuir. |

## Opcion 1: embebido en el firmware (sin SD)

1. Copiar el WAD en esta carpeta con uno de estos nombres:
   - `wad/doom.wad` (prioridad)
   - `wad/doom1.wad`
2. Compilar y flashear normalmente:

   ```sh
   idf.py build
   idf.py -p <PORT> flash
   ```

   Al compilar aparece `DOOM WAD Detected: ... Embedding into FATFS partition 'storage'`. CMake genera la imagen FAT de la particion `storage` (16 MB) y `idf.py flash` la graba. El firmware la monta read-only en `/internal`.

   - El WAD se detecta al configurar CMake: si lo anades despues del primer build, ejecutar `idf.py reconfigure` antes de compilar.
   - Para no embeberlo aunque este en esta carpeta (y usar la SD), desactivar `idf.py menuconfig` > *Doom* > *Embed the WAD from wad/* (`CONFIG_DOOM_EMBED_WAD`). El valor queda en tu `sdkconfig` local.
   - Con [ESP32Watch-Launcher](https://github.com/Sethyrus/ESP32Watch-Launcher), `./flash_all.sh` graba la imagen solo si este build la genero.
   - `idf.py flash` vuelve a grabar la imagen de 16 MB cada vez. Si solo cambia el codigo, `idf.py -p <PORT> app-flash` es mucho mas rapido.

Sin SD no se pueden guardar partidas (el juego muestra `SAVE FAILED: NO SD` y sigue).

## Opcion 2: en la microSD

Copiar el WAD en la raiz de la tarjeta (FAT32) como `doom.wad` o `doom1.wad`. No hace falta recompilar. Con SD montada, las partidas se guardan en `/sdcard/savegame/`. La configuracion (volumen, tamano de pantalla...) no persiste entre arranques.

Se pueden combinar: WAD embebido para jugar y SD para guardar.

## Orden de busqueda

```text
/internal/doom.wad
/internal/doom1.wad
/sdcard/doom.wad
/sdcard/doom1.wad
```

Se usa el primero que exista. Si no hay ninguno, la pantalla muestra barras de color, el log dice `No WAD found` y la tarea de Doom queda suspendida.

Detalles: [docs/DOOM_PORT.md](../docs/DOOM_PORT.md), seccion "Storage Y WAD".
