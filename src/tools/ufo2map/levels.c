/**
 * @file levels.c
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


vec3_t		v_epsilon = { 1, 1, 1 };
int			brush_start, brush_end;

vec3_t		worldMins, worldMaxs;

int		oldleafs, oldleaffaces, oldleafbrushes, oldplanes, oldvertexes, oldnodes, oldtexinfo, oldfaces, oldedges, oldsurfedges;

/*
============
PushInfo
============
*/
void PushInfo ( void )
{
	oldleafs = numleafs;
	oldleaffaces = numleaffaces;
	oldleafbrushes = numleafbrushes;
	oldplanes = numplanes;
	oldvertexes = numvertexes;
	oldnodes = numnodes;
	oldtexinfo = numtexinfo;
	oldfaces = numfaces;
	oldedges = numedges;
	oldsurfedges = numsurfedges;
}

/*
============
PopInfo
============
*/
void PopInfo ( void )
{
	numleafs = oldleafs;
	numleaffaces = oldleaffaces;
	numleafbrushes = oldleafbrushes;
	numplanes = oldplanes;
	numvertexes = oldvertexes;
	numnodes = oldnodes;
	numtexinfo = oldtexinfo;
	numfaces = oldfaces;
	numedges = oldedges;
	numsurfedges = oldsurfedges;
}


/*
============
BuildNodeChildren
============
*/
int BuildNodeChildren( vec3_t mins, vec3_t maxs, int n[3] )
{
	int		node;
	int		i;

	node = -1;

	for ( i = 0; i < 3; i++ ) {
		if ( n[i] == -1 )
			continue;

		if ( node == -1 ) {
			/* store the valid node */
			node = n[i];
		} else {
			dnode_t		*newnode;
			vec3_t		newmins, newmaxs;
			vec3_t		addvec;

			/* add a new "special" dnode */
			/* and store it */
			newnode = &dnodes[numnodes];
			numnodes++;

			newnode->planenum = -1;
			newnode->children[0] = node;
			newnode->children[1] = n[i];
			newnode->firstface = 0;
			newnode->numfaces = 0;

			ClearBounds( newmins, newmaxs );
			VectorCopy( dnodes[node].mins, addvec ); AddPointToBounds( addvec, newmins, newmaxs );
			VectorCopy( dnodes[node].maxs, addvec ); AddPointToBounds( addvec, newmins, newmaxs );
			VectorCopy( dnodes[n[i]].mins, addvec ); AddPointToBounds( addvec, newmins, newmaxs );
			VectorCopy( dnodes[n[i]].maxs, addvec ); AddPointToBounds( addvec, newmins, newmaxs );
			VectorCopy( newmins, newnode->mins );
			VectorCopy( newmaxs, newnode->maxs );

			AddPointToBounds( newmins, worldMins, worldMaxs );
			AddPointToBounds( newmaxs, worldMins, worldMaxs );

			node = numnodes - 1;
		}
	}

	/* return the last stored node */
	return node;
}


/*
============
ConstructLevelNodes_r
============
*/
int	ConstructLevelNodes_r( int levelnum, vec3_t cmins, vec3_t cmaxs )
{
	bspbrush_t	*list;
	tree_t		*tree;
	vec3_t		diff;
	vec3_t		bmins, bmaxs;
	int			nn[3];
	node_t		*n;

	/* calculate bounds, stop if no brushes are available */
	if ( !MapBrushesBounds( brush_start, brush_end, levelnum, cmins, cmaxs, bmins, bmaxs ) )
		return -1;

/*	VectorCopy( cmins, bmins ); */
/*	VectorCopy( cmaxs, bmaxs ); */

	VectorSubtract( bmaxs, bmins, diff );

/*	printf( "(%i): %i %i: (%i %i) (%i %i) -> (%i %i) (%i %i)\n", levelnum, (int)diff[0], (int)diff[1], */
/*		(int)cmins[0], (int)cmins[1], (int)cmaxs[0], (int)cmaxs[1], */
/*		(int)bmins[0], (int)bmins[1], (int)bmaxs[0], (int)bmaxs[1] ); */

	if ( diff[0] > 256 || diff[1] > 256 ) {
		/* continue subdivision */
		/* split the remaining hull at the middle of */
		/* the longer axis */
		vec3_t		nmins, nmaxs;
		int			n;

		if ( diff[1] > diff[0] )
			n = 1;
		else
			n = 0;

		VectorCopy( bmins, nmins );
		VectorCopy( bmaxs, nmaxs );

		nmaxs[n] -= diff[n]/2;
/*		printf( "  (%i %i) (%i %i)\n", (int)nmins[0], (int)nmins[1], (int)nmaxs[0], (int)nmaxs[1] ); */
		nn[0] = ConstructLevelNodes_r( levelnum, nmins, nmaxs );

		nmins[n] += diff[n]/2;
		nmaxs[n] += diff[n]/2;
/*		printf( "    (%i %i) (%i %i)\n", (int)nmins[0], (int)nmins[1], (int)nmaxs[0], (int)nmaxs[1] ); */
		nn[1] = ConstructLevelNodes_r( levelnum, nmins, nmaxs );
	} else {
		/* no children */
		nn[0] = -1;
		nn[1] = -1;
	}

	BeginModel ();

	/* add v_epsilon to avoid clipping errors */
	VectorSubtract( bmins, v_epsilon, bmins );
	VectorAdd( bmaxs, v_epsilon, bmaxs );

	list = MakeBspBrushList( brush_start, brush_end, levelnum, bmins, bmaxs);
	if ( !list ) {
		nn[2] = -1;
		return BuildNodeChildren( bmins, bmaxs, nn );
	}

	if (!nocsg)
		list = ChopBrushes (list);

	/* begin model creation now */
	tree = BrushBSP (list, bmins, bmaxs);
	MakeTreePortals (tree);
	MarkVisibleSides (tree, brush_start, brush_end);
	MakeFaces (tree->headnode);
	FixTjuncs (tree->headnode);

	/* correct bounds */
	n = tree->headnode;
	VectorAdd( bmins, v_epsilon, n->mins );
	VectorSubtract( bmaxs, v_epsilon, n->maxs );

	/* finish model */
	WriteBSP (tree->headnode);
	FreeTree (tree);

/*	EndModel (); */

/*	printf( "  headnode: %i\n", dmodels[nummodels].headnode ); */

	nn[2] = dmodels[nummodels].headnode;
	return BuildNodeChildren( bmins, bmaxs, nn );
}


/*
============
ProcessLevel

============
*/
void ProcessLevel (int levelnum)
{
	vec3_t		mins, maxs;
	dmodel_t	*dm;

	/* get maxs */
	mins[0] = (block_xl)*512+1;
	mins[1] = (block_yl)*512+1;
	mins[2] = -4096+1;

	maxs[0] = (block_xh+1)*512-1;
	maxs[1] = (block_yh+1)*512-1;
	maxs[2] = 4096-1;

	dm = &dmodels[levelnum];
	memset( dm, 0, sizeof(dmodel_t) );

	/* back copy backup brush sides structure */
	/* to reset all the changed values (especialy "finished") */
	memcpy( mapbrushes, mapbrushes + nummapbrushes, sizeof(mapbrush_t)*nummapbrushes );

	/* store face number for later use */
	dm->firstface = numfaces;
	dm->headnode = ConstructLevelNodes_r( levelnum, mins, maxs );
	dm->numfaces = numfaces - dm->firstface;

/*	if ( !dm->numfaces ) */
/*	{ */
/*		printf( "level: %i -> %i : f %i\n", levelnum, dm->headnode, dm->numfaces ); */
/*	} */
}

