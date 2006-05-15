#include "qrad.h"
#include "qbsp.h"

char source[1024];
char name[1024];

void Check_BSP_Parameter ( int argc, char **argv )
{
	int i;
	for (i=1 ; i<argc ; i++)
	{
		if (!strcmp(argv[i],"-threads"))
		{
			numthreads = atoi (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-glview"))
		{
			glview = qtrue;
		}
		else if (!strcmp(argv[i], "-v"))
		{
			printf ("verbose = true\n");
			verbose = qtrue;
		}
		else if (!strcmp(argv[i], "-draw"))
		{
			printf ("drawflag = true\n");
			drawflag = qtrue;
		}
		else if (!strcmp(argv[i], "-noweld"))
		{
			printf ("noweld = true\n");
			noweld = qtrue;
		}
		else if (!strcmp(argv[i], "-nocsg"))
		{
			printf ("nocsg = true\n");
			nocsg = qtrue;
		}
		else if (!strcmp(argv[i], "-noshare"))
		{
			printf ("noshare = true\n");
			noshare = qtrue;
		}
		else if (!strcmp(argv[i], "-notjunc"))
		{
			printf ("notjunc = true\n");
			notjunc = qtrue;
		}
		else if (!strcmp(argv[i], "-nowater"))
		{
			printf ("nowater = true\n");
			nowater = qtrue;
		}
		else if (!strcmp(argv[i], "-noopt"))
		{
			printf ("noopt = true\n");
			noopt = qtrue;
		}
		else if (!strcmp(argv[i], "-noprune"))
		{
			printf ("noprune = true\n");
			noprune = qtrue;
		}
		else if (!strcmp(argv[i], "-nofill"))
		{
			printf ("nofill = true\n");
			nofill = qtrue;
		}
		else if (!strcmp(argv[i], "-nomerge"))
		{
			printf ("nomerge = true\n");
			nomerge = qtrue;
		}
		else if (!strcmp(argv[i], "-nosubdiv"))
		{
			printf ("nosubdiv = true\n");
			nosubdiv = qtrue;
		}
		else if (!strcmp(argv[i], "-nodetail"))
		{
			printf ("nodetail = true\n");
			nodetail = qtrue;
		}
		else if (!strcmp(argv[i], "-fulldetail"))
		{
			printf ("fulldetail = true\n");
			fulldetail = qtrue;
		}
		else if (!strcmp(argv[i], "-onlyents"))
		{
			printf ("onlyents = true\n");
			onlyents = qtrue;
		}
		else if (!strcmp(argv[i], "-micro"))
		{
			microvolume = atof(argv[i+1]);
			printf ("microvolume = %f\n", microvolume);
			i++;
		}
		else if (!strcmp(argv[i], "-leaktest"))
		{
			printf ("leaktest = true\n");
			leaktest = qtrue;
		}
		else if (!strcmp(argv[i], "-verboseentities"))
		{
			printf ("verboseentities = true\n");
			verboseentities = qtrue;
		}
		else if (!strcmp(argv[i], "-chop"))
		{
			subdivide_size = atof(argv[i+1]);
			printf ("subdivide_size = %f\n", subdivide_size);
			i++;
		}
		else if (!strcmp(argv[i], "-block"))
		{
			block_xl = block_xh = atoi(argv[i+1]);
			block_yl = block_yh = atoi(argv[i+2]);
			printf ("block: %i,%i\n", block_xl, block_yl);
			i+=2;
		}
		else if (!strcmp(argv[i], "-blocks"))
		{
			block_xl = atoi(argv[i+1]);
			block_yl = atoi(argv[i+2]);
			block_xh = atoi(argv[i+3]);
			block_yh = atoi(argv[i+4]);
			printf ("blocks: %i,%i to %i,%i\n", 
				block_xl, block_yl, block_xh, block_yh);
			i+=4;
		}
		else if (!strcmp (argv[i],"-tmpout"))
		{
			strcpy (outbase, "/tmp");
		}
		else if (!strcmp (argv[i],"-norouting"))
		{
			printf ("norouting = true\n");
			norouting = qtrue;
		}
		else if (!strcmp(argv[i], "-nobackclip"))
		{
			printf ("nobackclip = true\n");
			nobackclip = qtrue;
		}
		else if (argv[i][0] == '-')
			Error ("Unknown option \"%s\"", argv[i]);
		else
			break;
	}
}


void Check_RAD_Parameter( int argc, char** argv )
{
	int i;
	for (i=1 ; i<argc ; i++)
	{
		if (!strcmp(argv[i],"-dump"))
			dumppatches = qtrue;
		else if (!strcmp(argv[i],"-bounce"))
		{
			numbounce = atoi (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-v"))
		{
			verbose = qtrue;
		}
		else if (!strcmp(argv[i],"-extra"))
		{
			extrasamples = qtrue;
			printf ("extrasamples = true\n");
		}
		else if (!strcmp(argv[i],"-threads"))
		{
			numthreads = atoi (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-chop"))
		{
			subdiv = atoi (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-quant"))
		{
			lightquant = atoi (argv[i+1]);
			if ( lightquant < 1 || lightquant > 6 ) lightquant = 3;
			i++;
		}
		else if (!strcmp(argv[i],"-scale"))
		{
			lightscale = atof (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-direct"))
		{
			direct_scale *= atof(argv[i+1]);
			printf ("direct light scaling at %f\n", direct_scale);
			i++;
		}
		else if (!strcmp(argv[i],"-entity"))
		{
			entity_scale *= atof(argv[i+1]);
			printf ("entity light scaling at %f\n", entity_scale);
			i++;
		}
		else if (!strcmp(argv[i],"-glview"))
		{
			glview = qtrue;
			printf ("glview = true\n");
		}
		else if (!strcmp(argv[i],"-nopvs"))
		{
			nopvs = qtrue;
			printf ("nopvs = true\n");
		}
		else if (!strcmp(argv[i],"-maxlight"))
		{
			maxlight = atof (argv[i+1]) * 128;
			i++;
		}
		else if (!strcmp (argv[i],"-tmpin"))
			strcpy (inbase, "/tmp");
		else if (!strcmp (argv[i],"-tmpout"))
			strcpy (outbase, "/tmp");
		else
			break;
	}
}

/*
============
main
============
*/
int main (int argc, char **argv)
{
	double		begin, start, end;
	char		path[1024];

	printf ("---- ufo2map ----\n");

	Check_BSP_Parameter( argc, argv );
	Check_RAD_Parameter( argc, argv );

	if (argc < 2)
		Error ("usage: ufo2map [-threads num] [-glview] [-v] [-draw] [-noweld] [-nocsg] [-noshare] [-notjunc] [-nowater] [-noopt] [-noprune] [-nofill] [-nomerge] [-nosubdiv] [-nodetail] [-fulldetail] [-onlyents] [-micro float] [-leaktest] [-verboseentities] [-chop] [-block num num] [-blocks num num num num] [-tmpout] [-norouting] [-nobackclip] mapfile");

	start = I_FloatTime ();

	ThreadSetDefault ();
	// numthreads = 1;		// multiple threads aren't helping...
	printf("argc: %i\n", argc);
	SetQdirFromPath (argv[argc-1]);

	strcpy (source, ExpandArg (argv[argc-1]));
	StripExtension (source);

	// delete portal and line files
	sprintf (path, "%s.prt", source);
	remove (path);
	sprintf (path, "%s.lin", source);
	remove (path);

	strcpy (name, ExpandArg (argv[argc-1]));	
	DefaultExtension (name, ".map");	// might be .reg

	//
	// if onlyents, just grab the entites and resave
	//
	if (onlyents)
	{
		char out[1024];

		sprintf (out, "%s.bsp", source);
		LoadBSPFile (out);
		num_entities = 0;

		LoadMapFile (name);
		SetModelNumbers ();
		SetLightStyles ();

		UnparseEntities ();

		WriteBSPFile (out);
	}
	else
	{
		//
		// start from scratch
		//
		LoadMapFile (name);
		SetModelNumbers ();
		SetLightStyles ();

		ProcessModels ();
	}

	end = I_FloatTime ();
	printf ("%5.0f seconds elapsed\n", end-start);

	printf ("----- Radiosity ----\n");

	begin = start;

	start = I_FloatTime ();
	DefaultExtension (source, ".bsp");

//	ReadLightFile ();

	sprintf (name, "%s%s", inbase, source);
	printf ("reading %s\n", name);
	LoadBSPFile (name);
	ParseEntities ();
	CalcTextureReflectivity ();

	RadWorld ();

	sprintf (name, "%s%s", outbase, source);
	printf ("writing %s\n", name);
	WriteBSPFile (name);

	end = I_FloatTime ();

	printf ("%5.0f seconds elapsed\n", end-start);
	printf ("sum: %5.0f seconds elapsed\n", end-begin);

	return 0;
}
