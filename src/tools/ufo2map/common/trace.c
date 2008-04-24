/**
 * @file trace.c
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


#include "shared.h"
#include "cmdlib.h"
#include "../bsp.h"
#include "bspfile.h"
#include <stddef.h>


/**
 * @brief Use the bsp node structure to reconstruct efficient tracing structures
 * that are used for fast visibility and pathfinding checks
 * @note curTile->tnodes is expected to have enough memory malloc'ed for the function to work.
 * @sa BuildTracingNode_r
 */
void MakeTracingNodes (int levels)
{
	size_t size;
	int i;

	/* Release any memory we have for existing tnodes, just in case. */
	if (curTile->tnodes)
		CloseTracingNodes();

	size = (curTile->numnodes + 1) * sizeof(tnode_t);
	/* 32 byte align the structs */
	size = (size + 31) &~ 31;
	/* allocate memory for the tnodes structure */
	curTile->tnodes = malloc(size);
	tnode_p = curTile->tnodes;
	curTile->numtheads = 0;

	for (i = 0; i < levels; i++) {
		if (!curTile->models[i].numfaces)
			continue;

		curTile->thead[curTile->numtheads] = tnode_p - curTile->tnodes;
		curTile->theadlevel[curTile->numtheads] = i;
		curTile->numtheads++;
		assert(curTile->numtheads < LEVEL_MAX);
		TR_BuildTracingNode_r(curTile->models[i].headnode, i);
	}
}


/**
 * @brief
 */
static int TestContents_r (int node, const vec3_t pos)
{
	tnode_t	*tnode;
	float	front;
	int		r;

	if (node & (1 << 31))
		return node & ~(1 << 31);	/* leaf node */

	tnode = &curTile->tnodes[node];
	switch (tnode->type) {
	case PLANE_X:
		front = pos[0] - tnode->dist;
		break;
	case PLANE_Y:
		front = pos[1] - tnode->dist;
		break;
	case PLANE_Z:
		front = pos[2] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TestContents_r(tnode->children[0], pos);
		if (r)
			return r;
		return TestContents_r(tnode->children[1], pos);
		break;
	default:
		front = (pos[0] * tnode->normal[0] + pos[1] * tnode->normal[1] + pos[2] * tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= 0)
		return TestContents_r(tnode->children[0], pos);
	else
		return TestContents_r(tnode->children[1], pos);
}


/**
 * @brief Step height check
 * @sa TestContents_r
 */
qboolean TestContents (const vec3_t pos)
{
	int i;

	/* loop over all theads */
	for (i = curTile->numtheads - 1; i >= 0; i--) {
		if (curTile->theadlevel[i] != LEVEL_STEPON) /* only check stepon here */
			continue;

		if (TestContents_r(curTile->thead[i], pos))
			return qtrue;
		break;
	}
	return qfalse;
}

/**
 * @brief
 * @sa MakeTnodes
 */
void CloseTracingNodes (void)
{
	if (curTile->tnodes)
		free(curTile->tnodes);
	curTile->tnodes = NULL;
}
