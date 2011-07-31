/**
 * @file ufoslicer.c
 * @brief Utility to slice a bsp file into a flat 2d plan of the map
 * @note Based on the BSP_tools by botman
 */

#include "../common/bspslicer.h"
#include "../common/mem.h"
#include "../shared/byte.h"
#include "../common/tracing.h"
#include "../tools/ufo2map/common/bspfile.h"

struct memPool_s *com_fileSysPool;
struct memPool_s *com_genericPool;
dMapTile_t *curTile;
mapTiles_t mapTiles;

void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}

int main (int argc, const char **argv)
{
	char bspFilename[MAX_OSPATH];

	if (argc < 2) {
		Com_Printf("please specify the bsp file\n");
		return EXIT_FAILURE;
	}

	com_genericPool = Mem_CreatePool("slicer");
	com_fileSysPool = Mem_CreatePool("slicer filesys");

	Swap_Init();
	Mem_Init();

	Com_StripExtension(argv[1], bspFilename, sizeof(bspFilename));
	Com_DefaultExtension(bspFilename, sizeof(bspFilename), ".bsp");

	FS_InitFilesystem(qfalse);
	SL_BSPSlice(LoadBSPFile(bspFilename), 8.0, 1, qtrue, qtrue);

	Mem_Shutdown();

	return EXIT_SUCCESS;
}
