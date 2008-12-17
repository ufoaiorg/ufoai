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
	int new_z;

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
	const int basenum = unitnum * PATHFINDING_HEIGHT;
	int newnum;
	for (newnum = basenum + PATHFINDING_HEIGHT - 1; newnum >= basenum; newnum--)
		newnum += CheckUnit(newnum);
}


/**
 * @sa DoRouting
 */
static int CheckConnections (unsigned int unitnum)
{
	int new_z;

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
	const int basenum = unitnum * PATHFINDING_HEIGHT;
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
	vec3_t mins, maxs;
	pos3_t pos;

	/* Turn on trace debugging if requested. */
	if (config.generateDebugTrace)
		debugTrace = qtrue;

	/* Record the current mapTiles[0] state so we can remove all CLIPS when done. */
	PushInfo();

	/* build tracing structure */
	EmitBrushes();
	EmitPlanes(); /** This is needed for tracing to work!!! */
	/** @note LEVEL_TRACING is not an actual level */
	MakeTracingNodes(LEVEL_ACTORCLIP + 1);

	/* Reset the whole block of map data to 0 */
	memset(Nmap, 0, sizeof(Nmap));

	/* get world bounds for optimizing */
	RT_GetMapSize(mins, maxs);
	/* Com_Printf("Vectors: (%f, %f, %f) to (%f, %f, %f)\n", mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2]); */
	VecToPos(mins, wpMins);
	VecToPos(maxs, wpMaxs);

	/* scan area heights */
	RunThreadsOn(CheckUnitThread, PATHFINDING_WIDTH * PATHFINDING_WIDTH * ACTOR_MAX_SIZE, config.verbosity >= VERB_NORMAL, "UNITCHECK");

	/* scan connections */
	RunThreadsOn(CheckConnectionsThread, PATHFINDING_WIDTH * PATHFINDING_WIDTH * CORE_DIRECTIONS * ACTOR_MAX_SIZE, config.verbosity >= VERB_NORMAL, "CONNCHECK");

	/* Try to shrink the world bounds along the x and y coordinates */
	for (i = 0; i < 2; i++) {
		/* Increase the mins */
		while (wpMaxs[i] > wpMins[i]) {
			VectorSet(pos, wpMins[0], wpMins[1], wpMaxs[2]);
			for (pos[i] = wpMins[i]; pos[i] <= wpMaxs[i]; pos[i]++) {
				if (RT_FLOOR(Nmap, 1, pos[0], pos[1], wpMaxs[2]) + wpMaxs[2] * CELL_HEIGHT != -1)
					break;
			}
			if (pos[i] <= wpMaxs[i])
				break;
			wpMins[i ^ 1]++;
		}
		/* Increase the mins */
		while (wpMaxs[i] > wpMins[i]) {
			VectorCopy(wpMaxs, pos);
			for (pos[i] = wpMins[i]; pos[i] <= wpMaxs[i]; pos[i]++) {
				if (RT_FLOOR(Nmap, 1, pos[0], pos[1], wpMaxs[2]) + wpMaxs[2] * CELL_HEIGHT != -1)
					break;
			}
			if (pos[i] <= wpMaxs[i])
				break;
			wpMaxs[i ^ 1]--;
		}
	}

	/* Output the floor trace file if set */
	if (config.generateTraceFile) {
		RT_WriteCSVFiles(Nmap, baseFilename, wpMins, wpMaxs);
	}

	/* store the data */
	data = curTile->routedata;
	data = CompressRouting((byte*)wpMins, data, sizeof(wpMins));
	data = CompressRouting((byte*)wpMaxs, data, sizeof(wpMaxs));
	data = CompressRouting((byte*)Nmap, data, sizeof(Nmap));

	curTile->routedatasize = data - curTile->routedata;

	/* Remove the CLIPS fom the tracing structure by resetting it. */
	PopInfo();
}
