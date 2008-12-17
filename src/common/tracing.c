/**
 * @file tracing.c
 * @brief model tracing and bounding
 * @note collision detection code
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "common.h"
#include "mem.h"
#include "tracing.h"

/** @note used for statistics
 * @sa CM_LoadMap (cmodel.c) */
int c_traces, c_brush_traces;

/* static */
static cBspSurface_t nullsurface;

/**
 * @note used as a shortcut so the tile being processed does not need to be repeatedly passed between functions.
 * @sa TR_MakeTracingNode (tracing.c)
 * @sa TR_MakeTracingNodes (tracing.c)
 * @sa TR_BuildTracingNode_r (tracing.c)
 * @sa TR_TestLine_r (tracing.c)
 * @sa TR_TestLineDist_r (tracing.c)
 * @sa TR_TestLineMask (tracing.c)
 * @sa TR_InitBoxHull (tracing.c)
 * @sa TR_TestLineDM (tracing.c)
 * @sa TR_BoxLeafnums_r (tracing.c)
 * @sa TR_BoxLeafnums_headnode (tracing.c)
 * @sa TR_TestBoxInBrush (tracing.c)
 * @sa TR_TraceToLeaf (tracing.c)
 * @sa TR_TestInLeaf (tracing.c)
 * @sa TR_RecursiveHullCheck (tracing.c)
 * @sa TR_BoxTrace (tracing.c)
 * @sa TR_TransformedBoxTrace (tracing.c)
 * @sa CMod_LoadSubmodels (cmodel.c)
 * @sa CMod_LoadSurfaces (cmodel.c)
 * @sa CMod_LoadNodes (cmodel.c)
 * @sa CMod_LoadBrushes (cmodel.c)
 * @sa CMod_LoadLeafs (cmodel.c)
 * @sa CMod_LoadPlanes (cmodel.c)
 * @sa CMod_LoadLeafBrushes (cmodel.c)
 * @sa CMod_LoadBrushSides (cmodel.c)
 * @sa CM_MakeTracingNodes (cmodel.c)
 * @sa CM_InitBoxHull (cmodel.c)
 * @sa CM_CompleteBoxTrace (cmodel.c)
 * @sa CMod_LoadRouting (cmodel.c)
 * @sa CM_AddMapTile (cmodel.c)
 */
TR_TILE_TYPE *curTile;

/** @note loaded map tiles with this assembly.  ufo2map has exactly 1.*/
TR_TILE_TYPE mapTiles[MAX_MAPTILES];

/** @note number of loaded map tiles (map assembly) */
int numTiles = 0;

/** @note For multi-check avoidance. */
static int checkcount;

/** @note used to hold the point on a line that an obstacle is encountered. */
static vec3_t tr_end;

static int leaf_count, leaf_maxcount;
static int *leaf_list;
static float *leaf_mins, *leaf_maxs;
static int leaf_topnode;

/*
static vec3_t trace_start, trace_end;
static vec3_t trace_mins, trace_maxs;
static vec3_t trace_extents;

static trace_t trace_trace;
static int trace_contents;
static int trace_rejects;
static qboolean trace_ispoint;			/ * optimized case * /
*/


tnode_t *tnode_p;

/*
===============================================================================
TRACING NODES
===============================================================================
*/

/**
 * @brief Converts the disk node structure into the efficient tracing structure
 */
static void TR_MakeTracingNode (int nodenum)
{
	tnode_t *t;
	TR_PLANE_TYPE *plane;
	int i;
	TR_NODE_TYPE *node;

	t = tnode_p++;

	node = curTile->nodes + nodenum;
#ifdef COMPILE_UFO
	plane = node->plane;
#else
	plane = curTile->planes + node->planenum;
#endif

	t->type = plane->type;
	VectorCopy(plane->normal, t->normal);
	t->dist = plane->dist;

	for (i = 0; i < 2; i++) {
		if (node->children[i] < 0) {
			const int index = -(node->children[i]) - 1;
			const TR_LEAF_TYPE *leaf = &curTile->leafs[index];
			const int contentFlags = leaf->contentFlags & ~(1 << 31);
			if ((contentFlags & MASK_IMPASSABLE) && !(contentFlags & CONTENTS_PASSABLE))
				t->children[i] = -node->children[i] | (1 << 31);
			else
				t->children[i] = (1 << 31);
		} else {
			t->children[i] = tnode_p - curTile->tnodes;
			if (t->children[i] > curTile->numnodes) {
				Com_Printf("Exceeded alloted memory for tracing structure.\n");
			}
			TR_MakeTracingNode(node->children[i]);
		}
	}
}

/**
 * @sa CMod_LoadNodes
 * @sa R_ModLoadNodes
 */
void TR_BuildTracingNode_r (int node, int level)
{
	assert(node < curTile->numnodes + 6); /* +6 => bbox */

	/**
	 *  We are checking for a leaf in the tracing node.  For ufo2map, planenum == PLANENUMLEAF.
	 *  For the game, plane will be NULL.
	 */
#ifdef COMPILE_UFO
	if (!curTile->nodes[node].plane) {
#else
	if (curTile->nodes[node].planenum == PLANENUM_LEAF) {
#endif
		TR_NODE_TYPE *n;
		tnode_t *t;
		vec3_t c0maxs, c1mins;
		int i;

		n = &curTile->nodes[node];

		/* alloc new node */
		t = tnode_p++;

		/* no leafs here */
		if (n->children[0] < 0 || n->children[1] < 0)
#ifdef COMPILE_UFO
			Com_Error(ERR_DROP, "Unexpected leaf");
#else
			Sys_Error("Unexpected leaf");
#endif

		VectorCopy(curTile->nodes[n->children[0]].maxs, c0maxs);
		VectorCopy(curTile->nodes[n->children[1]].mins, c1mins);

#if 0
		Com_Printf("(%i %i : %i %i) (%i %i : %i %i)\n",
			(int)curTile->nodes[n->children[0]].mins[0], (int)curTile->nodes[n->children[0]].mins[1],
			(int)curTile->nodes[n->children[0]].maxs[0], (int)curTile->nodes[n->children[0]].maxs[1],
			(int)curTile->nodes[n->children[1]].mins[0], (int)curTile->nodes[n->children[1]].mins[1],
			(int)curTile->nodes[n->children[1]].maxs[0], (int)curTile->nodes[n->children[1]].maxs[1]);
#endif

		for (i = 0; i < 2; i++)
			if (c0maxs[i] <= c1mins[i]) {
				/* create a separation plane */
				t->type = i;
				VectorSet(t->normal, i, (i ^ 1), 0);
				t->dist = (c0maxs[i] + c1mins[i]) / 2;

				t->children[1] = tnode_p - curTile->tnodes;
				TR_BuildTracingNode_r(n->children[0], level);
				t->children[0] = tnode_p - curTile->tnodes;
				TR_BuildTracingNode_r(n->children[1], level);
				return;
			}

		/* can't construct such a separation plane */
		t->type = PLANE_NONE;

		for (i = 0; i < 2; i++) {
			t->children[i] = tnode_p - curTile->tnodes;
			TR_BuildTracingNode_r(n->children[i], level);
		}
	} else {
		/* Make a lookup table for TR_CompleteBoxTrace */
		curTile->cheads[curTile->numcheads].cnode = node;
		curTile->cheads[curTile->numcheads].level = level;
		curTile->numcheads++;
		assert(curTile->numcheads <= MAX_MAP_NODES);
		/* Make the tracing node. */
		TR_MakeTracingNode(node);
	}
}


/*
===============================================================================
LINE TRACING - TEST FOR BRUSH PRESENCE
===============================================================================
*/


/**
 * @param[in] node Node index
 * @sa TR_TestLineDist_r
 * @sa CM_TestLine
 */
static int TR_TestLine_r (int node, const vec3_t start, const vec3_t stop)
{
	tnode_t *tnode;
	float front, back;
	int r;

	/* leaf node */
	if (node & (1 << 31))
		return node & ~(1 << 31);

	tnode = &curTile->tnodes[node];
	assert(tnode);
	switch (tnode->type) {
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		front = start[tnode->type] - tnode->dist;
		back = stop[tnode->type] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TR_TestLine_r(tnode->children[0], start, stop);
		if (r)
			return r;
		return TR_TestLine_r(tnode->children[1], start, stop);
	default:
		front = DotProduct(start, tnode->normal) - tnode->dist;
		back = DotProduct(stop, tnode->normal) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TR_TestLine_r(tnode->children[0], start, stop);
	else if (front < ON_EPSILON && back < ON_EPSILON)
		return TR_TestLine_r(tnode->children[1], start, stop);
	else {
		const int side = front < 0;
		const float frac = front / (front - back);
		vec3_t mid;

		VectorInterpolation(start, stop, frac, mid);

		r = TR_TestLine_r(tnode->children[side], start, mid);
		if (r)
			return r;
		return TR_TestLine_r(tnode->children[!side], mid, stop);
	}
}


/**
 * @brief Tests to see if a line intersects any brushes in a tile.
 * @param[in] tile The map tile containing the structures to be traced.
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] levelmask
 * @sa CL_TargetingToHit
 * @note levels:
 * 0-255: brushes are assigned to a level based on their assigned viewing levels.  A brush with
 *    no levels assigned will be stuck in 0, a brush viewable from all 8 levels will be in 255, and
 *    so on.  Each brush will only appear in one level.
 * 256: weaponclip-level
 * 257: actorclip-level
 */
static qboolean TR_TileTestLine (TR_TILE_TYPE *tile, const vec3_t start, const vec3_t stop, int levelmask)
{
	const int corelevels = (levelmask & TL_FLAG_REGULAR_LEVELS);
	int i;

	curTile = tile;
	/* loop over all theads */
	for (i = 0; i < curTile->numtheads; i++) {
		const int level = curTile->theadlevel[i];
		if (level && corelevels && !(level & levelmask))
			continue;
		if (level == LEVEL_ACTORCLIP && !(levelmask & TL_FLAG_ACTORCLIP))
			continue;
		if (level == LEVEL_WEAPONCLIP && !(levelmask & TL_FLAG_WEAPONCLIP))
			continue;
		if (TR_TestLine_r(curTile->thead[i], start, stop))
			return qtrue;
	}
	return qfalse;
}

#ifdef COMPILE_MAP
/**
 * @brief Checks traces against a single-tile map, optimized for ufo2map
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @sa TR_TestLine
 * @sa GatherSampleLight
 * @return qfalse if not blocked
 */
qboolean TR_TestLineSingleTile (const vec3_t start, const vec3_t stop)
{
	int i;
	static int lastthead = 0;

	assert(numTiles == 1);
	curTile = &mapTiles[0];

	/* ufo2map does many traces to the same endpoint.
	 * Often an occluding node will be found in the same thead
	 * as the last trace, so test that one first. */
	if (curTile->theadlevel[lastthead] != LEVEL_ACTORCLIP
	 && curTile->theadlevel[lastthead] != LEVEL_WEAPONCLIP
	 && TR_TestLine_r(curTile->thead[lastthead], start, stop))
		return qtrue;

	for (i = 0; i < curTile->numtheads; i++) {
		const int level = curTile->theadlevel[i];
		if (i == lastthead)
			continue;
		if (level == LEVEL_ACTORCLIP)
			continue;
		if (level == LEVEL_WEAPONCLIP)
			continue;
		if (TR_TestLine_r(curTile->thead[i], start, stop)) {
			lastthead = i;
			return qtrue;
		}
	}
	return qfalse;
}
#endif


/**
 * @brief Checks traces against the world
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] levelmask Indicates which special levels, if any, to include in the trace.
 * @note Special levels are LEVEL_ACTORCLIP and LEVEL_WEAPONCLIP.
 * @sa TR_TestLine_r
 * @sa CL_TargetingToHit
 * @return qfalse if not blocked
 */
qboolean TR_TestLine (const vec3_t start, const vec3_t stop, const int levelmask)
{
	int tile;

	for (tile = 0; tile < numTiles; tile++) {
		if (TR_TileTestLine(&mapTiles[tile], start, stop, levelmask))
			return qtrue;
	}
	return qfalse;
}


/*
===============================================================================
LINE TRACING - TEST FOR BRUSH LOCATION
===============================================================================
*/


/**
 * @param[in] node Node index
 * @sa TR_TestLine_r
 * @sa TR_TestLineDM
 */
static int TR_TestLineDist_r (int node, const vec3_t start, const vec3_t stop)
{
	tnode_t *tnode;
	float front, back;
	vec3_t mid;
	float frac;
	int side;
	int r;

	if (node & (1 << 31)) {
		r = node & ~(1 << 31);
		if (r)
			VectorCopy(start, tr_end);
		return r;				/* leaf node */
	}

	tnode = &curTile->tnodes[node];
	assert(tnode);
	switch (tnode->type) {
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		front = start[tnode->type] - tnode->dist;
		back = stop[tnode->type] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TR_TestLineDist_r(tnode->children[0], start, stop);
		if (r)
			VectorCopy(tr_end, mid);
		side = TR_TestLineDist_r(tnode->children[1], start, stop);
		if (side && r) {
			if (VectorNearer(mid, tr_end, start)) {
				VectorCopy(mid, tr_end);
				return r;
			} else {
				return side;
			}
		}

		if (r) {
			VectorCopy(mid, tr_end);
			return r;
		}

		return side;
	default:
		front = (start[0] * tnode->normal[0] + start[1] * tnode->normal[1] + start[2] * tnode->normal[2]) - tnode->dist;
		back = (stop[0] * tnode->normal[0] + stop[1] * tnode->normal[1] + stop[2] * tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TR_TestLineDist_r(tnode->children[0], start, stop);

	if (front < ON_EPSILON && back < ON_EPSILON)
		return TR_TestLineDist_r(tnode->children[1], start, stop);

	side = front < 0;

	frac = front / (front - back);

	mid[0] = start[0] + (stop[0] - start[0]) * frac;
	mid[1] = start[1] + (stop[1] - start[1]) * frac;
	mid[2] = start[2] + (stop[2] - start[2]) * frac;

	r = TR_TestLineDist_r(tnode->children[side], start, mid);
	if (r)
		return r;
	return TR_TestLineDist_r(tnode->children[!side], mid, stop);
}

/**
 * @brief Checks traces against the world, gives hit position back
 * @param[in] tile The map tile containing the structures to be traced.
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] end The position where the trace hits a object or the stop position if nothing is in the line.
 * @sa TR_TestLineDM
 * @sa CL_ActorMouseTrace
 * @return qfalse if no connection between start and stop - 1 otherwise
 */
static qboolean TR_TileTestLineDM (TR_TILE_TYPE *tile, const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask)
{
	const int corelevels = (levelmask & TL_FLAG_REGULAR_LEVELS);
	int i;

	curTile = tile;

	VectorCopy(stop, end);

	for (i = 0; i < tile->numtheads; i++) {
		const int level = tile->theadlevel[i];
		if (level && corelevels && !(level & levelmask))
			continue;
		if (level == LEVEL_ACTORCLIP && !(levelmask & TL_FLAG_ACTORCLIP))
			continue;
		if (level == LEVEL_WEAPONCLIP && !(levelmask & TL_FLAG_WEAPONCLIP))
			continue;
		if (TR_TestLineDist_r(tile->thead[i], start, stop))
			if (VectorNearer(tr_end, end, start))
				VectorCopy(tr_end, end);
	}

	if (VectorCompareEps(end, stop, EQUAL_EPSILON))
		return qfalse;
	else
		return qtrue;
}


/**
 * @brief Checks traces against the world, gives hit position back
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] end The position where the trace hits a object or the stop position if nothing is in the line.
 * @sa TR_TestLineDM
 * @sa CL_ActorMouseTrace
 * @return qfalse if no connection between start and stop - 1 otherwise
 */
qboolean TR_TestLineDM (const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask)
{
	int tile;
	vec3_t t_end;

	VectorCopy(stop, end);
	VectorCopy(stop, t_end);

	for (tile = 0; tile < numTiles; tile++) {
		if (TR_TileTestLineDM(&mapTiles[tile], start, stop, t_end, levelmask)) {
			if (VectorNearer(t_end, end, start))
				VectorCopy(t_end, end);
		}
	}

	if (VectorCompareEps(end, stop, EQUAL_EPSILON))
		return qfalse;
	else
		return qtrue;
}


/*
===============================================================================
BOX TRACING
===============================================================================
*/


/**
 * @brief Returns PSIDE_FRONT, PSIDE_BACK, or PSIDE_BOTH
 */
int TR_BoxOnPlaneSide (const vec3_t mins, const vec3_t maxs, const TR_PLANE_TYPE *plane)
{
	int side, i;
	vec3_t corners[2];
	vec_t dist1, dist2;

	/* axial planes are easy */
	if (AXIAL(plane)) {
		side = 0;
		if (maxs[plane->type] > plane->dist + PLANESIDE_EPSILON)
			side |= PSIDE_FRONT;
		if (mins[plane->type] < plane->dist - PLANESIDE_EPSILON)
			side |= PSIDE_BACK;
		return side;
	}

	/* create the proper leading and trailing verts for the box */

	for (i = 0; i < 3; i++) {
		if (plane->normal[i] < 0) {
			corners[0][i] = mins[i];
			corners[1][i] = maxs[i];
		} else {
			corners[1][i] = mins[i];
			corners[0][i] = maxs[i];
		}
	}

	dist1 = DotProduct(plane->normal, corners[0]) - plane->dist;
	dist2 = DotProduct(plane->normal, corners[1]) - plane->dist;
	side = 0;
	if (dist1 >= PLANESIDE_EPSILON)
		side = PSIDE_FRONT;
	if (dist2 < PLANESIDE_EPSILON)
		side |= PSIDE_BACK;

	return side;
}



/**
 * @brief To keep everything totally uniform, bounding boxes are turned into small
 * BSP trees instead of being compared directly.
 */
int TR_HeadnodeForBox (mapTile_t *tile, const vec3_t mins, const vec3_t maxs)
{
	tile->box_planes[0].dist = maxs[0];
	tile->box_planes[1].dist = -maxs[0];
	tile->box_planes[2].dist = mins[0];
	tile->box_planes[3].dist = -mins[0];
	tile->box_planes[4].dist = maxs[1];
	tile->box_planes[5].dist = -maxs[1];
	tile->box_planes[6].dist = mins[1];
	tile->box_planes[7].dist = -mins[1];
	tile->box_planes[8].dist = maxs[2];
	tile->box_planes[9].dist = -maxs[2];
	tile->box_planes[10].dist = mins[2];
	tile->box_planes[11].dist = -mins[2];

	assert(tile->box_headnode < MAX_MAP_NODES);
	return tile->box_headnode;
}

/**
 * @brief Fills in a list of all the leafs touched
 * call with topnode set to the headnode, returns with topnode
 * set to the first node that splits the box
 */
static void TR_BoxLeafnums_r (boxtrace_t *trace_data, int nodenum)
{
	TR_PLANE_TYPE *plane;
	TR_NODE_TYPE *node;
	int s;
	TR_TILE_TYPE *myTile = trace_data->tile;

	while (1) {
		if (nodenum <= LEAFNODE) {
			if (leaf_count >= leaf_maxcount) {
/*				Com_Printf("CM_BoxLeafnums_r: overflow\n"); */
				return;
			}
			leaf_list[leaf_count++] = -1 - nodenum;
			return;
		}

		assert(nodenum < myTile->numnodes + 6); /* +6 => bbox */
		node = &myTile->nodes[nodenum];
#ifdef COMPILE_UFO
		plane = node->plane;
#else
		plane = myTile->planes + node->planenum;
#endif

		s = TR_BoxOnPlaneSide(leaf_mins, leaf_maxs, plane);
		if (s == PSIDE_FRONT)
			nodenum = node->children[0];
		else if (s == PSIDE_BACK)
			nodenum = node->children[1];
		else {					/* go down both */
			if (leaf_topnode == LEAFNODE)
				leaf_topnode = nodenum;
			TR_BoxLeafnums_r(trace_data, node->children[0]);
			nodenum = node->children[1];
		}
	}
}

/**
 * @param[in] headnode if < 0 we are in a leaf node
 */
static int TR_BoxLeafnums_headnode (boxtrace_t *trace_data, vec3_t mins, vec3_t maxs, int *list, int listsize, int headnode, int *topnode)
{
	TR_TILE_TYPE *myTile = trace_data->tile;

	leaf_list = list;
	leaf_count = 0;
	leaf_maxcount = listsize;
	leaf_mins = mins;
	leaf_maxs = maxs;

	leaf_topnode = LEAFNODE;

	assert(headnode < myTile->numnodes + 6); /* +6 => bbox */
	TR_BoxLeafnums_r(trace_data, headnode);

	if (topnode)
		*topnode = leaf_topnode;

	return leaf_count;
}


/**
 * @param[in] mins min vector of bounding box around the line from p1 to p2
 * @param[in] maxs max vector of bounding box around the line from p1 to p2
 * @param[in] p1 start vector
 * @param[in] p2 end vector
 * @param[in,out] trace the location of the last hit on the line, adjusted if this hit is closer.
 * @param[in] brush the brush that is being examined
 * @brief This function checks to see if any sides of a brush intersect the line from p1 to p2 or are located within
 *  the perpendicular bounding box from mins to maxs originating from the line. It also check to see if the line
 *  originates from inside the brush, terminates inside the brush, or is completely contained within the brush.
 */
static void TR_ClipBoxToBrush (boxtrace_t *trace_data, cBspBrush_t *brush, TR_LEAF_TYPE *leaf)
{
	int i, j;
	TR_PLANE_TYPE *clipplane;
	int clipplanenum;
	float dist;
	float enterfrac, leavefrac;
	vec3_t ofs;
	float d1, d2;
	qboolean getout, startout;
	TR_BRUSHSIDE_TYPE *leadside;
	TR_TILE_TYPE *myTile = trace_data->tile;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if (!brush || !brush->numsides)
		return;

	c_brush_traces++;

	getout = qfalse;
	startout = qfalse;
	leadside = NULL;
	clipplanenum = 0;

	for (i = 0; i < brush->numsides; i++) {
		TR_BRUSHSIDE_TYPE *side = &myTile->brushsides[brush->firstbrushside + i];
#ifdef COMPILE_UFO
		TR_PLANE_TYPE *plane = side->plane;
#else
		TR_PLANE_TYPE *plane = myTile->planes + side->planenum;
#endif

		/** @todo special case for axial */
		if (!trace_data->ispoint) {	/* general box case */
			/* push the plane out appropriately for mins/maxs */
			for (j = 0; j < 3; j++) {
				if (plane->normal[j] < 0)
					ofs[j] = trace_data->maxs[j];
				else
					ofs[j] = trace_data->mins[j];
			}
			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		} else {				/* special point case */
			dist = plane->dist;
		}

		d1 = DotProduct(trace_data->start, plane->normal) - dist;
		d2 = DotProduct(trace_data->end, plane->normal) - dist;

		if (d2 > 0)
			getout = qtrue;		/* endpoint is not in solid */
		if (d1 > 0)
			startout = qtrue;	/* startpoint is not in solid */

		/* if completely in front of face, no intersection */
		if (d1 > 0 && d2 >= d1)
			return;

		if (d1 <= 0 && d2 <= 0)
			continue;

		/* crosses face */
		if (d1 > d2) {			/* enter */
			const float f = (d1 - DIST_EPSILON) / (d1 - d2);
			if (f > enterfrac) {
				enterfrac = f;
				clipplane = plane;
#ifdef COMPILE_MAP
				clipplanenum = side->planenum;
#endif
				leadside = side;
			}
		} else {				/* leave */
			const float f = (d1 + DIST_EPSILON) / (d1 - d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	/*
	if (!trace->trace)
		return;
	*/

	if (!startout) {			/* original point was inside brush */
		trace_data->trace.startsolid = qtrue;
		if (!getout)
			trace_data->trace.allsolid = qtrue;
		trace_data->trace.leafnum = leaf - curTile->leafs;
		return;
	}
	if (enterfrac < leavefrac) {
		if (enterfrac > -1 && enterfrac < trace_data->trace.fraction) {
			if (enterfrac < 0)
				enterfrac = 0;
			trace_data->trace.fraction = enterfrac;
			trace_data->trace.plane = *clipplane;
			trace_data->trace.planenum = clipplanenum;
#ifdef COMPILE_UFO
			trace_data->trace.surface = leadside->surface;
#endif
			trace_data->trace.contentFlags = brush->contentFlags;
			trace_data->trace.leafnum = leaf - curTile->leafs;
		}
	}
}

/**
 * @sa CM_TraceToLeaf
 */
static void TR_TestBoxInBrush (boxtrace_t *trace_data, cBspBrush_t * brush)
{
	int i, j;
	TR_PLANE_TYPE *plane;
	float dist;
	vec3_t ofs;
	float d1;
	TR_BRUSHSIDE_TYPE *side;
	TR_TILE_TYPE *myTile = trace_data->tile;

	if (!brush || !brush->numsides)
		return;

	for (i = 0; i < brush->numsides; i++) {
		side = &myTile->brushsides[brush->firstbrushside + i];
#ifdef COMPILE_UFO
		plane = side->plane;
#else
		plane = myTile->planes + side->planenum;
#endif

		/** @todo special case for axial */
		/* general box case */
		/* push the plane out appropriately for mins/maxs */
		for (j = 0; j < 3; j++) {
			if (plane->normal[j] < 0)
				ofs[j] = trace_data->maxs[j];
			else
				ofs[j] = trace_data->mins[j];
		}
		dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct(trace_data->start, plane->normal) - dist;

		/* if completely in front of face, no intersection */
		if (d1 > 0)
			return;
	}

	/* inside this brush */
	trace_data->trace.startsolid = trace_data->trace.allsolid = qtrue;
	trace_data->trace.fraction = 0;
	trace_data->trace.contentFlags = brush->contentFlags;
}


/**
 * @param[in] leafnum the leaf index that we are looking in for a hit against
 * @sa trace_contents (set in TR_BoxTrace)
 * @sa CM_ClipBoxToBrush
 * @sa CM_TestBoxInBrush
 * @sa CM_RecursiveHullCheck
 * @brief This function checks if the specified leaf matches any mask specified in trace_contents.  If so, each brush
 *  in the leaf is examined to see if it is intersected by the line drawn in TR_RecursiveHullCheck or is within the
 *  bounding box set in trace_mins and trace_maxs with the origin on the line.
 */
static void TR_TraceToLeaf (boxtrace_t *trace_data, int leafnum)
{
	int k;
	TR_LEAF_TYPE *leaf;

	assert(leafnum > LEAFNODE);
	assert(leafnum <= curTile->numleafs);

	leaf = &curTile->leafs[leafnum];
	if (!(leaf->contentFlags & trace_data->contents) || (leaf->contentFlags & trace_data->rejects))
		return;

	/* trace line against all brushes in the leaf */
	for (k = 0; k < leaf->numleafbrushes; k++) {
		const int brushnum = curTile->leafbrushes[leaf->firstleafbrush + k];
		cBspBrush_t *b = &curTile->brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;			/* already checked this brush in another leaf */
		b->checkcount = checkcount;

		if (!(b->contentFlags & trace_data->contents) || (b->contentFlags & trace_data->rejects))
			continue;

		TR_ClipBoxToBrush(trace_data, b, leaf);
		if (!trace_data->trace.fraction)
			return;
	}
}


/**
 * @sa CM_TestBoxInBrush
 */
static void TR_TestInLeaf (boxtrace_t *trace_data, int leafnum)
{
	int k;
	const TR_LEAF_TYPE *leaf;
	TR_TILE_TYPE *myTile = trace_data->tile;

	assert(leafnum > LEAFNODE);
	assert(leafnum <= myTile->numleafs);

	leaf = &myTile->leafs[leafnum];
	if (!(leaf->contentFlags & trace_data->contents) || (leaf->contentFlags & trace_data->rejects))
		return;

	/* trace line against all brushes in the leaf */
	for (k = 0; k < leaf->numleafbrushes; k++) {
		const int brushnum = myTile->leafbrushes[leaf->firstleafbrush + k];
		cBspBrush_t *b = &myTile->brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;			/* already checked this brush in another leaf */
		b->checkcount = checkcount;

		if (!(b->contentFlags & trace_data->contents) || (b->contentFlags & trace_data->rejects))
			continue;
		TR_TestBoxInBrush(trace_data, b);
		if (!trace_data->trace.fraction)
			return;
	}
}


/**
 * @param[in] num the node index that we are looking in for a hit
 * @param[in] p1f based on the original line, the fraction traveled to reach the start vector
 * @param[in] p2f based on the original line, the fraction traveled to reach the end vector
 * @param[in] p1 start vector
 * @param[in] p2 end vector
 * @sa CM_BoxTrace
 * @sa CM_TraceToLeaf
 * @brief This recursive function traces through the bsp tree looking to see if a brush can be
 *  found that intersects the line from p1 to p2, including a bounding box (plane) offset that
 *  is perpendicular to the line.  If the node of the tree is a leaf, the leaf is checked.  If
 *  not, it is determined which side(s) of the tree need to be traversed, and this function is
 *  called again.  The bounding box mentioned earlier is set in TR_BoxTrace, and propagated
 *  using trace_extents.  Trace_extents is specifically how far from the line a bsp node needs
 *  to be in order to be included or excluded in the search.
 */
static void TR_RecursiveHullCheck (boxtrace_t *trace_data, int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2)
{
	TR_NODE_TYPE *node;
	TR_PLANE_TYPE *plane;
	float t1, t2, offset;
	float frac, frac2;
	int side;
	float midf;
	vec3_t mid;
	trace_t *trace = &trace_data->trace;
	TR_TILE_TYPE *myTile = trace_data->tile;

	if (trace->fraction <= p1f)
		return;					/* already hit something nearer */

	/* if < 0, we are in a leaf node */
	if (num <= LEAFNODE) {
		TR_TraceToLeaf(trace_data, LEAFNODE - num);
		return;
	}

	assert(num < MAX_MAP_NODES);

	/* find the point distances to the seperating plane
	 * and the offset for the size of the box */
	node = myTile->nodes + num;
#ifdef COMPILE_UFO
	plane = node->plane;
#else
	assert(node->planenum < MAX_MAP_PLANES);
	plane = myTile->planes + node->planenum;
#endif

	if (AXIAL(plane)) {
		const int type = plane->type;
		t1 = p1[type] - plane->dist;
		t2 = p2[type] - plane->dist;
		offset = trace_data->extents[type];
	} else {
		const float dist = plane->dist;
		t1 = DotProduct(plane->normal, p1) - dist;
		t2 = DotProduct(plane->normal, p2) - dist;
		if (trace_data->ispoint)
			offset = 0;
		else
			offset = fabs(trace_data->extents[0] * plane->normal[0])
				+ fabs(trace_data->extents[1] * plane->normal[1])
				+ fabs(trace_data->extents[2] * plane->normal[2]);
	}

	/* see which sides we need to consider */
	if (t1 >= offset && t2 >= offset) {
		TR_RecursiveHullCheck(trace_data, node->children[0], p1f, p2f, p1, p2);
		return;
	} else if (t1 < -offset && t2 < -offset) {
		TR_RecursiveHullCheck(trace_data, node->children[1], p1f, p2f, p1, p2);
		return;
	}

	/* put the crosspoint DIST_EPSILON pixels on the near side */
	if (t1 < t2) {
		const float idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	} else if (t1 > t2) {
		const float idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	/* move up to the node */
	if (frac < 0)
		frac = 0;
	else if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f) * frac;
	VectorInterpolation(p1, p2, midf, mid);
	TR_RecursiveHullCheck(trace_data, node->children[side], p1f, midf, p1, mid);

	/* go past the node */
	if (frac2 < 0)
		frac2 = 0;
	else if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f) * frac2;
	VectorInterpolation(p1, p2, midf, mid);
	TR_RecursiveHullCheck(trace_data, node->children[side ^ 1], midf, p2f, mid, p2);
}

/**
 * @param[in] start trace start vector
 * @param[in] end trace end vector
 * @param[in] mins box mins
 * @param[in] maxs box maxs
 * @param[in] tile Tile to check (normally 0 - except in assembled maps)
 * @param[in] headnode if < 0 we are in a leaf node
 * @param[in] brushmask brushes the trace should stop at (see MASK_*)
 * @sa TR_TransformedBoxTrace
 * @sa TR_CompleteBoxTrace
 * @sa TR_RecursiveHullCheck
 * @sa TR_BoxLeafnums_headnode
 * @brief This function traces a line from start to end.  It returns a trace_t indicating what portion of the line
 *  can be traveled from start to end before hitting a brush that meets the criteria in brushmask.  The point that
 *  this line intersects that brush is also returned.
 *  There is a special case when start and end are the same vector.  In this case, the bounding box formed by mins
 *  and maxs offset from start is examined for any brushes that meet the criteria.  The first brush found inside
 *  the bounding box is returned.
 *  There is another special case when mins and maxs are both origin vectors (0, 0, 0).  In this case, the
 */
trace_t TR_BoxTrace (const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, TR_TILE_TYPE *tile, int headnode, int brushmask, int brushreject)
{
	int i;
	vec3_t offset, amins, amaxs, astart, aend;
	boxtrace_t trace_data;

	checkcount++;	/* for multi-check avoidance */
	c_traces++;		/* for statistics, may be zeroed */

	assert(headnode < tile->numnodes + 6); /* +6 => bbox */

	/* fill in a default trace */
	memset(&trace_data.trace, 0, sizeof(trace_data.trace));
	trace_data.trace.fraction = 1;
	trace_data.trace.surface = &(nullsurface);

	if (!tile->numnodes)		/* map not loaded */
		return trace_data.trace;

	/* Optimize the trace by moving the line to be traced across into the origin of the box trace. */
	/* Calculate the offset needed to center the trace about the line */
	VectorAdd(mins, maxs, offset);
	VectorDiv(offset, 2, offset);

	/* Now remove the offset from bmin and bmax (effectively centering the trace box about the origin)
	 * and add the offset to the trace line (effectively repositioning the trace box at the desired coordinates) */
	VectorSubtract(mins, offset, amins);
	VectorSubtract(maxs, offset, amaxs);
	VectorAdd(start, offset, astart);
	VectorAdd(end, offset, aend);

	trace_data.contents = brushmask;
	trace_data.rejects = brushreject;
	trace_data.tile = tile;
	VectorCopy(astart, trace_data.start);
	VectorCopy(aend, trace_data.end);
	VectorCopy(amins, trace_data.mins);
	VectorCopy(amaxs, trace_data.maxs);

	/* check for position test special case */
	if (VectorCompare(astart, aend)) {
		int leafs[MAX_LEAFS];
		int numleafs;
		vec3_t c1, c2;
		int topnode;

		VectorAdd(astart, amins, c1);
		VectorAdd(astart, amaxs, c2);
		for (i = 0; i < 3; i++) {
			/* expand the box by 1 */
			c1[i] -= 1;
			c2[i] += 1;
		}

		numleafs = TR_BoxLeafnums_headnode(&trace_data, c1, c2, leafs, MAX_LEAFS, headnode, &topnode);
		for (i = 0; i < numleafs; i++) {
			TR_TestInLeaf(&trace_data, leafs[i]);
			if (trace_data.trace.allsolid)
				break;
		}
		VectorCopy(start, trace_data.trace.endpos);
		return trace_data.trace;
	}

	/* check for point special case */
	if (VectorCompare(amins, vec3_origin) && VectorCompare(amaxs, vec3_origin)) {
		trace_data.ispoint = qtrue;
		VectorClear(trace_data.extents);
	} else {
		trace_data.ispoint = qfalse;
		trace_data.extents[0] = -amins[0] > amaxs[0] ? -amins[0] : amaxs[0];
		trace_data.extents[1] = -amins[1] > amaxs[1] ? -amins[1] : amaxs[1];
		trace_data.extents[2] = -amins[2] > amaxs[2] ? -amins[2] : amaxs[2];
	}

	/* general sweeping through world */
	TR_RecursiveHullCheck(&trace_data, headnode, 0, 1, astart, aend);

	if (trace_data.trace.fraction == 1.0) {
		VectorCopy(aend, trace_data.trace.endpos);
	} else {
		VectorInterpolation(trace_data.start, trace_data.end, trace_data.trace.fraction, trace_data.trace.endpos);
	}
	/* Now un-offset the end position. */
	VectorSubtract(trace_data.trace.endpos, offset, trace_data.trace.endpos);
	return trace_data.trace;
}

/**
 * @brief Handles offseting and rotation of the end points for moving and rotating entities
 * @sa CM_BoxTrace
 */
trace_t TR_TransformedBoxTrace (const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, TR_TILE_TYPE *tile, int headnode, int brushmask, int brushreject, const vec3_t origin, const vec3_t angles)
{
	trace_t trace;
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;
	qboolean rotated;

	/* init */
	curTile = tile;

	/* subtract origin offset */
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	/* rotate start and end into the models frame of reference */
	if (headnode != curTile->box_headnode && VectorNotEmpty(angles)) {
		rotated = qtrue;
	} else {
		rotated = qfalse;
	}

	if (rotated) {
		AngleVectors(angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	if (headnode >= curTile->numnodes + 6) {/* +6 => bbox */
		Com_Printf("TR_TransformedBoxTrace: headnode: %i, curTile->numnodes: %i\n", headnode, curTile->numnodes + 6);
		headnode = 0;
	}

	/* sweep the box through the model */
	trace = TR_BoxTrace(start_l, end_l, mins, maxs, tile, headnode, brushmask, brushreject);
	trace.mapTile = mapTiles - tile;

	if (rotated && trace.fraction != 1.0) {
		/** @todo figure out how to do this with existing angles */
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	trace.endpos[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.endpos[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.endpos[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

/**
 * @brief Handles all 255 level specific submodels too
 */
trace_t TR_CompleteBoxTrace (const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, int levelmask, int brushmask, int brushreject)
{
	trace_t newtr, tr;
	int tile, i;
	cBspHead_t *h;

	memset(&tr, 0, sizeof(tr));
	tr.fraction = 2.0f;

	/* trace against all loaded map tiles */
	for (tile = 0; tile < numTiles; tile++) {
		curTile = &mapTiles[tile];
		for (i = 0, h = curTile->cheads; i < curTile->numcheads; i++, h++) {
			/** @todo Is this levelmask supposed to limit by game level (1-8)
			 *  or map level (0-LEVEL_ACTORCLIP)?
			 * @brief This code uses levelmask to limit by maplevel.  Supposedly maplevels 1-255
			 * are bitmasks of game levels 1-8.  0 is a special case repeat fo 255.
			 * However a levelmask including 0x100 is usually included so the CLIP levels are
			 * examined.
			 * @todo @note that LEVEL_STEPON should not be available at this point, but may be erroneously
			 * included in another level, requiring the addition ot the brushreject parameter.
			 * @todo the above is no longer true - clean up here
			 */
			if (h->level && levelmask && !(h->level & levelmask))
				continue;

			assert(h->cnode < curTile->numnodes + 6); /* +6 => bbox */
			newtr = TR_BoxTrace(start, end, mins, maxs, &mapTiles[tile], h->cnode, brushmask, brushreject);
			newtr.mapTile = tile;

			/* memorize the trace with the minimal fraction */
			if (newtr.fraction == 0.0)
				return newtr;
			if (newtr.fraction < tr.fraction)
				tr = newtr;
		}
	}
	return tr;
}
