/**
 * @file
 * @brief model loading and grid oriented movement and scanning
 * @note collision detection code (server side)
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

#include "common.h"
#include "tracing.h"

/*
===============================================================================
GAME RELATED TRACING USING ENTITIES
===============================================================================
*/

/**
 * @brief Calculates the worst case bounding box for the given bsp model
 * @param[in] model The model to calculate the bbox for
 * @param[out] box The bbox to fill
 */
static void CM_CalculateWidestBoundingBox (const cBspModel_t* model, AABB& box)
{
	/* Quickly calculate the bounds of this model to see if they can overlap. */
	box.set(model->cbmBox);
	box.shift(model->origin);
	if (VectorNotEmpty(model->angles)) {
		const float offset = std::max(std::max(box.getWidthX(), box.getWidthY()), box.getWidthZ()) / 2.0;
		box.expand(offset);		/* expand the whole box by the highest extent we found */
	}
}

/**
 * @brief A quick test if the trace might hit the inline model
 * @param[in] tLine The traceline
 * @param[in] model The entity to check
 * @return true - the line isn't anywhere near the model; false - we can't be sure
 */
static bool CM_LineMissesModel (const Line& tLine, const cBspModel_t* model)
{
	AABB absbox;
	CM_CalculateWidestBoundingBox(model, absbox);
	/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
	if (!absbox.canBeHitBy(tLine))
		return true;	/* impossible */

	return false;		/* maybe */
}


/**
 * @param[in] tile Tile to check (normally 0 - except in assembled maps)
 * @param[in] traceLine The start and stop vectors of the trace
 * @param[in] traceBox The box we shove through the world
 * @param[in] headnode if < 0 we are in a leaf node
 * @param[in] contentmask content flags the trace should stop at (see MASK_*)
 * @param[in] brushrejects brushes the trace should ignore (see MASK_*)
 * @param[in] origin center for rotating objects
 * @param[in] angles current rotation status (in degrees) for rotating objects
 * @param[in] rmaShift how much the object was shifted by the RMA process (needed for doors)
 * @param[in] fraction The furthest distance needed to trace before we stop.
 * @brief Handles offseting and rotation of the end points for moving and rotating entities
 * @sa CM_BoxTrace
 */
trace_t CM_HintedTransformedBoxTrace (MapTile& tile, const Line& traceLine, const AABB& traceBox, const int headnode, const int contentmask, const int brushrejects, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction)
{
	vec3_t start_l, end_l;
	vec3_t forward, right, up;
	vec3_t temp;
	bool rotated;

	/* subtract origin offset */
	VectorSubtract(traceLine.start, origin, start_l);
	VectorSubtract(traceLine.stop, origin, end_l);

	/* rotate start and end into the models frame of reference */
	if (headnode != tile.box_headnode && VectorNotEmpty(angles)) {
		rotated = true;
	} else {
		rotated = false;
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

	/* When tracing through a model, we want to use the nodes, planes etc. as calculated by ufo2map.
	 * But nodes and planes have been shifted in case of an RMA. At least for doors we need to undo the shift. */
	if (VectorNotEmpty(origin)) {					/* only doors seem to have their origin set */
		VectorAdd(start_l, rmaShift, start_l);		/* undo the shift */
		VectorAdd(end_l, rmaShift, end_l);
	}

	/* sweep the box through the model */
	boxtrace_t traceData;
	traceData.init(&tile, contentmask, brushrejects, fraction);
	traceData.setLineAndBox(Line(start_l, end_l), traceBox);
	trace_t trace = TR_BoxTrace(traceData, Line(start_l, end_l), traceBox, headnode, fraction);
	trace.mapTile = tile.idx;

	if (rotated && trace.fraction != 1.0) {
		vec3_t a;
		/** @todo figure out how to do this with existing angles */
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	VectorInterpolation(traceLine.start, traceLine.stop, trace.fraction, trace.endpos);

	return trace;
}

/**
 * @brief To keep everything totally uniform, bounding boxes are turned into small
 * BSP trees instead of being compared directly.
 */
int32_t CM_HeadnodeForBox (MapTile& tile, const AABB& box)
{
	tile.box_planes[0].dist = box.maxs[0];
	tile.box_planes[1].dist = -box.maxs[0];
	tile.box_planes[2].dist = box.mins[0];
	tile.box_planes[3].dist = -box.mins[0];
	tile.box_planes[4].dist = box.maxs[1];
	tile.box_planes[5].dist = -box.maxs[1];
	tile.box_planes[6].dist = box.mins[1];
	tile.box_planes[7].dist = -box.mins[1];
	tile.box_planes[8].dist = box.maxs[2];
	tile.box_planes[9].dist = -box.maxs[2];
	tile.box_planes[10].dist = box.mins[2];
	tile.box_planes[11].dist = -box.mins[2];

	assert(tile.box_headnode < MAX_MAP_NODES);
	return tile.box_headnode;
}

/* TRACING FUNCTIONS */

/**
 * @brief Checks traces against the world and all inline models
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] traceLine The start/stop positions of the trace.
 * @param[in] levelmask
 * @param[in] entlist The local models list
 * @sa TR_TestLine
 * @sa CM_InlineModel
 * @sa CM_TransformedBoxTrace
 * @return true - hit something
 * @return false - hit nothing
 */
bool CM_EntTestLine (mapTiles_t* mapTiles, const Line& traceLine, const int levelmask, const char** entlist)
{
	const char** name;

	/* trace against world first */
	if (TR_TestLine(mapTiles, traceLine.start, traceLine.stop, levelmask))
		/* We hit the world, so we didn't make it anyway... */
		return true;

	/* no local models, so we made it. */
	if (!entlist)
		return false;

	for (name = entlist; *name; name++) {
		const cBspModel_t* model;
		/* check whether this is really an inline model */
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		model = CM_InlineModel(mapTiles, *name);
		assert(model);
		if (model->headnode >= mapTiles->mapTiles[model->tile].numnodes + 6)
			continue;

		/* check if we can safely exclude that the trace can hit the model */
		if (CM_LineMissesModel(traceLine, model))
			continue;

		const trace_t trace = CM_HintedTransformedBoxTrace(mapTiles->mapTiles[model->tile], traceLine, AABB(),
				model->headnode, MASK_VISIBILILITY, 0, model->origin, model->angles, model->shift, 1.0);
		/* if we started the trace in a wall */
		/* or the trace is not finished */
		if (trace.startsolid || trace.fraction < 1.0)
			return true;
	}

	/* not blocked */
	return false;
}

/**
 * @brief Checks traces against the world and all inline models, gives the hit position back
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] trLine The start/stop positions of the trace.
 * @param[out] hit The position where the line hits a object or the stop position if nothing is in the line
 * @param[in] levelmask
 * @param[in] entlist The local models list
 * @sa TR_TestLineDM
 * @sa CM_TransformedBoxTrace
 */
bool CM_EntTestLineDM (mapTiles_t* mapTiles, const Line& trLine, vec3_t hit, const int levelmask, const char** entlist)
{
	const char** name;
	bool blocked;
	float fraction = 2.0f;

	/* trace against world first */
	blocked = TR_TestLineDM(mapTiles, trLine.start, trLine.stop, hit, levelmask);
	if (!entlist)
		return blocked;

	for (name = entlist; *name; name++) {
		const cBspModel_t* model;
		/* check whether this is really an inline model */
		if (*name[0] != '*') {
			/* Let's see what the data looks like... */
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s' (inlines: %p, name: %p)",
					*name, (void*)entlist, (void*)name);
		}
		model = CM_InlineModel(mapTiles, *name);
		assert(model);
		if (model->headnode >= mapTiles->mapTiles[model->tile].numnodes + 6)
			continue;

		/* check if we can safely exclude that the trace can hit the model */
		if (CM_LineMissesModel(trLine, model))
			continue;

		const trace_t trace = CM_HintedTransformedBoxTrace(mapTiles->mapTiles[model->tile], Line(trLine.start, hit), AABB(),
				model->headnode, MASK_ALL, 0, model->origin, model->angles, vec3_origin, fraction);
		/* if we started the trace in a wall */
		if (trace.startsolid) {
			VectorCopy(trLine.start, hit);
			return true;
		}
		/* trace not finished */
		if (trace.fraction < fraction) {
			blocked = true;
			fraction = trace.fraction;
			VectorCopy(trace.endpos, hit);
		}
	}

	/* return result */
	return blocked;
}

/**
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] trLine The line to trace
 * @param[in] box The box we move through the world
 * @param[in] levelmask Selects which submodels get scanned.
 * @param[in] brushmask brushes the trace should stop at (see MASK_*)
 * @param[in] brushreject brushes the trace should ignore (see MASK_*)
 * @brief Traces all submodels in all tiles.  Used by ufo and ufo_ded.
 */
trace_t CM_CompleteBoxTrace (mapTiles_t* mapTiles, const Line& trLine, const AABB& box, int levelmask, int brushmask, int brushreject)
{
	int tile;
	vec3_t smin, smax, emin, emax, wpmins, wpmaxs;
	const vec3_t offset = {UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2};

	trace_t tr;
	tr.fraction = 2.0f;

	/* Prep the mins and maxs */
	for (int i = 0; i < 3; i++) {
		smin[i] = trLine.start[i] + std::min(box.mins[i], box.maxs[i]);
		smax[i] = trLine.start[i] + std::max(box.mins[i], box.maxs[i]);
		emin[i] = trLine.stop[i] + std::min(box.mins[i], box.maxs[i]);
		emax[i] = trLine.stop[i] + std::max(box.mins[i], box.maxs[i]);
	}

	/* trace against all loaded map tiles */
	for (tile = 0; tile < mapTiles->numTiles; tile++) {
		MapTile& myTile = mapTiles->mapTiles[tile];
		PosToVec(myTile.wpMins, wpmins);
		VectorSubtract(wpmins, offset, wpmins);
		PosToVec(myTile.wpMaxs, wpmaxs);
		VectorAdd(wpmaxs, offset, wpmaxs);
		/* If the trace is completely outside of the tile, then skip it. */
		if (smax[0] < wpmins[0] && emax[0] < wpmins[0])
			continue;
		if (smax[1] < wpmins[1] && emax[1] < wpmins[1])
			continue;
		if (smax[2] < wpmins[2] && emax[2] < wpmins[2])
			continue;
		if (smin[0] > wpmaxs[0] && emin[0] > wpmaxs[0])
			continue;
		if (smin[1] > wpmaxs[1] && emin[1] > wpmaxs[1])
			continue;
		if (smin[2] > wpmaxs[2] && emin[2] > wpmaxs[2])
			continue;
		trace_t newtr = TR_TileBoxTrace(&myTile, trLine.start, trLine.stop, box, levelmask, brushmask, brushreject);
		newtr.mapTile = tile;

		/* memorize the trace with the minimal fraction */
		if (newtr.fraction == 0.0)
			return newtr;
		if (newtr.fraction < tr.fraction)
			tr = newtr;
	}
	return tr;
}


/**
 * @brief Performs box traces against the world and all inline models, gives the hit position back
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] traceLine The start/stop position of the trace.
 * @param[in] traceBox The minimum/maximum extents of the collision box that is projected.
 * @param[in] levelmask A mask of the game levels to trace against. Mask 0x100 filters clips.
 * @param[in] brushmask Any brush detected must at least have one of these contents.
 * @param[in] brushreject Any brush detected with any of these contents will be ignored.
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 * @return a trace_t with the information of the closest brush intersected.
 * @sa CM_CompleteBoxTrace
 * @sa CM_HintedTransformedBoxTrace
 */
trace_t CM_EntCompleteBoxTrace (mapTiles_t* mapTiles, const Line& traceLine, const AABB* traceBox, int levelmask, int brushmask, int brushreject, const char** list)
{
	AABB lineBox(*traceBox);
	lineBox.shift(traceLine.start);		/* the traceBox in starting position */
	AABB lineBoxTemp(*traceBox);
	lineBoxTemp.shift(traceLine.stop);	/* in end position */
	lineBox.add(lineBoxTemp);			/* bounding box for the whole trace */
	/* Now lineBox specifies the whole volume to be traced through. */

	/* reconstruct a levelmask */
	const vec_t minZ = lineBox.getMinZ();
	const vec_t maxZ = lineBox.getMaxZ();
	int newLevelMask = 0;
	if (levelmask & TL_FLAG_ACTORCLIP)		/* if the passed levelmask contains the bit for the cliplevels, */
		newLevelMask = TL_FLAG_ACTORCLIP;	/* preserve it */
	for (int i = 0; i < PATHFINDING_HEIGHT; i++) {
		const vec_t lower = i * UNIT_HEIGHT;	/* the height bounds of the level */
		const vec_t upper = (i + 1) * UNIT_HEIGHT;
		if (minZ > upper || maxZ < lower)
			continue;
		newLevelMask |= (1 << i);
	}

	/* trace against world first */
	const trace_t tr = CM_CompleteBoxTrace(mapTiles, traceLine, *traceBox, newLevelMask, brushmask, brushreject);
	if (!list || tr.fraction == 0.0)
		return tr;

	trace_t trace = tr;
	for (const char** name = list; *name; name++) {
		/* check whether this is really an inline model */
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		const cBspModel_t* model = CM_InlineModel(mapTiles, *name);
		assert(model);
		if (model->headnode >= mapTiles->mapTiles[model->tile].numnodes + 6)
			continue;

		AABB modelBox;
		/* Quickly calculate the bounds of this model to see if they can overlap. */
		CM_CalculateWidestBoundingBox(model, modelBox);

		/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
		if (!lineBox.doesIntersect(modelBox))
			continue;

		const trace_t newtr = CM_HintedTransformedBoxTrace(mapTiles->mapTiles[model->tile], traceLine, *traceBox,
				model->headnode, brushmask, brushreject, model->origin, model->angles, model->shift, trace.fraction);

		/* memorize the trace with the minimal fraction */
		if (newtr.fraction == 0.0)
			return newtr;
		if (newtr.fraction < trace.fraction)
			trace = newtr;
	}
	return trace;
}
