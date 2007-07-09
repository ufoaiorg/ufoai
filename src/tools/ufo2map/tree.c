/**
 * @file tree.c
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

extern int c_nodes;

void RemovePortalFromNode (portal_t *portal, node_t *l);

/**
 * @brief
 */
static void FreeTreePortals_r (node_t *node)
{
	portal_t *p, *nextp;
	int s;

	/* free children */
	if (node->planenum != PLANENUM_LEAF) {
		FreeTreePortals_r (node->children[0]);
		FreeTreePortals_r (node->children[1]);
	}

	/* free portals */
	for (p = node->portals; p; p = nextp) {
		s = (p->nodes[1] == node);
		nextp = p->next[s];

		RemovePortalFromNode (p, p->nodes[!s]);
		FreePortal (p);
	}
	node->portals = NULL;
}

/**
 * @brief
 */
static void FreeTree_r (node_t *node)
{
	face_t *f, *nextf;

	/* free children */
	if (node->planenum != PLANENUM_LEAF) {
		FreeTree_r(node->children[0]);
		FreeTree_r(node->children[1]);
	}

	/* free bspbrushes */
	FreeBrushList(node->brushlist);

	/* free faces */
	for (f = node->faces; f; f = nextf) {
		nextf = f->next;
		FreeFace(f);
	}

	/* free the node */
	if (node->volume)
		FreeBrush(node->volume);

	c_nodes--;
	free(node);
}


/**
 * @brief
 */
void FreeTree (tree_t *tree)
{
	FreeTreePortals_r(tree->headnode);
	FreeTree_r(tree->headnode);
	free(tree);
}

/**
 * @brief
 */
static void PrintTree_r (node_t *node, int depth)
{
	int i;
	plane_t *plane;
	bspbrush_t *bb;

	for (i = 0; i < depth; i++)
		Sys_Printf("  ");
	if (node->planenum == PLANENUM_LEAF) {
		if (!node->brushlist)
			Sys_Printf("NULL\n");
		else {
			for (bb = node->brushlist; bb; bb = bb->next)
				Sys_Printf("%i ", bb->original->brushnum);
			Sys_Printf("\n");
		}
		return;
	}

	plane = &mapplanes[node->planenum];
	Sys_Printf("#%i (%5.2f %5.2f %5.2f):%5.2f\n", node->planenum,
		plane->normal[0], plane->normal[1], plane->normal[2],
		plane->dist);
	PrintTree_r(node->children[0], depth+1);
	PrintTree_r(node->children[1], depth+1);
}

/*=========================================================
NODES THAT DON'T SEPERATE DIFFERENT CONTENTS CAN BE PRUNED
=========================================================*/

int	c_pruned;

/**
 * @brief
 * @sa PruneNodes
 */
void PruneNodes_r (node_t *node)
{
	bspbrush_t *b, *next;

	if (node->planenum == PLANENUM_LEAF)
		return;
	PruneNodes_r(node->children[0]);
	PruneNodes_r(node->children[1]);

	if ((node->children[0]->contents & CONTENTS_SOLID)
		&& (node->children[1]->contents & CONTENTS_SOLID)) {
		if (node->faces)
			Error("node->faces seperating CONTENTS_SOLID");
		if (node->children[0]->faces || node->children[1]->faces)
			Error("!node->faces with children");

		/* FIXME: free stuff */
		node->planenum = PLANENUM_LEAF;
		node->contents = CONTENTS_SOLID;
		node->detail_seperator = qfalse;

		if (node->brushlist)
			Error("PruneNodes: node->brushlist");

		/* combine brush lists */
		node->brushlist = node->children[1]->brushlist;

		for (b = node->children[0]->brushlist; b; b = next) {
			next = b->next;
			b->next = node->brushlist;
			node->brushlist = b;
		}

		c_pruned++;
	}
}

/**
 * @brief
 * @sa PruneNodes_r
 */
void PruneNodes (node_t *node)
{
	Sys_FPrintf(SYS_VRB, "--- PruneNodes ---\n");
	c_pruned = 0;
	PruneNodes_r(node);
	Sys_FPrintf(SYS_VRB, "%5i pruned nodes\n", c_pruned);
}
