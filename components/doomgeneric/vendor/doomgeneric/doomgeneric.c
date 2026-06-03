#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

	size_t framebuffer_size = DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4;

#ifdef ESP_PLATFORM
	DG_ScreenBuffer = heap_caps_malloc(framebuffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (DG_ScreenBuffer == NULL) {
		DG_ScreenBuffer = heap_caps_malloc(framebuffer_size, MALLOC_CAP_8BIT);
	}
#else
	DG_ScreenBuffer = malloc(framebuffer_size);
#endif

	if (DG_ScreenBuffer != NULL) {
		memset(DG_ScreenBuffer, 0, framebuffer_size);
	}

	DG_Init();

	D_DoomMain ();
}
