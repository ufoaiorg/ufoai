/**
 * @file tree.c
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

#include "bsp.h"

int c_nodes;
int c_nonvis;

/**
 * @sa AllocBrush
 * @sa AllocTree
 */
node_t *AllocNode (void)
{
	return Mem_AllocType(node_t);
}

static void FreeTreePortals_r (node_t *node)
{
	portal_t *p, *nextp;

	/* free children */
	if (node->planenum != PLANENUM_LEAF) {
		FreeTreePortals_r(node->children[0]);
		FreeTreePortals_r(node->children[1]);
	}

	/* free portals */
	for (p = node->portals; p; p = nextp) {
		const int s = (p->nodes[1] == node);
		nextp = p->next[s];

		RemovePortalFromNode(p, p->nodes[!s]);
		FreePortal(p);
	}
	node->portals = NULL;
}

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

	if (threadstate.numthreads == 1)
		c_nodes--;
	Mem_Free(node);
}


/**
 * @brief Allocates a tree and initializes it
 */
tree_t *AllocTree (void)
{
	tree_t *tree = Mem_AllocType(tree_t);

	ClearBounds(tree->mins, tree->maxs);

	return tree;
}


void FreeTree (tree_t *tree)
{
	FreeTreePortals_r(tree->headnode);
	FreeTree_r(tree->headnode);
	Mem_Free(tree);
}


static void CheckPlaneAgainstParents (uint16_t pnum, const node_t *node)
{
	node_t *p;

	for (p = node->parent; p; p = p->parent) {
		if (p->planenum == pnum)
			Sys_Error("Tried parent");
	}
}

static void LeafNode (node_t *node, bspbrush_t *brushes)
{
	node->side = NULL;
	node->planenum = PLANENUM_LEAF;

	Verb_Printf(VERB_DUMP, "LeafNode: scanning brushes.\n");

	node->contentFlags = BrushListCalcContents(brushes);
	node->brushlist = brushes;
}

static node_t *BuildTree_r (node_t *node, bspbrush_t *brushes)
{
	node_t *newnode;
	side_t *bestside;
	int i;
	bspbrush_t *children[2];

	if (threadstate.numthreads == 1)
		c_nodes++;

	/* find the best plane to use as a splitter */
	bestside = SelectSplitSide(brushes, node->volume);
	if (!bestside) {
		/* leaf node */
		LeafNode(node, brushes);
		Verb_Printf(VERB_DUMP, "BuildTree_r: Created a leaf node.\n");
		return node;
	}
	/* make sure the selected plane hasn't been used before. */
	CheckPlaneAgainstParents(bestside->planenum, node);

	Verb_Printf(VERB_DUMP, "BuildTree_r: splitting along plane %i\n", (int)bestside->planenum);

	/* this is a splitplane node */
	node->side = bestside;
	node->planenum = bestside->planenum & ~1;	/* always use front facing */

	SplitBrushList(brushes, node->planenum, &children[0], &children[1]);
	FreeBrushList(brushes);

	/* allocate children before recursing */
	for (i = 0; i < 2; i++) {
		newnode = AllocNode();
		newnode->parent = node;
		node->children[i] = newnode;
	}

	SplitBrush(node->volume, node->planenum, &node->children[0]->volume,
		&node->children[1]->volume);

	/* recursively process children */
	for (i = 0; i < 2; i++) {
		node->children[i] = BuildTree_r(node->children[i], children[i]);
	}

	return node;
}

/**
 * @brief The incoming list will be freed before exiting
 */
tree_t *BuildTree (bspbrush_t *brushlist, vec3_t mins, vec3_t maxs)
{
	node_t *node;
	tree_t *tree;
	vec3_t blmins, blmaxs;

	Verb_Printf(VERB_EXTRA, "--- BrushBSP ---\n");

	tree = AllocTree();

	ClearBounds(blmins, blmaxs);
	BrushlistCalcStats(brushlist, blmins, blmaxs);
	AddPointToBounds(blmins, tree->mins, tree->maxs);
	AddPointToBounds(blmaxs, tree->mins, tree->maxs);

	c_nodes = 0;
	c_nonvis = 0;
	node = AllocNode();

	node->volume = BrushFromBounds(mins, maxs);

	tree->headnode = node;

	node = BuildTree_r(node, brushlist);

	Verb_Printf(VERB_EXTRA, "%5i visible nodes\n", c_nodes / 2 - c_nonvis);
	Verb_Printf(VERB_EXTRA, "%5i nonvis nodes\n", c_nonvis);
	Verb_Printf(VERB_EXTRA, "%5i leafs\n", (c_nodes + 1) / 2);
	return tree;
}

/*=========================================================
NODES THAT DON'T SEPERATE DIFFERENT CONTENTS CAN BE PRUNED
=========================================================*/

static int c_pruned;

/**
 * @sa PruneNodes
 * @brief Will cut solid nodes by recursing down the bsp tree
 */
static void PruneNodes_r (node_t *node)
{
	bspbrush_t *b, *next;

	if (node->planenum == PLANENUM_LEAF)
		return;
	PruneNodes_r(node->children[0]);
	PruneNodes_r(node->children[1]);

	if ((node->children[0]->contentFlags & CONTENTS_SOLID)
	 && (node->children[1]->contentFlags & CONTENTS_SOLID)) {
		if (node->faces)
			Sys_Error("node->faces seperating CONTENTS_SOLID");
		if (node->children[0]->faces || node->children[1]->faces)
			Sys_Error("!node->faces with children");

		/** @todo free stuff */
		node->planenum = PLANENUM_LEAF;
		node->contentFlags = CONTENTS_SOLID;

		if (node->brushlist)
			Sys_Error("PruneNodes: node->brushlist");

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
 * @sa PruneNodes_r
 */
void PruneNodes (node_t *node)
{
	Verb_Printf(VERB_EXTRA, "--- PruneNodes ---\n");
	c_pruned = 0;
	PruneNodes_r(node);
	Verb_Printf(VERB_EXTRA, "%5i pruned nodes\n", c_pruned);
}
