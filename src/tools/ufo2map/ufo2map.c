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

#define VERSION "1.0"

#include "qrad.h"
#include "qbsp.h"

#if defined(__linux__) || defined(__NetBSD__) || defined(__FreeBSD__)
#include <sys/time.h>
#include <sys/resource.h>
#endif

char source[1024];
char name[1024];

/**< convert function pointer */
int (*convertFunc) (char *);

/**< BSP2ASE Convert prototype */
int ConvertBSPToASE (char *bspName);

/**
 * @brief Check for bsping command line parameters
 * @note Some are also used for radiosity (e.g. threads)
 */
static void Check_BSP_Parameter ( int argc, char **argv )
{
	int i;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__)
	int nice = 0;
#endif
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i],"-threads")) {
			numthreads = atoi (argv[i+1]);
			i++;
		} else if (!strcmp(argv[i],"-glview"))
			glview = qtrue;
		else if (!strcmp(argv[i], "-v")) {
			Sys_Printf ("verbose = true\n");
			verbose = qtrue;
		} else if (!strcmp(argv[i], "-draw")) {
			Sys_Printf ("drawflag = true\n");
			drawflag = qtrue;
		} else if (!strcmp(argv[i], "-convert")) {
			Sys_Printf ("convert bsp to ase = true\n");
			convertFunc = ConvertBSPToASE;
		} else if (!strcmp(argv[i], "-noweld")) {
			Sys_Printf ("noweld = true\n");
			noweld = qtrue;
		} else if (!strcmp(argv[i], "-nocsg")) {
			Sys_Printf ("nocsg = true\n");
			nocsg = qtrue;
		} else if (!strcmp(argv[i], "-noshare")) {
			Sys_Printf ("noshare = true\n");
			noshare = qtrue;
		} else if (!strcmp(argv[i], "-notjunc")) {
			Sys_Printf ("notjunc = true\n");
			notjunc = qtrue;
		} else if (!strcmp(argv[i], "-nowater")) {
			Sys_Printf ("nowater = true\n");
			nowater = qtrue;
		} else if (!strcmp(argv[i], "-nice")) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__)
			nice = atoi(argv[i+1]);
			Sys_Printf ("nice = %i\n", nice);
			if (setpriority(PRIO_PROCESS, 0, nice))
				Sys_Printf("failed to set nice level of %i\n", nice);
#else
			Sys_Printf ("nice not implemented for this arch\n");
#endif
			i++;
		} else if (!strcmp(argv[i], "-noopt")) {
			Sys_Printf ("noopt = true\n");
			noopt = qtrue;
		} else if (!strcmp(argv[i], "-noprune")) {
			Sys_Printf ("noprune = true\n");
			noprune = qtrue;
		} else if (!strcmp(argv[i], "-nofill")) {
			Sys_Printf ("nofill = true\n");
			nofill = qtrue;
		} else if (!strcmp(argv[i], "-nomerge")) {
			Sys_Printf ("nomerge = true\n");
			nomerge = qtrue;
		} else if (!strcmp(argv[i], "-nosubdiv")) {
			Sys_Printf ("nosubdiv = true\n");
			nosubdiv = qtrue;
		} else if (!strcmp(argv[i], "-nodetail")) {
			Sys_Printf ("nodetail = true\n");
			nodetail = qtrue;
		} else if (!strcmp(argv[i], "-fulldetail")) {
			Sys_Printf ("fulldetail = true\n");
			fulldetail = qtrue;
		} else if (!strcmp(argv[i], "-onlyents")) {
			Sys_Printf ("onlyents = true\n");
			onlyents = qtrue;
		} else if (!strcmp(argv[i], "-micro")) {
			microvolume = atof(argv[i+1]);
			Sys_Printf ("microvolume = %f\n", microvolume);
			i++;
		} else if (!strcmp(argv[i], "-verboseentities")) {
			Sys_Printf ("verboseentities = true\n");
			verboseentities = qtrue;
		} else if (!strcmp(argv[i], "-chop")) {
			subdivide_size = atof(argv[i+1]);
			Sys_Printf ("subdivide_size = %f\n", subdivide_size);
			i++;
		} else if (!strcmp(argv[i], "-block")) {
			block_xl = block_xh = atoi(argv[i+1]);
			block_yl = block_yh = atoi(argv[i+2]);
			Sys_Printf ("block: %i,%i\n", block_xl, block_yl);
			i+=2;
		} else if (!strcmp(argv[i], "-blocks")) {
			block_xl = atoi(argv[i+1]);
			block_yl = atoi(argv[i+2]);
			block_xh = atoi(argv[i+3]);
			block_yh = atoi(argv[i+4]);
			Sys_Printf ("blocks: %i,%i to %i,%i\n",
				block_xl, block_yl, block_xh, block_yh);
			i+=4;
		} else if (!strcmp (argv[i],"-tmpout"))
			strcpy (outbase, "/tmp");
		else if (!strcmp (argv[i],"-norouting")) {
			Sys_Printf ("norouting = true\n");
			norouting = qtrue;
		} else if (!strcmp(argv[i], "-nobackclip")) {
			Sys_Printf ("nobackclip = true\n");
			nobackclip = qtrue;
		}
	}
}

/**
 * @brief Check for radiosity command line parameters
 */
static void Check_RAD_Parameter( int argc, char** argv )
{
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i],"-dump"))
			dumppatches = qtrue;
		else if (!strcmp(argv[i],"-bounce")) {
			numbounce = atoi (argv[i+1]);
			Sys_Printf ("light bounces = %i\n", numbounce);
			i++;
		} else if (!strcmp(argv[i],"-extra")) {
			extrasamples = qtrue;
			Sys_Printf ("extrasamples = true\n");
		} else if (!strcmp(argv[i],"-chop")) {
			subdiv = atoi (argv[i+1]);
			i++;
		} else if (!strcmp(argv[i],"-quant")) {
			lightquant = atoi (argv[i+1]);
			if ( lightquant < 1 || lightquant > 6 )
				lightquant = 3;
			i++;
		} else if (!strcmp(argv[i],"-scale")) {
			lightscale = atof (argv[i+1]);
			i++;
		} else if (!strcmp(argv[i],"-direct")) {
			direct_scale *= atof(argv[i+1]);
			Sys_Printf ("direct light scaling at %f\n", direct_scale);
			i++;
		} else if (!strcmp(argv[i],"-entity")) {
			entity_scale *= atof(argv[i+1]);
			Sys_Printf ("entity light scaling at %f\n", entity_scale);
			i++;
		} else if (!strcmp(argv[i],"-nopvs")) {
			nopvs = qtrue;
			Sys_Printf ("nopvs = true\n");
		} else if (!strcmp(argv[i],"-maxlight")) {
			maxlight = atof (argv[i+1]) * 128;
			i++;
		} else if (!strcmp(argv[i], "-noradiosity")) {
			Sys_Printf ("noradiosity = true\n");
			noradiosity = qtrue;
		}
	}
}

/**
 * @brief
 */
int main (int argc, char **argv)
{
	double begin, start, end;
	char path[1024];
	char out[1024];

	convertFunc = NULL;

	Sys_Printf ("---- ufo2map %s ----\n", VERSION);

	Check_BSP_Parameter( argc, argv );
	Check_RAD_Parameter( argc, argv );

	if (argc < 2)
		Error ("usage: ufo2map [-threads num] [-convert] [-glview] [-nice prio] [-v] [-draw] [-noweld] [-nocsg] [-noshare] [-notjunc] [-nowater] [-noopt] [-noprune] [-nofill] [-nomerge] [-nosubdiv] [-nodetail] [-fulldetail] [-onlyents] [-micro float] [-verboseentities] [-chop] [-block num num] [-blocks num num num num] [-tmpout] [-norouting] [-nobackclip] [-extra] [-noradiosity] mapfile");

	start = I_FloatTime ();

	ThreadSetDefault ();
	/* numthreads = 1;		// multiple threads aren't helping... */
	SetQdirFromPath (argv[argc-1]);

	strcpy (source, ExpandArg (argv[argc-1]));
	StripExtension (source);

	/* delete portal and line files */
	sprintf (path, "%s.prt", source);
	remove (path);
	sprintf (path, "%s.lin", source);
	remove (path);

	strcpy (name, ExpandArg (argv[argc-1]));
	DefaultExtension (name, ".map");

	sprintf (out, "%s.bsp", source);

	/* if onlyents, just grab the entites and resave */
	if (onlyents) {
		LoadBSPFile (out);
		num_entities = 0;

		LoadMapFile (name);
		SetModelNumbers ();
		SetLightStyles ();

		UnparseEntities ();

		WriteBSPFile (out);
	} else if (convertFunc) {
		convertFunc(source);
	} else {
		/* start from scratch */
		LoadMapFile (name);
		SetModelNumbers ();
		SetLightStyles ();

		ProcessModels ();
	}

	end = I_FloatTime ();
	Sys_Printf ("%5.0f seconds elapsed\n", end-start);

	if (!convertFunc && !onlyents && !noradiosity) {
		Sys_Printf ("----- Radiosity ----\n");

		begin = start;

		start = I_FloatTime ();
		DefaultExtension (source, ".bsp");

/*		ReadLightFile (); */

		sprintf (name, "%s%s", inbase, source);
		CalcTextureReflectivity ();

		RadWorld ();

		sprintf (name, "%s%s", outbase, source);
		Sys_Printf ("writing %s\n", name);
		WriteBSPFile (name);

		end = I_FloatTime ();

		Sys_Printf ("%5.0f seconds elapsed\n", end-start);
		Sys_Printf ("sum: %5.0f seconds elapsed\n\n", end-begin);
	}

	return 0;
}
