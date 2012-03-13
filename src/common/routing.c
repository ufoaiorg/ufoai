/**
 * @file src/common/routing.c
 * @brief grid pathfinding and routing
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
  LOCAL CONSTANTS
==========================================================
*/

#define RT_NO_OPENING -1

/* Width of the box required to stand in a cell by an actor's feet.  */
#define halfMicrostepSize (PATHFINDING_MICROSTEP_SIZE / 2 - DIST_EPSILON)
/* This is a template for the extents of the box used by an actor's feet. */
static const box_t footBox = {{-halfMicrostepSize, -halfMicrostepSize, 0},
						{ halfMicrostepSize,  halfMicrostepSize, 0}};

/* Width of the box required to stand in a cell by an actor's torso.  */
#define half1x1Width (UNIT_SIZE * 1 / 2 - WALL_SIZE - DIST_EPSILON)
#define half2x2Width (UNIT_SIZE * 2 / 2 - WALL_SIZE - DIST_EPSILON)
/* These are templates for the extents of the box used by an actor's torso. */
static const box_t actor1x1Box = {{-half1x1Width, -half1x1Width, 0},
							{ half1x1Width,  half1x1Width, 0}};
static const box_t actor2x2Box = {{-half2x2Width, -half2x2Width, 0},
							{ half2x2Width,  half2x2Width, 0}};

/*
==========================================================
  LOCAL TYPES
==========================================================
*/

/**
 * @brief RT_data_s contains the essential data that is passed to most of the RT_* functions
 */
typedef struct RT_data_s {
	mapTiles_t *mapTiles;
	routing_t *map;				/**< The routing table */
	actorSizeEnum_t actorSize;	/**< The size of the actor, in cells */
	const char **list;			/**< The local models list */
} RT_data_t;

static inline void RT_ConnSet (RT_data_t *rtd, const int x, const int y, const int z, const int dir, const int val)
{
	RT_CONN(rtd->map, rtd->actorSize, x, y, z, dir) = val;
}

static inline void RT_StepupSet (RT_data_t *rtd, const int x, const int y, const int z, const int dir, const int val)
{
	RT_STEPUP(rtd->map, rtd->actorSize, x, y, z, dir) = val;
}

static inline void RT_ConnSetNoGo (RT_data_t *rtd, const int x, const int y, const int z, const int dir)
{
	RT_ConnSet(rtd, x, y, z, dir, 0);
	RT_STEPUP(rtd->map, rtd->actorSize, x, y, z, dir) = PATHFINDING_NO_STEPUP;
}

/**
 * @brief A 'place' is a part of a grid column where an actor can exist
 * Unlike for a grid-cell, floor and ceiling are absolute values
 */
typedef struct place_s {
	pos3_t cell;	/**< coordinates of the grid-cell this was derived from. */
	int floor;		/**< The floor of the place, given in absolute QUANTs */
	int ceiling;	/**< The ceiling of it, given in absolute QUANTs. */
	int floorZ;		/**< The level (0-7) of the floor. */
	qboolean usable;/**< does an actor fit in here ? */
} place_t;

static inline void RT_PlaceInit (const routing_t *map, const actorSizeEnum_t actorSize, place_t *p, const int x, const int y, const int z)
{
	const int relCeiling = RT_CEILING(map, actorSize, x, y, z);
	p->cell[0] = x;
	p->cell[1] = y;
	p->cell[2] = z;
	p->floor = RT_FLOOR(map, actorSize, x, y, z) + z * CELL_HEIGHT;
	p->ceiling = relCeiling + z * CELL_HEIGHT;
	p->floorZ = max(0, p->floor / CELL_HEIGHT) ;
	p->usable = (relCeiling && p->floor > -1 && p->ceiling - p->floor >= PATHFINDING_MIN_OPENING) ? qtrue : qfalse;
}

static inline qboolean RT_PlaceIsUsable (const place_t* p)
{
	return p->usable;
}

static inline qboolean RT_PlaceDoesIntersectEnough (const place_t* p, const place_t* other)
{
	return (min(p->ceiling, other->ceiling) - max(p->floor, other->floor) >= PATHFINDING_MIN_OPENING);
}

/**
 * @brief This function detects a special stairway situation, where one place is right
 * in front of a stairway and has a floor at eg. 1 and a ceiling at eg. 16.
 * The other place has the beginning of the stairway, so the floor is at eg. 6
 * and the ceiling is that of the higher level, eg. 32.
 */
static inline int RT_PlaceIsShifted (const place_t* p, const place_t* other)
{
	if (!RT_PlaceIsUsable(p) || !RT_PlaceIsUsable(other))
		return 0;
	if (p->floor < other->floor && p->ceiling < other->ceiling)
		return 1;	/* stepping up */
	if (p->floor > other->floor && p->ceiling > other->ceiling)
		return 2;	/* stepping down */
	return 0;
}

/**
 * @brief An 'opening' describes the connection between two adjacent spaces where an actor can exist in a cell
 * @note Note that if size is @c 0, the other members are undefined. They may contain reasonable values, though
 */
typedef struct opening_s {
	int size;		/**< The opening size (max actor height) that can travel this passage. */
	int base;		/**< The base height of the opening, given in abs QUANTs */
	int stepup;		/**< The stepup needed to travel through this passage in this direction. */
	int invstepup;	/**< The stepup needed to travel through this passage in the opposite direction. */
} opening_t;

/*
==========================================================
  GRID ORIENTED MOVEMENT AND SCANNING
==========================================================
*/

#ifdef DEBUG
/**
 * @brief Dumps contents of a map to console for inspection.
 * @sa Grid_RecalcRouting
 * @param[in] map The routing map (either server or client map)
 * @param[in] actorSize The size of the actor along the X and Y axis in cell units
 * @param[in] lx The low end of the x range updated
 * @param[in] ly The low end of the y range updated
 * @param[in] lz The low end of the z range updated
 * @param[in] hx The high end of the x range updated
 * @param[in] hy The high end of the y range updated
 * @param[in] hz The high end of the z range updated
 */
static void RT_DumpMap (const routing_t *map, actorSizeEnum_t actorSize, int lx, int ly, int lz, int hx, int hy, int hz)
{
	int x, y, z;

	Com_Printf("\nRT_DumpMap (%i %i %i) (%i %i %i)\n", lx, ly, lz, hx, hy, hz);
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
					, RT_CONN_NX(map, actorSize, x, y, z) ? "w" : " "
					, RT_CONN_PY(map, actorSize, x, y, z) ? "n" : " "
					, RT_CONN_NY(map, actorSize, x, y, z) ? "s" : " "
					, RT_CONN_PX(map, actorSize, x, y, z) ? "e" : " "
					);
			}
			Com_Printf("\n");
		}
	}
}

/**
 * @brief Dumps contents of the entire client map to console for inspection.
 * @param[in] map A pointer to the map being dumped
 */
void RT_DumpWholeMap (mapTiles_t *mapTiles, const routing_t *map)
{
	box_t box;
	vec3_t normal, origin;
	pos3_t start, end, test;
	trace_t trace;
	int i;

	/* Initialize start, end, and normal */
	VectorClear(start);
	VectorSet(end, PATHFINDING_WIDTH - 1, PATHFINDING_WIDTH - 1, PATHFINDING_HEIGHT - 1);
	VectorSet(normal, UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2);
	VectorClear(origin);

	for (i = 0; i < 3; i++) {
		/* Lower positive boundary */
		while (end[i] > start[i]) {
			/* Adjust ceiling */
			VectorCopy(start, test);
			test[i] = end[i] - 1; /* test is now one floor lower than end */
			/* Prep boundary box */
			PosToVec(test, box.mins);
			VectorSubtract(box.mins, normal, box.mins);
			PosToVec(end, box.maxs);
			VectorAdd(box.maxs, normal, box.maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE_SIZE(mapTiles, origin, origin, &box, NULL);
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
			PosToVec(start, box.mins);
			VectorSubtract(box.mins, normal, box.mins);
			PosToVec(test, box.maxs);
			VectorAdd(box.maxs, normal, box.maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE_SIZE(mapTiles, origin, origin, &box, NULL);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* Dump the client map */
	RT_DumpMap(map, 0, start[0], start[1], start[2], end[0], end[1], end[2]);
}
#endif

/**
 * @brief Calculate the map size via model data and store grid size
 * in map_min and map_max. This is done with every new map load
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[out] map_min The lower extents of the current map.
 * @param[out] map_max The upper extents of the current map.
 * @sa CMod_LoadRouting
 * @sa DoRouting
 */
void RT_GetMapSize (mapTiles_t *mapTiles, vec3_t map_min, vec3_t map_max)
{
	box_t box;
	const vec3_t normal = {UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2};
	pos3_t start, end, test;
	vec3_t origin;
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
			test[i] = end[i]; /* the box from test to end is now one cell high */
			/* Prep boundary box */
			PosToVec(test, box.mins);
			VectorSubtract(box.mins, normal, box.mins);
			PosToVec(end, box.maxs);
			VectorAdd(box.maxs, normal, box.maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE_SIZE(mapTiles, origin, origin, &box, NULL);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, lower the boundary. */
			end[i]--;
		}

		/* Raise negative boundary */
		while (end[i] > start[i]) {
			/* Adjust ceiling */
			VectorCopy(end, test);
			test[i] = start[i]; /* the box from start to test is now one cell high */
			/* Prep boundary box */
			PosToVec(start, box.mins);
			VectorSubtract(box.mins, normal, box.mins);
			PosToVec(test, box.maxs);
			VectorAdd(box.maxs, normal, box.maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			trace = RT_COMPLETEBOXTRACE_SIZE(mapTiles, origin, origin, &box, NULL);
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
 * @brief Check if pos is on solid ground
 * @param[in] map The map's routing data
 * @param[in] actorSize The size of the actor along the X and Y axis in cell units
 * @param[in] pos The position to check below
 * @return true if solid
 * @sa CL_AddTargetingBox
 * @todo see CL_ActorMoveMouse
 */
qboolean RT_AllCellsBelowAreFilled (const routing_t * map, const int actorSize, const pos3_t pos)
{
	int i;

	/* the -1 level is considered solid ground */
	if (pos[2] == 0)
		return qtrue;

	for (i = 0; i < pos[2]; i++) {
		if (RT_CEILING(map, actorSize, pos[0], pos[1], i) != 0)
			return qfalse;
	}
	return qtrue;
}

/**
 * @brief This function looks to see if an actor of a given size can occupy a cell(s) and if so
 *	identifies the floor and ceiling for that cell. If the cell has no floor, the floor will be negative
 *  with 0 indicating the base for the cell(s).  If there is no ceiling in the cell, the first ceiling
 *  found above the current cell will be used.  If there is no ceiling above the cell, the ceiling will
 *  be the top of the model.  This function will also adjust all floor and ceiling values for all cells
 *  between the found floor and ceiling.
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] map The map's routing data
 * @param[in] actorSize The size of the actor along the X and Y axis in cell units
 * @param[in] x The x position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] y The y position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] z The z position in the routing arrays (0 - PATHFINDING_HEIGHT-1)
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 * @return The z value of the next cell to scan, usually the cell with the ceiling.
 * @sa Grid_RecalcRouting
 */
int RT_CheckCell (mapTiles_t *mapTiles, routing_t * map, const int actorSize, const int x, const int y, const int z, const char **list)
{
	/* Width of the box required to stand in a cell by an actor's torso.  */
	const float halfActorWidth = UNIT_SIZE * actorSize / 2 - WALL_SIZE - DIST_EPSILON;
	/* This is a template for the extents of the box used by an actor's legs. */
	const box_t legBox = {{-halfMicrostepSize, -halfMicrostepSize, 0},
							{ halfMicrostepSize,  halfMicrostepSize, QuantToModel(PATHFINDING_LEGROOMHEIGHT) - DIST_EPSILON * 2}};
	/* This is a template for the extents of the box used by an actor's torso. */
	const box_t torsoBox = {{-halfActorWidth, -halfActorWidth, QuantToModel(PATHFINDING_LEGROOMHEIGHT)},
							{ halfActorWidth,  halfActorWidth, QuantToModel(PATHFINDING_MIN_OPENING) - DIST_EPSILON * 2}};
	/* This is a template for the ceiling trace after an actor's torso space has been found. */
	const box_t ceilBox = {{-halfActorWidth, -halfActorWidth, 0},
							{ halfActorWidth,  halfActorWidth, 0}};

	vec3_t start, end; /* Start and end of the downward traces. */
	vec3_t tstart, tend; /* Start and end of the upward traces. */
	box_t box; /* Holds the exact bounds to be traced for legs and torso. */
	pos3_t pos;
	float bottom, top; /* Floor and ceiling model distances from the cell base. (in mapunits) */
#ifdef DEBUG
	float initial; /* Cell floor and ceiling z coordinate. */
#endif
	int bottomQ, topQ; /* The floor and ceiling in QUANTs */
	int i;
	int fz, cz; /* Floor and ceiling Z cell coordinates */
	trace_t tr;

	assert(actorSize > ACTOR_SIZE_INVALID && actorSize <= ACTOR_MAX_SIZE);
	assert(map);
	/* x and y cannot exceed PATHFINDING_WIDTH - actorSize */
	assert((x >= 0) && (x <= PATHFINDING_WIDTH - actorSize));
	assert((y >= 0) && (y <= PATHFINDING_WIDTH - actorSize));
	assert(z < PATHFINDING_HEIGHT);

	/* calculate tracing coordinates */
	VectorSet(pos, x, y, z);
	SizedPosToVec(pos, actorSize, end); /* end is now at the center of the cells the actor occupies. */

	/* prepare fall down check */
	VectorCopy(end, start);
	/*
	 * Adjust these points so that start to end is from the top of the cell to the bottom of the model.
	 */
#ifdef DEBUG
	initial = start[2] + UNIT_HEIGHT / 2; /* This is the top-most starting point in this cell. */
#endif
	start[2] += UNIT_HEIGHT / 2 - QUANT; /* This one QUANT unit below initial. */
	end[2] = -UNIT_HEIGHT * 2; /* To the bottom of the model! (Plus some for good measure) */

	/*
	 * Trace for a floor.  Steps:
	 * 1. Start at the top of the designated cell and scan toward the model's base.
	 * 2. If we do not find a brush, then this cell is bottomless and not enterable.
	 * 3. We have found an upward facing brush.  Scan up PATHFINDING_LEGROOMHEIGHT height.
	 * 4. If we find anything, then this space is too small of an opening.  Restart just below our current floor.
	 * 5. Trace up towards the model ceiling with a box as large as the actor.  The first obstruction encountered
	 *      marks the ceiling.  If there are no obstructions, the model ceiling is the ceiling.
	 * 6. If the opening between the floor and the ceiling is not at least PATHFINDING_MIN_OPENING tall, then
	 *      restart below the current floor.
	 */
	while (qtrue) { /* Loop forever, we will exit if we hit the model bottom or find a valid floor. */
		if (debugTrace)
			Com_Printf("[(%i, %i, %i, %i)]Casting floor (%f, %f, %f) to (%f, %f, %f)\n",
				x, y, z, actorSize, start[0], start[1], start[2], end[0], end[1], end[2]);

		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, start, end, &footBox, list);
		if (tr.fraction >= 1.0) {
			/* There is no brush underneath this starting point. */
			if (debugTrace)
				Com_Printf("Reached bottom of map.  No floor in cell(s). %f\n", tr.endpos[2]);
			/* Mark all cells to the model base as filled. */
			for (i = z; i >= 0 ; i--) {
				/* no floor in this cell, it is bottomless! */
				RT_FLOOR(map, actorSize, x, y, i) = -1 - i * CELL_HEIGHT; /* There is no floor in this cell, place it at -1 below the model. */
				RT_CEILING(map, actorSize, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
			}
			/* return 0 to indicate we just scanned the model bottom. */
			return 0;
		}

		/* We have hit a brush that faces up and can be stood on. Look for a ceiling. */
		bottom = tr.endpos[2];  /* record the floor position. */

#ifdef DEBUG
		assert(initial > bottom);
#endif

		if (debugTrace)
			Com_Printf("Potential floor found at %f.\n", bottom);

		/* Record the hit position in tstart for later use. */
		VectorCopy(tr.endpos, tstart);

		/* Prep the start and end of the "leg room" test. */
		VectorAdd(tstart, legBox.mins, box.mins); /* Now bmins has the lower required foot space extent */
		VectorAdd(tstart, legBox.maxs, box.maxs); /* Now bmaxs has the upper required foot space extent */

		if (debugTrace)
			Com_Printf("    Casting leg room (%f, %f, %f) to (%f, %f, %f)\n",
				box.mins[0], box.mins[1], box.mins[2], box.maxs[0], box.maxs[1], box.maxs[2]);
		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, vec3_origin, vec3_origin, &box, list);
		if (tr.fraction < 1.0) {
			if (debugTrace)
				Com_Printf("Cannot use found surface- leg obstruction found.\n");
			/*
			 * There is a premature obstruction.  We can't use this as a floor.
			 * Check under start.  We need to have at least the minimum amount of clearance from our ceiling,
			 * So start at that point.
			 */
			start[2] = bottom - QuantToModel(PATHFINDING_MIN_OPENING);
			/* Check in case we are trying to scan too close to the bottom of the model. */
			if (start[2] <= QuantToModel(PATHFINDING_MIN_OPENING)) {
				/* There is no brush underneath this starting point. */
				if (debugTrace)
					Com_Printf("Reached bottom of map.  No floor in cell(s).\n");
				/* Mark all cells to the model base as filled. */
				for (i = z; i >= 0 ; i--) {
					/* no floor in this cell, it is bottomless! */
					RT_FLOOR(map, actorSize, x, y, i) = CELL_HEIGHT; /* There is no floor in this cell. */
					RT_CEILING(map, actorSize, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
				}
				/* return 0 to indicate we just scanned the model bottom. */
				return 0;
			}
			/* Restart */
			continue;
		}

		/* Prep the start and end of the "torso room" test. */
		VectorAdd(tstart, torsoBox.mins, box.mins); /* Now bmins has the lower required torso space extent */
		VectorAdd(tstart, torsoBox.maxs, box.maxs); /* Now bmaxs has the upper required torso space extent */

		if (debugTrace)
			Com_Printf("    Casting torso room (%f, %f, %f) to (%f, %f, %f)\n",
				box.mins[0], box.mins[1], box.mins[2], box.maxs[0], box.maxs[1], box.maxs[2]);
		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, vec3_origin, vec3_origin, &box, list);
		if (tr.fraction < 1.0) {
			if (debugTrace)
				Com_Printf("Cannot use found surface- torso obstruction found.\n");
			/*
			 * There is a premature obstruction.  We can't use this as a floor.
			 * Check under start.  We need to have at least the minimum amount of clearance from our ceiling,
			 * So start at that point.
			 */
			start[2] = bottom - QuantToModel(PATHFINDING_MIN_OPENING);
			/* Check in case we are trying to scan too close to the bottom of the model. */
			if (start[2] <= QuantToModel(PATHFINDING_MIN_OPENING)) {
				/* There is no brush underneath this starting point. */
				if (debugTrace)
					Com_Printf("Reached bottom of map.  No floor in cell(s).\n");
				/* Mark all cells to the model base as filled. */
				for (i = z; i >= 0 ; i--) {
					/* no floor in this cell, it is bottomless! */
					RT_FLOOR(map, actorSize, x, y, i) = CELL_HEIGHT; /* There is no floor in this cell. */
					RT_CEILING(map, actorSize, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
				}
				/* return 0 to indicate we just scanned the model bottom. */
				return 0;
			}
			/* Restart */
			continue;
		}

		/*
		 * If we are here, then the immediate floor is unobstructed MIN_OPENING units high.
		 * This is a valid floor.  Find the actual ceiling.
		 */

		tstart[2] = box.maxs[2]; /* The box trace for height starts at the top of the last trace. */
		VectorCopy(tstart, tend);
		tend[2] = PATHFINDING_HEIGHT * UNIT_HEIGHT; /* tend now reaches the model ceiling. */

		if (debugTrace)
			Com_Printf("    Casting ceiling (%f, %f, %f) to (%f, %f, %f)\n",
				tstart[0], tstart[1], tstart[2], tend[0], tend[1], tend[2]);

		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, tstart, tend, &ceilBox, list);

		/* We found the ceiling. */
		top = tr.endpos[2];

		/*
		 * There is one last possibility:
		 * If our found ceiling is above the cell we started the scan in, then we may have scanned up through another
		 * floor (one sided brush).  If this is the case, we set the ceiling to QUANT below the floor of the above
		 * ceiling if it is lower than our found ceiling.
		 */
		if (tr.endpos[2] > (z + 1) * UNIT_HEIGHT)
			top = min(tr.endpos[2], (z + 1) * UNIT_HEIGHT + QuantToModel(RT_FLOOR(map, actorSize, x, y, z + 1) - 1));

		/* We found the ceiling. */
		top = tr.endpos[2];

		/* exit the infinite while loop */
		break;
	}

	assert(bottom <= top);

	/* top and bottom are absolute model heights.  Find the actual cell z coordinates for these heights.
	 * ...but before rounding, give back the DIST_EPSILON that was added by the trace.
	 * Actually, we have to give back two DIST_EPSILON to prevent rounding issues */
	bottom -= 2 * DIST_EPSILON;
	top += 2 * DIST_EPSILON;
	bottomQ = ModelFloorToQuant(bottom); /* Convert to QUANT units to ensure the floor is rounded up to the correct value. */
	topQ = ModelCeilingToQuant(top); /* Convert to QUANT units to ensure the floor is rounded down to the correct value. */
	fz = floor(bottomQ / CELL_HEIGHT); /* Ensure we round down to get the bottom-most affected cell */
	/** @note Remember that ceiling values of 1-16 belong to a cell.  We need to adjust topQ by 1 to round to the correct z value. */
	cz = min(z, (int)(floor((topQ - 1) / CELL_HEIGHT))); /* Use the lower of z or the calculated ceiling */

	assert(fz <= cz);

	if (debugTrace)
		Com_Printf("Valid ceiling found, bottom=%f, top=%f, fz=%i, cz=%i.\n", bottom, top, fz, cz);

	/* Last, update the floors and ceilings of cells from (x, y, fz) to (x, y, cz) */
	for (i = fz; i <= cz; i++) {
		/* Round up floor to keep feet out of model. */
		RT_FLOOR(map, actorSize, x, y, i) = bottomQ - i * CELL_HEIGHT;
		/* Round down ceiling to heep head out of model.  Also offset by floor and max at 255. */
		RT_CEILING(map, actorSize, x, y, i) = topQ - i * CELL_HEIGHT;
		if (debugTrace) {
			Com_Printf("floor(%i, %i, %i, %i)=%i.\n", x, y, i, actorSize, RT_FLOOR(map, actorSize, x, y, i));
			Com_Printf("ceil(%i, %i, %i, %i)=%i.\n", x, y, i, actorSize, RT_CEILING(map, actorSize, x, y, i));
		}
	}

	/* Also, update the floors of any filled cells immediately above the ceiling up to our original cell. */
	for (i = cz + 1; i <= z; i++) {
		RT_FLOOR(map, actorSize, x, y, i) = CELL_HEIGHT; /* There is no floor in this cell. */
		RT_CEILING(map, actorSize, x, y, i) = 0; /* There is no ceiling, the true indicator of a filled cell. */
		if (debugTrace) {
			Com_Printf("floor(%i, %i, %i)=%i.\n", x, y, i, RT_FLOOR(map, actorSize, x, y, i));
			Com_Printf("ceil(%i, %i, %i)=%i.\n", x, y, i, RT_CEILING(map, actorSize, x, y, i));
		}
	}

	/* Return the lowest z coordinate that we updated floors for. */
	return fz;
}


/**
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] dir Direction of movement
 * @param[in] x Starting x coordinate
 * @param[in] y Starting y coordinate
 * @param[in] z Starting z coordinate
 * @param[in] openingSize Absolute height in QUANT units of the opening.
 * @param[in] openingBase Absolute height in QUANT units of the bottom of the opening.
 * @param[in] stepup Required stepup to travel in this direction.
 */
static int RT_FillPassageData (RT_data_t *rtd, const int dir, const int  x, const int y, const int z, const int openingSize, const int openingBase, const int stepup)
{
	const int openingTop = openingBase + openingSize;
	int fz, cz; /**< Floor and ceiling Z cell coordinates */
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
	cz = min(PATHFINDING_HEIGHT - 1, ceil((float)openingTop / CELL_HEIGHT) - 1);

	/* last chance- if cz < z, then bail (and there is an error with the ceiling data somewhere */
	if (cz < z) {
		/* We can't go this way. */
		RT_ConnSetNoGo(rtd, x, y, z, dir);
		if (debugTrace)
			Com_Printf("Passage found but below current cell, opening_base=%i, opening_top=%i, z = %i, cz = %i.\n", openingBase, openingTop, z, cz);
		return z;
	}

	if (debugTrace)
		Com_Printf("Passage found, opening_base=%i, opening_size=%i, opening_top=%i, stepup=%i. (%i to %i)\n", openingBase, openingSize, openingTop, stepup, fz, cz);

	assert(fz <= z && z <= cz);

	/* Last, update the routes of cells from (x, y, fz) to (x, y, cz) for direction dir */
	for (i = fz; i <= cz; i++) {
		int oh;
		RT_CONN_TEST(rtd->map, rtd->actorSize, x, y, i, dir);
		/* Offset from the floor or the bottom of the current cell, whichever is higher. */
		oh = openingTop - max(openingBase, i * CELL_HEIGHT);
		/* Only if > 0 */
		assert (oh >= 0);
		RT_ConnSet(rtd, x, y, i, dir, oh);
		/* The stepup is 0 for all cells that are not at the floor. */
		RT_StepupSet(rtd, x, y, i, dir, 0);
		if (debugTrace) {
			Com_Printf("RT_CONN for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, i, rtd->actorSize, dir, RT_CONN(rtd->map, rtd->actorSize, x, y, i, dir));
		}
	}

	RT_StepupSet(rtd, x, y, z, dir, stepup);
	if (debugTrace) {
		Com_Printf("Final RT_STEPUP for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, z, rtd->actorSize, dir, stepup);
	}

	/*
	 * Return the highest z coordinate scanned- cz if fz==cz, z==cz, or the floor in cz is negative.
	 * Otherwise cz - 1 to recheck cz in case there is a floor in cz with its own ceiling.
	 */
	if (fz == cz || z == cz || RT_FLOOR(rtd->map, rtd->actorSize, x, y, cz) < 0)
		return cz;
	return cz - 1;
}

/**
 * @brief Helper function to trace for walls
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] start The starting point of the trace, at the FLOOR'S CENTER.
 * @param[in] end The end point of the trace, centered x and y at the destination but at the same height as start.
 * @param[in] hi The upper height ABOVE THE FLOOR of the bounding box.
 * @param[in] lo The lower height ABOVE THE FLOOR of the bounding box.
 */
static trace_t RT_ObstructedTrace (RT_data_t *rtd, const vec3_t start, const vec3_t end, int hi, int lo)
{
	box_t box; /**< Tracing box extents */
	const float halfActorWidth = UNIT_SIZE * rtd->actorSize / 2 - WALL_SIZE - DIST_EPSILON;

	/* Configure the box trace extents. The box is relative to the original floor. */
	VectorSet(box.maxs, halfActorWidth, halfActorWidth, QuantToModel(hi) - DIST_EPSILON);
	VectorSet(box.mins, -halfActorWidth, -halfActorWidth, QuantToModel(lo)  + DIST_EPSILON);

	/* perform the trace, then return true if the trace was obstructed. */
	return RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, start, end, &box, rtd->list);
}


/**
 * @brief Performs a trace to find the floor of a passage a fraction of the way from start to end.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] start The starting coordinate to search for a floor from.
 * @param[in] end The starting coordinate to search for a floor from.
 * @param[in] frac The fraction of the distance traveled from start to end, using (0.0 to 1.0).
 * @param[in] startingHeight The starting height for this upward trace.
 * @return The absolute height of the found floor in QUANT units.
 */
static int RT_FindOpeningFloorFrac (RT_data_t *rtd, const vec3_t start, const vec3_t end, const float frac, const int startingHeight)
{
	vec3_t mstart, mend;	/**< Midpoint line to trace across */	/**< Tracing box extents */
	trace_t tr;
	const box_t* box = (rtd->actorSize == ACTOR_SIZE_NORMAL ? &actor1x1Box : &actor2x2Box);

	/* Position mstart and mend at the fraction point */
	VectorInterpolation(start, end, frac, mstart);
	VectorCopy(mstart, mend);
	mstart[2] = QuantToModel(startingHeight) + (QUANT / 2); /* Set at the starting height, plus a little more to keep us off a potential surface. */
	mend[2] = -QUANT;  /* Set below the model. */

	tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, mstart, mend, box, rtd->list);

	if (debugTrace)
		Com_Printf("Brush found at %f.\n", tr.endpos[2]);

	/* OK, now we have the floor height value in tr.endpos[2].
	 * Divide by QUANT and round up.
	 */
	return ModelFloorToQuant(tr.endpos[2] - DIST_EPSILON);
}


/**
 * @brief Performs a trace to find the ceiling of a passage a fraction of the way from start to end.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] start The starting coordinate to search for a ceiling from.
 * @param[in] end The starting coordinate to search for a ceiling from.
 * @param[in] frac The fraction of the distance traveled from start to end, using (0.0 to 1.0).
 * @param[in] startingHeight The starting height for this upward trace.
 * @return The absolute height of the found ceiling in QUANT units.
 */
static int RT_FindOpeningCeilingFrac (RT_data_t *rtd, const vec3_t start, const vec3_t end, const float frac, const int startingHeight)
{
	vec3_t mstart, mend;	/**< Midpoint line to trace across */
	trace_t tr;
	const box_t* box = (rtd->actorSize == ACTOR_SIZE_NORMAL ? &actor1x1Box : &actor2x2Box);	/**< Tracing box extents */

	/* Position mstart and mend at the midpoint */
	VectorInterpolation(start, end, frac, mstart);
	VectorCopy(mstart, mend);
	mstart[2] = QuantToModel(startingHeight) - (QUANT / 2); /* Set at the starting height, minus a little more to keep us off a potential surface. */
	mend[2] = UNIT_HEIGHT * PATHFINDING_HEIGHT + QUANT;  /* Set above the model. */

	tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, mstart, mend, box, rtd->list);

	if (debugTrace)
		Com_Printf("Brush found at %f.\n", tr.endpos[2]);

	/* OK, now we have the floor height value in tr.endpos[2].
	 * Divide by QUANT and round down. */
	return ModelCeilingToQuant(tr.endpos[2] + DIST_EPSILON);
}


/**
 * @brief Performs traces to find the approximate floor of a passage.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] start The starting coordinate to search for a floor from.
 * @param[in] end The starting coordinate to search for a floor from.
 * @param[in] startingHeight The starting height for this downward trace.
 * @param[in] floorLimit The lowest limit of the found floor.
 * @return The absolute height of the found floor in QUANT units.
 */
static int RT_FindOpeningFloor (RT_data_t *rtd, const vec3_t start, const vec3_t end, const int startingHeight, const int floorLimit)
{
	/* Look for additional space below init_bottom, down to lowest_bottom. */
	int midfloor;

	if (start[0] == end[0] || start[1] == end[1]) {
		/* For orthogonal dirs, find the height at the midpoint. */
		midfloor = RT_FindOpeningFloorFrac(rtd, start, end, 0.5, startingHeight);
		if (debugTrace)
			Com_Printf("midfloor:%i.\n", midfloor);
	} else {
		int midfloor2;

		/* If this is diagonal, trace the 1/3 and 2/3 points instead. */
		/* 1/3 point */
		midfloor = RT_FindOpeningFloorFrac(rtd, start, end, 0.33, startingHeight);
		if (debugTrace)
			Com_Printf("1/3floor:%i.\n", midfloor);

		/* 2/3 point */
		midfloor2 = RT_FindOpeningFloorFrac(rtd, start, end, 0.66, startingHeight);
		if (debugTrace)
			Com_Printf("2/3floor:%i.\n", midfloor2);
		midfloor = max(midfloor, midfloor2);
	}

	/* return the highest floor. */
	return max(floorLimit, midfloor);
}


/**
 * @brief Performs traces to find the approximate ceiling of a passage.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] start The starting coordinate to search for a ceiling from.
 * @param[in] end The starting coordinate to search for a ceiling from.
 * @param[in] startingHeight The starting height for this upward trace.
 * @param[in] ceilLimit The highest the ceiling may be.
 * @return The absolute height of the found ceiling in QUANT units.
 */
static int RT_FindOpeningCeiling (RT_data_t *rtd, const vec3_t start, const vec3_t end, const int startingHeight, const int ceilLimit)
{
	int midceil;

	if (start[0] == end[0] || start[1] == end[1]) {
		/* For orthogonal dirs, find the height at the midpoint. */
		midceil = RT_FindOpeningCeilingFrac(rtd, start, end, 0.5, startingHeight);
		if (debugTrace)
			Com_Printf("midceil:%i.\n", midceil);
	} else {
		int midceil2;

		/* If this is diagonal, trace the 1/3 and 2/3 points instead. */
		/* 1/3 point */
		midceil = RT_FindOpeningCeilingFrac(rtd, start, end, 0.33, startingHeight);
		if (debugTrace)
			Com_Printf("1/3ceil:%i.\n", midceil);

		/* 2/3 point */
		midceil2 = RT_FindOpeningCeilingFrac(rtd, start, end, 0.66, startingHeight);
		if (debugTrace)
			Com_Printf("2/3ceil:%i.\n", midceil2);
		midceil = min(midceil, midceil2);
	}

	/* return the lowest ceiling. */
	return min(ceilLimit, midceil);
}


static int RT_CalcNewZ (RT_data_t *rtd, const int ax, const int ay, const int top, const int hi)
{
	int temp_z, adj_lo;

	temp_z = min(floor((hi - 1) / CELL_HEIGHT), PATHFINDING_HEIGHT - 1);
	adj_lo = RT_FLOOR(rtd->map, rtd->actorSize, ax, ay, temp_z) + temp_z * CELL_HEIGHT;
	if (adj_lo > hi) {
		temp_z--;
		adj_lo = RT_FLOOR(rtd->map, rtd->actorSize, ax, ay, temp_z) + temp_z * CELL_HEIGHT;
	}
	/**
	 * @note Return a value only if there is a floor for the adjacent cell.
	 * Also the found adjacent lo must be at lease MIN_OPENING-MIN_STEPUP below
	 * the top.
	 */
	if (adj_lo >= 0 && top - adj_lo >= PATHFINDING_MIN_OPENING - PATHFINDING_MIN_STEPUP) {
		if (debugTrace)
			Com_Printf("Found floor in destination cell: %i (%i).\n", adj_lo, temp_z);
		return floor(adj_lo / CELL_HEIGHT);
	}
	if (debugTrace)
		Com_Printf("Skipping found floor in destination cell- not enough opening: %i (%i).\n", adj_lo, temp_z);

	return RT_NO_OPENING;
}


/**
 * @brief Performs actual trace to find a passage between two points given an upper and lower bound.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] start Starting trace coordinate
 * @param[in] end Ending trace coordinate
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] bottom Actual height of the starting floor.
 * @param[in] top Actual height of the starting ceiling.
 * @param[in] lo Actual height of the bottom of the slice trace.
 * @param[in] hi Actual height of the top of the slice trace.
 * @param[out] lo_val Actual height of the bottom of the found passage.
 * @param[out] hi_val Actual height of the top of the found passage.
 * @return The new z value of the actor after traveling in this direction from the starting location.
 */
static int RT_TraceOpening (RT_data_t *rtd, const vec3_t start, const vec3_t end, const int ax, const int ay, const int bottom, const int top, int lo, int hi, int *lo_val, int *hi_val)
{
	trace_t tr = RT_ObstructedTrace(rtd, start, end, hi, lo);
	if (tr.fraction >= 1.0) {
		lo = RT_FindOpeningFloor(rtd, start, end, lo, bottom);
		hi = RT_FindOpeningCeiling(rtd, start, end, hi, top);
		if (hi - lo >= PATHFINDING_MIN_OPENING) {
			int temp_z;
			if (lo == -1) {
				if (debugTrace)
					Com_Printf("Bailing- no floor in destination cell.\n");
				*lo_val = *hi_val = 0;
				return RT_NO_OPENING;
			}
			/* This opening works, use it! */
			*lo_val = lo;
			*hi_val = hi;
			/* Find the floor for the highest adjacent cell in this passage. */
			temp_z = RT_CalcNewZ(rtd, ax, ay, top, hi);
			if (temp_z != RT_NO_OPENING)
				return temp_z;
		}
	}
	*lo_val = *hi_val = hi;
	return RT_NO_OPENING;
}


/**
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] from Starting place
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] bottom Actual height of the starting floor.
 * @param[in] top Actual height of the starting ceiling.
 * @param[out] lo_val Actual height of the bottom of the found passage.
 * @param[out] hi_val Actual height of the top of the found passage.
 * @return The new z value of the actor after traveling in this direction from the starting location.
 */
static int RT_FindOpening (RT_data_t *rtd, const place_t* from, const int ax, const int ay, const int bottom, const int top, int *lo_val, int *hi_val)
{
	vec3_t start, end;
	pos3_t pos;
	int temp_z;

	const int endfloor = RT_FLOOR(rtd->map, rtd->actorSize, ax, ay, from->cell[2]) + from->cell[2] * CELL_HEIGHT;
	const int hifloor = max(endfloor, bottom);

	if (debugTrace)
		Com_Printf("ef:%i t:%i b:%i\n", endfloor, top, bottom);

	if (bottom == -1) {
		if (debugTrace)
			Com_Printf("Bailing- no floor in current cell.\n");
		*lo_val = *hi_val = 0;
		return RT_NO_OPENING;
	}

	/* Initialize the starting vector */
	SizedPosToVec(from->cell, rtd->actorSize, start);

	/* Initialize the ending vector */
	VectorSet(pos, ax, ay, from->cell[2]);
	SizedPosToVec(pos, rtd->actorSize, end);

	/* Initialize the z component of both vectors */
	start[2] = end[2] = 0;

	/* shortcut: if both ceilings are the sky, we can check for walls
	 * AND determine the bottom of the passage in just one trace */
	if (from->ceiling >= PATHFINDING_HEIGHT * CELL_HEIGHT
	 && from->cell[2] * CELL_HEIGHT + RT_CEILING(rtd->map, rtd->actorSize, ax, ay, from->cell[2]) >= PATHFINDING_HEIGHT * CELL_HEIGHT) {
		vec3_t sky, earth;
		const box_t* box = (rtd->actorSize == ACTOR_SIZE_NORMAL ? &actor1x1Box : &actor2x2Box);
		trace_t tr;
		int tempBottom;

		if (debugTrace)
			Com_Printf("Using sky trace.\n");

		VectorInterpolation(start, end, 0.5, sky);	/* center it halfway between the cells */
		VectorCopy(sky, earth);
		sky[2] = UNIT_HEIGHT * PATHFINDING_HEIGHT;  /* Set to top of model. */
		earth[2] = QuantToModel(bottom);

		tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, sky, earth, box, rtd->list);
		tempBottom = ModelFloorToQuant(tr.endpos[2]);
		if (tempBottom <= bottom + PATHFINDING_MIN_STEPUP) {
			const int hi = bottom + PATHFINDING_MIN_OPENING;
			if (debugTrace)
				Com_Printf("Found opening with sky trace.\n");
			*lo_val = tempBottom;
			*hi_val = CELL_HEIGHT * PATHFINDING_HEIGHT;
			return RT_CalcNewZ(rtd, ax, ay, top, hi);
		}
		if (debugTrace)
			Com_Printf("Failed sky trace.\n");
	}
	/* Warning: never try to make this an 'else if', or 'arched entry' situations will fail !! */

	/* Now calculate the "guaranteed" opening, if any. If the opening from
	 * the floor to the ceiling is not too tall, there must be a section that
	 * will always be vacant if there is a usable passage of any size and at
	 * any height. */
	if (top - bottom < PATHFINDING_MIN_OPENING * 2) {
		const int lo = top - PATHFINDING_MIN_OPENING;
		const int hi = bottom + PATHFINDING_MIN_OPENING;
		if (debugTrace)
			Com_Printf("Tracing closed space from %i to %i.\n", bottom, top);
		temp_z = RT_TraceOpening(rtd, start, end, ax, ay, hifloor, top, lo, hi, lo_val, hi_val);
	} else {
		/* There is no "guaranteed" opening, brute force search. */
		int lo = bottom;
		temp_z = 0;
		while (lo <= top - PATHFINDING_MIN_OPENING) {
			/* Check for a 1 QUANT opening. */
			if (debugTrace)
				Com_Printf("Tracing open space from %i.\n", lo);
			temp_z = RT_TraceOpening(rtd, start, end, ax, ay, bottom, top, lo, lo + 1, lo_val, hi_val);
			if (temp_z != RT_NO_OPENING)
				break;
			/* Credit to Duke: We skip the minimum opening, as if there is a
			 * viable opening, even one slice above, that opening would be open. */
			lo = *hi_val + PATHFINDING_MIN_OPENING;
		}
	}
	return temp_z;
}


/**
 * @brief Performs small traces to find places when an actor can step up.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] from Starting place
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[in] az Ending z coordinate
 * @param[in] stairwaySituation whether we are standing in front of a stairway
 * @param[out] opening descriptor of the opening found, if any
 * @return The change in floor height in QUANT units because of the additional trace.
*/
static int RT_MicroTrace (RT_data_t *rtd, const place_t* from, const int ax, const int ay, const int az, const int stairwaySituation, opening_t* opening)
{
	/* OK, now we have a viable shot across.  Run microstep tests now. */
	/* Now calculate the stepup at the floor using microsteps. */
	int top = opening->base + opening->size;
	signed char bases[UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE + 1];
	float sx, sy, ex, ey;
	/* Shortcut the value of UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE. */
	const int steps = UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE;
	trace_t tr;
	int i, current_h, highest_h, highest_i = 0, skipped, newBottom;
	vec3_t start, end;
	pos3_t pos;
	int last_step;

	/* First prepare the two known end values. */
	bases[0] = from->floor;
	bases[steps] = last_step = max(0, RT_FLOOR(rtd->map, rtd->actorSize, ax, ay, az)) + az * CELL_HEIGHT;

	/* Initialize the starting vector */
	SizedPosToVec(from->cell, rtd->actorSize, start);

	/* Initialize the ending vector */
	VectorSet(pos, ax, ay, az);
	SizedPosToVec(pos, rtd->actorSize, end);

	/* Now prep the z values for start and end. */
	start[2] = QuantToModel(opening->base) + 1; /**< Just above the bottom of the found passage */
	end[2] = -QUANT;

	/* Memorize the start and end x,y points */
	sx = start[0];
	sy = start[1];
	ex = end[0];
	ey = end[1];

	newBottom = max(bases[0], bases[steps]);
	/* Now calculate the rest of the microheights. */
	for (i = 1; i < steps; i++) {
		start[0] = end[0] = sx + (ex - sx) * (i / (float)steps);
		start[1] = end[1] = sy + (ey - sy) * (i / (float)steps);

		/* perform the trace, then return true if the trace was obstructed. */
		tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, start, end, &footBox, rtd->list);
		if (tr.fraction >= 1.0) {
			bases[i] = -1;
		} else {
			bases[i] = ModelFloorToQuant(tr.endpos[2] - DIST_EPSILON);
			/* Walking through glass fix:
			 * It is possible to have an obstruction that can be skirted around diagonally
			 * because the microtraces are so tiny.  But, we have a full size trace in opening->base
			 * that apporoximates where legroom ends.  If the found floor of the middle microtrace is
			 * too low, then set it to the worst case scenario floor based on base->opening.
			 */
			if (i == floor(steps / 2.0) && bases[i] < opening->base - PATHFINDING_MIN_STEPUP) {
				if (debugTrace)
					Com_Printf("Adjusting middle trace- the known base is too high. \n");
				bases[i] = opening->base - PATHFINDING_MIN_STEPUP;
			}
		}

		if (debugTrace)
			Com_Printf("Microstep %i from (%f, %f, %f) to (%f, %f, %f) = %i [%f]\n",
				i, start[0], start[1], start[2], end[0], end[1], end[2], bases[i], tr.endpos[2]);\

		newBottom = max(newBottom, bases[i]);
	}

	if (debugTrace)
		Com_Printf("z:%i az:%i bottom:%i new_bottom:%i top:%i bases[0]:%i bases[%i]:%i\n", from->cell[2], az, opening->base, newBottom, top, bases[0], steps, bases[steps]);


	/** @note This for loop is bi-directional: i may be decremented to retrace prior steps. */
	/* Now find the maximum stepup moving from (x, y) to (ax, ay). */
	/* Initialize stepup. */
	current_h = bases[0];
	highest_h = -1;
	highest_i = 1;
	opening->stepup = 0; /**<  Was originally -CELL_HEIGHT, but stepup is needed to go UP, not down. */
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
			opening->stepup = max(opening->stepup, bases[i] - current_h);
			current_h = bases[i];
			highest_h = -2;
			highest_i = i + 1;
			skipped = 0;
			if (debugTrace)
				Com_Printf(" Advancing b:%i stepup:%i\n", bases[i], opening->stepup);
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

	/** @note This for loop is bi-directional: i may be decremented to retrace prior steps. */
	/* Now find the maximum stepup moving from (x, y) to (ax, ay). */
	/* Initialize stepup. */
	current_h = bases[steps];
	highest_h = -1;
	highest_i = steps - 1; /**< Note that for this part of the code, this is the LOWEST i. */
	opening->invstepup = 0; /**<  Was originally -CELL_HEIGHT, but stepup is needed to go UP, not down. */
	skipped = 0;
	for (i = steps - 1; i >= 0; i--) {
		if (debugTrace)
			Com_Printf("Tracing backward i:%i h:%i\n", i, current_h);
		/* If there is a rise, use it. */
		if (bases[i] >= current_h || ++skipped > PATHFINDING_MICROSTEP_SKIP) {
			if (skipped == PATHFINDING_MICROSTEP_SKIP) {
				i = highest_i;
				if (debugTrace)
					Com_Printf(" Skipped too many steps, reverting to i:%i\n", i);
			}
			opening->invstepup = max(opening->invstepup, bases[i] - current_h);
			current_h = bases[i];
			highest_h = -2;
			highest_i = i - 1;
			skipped = 0;
			if (debugTrace)
				Com_Printf(" Advancing b:%i stepup:%i\n", bases[i], opening->invstepup);
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
			if (i == 0) {
				skipped = PATHFINDING_MICROSTEP_SKIP;
				i = highest_i + 1;
				if (debugTrace)
					Com_Printf(" Tripping skip counter to perform last tests.\n");
			}
		}
	}

	if (stairwaySituation) {
		const int middle = bases[4];	/* terrible hack by Duke. This relies on PATHFINDING_MICROSTEP_SIZE being set to 4 !! */

		if (stairwaySituation == 1) {		/* stepping up */
			if (bases[1] <= middle &&		/* if nothing in the 1st part of the passage is higher than what's at the border */
				bases[2] <= middle &&
				bases[3] <= middle ) {
				if (debugTrace)
					Com_Printf("Addition granted by ugly stair hack-stepping up.\n");
				return opening->base - middle;
			}
		} else if (stairwaySituation == 2) {/* stepping down */
			if (bases[5] <= middle &&		/* same for the 2nd part of the passage */
				bases[6] <= middle &&
				bases[7] <= middle )
				if (debugTrace)
					Com_Printf("Addition granted by ugly stair hack-stepping down.\n");
				return opening->base - middle;
		}
	}

	/* Return the confirmed passage opening. */
	return opening->base - newBottom;
}


/**
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] from Starting place
 * @param[in] to Ending place
 * @param[out] opening descriptor of the opening found, if any
 * @return The size in QUANT units of the detected opening.
 */
static int RT_TraceOnePassage (RT_data_t *rtd, const place_t* from, const place_t* to, opening_t* opening)
{
	int hi; /**< absolute ceiling of the passage found. */
	const int z = from->cell[2];
	int az; /**< z height of the actor after moving in this direction. */
	const int lower = max(from->floor, to->floor);
	const int upper = min(from->ceiling, to->ceiling);
	const int ax = to->cell[0];
	const int ay = to->cell[1];

	RT_FindOpening(rtd, from, ax, ay, lower, upper, &opening->base, &hi);
	/* calc opening found so far and set stepup */
	opening->size = hi - opening->base;
	az = to->floorZ;

	/* We subtract MIN_STEPUP because that is foot space-
	 * the opening there only needs to be the microtrace
	 * wide and not the usual dimensions.
	 */
	if (az != RT_NO_OPENING && opening->size >= PATHFINDING_MIN_OPENING - PATHFINDING_MIN_STEPUP) {
		const int srcFloor = from->floor;
		const int dstFloor = RT_FLOOR(rtd->map, rtd->actorSize, ax, ay, az) + az * CELL_HEIGHT;
		/* if we already have enough headroom, try to skip microtracing */
		if (opening->size < ACTOR_MAX_HEIGHT
			|| abs(srcFloor - opening->base) > PATHFINDING_MIN_STEPUP
			|| abs(dstFloor - opening->base) > PATHFINDING_MIN_STEPUP) {
			int stairway = RT_PlaceIsShifted(from, to);
			/* This returns the total opening height, as the
			 * microtrace may reveal more passage height from the foot space. */
			const int bonusSize = RT_MicroTrace(rtd, from, ax, ay, az, stairway, opening);
			opening->base -= bonusSize;
			opening->size = hi - opening->base;	/* re-calculate */
		} else {
			/* Skipping microtracing, just set the stepup values. */
			opening->stepup = max(0, opening->base - srcFloor);
			opening->invstepup = max(0, opening->base - dstFloor);
		}

		/* Now place an upper bound on stepup */
		if (opening->stepup > PATHFINDING_MAX_STEPUP) {
			opening->stepup = PATHFINDING_NO_STEPUP;
		} else {
			/* Add rise/fall bit as needed. */
			if (az < z && opening->invstepup <= PATHFINDING_MAX_STEPUP)
			/* BIG_STEPDOWN indicates 'walking down', don't set it if we're 'falling' */
				opening->stepup |= PATHFINDING_BIG_STEPDOWN;
			else if (az > z)
				opening->stepup |= PATHFINDING_BIG_STEPUP;
		}

		/* Now place an upper bound on stepup */
		if (opening->invstepup > PATHFINDING_MAX_STEPUP) {
			opening->invstepup = PATHFINDING_NO_STEPUP;
		} else {
			/* Add rise/fall bit as needed. */
			if (az > z)
				opening->invstepup |= PATHFINDING_BIG_STEPDOWN;
			else if (az < z)
				opening->invstepup |= PATHFINDING_BIG_STEPUP;
		}

		if (opening->size >= PATHFINDING_MIN_OPENING) {
			return opening->size;
		}
	}

	if (debugTrace)
		Com_Printf(" No opening found.\n");
	opening->stepup = PATHFINDING_NO_STEPUP;
	opening->invstepup = PATHFINDING_NO_STEPUP;
	return 0;
}

/**
 * @brief Performs traces to find a passage between two points.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] x Starting x coordinate
 * @param[in] y Starting y coordinate
 * @param[in] z Starting z coordinate
 * @param[in] ax Ending x coordinate
 * @param[in] ay Ending y coordinate
 * @param[out] opening descriptor of the opening found, if any
 */
static void RT_TracePassage (RT_data_t *rtd, const int x, const int y, const int z, const int ax, const int ay, opening_t* opening)
{
	int aboveCeil, lowCeil;
	/** we don't need the cell below the adjacent cell because we should have already checked it */
	place_t from, to, above;
	const place_t* placeToCheck = NULL;

	RT_PlaceInit(rtd->map, rtd->actorSize, &from, x, y, z);
	RT_PlaceInit(rtd->map, rtd->actorSize, &to, ax, ay, z);

	aboveCeil = (z < PATHFINDING_HEIGHT - 1) ? RT_CEILING(rtd->map, rtd->actorSize, ax, ay, z + 1) + (z + 1) * CELL_HEIGHT : to.ceiling;
	lowCeil = min(from.ceiling, (RT_CEILING(rtd->map, rtd->actorSize, ax, ay, z) == 0 || to.ceiling - from.floor < PATHFINDING_MIN_OPENING) ? aboveCeil : to.ceiling);

	/*
	 * First check the ceiling for the cell beneath the adjacent floor to see
	 * if there is a potential opening.  The difference between the
	 * ceiling and the floor is at least PATHFINDING_MIN_OPENING tall, then
	 * scan it to see if we can use it.  If we can, then one of two things
	 * will happen:
	 *  - The actual adjacent cell has no floor of its own, and we will walk
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
	if (RT_PlaceIsUsable(&to) && RT_PlaceDoesIntersectEnough(&from, &to)) {
		placeToCheck = &to;
	} else if (z < PATHFINDING_HEIGHT - 1) {
		RT_PlaceInit(rtd->map, rtd->actorSize, &above, ax, ay, z + 1);
		if (RT_PlaceIsUsable(&above) && RT_PlaceDoesIntersectEnough(&from, &above)) {
			placeToCheck = &above;
		}
	}
	if (!placeToCheck) {
		if (debugTrace)
			Com_Printf(" No opening found. c:%i lc:%i.\n", from.ceiling, lowCeil);
		/* If we got here, then there is no opening from floor to ceiling. */
		opening->stepup = PATHFINDING_NO_STEPUP;
		opening->invstepup = PATHFINDING_NO_STEPUP;
		opening->base = lowCeil;
		opening->size = 0;
		return;
	}

	/*
	 * Now that we got here, we know that either the opening between the
	 * ceiling below the adjacent cell and the current floor is too small or
	 * obstructed.  Try to move onto the adjacent floor.
	 */
	if (debugTrace)
		Com_Printf(" Testing up c:%i lc:%i.\n", from.ceiling, lowCeil);

	RT_TraceOnePassage(rtd, &from, placeToCheck, opening);
	if (opening->size < PATHFINDING_MIN_OPENING) {
		if (debugTrace)
			Com_Printf(" No opening found.\n");
		/* If we got here, then there is no useable opening from floor to ceiling. */
		opening->stepup = PATHFINDING_NO_STEPUP;
		opening->invstepup = PATHFINDING_NO_STEPUP;
		opening->base = lowCeil;
		opening->size = 0;
	}
}


/**
 * @brief Routing Function to update the connection between two fields
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] x The x position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] y The y position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] ax The x of the adjacent cell
 * @param[in] ay The y of the adjacent cell
 * @param[in] z The z position in the routing arrays (0 to PATHFINDING_HEIGHT - 1)
 * @param[in] dir The direction to test for a connection through
 */
static int RT_UpdateConnection (RT_data_t *rtd, const int x, const int y, const int ax, const int ay, const int z, const int dir)
{
	const int ceiling = RT_CEILING(rtd->map, rtd->actorSize, x, y, z);
	const int adjCeiling = RT_CEILING(rtd->map, rtd->actorSize, ax, ay, z);
	const int extAdjCeiling = (z < PATHFINDING_HEIGHT - 1) ? RT_CEILING(rtd->map, rtd->actorSize, ax, ay, z + 1) : adjCeiling;
	const int absCeiling = ceiling + z * CELL_HEIGHT;
	const int absAdjCeiling = adjCeiling + z * CELL_HEIGHT;
	const int absExtAdjCeiling = (z < PATHFINDING_HEIGHT - 1) ? adjCeiling + (z + 1) * CELL_HEIGHT : absCeiling;
	const int absFloor = RT_FLOOR(rtd->map, rtd->actorSize, x, y, z) + z * CELL_HEIGHT;
	const int absAdjFloor = RT_FLOOR(rtd->map, rtd->actorSize, ax, ay, z) + z * CELL_HEIGHT;
	opening_t opening;	/** the opening between the two cells */
	int new_z1, az = z;
#if RT_IS_BIDIRECTIONAL == 1
	int new_z2;
#endif

	if (debugTrace)
		Com_Printf("\n(%i, %i, %i) to (%i, %i, %i) as:%i\n", x, y, z, ax, ay, z, rtd->actorSize);

	/** test if the adjacent cell and the cell above it are blocked by a loaded model */
	if (adjCeiling == 0 && (extAdjCeiling == 0 || ceiling == 0)) {
		/* We can't go this way. */
		RT_ConnSetNoGo(rtd, x, y, z, dir);
#if RT_IS_BIDIRECTIONAL == 1
		RT_ConnSetNoGo(rtd, ax, ay, z, dir ^ 1);
#endif
		if (debugTrace)
			Com_Printf("Current cell filled. c:%i ac:%i\n", RT_CEILING(rtd->map, rtd->actorSize, x, y, z), RT_CEILING(rtd->map, rtd->actorSize, ax, ay, z));
		return z;
	}

#if RT_IS_BIDIRECTIONAL == 1
	/** In case the adjacent floor has no ceiling, swap the current and adjacent cells. */
	if (ceiling == 0 && adjCeiling != 0) {
		return RT_UpdateConnection(rtd->map, actorSize, ax, ay, x, y, z, dir ^ 1);
	}
#endif

	/**
	 * @note OK, simple test here.  We know both cells have a ceiling, so they are both open.
	 *  If the absolute ceiling of one is below the absolute floor of the other, then there is no intersection.
	 */
	if (absCeiling < absAdjFloor || absExtAdjCeiling < absFloor) {
		/* We can't go this way. */
		RT_ConnSetNoGo(rtd, x, y, z, dir);
#if RT_IS_BIDIRECTIONAL == 1
		RT_ConnSetNoGo(rtd, ax, ay, z, dir ^ 1);
#endif
		if (debugTrace)
			Com_Printf("Ceiling lower than floor. f:%i c:%i af:%i ac:%i\n", absFloor, absCeiling, absAdjFloor, absAdjCeiling);
		return z;
	}

	/** Find an opening. */
	RT_TracePassage(rtd, x, y, z, ax, ay, &opening);
	if (debugTrace) {
		Com_Printf("Final RT_STEPUP for (%i, %i, %i) as:%i dir:%i = %i\n", x, y, z, rtd->actorSize, dir, opening.stepup);
	}
	/** Apply the data to the routing table.
	 * We always call the fill function.  If the passage cannot be traveled, the
	 * function fills it in as unpassable. */
	new_z1 = RT_FillPassageData(rtd, dir, x, y, z, opening.size, opening.base, opening.stepup);

	if (opening.stepup & PATHFINDING_BIG_STEPUP) {
		/* ^ 1 reverses the direction of dir */
#if RT_IS_BIDIRECTIONAL == 1
		RT_ConnSetNoGo(rtd, ax, ay, z, dir ^ 1);
#endif
		az++;
	} else if (opening.stepup & PATHFINDING_BIG_STEPDOWN) {
		az--;
	}
#if RT_IS_BIDIRECTIONAL == 1
	new_z2 = RT_FillPassageData(rtd, dir ^ 1, ax, ay, az, opening.size, opening.base, opening.invstepup);
	if (new_z2 == az && az < z)
		new_z2++;
	return min(new_z1, new_z2);
#else
	return new_z1;
#endif
}


/**
 * @brief Routing Function to update the connection between two fields
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] map Routing table of the current loaded map
 * @param[in] actorSize The size of the actor, in units
 * @param[in] x The x position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] y The y position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] dir The direction to test for a connection through
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void RT_UpdateConnectionColumn (mapTiles_t *mapTiles, routing_t * map, const int actorSize, const int x, const int y, const int dir, const char **list)
{
	int z = 0; /**< The current z value that we are testing. */
	RT_data_t rtd;	/* the essential data passed down the calltree */

	/* get the neighbor cell's coordinates */
	const int ax = x + dvecs[dir][0];
	const int ay = y + dvecs[dir][1];

	assert(actorSize > ACTOR_SIZE_INVALID && actorSize <= ACTOR_MAX_SIZE);
	assert(map);
	assert((x >= 0) && (x <= PATHFINDING_WIDTH - actorSize));
	assert((y >= 0) && (y <= PATHFINDING_WIDTH - actorSize));

#ifdef DEBUG
	/** @todo remove me */
	/* just a place to place a breakpoint */
	if (x == 126 && y == 121 && dir == 3) {
		z = 7;
	}
#endif

	/* Ensure that the current coordinates are valid. */
	RT_CONN_TEST(map, actorSize, x, y, z, dir);

	/* Com_Printf("At (%i, %i, %i) looking in direction %i with size %i\n", x, y, z, dir, actorSize); */

	/* build the param list passed to most of the RT_* functions */
	rtd.mapTiles = mapTiles;
	rtd.map = map;
	rtd.actorSize = actorSize;
	rtd.list = list;

	/* if our destination cell is out of bounds, bail. */
	if (ax < 0 || ax > PATHFINDING_WIDTH - actorSize || ay < 0 || y > PATHFINDING_WIDTH - actorSize) {
		/* We can't go this way. */
		RT_ConnSetNoGo(&rtd, x, y, z, dir);
		/* There is only one entry here: There is no inverse cell to store data for. */
		if (debugTrace)
			Com_Printf("Destination cell non-existant.\n");
		return;
	}

	/* Ensure that the destination coordinates are valid. */
	RT_CONN_TEST(map, actorSize, ax, ay, z, dir);

	/* Main loop */
	for (z = 0; z < PATHFINDING_HEIGHT; z++) {
		/* The last z value processed by the tracing function.  */
		const int new_z = RT_UpdateConnection(&rtd, x, y, ax, ay, z, dir);
		assert(new_z >= z);
		z = new_z;
	}
}

void RT_WriteCSVFiles (const routing_t *map, const char* baseFilename, const ipos3_t mins, const ipos3_t maxs)
{
	char filename[MAX_OSPATH], ext[MAX_OSPATH];
	qFILE f;
	int i, x, y, z;

	/* An elevation files- dumps the floor and ceiling levels relative to each cell. */
	for (i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.elevation.csv", i);
		Com_DefaultExtension(filename, sizeof(filename), ext);
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
		Com_DefaultExtension(filename, sizeof(filename), ext);
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

#ifdef DEBUG
/**
 * @brief A debug function to be called from CL_DebugPath_f
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] map Routing table of the current loaded map
 * @param[in] actorSize The size of the actor, in units
 * @param[in] x The x position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] y The y position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] dir The direction to test for a connection through
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
int RT_DebugSpecial (mapTiles_t *mapTiles, routing_t * map, const int actorSize, const int x, const int y, const int dir, const char **list)
{
	int z = 0; /**< The current z value that we are testing. */
	int new_z; /**< The last z value processed by the tracing function.  */
	RT_data_t rtd;	/* the essential data passed down the calltree */

	/* get the neighbor cell's coordinates */
	const int ax = x + dvecs[dir][0];
	const int ay = y + dvecs[dir][1];

	/* build the param list passed to most of the RT_* functions */
	rtd.mapTiles = mapTiles;
	rtd.map = map;
	rtd.actorSize = actorSize;
	rtd.list = list;

	new_z = RT_UpdateConnection(&rtd, x, y, ax, ay, z, dir);
	return new_z;
}
#endif
