/**
 * @file
 * @brief model tracing and bounding
 * @note collision detection code
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "tracing.h"
#include "common.h"

/* TR_TILE_TYPE	and TR_PLANE_TYPE are defined in tracing.h */
#if defined(COMPILE_MAP)
  #define TR_NODE_TYPE			dBspNode_t
  #define TR_LEAF_TYPE			dBspLeaf_t
  #define TR_BRUSHSIDE_TYPE		dBspBrushSide_t
#elif defined(COMPILE_UFO)
  #define TR_NODE_TYPE			cBspNode_t
  #define TR_LEAF_TYPE			cBspLeaf_t
  #define TR_BRUSHSIDE_TYPE		cBspBrushSide_t
#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif
/** @note all the above types are declared in typedefs.h */

void boxtrace_s::init (TR_TILE_TYPE* _tile, const int contentmask, const int brushreject, const float fraction)
{
	trace.init();
	trace.surface = nullptr;
	contents = contentmask;
	rejects = brushreject;
	tile = _tile;
	trace.fraction = std::min(fraction, 1.0f);
}

/* Optimize the trace by moving the line to be traced across into the origin of the box trace. */
void boxtrace_s::setLineAndBox(const Line& line, const AABB& box)
{
	/* Calculate the offset needed to center the trace about the line */
	box.getCenter(offset);

	/* Now remove the offset from bmin and bmax (effectively centering the trace box about the origin of the line)
	 * and add the offset to the trace line (effectively repositioning the trace box at the desired coordinates) */
	VectorSubtract(box.mins, offset, this->mins);
	VectorSubtract(box.maxs, offset, this->maxs);
	VectorAdd(line.start, offset, this->start);
	VectorAdd(line.stop, offset, this->end);
}

/** @note For multi-check avoidance.
 * @todo not thread safe */
static int checkcount;

/*
===============================================================================
TRACING NODES
===============================================================================
*/

/**
 * @brief Converts the disk node structure into the efficient tracing structure for LineTraces
 */
static void TR_MakeTracingNode (TR_TILE_TYPE* tile, tnode_t**  tnode, int32_t nodenum)
{
	tnode_t* t = (*tnode)++;		/* the tracing node to build */
	TR_NODE_TYPE* node = tile->nodes + nodenum;		/* the node we are investigating */

	TR_PLANE_TYPE* plane;
#ifdef COMPILE_UFO
	plane = node->plane;
#else
	plane = tile->planes + node->planenum;
#endif

	t->type = plane->type;
	VectorCopy(plane->normal, t->normal);
	t->dist = plane->dist;

	for (int i = 0; i < 2; i++) {
		if (node->children[i] < 0) {	/* is it a leaf ? */
			const int32_t leafnum = -(node->children[i]) - 1;
			const TR_LEAF_TYPE* leaf = &tile->leafs[leafnum];
			const uint32_t contentFlags = leaf->contentFlags & ~(1 << 31);
			if ((contentFlags & (CONTENTS_SOLID | MASK_CLIP)) && !(contentFlags & CONTENTS_PASSABLE))
				t->children[i] = -node->children[i] | (1 << 31);	/* mark as 'blocking' */
			else
				t->children[i] = (1 << 31);				/* mark as 'empty leaf' */
		} else {										/* not a leaf */
			t->children[i] = *tnode - tile->tnodes;
			if (t->children[i] > tile->numnodes) {
				Com_Printf("Exceeded allocated memory for tracing structure (%i > %i)\n",
						t->children[i], tile->numnodes);
			}
			TR_MakeTracingNode(tile, tnode, node->children[i]);		/* recurse further down the tree */
		}
	}
}

/**
 * @sa CMod_LoadNodes
 * @sa R_ModLoadNodes
 */
void TR_BuildTracingNode_r (TR_TILE_TYPE* tile, tnode_t** tnode, int32_t nodenum, int level)
{
	assert(nodenum < tile->numnodes + 6); /* +6 => bbox */

	/**
	 *  We are checking for a leaf in the tracing node.  For ufo2map, planenum == PLANENUMLEAF.
	 *  For the game, plane will be nullptr.
	 */
#ifdef COMPILE_UFO
	if (!tile->nodes[nodenum].plane) {
#else
	if (tile->nodes[nodenum].planenum == PLANENUM_LEAF) {
#endif
		TR_NODE_TYPE* n = &tile->nodes[nodenum];

		/* alloc new node */
		tnode_t* t = (*tnode)++;

		/* no leafs here */
		if (n->children[0] < 0 || n->children[1] < 0)
#ifdef COMPILE_UFO
			Com_Error(ERR_DROP, "Unexpected leaf");
#else
			Sys_Error("Unexpected leaf");
#endif

		vec3_t c0maxs, c1mins;
		VectorCopy(tile->nodes[n->children[0]].maxs, c0maxs);
		VectorCopy(tile->nodes[n->children[1]].mins, c1mins);

#if 0
		Com_Printf("(%i %i : %i %i) (%i %i : %i %i)\n",
			(int)tile->nodes[n->children[0]].mins[0], (int)tile->nodes[n->children[0]].mins[1],
			(int)tile->nodes[n->children[0]].maxs[0], (int)tile->nodes[n->children[0]].maxs[1],
			(int)tile->nodes[n->children[1]].mins[0], (int)tile->nodes[n->children[1]].mins[1],
			(int)tile->nodes[n->children[1]].maxs[0], (int)tile->nodes[n->children[1]].maxs[1]);
#endif

		int i;
		for (i = 0; i < 2; i++)
			if (c0maxs[i] <= c1mins[i]) {
				/* create a separation plane */
				t->type = i;
				VectorSet(t->normal, i, (i ^ 1), 0);
				t->dist = (c0maxs[i] + c1mins[i]) / 2;

				t->children[1] = *tnode - tile->tnodes;
				TR_BuildTracingNode_r(tile, tnode, n->children[0], level);
				t->children[0] = *tnode - tile->tnodes;
				TR_BuildTracingNode_r(tile, tnode, n->children[1], level);
				return;
			}

		/* can't construct such a separation plane */
		t->type = PLANE_NONE;

		for (i = 0; i < 2; i++) {
			t->children[i] = *tnode - tile->tnodes;
			TR_BuildTracingNode_r(tile, tnode, n->children[i], level);
		}
	} else {
		/* Make a lookup table */
		tile->cheads[tile->numcheads].cnode = nodenum;
		tile->cheads[tile->numcheads].level = level;
		tile->numcheads++;
		assert(tile->numcheads <= MAX_MAP_NODES);
		/* Make the tracing node. */
		TR_MakeTracingNode(tile, tnode, nodenum);
	}
}


/*
===============================================================================
LINE TRACING - TEST FOR BRUSH PRESENCE
===============================================================================
*/


/**
 * @param[in] tile The map tile containing the structures to be traced.
 * @param[in] nodenum Node index
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @return zero if the line is not blocked, else a positive value
 * @sa TR_TestLineDist_r
 * @sa CM_TestLine
 */
int TR_TestLine_r (TR_TILE_TYPE* tile, int32_t nodenum, const vec3_t start, const vec3_t end)
{
	tnode_t* tnode;
	float front, back;
	int r;

	/* negative numbers indicate leaf nodes. Empty leaf nodes are marked as (1 << 31).
	 * Turning off that bit makes us return 0 or the positive node number to indicate blocking. */
	if (nodenum & (1 << 31))
		return nodenum & ~(1 << 31);

	tnode = &tile->tnodes[nodenum];
	assert(tnode);
	switch (tnode->type) {
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		front = start[tnode->type] - tnode->dist;
		back = end[tnode->type] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TR_TestLine_r(tile, tnode->children[0], start, end);
		if (r)
			return r;
		return TR_TestLine_r(tile, tnode->children[1], start, end);
	default:
		front = DotProduct(start, tnode->normal) - tnode->dist;
		back = DotProduct(end, tnode->normal) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TR_TestLine_r(tile, tnode->children[0], start, end);
	else if (front < ON_EPSILON && back < ON_EPSILON)
		return TR_TestLine_r(tile, tnode->children[1], start, end);
	else {
		const int side = front < 0;
		const float frac = front / (front - back);
		vec3_t mid;

		VectorInterpolation(start, end, frac, mid);

		r = TR_TestLine_r(tile, tnode->children[side], start, mid);
		if (r)
			return r;
		return TR_TestLine_r(tile, tnode->children[!side], mid, end);
	}
}


/**
 * @brief Tests to see if a line intersects any brushes in a tile.
 * @param[in] tile The map tile containing the structures to be traced.
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[in] levelmask
 * @return true if the line is blocked
 * @note This function uses levels and levelmasks.  The levels are as following:
 * 0-255: brushes are assigned to a level based on their assigned viewing levels.  A brush with
 *    no levels assigned will be stuck in 0, a brush viewable from all 8 levels will be in 255, and
 *    so on.  Each brush will only appear in one level.
 * 256: weaponclip-level
 * 257: actorclip-level
 *
 * The levelmask is used to determine which levels AND which, if either, clip to trace through.
 * The mask bits are as follows:
 * 0x0FF: Level bits.  If any bits are set then a brush's level ANDed with the levelmask, then
 *     that level is traced.  It could possibly be used to speed up traces.
 * 0x100: Actorclip bit.  If this bit is set, the actorclip level will be traced.
 * 0x200: Weaponclip bit.  If this bit is set, the weaponclip level will be traced.
 */
static bool TR_TileTestLine (TR_TILE_TYPE* tile, const vec3_t start, const vec3_t end, const int levelmask)
{
	const int corelevels = (levelmask & TL_FLAG_REGULAR_LEVELS);

	/* loop over all theads */
	for (int i = 0; i < tile->numtheads; i++) {
		const int level = tile->theadlevel[i];
		if (level && corelevels && !(level & corelevels))
			continue;
		if (level == LEVEL_LIGHTCLIP)	/* lightclips are only used in ufo2map, and it does not use this function */
			continue;
		if (level == LEVEL_ACTORCLIP && !(levelmask & TL_FLAG_ACTORCLIP))
			continue;
		if (level == LEVEL_WEAPONCLIP && !(levelmask & TL_FLAG_WEAPONCLIP))
			continue;
		if (TR_TestLine_r(tile, tile->thead[i], start, end))
			return true;
	}
	return false;
}

/**
 * @brief Checks traces against the world
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[in] levelmask Indicates which special levels, if any, to include in the trace.
 * @note Special levels are LEVEL_ACTORCLIP and LEVEL_WEAPONCLIP.
 * @sa TR_TestLine_r
 * @return false if not blocked
 */
bool TR_TestLine (mapTiles_t* mapTiles, const vec3_t start, const vec3_t end, const int levelmask)
{
	int tile;

	for (tile = 0; tile < mapTiles->numTiles; tile++) {
		if (TR_TileTestLine(&mapTiles->mapTiles[tile], start, end, levelmask))
			return true;
	}
	return false;
}


/*
===============================================================================
LINE TRACING - TEST FOR BRUSH LOCATION
===============================================================================
*/


/**
 * @param[in] tile The map tile containing the structures to be traced.
 * @param[in] nodenum Node index
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[in,out] tr_end used to hold the point on a line that an obstacle is encountered.
 * @sa TR_TestLine_r
 * @sa TR_TestLineDM
 */
static int TR_TestLineDist_r (TR_TILE_TYPE* tile, int32_t nodenum, const vec3_t start, const vec3_t end, vec3_t tr_end)
{
	tnode_t* tnode;
	float front, back;
	vec3_t mid;
	float frac;
	int side;
	int r;

	if (nodenum & (1 << 31)) {
		r = nodenum & ~(1 << 31);
		if (r)
			VectorCopy(start, tr_end);
		return r;				/* leaf node */
	}

	tnode = &tile->tnodes[nodenum];
	assert(tnode);
	switch (tnode->type) {
	case PLANE_X:
	case PLANE_Y:
	case PLANE_Z:
		front = start[tnode->type] - tnode->dist;
		back = end[tnode->type] - tnode->dist;
		break;
	case PLANE_NONE:
		r = TR_TestLineDist_r(tile, tnode->children[0], start, end, tr_end);
		if (r)
			VectorCopy(tr_end, mid);
		side = TR_TestLineDist_r(tile, tnode->children[1], start, end, tr_end);
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
		back = (end[0] * tnode->normal[0] + end[1] * tnode->normal[1] + end[2] * tnode->normal[2]) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TR_TestLineDist_r(tile, tnode->children[0], start, end, tr_end);

	if (front < ON_EPSILON && back < ON_EPSILON)
		return TR_TestLineDist_r(tile, tnode->children[1], start, end, tr_end);

	side = front < 0;

	frac = front / (front - back);

	VectorInterpolation(start, end, frac, mid);

	r = TR_TestLineDist_r(tile, tnode->children[side], start, mid, tr_end);
	if (r)
		return r;
	return TR_TestLineDist_r(tile, tnode->children[!side], mid, end, tr_end);
}

/**
 * @brief Checks traces against the world, gives hit position back
 * @param[in] tile The map tile containing the structures to be traced.
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[out] hit The position where the trace hits a object or the 'end' position if nothing is in the line.
 * @param[in] levelmask The bitmask of the levels (1<<[0-7]) to trace for
 * @sa TR_TestLineDM
 * @sa CL_ActorMouseTrace
 * @return false if no connection between start and end - 1 otherwise
 */
static bool TR_TileTestLineDM (TR_TILE_TYPE* tile, const vec3_t start, const vec3_t end, vec3_t hit, const int levelmask)
{
#ifdef COMPILE_MAP
	const int corelevels = (levelmask & TL_FLAG_REGULAR_LEVELS);
#endif

	VectorCopy(end, hit);

	for (int i = 0; i < tile->numtheads; i++) {
#ifdef COMPILE_MAP
		const int level = tile->theadlevel[i];
		if (level && corelevels && !(level & levelmask))
			continue;
/*		if (level == LEVEL_LIGHTCLIP)
			continue;*/
		if (level == LEVEL_ACTORCLIP && !(levelmask & TL_FLAG_ACTORCLIP))
			continue;
		if (level == LEVEL_WEAPONCLIP && !(levelmask & TL_FLAG_WEAPONCLIP))
			continue;
#endif
		vec3_t tr_end;
		if (TR_TestLineDist_r(tile, tile->thead[i], start, end, tr_end))
			if (VectorNearer(tr_end, hit, start))
				VectorCopy(tr_end, hit);
	}

	if (VectorCompareEps(hit, end, EQUAL_EPSILON))
		return false;
	else
		return true;
}


/**
 * @brief Checks traces against the world, gives hit position back
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[out] hit The position where the trace hits a object or the 'end' position if nothing is in the line.
 * @param[in] levelmask Indicates which special levels, if any, to include in the trace.
 * @sa TR_TestLineDM
 * @sa CL_ActorMouseTrace
 * @return false if no connection between start and end - 1 otherwise
 */
bool TR_TestLineDM (mapTiles_t* mapTiles, const vec3_t start, const vec3_t end, vec3_t hit, const int levelmask)
{
	int tile;
	vec3_t t_end;

	VectorCopy(end, hit);
	VectorCopy(end, t_end);

	for (tile = 0; tile < mapTiles->numTiles; tile++) {
		if (TR_TileTestLineDM(&mapTiles->mapTiles[tile], start, end, t_end, levelmask)) {
			if (VectorNearer(t_end, hit, start))
				VectorCopy(t_end, hit);
		}
	}

	if (VectorCompareEps(hit, end, EQUAL_EPSILON))
		return false;
	else
		return true;
}

void mapTiles_s::getTilesAt (int x ,int y, byte& fromTile1, byte& fromTile2, byte& fromTile3)
{
#if defined(COMPILE_UFO)
	for (int i = 0; i < numTiles; i++) {
		if ( mapTiles[i].wpMins[0] > x
		  || mapTiles[i].wpMaxs[0] - 1 < x	/* the -1 is a temporary fix for wpMaxs being off by 1 */
		  || mapTiles[i].wpMins[1] > y
		  || mapTiles[i].wpMaxs[1] - 1 < y)
			continue;
		/* this tile exists at x/y, so store it */
		if (!fromTile1)
			fromTile1 = mapTiles[i].idx + 1;	/* tile number, not index */
		else if (!fromTile2)
			fromTile2 = mapTiles[i].idx + 1;	/* tile number, not index */
		else
			fromTile3 = 99;								/* stacking of more than two tiles is not supported */
	}
#endif
}

void mapTiles_s::getTileOverlap (const byte tile1, const byte tile2, int& minZ, int& maxZ)
{
#if defined(COMPILE_UFO)
	int lowZ1 = mapTiles[tile1 - 1].wpMins[2];
	int lowZ2 = mapTiles[tile2 - 1].wpMins[2];
	int highZ1 = mapTiles[tile1 - 1].wpMaxs[2];
	int highZ2 = mapTiles[tile2 - 1].wpMaxs[2];
	minZ = std::max(lowZ1, lowZ2);
	if (minZ > 0)
		minZ--;			/* routing needs to start one level below the actual overlap */
	maxZ = std::min(highZ1, highZ2);
	maxZ++;				/* ... and one level above */
#endif
}

void mapTiles_s::printTilesAt (int x ,int y)
{
#if defined(COMPILE_UFO)
	byte fromTile1 = 0;		/* tile number, not index */
	byte fromTile2 = 0;
	byte fromTile3 = 0;
	getTilesAt(x ,y, fromTile1, fromTile2, fromTile3);
	if (fromTile1) {
		const MapTile& tile = mapTiles[fromTile1 - 1];
		Com_Printf("Tilenames: %s (%i %i) (%i %i)", tile.name, tile.wpMins[0], tile.wpMins[1], tile.wpMaxs[0], tile.wpMaxs[1]);
	}
	if (fromTile2)
		Com_Printf(", %s", mapTiles[fromTile2 - 1].name);
	if (fromTile3)
		Com_Printf(", %s", mapTiles[fromTile3 - 1].name);
	Com_Printf("\n");
#endif
}

/*
===============================================================================
BOX TRACING
===============================================================================
*/


/**
 * @brief Returns PSIDE_FRONT, PSIDE_BACK, or PSIDE_BOTH
 */
int TR_BoxOnPlaneSide (const vec3_t mins, const vec3_t maxs, const TR_PLANE_TYPE* plane)
{
	int side;

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

	vec3_t corners[2];
	for (int i = 0; i < 3; i++) {
		if (plane->normal[i] < 0) {
			corners[0][i] = mins[i];
			corners[1][i] = maxs[i];
		} else {
			corners[1][i] = mins[i];
			corners[0][i] = maxs[i];
		}
	}

	vec_t dist1 = DotProduct(plane->normal, corners[0]) - plane->dist;
	vec_t dist2 = DotProduct(plane->normal, corners[1]) - plane->dist;
	side = 0;
	if (dist1 >= PLANESIDE_EPSILON)
		side = PSIDE_FRONT;
	if (dist2 < PLANESIDE_EPSILON)
		side |= PSIDE_BACK;

	return side;
}

typedef struct leaf_check_s {
	int leaf_count, leaf_maxcount;
	int32_t* leaf_list;
	int32_t leaf_topnode;
} leaf_check_t;

/**
 * @brief Fills in a list of all the leafs touched
 * call with topnode set to the headnode, returns with topnode
 * set to the first node that splits the box
 */
static void TR_BoxLeafnums_r (boxtrace_t* traceData, int32_t nodenum, leaf_check_t* lc)
{
	TR_TILE_TYPE* myTile = traceData->tile;

	while (1) {
		if (nodenum <= LEAFNODE) {
			if (lc->leaf_count >= lc->leaf_maxcount) {
/*				Com_Printf("CM_BoxLeafnums_r: overflow\n"); */
				return;
			}
			lc->leaf_list[lc->leaf_count++] = -1 - nodenum;
			return;
		}

		assert(nodenum < myTile->numnodes + 6); /* +6 => bbox */
		TR_NODE_TYPE* node = &myTile->nodes[nodenum];

		TR_PLANE_TYPE* plane;
#ifdef COMPILE_UFO
		plane = node->plane;
#else
		plane = myTile->planes + node->planenum;
#endif

		int s = TR_BoxOnPlaneSide(traceData->absmins, traceData->absmaxs, plane);
		if (s == PSIDE_FRONT)
			nodenum = node->children[0];
		else if (s == PSIDE_BACK)
			nodenum = node->children[1];
		else {					/* go down both */
			if (lc->leaf_topnode == LEAFNODE)
				lc->leaf_topnode = nodenum;
			TR_BoxLeafnums_r(traceData, node->children[0], lc);
			nodenum = node->children[1];
		}
	}
}

/**
 * @brief Fill a list of leafnodes that the trace touches
 * @param[in] traceData both parameters and results of the trace
 * @param[out] list The list to fill
 * @param[in] listsize Maximum size of that list
 * @param[in] headnode if < 0 we are in a leaf node
 */
static int TR_BoxLeafnums_headnode (boxtrace_t* traceData, int32_t* list, int listsize, int32_t headnode)
{
	leaf_check_t lc;
	lc.leaf_list = list;
	lc.leaf_count = 0;
	lc.leaf_maxcount = listsize;
	lc.leaf_topnode = LEAFNODE;

	assert(headnode < traceData->tile->numnodes + 6); /* +6 => bbox */
	TR_BoxLeafnums_r(traceData, headnode, &lc);

	return lc.leaf_count;
}


/**
 * @param[in,out] traceData both parameters and results of the trace
 * @param[in] brush the brush that is being examined
 * @param[in] leaf the leafnode the brush that is being examined belongs to
 * @brief This function checks to see if any sides of a brush intersect the line from p1 to p2 or are located within
 *  the perpendicular bounding box from mins to maxs originating from the line. It also check to see if the line
 *  originates from inside the brush, terminates inside the brush, or is completely contained within the brush.
 */
static void TR_ClipBoxToBrush (boxtrace_t* traceData, cBspBrush_t* brush, TR_LEAF_TYPE* leaf)
{
	if (!brush || !brush->numsides)
		return;

#ifdef COMPILE_UFO
	TR_BRUSHSIDE_TYPE* leadside = nullptr;
#endif
	TR_TILE_TYPE* myTile = traceData->tile;

	float enterfrac = -1.0;
	float leavefrac = 1.0;
	bool getout = false;
	bool startout = false;

	TR_PLANE_TYPE* clipplane = nullptr;
	int clipplanenum = 0;

	float dist;
	vec3_t ofs;
	for (int i = 0; i < brush->numsides; i++) {
		TR_BRUSHSIDE_TYPE* side = &myTile->brushsides[brush->firstbrushside + i];
#ifdef COMPILE_UFO
		TR_PLANE_TYPE* plane = side->plane;
#else
		TR_PLANE_TYPE* plane = myTile->planes + side->planenum;
#endif

		/** @todo special case for axial */
		if (!traceData->ispoint) {	/* general box case */
			/* push the plane out appropriately for mins/maxs */
			for (int j = 0; j < 3; j++) {
				if (plane->normal[j] < 0)
					ofs[j] = traceData->maxs[j];
				else
					ofs[j] = traceData->mins[j];
			}
			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		} else {				/* special point case */
			dist = plane->dist;
		}

		float d1 = DotProduct(traceData->start, plane->normal) - dist;
		float d2 = DotProduct(traceData->end, plane->normal) - dist;

		if (d2 > 0)
			getout = true;		/* endpoint is not in solid */
		if (d1 > 0)
			startout = true;	/* startpoint is not in solid */

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
#else
				leadside = side;
#endif
			}
		} else {				/* leave */
			const float f = (d1 + DIST_EPSILON) / (d1 - d2);
			if (f < leavefrac)
				leavefrac = f;
		}
	}

	if (!startout) {			/* original point was inside brush */
		traceData->trace.startsolid = true;
		if (!getout)
			traceData->trace.allsolid = true;
		traceData->trace.leafnum = leaf - myTile->leafs;
		return;
	}
	if (enterfrac < leavefrac) {
		if (enterfrac > -1 && enterfrac < traceData->trace.fraction) {
			if (enterfrac < 0)
				enterfrac = 0;
			traceData->trace.fraction = enterfrac;
			traceData->trace.plane = *clipplane;
			traceData->trace.planenum = clipplanenum;
#ifdef COMPILE_UFO
			traceData->trace.surface = leadside->surface;
#endif
			traceData->trace.contentFlags = brush->brushContentFlags;
			traceData->trace.leafnum = leaf - myTile->leafs;
		}
	}
}

/**
 * @sa CM_TraceToLeaf
 */
static void TR_TestBoxInBrush (boxtrace_t* traceData, cBspBrush_t* brush)
{
	TR_PLANE_TYPE* plane;
	vec3_t ofs;
	TR_TILE_TYPE* myTile = traceData->tile;

	if (!brush || !brush->numsides)
		return;

	for (int i = 0; i < brush->numsides; i++) {
		TR_BRUSHSIDE_TYPE* side = &myTile->brushsides[brush->firstbrushside + i];
#ifdef COMPILE_UFO
		plane = side->plane;
#else
		plane = myTile->planes + side->planenum;
#endif

		/** @todo special case for axial */
		/* general box case */
		/* push the plane out appropriately for mins/maxs */
		for (int j = 0; j < 3; j++) {
			if (plane->normal[j] < 0)
				ofs[j] = traceData->maxs[j];
			else
				ofs[j] = traceData->mins[j];
		}
		float dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		float d1 = DotProduct(traceData->start, plane->normal) - dist;

		/* if completely in front of face, no intersection */
		if (d1 > 0)
			return;
	}

	/* inside this brush */
	traceData->trace.startsolid = traceData->trace.allsolid = true;
	traceData->trace.fraction = 0;
	traceData->trace.contentFlags = brush->brushContentFlags;
}


/**
 * @param[in] traceData both parameters and results of the trace
 * @param[in] leafnum the leaf index that we are looking in for a hit against
 * @sa CM_ClipBoxToBrush
 * @sa CM_TestBoxInBrush
 * @sa CM_RecursiveHullCheck
 * @brief This function checks if the specified leaf matches any mask specified in traceData.contents. and does
 *  not contain any mask specified in traceData.rejects  If so, each brush in the leaf is examined to see if it
 *  is intersected by the line drawn in TR_RecursiveHullCheck or is within the bounding box set in trace_mins and
 *  trace_maxs with the origin on the line.
 */
static void TR_TraceToLeaf (boxtrace_t* traceData, int32_t leafnum)
{
	TR_TILE_TYPE* myTile = traceData->tile;

	assert(leafnum > LEAFNODE);
	assert(leafnum <= myTile->numleafs);

	TR_LEAF_TYPE* leaf = &myTile->leafs[leafnum];

	if (traceData->contents != MASK_ALL && (!(leaf->contentFlags & traceData->contents) || (leaf->contentFlags & traceData->rejects)))
		return;

	/* trace line against all brushes in the leaf */
	for (int k = 0; k < leaf->numleafbrushes; k++) {
		const int brushnum = myTile->leafbrushes[leaf->firstleafbrush + k];
		cBspBrush_t* b = &myTile->brushes[brushnum];

		if (b->checkcount == checkcount)
			continue;			/* already checked this brush in another leaf */
		b->checkcount = checkcount;

		if (traceData->contents != MASK_ALL && (!(b->brushContentFlags & traceData->contents) || (b->brushContentFlags & traceData->rejects)))
			continue;

		TR_ClipBoxToBrush(traceData, b, leaf);
		if (!traceData->trace.fraction)
			return;
	}
}


/**
 * @sa CM_TestBoxInBrush
 */
static void TR_TestInLeaf (boxtrace_t* traceData, int32_t leafnum)
{
	TR_TILE_TYPE* myTile = traceData->tile;

	assert(leafnum > LEAFNODE);
	assert(leafnum <= myTile->numleafs);

	const TR_LEAF_TYPE* leaf = &myTile->leafs[leafnum];
	/* If this leaf contains no flags we need to look for, then skip it. */
	if (!(leaf->contentFlags & traceData->contents)) /* || (leaf->contentFlags & traceData->rejects) */
		return;

	/* trace line against all brushes in the leaf */
	for (int k = 0; k < leaf->numleafbrushes; k++) {
		const int brushnum = myTile->leafbrushes[leaf->firstleafbrush + k];
		cBspBrush_t* b = &myTile->brushes[brushnum];
		if (b->checkcount == checkcount)
			continue;			/* already checked this brush in another leaf */
		b->checkcount = checkcount;

		if (!(traceData->contents && (b->brushContentFlags & traceData->contents)) || (b->brushContentFlags & traceData->rejects))
			continue;
		TR_TestBoxInBrush(traceData, b);
		if (!traceData->trace.fraction)
			return;
	}
}


/**
 * @param[in] traceData both parameters and results of the trace
 * @param[in] nodenum the node index that we are looking in for a hit
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
static void TR_RecursiveHullCheck (boxtrace_t* traceData, int32_t nodenum, float p1f, float p2f, const vec3_t p1, const vec3_t p2)
{
	TR_NODE_TYPE* node;
	TR_PLANE_TYPE* plane;
	float t1, t2, offset;
	float frac, frac2;
	int side;
	float midf;
	vec3_t mid;
	TR_TILE_TYPE* myTile = traceData->tile;

	if (traceData->trace.fraction <= p1f)
		return;					/* already hit something nearer */

	/* if < 0, we are in a leaf node */
	if (nodenum <= LEAFNODE) {
		TR_TraceToLeaf(traceData, LEAFNODE - nodenum);
		return;
	}

	assert(nodenum < MAX_MAP_NODES);

	/* find the point distances to the seperating plane
	 * and the offset for the size of the box */
	node = myTile->nodes + nodenum;

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
		offset = traceData->extents[type];
	} else {
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;
		if (traceData->ispoint)
			offset = 0;
		else
			offset = fabs(traceData->extents[0] * plane->normal[0])
				+ fabs(traceData->extents[1] * plane->normal[1])
				+ fabs(traceData->extents[2] * plane->normal[2]);
	}

	/* see which sides we need to consider */
	if (t1 >= offset && t2 >= offset) {
		TR_RecursiveHullCheck(traceData, node->children[0], p1f, p2f, p1, p2);
		return;
	} else if (t1 < -offset && t2 < -offset) {
		TR_RecursiveHullCheck(traceData, node->children[1], p1f, p2f, p1, p2);
		return;
	}

	/* put the crosspoint DIST_EPSILON pixels on the near side */
	/** @note t1 and t2 refer to the distance the endpoints of the line are from the bsp dividing plane for this node.
	 * Additionally, frac and frac2 are the fractions of the CURRENT PIECE of the line that was being tested, and will
	 * add to (approximately) 1.  When midf is calculated, frac and frac2 are converted to reflect the fraction of the
	 * WHOLE line being traced.  However, the interpolated vector is based on the CURRENT endpoints so uses frac and
	 * frac2 to find the actual splitting point.
	 */
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
	VectorInterpolation(p1, p2, frac, mid);
	TR_RecursiveHullCheck(traceData, node->children[side], p1f, midf, p1, mid);

	/* go past the node */
	if (frac2 < 0)
		frac2 = 0;
	else if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f) * frac2;
	VectorInterpolation(p1, p2, frac2, mid);
	TR_RecursiveHullCheck(traceData, node->children[side ^ 1], midf, p2f, mid, p2);
}

/**
 * @brief This function traces a line from start to end.  It returns a trace_t indicating what portion of the line
 * can be traveled from start to end before hitting a brush that meets the criteria in brushmask.  The point that
 * this line intersects that brush is also returned.
 * There is a special case when start and end are the same vector.  In this case, the bounding box formed by mins
 * and maxs offset from start is examined for any brushes that meet the criteria.  The first brush found inside
 * the bounding box is returned.
 * There is another special case when mins and maxs are both origin vectors (0, 0, 0).  In this case, the
 * @param[in] traceData All parameters of the trace; also stores some intermediate results
 * @param[in] traceLine The trace start and end vector
 * @param[in] traceBox The box we shove through the world
 * @param[in] headnode if < 0 we are in a leaf node
 * @param[in] fraction The furthest distance needed to trace before we stop.
 */
trace_t TR_BoxTrace (boxtrace_t& traceData, const Line& traceLine, const AABB& traceBox, const int headnode, const float fraction)
{
	checkcount++;	/* for multi-check avoidance */

#ifdef COMPILE_UFO
	if (headnode >= traceData.tile->numnodes + 6)
		Com_Error(ERR_DROP, "headnode (%i) is out of bounds: %i", headnode, traceData.tile->numnodes + 6);
#else
	assert(headnode < traceData.tile->numnodes + 6); /* +6 => bbox */
#endif

	if (!traceData.tile->numnodes)		/* map not loaded */
		return traceData.trace;

	/* check for position test special case */
	if (VectorEqual(traceData.start, traceData.end)) {
		int32_t leafs[MAX_LEAFS];

		VectorAdd(traceData.start, traceData.maxs, traceData.absmaxs);
		VectorAdd(traceData.start, traceData.mins, traceData.absmins);

		int numleafs = TR_BoxLeafnums_headnode(&traceData, leafs, MAX_LEAFS, headnode);
		for (int i = 0; i < numleafs; i++) {
			TR_TestInLeaf(&traceData, leafs[i]);
			if (traceData.trace.allsolid)
				break;
		}
		VectorCopy(traceLine.start, traceData.trace.endpos);
		return traceData.trace;
	}

	/* check for point special case */
	if (VectorEmpty(traceData.mins) && VectorEmpty(traceData.maxs)) {
		traceData.ispoint = true;
		VectorClear(traceData.extents);
	} else {
		traceData.ispoint = false;
		VectorCopy(traceData.maxs, traceData.extents);
	}

	/* general sweeping through world */
	/** @todo Would Interpolating traceData.end to traceData.fraction and passing traceData.fraction instead of 1.0 make this faster? */
	TR_RecursiveHullCheck(&traceData, headnode, 0.0, 1.0, traceData.start, traceData.end);

	if (traceData.trace.fraction >= 1.0) {
		VectorCopy(traceData.end, traceData.trace.endpos);
	} else {
		VectorInterpolation(traceData.start, traceData.end, traceData.trace.fraction, traceData.trace.endpos);
	}
	/* Now un-offset the end position. */
	VectorSubtract(traceData.trace.endpos, traceData.offset, traceData.trace.endpos);
	return traceData.trace;
}

/**
 * @brief Traces all submodels in the specified tile.  Provides for a short
 *   circuit if the trace tries to move past fraction to save time.
 * @param[in] myTile The tile being traced
 * @param[in] traceLine The trace start and end vectors
 * @param[in] aabb The box we are moving through the world
 * @param[in] levelmask Selects which submodels get scanned.
 * @param[in] brushmask brushes the trace should stop at (see MASK_*)
 * @param[in] brushreject brushes the trace should ignore (see MASK_*)
 */
trace_t TR_TileBoxTrace (TR_TILE_TYPE* myTile, const Line& traceLine, const AABB& aabb, const int levelmask, const int brushmask, const int brushreject)
{
	int i;
	cBspHead_t* h;
	trace_t tr;

	/* ensure that the first trace is set in every case */
	tr.fraction = 2.0f;

	boxtrace_t traceData;
	traceData.init(myTile, brushmask, brushreject, tr.fraction);
	traceData.setLineAndBox(traceLine, aabb);
	/* trace against all loaded map tiles */
	for (i = 0, h = myTile->cheads; i < myTile->numcheads; i++, h++) {
		/* This code uses levelmask to limit by maplevel.  Supposedly maplevels 1-255
		 * are bitmasks of game levels 1-8.  0 is a special case repeat of 255.
		 * However a levelmask including 0x100 is usually included so the CLIP levels are
		 * examined. */
		if (h->level && h->level <= LEVEL_LASTVISIBLE && levelmask && !(h->level & levelmask))
			continue;

		assert(h->cnode < myTile->numnodes + 6); /* +6 => bbox */
		const trace_t newtr = TR_BoxTrace(traceData, traceLine, aabb, h->cnode, tr.fraction);

		/* memorize the trace with the minimal fraction */
		if (newtr.fraction == 0.0)
			return newtr;
		if (newtr.fraction < tr.fraction)
			tr = newtr;
	}
	return tr;
}

#ifdef COMPILE_MAP

/**
 * @param[in] start trace start vector
 * @param[in] end trace end vector
 * @param[in] mins box mins
 * @param[in] maxs box maxs
 * @param[in] levelmask Selects which submodels get scanned.
 * @param[in] brushmask brushes the trace should stop at (see MASK_*)
 * @param[in] brushreject brushes the trace should ignore (see MASK_*)
 * @brief Traces all submodels in the first tile.  Used by ufo2map.
 */
trace_t TR_SingleTileBoxTrace (mapTiles_t* mapTiles, const Line& traceLine, const AABB* traceBox, const int levelmask, const int brushmask, const int brushreject)
{
	/* Trace the whole line against the first tile. */
	trace_t tr = TR_TileBoxTrace(&mapTiles->mapTiles[0], traceLine, *traceBox, levelmask, brushmask, brushreject);
	tr.mapTile = 0;
	return tr;
}
#endif
