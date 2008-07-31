/**
 * @file ufo2map.c
 * @brief Starting point for map compiler
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#define VERSION "1.2.2"

#include "radiosity.h"
#include "bsp.h"
#include "check.h"
#include "../../shared/shared.h"

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

mapConfig_t config;

/**
 * @brief print usage information.
 */
static void Usage (void)
{
	Com_Printf("Usage: ufo2map <param1 <subparam1> <subparam2> <...>> <param2> <...> [map]\n"
#ifdef _WIN32
		"Even on Windows, use / slashes in the path\n"
#endif
		"\nGeneral options:\n"
		" -h --help                  : print (this) help and exit\n"
#ifndef _WIN32
		" -nice <prio>               : priority level [unix nice level from -20 to 19 where 19 is the lowest priority]\n"
#else
		" -nice <prio>               : priority level [0 = HIGH, 1 = NORMAL, 2 = IDLE]\n"
#endif
		" -nofootstep                : don't generate a footstep file\n"
		" -onlynewer                 : only proceed when the map is newer than the bsp\n"
		" -v                         : verbose output\n"
		"\nRadiosity options:\n"
		" -bounce <num>              : light bounces\n"
		" -extra                     : extra light samples\n"
		" -maxlight                  : \n"
		" -noradiosity TYPE          : don't perform the radiosity calculations, where TYPE is one of day, night, all\n"
		"                            : default is all\n"
		" -quant                     : lightquant\n"
		" -radchop                   : \n"
		" -scale                     : lightscale\n"
		" -t --threads               : thread amount\n"
		"\nBinary space partitioning (BSPing) options:\n"
		" -block <xl> <yl>           : \n"
		" -blocks <xl> <yl> <xh> <yh>: \n"
		" -chop                      : \n"
		" -direct                    : \n"
		" -dump                      : \n"
		" -entity                    : \n"
		" -fulldetail                : don't treat details (and trans surfaces) as details\n"
		" -info                      : print bsp file info\n"
		" -material                  : generate a material file\n"
		" -micro <float>             : warn if a brush has a volume lower than the specified float.\n"
		" -nobackclip                : draw downward pointing faces. (so actors cannot see up through floors\n"
		"                              in first person view). default is to set SURF_NODRAW to downard faces.\n"
		" -nocsg                     : \n"
		" -nodetail                  : \n"
		" -nofill                    : \n"
		" -nomerge                   : \n"
		" -noprune                   : \n"
		" -nosubdiv                  : \n"
		" -noshare                   : \n"
		" -notjunc                   : \n"
		" -nowater                   : \n"
		" -noweld                    : \n"
		" -onlyents                  : modify existing bsp file with entities from map file\n"
		" -verboseentities           : also be verbose about submodels (entities)\n"
		"\nMapping options:\n"
		"\n These options operate on map file only. No bsp file is created.\n"
		" Output prefixed by an asterisk (*) indicates operations that would change the map file.\n"
		" \n -check                     : check source map, only print information.\n"
		" -fix                       : same subparameters as -check, changes the source map file.\n"
		" \n subparameters for -check and -fix\n"
		"    all                     : includes 'brushes entities'. Performs all checks and fixes. This is the default.\n"
		"    bru brushes             : includes 'lvl tex mfc'. Performs all checks and fixes associated with brushes.\n"
		"    ent entities            : performs all checks and fixes associated with entities.\n"
		"    lvl levelflags          : if no levelflags for a brush or entity are set, all of them are set\n"
		"    ndr nodraws             : assigns SURF_NODRAW to hidden faces and checks for faces that\n"
		"                              may have it incorrectly assigned. ***not working properly, do not included in 'all'.\n"
		"    tex textures            : warns when no texture or error texture is assigned.\n"
		"                              ensures special textures and content/surface flags are consistent.\n"
		"    mfc mixedfacecontents   : ensures the contentflags are the same on each face of each brush.\n"
	);
}

/**
 * @brief Check for bsping, radiosity and checking/fixing command line parameters
 */
static void U2M_Parameter (int argc, const char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			Com_Printf("verbose = true\n");
			config.verbose = qtrue;
		} else if (!strcmp(argv[i], "-noweld")) {
			/* make every point unique */
			Com_Printf("noweld = true\n");
			config.noweld = qtrue;
		} else if (!strcmp(argv[i], "-check") || !strcmp(argv[i], "-fix")) {
			/* check for subparameters terminate loop before last arg (path) or
			 * when we hit a param (as opposed to a subparam).
			 * full parameters are prefixed with "-". */
			const int iInitial = i;

			if (!strcmp(argv[i], "-check")) {
				Com_Printf("check = true\n");
				config.performMapCheck = qtrue;
			}
			if (!strcmp(argv[i], "-fix")) {
				Com_Printf("fix = true\n");
				config.fixMap = qtrue;
			}
			while (++i < (argc - 1) && argv[i][0] != '-') {
				if (!strcmp(argv[i], "entities") || !strcmp(argv[i], "ent")) {
					Com_Printf("  %s entities\n", config.fixMap ? "fixing" : "checking");
					config.chkEntities = qtrue;
				} else if (!strcmp(argv[i], "brushes") || !strcmp(argv[i], "bru")) {
					Com_Printf("  %s brushes\n", config.fixMap ? "fixing" : "checking");
					config.chkBrushes = qtrue;
				} else if (!strcmp(argv[i], "levelflags") || !strcmp(argv[i], "lvl")) {
					Com_Printf("  %s levelflags\n", config.fixMap ? "fixing" : "checking");
					config.chkLevelFlags = qtrue;
				} else if (!strcmp(argv[i], "textures") || !strcmp(argv[i], "tex")) {
					Com_Printf("  %s textures\n", config.fixMap ? "fixing" : "checking");
					config.chkTextures = qtrue;
				} else if (!strcmp(argv[i], "nodraws") || !strcmp(argv[i], "ndr")) {
					Com_Printf("  %s nodraws\n", config.fixMap ? "fixing" : "checking");
					config.chkNodraws = qtrue;
				} else if (!strcmp(argv[i], "mixedfacecontents") || !strcmp(argv[i], "mfc")) {
					Com_Printf("  %s mixedfacecontents\n", config.fixMap ? "fixing" : "checking");
					config.chkMixedFaceContents = qtrue;
				} else if (!strcmp(argv[i], "all")) {
					Com_Printf("  %s all (entites brushes)\n", config.fixMap ? "fixing" : "checking");
					config.chkAll = qtrue;
				} else {
					Com_Printf("  WARNING: %s subparameter not understood:%s  try --help for more info\n", config.fixMap ? "fix" : "check", argv[i]);
				}
			}
			i--;
			/* if no subparams set, assume all */
			if (i == iInitial) {
				Com_Printf("  no %s subparameters set, assuming all\n", config.fixMap ? "fix" : "check");
				config.chkAll = qtrue;
			}
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			Usage();
			exit(0);
		} else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "-threads")) {
			threadstate.numthreads = atoi(argv[++i]);
			Com_Printf("threads: #%i\n", threadstate.numthreads);
			continue;
		} else if (!strcmp(argv[i], "-info")) {
			config.info = qtrue;
		} else if (!strcmp(argv[i], "-nocsg")) {
			Com_Printf("nocsg = true\n");
			config.nocsg = qtrue;
		} else if (!strcmp(argv[i], "-noshare")) {
			Com_Printf("noshare = true\n");
			config.noshare = qtrue;
		} else if (!strcmp(argv[i], "-onlynewer")) {
			Com_Printf("onlynewer = true\n");
			config.onlynewer = qtrue;
		} else if (!strcmp(argv[i], "-notjunc")) {
			Com_Printf("notjunc = true\n");
			config.notjunc = qtrue;
		} else if (!strcmp(argv[i], "-nowater")) {
			Com_Printf("nowater = true\n");
			config.nowater = qtrue;
		} else if (!strcmp(argv[i], "-nice")) {
#ifdef HAVE_SETPRIORITY
			config.nice = atoi(argv[++i]);
			Com_Printf("nice = %i\n", config.nice);
			if (setpriority(PRIO_PROCESS, 0, config.nice))
				Com_Printf("failed to set nice level of %i\n", config.nice);
#elif defined _WIN32
			HANDLE proc = GetCurrentProcess();
			config.nice = atoi(argv[++i]);
			Com_Printf("nice = %i\n", config.nice);
			switch (config.nice) {
			case 0:
				SetPriorityClass(proc, HIGH_PRIORITY_CLASS);
				Com_Printf("Priority changed to HIGH\n");
				break;
			case 1:
				SetPriorityClass(proc, NORMAL_PRIORITY_CLASS);
				Com_Printf("Priority changed to NORMAL\n");
				break;
			default:
				SetPriorityClass(proc, IDLE_PRIORITY_CLASS);
				Com_Printf("Priority changed to IDLE\n");
				break;
			}
			CloseHandle(proc);
#else
			Com_Printf("nice not implemented for this arch\n");
			i++;
#endif
		} else if (!strcmp(argv[i], "-noprune")) {
			Com_Printf("noprune = true\n");
			config.noprune = qtrue;
		} else if (!strcmp(argv[i],"-nofootstep")) {
			config.generateFootstepFile = qfalse;
			Com_Printf("generateFootstepFile = false\n");
		} else if (!strcmp(argv[i],"-material")) {
			config.generateMaterialFile = qtrue;
			Com_Printf("generateMaterialFile = true\n");
		} else if (!strcmp(argv[i], "-nofill")) {
			Com_Printf("nofill = true\n");
			config.nofill = qtrue;
		} else if (!strcmp(argv[i], "-nomerge")) {
			Com_Printf("nomerge = true\n");
			config.nomerge = qtrue;
		} else if (!strcmp(argv[i], "-nosubdiv")) {
			Com_Printf("nosubdiv = true\n");
			config.nosubdiv = qtrue;
		} else if (!strcmp(argv[i], "-nodetail")) {
			Com_Printf("nodetail = true\n");
			config.nodetail = qtrue;
		} else if (!strcmp(argv[i], "-fulldetail")) {
			Com_Printf("fulldetail = true\n");
			config.fulldetail = qtrue;
		} else if (!strcmp(argv[i], "-onlyents")) {
			Com_Printf("onlyents = true\n");
			config.onlyents = qtrue;
		} else if (!strcmp(argv[i], "-micro")) {
			config.microvolume = atof(argv[i + 1]);
			Com_Printf("microvolume = %f\n", config.microvolume);
			i++;
		} else if (!strcmp(argv[i], "-verboseentities")) {
			Com_Printf("verboseentities = true\n");
			config.verboseentities = qtrue;
		} else if (!strcmp(argv[i], "-chop")) {
			config.subdivideSize = atof(argv[i + 1]);
			Com_Printf("subdivide_size = %f\n", config.subdivideSize);
			i++;
		} else if (!strcmp(argv[i], "-block")) {
			config.block_xl = config.block_xh = atoi(argv[i + 1]);
			config.block_yl = config.block_yh = atoi(argv[i + 2]);
			Com_Printf("block: %i,%i\n", config.block_xl, config.block_yl);
			i += 2;
		} else if (!strcmp(argv[i], "-blocks")) {
			config.block_xl = atoi(argv[i + 1]);
			config.block_yl = atoi(argv[i + 2]);
			config.block_xh = atoi(argv[i + 3]);
			config.block_yh = atoi(argv[i + 4]);
			Com_Printf("blocks: %i,%i to %i,%i\n",
				config.block_xl, config.block_yl, config.block_xh, config.block_yh);
			i += 4;
		} else if (!strcmp(argv[i], "-nobackclip")) {
			Com_Printf("nobackclip = true\n");
			config.nobackclip = qtrue;
		} else if (!strcmp(argv[i],"-bounce")) {
			config.numbounce = atoi(argv[i + 1]);
			Com_Printf("light bounces = %i\n", config.numbounce);
			i++;
		} else if (!strcmp(argv[i],"-extra")) {
			config.extrasamples = qtrue;
			Com_Printf("extrasamples = true\n");
		} else if (!strcmp(argv[i],"-radchop")) {
			Com_Printf("radiosity subdivide size = %s\n", argv[i + 1]);
			config.subdiv = atoi(argv[i + 1]);
			i++;
		} else if (!strcmp(argv[i],"-quant")) {
			config.lightquant = (byte)atoi(argv[i + 1]);
			if (config.lightquant < 1 || config.lightquant > 6) {
				config.lightquant = 4;
				Com_Printf("lightquant must be between 1 and 6\n");
			}
			i++;
		} else if (!strcmp(argv[i],"-scale")) {
			config.lightscale = atof(argv[i + 1]);
			i++;
		} else if (!strcmp(argv[i],"-direct")) {
			config.direct_scale *= atof(argv[i + 1]);
			Com_Printf("direct light scaling at %f\n", config.direct_scale);
			i++;
		} else if (!strcmp(argv[i],"-entity")) {
			config.entity_scale *= atof(argv[i + 1]);
			Com_Printf("entity light scaling at %f\n", config.entity_scale);
			i++;
		} else if (!strcmp(argv[i],"-maxlight")) {
			config.maxlight = atof(argv[i + 1]) * 128;
			i++;
		} else if (!strcmp(argv[i], "-noradiosity")) {
			if (argc > i + 1) {
				if (!strcmp(argv[i + 1], "day")) {
					Com_Printf("noradiosity = day\n");
					config.noradiosity = RADIOSITY_NIGHT_ONLY;
					i++;
				} else if (!strcmp(argv[i + 1], "night")) {
					Com_Printf("noradiosity = night\n");
					config.noradiosity = RADIOSITY_DAY_ONLY;
					i++;
				} else {
					Com_Printf("noradiosity = none\n");
					config.noradiosity = RADIOSITY_NONE;
				}
			} else {
				Sys_Error("invalid parameter count\n");
			}
		} else if (i < (argc - 1)) {
			/* Last param is the map path, every other param should have been caught by now. */
			Com_Printf("*** parameter not understood: %s try --help for more info\n", argv[i]);
		}
	}

	if (config.fixMap && config.performMapCheck) {
		Sys_Error("do not specify both -fix and -check\n");
	}

	/* if any check or fix option is active then skip footsteps and materials */
	if (config.performMapCheck || config.fixMap) {
		config.generateFootstepFile = qfalse;
		config.generateMaterialFile = qfalse;
	}
}

/**
 * @brief Set default values
 */
static void U2M_SetDefaultConfigValues (void)
{
	config.subdivideSize = 256.0f; /* bsp subdiv */
	config.block_xl = -8;
	config.block_xh = 7;
	config.block_yl = -8;
	config.block_yh = 7;
	config.microvolume = 1.0f;
	config.subdiv = 256.0f; /* rad chop/subdiv */

	config.night_ambient_red = 0.0;
	config.night_ambient_green = 0.0;
	config.night_ambient_blue = 0.0;
	config.night_sun_intensity = 35;
	config.night_sun_pitch = 15 * torad;
	config.night_sun_yaw = 60 * torad;
	VectorSet(config.night_sun_color, 0.8, 0.8, 1);

	config.day_ambient_red = 0.4;
	config.day_ambient_green = 0.4;
	config.day_ambient_blue = 0.4;
	config.day_sun_intensity = 120;
	config.day_sun_pitch = 30 * torad;
	config.day_sun_yaw = 210 * torad;
	VectorSet(config.day_sun_color, 1, 0.8, 0.8);

	VectorSet(config.night_sun_dir,
		cos(config.night_sun_yaw) * sin(config.night_sun_pitch),
		sin(config.night_sun_yaw) * sin(config.night_sun_pitch),
		cos(config.night_sun_pitch));

	VectorSet(config.day_sun_dir,
		cos(config.day_sun_yaw) * sin(config.day_sun_pitch),
		sin(config.day_sun_yaw) * sin(config.day_sun_pitch),
		cos(config.day_sun_pitch));

	config.maxlight = 196;
	config.lightscale = 1.0;
	config.lightquant = 4;
	config.direct_scale = 0.4f;
	config.entity_scale = 1.0f;

	config.generateFootstepFile = qtrue;
}

static int CheckTimeDiff (const char *map, const char *bsp)
{
#ifdef HAVE_SYS_STAT_H
	struct stat mapStat, bspStat;
	if (stat(map, &mapStat) == -1)
		return 0;
	if (stat(bsp, &bspStat) == -1)
		return 0;
	if (difftime(mapStat.st_mtime, bspStat.st_mtime) < 0)
		return 1;
#else
# ifdef _WIN32
	FILETIME ftCreate, ftAccess, ftWriteMap, ftWriteBsp;
	HANDLE hMapFile, hBspFile;
	/* open the files */
	hMapFile = CreateFile(map, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hMapFile == INVALID_HANDLE_VALUE)
		return 0;
	hBspFile = CreateFile(bsp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hBspFile == INVALID_HANDLE_VALUE)
		return 0;

	if (!GetFileTime(hMapFile, &ftCreate, &ftAccess, &ftWriteMap))
		return 0;

	if (!GetFileTime(hBspFile, &ftCreate, &ftAccess, &ftWriteBsp))
		return 0;

	if (CompareFileTime(&ftWriteMap, &ftWriteBsp) == -1)
		return 1;
# endif
#endif
	/* not up-to-date - recompile */
	return 0;
}

int main (int argc, const char **argv)
{
	char mapFilename[MAX_OSPATH];
	char bspFilename[MAX_OSPATH];
	double begin, start, end;

	memset(&config, 0, sizeof(config));
	/* init thread state */
	memset(&threadstate, 0, sizeof(threadstate_t));

	U2M_SetDefaultConfigValues();

	Com_Printf("---- ufo2map "VERSION" ----\n");

	U2M_Parameter(argc, argv);

	if (argc < 2) {
		Usage();
		Sys_Error("At least provide 1 argument: the map filename.");
	}

	Swap_Init();

	start = time(NULL);

	Com_Printf("path: '%s'\n", argv[argc - 1]);
	FS_Init(argv[argc - 1]);

	COM_StripExtension(COM_ExpandRelativePath(argv[argc - 1]), mapFilename, sizeof(mapFilename));
	strncpy(bspFilename, mapFilename, sizeof(bspFilename) - 1);
	COM_DefaultExtension(mapFilename, sizeof(mapFilename), ".map");
	COM_DefaultExtension(bspFilename, sizeof(bspFilename), ".bsp");

	if (config.info) {
		LoadBSPFile(bspFilename);
		PrintBSPFileSizes();
		return 0;
	}

	Com_Printf("...map: '%s'\n", mapFilename);
	if(!(config.performMapCheck || config.fixMap))
		Com_Printf("...bsp: '%s'\n", bspFilename);

	if (config.onlynewer && CheckTimeDiff(mapFilename, bspFilename)) {
		Com_Printf("bsp file is up-to-date\n");
		return 0;
	}

	/* if onlyents just grab the entites and resave */
	if (config.onlyents) {
		LoadBSPFile(bspFilename);
		num_entities = 0; /* use the map source entities */

		LoadMapFile(mapFilename);
		SetModelNumbers();

		UnparseEntities();

		WriteBSPFile(bspFilename);
	} else if (config.performMapCheck || config.fixMap) {
		Com_Printf("Starting map %s\n", config.fixMap ? "fixes" : "checks");
		LoadMapFile(mapFilename);
		/* level flags must be fixed before mixed face contents, or they swamp the
		 * console with output, as levelflags are contentflags */
		if (config.chkLevelFlags || config.chkBrushes || config.chkAll)
			CheckLevelFlags();
		/* mixed face contents check may remove contentflags. this should be done
		 * before tex check, as tex may replace tex on the basis of contentflags.*/
		if (config.chkMixedFaceContents || config.chkBrushes ||config.chkAll )
			CheckMixedFaceContents();
		if (config.chkTextures || config.chkBrushes || config.chkAll)
			CheckTextures();
		if (config.chkBrushes || config.chkAll)
			CheckBrushes();
		if (config.chkNodraws /* || config.chkAll */) /** @todo include in chkAll when it works */
			CheckNodraws();
		if (config.chkEntities || config.chkAll)
			CheckEntities();
		if (config.fixMap) {
			/* update dentdata */
			UnparseEntities();
			WriteMapFile(GetScriptFile());
		}
		return 0;
	} else {
		/* start from scratch */
		LoadMapFile(mapFilename);
		SetModelNumbers();

		ProcessModels(bspFilename);
	}

	end = time(NULL);
	Com_Printf("%5.0f seconds elapsed\n", end - start);

	if (!config.onlyents && config.noradiosity != RADIOSITY_NONE) {
		long size;

		Com_Printf("----- Radiosity ----\n");

		begin = start;

		CalcTextureReflectivity();

		if (config.noradiosity != RADIOSITY_DAY_ONLY) {
			/* compile night version */
			start = time(NULL);
			RadWorld();
			end = time(NULL);
			Com_Printf("%5.0f seconds elapsed\n", end - start);
		}

		if (config.noradiosity != RADIOSITY_NIGHT_ONLY) {
			/* compile day version */
			config.compile_for_day = 1;
			start = time(NULL);
			RadWorld();
			end = time(NULL);
			Com_Printf("%5.0f seconds elapsed\n", end - start);
		}

		Com_Printf("writing %s\n", bspFilename);
		size = WriteBSPFile(bspFilename);

		Com_Printf("sum: %5.0f seconds elapsed - %.1g MB (%li bytes)\n\n", end - begin, (float)size / (1024.0f * 1024.0f), size);
	}

	return 0;
}
