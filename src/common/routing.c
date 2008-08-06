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

/*
===============================================================================
GAME RELATED TRACING
===============================================================================
*/


/**
 * @param[in] x The x position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] y The y position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] z The z position in the routing arrays (0 - PATHFINDING_HEIGHT-1)
 * @sa Grid_RecalcRouting
 */
void RT_CheckUnit (old_routing_t * map, int x, int y, int z)
{
	vec3_t start, end;
	vec3_t tend, tvs, tve;
	pos3_t pos;
	float height;
	int i;

	assert(map);
	assert((x >= 0) && (x < PATHFINDING_WIDTH));
	assert((y >= 0) && (y < PATHFINDING_WIDTH));
	assert(z < PATHFINDING_HEIGHT);

#ifdef COMPILE_UFO
	/* reset flags */
	map->filled[y][x] &= ~(1 << z);
	map->fall[y][x] &= ~(1 << z);
/* #else these are already zeroed. */
#endif

	/* calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	PosToVec(pos, end);

	/* prepare fall down check */
	VectorCopy(end, start);
	/*
	 * These points are centered at the base of the cell, + or - one step unit (4 model units)
	 * Start is on top.
	*/
	start[2] -= UNIT_HEIGHT / 2 - QUANT;
	end[2]   -= UNIT_HEIGHT / 2 + QUANT;

	/* test for fall down */
	if (RT_TESTLINE(start, end, TL_FLAG_ACTORCLIP)) {
		PosToVec(pos, end);
		/* This puts start at PLAYER_HEIGHT above the base of the cell */
		VectorAdd(end, dup_vec, start);
		/* This puts end at the base of the cell. */
		VectorAdd(end, dwn_vec, end);
		height = 0;

		/* test for ground with a "middled" height */
		for (i = 0; i < 5; i++) {
			VectorAdd(start, testvec[i], tvs);
			VectorAdd(end, testvec[i], tve);
			TR_TestLineDM(tvs, tve, tend, TL_FLAG_ACTORCLIP);
			height += tend[2];

			/* stop if it's totally blocked somewhere */
			/* and try a higher starting point */
			if (VectorCompareEps(tvs, tend, EQUAL_EPSILON))
				break;
		}

		/* tend[0] & [1] are correct (testvec[4]) */
		height += tend[2];
        /* Divide by 6 to get the average height */
		tend[2] = height / 6.0;

		if (i == 5 && !VectorCompareEps(start, tend, EQUAL_EPSILON)) {
			/* found a possibly valid ground */
			height = PLAYER_HEIGHT - (start[2] - tend[2]);
			end[2] = start[2] + height;

			if (!RT_TESTLINEDM(start, end, tend, TL_FLAG_ACTORCLIP))
				map->route[z][y][x] = ((height + QUANT / 2) / QUANT < 0) ? 0 : (height + QUANT / 2) / QUANT;
			else
				map->filled[y][x] |= 1 << z;	/* don't enter */
		} else {
			/* elevated a lot */
			end[2] = start[2];
			/* Start is now where head would be */
			start[2] += UNIT_HEIGHT - PLAYER_HEIGHT;
			height = 0;

			/* test for ground with a "middled" height */
			for (i = 0; i < 5; i++) {
				VectorAdd(start, testvec[i], tvs);
				VectorAdd(end, testvec[i], tve);
				RT_TESTLINEDM(tvs, tve, tend, TL_FLAG_ACTORCLIP);
				height += tend[2];
			}
			/* tend[0] & [1] are correct (testvec[4]) */
			height += tend[2];
			/* Divide by 6 to get the average height */
			tend[2] = height / 6.0;

			if (VectorCompareEps(start, tend, EQUAL_EPSILON)) {
				map->filled[y][x] |= 1 << z;	/* don't enter */
			} else {
				/* found a possibly valid elevated ground */
				end[2] = start[2] + PLAYER_HEIGHT - (start[2] - tend[2]);
				height = UNIT_HEIGHT - (start[2] - tend[2]);

/*				Com_DPrintf(DEBUG_ENGINE, "%i %i\n", (int)height, (int)(start[2] - tend[2])); */

				if (!RT_TESTLINEDM(start, end, tend, TL_FLAG_ACTORCLIP))
					map->route[z][y][x] = ((height + QUANT / 2) / QUANT < 0) ? 0 : (height + QUANT / 2) / QUANT;
				else
					map->filled[y][x] |= 1 << z;	/* don't enter */
			}
		}
	} else {
		/* fall down */
		map->route[z][y][x] = 0;
		map->fall[y][x] |= 1 << z;
	}
}


/**
 * @brief Routing Function to update the connection between two fields
 * @param[in] map Routing field of the current loaded map
 * @param[in] x The x position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] y The y position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] z The z position in the routing arrays (0 - PATHFINDING_HEIGHT-1)
 * @param[in] fill
 */
void RT_UpdateConnection (old_routing_t * map, int x, int y, int z, qboolean fill)
{
	vec3_t center, start, end;
	pos3_t pos;
	int h, sh, ax, ay, dir;
	byte az;
	/* Falling deeper than one level or falling through the map is forbidden. This array */
	/* includes the critical bit mask for every level: the bit for the current level and */
	/* the level below, for the lowest level falling is completely forbidden */
	const byte deep_fall[] = {0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0};

	assert(map);
	assert((x >= 0) && (x < PATHFINDING_WIDTH));
	assert((y >= 0) && (y < PATHFINDING_WIDTH));
	assert(z < PATHFINDING_HEIGHT);

	/* assume no connection */
	map->route[z][y][x] &= ~((0xF0) & UCHAR_MAX);

	/* test if the unit is blocked by a loaded model */
	if (fill && (map->filled[y][x] & (1 << z)))
		return;

	/* no ground under the position */
	if ((map->fall[y][x] & deep_fall[z]) == deep_fall[z])
		return;

	/* get step height and trace vectors */
#ifdef COMPILE_MAP
	sh = R_STEP(map, x, y, z) ? sh_big : sh_low;
	h = R_HEIGHT(map, x, y, z);
#else
    sh = h = 0;
#endif

	/* Setup the points used to check for walls and such */
	VectorSet(pos, x, y, z);
	PosToVec(pos, center);
	/* Adjust height to the actual floor height. */
	center[2] += h * QUANT;

	az = z + (h + sh) / 0x10;
	h = (h + sh) % 0x10;

	/* range check */
	if (az >= PATHFINDING_HEIGHT) {
		az = PATHFINDING_HEIGHT - 1;
		h = 0x0F;
	}

	/* test connections in all 4 directions */
	for (dir = 0; dir < 4; dir++) {
		/* target coordinates */
		ax = x + dvecs[dir][0];
		ay = y + dvecs[dir][1];

		/* range check- ensure that ax and ay are on the map*/
		if (ax < 0 || ax >= PATHFINDING_WIDTH || ay < 0 || ay >= PATHFINDING_WIDTH)
			continue;

		/* test if the neighbor field is to high to step on */
#ifdef COMPILE_MAP
		if (R_HEIGHT(map, ax, ay, az) > h)
			continue;
#endif

		/* test filled */
		if (fill && (map->filled[ay][ax] & (1 << az)))
			continue;

		/* test ground under the neighbor position */
		if (((map->fall[ay][ax] & deep_fall[az]) == deep_fall[az]))
			continue;

		/* Reset the center */
		VectorCopy(center, start);
		/* Set the end point to the edge of the field unit in the direction traveling */
		/** @todo should this check into the center of the adjacent field unit insead of just to the edge? */
		VectorSet(end, start[0] + (UNIT_HEIGHT / 2) * dvecs[dir][0], start[1] + (UNIT_HEIGHT / 2) * dvecs[dir][1], start[2]);

		/** @todo Do we need to use a bounding box to check for things in the way of this move? */
		/* center check */
		if (RT_TESTLINE(start, end, TL_FLAG_ACTORCLIP))
			continue;

		/* lower check */
		start[2] = end[2] -= UNIT_HEIGHT / 2 - sh * QUANT - 2;
		if (RT_TESTLINE(start, end, TL_FLAG_ACTORCLIP))
			continue;

		/* allow connection to neighbor field */
		map->route[z][y][x] |= (0x10 << dir) & UCHAR_MAX;
	}
}


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
			trace = TR_CompleteBoxTrace(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
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
			trace = TR_CompleteBoxTrace(origin, origin, mins, maxs, 0x1FF, MASK_ALL, 0);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* Dump the client map */
	Grid_DumpMap(map, 0, start[0], start[1], start[2], end[0], end[1], end[2]);
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
int RT_NewCheckCell (routing_t * map, const int actor_size, const int x, const int y, const int z)
{
	vec3_t start, end; /* Start and end of the traces */
	vec3_t bmin, bmax; /* Extents of the box being traced */
	vec3_t tstart, tend, temp;
	pos3_t pos;
	float bottom, top; /* Floor and ceiling model distances from the cell base. */
	float base, initial; /* Cell floor and ceiling z coordinate. */
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
	start[2] += UNIT_HEIGHT / 2 - QUANT;
	base = end[2] - UNIT_HEIGHT / 2; /* This is the point where not finding a valid floor means failure. */
	initial = end[2] + UNIT_HEIGHT / 2; /* This is the point where not finding a valid floor means failure. */
	end[2] = 0; /* To the bottom of the model! */


	/* Configure bmin and bmax for the main ceiling scan */
	VectorSet(bmin, UNIT_SIZE * actor_size / -2 + WALL_SIZE, UNIT_SIZE * actor_size / -2 + WALL_SIZE, 0);
	VectorSet(bmax, UNIT_SIZE * actor_size /  2 - WALL_SIZE, UNIT_SIZE * actor_size /  2 - WALL_SIZE, 0);
	/* VectorSubtract(vec3_origin, bmin, bmax);*/ /* The box is symmetrical about origin and parallel to z axis */

	/*
	 * Trace for a floor.  There are a few outcomes:
	 * 1. Both points are in the same brush.  floor = ceiling = 0.
	 * 2. The starting point is not in a brush and a line intersects a surface.  This is a ceiling
	 *       FOR THE CELLS BELOW IT. Retrace just inside the brush to look for a floor.
	 * 3. The line crosses no surfaces.  Retrace to the bottom of the model to find the floor.
	 * 4. The starting point is in a brush and the line crosses a surface.  Possible floor.
	 * 5. When case 3 traces down and finds no brush thru the bottom of the model, it is bottomless.
	 */
	/* Com_Printf("[(%i, %i, %i)]Casting (%f, %f, %f) to (%f, %f, %f)- i:%f\n", x, y, z, start[0], start[1], start[2], end[0], end[1], end[2], base); */
	while (base < start [2]) { /* Loop until we start a trace under the bottom of the cell or find something. */
		/*Com_Printf("[(%i, %i, %i)]Casting (%f, %f, %f) to (%f, %f, %f)- i:%f\n", x, y, z, start[0], start[1], start[2], end[0], end[1], end[2], base); */
		RT_TESTLINEDM(start, end, temp, TL_FLAG_ACTORCLIP);
		/*if (!RT_TESTLINEDM(start, end, temp, TL_FLAG_ACTORCLIP)) { */
			/* Com_Printf("No upward facing surface encountered (Freefall!!!).\n"); */
			/*
			RT_FLOOR(map, actor_size, x, y, z) = 1;
			RT_CEILING(map, actor_size, x, y, z) = 0;
			*/
			/* return the next z coordinate to scan. */
			/*
			return z + 1;
		}
		*/
		/* We have hit a brush that faces up and can be stood on. Look for a ceiling. */
		bottom = temp[2];  /* record the floor position. */

		/* Record the debugging data in case it is used. */
		VectorCopy(temp, brushesHit.floor);

		assert(initial>bottom);
		/*
		if (bottom >= initial) {
			Com_Printf("Casting (%f, %f, %f) to (%f, %f, %f) hit [(%f, %f, %f)], which is above the cell top at %f\n", start[0], start[1], start[2], end[0], end[1], end[2], temp[0], temp[1], temp[2], initial);
		} */
		/* Com_Printf("Potential floor found at %f.\n", bottom); */

		VectorCopy(temp, tstart);
		VectorCopy(tstart, tend);
		tend[2] += PATHFINDING_STEPUP * QUANT; /* tend is now STEPUP above tstart */
		/* om_Printf("    Casting (%f, %f, %f) to (%f, %f, %f)\n", tstart[0], tstart[1], tstart[2], tend[0], tend[1], tend[2]); */
		if (RT_TESTLINEDM(tstart, tend, temp, TL_FLAG_ACTORCLIP)) {
			/* Com_Printf("Cannot use found surface- stepup obstruction found at %f.\n", temp[2]); */
			/*
			 * There is a premature obstruction.  We can't use this as a floor.
			 * Check under start.  We need to have at least the minimum amount of clearance from our ceiling,
			 * So shart at that point.
			 */
			start[2] = temp[2] - PATHFINDING_MIN_OPENING * QUANT;
			continue;
		}

		/*
		 * If we are here, then the immediate floor is unobstructed STEPUP units high.
		 * Scan for the ceiling with a box as big as the actor.  If not brush is found, then the top of
		 * the model is used as the ceiling.  There are two possible outcomes:
		 * 1. The ceiling is less than PATHFINDING_MIN_OPENING above the floor, therefore
		 *      cannot be used as a standing surface.  Restart the floor scan below this.
		 * 2. The ceiling is at least PATHFINDING_MIN_OPENING above the floor.  This is OK.
		 */

		VectorCopy(tstart, temp); /* Record the old tstart, we may need it.*/
		tstart[2] = tend[2]; /* The box trace for height starts at stepup height. */
		tend[2] = PATHFINDING_HEIGHT * UNIT_HEIGHT; /* tend now reaches the model ceiling. */

		tr = TR_CompleteBoxTrace (tstart, tend, bmin, bmax, 0x1FF, MASK_VERY_SOLID, CONTENTS_PASSABLE);
		if (tr.endpos[2] - bottom < PATHFINDING_MIN_OPENING * QUANT) { /* Case 1 */
			/* Com_Printf("Ceiling not at least PATHING_MIN_OPENING; surface found at %f.\n", tr.endpos[2]); */
			start[2] = tr.endpos[2] - PATHFINDING_MIN_OPENING * QUANT; /* Move start the minimum opening distance below our ceiling. */
			/* Repeat the floor test */
			continue;
		}

		/* Case 2: tr.endpos[2] is the ceiling. */
		top = tr.endpos[2];

		/* Record the debugging data in case it is used. */
		VectorCopy(tr.endpos, brushesHit.ceiling);

		/* exit the infinite while loop */
		break;
	}

	/* if the start point is below or at the cell's base, fail. */
	if (start[2] <= base) {
		/* Com_Printf("Next start point below bottom of cell.  No floor in cell.\n"); */

		/* no floor in this cell, we are trying to start a new search for a floor above the top of the cell */
		RT_FLOOR(map, actor_size, x, y, z) = 16; /* The floor is at least the top of this cell. */
		RT_CEILING(map, actor_size, x, y, z) = 0; /* There is no ceiling, the true indicator of a filled cell. */

		/* Zero the debugging data */
		/* On second thought, dont- this will show the proper floor for any given cell.
		 * VectorSet(brushesHit[actor_size - 1].floor[z][y][x], 0, 0, 0);
		 * VectorSet(brushesHit[actor_size - 1].ceiling[z][y][x], 0, 0, 0); */

		/* return the next z coordinate to scan. */
		return z + 1;
		/* top = initial; */
	}

	/* if the ceiling is below the cell's base, fail.
	 * On second thought, mark it as Fall- if there's no floor, then there's no floor!
	 */
	if (top < base) {
		/* Com_Printf("Ceiling below bottom of cell.  No floor in cell.\n"); */
		/* no floor in this cell, we are trying to start a new search for a floor above the top of the cell */
		/*
		RT_FLOOR(map, actor_size, x, y, z) = 3;
		RT_CEILING(map, actor_size, x, y, z) = 0;
		*/
		/* return the next z coordinate to scan. */
		/* return z + 1; */
		top = initial; /* force top to the top of the cell. */
	}

	assert (bottom <= top);

	/* top and bottom are absolute model heights.  Find the actual cell z coordinates for these heights. */
	fz = (int)(floor((bottom + QUANT) / UNIT_HEIGHT));
	cz = (int)(ceil(top / UNIT_HEIGHT) -1);

	/* Com_Printf("Valid ceiling found, bottom=%f, top=%f, fz=%i, cz=%i.\n", bottom, top, fz, cz); */

	/* Last, update the floors and ceilings of cells from (x, y, fz) to (x, y, cz) */
	for (i = fz; i <= cz; i++) {
		/* Round up floor to keep feet out of model. */
		RT_FLOOR(map, actor_size, x, y, i) = (int)ceil((bottom - i * UNIT_HEIGHT) / QUANT);
		/* Com_Printf("floor(%i, %i, %i)=%i.\n", x, y, i, RT_FLOOR(map, size, x, y, i)); */
		/* Round down ceiling to heep head out of model.  Also offset by floor and max at 255. */
		RT_CEILING(map, actor_size, x, y, i) = (int)floor((top - i * UNIT_HEIGHT) / QUANT);
		/* Com_Printf("ceil(%i, %i, %i)=%i.\n", x, y, i, RT_CEILING(map, size, x, y, i)); */
	}

	/* Also, update the floors of any filled cells immediately beneath the cell with a floor. */
	for (i = fz - 1; i >= 0; i--) {
		if (map->ceil[i][y][x] != 0)
			break; /* Stop when we find an enterable cell. */
		/* Round up floor to keep feet out of model. */
		RT_FLOOR(map, actor_size, x, y, i) = (int)ceil((bottom - i * UNIT_HEIGHT) / QUANT);
	}

	/*
	 * Return the next z coordinate to scan- the cell above cz if fz==cz or z==cz,
	 * otherwise cz to check in case the ceiling of cells beneath is the floor of cell cz.
	 */
	if (cz == fz || cz == z)
		return cz + 1;
	return cz;
}


/**
 * @brief Helper function to trace for walls
 * @param[in] start The starting point of the trace, at the FLOOR'S CENTER.
 * @param[in] end The end point of the trace, centered x and y at the destination but at the same height as start.
 * @param[in] hi The upper height ABOVE THE FLOOR of the bounding box.
 * @param[in] lo The lower height ABOVE THE FLOOR of the bounding box.
 */
static qboolean RT_ObstructedTrace(const vec3_t start, const vec3_t end, int actor_size, int hi, int lo) {
	vec3_t bmin, bmax;

	/* Configure the box trace extents. The box is relative to the original floor. */
	VectorSet(bmax, UNIT_SIZE * actor_size / 2 - WALL_SIZE, UNIT_SIZE * actor_size / 2 - WALL_SIZE, hi * QUANT);
	VectorSet(bmin, -UNIT_SIZE * actor_size / 2 + WALL_SIZE, -UNIT_SIZE * actor_size / 2 + WALL_SIZE, lo * QUANT + DIST_EPSILON);

	/* perform the trace, then return true if the trace was obstructed. */
	tr_obstruction = TR_CompleteBoxTrace (start, end, bmin, bmax, 0x1FF, MASK_VERY_SOLID, CONTENTS_PASSABLE | CONTENTS_STEPON);
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
int RT_NewUpdateConnection (routing_t * map, const int actor_size, const int x, const int y, const int z, const int dir)
{
	vec3_t start, end;
	pos3_t pos;
	int i;
	int h, c; /**< height, ceiling */
	int ah, ac; /**< attributes of the destination cell */
	int fz, cz; /**< Floor and celing Z cell coordinates */
	int floor_p, ceil_p; /**< floor of the passage, ceiling of the passage. */
	int hi, lo, worst; /**< used to find floor and ceiling of passage */
	int maxh; /**< The higher value of h and ah */
	qboolean obstructed;
	/** 127 <= y <= 134, z=0, 129 <= x <= 135 */
	/* qboolean doit = 127 <= y && y <= 134 && z==0 && 129 <= x && x <= 135 && actor_size == 1 && dir < 4; */
	qboolean doit = qfalse; /**< If true causes debug output for wall checks */

	/* get the neighbor cell's coordinates */
	const int ax = x + dvecs[dir][0];
	const int ay = y + dvecs[dir][1];

	assert(map);
	assert((x >= 0) && (x <= PATHFINDING_WIDTH - actor_size));
	assert((y >= 0) && (y <= PATHFINDING_WIDTH - actor_size));
	assert(z < PATHFINDING_HEIGHT);

	/* Ensure that the current coordinates are valid. */
	RT_CONN_TEST(map, actor_size, x, y, z, dir);

	if (doit) Com_Printf("\n(%i, %i, %i) to (%i, %i, %i)\n", x, y, z, ax, ay, z);

	/* Com_Printf("At (%i, %i, %i) looking in direction %i with size %i\n", x, y, z, dir, actor_size); */

	/* test if the unit is blocked by a loaded model */
	if (RT_FLOOR(map, actor_size, x, y, z) >= UNIT_HEIGHT / QUANT || RT_CEILING(map, actor_size, x, y, z) - RT_FLOOR(map, actor_size, x, y, z) < PATHFINDING_MIN_OPENING){
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Zero the debugging data */
		VectorSet(brushesHit.floor, 0, 0, 0);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;
		if (doit)  Com_Printf("Current cell filled. f:%i c:%i\n", RT_FLOOR(map, actor_size, x, y, z), RT_CEILING(map, actor_size, x, y, z));
		return z + 1;
	}


	/* if our destination cell is out of bounds, bail. */
	if ((ax < 0) || (ax > PATHFINDING_WIDTH - actor_size) || (ay < 0) || (y > PATHFINDING_WIDTH - actor_size)) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Zero the debugging data */
		VectorSet(brushesHit.floor, 0, 0, 0);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;
		if (doit)  Com_Printf("Destination cell non-existant.\n");
		return z + 1;
	}

	/* Ensure that the destination coordinates are valid. */
	RT_CONN_TEST(map, actor_size, ax, ay, z, dir);

	/* get the originating floor's absolute height */
	h = RT_FLOOR(map, actor_size, x, y, z) + z * (UNIT_HEIGHT / QUANT);

	/* get the originating ceiling's absolute height */
	c = RT_CEILING(map, actor_size, x, y, z) + z * (UNIT_HEIGHT / QUANT);

	/* get the destination floor's absolute height */
	ah = RT_FLOOR(map, actor_size, ax, ay, z) + z * (UNIT_HEIGHT / QUANT);

	/* get the destination ceiling's absolute height */
	ac = RT_CEILING(map, actor_size, ax, ay, z) + z * (UNIT_HEIGHT / QUANT);

	/* Set maxh to the larger of h and ah. */
	maxh = max(h, ah);

	/* Calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	SizedPosToVec(pos, actor_size, start);
	start[2] = maxh * QUANT; /** start is now at the center of the floor of the originating cell and as high as the higher of the two floors. */

	/* Locate the destination point. */
	VectorSet(pos, ax, ay, z);
	SizedPosToVec(pos, actor_size, end);
	end[2] = start[2]; /** end is now at the center of the destination cell and at the same height as the starting point. */

	/* Test to see if c <= h or ac <= ah - indicates a filled cell. */
	if (c <= h || ac <= ah) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (doit)  Com_Printf("Destination cell filled. h = %i, c = %i, ah = %i, ac = %i\n", h, c, ah, ac);
		return z + 1;
	}

	/* test if the destination cell is blocked by a loaded model */
	if (h >= ac || ah >= c) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (doit)  Com_Printf("Destination cell filled. h = %i, c = %i, ah = %i, ac = %i\n", h, c, ah, ac);
		return z + 1;
	}

	/* If the destination floor is more than PATHFINDING_STEPUP higher than the base of the current floor
	 * AND the base of the current cell, then we can't go there. */
	if (RT_FLOOR(map, actor_size, ax, ay, z) - max(0, RT_FLOOR(map, actor_size, x, y, z)) > PATHFINDING_STEPUP) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		VectorCopy(end, brushesHit.floor);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;
		if (doit) Com_Printf("Destination cell too high up. cf:%i df:%i\n", RT_FLOOR(map, actor_size, x, y, z), RT_FLOOR(map, actor_size, ax, ay, z));
		return z + 1;
	}

	/* Point to where we need be. */
	VectorCopy(end, brushesHit.floor);
	VectorCopy(brushesHit.floor, brushesHit.ceiling);

	/* test if the destination cell is blocked by a loaded model */
	if (RT_CEILING(map, actor_size, ax, ay, z) - RT_FLOOR(map, actor_size, ax, ay, z) < PATHFINDING_MIN_OPENING) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (doit) Com_Printf("Destination cell filled. f:%i c:%i\n", RT_FLOOR(map, actor_size, ax, ay, z), RT_CEILING(map, actor_size, ax, ay, z));
		return z + 1;
	}



	if (doit) Com_Printf("h = %i, c = %i, ah = %i, ac = %i\n", h, c, ah, ac);


	/*
	 * OK, here's the plan: we can move between cells, provided that there are no obstructions
	 * highter than PATHFINDING_STEPUP from the floor of the current cell into the destination cell.  The cell being moved
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
	 * Part A: Look for the mandatory opening. Low point at the higher floor plus PATHFINDING_STEPUP,
	 * high point at original floor plus PATHFINDING_MIN_OPENING. If this is not open, there is no way to
	 * enter the cell.  There cannot be any obstructions above PATHFINDING_STEPUP because
	 * the floor would be too high to enter, and there cannot be obstructions below
	 * PATHFINDING_MIN_OPENING, otherwise the ceiling in the adjacent cell would be too low to enter.
	 * Technically, yes the total space checked is not PATHFINDING_MIN_OPENING tall, but
	 * above and below this are checked separately to verify the minimum height requirement.
	 * Remember fhat our trace line is height adjusted, so our test heights are relative to the floor!
	 */
	floor_p = PATHFINDING_STEPUP;
	ceil_p = PATHFINDING_MIN_OPENING;
	if (RT_ObstructedTrace(start, end, actor_size, ceil_p, floor_p)) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, x, y, z, dir) = 0;

		/* Record the debugging data in case it is used. */
		VectorCopy(tr_obstruction.endpos, brushesHit.floor);
		VectorCopy(brushesHit.floor, brushesHit.ceiling);
		brushesHit.obstructed = qtrue;

		if (doit) Com_Printf("No opening in required space between cells. hi:%i lo:%i\n", ceil_p + maxh , floor_p + maxh);
		return z + 1;
	}

	/*
	 * Part B: Look for additional space below PATHFINDING_STEPUP. Low points at the higher of the two floors;
	 * high points at the current floor_p, the current lowest valid floor.
	 */
	lo = max (0, ah - h); /* Relative to the current floor h */
	hi = floor_p;
	worst = lo;
	/* Move the floor brush to the lowest possible floor. */
	brushesHit.ceiling[2] += lo * QUANT;
	if (doit) Com_Printf("Checking for passage floor from %i to %i.\n", lo, hi);
	while (hi > lo) {
		if (doit) Com_Printf("Looking between %i and %i at (%i, %i) facing %i... ", lo, hi, x, y, dir);
		obstructed = RT_ObstructedTrace(start, end, actor_size, hi, lo);
		if (obstructed) {
			VectorCopy(tr_obstruction.endpos, brushesHit.floor);
			if (doit)  Com_Printf("found obstruction.\n");
			/* if hi and lo differ by one, then we are done.*/
			if (hi == lo + 1) {
				if (doit)  Com_Printf("Last possible block scanned.\n");
				break;
			}
			/* Since we cant use this, place lo halfway between hi and lo. */
			lo = (hi + lo) / 2;
		} else {
			/* Since we can use this, lower floor_p to lo and test between worst and floor_p. */
			floor_p = lo;
			if (doit)  Com_Printf("no obstructions found, floor_p = %i.\n", floor_p);
			hi = floor_p;
			lo = worst;
		}
	}
	/* We need the absolute height of floor_p, not relative to the current floor */
	floor_p += h;

	/*
	 * Part C: Look for additional space above PATHFINDING_STEPUP. Low points to ceil_p, the current
	 * valid ceiling; high points at the lower of both ceilings.
	 */
	lo = ceil_p;
	hi = min (c, ac) - h;
	worst = hi;
	/* Move the ceiling brush to the highest possible ceiling. */
	brushesHit.ceiling[2] += hi * QUANT;
	if (doit)  Com_Printf("Checking for passage ceiling from %i to %i.\n", lo, hi);
	while (hi > lo) {
		if (doit)  Com_Printf("Looking between %i and %i at (%i, %i) facing %i... ", lo, hi, x, y, dir);
		obstructed = RT_ObstructedTrace(start, end, actor_size, hi, lo);
		if (obstructed) {
			/* Record the debugging data in case it is used. */
			VectorCopy(tr_obstruction.endpos, brushesHit.ceiling);

			if (doit)  Com_Printf("found obstruction.\n");
			/* if hi and lo differ by one, then we are done.*/
			if (hi == lo + 1) {
				if (doit)  Com_Printf("Last possible block scanned.\n");
				break;
			}
			/* Since we cant use this, place hi halfway between hi and lo. */
			hi = (hi + lo) / 2;
		} else {
			/* Since we can use this, raise ceil_p to hi and test between worst and ceil_p. */
			ceil_p = hi;
			if (doit)  Com_Printf("no obstructions found, ceil_p = %i.\n", ceil_p);
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
	 * Also reduce ceil_p by one because ceilings divisible by 16 are only in the cells below.
	 */
	fz = h * QUANT / UNIT_HEIGHT;
	cz = (ceil_p - 1) * QUANT / UNIT_HEIGHT;

	/* last chance- if cz < z, then bail (and there is an error with the ceiling data somewhere */
	if (cz < z) {
		/* We can't go this way. */
		RT_CONN(map, actor_size, ax, ay, z, dir) = 0;
		/* Point to where we can't be. */
		brushesHit.obstructed = qtrue;
		if (doit) Com_Printf("Passage found but below current cell, h = %i, c = %i, ah = %i, ac = %i, floor_p=%i, ceil_p=%i, z = %i, cz = %i.\n", h, c, ah, ac, floor_p, ceil_p, z, cz);
		return z + 1;
	}

	/* Point to where we CAN be. */
	brushesHit.obstructed = qfalse;

	if (doit) Com_Printf("Passage found, floor_p=%i, ceil_p=%i. (%i to %i)\n", floor_p, ceil_p, fz, cz);

	assert (fz <= z && z <= cz);

	/* Last, update the routes of cells from (x, y, fz) to (x, y, cz) for direction dir */
	for (i = fz; i <= cz; i++) {
		/* Offset from the floor or the bottom of the current cell, whichever is higher. */
		RT_CONN_TEST(map, actor_size, x, y, i, dir);
		RT_CONN(map, actor_size, x, y, i, dir) = ceil_p - max(floor_p, i * UNIT_HEIGHT / QUANT);
		if (doit) Com_Printf("RT_CONN for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, i, actor_size, dir, RT_CONN(map, actor_size, x, y, i, dir));
	}
	/* RT_CONN(map, actor_size, ax, ay, z, dir) = ceil_p - floor_p; */

	/*
	 * Return the next z coordinate to scan- the cell above cz if fz==cz or z==cz,
	 * otherwise cz to check in case the ceiling of cells beneath is the floor of cell cz.
	 */
	if (cz == fz || cz == z)
		return cz + 1;
	return cz;
}
