// csg4.c

#include "qbsp.h"

extern	float subdivide_size;

char		source[1024];
char		name[1024];

vec_t		microvolume = 1.0;
qboolean	noprune;
qboolean	glview;
qboolean	nodetail;
qboolean	fulldetail;
qboolean	onlyents;
qboolean	nomerge;
qboolean	nowater;
qboolean	nofill;
qboolean	nocsg;
qboolean	noweld;
qboolean	noshare;
qboolean	nosubdiv;
qboolean	notjunc;
qboolean	noopt;
qboolean	leaktest;
qboolean	verboseentities;
qboolean	norouting;
qboolean	nobackclip;

char		outbase[32];
int			block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;
int			entity_num;
vec3_t		g_mins, g_maxs;
node_t		*block_nodes[18][18];
int			level;



/*
============
ProcessWorldModel

============
*/
void ProcessWorldModel (void)
{
	entity_t	*e;
	int			i;

//	BeginModel ();

	e = &entities[entity_num];

	brush_start = e->firstbrush;
	brush_end = brush_start + e->numbrushes;

	ClearBounds( worldMins, worldMaxs );

	// process levels
	{
		int	start, end;
		nummodels = 258;

		start = I_FloatTime();
		for ( i = 0; i < 258; i++ )
		{
			if ( !(i%26) )
				printf ("%i...", (int)(i/26) );

			// process brushes with that level mask
			ProcessLevel( i );
		}
		end = I_FloatTime();

		printf( " (%i)\n", end-start );
	}

	// calculate routing
	if ( !norouting ) 
		DoRouting();
}


/*
============
ProcessSubModel

============
*/
void ProcessSubModel (void)
{
	entity_t	*e;
	int			start, end;
	tree_t		*tree;
	bspbrush_t	*list;
	vec3_t		mins, maxs;

	BeginModel ();

	e = &entities[entity_num];

	start = e->firstbrush;
	end = start + e->numbrushes;

	if ( !strcmp ("func_detail", ValueForKey (e, "classname") ) )
	{
	}

	mins[0] = mins[1] = mins[2] = -4096;
	maxs[0] = maxs[1] = maxs[2] = 4096;
	list = MakeBspBrushList (start, end, -1, mins, maxs);
	if (!nocsg)
		list = ChopBrushes (list);
	tree = BrushBSP (list, mins, maxs);
	MakeTreePortals (tree);
	MarkVisibleSides (tree, start, end);
	MakeFaces (tree->headnode);
	FixTjuncs (tree->headnode);
	WriteBSP (tree->headnode);
	FreeTree (tree);

	EndModel ();
}


/*
============
ProcessModels
============
*/
void ProcessModels (void)
{
	BeginBSPFile ();

	for (entity_num=0 ; entity_num< num_entities ; entity_num++)
	{
		if (!entities[entity_num].numbrushes)
			continue;

		qprintf ("############### model %i ###############\n", nummodels);
		if (entity_num == 0)
			ProcessWorldModel ();
		else
			ProcessSubModel ();

		if (!verboseentities)
			verbose = false;	// don't bother printing submodels
	}

	EndBSPFile ();
}


/*
============
main
============
*/
int main (int argc, char **argv)
{
	int		i;
	double		start, end;
	char		path[1024];

	printf ("---- qbsp3 ----\n");

	for (i=1 ; i<argc ; i++)
	{
		if (!strcmp(argv[i],"-threads"))
		{
			numthreads = atoi (argv[i+1]);
			i++;
		}
		else if (!strcmp(argv[i],"-glview"))
		{
			glview = true;
		}
		else if (!strcmp(argv[i], "-v"))
		{
			printf ("verbose = true\n");
			verbose = true;
		}
		else if (!strcmp(argv[i], "-draw"))
		{
			printf ("drawflag = true\n");
			drawflag = true;
		}
		else if (!strcmp(argv[i], "-noweld"))
		{
			printf ("noweld = true\n");
			noweld = true;
		}
		else if (!strcmp(argv[i], "-nocsg"))
		{
			printf ("nocsg = true\n");
			nocsg = true;
		}
		else if (!strcmp(argv[i], "-noshare"))
		{
			printf ("noshare = true\n");
			noshare = true;
		}
		else if (!strcmp(argv[i], "-notjunc"))
		{
			printf ("notjunc = true\n");
			notjunc = true;
		}
		else if (!strcmp(argv[i], "-nowater"))
		{
			printf ("nowater = true\n");
			nowater = true;
		}
		else if (!strcmp(argv[i], "-noopt"))
		{
			printf ("noopt = true\n");
			noopt = true;
		}
		else if (!strcmp(argv[i], "-noprune"))
		{
			printf ("noprune = true\n");
			noprune = true;
		}
		else if (!strcmp(argv[i], "-nofill"))
		{
			printf ("nofill = true\n");
			nofill = true;
		}
		else if (!strcmp(argv[i], "-nomerge"))
		{
			printf ("nomerge = true\n");
			nomerge = true;
		}
		else if (!strcmp(argv[i], "-nosubdiv"))
		{
			printf ("nosubdiv = true\n");
			nosubdiv = true;
		}
		else if (!strcmp(argv[i], "-nodetail"))
		{
			printf ("nodetail = true\n");
			nodetail = true;
		}
		else if (!strcmp(argv[i], "-fulldetail"))
		{
			printf ("fulldetail = true\n");
			fulldetail = true;
		}
		else if (!strcmp(argv[i], "-onlyents"))
		{
			printf ("onlyents = true\n");
			onlyents = true;
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
			leaktest = true;
		}
		else if (!strcmp(argv[i], "-verboseentities"))
		{
			printf ("verboseentities = true\n");
			verboseentities = true;
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
			norouting = true;
		}
		else if (!strcmp(argv[i], "-nobackclip"))
		{
			printf ("nobackclip = true\n");
			nobackclip = true;
		}
		else if (argv[i][0] == '-')
			Error ("Unknown option \"%s\"", argv[i]);
		else
			break;
	}

	if (i != argc - 1)
		Error ("usage: qbsp3 [-threads num] [-glview] [-v] [-draw] [-noweld] [-nocsg] [-noshare] [-notjunc] [-nowater] [-noopt] [-noprune] [-nofill] [-nomerge] [-nosubdiv] [-nodetail] [-fulldetail] [-onlyents] [-micro float] [-leaktest] [-verboseentities] [-chop] [-block num num] [-blocks num num num num] [-tmpout] [-norouting] [-nobackclip] mapfile");

	start = I_FloatTime ();

	ThreadSetDefault ();
	// numthreads = 1;		// multiple threads aren't helping...
	SetQdirFromPath (argv[i]);

	strcpy (source, ExpandArg (argv[i]));
	StripExtension (source);

	// delete portal and line files
	sprintf (path, "%s.prt", source);
	remove (path);
	sprintf (path, "%s.lin", source);
	remove (path);

	strcpy (name, ExpandArg (argv[i]));	
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

	return 0;
}

