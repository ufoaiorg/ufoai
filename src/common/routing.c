/**
 * @file common/routing.c
 * @brief grid pathfinding and routing
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include "routing.h"

static trace_t tr_obstruction;

/*
===============================================================================
MAP TRACING DEBUGGING TABLES
===============================================================================
*/

crossPoints_t brushesHit;
qboolean debugTrace = qfalse;


/*
==========================================================
  GRID ORIENTED MOVEMENT AND SCANNING
==========================================================
*/

/**
 * @brief  Dumps contents of a map to console for inspection.
 * @sa Grid_RecalcRouting
 * @param[in] map The routing map (either server or client map)
 * @param[in] lx  The low end of the x range updated
 * @param[in] ly  The low end of the y range updated
 * @param[in] lz  The low end of the z range updated
 * @param[in] hx  The high end of the x range updated
 * @param[in] hy  The high end of the y range updated
 * @param[in] hz  The high end of the z range updated
 */
void Grid_DumpMap (struct routing_s *map, int actor_size, int lx, int ly, int lz, int hx, int hy, int hz)
{
	int x, y, z;

	Com_Printf("\nGrid_DumpMap (%i %i %i) (%i %i %i)\n", lx, ly, lz, hx, hy, hz);
	for (z = hz; z >= lz; --z) {
		Com_Printf("\nLayer %i:\n   ", z);
		for (x = lx; x <= hx; ++x) {
			Com_Printf("%9i", x);
		}
		Com_Printf("\n");
		for (y = hy; y >= ly; --y) {
			Com_Printf("%3i ", y);
			for (x = lx; x <= hx; ++x) {
				Com_Printf("%s%s%s%s "
					, RT_CONN_NX(map, actor_size, x, y, z) ? "w" : " "
					, RT_CONN_PY(map, actor_size, x, y, z) ? "n" : " "
					, RT_CONN_NY(map, actor_size, x, y, z) ? "s" : " "
					, RT_CONN_PX(map, actor_size, x, y, z) ? "e" : " "
					);
			}
			Com_Printf("\n");
		}
	}
}


/**
 * @brief  Dumps contents of the entire client map to console for inspection.
 * @param[in] map A pointer to the map being dumped
 */
void Grid_DumpWholeMap (routing_t *map)
{
	vec3_t mins, maxs, normal, origin;
	pos3_t start, end, test;
	trace_t trace;
	int i;

	/* Initialize start, end, and normal */
	VectorSet(start, 0, 0, 0);
	VectorSet(end, PATHFINDING_WIDTH - 1, PATHFINDING_WIDTH - 1, PATHFINDING_HEIGHT - 1);
	VectorSet(normal, UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2);
	VectorCopy(vec3_origin, origin);

	for (i = 0; i < 3; i++) {
		/* Lower positive boundary */
		while (end[i]>start[i]) {
			/* Adjust ceiling */
			VectorCopy(start, test);
			test[i] = end[i] - 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(test, mins);
			VectorSubtract(mins, normal, mins);
			PosToVec(end, maxs);
			VectorAdd(maxs, normal, maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, lower the boundary. */
			end[i]--;
		}

		/* Raise negative boundary */
		while (end[i]>start[i]) {
			/* Adjust ceiling */
			VectorCopy(end, test);
			test[i] = start[i] + 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(start, mins);
			VectorSubtract(mins, normal, mins);
			PosToVec(test, maxs);
			VectorAdd(maxs, normal, maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* Dump the client map */
	Grid_DumpMap(map, 0, start[0], start[1], start[2], end[0], end[1], end[2]);
}


/**
 * @brief Calculate the map size via model data and store grid size
 * in map_min and map_max. This is done with every new map load
 * @param[out] map_min The lower extents of the current map.
 * @param[out] map_max The upper extents of the current map.
 * @sa CMod_LoadRouting
 * @sa DoRouting
 */
void RT_GetMapSize (vec3_t map_min, vec3_t map_max)
{
	const vec3_t normal = {UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2};
	pos3_t start, end, test;
	vec3_t mins, maxs, origin;
	int i;
	trace_t trace;

	/* Initialize start, end, and normal */
	VectorSet(start, 0, 0, 0);
	VectorSet(end, PATHFINDING_WIDTH - 1, PATHFINDING_WIDTH - 1, PATHFINDING_HEIGHT - 1);
	VectorCopy(vec3_origin, origin);

	for (i = 0; i < 3; i++) {
		/* Lower positive boundary */
		while (end[i] > start[i]) {
			/* Adjust ceiling */
			VectorCopy(start, test);
			test[i] = end[i] - 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(test, mins);
			VectorSubtract(mins, normal, mins);
			PosToVec(end, maxs);
			VectorAdd(maxs, normal, maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, lower the boundary. */
			end[i]--;
		}

		/* Raise negative boundary */
		while (end[i] > start[i]) {
			/* Adjust ceiling */
			VectorCopy(end, test);
			test[i] = start[i] + 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(start, mins);
			VectorSubtract(mins, normal, mins);
			PosToVec(test, maxs);
			VectorAdd(maxs, normal, maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* Com_Printf("Extents: (%i, %i, %i) to (%i, %i, %i)\n", start[0], start[1], start[2], end[0], end[1], end[2]); */

	/* convert to vectors */
	PosToVec(start, map_min);
	PosToVec(end, map_max);

	/* Stretch to the exterior edges of our extents */
	VectorSubtract(map_min, normal, map_min);
	VectorAdd(map_max, normal, map_max);
}


/*
===============================================================================
NEW MAP TRACING FUNCTIONS
===============================================================================
*/

/**
 * @brief This function looks to see if an actor of a given size can occupy a cell(s) and if so
 *	identifies the floor and ceiling for that cell. If the cell has no floor, the floor will be negative
 *  with 0 indicating the base for the cell(s).  If there is no ceiling in the cell, the first ceiling
 *  found above the current cell will be used.  If there is no ceiling above the cell, the ceiling will
 *  be the top of the model.  This function will also adjust all floor and ceiling values for all cells
 *  between the found floor and ceiling.
 * @param[in] x The x position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] y The y position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] z The z position in the routing arrays (0 - PATHFINDING_HEIGHT-1)
 * @param[in] size The size of the actor along the X and Y axis in cell units
 * @return The z value of the next cell to scan, usually the cell with the ceiling.
 * @sa Grid_RecalcRouting
 */
int RT_CheckCell (routing_t * map, const int actor_size, const int x, const int y, const int z)
{
	vec3_t start, end; /* Start and end of the traces */
	vec3_t bmin, bmax; /* Extents of the ceiling box being traced */
	/* Extents of the floor box being traced */
	const vec3_t bmin2 = {-2 + DIST_EPSILON, -2 + DIST_EPSILON, 0};
	const vec3_t bmax2 = {2 - DIST_EPSILON, 2 - DIST_EPSILON, 0};
	vec3_t tstart, tend;
	pos3_t pos;
	float bottom, top; /* Floor and ceiling model distances from the cell base. */
	float initial; /* Cell floor and ceiling z coordinate. */
	int i;
	int fz, cz; /* Floor and celing Z cell coordinates */
	trace_t tr;

	assert(map);
	/* x and y cannot exceed PATHFINDING_WIDTH - actor_size */
	assert((x >= 0) && (x <= PATHFINDING_WIDTH - actor_size));
	assert((y >= 0) && (y <= PATHFINDING_WIDTH - actor_size));
	assert(z < PATHFINDING_HEIGHT);

	/* calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	SizedPosToVec(pos, actor_size, end); /** end is now at the center of the cells the actor occupies. */

	/* prepare fall down check */
	VectorCopy(end, start);
	/*
	 * Adjust these points so that start to end is from the top of the cell to the bottom of the model.
	 */
	initial = start[2] + UNIT_HEIGHT / 2; /* This is the top-most starting point. */
	start[2] += UNIT_HEIGHT / 2 - QUANT;
	end[2] = -UNIT_HEIGHT * 2; /* To the bottom of the model! (Plus some for good measure) */


	/* Configure bmin and bmax for the main ceiling scan */
	VectorSet(bmin, UNIT_SIZE * actor_size / -2 + WALL_SIZE + DIST_EPSILON, UNIT_SIZE * actor_size / -2 + WALL_SIZE + DIST_EPSILON, 0);
	VectorSet(bmax, UNIT_SIZE * actor_size /  2 - WALL_SIZE - DIST_EPSILON, UNIT_SIZE * actor_size /  2 - WALL_SIZE - DIST_EPSILON, 0);


	/*
	 * Trace for a floor.  Steps:
	 * 1. Start at the top of the designated cell and scan twoard the model's base.
	 * 2. If we do not find a brush, then this cell is bottomless and not enterable.
	 * 3. We have found an upward facing brush.  Scan up PATHFINDING_MIN_STEPUP height.
	 * 4. If we find anything, then this space is too small of an opening.  Restart just below our current floor.
	 * 5. Trace up towards the model ceiling with a box as large as the actor.  The first obstruction encountered
	 *      marks the ceiling.  If there are no obstructions, the model ceiling is the ceiling.
	 * 6. If the opening between the floor and the ceiling is not at least PATHFINDING_MIN_OPENING tall, then
	 *      restart below the current floor.
	 */
	while (qtrue) { /* Loop forever, we will exit if we hit the model bottom or find a valid floor. */
		if (debugTrace)
			Com_Printf("[(%i, %i, %i, %i)]Casting floor (%f, %f, %f) to (%f, %f, %f)\n",
				x, y, z, actor_size, start[0], start[1], start[2], end[0], end[1], end[2]);

		tr = RT_COMPLETEBOXTRACE(start, end, bmin2, bmax2, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);
		if (tr.fraction >= 1.0) {
			/* There is no brush underneath this starting point. */
			if (debugTrace)
				Com_Printf("Reached bottom of map.  No floor in cell(s). %f\n", tr.endpos[2]);
			/* Mark all cells to the model base as filled. */
			for (i = z; i >= 0 ; i--) {
				/* no floor in this cell, it is bottomless! */
				RT_FLOOR(map, actor_size, x, y, i) = -1 - i * CELL_HEIGHT; /* There is no floor in this cell, place it at -1 below the model. */
				RT_CEILING(map, actor_size, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
			}
			/* return 0 to indicate we just scanned the model bottom. */
			return 0;
		}

		/* We have hit a brush that faces up and can be stood on. Look for a ceiling. */
		bottom = tr.endpos[2];  /* record the floor position. */

		/* Record the debugging data in case it is used. */
		VectorCopy(tr.endpos, brushesHit.floor);

		assert(initial > bottom);

		if (debugTrace)
			Com_Printf("Potential floor found at %f.\n", bottom);

		/* Prep the start and end of the "leg room" test. */
		VectorCopy(start, tstart);
		tstart[2] = bottom;
		VectorCopy(tstart, tend);
		tend[2] += PATHFINDING_MIN_STEPUP * QUANT; /* tend is now MIN_STEPUP above tstart */

		if (debugTrace)
			Com_Printf("    Casting leg room (%f, %f, %f) to (%f, %f, %f)\n",
				tstart[0], tstart[1], tstart[2], tend[0], tend[1], tend[2]);
		tr = RT_COMPLETEBOXTRACE(tstart, tend, bmin2, bmax2, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);
		if (tr.fraction < 1.0) {
			if (debugTrace)
				Com_Printf("Cannot use found surface- stepup obstruction found at %f.\n", tr.endpos[2]);
			/*
			 * There is a premature obstruction.  We can't use this as a floor.
			 * Check under start.  We need to have at least the minimum amount of clearance from our ceiling,
			 * So shart at that point.
			 */
			start[2] = tr.endpos[2] - PATHFINDING_MIN_OPENING * QUANT;
			/* Check in case we are trying to scan too close to the bottom of the model. */
			if (start[2] <= PATHFINDING_MIN_OPENING) {
				/* There is no brush underneath this starting point. */
				if (debugTrace)
					Com_Printf("Reached bottom of map.  No floor in cell(s).\n");
				/* Mark all cells to the model base as filled. */
				for (i = z; i >= 0 ; i--) {
					/* no floor in this cell, it is bottomless! */
					RT_FLOOR(map, actor_size, x, y, i) = CELL_HEIGHT; /* There is no floor in this cell. */
					RT_CEILING(map, actor_size, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
				}
				/* return 0 to indicate we just scanned the model bottom. */
				return 0;
			}

			/* Restart */
			continue;
		}

		/*
		 * If we are here, then the immediate floor is unobstructed STEPUP units high.
		 * Scan for the ceiling with a box as big as the actor and check that the total
		 * distance from floor to ceiling is at least PATHIFINDING_MIN_OPENING.
		 */

		tstart[2] = tend[2]; /* The box trace for height starts at stepup height. */
		tend[2] = PATHFINDING_HEIGHT * UNIT_HEIGHT; /* tend now reaches the model ceiling. */

		if (debugTrace)
			Com_Printf("    Casting ceiling (%f, %f, %f) to (%f, %f, %f)\n",
				tstart[0], tstart[1], tstart[2], tend[0], tend[1], tend[2]);

		tr = RT_COMPLETEBOXTRACE(tstart, tend, bmin, bmax, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);

		/*
		 * There is one last possibility:
		 * If our found ceiling is above the cell we started the scan in, then we may have scanned up through another
		 * floor (one sided brush).  If this is the case, we set the ceiling to QUANT below the floor of the above
		 * ceiling if it is lower than our found ceiling.
		 */
		if (tr.endpos[2] > (z + 1) * UNIT_HEIGHT)
			tr.endpos[2] = min(tr.endpos[2], (z + 1) * UNIT_HEIGHT + (RT_FLOOR(map, actor_size, x, y, z + 1) - 1) * QUANT);

		if (tr.endpos[2] - bottom < PATHFINDING_MIN_OPENING * QUANT) { /* Case 1 */
			if (debugTrace)
				Com_Printf("Ceiling not at least PATHING_MIN_OPENING; surface found at %f.\n", tr.endpos[2]);
			start[2] = tr.endpos[2] - PATHFINDING_MIN_OPENING * QUANT; /* Move start the minimum opening distance below our ceiling. */
			/* Repeat the floor test */
			continue;
		}

		/* We found a valid ceiling. */
		top = tr.endpos[2];

		/* Record the debugging data in case it is used. */
		VectorCopy(tr.endpos, brushesHit.ceiling);

		/* exit the infinite while loop */
		break;
	}

	assert(bottom <= top);

	/* top and bottom are absolute model heights.  Find the actual cell z coordinates for these heights. */
	fz = (int)(floor(bottom / UNIT_HEIGHT));
	cz = min(z, (int)(ceil(top / UNIT_HEIGHT) - 1)); /* Use the lower of z or the calculated ceiling */

	assert(fz <= cz);

	if (debugTrace)
		Com_Printf("Valid ceiling found, bottom=%f, top=%f, fz=%i, cz=%i.\n", bottom, top, fz, cz);

	/* Last, update the floors and ceilings of cells from (x, y, fz) to (x, y, cz) */
	for (i = fz; i <= cz; i++) {
		/* Round up floor to keep feet out of model. */
		RT_FLOOR(map, actor_size, x, y, i) = (int)ceil((bottom - i * UNIT_HEIGHT) / QUANT);
		/* Round down ceiling to heep head out of model.  Also offset by floor and max at 255. */
		RT_CEILING(map, actor_size, x, y, i) = (int)floor((top - i * UNIT_HEIGHT) / QUANT);
		if (debugTrace) {
			Com_Printf("floor(%i, %i, %i, %i)=%i.\n", x, y, i, actor_size, RT_FLOOR(map, actor_size, x, y, i));
			Com_Printf("ceil(%i, %i, %i, %i)=%i.\n", x, y, i, actor_size, RT_CEILING(map, actor_size, x, y, i));
		}
	}

	/* Also, update the floors of any filled cells immediately above the ceiling up to our original cell. */
	for (i = cz + 1; i <= z; i++) {
		RT_FLOOR(map, actor_size, x, y, i) = CELL_HEIGHT; /* There is no floor in this cell. */
		RT_CEILING(map, actor_size, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
		if (debugTrace) {
			Com_Printf("floor(%i, %i, %i)=%i.\n", x, y, i, RT_FLOOR(map, actor_size, x, y, i));
			Com_Printf("ceil(%i, %i, %i)=%i.\n", x, y, i, RT_CEILING(map, actor_size, x, y, i));
		}
	}

	/* Return the lowest z coordinate that we updated floors for. */
	return fz;
}


/**
 * @brief Helper function to trace for walls
 * @param[in] start The starting point of the trace, at the FLOOR'S CENTER.
 * @param[in] end The end point of the trace, centered x and y at the destination but at the same height as start.
 * @param[in] hi The upper height ABOVE THE FLOOR of the bounding box.
 * @param[in] lo The lower height ABOVE THE FLOOR of the bounding box.
 */
static qboolean RT_ObstructedTrace (const vec3_t start, const vec3_t end, int actor_size, int hi, int lo)
{
	vec3_t bmin, bmax;
	int hz, lz;
	int bitmask = 0x100; /**< Trace the clip levels by default */

	/* Configure the box trace extents. The box is relative to the original floor. */
	VectorSet(bmax, UNIT_SIZE * actor_size / 2 - WALL_SIZE - DIST_EPSILON, UNIT_SIZE * actor_size / 2 - WALL_SIZE - DIST_EPSILON, hi * QUANT - DIST_EPSILON);
	VectorSet(bmin, -UNIT_SIZE * actor_size / 2 + WALL_SIZE + DIST_EPSILON, -UNIT_SIZE * actor_size / 2 + WALL_SIZE + DIST_EPSILON, lo * QUANT + DIST_EPSILON);

	/* Calculate the needed bitmask. */
	/* Include levels up to the hi mark. */
	hz = floor((hi + start[2]) / UNIT_SIZE);
	bitmask |= (1 << (hz + 1)) -1;
	/* Remove levels below the lo mark */
	lz = floor((lo + start[2]) / UNIT_SIZE);
	if (lz > 0)
		bitmask ^= (1 << lz) -1;

	/* perform the trace, then return true if the trace was obstructed. */
	tr_obstruction = RT_COMPLETEBOXTRACE(start, end, bmin, bmax, bitmask, MASK_IMPASSABLE, MASK_PASSABLE);

	return tr_obstruction.fraction < 1.0;
}


/**
 * @brief Routing Function to update the connection between two fields
 * @param[in] map Routing field of the current loaded map
 * @param[in] actor_size The size of the actor, in units
 * @param[in] x The x position in the routing arrays (0 to PATHFINDING_WIDTH - actor_size)
 * @param[in] y The y position in the routing arrays (0 to PATHFINDING_WIDTH - actor_size)
 * @param[in] z The z position in the routing arrays (0 to PATHFINDING_HEIGHT - 1)
 * @param[in] dir The direction to test for a connection through
 */
int RT_UpdateConnection (routing_t * map, const int actor_size, const int x, const int y, const int z, const int dir)
{
	vec3_t start, end;
	pos3_t pos;
	int i;
	int h, c; /**< height, ceiling */
	int dz = z; /**< destination z, changed if the floor in the starting cell is high enough. */
	int ah, ac; /**< attributes of the destination cell */
	int fz, cz; /**< Floor and celing Z cell coordinates */
	int floor_p, ceil_p; /**< floor of the passage, ceiling of the passage. */
	int hi, lo, worst; /**< used to find floor and ceiling of passage */
	int maxh; /**< The higher value of h and ah */
	qboolean obstructed;

	/* get the neighbor cell's coordinates */
	const int ax = x + dvecs[dir][0];
	const int ay = y + dvecs[dir][1];

	assert(map);
	assert((x >= 0) && (x <= PATHFINDING_WIDTH - actor_size));
	assert((y >= 0) && (y <= PATHFINDING_WIDTH - actor_size));
	assert(z < PATHFINDING_HEIGHT);

	/* Ensure that the current coordinates are valid. */
	RT_CONN_TEST(map, actor_size, x, y, z, dir);

	if (debugTrace)
		Com_Printf("\n(%i, %i, %i) to (%i, %i, %i) as:%i\n", x, y, z, ax, ay, z, actor_size);

	/* Com_Printf("At (%i, %i, %i) looking in direction %i with size %i\n", x, y, z, dir, actor_size); */

	/* test if the unit is blocked by a loaded model */
	if (RT_FLOOR(map, actor_size, x, y, z) >= CELL_HEIGHT || RT_CEILING(map, actor_size, x, y, z) - RT_FLOOR(map, actor_size, x, y, z) < PATHFINDING_MIN_OPENING){
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Zero the debugging data */
		VectorSet(brushesHit.floor, 0, 0, 0);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Current cell filled. f:%i c:%i\n", RT_FLOOR(map, actor_size, x, y, z), RT_CEILING(map, actor_size, x, y, z));
		return z;
	}

	/* if our destination cell is out of bounds, bail. */
	if (ax < 0 || ax > PATHFINDING_WIDTH - actor_size || ay < 0 || y > PATHFINDING_WIDTH - actor_size) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Zero the debugging data */
		VectorSet(brushesHit.floor, 0, 0, 0);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Destination cell non-existant.\n");
		return z;
	}

	/* Ensure that the destination coordinates are valid. */
	RT_CONN_TEST(map, actor_size, ax, ay, z, dir);

	/* get the originating floor's absolute height */
	h = max(0, RT_FLOOR(map, actor_size, x, y, z)) + z * CELL_HEIGHT;

	/* get the originating ceiling's absolute height */
	c = RT_CEILING(map, actor_size, x, y, z) + z * CELL_HEIGHT;

	/* Check if we can step up into a higher cell.  Criteria are:
	 * 1. The floor in the current cell is high enough to stepup into the next cell.
	 * 2. The floor in the cell above the destination cell is low enough to be steped onto.
	 * 3. The floor in the cell above the destination cell is in that cell.
	 * 4. We are not at the highest z.
	 */
	if (z < PATHFINDING_HEIGHT - 1 && h < (z + 1) * CELL_HEIGHT
	 && RT_FLOOR(map, actor_size, ax, ay, z + 1) >= 0
	 && h + PATHFINDING_MIN_STEPUP >= RT_FLOOR(map, actor_size, ax, ay, z + 1) + (z + 1) * CELL_HEIGHT) {
		dz++;
		if (debugTrace)
			Com_Printf("Adjusting dz for stepup. z = %i, dz = %i\n", z, dz);
	}

	/* get the destination floor's absolute height */
	ah = max(0, RT_FLOOR(map, actor_size, ax, ay, dz)) + dz * CELL_HEIGHT;

	/* get the destination ceiling's absolute height */
	ac = RT_CEILING(map, actor_size, ax, ay, dz) + dz * CELL_HEIGHT;

	/* Set maxh to the larger of h and ah. */
	maxh = max(h, ah);


	/* Calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	SizedPosToVec(pos, actor_size, start);
	start[2] = maxh * QUANT; /** start is now at the center of the floor of the originating cell and as high as the higher of the two floors. */

	/* Locate the destination point. */
	VectorSet(pos, ax, ay, dz);
	SizedPosToVec(pos, actor_size, end);
	end[2] = start[2]; /** end is now at the center of the destination cell and at the same height as the starting point. */

	/* Test to see if c <= h or ac <= ah - indicates a filled cell. */
	if (c <= h || ac <= ah) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Destination cell filled. h = %i, c = %i, ah = %i, ac = %i\n", h, c, ah, ac);
		return z;
	}

	/* test if the destination cell is blocked by a loaded model */
	else if (h >= ac || ah >= c) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Destination cell filled. h = %i, c = %i, ah = %i, ac = %i\n", h, c, ah, ac);
		return z;
	}

	/* If the destination floor is more than PATHFINDING_MIN_STEPUP higher than the base of the current cell
	 * then we can't go there. */
	else if (ah - h > PATHFINDING_MIN_STEPUP) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		VectorCopy(end, brushesHit.floor);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Destination cell too high up. h:%i ah:%i\n", h, ah);
		return z;
	}

	/* Point this debugging pointer to where we need be. */
	VectorCopy(end, brushesHit.floor);
	VectorCopy(brushesHit.floor, brushesHit.ceiling);

	/* test if the destination cell is blocked by a loaded model */
	if (ac - h < PATHFINDING_MIN_OPENING) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Destination cell filled. h:%i ac:%i\n", h, ac);
		return z;
	}

	if (debugTrace)
		Com_Printf("h = %i, c = %i, ah = %i, ac = %i\n", h, c, ah, ac);

	/*
	 * OK, here's the plan: we can move between cells, provided that there are no obstructions
	 * highter than PATHFINDING_MIN_STEPUP from the floor of the current cell into the destination cell.  The cell being moved
	 * into counts as an obstruction, so you can't move into it if it is too tall.
	 * We start by finding the highest height of floor between the two cells.  We then start testing
	 * QUANT tall layers with box traces starting at the stepup height and move down.
	 * The first obstacle found marks the floor of the passage to the adjacent cell.
	 * Next, we again use the box tracing, but move up until we reach the lowest ceiling starting at
	 * the point just above the stepup height.  First obstruction marks the ceiling of the passage.
	 * The passage must be at least PATHING_MIN_OPENING tall.  We will connect all cells that a fall
	 * greater than PATHFINDING_MAX_FALL would occur, but we will check that in the A* code now.
	 * This will allow for flying units later.
	 */


	/*
	 * Part A: Look for the mandatory opening. Low point at the higher floor plus PATHFINDING_MIN_STEPUP,
	 * high point at original floor plus PATHFINDING_MIN_OPENING. If this is not open, there is no way to
	 * enter the cell.  There cannot be any obstructions above PATHFINDING_MIN_STEPUP because
	 * the floor would be too high to enter, and there cannot be obstructions below
	 * PATHFINDING_MIN_OPENING, otherwise the ceiling in the adjacent cell would be too low to enter.
	 * Technically, yes the total space checked is not PATHFINDING_MIN_OPENING tall, but
	 * above and below this are checked separately to verify the minimum height requirement.
	 * Remember fhat our trace line is height adjusted, so our test heights are relative to the floor!
	 */
	floor_p = PATHFINDING_MIN_STEPUP;
	ceil_p = PATHFINDING_MIN_OPENING;
	if (RT_ObstructedTrace(start, end, actor_size, ceil_p, floor_p)) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;

		/* Record the debugging data in case it is used. */
		VectorCopy(tr_obstruction.endpos, brushesHit.floor);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;

		if (debugTrace)
			Com_Printf("No opening in required space between cells. hi:%i lo:%i\n", ceil_p + maxh , floor_p + maxh);
		return z;
	}

	/*
	 * Part B: Look for additional space below PATHFINDING_MIN_STEPUP. Low points at the higher of the two floors;
	 * high points at the current floor_p, the current lowest valid floor.
	 */
	lo = max(0, ah - h); /* Relative to the current floor h */
	hi = floor_p;
	worst = lo;
	/* Move the floor brush to the lowest possible floor. */
	brushesHit.ceiling[2] += lo * QUANT;
	if (debugTrace)
		Com_Printf("Checking for passage floor from %i to %i.\n", lo, hi);
	while (hi > lo) {
		if (debugTrace)
			Com_Printf("Looking between %i and %i at (%i, %i) facing %i... ", lo, hi, x, y, dir);
		obstructed = RT_ObstructedTrace(start, end, actor_size, hi, lo);
		if (obstructed) {
			VectorCopy(tr_obstruction.endpos, brushesHit.floor);
			if (debugTrace)
				Com_Printf("found obstruction.\n");
			/* if hi and lo differ by one, then we are done.*/
			if (hi == lo + 1) {
				if (debugTrace)
					Com_Printf("Last possible block scanned.\n");
				break;
			}
			/* Since we cant use this, place lo halfway between hi and lo. */
			lo = (hi + lo) / 2;
		} else {
			/* Since we can use this, lower floor_p to lo and test between worst and floor_p. */
			floor_p = lo;
			if (debugTrace)
				Com_Printf("no obstructions found, floor_p = %i.\n", floor_p);
			hi = floor_p;
			lo = worst;
		}
	}
	/* We need the absolute height of floor_p, not relative to the current floor */
	floor_p += h;

	/*
	 * Part C: Look for additional space above PATHFINDING_MIN_STEPUP. Low points to ceil_p, the current
	 * valid ceiling; high points at the lower of both ceilings.
	 */
	lo = ceil_p;
	hi = min(c, ac) - h;
	worst = hi;
	/* Move the ceiling brush to the highest possible ceiling. */
	brushesHit.ceiling[2] += hi * QUANT;
	if (debugTrace)
		Com_Printf("Checking for passage ceiling from %i to %i.\n", lo, hi);
	while (hi > lo) {
		if (debugTrace)
			Com_Printf("Looking between %i and %i at (%i, %i) facing %i... ", lo, hi, x, y, dir);
		obstructed = RT_ObstructedTrace(start, end, actor_size, hi, lo);
		if (obstructed) {
			/* Record the debugging data in case it is used. */
			VectorCopy(tr_obstruction.endpos, brushesHit.ceiling);
			if (debugTrace)
				Com_Printf("found obstruction.\n");
			/* if hi and lo differ by one, then we are done.*/
			if (hi == lo + 1) {
				if (debugTrace)
					Com_Printf("Last possible block scanned.\n");
				break;
			}
			/* Since we cant use this, place hi halfway between hi and lo. */
			hi = (hi + lo) / 2;
		} else {
			/* Since we can use this, raise ceil_p to hi and test between worst and ceil_p. */
			ceil_p = hi;
			if (debugTrace)
				Com_Printf("no obstructions found, ceil_p = %i.\n", ceil_p);
			hi = worst;
			lo = ceil_p;
		}
	}
	/* We need the absolute height of ceil_p, not relative to the current floor */
	ceil_p += h;

	/* Final interpretation:
	 * We now have the floor and the ceiling of the passage traveled between the two cells.
	 * This span may cover many cells vertically.  We can use this to our advantage:
	 * +Like in the floor tracing, we can assign the direction value for multiple cells and
	 *  skip some scans.
	 * +The value of each current cell will list the max allowed height of an actor in the passageway,
	 *  which also can be used to see if an actor can fly upward.
	 * +The allowed height will be based off the floor in the cell or the bottom of the cell; we do not
	 *  want super tall characters to fly through ceilings.
	 * +To see if an actor can fly down, we check the cells on level down to see if the diagonal movement
	 *  can be made and that both have ceilings above the current level.
	 */

	/* floor_p and ceil_p are absolute step heights.  Find the actual cell z coordinates for these heights.
	 * Use h instead of floor_p, stepping into the new cell my move us up one z unit
	 * Also reduce ceil_p by one because ceilings divisible by CELL_HEIGHT are only in the cells below.
	 */
	fz = h * QUANT / UNIT_HEIGHT;
	cz = (ceil_p - 1) * QUANT / UNIT_HEIGHT;

	/* last chance- if cz < z, then bail (and there is an error with the ceiling data somewhere */
	if (cz < z) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, ax, ay, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (debugTrace)
			Com_Printf("Passage found but below current cell, h = %i, c = %i, ah = %i, ac = %i, floor_p=%i, ceil_p=%i, z = %i, cz = %i.\n", h, c, ah, ac, floor_p, ceil_p, z, cz);
		return z;
	}

	/* Point to where we CAN be. */
	brushesHit.obstructed = qfalse;

	if (debugTrace)
		Com_Printf("Passage found, floor_p=%i, ceil_p=%i. (%i to %i)\n", floor_p, ceil_p, fz, cz);

	assert(fz <= z && z <= cz);

	/* Last, update the routes of cells from (x, y, fz) to (x, y, cz) for direction dir */
	for (i = fz; i <= cz; i++) {
		/* Offset from the floor or the bottom of the current cell, whichever is higher. */
		RT_CONN_TEST(map, actor_size, x, y, i, dir);
		RT_CONN(map, actor_size, x, y, i, dir) = ceil_p - max(floor_p, i * CELL_HEIGHT);
		if (debugTrace)
			Com_Printf("RT_CONN for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, i, actor_size, dir, RT_CONN(map, actor_size, x, y, i, dir));
	}

	/*
	 * Return the highest z coordinate scanned- cz if fz==cz, z==cz, or the floor in cz is negative.
	 * Otherwise cz - 1 to recheck cz in case there is a floor in cz with its own ceiling.
	 */
	/** @todo: Ensure that walls that can be moved OVER by fliers in fact can be moved over.
	* Current code always goes to the floor of the source cell.*/
	if (fz == cz || z == cz || RT_FLOOR(map, actor_size, x, y, cz) < 0)
		return cz;
	return cz - 1;
}


void RT_WriteCSVFiles(struct routing_s *map, const char* baseFilename, const ipos3_t mins, const ipos3_t maxs){
	char filename[MAX_OSPATH], ext[MAX_OSPATH];
	FILE *handle;
	int i, x, y, z;

	/* An elevation files- dumps the floor and ceiling levels relative to each cell. */
	for (i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.elevation.csv", i);
		COM_DefaultExtension(filename, sizeof(filename), ext);
		handle = fopen(filename, "w");
		if (!handle)
			Sys_Error("Could not open file %s.", filename);
		fprintf(handle, ",");
		for (x = mins[0]; x <= maxs[0] - i + 1; x++)
			fprintf(handle, "x:%i,", x);
		fprintf(handle, "\n");
		for (z = maxs[2]; z >= mins[2]; z--) {
			for (y = maxs[1]; y >= mins[1] - i + 1; y--) {
				fprintf(handle, "z:%i  y:%i,", z ,y);
				for (x = mins[0]; x <= maxs[0] - i + 1; x++) {
					/* compare results */
					fprintf(handle, "h:%i c:%i,", RT_FLOOR(map, i, x, y, z), RT_CEILING(map, i, x, y, z));
				}
				fprintf(handle, "\n");
			}
			fprintf(handle, "\n");
		}
		fclose(handle);
	}

	/* Output the walls/passage files. */
	for (i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.walls.csv", i);
		COM_DefaultExtension(filename, sizeof(filename), ext);
		handle = fopen(filename, "w");
		if (!handle)
			Sys_Error("Could not open file %s.", filename);
		fprintf(handle, ",");
		for (x = mins[0]; x <= maxs[0] - i + 1; x++)
			fprintf(handle, "x:%i,", x);
		fprintf(handle, "\n");
		for (z = maxs[2]; z >= mins[2]; z--) {
			for (y = maxs[1]; y >= mins[1] - i + 1; y--) {
				fprintf(handle, "z:%i  y:%i,", z ,y);
				for (x = mins[0]; x <= maxs[0] - i + 1; x++) {
					/* compare results */
					fprintf(handle, "\"");

					/* NW corner */
					fprintf(handle, "%3i ", RT_CONN_NX_PY(map, i, x, y, z));

					/* N side */
					fprintf(handle, "%3i ", RT_CONN_PY(map, i, x, y, z));

					/* NE corner */
					fprintf(handle, "%3i ", RT_CONN_PX_PY(map, i, x, y, z));

					fprintf(handle, "\n");

					/* W side */
					fprintf(handle, "%3i ", RT_CONN_NX(map, i, x, y, z));

					/* Center - display floor height */
					fprintf(handle, "_%+2i_ ", RT_FLOOR(map, i, x, y, z));

					/* E side */
					fprintf(handle, "%3i ", RT_CONN_PX(map, i, x, y, z));

					fprintf(handle, "\n");

					/* SW corner */
					fprintf(handle, "%3i ", RT_CONN_NX_NY(map, i, x, y, z));

					/* S side */
					fprintf(handle, "%3i ", RT_CONN_NY(map, i, x, y, z));

					/* SE corner */
					fprintf(handle, "%3i ", RT_CONN_PX_NY(map, i, x, y, z));

					fprintf(handle, "\",");
				}
				fprintf(handle, "\n");
			}
			fprintf(handle, "\n");
		}
		fclose(handle);
	}
}
