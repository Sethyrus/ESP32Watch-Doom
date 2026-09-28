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
   idf.py -p /dev/tty.usbmodem1101 flash
   ```

   Al compilar aparece `DOOM WAD Detected: ... Embedding into FATFS partition 'storage'`. CMake genera la imagen FAT de la particion `storage` (12 MB) y `idf.py flash` la graba. El firmware la monta read-only en `/internal`.

Sin SD, las partidas guardadas y la configuracion **no persisten**.

## Opcion 2: en la microSD

Copiar el WAD en la raiz de la tarjeta (FAT32) como `doom.wad` o `doom1.wad`. No hace falta recompilar. Con SD montada, `default.cfg`, `doom.cfg` y las partidas se guardan en `/sdcard/`.

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
