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
 * @brief Calculates the bounding box for the given bsp model
 * @param[in] model The model to calculate the bbox for
 * @param[out] mins The maxs of the bbox
 * @param[out] maxs The mins of the bbox
 */
static void CM_CalculateBoundingBox (const cBspModel_t* model, vec3_t mins, vec3_t maxs)
{
	/* Quickly calculate the bounds of this model to see if they can overlap. */
	VectorAdd(model->origin, model->mins, mins);
	VectorAdd(model->origin, model->maxs, maxs);
	if (VectorNotEmpty(model->angles)) {
		vec3_t acenter, aoffset;
		const float offset = std::max(std::max(fabs(mins[0] - maxs[0]), fabs(mins[1] - maxs[1])), fabs(mins[2] - maxs[2])) / 2.0;
		VectorCenterFromMinsMaxs(mins, maxs, acenter);
		VectorSet(aoffset, offset, offset, offset);
		VectorAdd(acenter, aoffset, maxs);
		VectorSubtract(acenter, aoffset, mins);
	}
}

/**
 * @brief A quick test if the trace might hit the inline model
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] model The entity to check
 * @return true - the line isn't anywhere near the model
 */
static bool CM_LineMissesModel (const vec3_t start, const vec3_t stop, const cBspModel_t* model)
{
	vec3_t amins, amaxs;
	CM_CalculateBoundingBox(model, amins, amaxs);
	/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
	if ((start[0] > amaxs[0] && stop[0] > amaxs[0])
		|| (start[1] > amaxs[1] && stop[1] > amaxs[1])
		|| (start[2] > amaxs[2] && stop[2] > amaxs[2])
		|| (start[0] < amins[0] && stop[0] < amins[0])
		|| (start[1] < amins[1] && stop[1] < amins[1])
		|| (start[2] < amins[2] && stop[2] < amins[2]))
		return true;

	return false;
}


/**
 * @param[in] tile Tile to check (normally 0 - except in assembled maps)
 * @param[in] start trace start vector
 * @param[in] end trace end vector
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
	trace_t trace = TR_BoxTrace(&tile, start_l, end_l, traceBox, headnode, contentmask, brushrejects, fraction);
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
int32_t CM_HeadnodeForBox (MapTile& tile, const vec3_t mins, const vec3_t maxs)
{
	tile.box_planes[0].dist = maxs[0];
	tile.box_planes[1].dist = -maxs[0];
	tile.box_planes[2].dist = mins[0];
	tile.box_planes[3].dist = -mins[0];
	tile.box_planes[4].dist = maxs[1];
	tile.box_planes[5].dist = -maxs[1];
	tile.box_planes[6].dist = mins[1];
	tile.box_planes[7].dist = -mins[1];
	tile.box_planes[8].dist = maxs[2];
	tile.box_planes[9].dist = -maxs[2];
	tile.box_planes[10].dist = mins[2];
	tile.box_planes[11].dist = -mins[2];

	assert(tile.box_headnode < MAX_MAP_NODES);
	return tile.box_headnode;
}

/* TRACING FUNCTIONS */

/**
 * @brief Checks traces against the world and all inline models
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] levelmask
 * @param[in] entlist The local models list
 * @sa TR_TestLine
 * @sa CM_InlineModel
 * @sa CM_TransformedBoxTrace
 * @return true - hit something
 * @return false - hit nothing
 */
bool CM_EntTestLine (mapTiles_t* mapTiles, const vec3_t start, const vec3_t stop, const int levelmask, const char** entlist)
{
	const char** name;

	/* trace against world first */
	if (TR_TestLine(mapTiles, start, stop, levelmask))
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
		if (CM_LineMissesModel(start, stop, model))
			continue;

		const trace_t trace = CM_HintedTransformedBoxTrace(mapTiles->mapTiles[model->tile], Line(start, stop), AABB(),
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
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] hit The position where the line hits a object or the stop position if nothing is in the line
 * @param[in] levelmask
 * @param[in] entlist The local models list
 * @sa TR_TestLineDM
 * @sa CM_TransformedBoxTrace
 */
bool CM_EntTestLineDM (mapTiles_t* mapTiles, const vec3_t start, const vec3_t stop, vec3_t hit, const int levelmask, const char** entlist)
{
	const char** name;
	bool blocked;
	float fraction = 2.0f;

	/* trace against world first */
	blocked = TR_TestLineDM(mapTiles, start, stop, hit, levelmask);
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
		if (CM_LineMissesModel(start, stop, model))
			continue;

		const trace_t trace = CM_HintedTransformedBoxTrace(mapTiles->mapTiles[model->tile], Line(start, hit), AABB(),
				model->headnode, MASK_ALL, 0, model->origin, model->angles, vec3_origin, fraction);
		/* if we started the trace in a wall */
		if (trace.startsolid) {
			VectorCopy(start, hit);
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
	int tile, i;
	vec3_t smin, smax, emin, emax, wpmins, wpmaxs;
	const vec3_t offset = {UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2};

	trace_t tr;
	tr.fraction = 2.0f;

	/* Prep the mins and maxs */
	for (i = 0; i < 3; i++) {
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
	/* trace against world first */
	const trace_t tr = CM_CompleteBoxTrace(mapTiles, traceLine, *traceBox, levelmask, brushmask, brushreject);
	if (!list || tr.fraction == 0.0)
		return tr;

	AABB lineBox(traceLine);	/* Find the original bounding box for the tracing line. */
	lineBox.add(*traceBox);		/* Now increase the bounding box by traceBox in both directions. */
	/* Now lineBox specifies the whole volume to be traced through. */

	trace_t trace = tr;
	for (const char** name = list; *name; name++) {
		/* check whether this is really an inline model */
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		const cBspModel_t* model = CM_InlineModel(mapTiles, *name);
		assert(model);
		if (model->headnode >= mapTiles->mapTiles[model->tile].numnodes + 6)
			continue;

		vec3_t amins, amaxs;
		/* Quickly calculate the bounds of this model to see if they can overlap. */
		CM_CalculateBoundingBox(model, amins, amaxs);

		/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
		AABB modelBox(amins, amaxs);
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
