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


static const vec3_t move_vec[4] = {
	{ UNIT_SIZE,          0, 0},
	{-UNIT_SIZE,          0, 0},
	{         0,  UNIT_SIZE, 0},
	{         0, -UNIT_SIZE, 0} };

/** routing data structures */
static routing_t Nmap[ACTOR_MAX_SIZE]; /* A routing_t per size */

/** @brief world min and max values converted from vec to pos */
static ipos3_t wpMins, wpMaxs;


/**
 * @sa CheckUnitThread
 */
static int CheckUnit (unsigned int unitnum)
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
	new_z = RT_CheckCell(Nmap, actor_size + 1, x, y, z);

	/* Com_Printf("z:%i nz:%i\n", z, new_z); */

	/* new_z should never be above z. */
	assert(new_z <= z);

	/* Adjust unitnum if this check adjusted multiple cells. */
	return new_z - z;
}

/**
 * @brief A wrapper for CheckUnit that is thread safe.
 * @sa DoRouting
 */
static void CheckUnitThread (unsigned int unitnum)
{
	int basenum = unitnum * PATHFINDING_HEIGHT;
	int newnum;
	for (newnum = basenum + PATHFINDING_HEIGHT - 1; newnum >= basenum; newnum--)
		newnum += CheckUnit(newnum);
}


/**
 * @sa DoRouting
 */
static int CheckConnections (unsigned int unitnum)
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

	new_z = RT_UpdateConnection(Nmap, actor_size + 1, x, y, z, dir);

	/* Com_Printf("z:%i nz:%i\n", z, new_z); */

	/* new_z should never be below z. */
	assert(new_z >= z);

	/* Adjust unitnum if this check adjusted multiple cells. */
	return new_z - z;
}

/**
 * @brief A wrapper for CheckConnections that is thread safe.
 * @sa DoRouting
 */
static void CheckConnectionsThread (unsigned int unitnum)
{
	int basenum = unitnum * PATHFINDING_HEIGHT;
	int newnum;
	for (newnum = basenum; newnum < basenum + PATHFINDING_HEIGHT; newnum++)
		newnum += CheckConnections(newnum);
}


/**
 * @brief Calculates the routing of a map
 * @sa CheckUnit
 * @sa CheckConnections
 * @sa ProcessWorldModel
 */
void DoRouting (void)
{
	int i;
	byte *data;

	/* Turn on trace debugging if requested. */
	if (config.generateDebugTrace)
		debugTrace = qtrue;

	/* Record the current mapTiles[0] state so we can remove all CLIPS when done. */
	PushInfo();

	/* build tracing structure */
	EmitBrushes();
	EmitPlanes(); /** This is needed for tracing to work!!! */
	/** @note LEVEL_TRACING is not an actual level- LEVEL_MAX comes after LEVEL_STEPON */
	MakeTracingNodes(LEVEL_ACTORCLIP + 1);

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
		if (wpMaxs[i] > PATHFINDING_WIDTH)
			wpMaxs[i] = PATHFINDING_WIDTH;
	}
	if (wpMaxs[2] > PATHFINDING_HEIGHT)
		wpMaxs[2] = PATHFINDING_HEIGHT;

	/* scan area heights */
	RunSingleThreadOn(CheckUnitThread, PATHFINDING_WIDTH * PATHFINDING_WIDTH * ACTOR_MAX_SIZE, config.verbosity >= VERB_NORMAL, "UNITCHECK");

	/* scan connections */
	RunSingleThreadOn(CheckConnectionsThread, PATHFINDING_WIDTH * PATHFINDING_WIDTH * CORE_DIRECTIONS * ACTOR_MAX_SIZE, config.verbosity >= VERB_NORMAL, "CONNCHECK");

	/* Output the floor trace file if set */
	if (config.generateTraceFile) {
		char filename[MAX_OSPATH];
		FILE *handle;
		int x, y, z;

		/* An elevation file- dumps the floor and ceiling levels relative to each cell. */
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		COM_DefaultExtension(filename, sizeof(filename), ".elevation.csv");
		handle = fopen(filename, "w");
		fprintf(handle, ",");
		for (x = wpMins[0]; x <= wpMaxs[0]; x++)
				fprintf(handle, "x:%i,", x);
		for (z = wpMaxs[2]; z >= wpMins[2]; z--) {
			for (y = wpMaxs[1]; y >= wpMins[1]; y--) {
				fprintf(handle, "z:%i  y:%i,", z ,y);
				for (x = wpMins[0]; x <= wpMaxs[0]; x++) {
					/* compare results */
					fprintf(handle, "h:%i c:%i,", RT_FLOOR(Nmap, 1, x, y, z), RT_CEILING(Nmap, 1, x, y, z));
				}
				fprintf(handle, "\n");
			}
			fprintf(handle, "\n");
		}
		fclose(handle);

		/* Output the walls/passage file. */
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		COM_DefaultExtension(filename, sizeof(filename), ".walls.csv");
		handle = fopen(filename, "w");
		fprintf(handle, ",");
		for (x = wpMins[0]; x <= wpMaxs[0]; x++)
				fprintf(handle, "x:%i,", x);
		fprintf(handle, "\n");
		for (z = wpMaxs[2]; z >= wpMins[2]; z--) {
			for (y = wpMaxs[1]; y >= wpMins[1]; y--) {
				fprintf(handle, "z:%i  y:%i,", z ,y);
				for (x = wpMins[0]; x <= wpMaxs[0]; x++) {
					/* compare results */
					fprintf(handle, "\"");

					/* NW corner */
					fprintf(handle, "%3i ", RT_CONN_NX_PY(Nmap, 1, x, y, z));

					/* N side */
					fprintf(handle, "%3i ", RT_CONN_PY(Nmap, 1, x, y, z));

					/* NE corner */
					fprintf(handle, "%3i ", RT_CONN_PX_PY(Nmap, 1, x, y, z));

					fprintf(handle, "\n");

					/* W side */
					fprintf(handle, "%3i ", RT_CONN_NX(Nmap, 1, x, y, z));

					/* Center - display floor height */
					fprintf(handle, "_%+2i_ ", RT_FLOOR(Nmap, 1, x, y, z));

					/* E side */
					fprintf(handle, "%3i ", RT_CONN_PX(Nmap, 1, x, y, z));

					fprintf(handle, "\n");

					/* SW corner */
					fprintf(handle, "%3i ", RT_CONN_NX_NY(Nmap, 1, x, y, z));

					/* S side */
					fprintf(handle, "%3i ", RT_CONN_NY(Nmap, 1, x, y, z));

					/* SE corner */
					fprintf(handle, "%3i ", RT_CONN_PX_NY(Nmap, 1, x, y, z));

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
	data = CompressRouting((byte*)wpMaxs, data, sizeof(wpMaxs));
	data = CompressRouting((byte*)Nmap, data, sizeof(Nmap));

	curTile->routedatasize = data - curTile->routedata;

	/* Remove the CLIPS fom the tracing structure by resetting it. */
	PopInfo();
}
