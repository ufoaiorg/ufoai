/**
 * @file routing.c
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


#include "bsp.h"
#include "../../common/tracing.h"
#include "../../common/routing.h"


#define SH_BIG	9
#define SH_LOW	2
/** @note @todo The old value for the normal step up (will become obsolete) */
byte sh_low = SH_LOW;

/** @note @todo The old value for the STEPON flagged step up (will become obsolete) */
byte sh_big = SH_BIG;

static const vec3_t move_vec[4] = {
	{ UNIT_SIZE,          0, 0},
	{-UNIT_SIZE,          0, 0},
	{         0,  UNIT_SIZE, 0},
	{         0, -UNIT_SIZE, 0} };

/** routing data structures */
static old_routing_t map;
static routing_t Nmap[ACTOR_MAX_SIZE]; /* A routing_t per size */

/** @brief world min and max values converted from vec to pos */
static ipos3_t wpMins, wpMaxs;


/**
 * @sa DoRouting
 */
static int CheckUnit (unsigned int unitnum)
{
	pos3_t pos;
	vec3_t end;

	/* get coordinates of that unit */
	const int z = unitnum / PATHFINDING_WIDTH / PATHFINDING_WIDTH;
	const int y = (unitnum / PATHFINDING_WIDTH) % PATHFINDING_WIDTH;
	const int x = unitnum % PATHFINDING_WIDTH;

	/* test bounds */
	if (x > wpMaxs[0] || y > wpMaxs[1] || z > wpMaxs[2]
			|| x < wpMins[0] || y < wpMins[1] || z < wpMins[2]) {
		/* don't enter - outside world */
		map.fall[y][x] |= 1 << z;
		return 0;
	}

	/* calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	PosToVec(pos, end);

	/* step height check (the default function does not) */
	if (TestContents(end))
		map.step[y][x] |= 1 << z;

	/* Call the common CheckUnit function */
	RT_CheckUnit(&map, x, y, z);

	return 0;
}


/**
 * @sa DoRouting
 */
static int NewCheckUnit (unsigned int unitnum)
{
	int  new_z;

	/* get coordinates of that unit */
	const int z = unitnum % PATHFINDING_HEIGHT;
	const int y = (unitnum / PATHFINDING_HEIGHT) % PATHFINDING_WIDTH;
	const int x = (unitnum / PATHFINDING_HEIGHT / PATHFINDING_WIDTH) % PATHFINDING_WIDTH;
	const int actor_size = unitnum / PATHFINDING_HEIGHT / PATHFINDING_WIDTH / PATHFINDING_WIDTH;

	/* test bounds - the size adjustment is needed because large actor cells occupy multiple cell units. */
	if (x > wpMaxs[0] - actor_size || y > wpMaxs[1] - actor_size || z > wpMaxs[2]
			|| x < wpMins[0] || y < wpMins[1] || z < wpMins[2] ) {
		/* don't enter - outside world */
		return 0;
	}

	/* Com_Printf("%i %i %i %i\n", x, y, z, size); */
	/* Call the common CheckUnit function */
	new_z = RT_NewCheckCell(Nmap, actor_size + 1, x, y, z);

	/* Com_Printf("z:%i nz:%i\n", z, new_z); */

	/* new_z should always be above z. */
	assert(new_z > z);

	/* Adjust unitnum if this check adjusted multiple cells. */
	return new_z - z -1;
}


/**
 * @sa DoRouting
 */
static int CheckConnections (unsigned int unitnum)
{
	/* get coordinates of that unit */
	const int z = unitnum / PATHFINDING_WIDTH / PATHFINDING_WIDTH;
	const int y = (unitnum / PATHFINDING_WIDTH) % PATHFINDING_WIDTH;
	const int x = unitnum % PATHFINDING_WIDTH;

	RT_UpdateConnection(&map, x, y, z, qtrue);

	return 0;
}


/**
 * @sa DoRouting
 */
static int NewCheckConnections (unsigned int unitnum)
{
	int  new_z;

	/* get coordinates of that unit */
	const int z = unitnum % PATHFINDING_HEIGHT;
	const int dir = (unitnum / PATHFINDING_HEIGHT) % CORE_DIRECTIONS;
	const int y = (unitnum / PATHFINDING_HEIGHT / CORE_DIRECTIONS) % PATHFINDING_WIDTH;
	const int x = (unitnum / PATHFINDING_HEIGHT / CORE_DIRECTIONS / PATHFINDING_WIDTH) % PATHFINDING_WIDTH;
	const int actor_size = unitnum / PATHFINDING_HEIGHT / PATHFINDING_WIDTH / PATHFINDING_WIDTH / CORE_DIRECTIONS;


	/* test bounds - the size adjustment is needed because large actor cells occupy multiple cell units. */
	if (x > wpMaxs[0] - actor_size || y > wpMaxs[1] - actor_size || z > wpMaxs[2]
			|| x < wpMins[0] || y < wpMins[1] || z < wpMins[2] ) {
		/* don't enter - outside world */
		/* Com_Printf("x%i y%i z%i dir%i size%i (%i, %i, %i) (%i, %i, %i)\n", x, y, z, dir, size, wpMins[0], wpMins[1], wpMins[2], wpMaxs[0], wpMaxs[1], wpMaxs[2]); */
		return 0;
	}

	/* Com_Printf("%i %i %i %i %i (%i, %i, %i) (%i, %i, %i)\n", x, y, z, dir, size, wpMins[0], wpMins[1], wpMins[2], wpMaxs[0], wpMaxs[1], wpMaxs[2]); */

	new_z = RT_NewUpdateConnection(Nmap, actor_size + 1, x, y, z, dir);

	/* Com_Printf("z:%i nz:%i\n", z, new_z); */

	/* new_z should never be below z. */
	assert(new_z >= z);

	/* Adjust unitnum if this check adjusted multiple cells. */
	return new_z - z -1;
}


/**
 * @brief Calculates the routing of a map
 * @sa CheckUnit
 * @sa CheckConnections
 * @sa ProcessWorldModel
 */
void DoRouting (void)
{
	unsigned int i;
	byte *data;

	/* Record the current mapTiles[0] state so we can remove STEPON when done looking for steps */
	PushInfo();

	/* Set the model count to LEVEL_STEPON. */
	curTile->nummodels = LEVEL_MAX; /* must be one more than the last index */

	/* process stepon level */
	ProcessLevel(LEVEL_STEPON);

	/* build tracing structure */
	EmitBrushes();
	EmitPlanes();
	/** @note LEVEL_TRACING is not an actual level- LEVEL_MAX comes after LEVEL_STEPON */
	MakeTracingNodes(LEVEL_MAX);

	/* reset */
	memset(map.fall, 0, sizeof(map.fall));
	memset(map.filled, 0, sizeof(map.filled));

	/* Reset the whole block of map data to 0 */
	memset(Nmap, 0, sizeof(Nmap));

	/* get world bounds for optimizing */
	VecToPos(worldMins, wpMins);
	VectorSubtract(wpMins, v_epsilon, wpMins);

	VecToPos(worldMaxs, wpMaxs);
	VectorAdd(wpMaxs, v_epsilon, wpMaxs);

	for (i = 0; i < 3; i++) {
		if (wpMins[i] < 0)
			wpMins[i] = 0;
		if (wpMaxs[i] > ROUTING_NOT_REACHABLE)
			wpMaxs[i] = ROUTING_NOT_REACHABLE;
	}

	/* scan area heights */
	U2M_ProgressBar(CheckUnit, PATHFINDING_HEIGHT * PATHFINDING_WIDTH * PATHFINDING_WIDTH, qtrue, "UNITCHECK");

	/* scan area heights with new code*/
	U2M_ProgressBar(NewCheckUnit, PATHFINDING_HEIGHT * PATHFINDING_WIDTH * PATHFINDING_WIDTH * ACTOR_MAX_SIZE, qtrue, "UNITCHEK2");

	/* Output the floor trace file if set */
	if (config.generateTraceFile) {
		char filename[MAX_OSPATH];
		FILE *handle;
		int x, y, z;

		strncpy(filename, baseFilename, sizeof(filename) - 1);
		COM_DefaultExtension(filename, sizeof(filename), ".floor.csv");
		handle = fopen(filename, "w");
		fprintf(handle, ",");
		for (x = 0; x < PATHFINDING_WIDTH; x++)
				fprintf(handle, "%i,", x);
		for (z = PATHFINDING_HEIGHT - 1; z >= 0; z--) {
			for (y = PATHFINDING_WIDTH - 1; y >= 0; y--) {
				fprintf(handle, "%i - %i,", z ,y);
				for (x = 0; x < PATHFINDING_WIDTH; x++) {
					/* compare results */
					if (map.filled[y][x] & (1 << z) && RT_CEILING(Nmap, 1, x, y, z) - RT_FLOOR(Nmap, 1, x, y, z) < PATHFINDING_MIN_OPENING) {
						fprintf(handle, "M Filled,");
					} else if ((map.fall[y][x] & (1 << z)) && RT_FLOOR(Nmap, 1, x, y, z) < 0) {
						fprintf(handle, "M Fall %i %i,", RT_FLOOR(Nmap, 1, x, y, z), RT_CEILING(Nmap, 1, x, y, z));
					} else if ((map.route[z][y][x] & 0x0F) == RT_FLOOR(Nmap, 1, x, y, z)) {
						fprintf(handle, "M Floor %i %i,", RT_FLOOR(Nmap, 1, x, y, z), RT_CEILING(Nmap, 1, x, y, z));
					} else {
						/* old results */
						if (map.filled[y][x] & (1 << z)) {
							fprintf(handle, "X ");
						} else if (map.fall[y][x] & (1 << z)) {
							fprintf(handle, "F ");
						} else {
							fprintf(handle, "  ");
						}

						fprintf(handle, "%i ", map.route[z][y][x] & 0x0F);

						/* new results */
						if (RT_CEILING(Nmap, 1, x, y, z) - RT_FLOOR(Nmap, 1, x, y, z) < PATHFINDING_MIN_OPENING) { /* filled */
							fprintf(handle, "X:%i %i,", RT_FLOOR(Nmap, 1, x, y, z), RT_CEILING(Nmap, 1, x, y, z));
						} else {
							fprintf(handle, "H:%i", RT_FLOOR(Nmap, 1, x, y, z));
							if (RT_FLOOR(Nmap, 1, x, y, z) < -PATHFINDING_MAX_FALL)
								fprintf(handle, "(F)");
							fprintf(handle, " C:%i,", RT_CEILING(Nmap, 1, x, y, z));
						}
					}

					/* For giggles, backport our new height data into the old data structure. */
					map.route[z][y][x] &= 0xF0;
					if (RT_CEILING(Nmap, 1, x, y, z) - RT_FLOOR(Nmap, 1, x, y, z) >= PATHFINDING_MIN_OPENING) {
						map.route[z][y][x] |= min(15, max(0, RT_FLOOR(Nmap, 1, x, y, z)));
						map.filled[y][x] &= ~(1 << z);
					} else {
						map.filled[y][x] |= (1 << z);
					}
					if (RT_FLOOR(Nmap, 1, x, y, z) < 0) {
						map.fall[y][x] |= (1 << z);
					} else {
						map.fall[y][x] &= ~(1 << z);
					}
				}
				fprintf(handle, "\n");
			}
			fprintf(handle, "\n");
		}
		fclose(handle);

		/* A separate elevation file- this will be the only file of these two created
		 * after the old tracing code is removed. */
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		COM_DefaultExtension(filename, sizeof(filename), ".elevation.csv");
		handle = fopen(filename, "w");
		fprintf(handle, ",");
		for (x = 0; x < PATHFINDING_WIDTH; x++)
				fprintf(handle, "%i,", x);
		for (z = PATHFINDING_HEIGHT - 1; z >= 0; z--) {
			for (y = PATHFINDING_WIDTH - 1; y >= 0; y--) {
				fprintf(handle, "%i - %i,", z ,y);
				for (x = 0; x < PATHFINDING_WIDTH; x++) {
					/* compare results */
					fprintf(handle, "h:%i c:%i,", RT_FLOOR(Nmap, 1, x, y, z), RT_CEILING(Nmap, 1, x, y, z));
				}
				fprintf(handle, "\n");
			}
			fprintf(handle, "\n");
		}
		fclose(handle);
	}

	/* scan connections */
	U2M_ProgressBar(CheckConnections, PATHFINDING_HEIGHT * PATHFINDING_WIDTH * PATHFINDING_WIDTH, qtrue, "CONNCHECK");

	/* scan area heights with new code*/
	U2M_ProgressBar(NewCheckConnections, PATHFINDING_HEIGHT * PATHFINDING_WIDTH * PATHFINDING_WIDTH * CORE_DIRECTIONS * ACTOR_MAX_SIZE, qtrue, "CONNCHEK2");

	if (config.generateTraceFile) {
		char filename[MAX_OSPATH];
		FILE *handle;
		int x, y, z, cc1, cc2;

		strncpy(filename, baseFilename, sizeof(filename) - 1);
		COM_DefaultExtension(filename, sizeof(filename), ".walls.csv");
		handle = fopen(filename, "w");
		fprintf(handle, ",");
		for (x = 0; x < PATHFINDING_WIDTH; x++)
				fprintf(handle, "x = %i,", x);
		for (z = PATHFINDING_HEIGHT - 1; z >= 0; z--) {
			for (y = PATHFINDING_WIDTH - 1; y >= 0; y--) {
				fprintf(handle, "z = %i, y = %i,", z ,y);
				for (x = 0; x < PATHFINDING_WIDTH; x++) {
					/* compare results */
					fprintf(handle, "\"");

					/* NW corner */
					cc1= R_CONN_NX_PY(&map,x,y,z);
					cc2 = RT_CONN_NX_PY(Nmap, 0, x, y, z) >= PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_NX_PY(Nmap, 0, x, y, z));

					/* N side */
					cc1= R_CONN_PY(&map,x,y,z);
					cc2 = RT_CONN_PY(Nmap, 0, x, y, z) >= PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_PY(Nmap, 0, x, y, z));

					/* NE corner */
					cc1= R_CONN_PX_PY(&map,x,y,z);
					cc2 = RT_CONN_PX_PY(Nmap, 0, x, y, z) >= PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_PX_PY(Nmap, 0, x, y, z));

					fprintf(handle, "\n");

					/* W side */
					cc1= R_CONN_NX(&map,x,y,z);
					cc2 = RT_CONN_NX(Nmap, 0, x, y, z) >= PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_NX(Nmap, 0, x, y, z));

					fprintf(handle, "___");

					/* E side */
					cc1= R_CONN_PX(&map,x,y,z);
					cc2 = RT_CONN_PX(Nmap, 0, x, y, z) > PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_PX(Nmap, 0, x, y, z));

					fprintf(handle, "\n");

					/* SW corner */
					cc1= R_CONN_NX_NY(&map,x,y,z);
					cc2 = RT_CONN_NX_NY(Nmap, 0, x, y, z) > PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_NX_NY(Nmap, 0, x, y, z));

					/* S side */
					cc1= R_CONN_NY(&map,x,y,z);
					cc2 = RT_CONN_NY(Nmap, 0, x, y, z) > PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_NY(Nmap, 0, x, y, z));

					/* SE corner */
					cc1= R_CONN_PX_NY(&map,x,y,z);
					cc2 = RT_CONN_PX_NY(Nmap, 0, x, y, z) > PATHFINDING_MIN_OPENING;
					if (cc1 && cc2) {
						fprintf(handle, "*");
					} else if (!cc1 && cc2) {
						fprintf(handle, "+");
					} else if (cc1 && !cc2) {
						fprintf(handle, "o");
					} else {
						fprintf(handle, " ");
					}
					fprintf(handle, "%3i ", RT_CONN_PX_NY(Nmap, 0, x, y, z));

					fprintf(handle, "\",");
				}
				fprintf(handle, "\n");
			}
			fprintf(handle, "\n");
		}
		fclose(handle);
	}

	/* store the data */
	data = curTile->routedata;
	/*
	*data++ = SH_LOW;
	*data++ = SH_BIG;
	data = CompressRouting(&(map.route[0][0][0]), data, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT);
	Com_Printf("map.route: %i\n", sizeof(map.route));
	data = CompressRouting(&(map.fall[0][0]), data, PATHFINDING_WIDTH * PATHFINDING_WIDTH);
	Com_Printf("map.fall: %i\n", sizeof(map.fall));
	data = CompressRouting(&(map.step[0][0]), data, PATHFINDING_WIDTH * PATHFINDING_WIDTH);
	Com_Printf("map.step: %i\n", sizeof(map.step));
	*/

	data = CompressRouting((byte*)wpMins, data, sizeof(wpMins));
	Com_Printf("wpMins: %u\n", sizeof(wpMins));
	data = CompressRouting((byte*)wpMaxs, data, sizeof(wpMaxs));
	Com_Printf("wpMins: %u\n", sizeof(wpMaxs));
	data = CompressRouting((byte*)Nmap, data, sizeof(Nmap));
	Com_Printf("Nmap: %u\n", sizeof(Nmap));

	curTile->routedatasize = data - curTile->routedata;

	/* reset our bsp info to remove the LEVEL_STEPON data */
	PopInfo();
}
