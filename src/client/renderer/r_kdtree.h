/**
 * @file r_kdtree.h
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


#ifndef RENDERER_R_KDTREE_H
#define RENDERER_R_KDTREE_H

#include "r_entity.h"


typedef struct kdTreeNode_s {
	/* what dimension and value to split on */
	unsigned short dimension;
	float testVal;

	/* location and bounding info for this node */
	vec3_t center;
	vec3_t mins;
	vec3_t maxs;
	float radius;

	/* pointers to connected nodes */
	struct kdTreeNode_s *parent;
	struct kdTreeNode_s *left;
	struct kdTreeNode_s *right;

	/* pointer to the list of entities that overlap with this node*/
	entityListNode_t *ents;
} kdTreeNode_t;

extern kdTreeNode_t *renderTree;


void R_AddEntityToTree(entity_t *e);
/* @todo - add the ability to create non-uniform splits */
void R_InitRenderTree(int depth, vec3_t mins, vec3_t maxs);
void R_ClearRenderTree();

entityListNode_t * R_TreeLookupBox(vec3_t mins, vec3_t maxs);
entityListNode_t * R_TreeLookupSphere(vec3_t center, float radius);
entityListNode_t * R_TreeLookupFrustum(renderData_t *camera);
entity_t * R_TreeLookupNearest(vec3_t point);

#endif
