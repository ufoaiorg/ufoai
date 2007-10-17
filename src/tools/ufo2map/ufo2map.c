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

#define VERSION "1.1"

#include "qrad.h"
#include "qbsp.h"
#include "../../shared/shared.h"

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/time.h>
#include <sys/resource.h>
#endif

static char outbase[32];
static char source[1024];
static char name[1024];
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
		} else if (!strcmp(argv[i], "-nocsg")) {
			Com_Printf("nocsg = true\n");
			config.nocsg = qtrue;
		} else if (!strcmp(argv[i], "-noshare")) {
			Com_Printf("noshare = true\n");
			config.noshare = qtrue;
		} else if (!strcmp(argv[i], "-notjunc")) {
			Com_Printf("notjunc = true\n");
			config.notjunc = qtrue;
		} else if (!strcmp(argv[i], "-nowater")) {
			Com_Printf("nowater = true\n");
			config.nowater = qtrue;
		} else if (!strcmp(argv[i], "-nice")) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
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
		} else if (!strcmp (argv[i],"-tmpout")) {
			strcpy(outbase, "/tmp");
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
		if (!strcmp(argv[i],"-dump"))
			config.dumppatches = qtrue;
		else if (!strcmp(argv[i],"-bounce")) {
			config.numbounce = atoi(argv[i + 1]);
			Com_Printf("light bounces = %i\n", config.numbounce);
			i++;
		} else if (!strcmp(argv[i],"-extra")) {
			config.extrasamples = qtrue;
			Com_Printf("extrasamples = true\n");
		} else if (!strcmp(argv[i],"-radchop")) {
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
			Com_Printf("noradiosity = true\n");
			config.noradiosity = qtrue;
		}
	}
}

/**
 * @brief Set default values
 */
static void U2M_SetDefaultConfigValues (void)
{
	config.subdivideSize = 2048.0f;
	config.block_xl = -8;
	config.block_xh = 7;
	config.block_yl = -8;
	config.block_yh = 7;
	config.microvolume = 1.0f;
	config.subdiv = 2048.0f;
	config.ambient_red = 0.1;
	config.ambient_green = 0.1;
	config.ambient_blue = 0.1;
	config.maxlight = 196;
	config.lightscale = 1.0;
	config.lightquant = 4;
	config.direct_scale = 0.4f;
	config.entity_scale = 1.0f;
}

int main (int argc, char **argv)
{
	double begin, start, end;
	char out[1024];

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
		" -chop                    : \n"
		" -direct                  : \n"
		" -draw                    : \n"
		" -dump                    : \n"
		" -extra                   : extra light samples\n"
		" -entity                  : \n"
		" -fulldetail              : don't treat details (and trans surfaces) as details\n"
		" -maxlight                : \n"
		" -micro <float>           : \n"
#ifndef _WIN32
		" -nice <prio>             : priority level [normal unix nice level - e.g. 19 = IDLE]\n"
#else
		" -nice <prio>             : priority level [0 = HIGH, 1 = NORMAL, 2 = IDLE]\n"
#endif
		" -nobackclip              : \n"
		" -nocsg                   : \n"
		" -nodetail                : \n"
		" -nofill                  : \n"
		" -nomerge                 : \n"
		" -noprune                 : \n"
		" -nosubdiv                : \n"
		" -noshare                 : \n"
		" -notjunc                 : \n"
		" -nowater                 : \n"
		" -noweld                  : \n"
		" -noradiosity             : don't perform the radiosity calculations\n"
		" -onlyents                : \n"
		" -quant                   : lightquant\n"
		" -radchop                 : \n"
		" -scale                   : lightscale\n"
		" -tmpout                  : \n"
		" -v                       : verbose output\n"
		" -verboseentities         : also be verbose about submodels (entities)\n"
	);

	Swap_Init();

	start = I_FloatTime();

	Com_Printf("path: '%s'\n", argv[argc - 1]);
	SetQdirFromPath(argv[argc - 1]);

	strcpy(name, ExpandArg(argv[argc - 1]));
	COM_StripExtension(name, source, sizeof(source));
	DefaultExtension(name, ".map");

	snprintf(out, sizeof(out), "%s.bsp", source);

	Com_Printf("...map: '%s'\n...bsp: '%s'\n", name, out);

	/* if onlyents just grab the entites and resave */
	if (config.onlyents) {
		LoadBSPFile(out);
		num_entities = 0;

		LoadMapFile(name);
		SetModelNumbers();
		SetLightStyles();

		UnparseEntities();

		WriteBSPFile(out);
	} else {
		/* start from scratch */
		LoadMapFile(name);
		SetModelNumbers();
		SetLightStyles();

		ProcessModels(source);
	}

	end = I_FloatTime();
	Com_Printf("%5.0f seconds elapsed\n", end-start);

	if (!config.onlyents && !config.noradiosity) {
		Com_Printf("----- Radiosity ----\n");

		begin = start;

		start = I_FloatTime();

		CalcTextureReflectivity();

		RadWorld();

		DefaultExtension(source, ".bsp");
		sprintf(name, "%s%s", outbase, source);
		Com_Printf("writing %s\n", name);
		WriteBSPFile(name);

		end = I_FloatTime();

		Com_Printf("%5.0f seconds elapsed\n", end-start);
		Com_Printf("sum: %5.0f seconds elapsed\n\n", end-begin);
	}

	return 0;
}
