/**
 * @file levels.c
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

vec3_t worldMins, worldMaxs;

static int oldmodels, oldleafs, oldleafbrushes, oldplanes, oldvertexes, oldnormals, oldnodes, oldtexinfo, oldfaces, oldedges, oldsurfedges;

void ClearWorldBounds (void)
{
	ClearBounds(worldMins, worldMaxs);
}

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
	int i;

	for (i = 0; i < 3; i++) {
		dBspNode_t *newnode;
		vec3_t newmins, newmaxs, addvec;

		if (n[i] == LEAFNODE)
			continue;

		if (node == LEAFNODE) {
			/* store the valid node */
			node = n[i];

			ClearBounds(newmins, newmaxs);
			VectorCopy(curTile->nodes[node].mins, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(curTile->nodes[node].maxs, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
		} else {
			/* add a new "special" dnode and store it */
			newnode = &curTile->nodes[curTile->numnodes];
			curTile->numnodes++;

			newnode->planenum = PLANENUM_LEAF;
			newnode->children[0] = node;
			newnode->children[1] = n[i];
			newnode->firstface = 0;
			newnode->numfaces = 0;

			ClearBounds(newmins, newmaxs);
			VectorCopy(curTile->nodes[node].mins, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(curTile->nodes[node].maxs, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(curTile->nodes[n[i]].mins, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(curTile->nodes[n[i]].maxs, addvec);
			AddPointToBounds(addvec, newmins, newmaxs);
			VectorCopy(newmins, newnode->mins);
			VectorCopy(newmaxs, newnode->maxs);

			node = curTile->numnodes - 1;
		}

		AddPointToBounds(newmins, worldMins, worldMaxs);
		AddPointToBounds(newmaxs, worldMins, worldMaxs);

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
static int32_t ConstructLevelNodes_r (const int levelnum, const vec3_t cmins, const vec3_t cmaxs, int entityNum)
{
	bspbrush_t *list;
	tree_t *tree;
	vec3_t bmins, bmaxs;
	int32_t tmins[SPLIT_COORDS], tmaxs[SPLIT_COORDS];
	int32_t nn[3];
	node_t *node;
	int i;

	/* calculate bounds, stop if no brushes are available */
	if (!MapBrushesBounds(brush_start, brush_end, levelnum, cmins, cmaxs, bmins, bmaxs))
		return LEAFNODE;

	Verb_Printf(VERB_DUMP, "ConstructLevelNodes_r: lv=%i (%f %f %f) (%f %f %f)\n", levelnum,
		cmins[0], cmins[1], cmins[2], cmaxs[0], cmaxs[1], cmaxs[2]);

	for (i = 0; i < SPLIT_COORDS; i++) {
		tmins[i] = ((int)floor(bmins[i])) >> SPLIT_AT_POW2;
		tmaxs[i] = ((int)ceil(bmaxs[i])) >> SPLIT_AT_POW2;
	}

	Verb_Printf(VERB_DUMP, "(%i): %i %i: (%i %i) (%i %i) -> (%i %i) (%i %i)\n", levelnum, tmaxs[0] - tmins[0], tmaxs[1] - tmins[1],
		(int)cmins[0], (int)cmins[1], (int)cmaxs[0], (int)cmaxs[1],
		(int)bmins[0], (int)bmins[1], (int)bmaxs[0], (int)bmaxs[1]);

	/** @todo better algo to chose a split position - could force all splits to make continuous grid, for example */
	if (tmaxs[1] - tmins[1] >= 2 || tmaxs[0] - tmins[0] >= 2) {
		/* continue subdivision */
		/* split the remaining hull at pow2 pos about the middle of the longer axis */
		vec3_t nmins, nmaxs;
		int n;
		int32_t splitAt;

		if (tmaxs[1] - tmins[1] > tmaxs[0] - tmins[0])
			n = 1;
		else
			n = 0;

		VectorCopy(bmins, nmins);
		VectorCopy(bmaxs, nmaxs);

		splitAt = (tmins[n] + ((tmaxs[n] - tmins[n]) >> 1)) << SPLIT_AT_POW2;
		nmaxs[n] = splitAt;

		Verb_Printf(VERB_DUMP, "Chlid 0:  (%i %i) (%i %i)\n", (int)nmins[0], (int)nmins[1], (int)nmaxs[0], (int)nmaxs[1]);
		nn[0] = ConstructLevelNodes_r(levelnum, nmins, nmaxs, entityNum);

		VectorCopy(bmaxs, nmaxs);

		nmins[n] = splitAt;
		Verb_Printf(VERB_DUMP, "Child 1:  (%i %i) (%i %i)\n", (int)nmins[0], (int)nmins[1], (int)nmaxs[0], (int)nmaxs[1]);
		nn[1] = ConstructLevelNodes_r(levelnum, nmins, nmaxs, entityNum);
	} else {
		/* no children */
		nn[0] = LEAFNODE;
		nn[1] = LEAFNODE;
	}

	/* add v_epsilon to avoid clipping errors */
	VectorSubtract(bmins, v_epsilon, bmins);
	VectorAdd(bmaxs, v_epsilon, bmaxs);

	/* Call BeginModel only to initialize brush pointers */
	BeginModel(entityNum);

	list = MakeBspBrushList(brush_start, brush_end, levelnum, bmins, bmaxs);
	if (!list) {
		nn[2] = LEAFNODE;
		return BuildNodeChildren(nn);
	}

	if (!config.nocsg)
		list = ChopBrushes(list);

	/* begin model creation now */
	tree = BuildTree(list, bmins, bmaxs);
	MakeTreePortals(tree);
	MarkVisibleSides(tree, brush_start, brush_end);
	MakeFaces(tree->headnode);
	FixTjuncs(tree->headnode);

	if (!config.noprune)
		PruneNodes(tree->headnode);

	/* correct bounds */
	node = tree->headnode;
	VectorAdd(bmins, v_epsilon, node->mins);
	VectorSubtract(bmaxs, v_epsilon, node->maxs);

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
	dBspModel_t *dm;

	/* oversizing the blocks guarantees that all the boundaries will also get nodes. */
	/* get maxs */
	mins[0] = (config.block_xl) * 512.0 + 1.0;
	mins[1] = (config.block_yl) * 512.0 + 1.0;
	mins[2] = -MAX_WORLD_WIDTH + 1.0;

	maxs[0] = (config.block_xh + 1.0) * 512.0 - 1.0;
	maxs[1] = (config.block_yh + 1.0) * 512.0 - 1.0f;
	maxs[2] = MAX_WORLD_WIDTH - 1.0;

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
	dm->headnode = ConstructLevelNodes_r(levelnum, mins, maxs, entityNum);
	/* This here replaces the calls to EndModel */
	dm->numfaces = curTile->numfaces - dm->firstface;

/*	if (!dm->numfaces)
		Com_Printf("level: %i -> %i : f %i\n", levelnum, dm->headnode, dm->numfaces);
*/
}
