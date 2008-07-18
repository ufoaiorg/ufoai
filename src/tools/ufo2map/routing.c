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
static routing_t map;

/** @brief world min and max values converted from vec to pos */
static ipos3_t wpMins, wpMaxs;


/**
 * @sa DoRouting
 */
static void CheckUnit (unsigned int unitnum)
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
		return;
	}

	/* calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	PosToVec(pos, end);

	/* step height check (the default function does not) */
	if (TestContents(end))
		map.step[y][x] |= 1 << z;

	/* Call the common CheckUnit function */
	RT_CheckUnit(&map, x, y, z);
}


/**
 * @sa DoRouting
 */
static void CheckConnections (unsigned int unitnum)
{
	/* get coordinates of that unit */
	const int z = unitnum / PATHFINDING_WIDTH / PATHFINDING_WIDTH;
	const int y = (unitnum / PATHFINDING_WIDTH) % PATHFINDING_WIDTH;
	const int x = unitnum % PATHFINDING_WIDTH;

	RT_UpdateConnection(&map, x, y, z, qtrue);
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

	/*	Com_Printf("(%i %i %i) (%i %i %i)\n", wpMins[0], wpMins[1], wpMins[2], wpMaxs[0], wpMaxs[1], wpMaxs[2]); */

	/* scan area heights */
	U2M_ProgressBar(CheckUnit, PATHFINDING_HEIGHT * PATHFINDING_WIDTH * PATHFINDING_WIDTH, qtrue, "UNITCHECK");

	/* scan connections */
	U2M_ProgressBar(CheckConnections, PATHFINDING_HEIGHT * PATHFINDING_WIDTH * PATHFINDING_WIDTH, qtrue, "CONNCHECK");

	/* store the data */
	data = curTile->routedata;
	*data++ = SH_LOW;
	*data++ = SH_BIG;
	data = CompressRouting(&(map.route[0][0][0]), data, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT);
	data = CompressRouting(&(map.fall[0][0]), data, PATHFINDING_WIDTH * PATHFINDING_WIDTH);
	data = CompressRouting(&(map.step[0][0]), data, PATHFINDING_WIDTH * PATHFINDING_WIDTH);

	curTile->routedatasize = data - curTile->routedata;

	/* reset our bsp info to remove the LEVEL_STEPON data */
	PopInfo();
}
