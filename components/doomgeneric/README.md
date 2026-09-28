# DoomGeneric Vendor Source

This component vendors DoomGeneric for the standalone Doom firmware.

Source: https://github.com/ozkl/doomgeneric
Imported commit: `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`
License: GPL-2.0, preserved in `vendor/LICENSE`. Upstream credits in `vendor/README.TXT` and `vendor/README.md`.

## Pruned from upstream

Only the sources compiled by `CMakeLists.txt` are kept. Removed because they target other platforms or are never built here:

- Build files: `Makefile*`, `doomgeneric.sln`, `doomgeneric.vcxproj*`, `vendor/.gitignore`.
- Platform backends: `doomgeneric_{allegro,emscripten,linuxvt,sdl,soso,sosox,win,xlib}.c` (the ESP32 backend lives in `components/doom_app/doom_port.c`).
- Desktop audio/music: `i_allegromusic.c`, `i_allegrosound.c`, `i_sdlmusic.c`, `i_sdlsound.c`, `gusconf.c`, `mus2mid.c`/`.h` (SFX backend is `components/doom_app/i_sound_esp32.c`; music is disabled with `-nomusic`).
- Unused: `icon.c`, `d_textur.h`, `doom.h`, `net_packet.h`.

To compare with upstream, check out the imported commit and diff only the files present here.

Local ESP-IDF patches are intentionally small and should stay documented in `docs/DOOM_PORT.md`.
