/**
 * @file cmodel.c
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
#include "routing.h"
#include "../shared/parse.h"

/**
 * @note The vectors are from 0 up to 2*MAX_WORLD_WIDTH - but not negative
 * @note holds the smallest bounding box that will contain the map
 * @sa CL_ClampCamToMap
 * @sa CL_OutsideMap
 * @sa CMod_GetMapSize
 * @sa SV_ClearWorld
 */
vec3_t mapMin, mapMax;

/** @note a list with all inline models (like func_breakable)
 * @todo not threadsafe */
static const char **inlineList;

/** @note these are the TUs used to intentionally move in a given direction.  Falling not included. */
static const int TUsUsed[] = {
	TU_MOVE_STRAIGHT, /* E  */
	TU_MOVE_STRAIGHT, /* W  */
	TU_MOVE_STRAIGHT, /* N  */
	TU_MOVE_STRAIGHT, /* S  */
	TU_MOVE_DIAGONAL, /* NE */
	TU_MOVE_DIAGONAL, /* SW */
	TU_MOVE_DIAGONAL, /* NW */
	TU_MOVE_DIAGONAL, /* SE */
	TU_MOVE_CLIMB,    /* UP     */
	TU_MOVE_CLIMB,    /* DOWN   */
	TU_CROUCH,        /* STAND  */
	TU_CROUCH,        /* CROUCH */
	0,				  /* ???    */
	TU_MOVE_FALL,	  /* FALL   */
	0,				  /* ???    */
	0,				  /* ???    */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & SE */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & SE */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR  /* FLY DOWN & SE */
};
CASSERT(lengthof(TUsUsed) == PATHFINDING_DIRECTIONS);

static void CM_SetInlineList (const char **list)
{
	inlineList = list;
	if (inlineList != NULL && *inlineList == NULL)
		inlineList = NULL;
}

/**
 * @brief This function recalculates the routing surrounding the entity name.
 * @sa CM_InlineModel
 * @sa CM_CheckUnit
 * @sa CM_UpdateConnection
 * @sa CMod_LoadSubmodels
 * @sa Grid_RecalcBoxRouting
 * @param[in] map The routing map (either server or client map)
 * @param[in] name Name of the inline model to compute the mins/maxs for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void CM_RecalcRouting (routing_t *map, const char *name, const char **list)
{
	CM_SetInlineList(list);
	Grid_RecalcRouting(map, name);
	CM_SetInlineList(NULL);
}

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
		const float offset = max(max(fabs(mins[0] - maxs[0]), fabs(mins[1] - maxs[1])), fabs(mins[2] - maxs[2])) / 2.0;
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
 * @return qtrue - the line isn't anywhere near the model
 */
static qboolean CM_LineMissesModel (const vec3_t start, const vec3_t stop, const cBspModel_t *model)
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
		return qtrue;

	return qfalse;
}

/**
 * @brief Checks traces against the world and all inline models
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] levelmask
 * @sa TR_TestLine
 * @sa CM_InlineModel
 * @sa TR_TransformedBoxTrace
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_EntTestLine (const vec3_t start, const vec3_t stop, const int levelmask)
{
	trace_t trace;
	cBspModel_t *model;
	const char **name;

	/* trace against world first */
	if (TR_TestLine(start, stop, levelmask))
		/* We hit the world, so we didn't make it anyway... */
		return qtrue;

	/* no local models, so we made it. */
	if (!inlineList)
		return qfalse;

	for (name = inlineList; *name; name++) {
		/* check whether this is really an inline model */
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;

		/* check if we can safely exclude that the trace can hit the model */
		if (CM_LineMissesModel(start, stop, model))
			continue;

		trace = CM_HintedTransformedBoxTrace(model->tile, start, stop, vec3_origin, vec3_origin, model->headnode, MASK_VISIBILILITY, 0, model->origin, model->angles, model->shift, 1.0);
		/* if we started the trace in a wall */
		/* or the trace is not finished */
		if (trace.startsolid || trace.fraction < 1.0)
			return qtrue;
	}

	/* not blocked */
	return qfalse;
}


/**
 * @brief Checks traces against the world and all inline models
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[in] levelmask
 * @param[in] entlist of entities that might be on this line
 * @sa CM_EntTestLine
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_TestLineWithEnt (const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist)
{
	qboolean hit;

	/* set the list of entities to check */
	CM_SetInlineList(entlist);
	/* do the line test */
	hit = CM_EntTestLine(start, stop, levelmask);
	/* zero the list */
	CM_SetInlineList(NULL);

	return hit;
}


/**
 * @brief Checks traces against the world and all inline models
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] end The position where the trace hits something
 * @param[in] levelmask The bsp level that is used for tracing in (see @c TL_FLAG_*)
 * @param[in] entlist of entities that might be on this line
 * @sa CM_EntTestLineDM
 * @return qtrue - hit something
 * @return qfalse - hit nothing
 */
qboolean CM_TestLineDMWithEnt (const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask, const char **entlist)
{
	qboolean hit;

	/* set the list of entities to check */
	CM_SetInlineList(entlist);
	/* do the line test */
	hit = CM_EntTestLineDM(start, stop, end, levelmask);
	/* zero the list */
	CM_SetInlineList(NULL);

	return hit;
}


/**
 * @brief Checks traces against the world and all inline models, gives the hit position back
 * @param[in] start The position to start the trace.
 * @param[in] stop The position where the trace ends.
 * @param[out] end The position where the line hits a object or the stop position if nothing is in the line
 * @param[in] levelmask
 * @sa TR_TestLineDM
 * @sa TR_TransformedBoxTrace
 */
qboolean CM_EntTestLineDM (const vec3_t start, const vec3_t stop, vec3_t end, const int levelmask)
{
	trace_t trace;
	cBspModel_t *model;
	const char **name;
	int blocked;
	float fraction = 2.0f;

	/* trace against world first */
	blocked = TR_TestLineDM(start, stop, end, levelmask);
	if (!inlineList)
		return blocked;

	for (name = inlineList; *name; name++) {
		/* check whether this is really an inline model */
		if (*name[0] != '*') {
			/* Let's see what the data looks like... */
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s' (inlines: %p, name: %p)",
					*name, inlineList, name);
		}
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;

		/* check if we can safely exclude that the trace can hit the model */
		if (CM_LineMissesModel(start, stop, model))
			continue;

		trace = CM_HintedTransformedBoxTrace(model->tile, start, end, vec3_origin, vec3_origin, model->headnode, MASK_ALL, 0, model->origin, model->angles, vec3_origin, fraction);
		/* if we started the trace in a wall */
		if (trace.startsolid) {
			VectorCopy(start, end);
			return qtrue;
		}
		/* trace not finishd */
		if (trace.fraction < fraction) {
			blocked = qtrue;
			fraction = trace.fraction;
			VectorCopy(trace.endpos, end);
		}
	}

	/* return result */
	return blocked;
}

/**
 * @brief Wrapper for TR_TransformedBoxTrace that accepts a tile number,
 * @sa TR_TransformedBoxTrace
 */
trace_t CM_HintedTransformedBoxTrace (const int tile, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, const int headnode, const int brushmask, const int brushrejects, const vec3_t origin, const vec3_t angles, const vec3_t rmaShift, const float fraction)
{
	return TR_HintedTransformedBoxTrace(&mapTiles[tile], start, end, mins, maxs, headnode, brushmask, brushrejects, origin, angles, rmaShift, fraction);
}

/**
 * @brief Performs box traces against the world and all inline models, gives the hit position back
 * @param[in] start The position to start the trace.
 * @param[in] end The position where the trace ends.
 * @param[in] traceBox The minimum/maximum extents of the collision box that is projected.
 * @param[in] levelmask A mask of the game levels to trace against. Mask 0x100 filters clips.
 * @param[in] brushmask Any brush detected must at least have one of these contents.
 * @param[in] brushreject Any brush detected with any of these contents will be ignored.
 * @return a trace_t with the information of the closest brush intersected.
 * @sa TR_CompleteBoxTrace
 * @sa CM_HintedTransformedBoxTrace
 */
trace_t CM_EntCompleteBoxTrace (const vec3_t start, const vec3_t end, const box_t* traceBox, int levelmask, int brushmask, int brushreject)
{
	trace_t trace, newtr;
	cBspModel_t *model;
	const char **name;
	vec3_t bmins, bmaxs;

	/* trace against world first */
	trace = TR_CompleteBoxTrace(start, end, traceBox->mins, traceBox->maxs, levelmask, brushmask, brushreject);
	if (!inlineList || trace.fraction == 0.0)
		return trace;

	/* Find the original bounding box for the tracing line. */
	VectorSet(bmins, min(start[0], end[0]), min(start[1], end[1]), min(start[2], end[2]));
	VectorSet(bmaxs, max(start[0], end[0]), max(start[1], end[1]), max(start[2], end[2]));
	/* Now increase the bounding box by mins and maxs in both directions. */
	VectorAdd(bmins, traceBox->mins, bmins);
	VectorAdd(bmaxs, traceBox->maxs, bmaxs);
	/* Now bmins and bmaxs specify the whole volume to be traced through. */

	for (name = inlineList; *name; name++) {
		vec3_t amins, amaxs;

		/* check whether this is really an inline model */
		if (*name[0] != '*')
			Com_Error(ERR_DROP, "name in the inlineList is no inline model: '%s'", *name);
		model = CM_InlineModel(*name);
		assert(model);
		if (model->headnode >= mapTiles[model->tile].numnodes + 6)
			continue;

		/* Quickly calculate the bounds of this model to see if they can overlap. */
		CM_CalculateBoundingBox(model, amins, amaxs);

		/* If the bounds of the extents box and the line do not overlap, then skip tracing this model. */
		if (bmins[0] > amaxs[0]
		 || bmins[1] > amaxs[1]
		 || bmins[2] > amaxs[2]
		 || bmaxs[0] < amins[0]
		 || bmaxs[1] < amins[1]
		 || bmaxs[2] < amins[2])
			continue;

		newtr = CM_HintedTransformedBoxTrace(model->tile, start, end, traceBox->mins, traceBox->maxs, model->headnode, brushmask, brushreject, model->origin, model->angles, model->shift, trace.fraction);

		/* memorize the trace with the minimal fraction */
		if (newtr.fraction == 0.0)
			return newtr;
		if (newtr.fraction < trace.fraction)
			trace = newtr;
	}
	return trace;
}

/*
===============================================================================
BOX TRACING
===============================================================================
*/

/**
 * @brief To keep everything totally uniform, bounding boxes are turned into small
 * BSP trees instead of being compared directly.
 */
int CM_HeadnodeForBox (int tile, const vec3_t mins, const vec3_t maxs)
{
	assert(tile < numTiles && tile >= 0);
	return TR_HeadnodeForBox(&mapTiles[tile], mins, maxs);
}

/*
==============================================================================
TARGETING FUNCTIONS
==============================================================================
*/

/**
 * @note Grenade Aiming Maths
 * --------------------------------------------------------------
 *
 * There are two possibilities when aiming: either we can reach the target at
 * maximum speed or we can't. If we can reach it we would like to reach it with
 * as flat a trajectory as possible. To do this we calculate the angle to hit the
 * target with the projectile traveling at the maximum allowed velocity.
 *
 * However, if we can't reach it then we'd like the aiming curve to use the smallest
 * possible velocity that would have reached the target.
 *
 * let d  = horizontal distance to target
 *     h  = vertical distance to target
 *     g  = gravity
 *     v  = launch velocity
 *     vx = horizontal component of launch velocity
 *     vy = vertical component of launch velocity
 *     alpha = launch angle
 *     t  = time
 *
 * Then using the laws of linear motion and some trig
 *
 * d = vx * t
 * h = vy * t - 1/2 * g * t^2
 * vx = v * cos(alpha)
 * vy = v * sin(alpha)
 *
 * and some trig identities
 *
 * 2*cos^2(x) = 1 + cos(2*x)
 * 2*sin(x)*cos(x) = sin(2*x)
 * a*cos(x) + b*sin(x) = sqrt(a^2 + b^2) * cos(x - atan2(b,a))
 *
 * it is possible to show that:
 *
 * alpha = 0.5 * (atan2(d, -h) - theta)
 *
 * where
 *               /    2       2  \
 *              |    v h + g d    |
 *              |  -------------- |
 * theta = acos |        ________ |
 *              |   2   / 2    2  |
 *               \ v  \/ h  + d  /
 *
 *
 * Thus we can calculate the desired aiming angle for any given velocity.
 *
 * With some more rearrangement we can show:
 *
 *               ________________________
 *              /           2
 *             /         g d
 *  v =       / ------------------------
 *           /   ________
 *          /   / 2    2
 *        \/  \/ h  + d   cos(theta) - h
 *
 * Which we can also write as:
 *                _________________
 *               /        a
 * f(theta) =   / ----------------
 *            \/  b cos(theta) - c
 *
 * where
 *  a = g*d^2
 *  b = sqrt(h*h+d*d)
 *  c = h
 *
 * We can imagine a graph of launch velocity versus launch angle. When the angle is near 0 degrees (i.e. totally flat)
 * more and more velocity is needed. Similarly as the angle gets closer and closer to 90 degrees.
 *
 * Somewhere in the middle is the minimum velocity that we could possibly hit the target with and the 'optimum' angle
 * to fire at. Note that when h = 0 the optimum angle is 45 degrees. We want to find the minimum velocity so we need
 * to take the derivative of f (which I suggest doing with an algebra system!).
 *
 * f'(theta) =  a * b * sin(theta) / junk
 *
 * the `junk` is unimportant because we're just going to set f'(theta) = 0 and multiply it out anyway.
 *
 * 0 = a * b * sin(theta)
 *
 * Neither a nor b can be 0 as long as d does not equal 0 (which is a degenerate case). Thus if we solve for theta
 * we get 'sin(theta) = 0', thus 'theta = 0'. If we recall that:
 *
 *  alpha = 0.5 * (atan2(d, -h) - theta)
 *
 * then we get our 'optimum' firing angle alpha as
 *
 * alpha = 1/2 * atan2(d, -h)
 *
 * and if we substitute back into our equation for v and we get
 *
 *               _______________
 *              /        2
 *             /      g d
 *  vmin =    / ---------------
 *           /   ________
 *          /   / 2    2
 *        \/  \/ h  + d   - h
 *
 * as the minimum launch velocity for that angle.
 *
 * @brief Calculates parabola-type shot.
 * @param[in] from Starting position for calculations.
 * @param[in] at Ending position for calculations.
 * @param[in] speed Launch velocity.
 * @param[in] launched Set to true for grenade launchers.
 * @param[in] rolled Set to true for "roll" type shoot.
 * @param[in,out] v0 The velocity vector
 * @todo refactor and move me
 * @todo Com_GrenadeTarget() is called from CL_TargetingGrenade() with speed
 * param as (fireDef_s) fd->range (gi.GrenadeTarget, too), while it is being used here for speed
 * calculations - a bug or just misleading documentation?
 * @sa CL_TargetingGrenade
 */
float Com_GrenadeTarget (const vec3_t from, const vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0)
{
	const float rollAngle = 3.0; /* angle to throw at for rolling, in degrees. */
	vec3_t delta;
	float d, h, g, v, alpha, theta, vx, vy;
	float k, gd2, len;

	/* calculate target distance and height */
	h = at[2] - from[2];
	VectorSubtract(at, from, delta);
	delta[2] = 0;
	d = VectorLength(delta);

	/* check that it's not degenerate */
	if (d == 0) {
		return 0;
	}

	/* precalculate some useful values */
	g = GRAVITY;
	gd2 = g * d * d;
	len = sqrt(h * h + d * d);

	/* are we rolling? */
	if (rolled) {
		alpha = rollAngle * torad;
		theta = atan2(d, -h) - 2 * alpha;
		k = gd2 / (len * cos(theta) - h);
		if (k <= 0)	/* impossible shot at any velocity */
			return 0;
		v = sqrt(k);
	} else {
		/* firstly try with the maximum speed possible */
		v = speed;
		k = (v * v * h + gd2) / (v * v * len);

		/* check whether the shot's possible */
		if (launched && k >= -1 && k <= 1) {
			/* it is possible, so calculate the angle */
			alpha = 0.5 * (atan2(d, -h) - acos(k));
		} else {
			/* calculate the minimum possible velocity that would make it possible */
			alpha = 0.5 * atan2(d, -h);
			v = sqrt(gd2 / (len - h));
		}
	}

	/* calculate velocities */
	vx = v * cos(alpha);
	vy = v * sin(alpha);
	VectorNormalize(delta);
	VectorScale(delta, vx, v0);
	v0[2] = vy;

	/* prevent any rounding errors */
	VectorNormalize(v0);
	VectorScale(v0, v - DIST_EPSILON, v0);

	/* return time */
	return d / vx;
}
