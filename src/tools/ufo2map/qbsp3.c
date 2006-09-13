/**
 * @file qbsp.c
 * @brief
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
qboolean	verboseentities;
qboolean	norouting;
qboolean	nobackclip;

char outbase[32];
int block_xl = -8, block_xh = 7, block_yl = -8, block_yh = 7;
int entity_num;
vec3_t g_mins, g_maxs;
int level;



/**
 * @brief
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
		for ( i = 0; i < 258; i++ ) {
			if ( !(i%26) ) {
				fprintf (stdout, "%i...", (int)(i/26));
				fflush(stdout);
			/*	printf ("%i...", (int)(i/26) );*/
			}

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


/**
 * @brief
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


/**
 * @brief
 */
void ProcessModels (void)
{
	BeginBSPFile ();

	for (entity_num=0 ; entity_num< num_entities ; entity_num++) {
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
