/**
 * @file
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

#include "levels.h"
#include "bsp.h"


const vec3_t v_epsilon = { 1, 1, 1 };
int brush_start, brush_end;

static int oldmodels, oldleafs, oldleafbrushes, oldplanes, oldvertexes, oldnormals, oldnodes, oldtexinfo, oldfaces, oldedges, oldsurfedges;

void PushInfo (void)
{
	oldmodels = curTile->nummodels;
	oldleafs = curTile->numleafs;
	oldleafbrushes = curTile->numleafbrushes;
	oldplanes = curTile->numplanes;
	oldvertexes = curTile->numvertexes;
	oldnormals = curTile->numnormals;
	oldnodes = curTile->numnodes;
	oldtexinfo = curTile->numtexinfo;
	oldfaces = curTile->numfaces;
	oldedges = curTile->numedges;
	oldsurfedges = curTile->numsurfedges;
}

void PopInfo (void)
{
	curTile->nummodels = oldmodels;
	curTile->numleafs = oldleafs;
	curTile->numleafbrushes = oldleafbrushes;
	curTile->numplanes = oldplanes;
	curTile->numvertexes = oldvertexes;
	curTile->numnormals = oldnormals;
	curTile->numnodes = oldnodes;
	curTile->numtexinfo = oldtexinfo;
	curTile->numfaces = oldfaces;
	curTile->numedges = oldedges;
	curTile->numsurfedges = oldsurfedges;
}

/**
 * @param[in] n The node nums
 * @sa R_ModLoadNodes
 */
static int32_t BuildNodeChildren (const int n[3])
{
	int32_t node = LEAFNODE;

	for (int i = 0; i < 3; i++) {
		AABB aabb(vec3_origin, vec3_origin);

		if (n[i] == LEAFNODE)
			continue;

		if (node == LEAFNODE) {
			/* store the valid node */
			node = n[i];

			aabb.setNegativeVolume();
			VectorCopy(curTile->nodes[node].mins, aabb.mins);
			VectorCopy(curTile->nodes[node].maxs, aabb.maxs);
		} else {
			dBspNode_t* newnode;
			vec3_t addvec;
			/* add a new "special" dnode and store it */
			newnode = &curTile->nodes[curTile->numnodes];
			curTile->numnodes++;

			newnode->planenum = PLANENUM_LEAF;
			newnode->children[0] = node;
			newnode->children[1] = n[i];
			newnode->firstface = 0;
			newnode->numfaces = 0;

			aabb.setNegativeVolume();
			VectorCopy(curTile->nodes[node].mins, aabb.mins);
			VectorCopy(curTile->nodes[node].maxs, aabb.maxs);
			VectorCopy(curTile->nodes[n[i]].mins, addvec);
			aabb.add(addvec);
			VectorCopy(curTile->nodes[n[i]].maxs, addvec);
			aabb.add(addvec);
			VectorCopy(aabb.mins, newnode->mins);
			VectorCopy(aabb.maxs, newnode->maxs);

			node = curTile->numnodes - 1;
		}

		Verb_Printf(VERB_DUMP, "BuildNodeChildren: node=%i (%i %i %i) (%i %i %i)\n", node,
			curTile->nodes[node].mins[0], curTile->nodes[node].mins[1], curTile->nodes[node].mins[2], curTile->nodes[node].maxs[0], curTile->nodes[node].maxs[1], curTile->nodes[node].maxs[2]);
	}

	/* return the last stored node */
	return node;
}

#define SPLIT_AT_POW2 6
#define SPLIT_COORDS 2

/**
 * @sa ProcessLevel
 * @return The node num
 */
static int32_t ConstructLevelNodes_r (const int levelnum, const AABB& partBox, int entityNum)
{
	bspbrush_t* list;
	tree_t* tree;
	AABB bBox;		/* bounding box for all brushes inside partBox */
	int32_t tmins[SPLIT_COORDS], tmaxs[SPLIT_COORDS];
	int32_t nn[3];
	node_t* node;

	/* calculate bounds, stop if no brushes are available */
	if (!MapBrushesBounds(brush_start, brush_end, levelnum, partBox, bBox.mins, bBox.maxs))
		return LEAFNODE;

	Verb_Printf(VERB_DUMP, "ConstructLevelNodes_r: lv=%i (%f %f %f) (%f %f %f)\n", levelnum,
		partBox.mins[0], partBox.mins[1], partBox.mins[2], partBox.maxs[0], partBox.maxs[1], partBox.maxs[2]);

	for (int i = 0; i < SPLIT_COORDS; i++) {
		tmins[i] = ((int)floor(bBox.mins[i])) >> SPLIT_AT_POW2;
		tmaxs[i] = ((int)ceil(bBox.maxs[i])) >> SPLIT_AT_POW2;
	}

	Verb_Printf(VERB_DUMP, "(%i): %i %i: (%i %i) (%i %i) -> (%i %i) (%i %i)\n", levelnum, tmaxs[0] - tmins[0], tmaxs[1] - tmins[1],
		(int)partBox.mins[0], (int)partBox.mins[1], (int)partBox.maxs[0], (int)partBox.maxs[1],
		(int)bBox.mins[0], (int)bBox.mins[1], (int)bBox.maxs[0], (int)bBox.maxs[1]);

	/** @todo better algo to chose a split position - could force all splits to make continuous grid, for example */
	if (tmaxs[1] - tmins[1] >= 2 || tmaxs[0] - tmins[0] >= 2) {
		/* continue subdivision */
		/* split the remaining hull at pow2 pos about the middle of the longer axis */
		int n;
		int32_t splitAt;

		if (tmaxs[1] - tmins[1] > tmaxs[0] - tmins[0])
			n = 1;
		else
			n = 0;

		AABB newPartBox(bBox);		/* bounding box of the partition */

		splitAt = (tmins[n] + ((tmaxs[n] - tmins[n]) >> 1)) << SPLIT_AT_POW2;
		newPartBox.maxs[n] = splitAt;

		Verb_Printf(VERB_DUMP, "Chlid 0:  (%i %i) (%i %i)\n", (int)newPartBox.mins[0], (int)newPartBox.mins[1], (int)newPartBox.maxs[0], (int)newPartBox.maxs[1]);
		nn[0] = ConstructLevelNodes_r(levelnum, newPartBox, entityNum);

		newPartBox.setMaxs(bBox.maxs);
		newPartBox.mins[n] = splitAt;

		Verb_Printf(VERB_DUMP, "Child 1:  (%i %i) (%i %i)\n", (int)newPartBox.mins[0], (int)newPartBox.mins[1], (int)newPartBox.maxs[0], (int)newPartBox.maxs[1]);
		nn[1] = ConstructLevelNodes_r(levelnum, newPartBox, entityNum);
	} else {
		/* no children */
		nn[0] = LEAFNODE;
		nn[1] = LEAFNODE;
	}

	/* add v_epsilon to avoid clipping errors */
	VectorSubtract(bBox.mins, v_epsilon, bBox.mins);
	VectorAdd(bBox.maxs, v_epsilon, bBox.maxs);

	/* Call BeginModel only to initialize brush pointers */
	BeginModel(entityNum);

	list = MakeBspBrushList(brush_start, brush_end, levelnum, bBox);
	if (!list) {
		nn[2] = LEAFNODE;
		return BuildNodeChildren(nn);
	}

	if (!config.nocsg)
		list = ChopBrushes(list);

	/* begin model creation now */
	tree = BuildTree(list, bBox.mins, bBox.maxs);
	MakeTreePortals(tree);
	MarkVisibleSides(tree, brush_start, brush_end);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);

	if (!config.noprune)
		PruneNodes(tree->headnode);

	/* correct bounds */
	node = tree->headnode;
	VectorAdd(bBox.mins, v_epsilon, node->nBox.mins);
	VectorSubtract(bBox.maxs, v_epsilon, node->nBox.maxs);

	/* finish model */
	nn[2] = WriteBSP(tree->headnode);
	FreeTree(tree);

	/* Store created nodes */
	return BuildNodeChildren(nn);
}

/** @todo userdata for ProcessLevel thread */
static int entityNum;

void ProcessLevelEntityNumber (int entityNumber)
{
	entityNum = entityNumber;
}

/**
 * @brief process brushes with that level mask
 * @param[in] levelnum is the level mask
 * @note levelnum
 * 256: weaponclip-level
 * 257: actorclip-level
 * 258: stepon-level
 * 259: tracing structure
 * @sa ProcessWorldModel
 * @sa ConstructLevelNodes_r
 */
void ProcessLevel (unsigned int levelnum)
{
	vec3_t mins, maxs;
	dBspModel_t* dm;

	/* oversizing the blocks guarantees that all the boundaries will also get nodes. */
	/* get maxs */
	mins[0] = (config.block_xl) * 512.0 + 1.0;
	mins[1] = (config.block_yl) * 512.0 + 1.0;
	mins[2] = -MAX_WORLD_WIDTH + 1.0;

	maxs[0] = (config.block_xh + 1.0) * 512.0 - 1.0;
	maxs[1] = (config.block_yh + 1.0) * 512.0 - 1.0f;
	maxs[2] = MAX_WORLD_WIDTH - 1.0;
	const AABB partBox(mins, maxs);		/* bounding box of the level */

	Verb_Printf(VERB_EXTRA, "Process levelnum %i (curTile->nummodels: %i)\n", levelnum, curTile->nummodels);

	/** @note Should be reentrant as each thread has a unique levelnum at any given time */
	dm = &curTile->models[levelnum];
	OBJZERO(*dm);

	/** @todo Check what happens if two threads do the memcpy */
	/* back copy backup brush sides structure */
	/* to reset all the changed values (especialy "finished") */
	memcpy(mapbrushes, mapbrushes + nummapbrushes, sizeof(mapbrush_t) * nummapbrushes);

	/* Store face number for later use */
	dm->firstface = curTile->numfaces;
	dm->headnode = ConstructLevelNodes_r(levelnum, partBox, entityNum);
	/* This here replaces the calls to EndModel */
	dm->numfaces = curTile->numfaces - dm->firstface;

/*	if (!dm->numfaces)
		Com_Printf("level: %i -> %i : f %i\n", levelnum, dm->headnode, dm->numfaces);
*/
}
