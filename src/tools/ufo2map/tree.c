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
	portal_t	*p, *nextp;
	int			s;

	/* free children */
	if (node->planenum != PLANENUM_LEAF) {
		FreeTreePortals_r (node->children[0]);
		FreeTreePortals_r (node->children[1]);
	}

	/* free portals */
	for (p=node->portals ; p ; p=nextp) {
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
	face_t		*f, *nextf;

	/* free children */
	if (node->planenum != PLANENUM_LEAF) {
		FreeTree_r (node->children[0]);
		FreeTree_r (node->children[1]);
	}

	/* free bspbrushes */
	FreeBrushList (node->brushlist);

	/* free faces */
	for (f=node->faces ; f ; f=nextf) {
		nextf = f->next;
		FreeFace (f);
	}

	/* free the node */
	if (node->volume)
		FreeBrush (node->volume);

	if (numthreads == 1)
		c_nodes--;
	free (node);
}


/**
 * @brief
 */
extern void FreeTree (tree_t *tree)
{
	FreeTreePortals_r (tree->headnode);
	FreeTree_r (tree->headnode);
	free (tree);
}

/**
 * @brief
 */
static void PrintTree_r (node_t *node, int depth)
{
	int		i;
	plane_t	*plane;
	bspbrush_t	*bb;

	for (i=0 ; i<depth ; i++)
		printf ("  ");
	if (node->planenum == PLANENUM_LEAF) {
		if (!node->brushlist)
			printf ("NULL\n");
		else {
			for (bb=node->brushlist ; bb ; bb=bb->next)
				printf ("%i ", bb->original->brushnum);
			printf ("\n");
		}
		return;
	}

	plane = &mapplanes[node->planenum];
	printf ("#%i (%5.2f %5.2f %5.2f):%5.2f\n", node->planenum,
		plane->normal[0], plane->normal[1], plane->normal[2],
		plane->dist);
	PrintTree_r (node->children[0], depth+1);
	PrintTree_r (node->children[1], depth+1);
}
