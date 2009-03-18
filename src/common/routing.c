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

/*
===============================================================================
MAP TRACING DEBUGGING TABLES
===============================================================================
*/

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

	/* Last, update the floors and ceilings of cells from (x, y, fz) to (x, y, cz)
	 * ...but before rounding, give back the DIST_EPSILON that was added by the trace
	 * Well, give back two DIST_EPSILON just to be sure */
	bottom -= 2 * DIST_EPSILON;
	top += 2 * DIST_EPSILON;
	/* ...but before rounding, give back the DIST_EPSILON that was added by the trace */
	/* Well, give back two DIST_EPSILON just to be sure */
	bottom -= 2*DIST_EPSILON;
	top += 2*DIST_EPSILON;
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
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] map The map's routing data
 * @param[in] actor_size The actor's size
 * @param[in] dir Direction of movement
 * @param[in] x Starting x coordinate
 * @param[in] y Starting y coordinate
 * @param[in] z Starting z coordinate
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] az Ending z coordinate
 * @param[in] opening_size Absolute height in QUANT units of the opening.
 * @param[in] opening_base Absolute height in QUANT units of the bottom of the opening.
 * @param[in] stepup Required stepup to travel in this direction.
 */
static int RT_FillPassageData(routing_t * map, const int actor_size, const int dir, const int  x, const int y, const int z, const int opening_size, const int opening_base, const int stepup)
{
	const int opening_top = opening_base + opening_size;
	int fz, cz; /**< Floor and celing Z cell coordinates */
	int i;

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

	fz = z;
	cz = min(PATHFINDING_HEIGHT - 1, ceil((float)opening_top / CELL_HEIGHT) - 1);

	/* last chance- if cz < z, then bail (and there is an error with the ceiling data somewhere */
	if (cz < z) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		RT_STEPUP(map, actor_size, x, y, z, dir) = PATHFINDING_NO_STEPUP;
		if (debugTrace)
			Com_Printf("Passage found but below current cell, opening_base=%i, opening_top=%i, z = %i, cz = %i.\n", opening_base, opening_top, z, cz);
		return z;
	}

	if (debugTrace)
		Com_Printf("Passage found, opening_base=%i, opening_size=%i, opening_top=%i, stepup=%i. (%i to %i)\n", opening_base, opening_size, opening_top, stepup, fz, cz);

	assert(fz <= z && z <= cz);

	/* Last, update the routes of cells from (x, y, fz) to (x, y, cz) for direction dir */
	for (i = fz; i <= cz; i++) {
		/* Offset from the floor or the bottom of the current cell, whichever is higher. */
		/* Only if > 0 */
		RT_CONN_TEST(map, actor_size, x, y, i, dir);
		assert (opening_top - max(opening_base, i * CELL_HEIGHT) >= 0);
		RT_CONN(map, actor_size, x, y, i, dir) = opening_top - max(opening_base, i * CELL_HEIGHT);
		/* The stepup is 0 for all cells that are not at the floor. */
		RT_STEPUP(map, actor_size, x, y, i, dir) = 0;
		if (debugTrace) {
			Com_Printf("RT_CONN for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, i, actor_size, dir, RT_CONN(map, actor_size, x, y, i, dir));
		}
	}

	RT_STEPUP(map, actor_size, x, y, z, dir) = stepup;
	if (debugTrace) {
		Com_Printf("Final RT_STEPUP for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, z, actor_size, dir, stepup);
	}

	/*
	 * Return the highest z coordinate scanned- cz if fz==cz, z==cz, or the floor in cz is negative.
	 * Otherwise cz - 1 to recheck cz in case there is a floor in cz with its own ceiling.
	 */
	if (fz == cz || z == cz || RT_FLOOR(map, actor_size, x, y, cz) < 0)
		return cz;
	return cz - 1;
}

/**
 * @brief Helper function to trace for walls
 * @param[in] start The starting point of the trace, at the FLOOR'S CENTER.
 * @param[in] end The end point of the trace, centered x and y at the destination but at the same height as start.
 * @param[in] hi The upper height ABOVE THE FLOOR of the bounding box.
 * @param[in] lo The lower height ABOVE THE FLOOR of the bounding box.
 */
static trace_t RT_ObstructedTrace (const vec3_t start, const vec3_t end, int actor_size, int hi, int lo)
{
	vec3_t bmin, bmax; /**< Tracing box extents */

	/* Configure the box trace extents. The box is relative to the original floor. */
	VectorSet(bmax, UNIT_SIZE * actor_size / 2 - WALL_SIZE - DIST_EPSILON, UNIT_SIZE * actor_size / 2 - WALL_SIZE - DIST_EPSILON, hi * QUANT - DIST_EPSILON);
	VectorSet(bmin, -UNIT_SIZE * actor_size / 2 + WALL_SIZE + DIST_EPSILON, -UNIT_SIZE * actor_size / 2 + WALL_SIZE + DIST_EPSILON, lo * QUANT + DIST_EPSILON);

	/* perform the trace, then return true if the trace was obstructed. */
	return RT_COMPLETEBOXTRACE(start, end, bmin, bmax, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);
}


/**
 * @brief Performs a trace to find the floor of a passage a fraction of the way from start to end.
 * @param[in] start The starting coordinate to search for a floor from.
 * @param[in] end The starting coordinate to search for a floor from.
 * @param[in] actor_size The actor's size.
 * @param[in] frac The fraction of the distance traveled from start to end, using (0.0 to 1.0).
 * @param[in] starting_height The starting height for this upward trace.
 * @return The absolute height of the found floor in QUANT units.
 */
static int RT_FindOpeningFloorFrac(vec3_t start, vec3_t end, const int actor_size, const float frac, const int starting_height){
	vec3_t bmin, bmax; /**< Tracing box extents */
	vec3_t mstart, mend; /**< Midpoint line to trace across */
	trace_t tr;

	/* Configure bmin and bmax for the floor scan */
	VectorSet(bmin, UNIT_SIZE * actor_size / -2 + WALL_SIZE + DIST_EPSILON, UNIT_SIZE * actor_size / -2 + WALL_SIZE + DIST_EPSILON, 0);
	VectorSet(bmax, UNIT_SIZE * actor_size /  2 - WALL_SIZE - DIST_EPSILON, UNIT_SIZE * actor_size /  2 - WALL_SIZE - DIST_EPSILON, 0);

	/* Position mstart and mend at the fraction point */
	VectorInterpolation(start, end, frac, mstart);
	VectorCopy(mstart, mend);
	mstart[2] = starting_height * QUANT + (QUANT / 2); /* Set at the starting height, plus a little more to keep us off a potential surface. */
	mend[2] = -QUANT;  /* Set below the model. */

	tr = RT_COMPLETEBOXTRACE(mstart, mend, bmin, bmax, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);

	if (debugTrace)
		Com_Printf("Brush found at %f.\n", tr.endpos[2]);

	/* OK, now we have the floor height value in tr.endpos[2].
	 * Divide by QUANT and round up.
	 */
	return ceil((tr.endpos[2] - DIST_EPSILON) / QUANT);
}


/**
 * @brief Performs a trace to find the ceiling of a passage a fraction of the way from start to end.
 * @param[in] start The starting coordinate to search for a ceiling from.
 * @param[in] end The starting coordinate to search for a ceiling from.
 * @param[in] actor_size The actor's size.
 * @param[in] frac The fraction of the distance traveled from start to end, using (0.0 to 1.0).
 * @param[in] starting_height The starting height for this upward trace.
 * @return The absolute height of the found ceiling in QUANT units.
 */
static int RT_FindOpeningCeilingFrac(vec3_t start, vec3_t end, const int actor_size, const float frac, const int starting_height){
	vec3_t bmin, bmax; /**< Tracing box extents */
	vec3_t mstart, mend; /**< Midpoint line to trace across */
	trace_t tr;

	/* Configure bmin and bmax for the floor scan */
	VectorSet(bmin, UNIT_SIZE * actor_size / -2 + WALL_SIZE + DIST_EPSILON, UNIT_SIZE * actor_size / -2 + WALL_SIZE + DIST_EPSILON, 0);
	VectorSet(bmax, UNIT_SIZE * actor_size /  2 - WALL_SIZE - DIST_EPSILON, UNIT_SIZE * actor_size /  2 - WALL_SIZE - DIST_EPSILON, 0);

	/* Position mstart and mend at the midpoint */
	VectorInterpolation(start, end, frac, mstart);
	VectorCopy(mstart, mend);
	mstart[2] = starting_height * QUANT - (QUANT / 2); /* Set at the starting height, minus a little more to keep us off a potential surface. */
	mend[2] = UNIT_HEIGHT * PATHFINDING_HEIGHT + QUANT;  /* Set above the model. */

	tr = RT_COMPLETEBOXTRACE(mstart, mend, bmin, bmax, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);

	if (debugTrace)
		Com_Printf("Brush found at %f.\n", tr.endpos[2]);

	/* OK, now we have the floor height value in tr.endpos[2].
	 * Divide by QUANT and round down.
	 */
	return floor((tr.endpos[2] + DIST_EPSILON) / QUANT);
}


/**
 * @brief Performs traces to find the approximate floor of a passage.
 * @param[in] start The starting coordinate to search for a floor from.
 * @param[in] end The starting coordinate to search for a floor from.
 * @param[in] actor_size The actor's size.
 * @param[in] starting_height The starting height for this downward trace.
 * @param[in] starting_floor Starting coordinate's floor.
 * @param[in] ending_floor Ending coordinate's floor.
 * @return The absolute height of the found floor in QUANT units.
 */
static int RT_FindOpeningFloor(vec3_t start, vec3_t end, const int actor_size, const int starting_height, const int starting_floor, const int ending_floor){
	/*
	 * Look for additional space below init_bottom, down to lowest_bottom.
	 */
	int midfloor;
	int highest_floor;

	/* Find the height at the midpoint. */
	midfloor = RT_FindOpeningFloorFrac(start, end, actor_size, 0.5, starting_height);
	if (debugTrace)
		Com_Printf("midfloor:%i.\n", midfloor);

	/* Now make find the highest floor. */
	highest_floor = max(starting_floor, max(ending_floor, midfloor));

	/* If this is diagonal, trace the 1/4 and 3/4 points as well. */
	if (start[0] != end[0] && start[1] != end[1]) {
		/* 1/4 point */
		midfloor = RT_FindOpeningFloorFrac(start, end, actor_size, 0.25, starting_height);
		if (debugTrace)
			Com_Printf("1/4floor:%i.\n", midfloor);
		highest_floor = max(highest_floor, midfloor);

		/* 3/4 point */
		midfloor = RT_FindOpeningFloorFrac(start, end, actor_size, 0.75, starting_height);
		if (debugTrace)
			Com_Printf("3/4floor:%i.\n", midfloor);
		highest_floor = max(highest_floor, midfloor);
	}

	return highest_floor;
}


/**
 * @brief Performs traces to find the approximate ceiling of a passage.
 * @param[in] start The starting coordinate to search for a ceiling from.
 * @param[in] end The starting coordinate to search for a ceiling from.
 * @param[in] actor_size The actor's size.
 * @param[in] starting_height The starting height for this upward trace.
 * @param[in] starting_ceil Starting coordinate's ceiling.
 * @param[in] ending_ceil Ending coordinate's ceiling.
 * @return The absolute height of the found ceiling in QUANT units.
 */
static int RT_FindOpeningCeiling(vec3_t start, vec3_t end, const int actor_size, const int starting_height, const int starting_ceil, const int ending_ceil){
	int midceil;
	int lowest_ceil;

	/* Find the height at the midpoint. */
	midceil = RT_FindOpeningCeilingFrac(start, end, actor_size, 0.5, starting_height);
	if (debugTrace)
		Com_Printf("midceil:%i.\n", midceil);

	/* Now make midfloor the lowest ceiling. */
	lowest_ceil = min(starting_ceil, min(ending_ceil, midceil));

	/* If this is diagonal, trace the 1/4 and 3/4 points as well. */
	if (start[0] != end[0] && start[1] != end[1]) {
		/* 1/4 point */
		midceil = RT_FindOpeningCeilingFrac(start, end, actor_size, 0.25, starting_height);
		if (debugTrace)
			Com_Printf("1/4ceil:%i.\n", midceil);
		lowest_ceil = min(lowest_ceil, midceil);

		/* 3/4 point */
		midceil = RT_FindOpeningCeilingFrac(start, end, actor_size, 0.75, starting_height);
		if (debugTrace)
			Com_Printf("3/4ceil:%i.\n", midceil);
		lowest_ceil = min(lowest_ceil, midceil);
	}

	return lowest_ceil;
}


/**
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] map The map's routing data
 * @param[in] actor_size The actor's size
 * @param[in] x Starting x coordinate
 * @param[in] y Starting y coordinate
 * @param[in] z Starting z coordinate
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] bottom Actual height of the starting floor.
 * @param[in] top Actual height of the starting ceiling.
 * @param[out] lo_val Actual height of the bottom of the found passage.
 * @param[out] hi_val Actual height of the top of the found passage.
 * @return The new z value of the actor after traveling in this direction from the starting location.
 */
static int RT_FindOpening(routing_t * map, const int actor_size, const int  x, const int y, const int z, const int ax, const int ay, const int bottom, const int top, int *lo_val, int *hi_val) {
	vec3_t start, end;
	pos3_t pos;
	trace_t tr;
	int lo, hi, temp_z;

	const int endfloor = RT_FLOOR(map, actor_size, ax, ay, z) + z * CELL_HEIGHT;

	if (debugTrace)
		Com_Printf("ef:%i t:%i b:%i\n", endfloor, top, bottom);

	if (bottom == -1) {
		if (debugTrace)
			Com_Printf("Bailing- no floor in current cell.\n");
		*lo_val = *hi_val = 0;
		return -1;
	}

	/* Initialize the starting vector */
	VectorSet(pos, x, y, z);
	SizedPosToVec(pos, actor_size, start);

	/* Initialize the ending vector */
	VectorSet(pos, ax, ay, z);
	SizedPosToVec(pos, actor_size, end);

	/* Initialize the z component of both vetors */
	start[2] = end[2] = 0;


	/* First calculate the "guaranteed" opening, if any. If the opening from
	 * the floor to the ceiling is not too tall, there must be a section that
	 * will always be vacant if there is a usable passage of any size and at
	 * any height.
	 */
	if (top - bottom < PATHFINDING_MIN_OPENING * 2) {
		lo = top - PATHFINDING_MIN_OPENING;
		hi = bottom + PATHFINDING_MIN_OPENING;
		if (debugTrace)
			Com_Printf("Tracing closed space from %i to %i.\n", bottom, top);
		tr = RT_ObstructedTrace(start, end, actor_size, hi, lo);
		if (tr.fraction >= 1.0) {
			lo = RT_FindOpeningFloor(start, end, actor_size, lo, bottom, endfloor);
			hi = RT_FindOpeningCeiling(start, end, actor_size, hi, top, top);
			if (hi - lo >= PATHFINDING_MIN_OPENING) {
				if (lo == -1) {
					if (debugTrace)
						Com_Printf("Bailing- no floor in destination cell.\n");
					*lo_val = *hi_val = 0;
					return -1;
				}
				/* This opening works, use it! */
				*lo_val = lo;
				*hi_val = hi;
				/* Find the floor for the highest adjacent cell in this passage. */
				temp_z = min(floor((hi - 1) / CELL_HEIGHT), PATHFINDING_HEIGHT - 1);
				temp_z = RT_FLOOR(map, actor_size, ax, ay, temp_z) + temp_z * CELL_HEIGHT;
				/* Here's the code that makes an actor fall and not step down: */
				/** @note Don't do this here!  Let Grid_MoveMark make this decision. */
				/*
				if (bottom - temp_z > PATHFINDING_MAX_STEPUP){
					if (debugTrace)
						Com_Printf("Not stepping down so we fall bottom:%i temp_z:%i.\n", bottom, temp_z);
					return z;
				}
				*/
				return floor(temp_z / CELL_HEIGHT);
			}
		}
	} else {
		/* There is no "guaranteed" opening, brute force search. */
		lo = bottom;
		while (lo <= top - PATHFINDING_MIN_OPENING) {
			int old_lo = lo;
			/* Check for a 1 QUANT opening. */
			if (debugTrace)
				Com_Printf("Tracing open space from %i.\n", lo);
			tr = RT_ObstructedTrace(start, end, actor_size, lo + 1 , lo);
			if (tr.fraction < 1.0) {
				/* Credit to Duke: We skip the minimum opening, as if there is a
				 * viable opening, even one slice above, that opening would be open. */
				lo += PATHFINDING_MIN_OPENING;
			} else {
				/* Check for the full opening. Check the high first so we don't
				 * recheck the slice we just found. */
				hi = RT_FindOpeningCeiling(start, end, actor_size, lo + 1, top, top);
				lo = RT_FindOpeningFloor(start, end, actor_size, lo, bottom, endfloor);
				if (debugTrace)
					Com_Printf("Found ceiling at %i.\n", hi);
				if (hi - lo >= PATHFINDING_MIN_OPENING) {
					if (lo == -1) {
						if (debugTrace)
							Com_Printf("Bailing- no floor in destination cell.\n");
						*lo_val = *hi_val = 0;
						return -1;
					}
					/* This opening works, use it! */
					*lo_val = lo;
					*hi_val = hi;
					/* Find the floor for the highest adjacent cell in this passage. */
					temp_z = min(floor((hi - 1) / CELL_HEIGHT), PATHFINDING_HEIGHT - 1);
					temp_z = RT_FLOOR(map, actor_size, ax, ay, temp_z) + temp_z * CELL_HEIGHT;
					/* Here's the code that makes an actor fall vs step down: */
					/** @note Don't do this here!  Let Grid_MoveMark make this decision. */
					/*
					if (bottom - temp_z > PATHFINDING_MAX_STEPUP){
						if (debugTrace)
							Com_Printf("Not stepping down so we fall bottom:%i temp_z:%i.\n", bottom, temp_z);
						return z;
					}
					*/
					return floor(temp_z / CELL_HEIGHT);
				} else {
				/* Credit to Duke: We skip the minimum opening, as if there is a
				 * viable opening, even one slice above, that opening would be open. */
				lo = max(hi, old_lo) + PATHFINDING_MIN_OPENING;
				if (debugTrace)
					Com_Printf("Opening not large enough; setting lo to %i.\n", lo);
				}
			}
		}
	}

	*lo_val = *hi_val = top;
	return -1;
}


/**
 * @brief Performs small traces to find places when an actor can step up.
 * @param[in] map The map's routing data
 * @param[in] actor_size The actor's size
 * @param[in] x Starting x coordinate
 * @param[in] y Starting y coordinate
 * @param[in] z Starting z coordinate
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] az Ending z coordinate
 * @param[out] bottom Actual height of the bottom of the found passage.
 * @param[out] top Actual height of the top of the found passage.
 * @param[out] stepup Required stepup to travel in this direction.
 * @return The change in floor height in QUANT units because of the additional trace.
*/
static int RT_MicroTrace(routing_t * map, const int actor_size, const int x, const int y, const int z, const int ax, const int ay, const int az, const int bottom, const int top, int *stepup) {
	/* OK, now we have a viable shot across.  Run microstep tests now. */
	/* Now calculate the stepup at the floor using microsteps. */
	signed char bases[UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE + 1];
	float sx, sy, ex, ey;
	/* Shortcut the value of UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE. */
	const int steps = UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE;
	trace_t tr;
	vec3_t bmin, bmax;
	int i, current_h, highest_h, highest_i = 0, skipped, new_bottom;
	vec3_t start, end;
	pos3_t pos;

	/* First prepare the two known end values. */
	bases[0] = max(0, RT_FLOOR(map, actor_size, x, y, z)) + z * CELL_HEIGHT;
	bases[steps] = max(0, RT_FLOOR(map, actor_size, ax, ay, az)) + az * CELL_HEIGHT;

	/* Initialize the starting vector */
	VectorSet(pos, x, y, z);
	SizedPosToVec(pos, actor_size, start);

	/* Initialize the ending vector */
	VectorSet(pos, ax, ay, az);
	SizedPosToVec(pos, actor_size, end);

	/* Now prep the z values for start and end. */
	start[2] = min (top * QUANT - 1, (max(z, az) + 1) * UNIT_HEIGHT); /**< Just below the passage ceiling */
	end[2] = -QUANT;

	/* Memorize the start and end x,y points */
	sx = start[0];
	sy = start[1];
	ex = end[0];
	ey = end[1];

	/* Configure the box trace extents. */
	VectorSet(bmax, PATHFINDING_MICROSTEP_SIZE / 2 - DIST_EPSILON, PATHFINDING_MICROSTEP_SIZE / 2 - DIST_EPSILON, 0);
	VectorSet(bmin, -PATHFINDING_MICROSTEP_SIZE / 2 + DIST_EPSILON, -PATHFINDING_MICROSTEP_SIZE / 2 + DIST_EPSILON, 0);

	new_bottom = max(bases[0], bases[steps]);
	/* Now calculate the rest of the microheights. */
	for (i = 1; i <= steps; i++) {
		start[0] = end[0] = sx + (ex - sx) * (i / (float)steps);
		start[1] = end[1] = sy + (ey - sy) * (i / (float)steps);

		/* perform the trace, then return true if the trace was obstructed. */
		tr = RT_COMPLETEBOXTRACE(start, end, bmin, bmax, 0x1FF, MASK_IMPASSABLE, MASK_PASSABLE);
		if (tr.fraction >= 1.0) {
			bases[i] = -1;
		} else {
			bases[i] = ceil((tr.endpos[2] - DIST_EPSILON) / QUANT);
		}

		if (debugTrace)
			Com_Printf("Microstep %i from (%f, %f, %f) to (%f, %f, %f) = %i [%f]\n",
				i, start[0], start[1], start[2], end[0], end[1], end[2], bases[i], tr.endpos[2]);\

		new_bottom = max(new_bottom, bases[i]);
	}

	if (debugTrace)
		Com_Printf("z:%i az:%i bottom:%i new_bottom:%i top:%i bases[0]:%i bases[%i]:%i\n", z, az, bottom, new_bottom, top, bases[0], steps, bases[steps]);

	/** @note This for loop is bi-directional: i may be decremented to retrace prior steps. */
	/* Now find the maximum stepup moving from (x, y) to (ax, ay). */
	/* Initialize stepup. */
	current_h = bases[0];
	highest_h = -1;
	highest_i = 1;
	*stepup = 0; /**<  Was originally -CELL_HEIGHT, but stepup is needed to go UP, not down. */
	skipped = 0;
	for (i = 1; i <= steps; i++) {
		if (debugTrace)
			Com_Printf("Tracing forward i:%i h:%i\n", i, current_h);
		/* If there is a rise, use it. */
		if (bases[i] >= current_h || ++skipped > PATHFINDING_MICROSTEP_SKIP) {
			if (skipped == PATHFINDING_MICROSTEP_SKIP) {
				i = highest_i;
				if (debugTrace)
					Com_Printf(" Skipped too many steps, reverting to i:%i\n", i);
			}
			*stepup = max(*stepup, bases[i] - current_h);
			current_h = bases[i];
			highest_h = -2;
			highest_i = i + 1;
			skipped = 0;
			if (debugTrace)
				Com_Printf(" Advancing b:%i stepup:%i\n", bases[i], *stepup);
		} else {
			/* We are skipping this step in case the actor can step over this lower step. */
			/* Record the step in case it is the highest of the low steps. */
			if (bases[i] > highest_h) {
				highest_h = bases[i];
				highest_i = i;
			}
			if (debugTrace)
				Com_Printf(" Skipped because we are falling, skip:%i.\n", skipped);
			/* If this is the last iteration, make sure we go back and get our last stepup tests. */
			if (i == steps) {
				skipped = PATHFINDING_MICROSTEP_SKIP;
				i = highest_i - 1;
				if (debugTrace)
					Com_Printf(" Tripping skip counter to perform last tests.\n");
			}
		}
	}

	/* Now place an upper bound on stepup */
	if (*stepup > PATHFINDING_NO_STEPUP) {
		*stepup = PATHFINDING_NO_STEPUP;
	} else {
		/* Add rise/fall bit as needed. */
		if (az < z)
			*stepup |= PATHFINDING_BIG_STEPDOWN;
		else if (az > z)
			*stepup |= PATHFINDING_BIG_STEPUP;
	}

	/* Return the confirmed passage opening. */
	return bottom - new_bottom;
}


/**
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] map The map's routing data
 * @param[in] actor_size The actor's size
 * @param[in] x Starting x coordinate
 * @param[in] y Starting y coordinate
 * @param[in] z Starting z coordinate
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] az Ending z coordinate
 * @param[out] opening_base Actual height in QUANT units of the detected opening.
 * @param[out] stepup Required stepup to travel in this direction.
 * @return The size in QUANT units of the detected opening.
 */
static int RT_TracePassage(routing_t * map, const int actor_size, const int  x, const int y, const int z, const int ax, const int ay, int *opening_base, int *stepup)
{
	/* See if there is a passage TO the adjacent cell. */
	const int bottom = max(0, RT_FLOOR(map, actor_size, x, y, z)) + z * CELL_HEIGHT;
	const int top = RT_CEILING(map, actor_size, x, y, z) + z * CELL_HEIGHT;
	const int belowceil = z > 0 ? RT_CEILING(map, actor_size, ax, ay, z) + z * CELL_HEIGHT: 0;
	int hi; /**< absolute ceiling of the passage found. */
	int az; /**< z height of the actor after moving in this direction. */
	int opening_size, bonus_size;

	/*
	 * First check the ceiling fo the cell beneath the adjacent floor to see
	 * if there is a potential opening.  The the difference between the
	 * ceiling and the floor is at least PATHFINDING_MIN_OPENING tall, then
	 * scan it to see if we can use it.  If we can, then one of two things
	 * will happen:
	 *  - The actual adjacent call has no floor of its own, and we will walk
	 *      or fall into the cell below the adjacent cell anyway.
	 *  - There is a floor in the adjacent cell, but we will not be able to
	 *      walk into it anyway because there cannot be any steps if there is
	 *      a passage.  An actor can walk down into the cell ONLY IF it's
	 *      negative stepup meets or exceeds the change in floor height.
	 *      No actors will be allowed to fall because they cannot temporarily
	 *      occupy the space beneath the floor in the adjacent cell to fall
	 *      (all actors in the cell must be ON TOP of the floor in the cell).
	 * If there is no passage, then the obstruction may be used as steps to
	 * climb up to the adjacent floor.
	 */
	if (belowceil - bottom >= PATHFINDING_MIN_OPENING) {
		if (debugTrace)
			Com_Printf(" Testing down.\n");
		az = RT_FindOpening(map, actor_size, x, y, z, ax, ay, bottom, belowceil, opening_base, &hi);
		/* We subtract MIN_STEPUP because that is foot space-
		 * the opening there only needs to be the microtrace
		 * wide and not the usual dimensions.
		 */
		if (hi - *opening_base >= PATHFINDING_MIN_OPENING - PATHFINDING_MIN_STEPUP) {
			/* This returns the total opening height, as the
			 * microtrace may reveal more passage height from the foot space. */
			bonus_size = RT_MicroTrace(map, actor_size, x, y, z, ax, ay, az, *opening_base, hi, stepup);
			*opening_base += bonus_size;
			opening_size = hi - *opening_base;
			if (opening_size >= PATHFINDING_MIN_OPENING) {
				return opening_size;
			}
		}
	}

	/*
	 * Now that we got here, we know that either the opening between the
	 * ceiling below the adjacent cell and the current floor is too small or
	 * obstructed.  Try to move onto the adjacent floor.
	 */

	if (debugTrace)
		Com_Printf(" Testing up.\n");

	az = RT_FindOpening(map, actor_size, x, y, z, ax, ay, bottom, top, opening_base, &hi);
	/* We subtract MIN_STEPUP because that is foot space-
	 * the opening there only needs to be the microtrace
	 * wide and not the usual dimensions.
	 */
	if (hi - *opening_base >= PATHFINDING_MIN_OPENING - PATHFINDING_MIN_STEPUP) {
		/* This returns the total opening height, as the
		 * microtrace may reveal more passage height from the foot space. */
		bonus_size = RT_MicroTrace(map, actor_size, x, y, z, ax, ay, az, *opening_base, hi, stepup);
		*opening_base += bonus_size;
		opening_size = hi - *opening_base;
		if (opening_size >= PATHFINDING_MIN_OPENING) {
			return opening_size;
		}
	}

	if (debugTrace)
		Com_Printf(" No opening found.\n");
	/* If we got here, then there is no opening from floor to ceiling. */
	*stepup = PATHFINDING_NO_STEPUP;
	return 0;
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

	int opening_size; /**< The opening size (max actor hight) that can travel this passage. */
	int opening_base; /**< The base height of the opening. */
	int stepup; /**< The stepup needed to travel through this passage in this direction. */

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

	/* if our destination cell is out of bounds, bail. */
	if (ax < 0 || ax > PATHFINDING_WIDTH - actor_size || ay < 0 || y > PATHFINDING_WIDTH - actor_size) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		RT_STEPUP(map, actor_size, x, y, z, dir) = PATHFINDING_NO_STEPUP;
		/* There is only one entry here: There is no inverse cell to store data for. */
		if (debugTrace)
			Com_Printf("Destination cell non-existant.\n");
		return z;
	}

	/* Ensure that the destination coordinates are valid. */
	RT_CONN_TEST(map, actor_size, ax, ay, z, dir);

	/* test if the unit is blocked by a loaded model */
	/** @note This test as is will cause back tracing issues, as the cell we are tracing to might
	 * be open and we are potentially not assigning opening information to it.  Figure out a
	 * solution to this without simply calling this function again with the destination coordinates. */
	if (RT_FLOOR(map, actor_size, x, y, z) >= CELL_HEIGHT || RT_CEILING(map, actor_size, x, y, z) == 0){
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		RT_STEPUP(map, actor_size, x, y, z, dir) = PATHFINDING_NO_STEPUP;
		if (debugTrace)
			Com_Printf("Current cell filled. f:%i c:%i\n", RT_FLOOR(map, actor_size, x, y, z), RT_CEILING(map, actor_size, x, y, z));
		return z;
	}

	/* test if the destination unit is blocked by a loaded model */
	/** @note We don't need this test.  Even though the cell immediately in front is filled, we
	 * may still be able to stepup into the next level.  Scan just in case. */
	/*
	if (RT_CEILING(map, actor_size, ax, ay, z) == 0){
		/ * We can't go this way. * /
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		RT_STEPUP(map, actor_size, x, y, z, dir) = PATHFINDING_NO_STEPUP;
		if (debugTrace)
			Com_Printf("Destination cell filled. f:%i c:%i\n", RT_FLOOR(map, actor_size, x, y, z), RT_CEILING(map, actor_size, x, y, z));
		return z;
	}
	*/

	/* Find an opening. */
	opening_size = RT_TracePassage(map, actor_size, x, y, z, ax, ay, &opening_base, &stepup);
	if (debugTrace) {
		Com_Printf("Final RT_STEPUP for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, z, actor_size, dir, stepup);
	}
	/* We always call the fill function.  If the passage cannot be traveled, the
	 * function fills it in as unpassable. */
	return RT_FillPassageData(map, actor_size, dir, x, y, z, opening_size, opening_base, stepup);
}


void RT_WriteCSVFiles (struct routing_s *map, const char* baseFilename, const ipos3_t mins, const ipos3_t maxs)
{
	char filename[MAX_OSPATH], ext[MAX_OSPATH];
	qFILE f;
	int i, x, y, z;

	/* An elevation files- dumps the floor and ceiling levels relative to each cell. */
	for (i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.elevation.csv", i);
		COM_DefaultExtension(filename, sizeof(filename), ext);
		FS_OpenFile(filename, &f, FILE_WRITE);
		if (!f.f)
			Sys_Error("Could not open file %s.", filename);
		FS_Printf(&f, ",");
		for (x = mins[0]; x <= maxs[0] - i + 1; x++)
			FS_Printf(&f, "x:%i,", x);
		FS_Printf(&f, "\n");
		for (z = maxs[2]; z >= mins[2]; z--) {
			for (y = maxs[1]; y >= mins[1] - i + 1; y--) {
				FS_Printf(&f, "z:%i  y:%i,", z ,y);
				for (x = mins[0]; x <= maxs[0] - i + 1; x++) {
					/* compare results */
					FS_Printf(&f, "h:%i c:%i,", RT_FLOOR(map, i, x, y, z), RT_CEILING(map, i, x, y, z));
				}
				FS_Printf(&f, "\n");
			}
			FS_Printf(&f, "\n");
		}
		FS_CloseFile(&f);
	}

	/* Output the walls/passage files. */
	for (i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.walls.csv", i);
		COM_DefaultExtension(filename, sizeof(filename), ext);
		FS_OpenFile(filename, &f, FILE_WRITE);
		if (!f.f)
			Sys_Error("Could not open file %s.", filename);
		FS_Printf(&f, ",");
		for (x = mins[0]; x <= maxs[0] - i + 1; x++)
			FS_Printf(&f, "x:%i,", x);
		FS_Printf(&f, "\n");
		for (z = maxs[2]; z >= mins[2]; z--) {
			for (y = maxs[1]; y >= mins[1] - i + 1; y--) {
				FS_Printf(&f, "z:%i  y:%i,", z ,y);
				for (x = mins[0]; x <= maxs[0] - i + 1; x++) {
					/* compare results */
					FS_Printf(&f, "\"");

					/* NW corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NX_PY(map, i, x, y, z), RT_STEPUP_NX_PY(map, i, x, y, z));

					/* N side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PY(map, i, x, y, z), RT_STEPUP_PY(map, i, x, y, z));

					/* NE corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PX_PY(map, i, x, y, z), RT_STEPUP_PX_PY(map, i, x, y, z));

					FS_Printf(&f, "\n");

					/* W side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NX(map, i, x, y, z), RT_STEPUP_NX(map, i, x, y, z));

					/* Center - display floor height */
					FS_Printf(&f, "_%+2i_ ", RT_FLOOR(map, i, x, y, z));

					/* E side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PX(map, i, x, y, z), RT_STEPUP_PX(map, i, x, y, z));

					FS_Printf(&f, "\n");

					/* SW corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NX_NY(map, i, x, y, z), RT_STEPUP_NX_NY(map, i, x, y, z));

					/* S side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NY(map, i, x, y, z), RT_STEPUP_NY(map, i, x, y, z));

					/* SE corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PX_NY(map, i, x, y, z), RT_STEPUP_PX_NY(map, i, x, y, z));

					FS_Printf(&f, "\",");
				}
				FS_Printf(&f, "\n");
			}
			FS_Printf(&f, "\n");
		}
		FS_CloseFile(&f);
	}
}
