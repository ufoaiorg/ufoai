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

typedef struct slicerConfig_s {
	float thickness;
	int scale;
	qboolean singleContour;
	qboolean multipleContour;
	char filename[MAX_QPATH];
} slicerConfig_t;

static slicerConfig_t config = {
	.thickness = 8.0,
	.scale = 1,
	.singleContour = qtrue,
	.multipleContour = qtrue,
	.filename = ""
};

static void Usage (void)
{
	Com_Printf("Usage: ./ufoslicer -t N -s N -c -m bspfile\n");
	Com_Printf("\n");
	Com_Printf(" -t N      = use slice thickness of N units.\n");
	Com_Printf(" -s N      = use scale factor of N.\n");
	Com_Printf(" -c        = create a single contour map.\n");
	Com_Printf(" -m        = create multiple contour maps.\n");
	Com_Printf(" -h|--help = show this help screen.\n");
	Com_Printf("\n");
	Com_Printf("For example: ./ufoslicer -t 64 -s 8 maps/farm\n");
	Com_Printf("would slice the map \"farm.bsp\", moving by 64 units between slices and\n");
	Com_Printf("would create a 1/8th scale bitmap file.\n");
	Com_Printf("\n");
	Com_Printf("The default for -t is 64 units.\n");

	Mem_Shutdown();

	exit(EXIT_FAILURE);
}

/**
 * @brief Parameter parsing
 */
static void SL_Parameter (int argc, const char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (Q_streq(argv[i], "-t") && (i + 1 < argc)) {
			config.thickness = strtod(argv[++i], NULL);
		} else if (Q_streq(argv[i], "-s") && (i + 1 < argc)) {
			config.scale = atoi(argv[++i]);
		} else if (Q_streq(argv[i], "-c")) {
			config.singleContour = qtrue;
		} else if (Q_streq(argv[i], "-m")) {
			config.multipleContour = qtrue;
		} else if (Q_streq(argv[i], "-h") || Q_streq(argv[i], "--help")) {
			Usage();
		} else {
			if (config.filename[0] == '\0') {
				Q_strncpyz(config.filename, argv[i], sizeof(config.filename));
			} else {
				Com_Printf("Parameters unknown. Try --help.\n");
				Usage();
			}
		}
	}
}

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
		Usage();
	}

	com_genericPool = Mem_CreatePool("slicer");
	com_fileSysPool = Mem_CreatePool("slicer filesys");

	Swap_Init();
	Mem_Init();

	SL_Parameter(argc, argv);

	Com_StripExtension(config.filename, bspFilename, sizeof(bspFilename));
	Com_DefaultExtension(bspFilename, sizeof(bspFilename), ".bsp");

	FS_InitFilesystem(qfalse);
	SL_BSPSlice(LoadBSPFile(bspFilename), config.thickness, config.scale, config.singleContour, config.multipleContour);

	Mem_Shutdown();

	return EXIT_SUCCESS;
}
