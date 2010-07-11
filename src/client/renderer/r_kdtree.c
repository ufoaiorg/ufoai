/**
 * @file r_kdtree.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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


#include "r_kdtree.h"

#define MAX_RENDER_TREE_NODES 1024

static kdTreeNode_t renderTreeNodes[MAX_RENDER_TREE_NODES];
static qboolean kdtreeInitialized = qfalse;
static unsigned int numNodes;



/* @note - recursive implementation isn't super efficient, but we should
 * only need to build the tree once per map */
static kdTreeNode_t * R_RecursiveBuild (qboolean left, kdTreeNode_t *parent, int depth)
{
	kdTreeNode *node;
	unsigned short d;
	vec3_t v;

	if (depth == 0)
		return NULL;

	node = &renderTreeNodes[numNodes++];
	if (numNodes >= MAX_RENDER_TREE_NODES) 
		Com_Error(ERR_DROP, "R_RecursiveBuild: Max tree nodes exceeded");

	d = (parent->dimension + 1) % 3;
	node->dimension = d;
	node->parent = parent;
	node->ents = NULL;

	VectorCopy(parent->center, node->center);
	VectorCopy(parent->mins, node->mins);
	VectorCopy(parent->maxs, node->maxs);

	if (left) {
		node->center[d] = (parent->center[d] + parent->mins[d]) / 2.0;
		node->maxs[d] = parent->center[d];
	} else {
		node->center[d] = (parent->center[d] + parent->maxs[d]) / 2.0;
		node->mins[d] = parent->center[d];
	}

	VectorSubtract(node->maxs, node->mins, v);
	node->radius = sqrt(VectorLength(v) / 2.0);

	node->testVal = node->center[d];

	node->left = R_RecursiveBuild(qtrue, node, depth-1);
	node->right = R_RecursiveBuild(qfalse, node, depth-1);
}

/* @brief build KD-tree nodes with uniform splits 
 * @param[in] depth the depth to build the tree to
 * @param[in] mins lower bounds for the root node
 * @param[in] maxs upper bounds for the root node
 */
void R_InitRenderTree (int depth, vec3_t mins, vec3_t maxs)
{
	int i, j, k;
	kdTreeNode_t node = &renderTreeNodes[0];
	vec3_t v;

	numNodes = 1;

	renderTree = node;
	VectorCopy(mins, node->mins);
	VectorCopy(maxs, node->maxs);
	VectorAdd(mins, maxs, node->center);
	VectorScale(node->center, 0.5, node->center);

	VectorSubtract(maxs, mins, v);
	node->radius = sqrt(VectorLength(v) / 2.0);
	node->parent = NULL;
	node->ents = NULL;

	node->dimension = 0;
	/* @todo - add the ability to create non-uniform splits */
	node->testVal = node->center[0];

	node->left = R_RecursiveBuild(qtrue, node, depth-1);
	node->right = R_RecursiveBuild(qfalse, node, depth-1);

	kdtreeInitialized = qtrue;
}

/* @brief remove all entities from the KD-tree */
void R_ClearRenderTree (void)
{
	int i;

	if (!kdtreeInitialized) 
		return;

	for (i = 0; i < numNodes; i++) {
		if (renderTreeNodes[i].ents != NULL) {
			R_FreeEntityList(renderTreeNodes[i].ents);
			renderTreeNodes[i].ents = NULL;
		}
	}
	
}


/* @brief find all entities that overlap with a box */
entityListNode_t * R_TreeLookupBox (vec3_t mins, vec3_t maxs)
{

	/* @todo - write this */
	return NULL;
}

/* @brief find all entities that overlap with a sphere */
entityListNode_t * R_TreeLookupSphere (vec3_t center, float radius)
{

	/* @todo - write this */
	return NULL;
}

/* @brief find the entity nearest to the input point */
entity_t * R_TreeLookupNearest (vec3_t point)
{
	/* @todo - write this */
	return NULL;
}


/* @brief add an entity to the KD-tree */
void R_AddEntityToTree(entity_t *e) 
{
	kdTreeNode_t *ptr = renderTree;

	/* @todo - do we need to add e->origin to e->mins and maxs?  */
	while (ptr) {
		const unsigned short d = ptr->dimension;
		const float t = ptr->testVal;

		/* traverse the tree based on the appropriate test */
		if (e->mins[d] < t) {
			/* if the object stradles the split, or the "next" is null,
			 * this node is as low as we can go */
			if (e->maxs[d] > t || (ptr->left == NULL) ) {
				R_AddEntityToList(ptr->ents, e);
				return;
			}

			ptr = ptr->left;
		} else {
			if (ptr->right == NULL) {
				R_AddEntityToList(ptr->ents, e);
				return;
			}
			ptr = ptr->right;
		}

	}

}
