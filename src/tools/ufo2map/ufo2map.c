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

#define VERSION "1.2.1"

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
 * @brief Check for bsping command line parameters
 * @note Some are also used for radiosity
 */
static void U2M_BSP_Parameter (int argc, char **argv)
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
		} else if (!strcmp(argv[i], "-check")) {
			Com_Printf("check = true\n");
			config.performMapCheck = qtrue;
			config.generateFootstepFile = qfalse;
			config.generateMaterialFile = qfalse;
		} else if (!strcmp(argv[i], "-fix")) {
			Com_Printf("fix = true\n");
			config.fixMap = qtrue;
			config.generateFootstepFile = qfalse;
			config.generateMaterialFile = qfalse;
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
			config.nice = atoi(argv[i+1]);
			Com_Printf("nice = %i\n", config.nice);
			if (setpriority(PRIO_PROCESS, 0, config.nice))
				Com_Printf("failed to set nice level of %i\n", config.nice);
#elif defined _WIN32
			HANDLE proc = GetCurrentProcess();
			config.nice = atoi(argv[i+1]);
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
#endif
			i++;
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
		}
	}
}

/**
 * @brief Check for radiosity command line parameters
 */
static void U2M_RAD_Parameter (int argc, char** argv)
{
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i],"-bounce")) {
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
		}
	}
}

/**
 * @brief Set default values
 */
static void U2M_SetDefaultConfigValues (void)
{
	config.subdivideSize = 1024.0f; /* bsp subdiv */
	config.block_xl = -8;
	config.block_xh = 7;
	config.block_yl = -8;
	config.block_yh = 7;
	config.microvolume = 1.0f;
	config.subdiv = 1024.0f; /* rad chop/subdiv */

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

int main (int argc, char **argv)
{
	char mapFilename[MAX_OSPATH];
	char bspFilename[MAX_OSPATH];
	double begin, start, end;

	memset(&config, 0, sizeof(config));
	U2M_SetDefaultConfigValues();

	Com_Printf("---- ufo2map %s ----\n", VERSION);

	U2M_BSP_Parameter(argc, argv);
	U2M_RAD_Parameter(argc, argv);

	if (argc < 2)
		Sys_Error("Usage: ufo2map <parameter> [map]\n"
		" -bounce <num>            : light bounces\n"
		" -block num num           : \n"
		" -blocks num num num num  : \n"
		" -check                   : Performs a mapfile (source) check\n"
		" -chop                    : \n"
		" -direct                  : \n"
		" -draw                    : \n"
		" -dump                    : \n"
		" -extra                   : extra light samples\n"
		" -entity                  : \n"
		" -fulldetail              : don't treat details (and trans surfaces) as details\n"
		" -info                    : print bsp file info\n"
		" -maxlight                : \n"
		" -micro <float>           : \n"
#ifndef _WIN32
		" -nice <prio>             : priority level [normal unix nice level - e.g. 19 = IDLE]\n"
#else
		" -nice <prio>             : priority level [0 = HIGH, 1 = NORMAL, 2 = IDLE]\n"
#endif
		" -nobackclip              : remove brushes that are not visible from birds view\n"
		" -nocsg                   : \n"
		" -nodetail                : \n"
		" -nofill                  : \n"
		" -nomerge                 : \n"
		" -noprune                 : \n"
		" -nosubdiv                : \n"
		" -noshare                 : \n"
		" -nofootstep              : don't generate a footstep file\n"
		" -material                : generate a material file\n"
		" -notjunc                 : \n"
		" -nowater                 : \n"
		" -noweld                  : \n"
		" -noradiosity             : don't perform the radiosity calculations (day, night, all)\n"
		" -onlyents                : only update entities\n"
		" -quant                   : lightquant\n"
		" -radchop                 : \n"
		" -scale                   : lightscale\n"
		" -v                       : verbose output\n"
		" -verboseentities         : also be verbose about submodels (entities)\n"
	);

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

	Com_Printf("...map: '%s'\n...bsp: '%s'\n", mapFilename, bspFilename);

	if (config.onlynewer && CheckTimeDiff(mapFilename, bspFilename)) {
		Com_Printf("bsp file is up-to-date\n");
		return 0;
	}

	/* if onlyents just grab the entites and resave */
	if (config.onlyents) {
		LoadBSPFile(bspFilename);
		num_entities = 0;

		LoadMapFile(mapFilename);
		SetModelNumbers();

		UnparseEntities();

		WriteBSPFile(bspFilename);
	} else if (config.performMapCheck) {
		Com_Printf("Starting map check\n");
		LoadMapFile(mapFilename);
		CheckBrushes();
		CheckEntities();
		WriteMapFile(mapFilename);
		return 0;
	} else if (config.fixMap) {
		Com_Printf("Trying to fix the map\n");
		LoadMapFile(mapFilename);
		FixErrors();
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
		WriteBSPFile(bspFilename);

		Com_Printf("sum: %5.0f seconds elapsed\n\n", end - begin);
	}

	return 0;
}
