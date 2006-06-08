/* csg4.c */

#include "qbsp.h"

vec_t		microvolume = 1.0;
qboolean	noprune;
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

/*	BeginModel (); */

	e = &entities[entity_num];

	brush_start = e->firstbrush;
	brush_end = brush_start + e->numbrushes;

	ClearBounds( worldMins, worldMaxs );

	/* process levels */
	{
		int	start, end;
		nummodels = 258;

		start = I_FloatTime();
		for ( i = 0; i < 258; i++ )
		{
			if ( !(i%26) )
				printf ("%i...", (int)(i/26) );

			/* process brushes with that level mask */
			ProcessLevel( i );
		}
		end = I_FloatTime();

		printf( " (%i)\n", end-start );
	}

	/* calculate routing */
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
			verbose = qfalse;	/* don't bother printing submodels */
	}

	EndBSPFile ();
}
