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
GAME RELATED TRACING
===============================================================================
*/


/**
 * @param[in] x The x position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] y The y position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] z The z position in the routing arrays (0 - PATHFINDING_HEIGHT-1)
 * @sa Grid_RecalcRouting
 */
void RT_CheckUnit (routing_t * map, int x, int y, pos_t z)
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
	start[2] -= UNIT_HEIGHT / 2 - QUANT;
	end[2]   -= UNIT_HEIGHT / 2 + QUANT;

	/* test for fall down */
	if (RT_TESTLINE(start, end, TL_FLAG_ACTORCLIP)) {
		PosToVec(pos, end);
		VectorAdd(end, dup_vec, start);
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
void RT_UpdateConnection (routing_t * map, int x, int y, byte z, qboolean fill)
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
	sh = R_STEP(map, x, y, z) ? sh_big : sh_low;
	h = R_HEIGHT(map, x, y, z);

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
		if (R_HEIGHT(map, ax, ay, az) > h)
			continue;

		/* test filled */
		if (fill && (map->filled[ay][ax] & (1 << az)))
			continue;

		/* test ground under the neighbor position */
		if (((map->fall[ay][ax] & deep_fall[az]) == deep_fall[az]))
			continue;

		/* Reset the center */
		VectorCopy(center, start);
		/* Set the end point to the edge of the field unit in the direction traveling */
		/** @todo: should this check into the center of the adjacent field unit insead of just to the edge? */
		VectorSet(end, start[0] + (UNIT_HEIGHT / 2) * dvecs[dir][0], start[1] + (UNIT_HEIGHT / 2) * dvecs[dir][1], start[2]);

		/** @todo: Do we need to use a bounding box to check for things in the way of this move? */
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
void Grid_DumpMap (struct routing_s *map, int lx, int ly, int lz, int hx, int hy, int hz)
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
				Com_Printf("%s%s%s%s%s%s%2i "
					, R_CONN_NX(map, x, y, z) ? "w" : " "
					, R_CONN_PY(map, x, y, z) ? "n" : " "
					, R_CONN_NY(map, x, y, z) ? "s" : " "
					, R_CONN_PX(map, x, y, z) ? "e" : " "
					, R_STEP(map, x, y, z) ? "S" : " "
					, R_FALL(map, x, y, z) ? "F" : " "
					, R_HEIGHT(map, x, y, z));
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
			trace = TR_CompleteBoxTrace(origin, origin, mins, maxs, 0x1FF, MASK_ALL);
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
			trace = TR_CompleteBoxTrace(origin, origin, mins, maxs, 0x1FF, MASK_ALL);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* Dump the client map */
	Grid_DumpMap(map, start[0], start[1], start[2], end[0], end[1], end[2]);
}
