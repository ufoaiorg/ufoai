/**
 * @file
 * @brief grid pathfinding and routing
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "tracing.h"
#include "routing.h"
#include "common.h"

/*
===============================================================================
MAP TRACING DEBUGGING TABLES
===============================================================================
*/

bool debugTrace = false;

/*
==========================================================
  LOCAL CONSTANTS
==========================================================
*/

#define RT_NO_OPENING -1

/* Width of the box required to stand in a cell by an actor's feet.  */
#define halfMicrostepSize (PATHFINDING_MICROSTEP_SIZE / 2 - DIST_EPSILON)
/* This is a template for the extents of the box used by an actor's feet. */
static const AABB footBox(-halfMicrostepSize, -halfMicrostepSize, 0, halfMicrostepSize, halfMicrostepSize, 0);

/* Width of the box required to stand in a cell by an actor's torso.  */
#define half1x1Width (UNIT_SIZE * 1 / 2 - WALL_SIZE - DIST_EPSILON)
#define half2x2Width (UNIT_SIZE * 2 / 2 - WALL_SIZE - DIST_EPSILON)
/* These are templates for the extents of the box used by an actor's torso. */
static const AABB actor1x1Box(-half1x1Width, -half1x1Width, 0, half1x1Width, half1x1Width, 0);
static const AABB actor2x2Box(-half2x2Width, -half2x2Width, 0, half2x2Width, half2x2Width, 0);

/*
==========================================================
  LOCAL TYPES
==========================================================
*/
#if defined(COMPILE_MAP)
  #define RT_COMPLETEBOXTRACE_SIZE(mapTiles, b, list, lvl)		TR_SingleTileBoxTrace((mapTiles), Line(), (b), (lvl), MASK_NO_LIGHTCLIP, 0)
  #define RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, line, b, list)	TR_SingleTileBoxTrace((mapTiles), (line), (b), TRACE_ALL_LEVELS, MASK_IMPASSABLE, MASK_PASSABLE)

#elif defined(COMPILE_UFO)
  #define RT_COMPLETEBOXTRACE_SIZE(mapTiles, b, list, lvl)		CM_EntCompleteBoxTrace((mapTiles), Line(), (b), (lvl), MASK_NO_LIGHTCLIP, 0, (list))
  #define RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, line, b, list)	CM_EntCompleteBoxTrace((mapTiles), (line), (b), TRACE_ALL_LEVELS, MASK_IMPASSABLE, MASK_PASSABLE, (list))

#else
  #error Either COMPILE_MAP or COMPILE_UFO must be defined in order for tracing.c to work.
#endif

/**
 * @brief RT_data_s contains the essential data that is passed to most of the RT_* functions
 */
class RoutingData
{
public:
	mapTiles_t* mapTiles;
	Routing& routing;			/**< The routing tables */
	actorSizeEnum_t actorSize;	/**< The size of the actor, in cells */
	const char** list;			/**< The local models list */

	RoutingData (mapTiles_t* mapTiles, Routing& r, actorSizeEnum_t actorSize, const char** list);
};

RoutingData::RoutingData (mapTiles_t* mapTiles, Routing& r, actorSizeEnum_t actorSize, const char** list) : routing(r)
{
	this->mapTiles = mapTiles;
	this->actorSize = actorSize;
	this->list = list;
}

static inline void RT_StepupSet (RoutingData* rtd, const int x, const int y, const int z, const int dir, const int val)
{
	rtd->routing.setStepup(rtd->actorSize, x, y, z, dir, val);
}

static inline void RT_ConnSetNoGo (RoutingData* rtd, const int x, const int y, const int z, const int dir)
{
	rtd->routing.setConn(rtd->actorSize, x, y, z, dir, 0);
	rtd->routing.setStepup(rtd->actorSize, x, y, z, dir, PATHFINDING_NO_STEPUP);
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
	bool usable;	/**< does an actor fit in here ? */

	inline bool isUsable (void) const
	{
		return usable;
	}
} place_t;

static inline void RT_PlaceInit (const Routing& routing, const actorSizeEnum_t actorSize, place_t* p, const int x, const int y, const int z)
{
	p->cell[0] = x;
	p->cell[1] = y;
	p->cell[2] = z;
	const int relCeiling = routing.getCeiling(actorSize, p->cell);
	p->floor = routing.getFloor(actorSize, x, y, z) + z * CELL_HEIGHT;
	p->ceiling = relCeiling + z * CELL_HEIGHT;
	p->floorZ = std::max(0, p->floor / CELL_HEIGHT) ;
	p->usable = (relCeiling && p->floor > -1 && p->ceiling - p->floor >= PATHFINDING_MIN_OPENING) ? true : false;
}

static inline bool RT_PlaceDoesIntersectEnough (const place_t* p, const place_t* other)
{
	return (std::min(p->ceiling, other->ceiling) - std::max(p->floor, other->floor) >= PATHFINDING_MIN_OPENING);
}

/**
 * @brief This function detects a special stairway situation, where one place is right
 * in front of a stairway and has a floor at eg. 1 and a ceiling at eg. 16.
 * The other place has the beginning of the stairway, so the floor is at eg. 6
 * and the ceiling is that of the higher level, eg. 32.
 */
static inline int RT_PlaceIsShifted (const place_t* p, const place_t* other)
{
	if (!p->isUsable() || !other->isUsable())
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
 * @param[in] routing The routing maps (either server or client map)
 * @param[in] actorSize The size of the actor along the X and Y axis in cell units
 * @param[in] lx The low end of the x range updated
 * @param[in] ly The low end of the y range updated
 * @param[in] lz The low end of the z range updated
 * @param[in] hx The high end of the x range updated
 * @param[in] hy The high end of the y range updated
 * @param[in] hz The high end of the z range updated
 */
static void RT_DumpMap (const Routing& routing, actorSizeEnum_t actorSize, int lx, int ly, int lz, int hx, int hy, int hz)
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
					, RT_CONN_NX(routing, actorSize, x, y, z) ? "w" : " "
					, RT_CONN_PY(routing, actorSize, x, y, z) ? "n" : " "
					, RT_CONN_NY(routing, actorSize, x, y, z) ? "s" : " "
					, RT_CONN_PX(routing, actorSize, x, y, z) ? "e" : " "
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
void RT_DumpWholeMap (mapTiles_t* mapTiles, const Routing& routing)
{
	AABB mapBox;
	RT_GetMapSize(mapTiles, mapBox);

	/* convert the coords */
	pos3_t start, end;
	VecToPos(mapBox.mins, start);
	VecToPos(mapBox.maxs, end);
	start[0]--;
	start[1]--;

	/* Dump the client map */
	RT_DumpMap(routing, ACTOR_SIZE_NORMAL, start[0], start[1], start[2], end[0], end[1], end[2]);
}
#endif

/**
 * @brief Check if an actor can stand(up) in the cell given by pos
 */
bool RT_CanActorStandHere (const Routing& routing, const int actorSize, const pos3_t pos)
{
	if (routing.getCeiling(actorSize, pos) - routing.getFloor(actorSize, pos) >= PLAYER_STANDING_HEIGHT / QUANT)
		return true;
	else
		return false;
}

/**
 * @brief Calculate the map size via model data and store grid size
 * in map_min and map_max. This is done with every new map load
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[out] mapBox The lower and upper extents of the current map.
 * @sa CMod_LoadRouting
 * @sa DoRouting
 */
void RT_GetMapSize (mapTiles_t* mapTiles, AABB& mapBox)
{
	const vec3_t normal = {UNIT_SIZE / 2, UNIT_SIZE / 2, UNIT_HEIGHT / 2};
	pos3_t start, end, test;

	/* Initialize start, end, and normal */
	VectorClear(start);
	VectorSet(end, PATHFINDING_WIDTH - 1, PATHFINDING_WIDTH - 1, PATHFINDING_HEIGHT - 1);

	for (int i = 0; i < 3; i++) {
		AABB box;
		/* Lower positive boundary */
		while (end[i] > start[i]) {
			/* Adjust ceiling */
			VectorCopy(start, test);
			test[i] = end[i];
			/* Prep boundary box */
			PosToVec(test, box.mins);
			VectorSubtract(box.mins, normal, box.mins);
			PosToVec(end, box.maxs);
			VectorAdd(box.maxs, normal, box.maxs);
			/* Test for stuff in a small box, if there is something then exit while */
			const trace_t trace = RT_COMPLETEBOXTRACE_SIZE(mapTiles, &box, nullptr, TRACE_VISIBLE_LEVELS);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, lower the boundary. */
			end[i]--;
		}

		/* Raise negative boundary */
		while (end[i] > start[i]) {
			/* Adjust ceiling */
			VectorCopy(end, test);
			test[i] = start[i];

			/* Prep boundary box */
			PosToVec(start, box.mins);
			VectorSubtract(box.mins, normal, box.mins);
			PosToVec(test, box.maxs);
			VectorAdd(box.maxs, normal, box.maxs);
			box.maxs[i] -= DIST_EPSILON;			/* stay away from the upper bound just a little bit, so we don't catch a touching brush */

			/* Test for stuff in the box, if there is something then exit while */
			const trace_t trace = RT_COMPLETEBOXTRACE_SIZE(mapTiles, &box, nullptr, TRACE_VISIBLE_LEVELS);
			if (trace.fraction < 1.0)
				break;
			/* There is nothing, raise the boundary. */
			start[i]++;
		}
	}

	/* Com_Printf("Extents: (%i, %i, %i) to (%i, %i, %i)\n", start[0], start[1], start[2], end[0], end[1], end[2]); */

	/* convert to vectors */
	PosToVec(start, mapBox.mins);
	PosToVec(end, mapBox.maxs);

	/* Stretch to the exterior edges of our extents */
	VectorSubtract(mapBox.mins, normal, mapBox.mins);
	VectorAdd(mapBox.maxs, normal, mapBox.maxs);
}


/*
===============================================================================
NEW MAP TRACING FUNCTIONS
===============================================================================
*/

/**
 * @brief Check if pos is on solid ground
 * @param[in] routing The map's routing data
 * @param[in] actorSize The size of the actor along the X and Y axis in cell units
 * @param[in] pos The position to check below
 * @return true if solid
 * @sa CL_AddTargetingBox
 * @todo see CL_ActorMoveMouse
 */
bool RT_AllCellsBelowAreFilled (const Routing& routing, const int actorSize, const pos3_t pos)
{
	/* the -1 level is considered solid ground */
	if (pos[2] == 0)
		return true;

	for (int i = 0; i < pos[2]; i++) {
		if (routing.getCeiling(actorSize, pos[0], pos[1], i) != 0)
			return false;
	}
	return true;
}

/**
 * @brief This function looks to see if an actor of a given size can occupy a cell(s) and if so
 *	identifies the floor and ceiling for that cell. If the cell has no floor, the floor will be negative
 *  with 0 indicating the base for the cell(s).  If there is no ceiling in the cell, the first ceiling
 *  found above the current cell will be used.  If there is no ceiling above the cell, the ceiling will
 *  be the top of the model.  This function will also adjust all floor and ceiling values for all cells
 *  between the found floor and ceiling.
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] routing The map's routing data
 * @param[in] actorSize The size of the actor along the X and Y axis in cell units
 * @param[in] x,y The x/y position in the routing arrays (0 - PATHFINDING_WIDTH-1)
 * @param[in] z The z position in the routing arrays (0 - PATHFINDING_HEIGHT-1)
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 * @return The z value of the next cell to scan, usually the cell with the ceiling.
 * @sa Grid_RecalcRouting
 */
int RT_CheckCell (mapTiles_t* mapTiles, Routing& routing, const int actorSize, const int x, const int y, const int z, const char** list)
{
	/* Width of the box required to stand in a cell by an actor's torso.  */
	const float halfActorWidth = UNIT_SIZE * actorSize / 2 - WALL_SIZE - DIST_EPSILON;
	/* This is a template for the extents of the box used by an actor's legs. */
	const AABB legBox(-halfMicrostepSize, -halfMicrostepSize, 0,
						halfMicrostepSize,  halfMicrostepSize, QuantToModel(PATHFINDING_LEGROOMHEIGHT) - DIST_EPSILON * 2);
	/* This is a template for the extents of the box used by an actor's torso. */
	const AABB torsoBox(-halfActorWidth, -halfActorWidth, QuantToModel(PATHFINDING_LEGROOMHEIGHT),
						  halfActorWidth,  halfActorWidth, QuantToModel(PATHFINDING_MIN_OPENING) - DIST_EPSILON * 2);
	/* This is a template for the ceiling trace after an actor's torso space has been found. */
	const AABB ceilBox(-halfActorWidth, -halfActorWidth, 0,
						 halfActorWidth,  halfActorWidth, 0);

	vec3_t start, end; /* Start and end of the downward traces. */
	vec3_t tstart, tend; /* Start and end of the upward traces. */
	AABB box; /* Holds the exact bounds to be traced for legs and torso. */
	pos3_t pos;
	float bottom, top; /* Floor and ceiling model distances from the cell base. (in mapunits) */
#ifdef DEBUG
	float initial; /* Cell floor and ceiling z coordinate. */
#endif
	int bottomQ, topQ; /* The floor and ceiling in QUANTs */
	int i;
	int fz, cz; /* Floor and ceiling Z cell coordinates */

	assert(actorSize > ACTOR_SIZE_INVALID && actorSize <= ACTOR_MAX_SIZE);
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
	for (;;) { /* Loop forever, we will exit if we hit the model bottom or find a valid floor. */

		/* Check if a previous iteration has brought us too close to the bottom of the model. */
		if (start[2] <= QuantToModel(PATHFINDING_MIN_OPENING)) {
			/* There is no useable brush underneath this starting point. */
			routing.setFilled(actorSize, x, y, 0, z);	/* mark all cells to the model base as filled. */
			return 0;									/* return (a z-value of)0 to indicate we just scanned the model bottom. */
		}

		trace_t tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, Line(start, end), &footBox, list);
		if (tr.fraction >= 1.0) {						/* If there is no brush underneath this starting point, */
			routing.setFilled(actorSize, x, y, 0, z);	/* mark all cells to the model base as filled. */
			return 0;									/* return (a z-value of)0 to indicate we just scanned the model bottom. */
		}

		/* We have hit a brush that faces up and can be stood on. A potential floor. Look for a ceiling. */
		bottom = tr.endpos[2];  /* record the floor position. */

#ifdef DEBUG
		assert(initial > bottom);
#endif

		/* Record the hit position in tstart for later use. */
		VectorCopy(tr.endpos, tstart);

		/* Prep the start and end of the "leg room" test. */
		box.set(legBox);
		box.shift(tstart);	/* Now box has the lower and upper required foot space extent */

		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, Line(), &box, list);
		if (tr.fraction < 1.0) {
			/*
			 * There is a premature obstruction.  We can't use this as a floor.
			 * Check under start.  We need to have at least the minimum amount of clearance from our ceiling,
			 * So start at that point.
			 */
			start[2] = bottom - QuantToModel(PATHFINDING_MIN_OPENING);
			/* Restart  with the new start[] value */
			continue;
		}

		/* Prep the start and end of the "torso room" test. */
		box.set(torsoBox);
		box.shift(tstart);	/* Now box has the lower and upper required torso space extent */

		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, Line(), &box, list);
		if (tr.fraction < 1.0) {
			/*
			 * There is a premature obstruction.  We can't use this as a floor.
			 * Check under start.  We need to have at least the minimum amount of clearance from our ceiling,
			 * So start at that point.
			 */
			start[2] = bottom - QuantToModel(PATHFINDING_MIN_OPENING);
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

		tr = RT_COMPLETEBOXTRACE_PASSAGE(mapTiles, Line(tstart, tend), &ceilBox, list);

		/* We found the ceiling. */
		top = tr.endpos[2];

		/*
		 * There is one last possibility:
		 * If our found ceiling is above the cell we started the scan in, then we may have scanned up through another
		 * floor (one sided brush).  If this is the case, we set the ceiling to QUANT below the floor of the above
		 * ceiling if it is lower than our found ceiling.
		 */
		if (tr.endpos[2] > (z + 1) * UNIT_HEIGHT) {
			const float topf = (z + 1) * UNIT_HEIGHT + QuantToModel(routing.getFloor(actorSize, x, y, z + 1) - 1);
			top = std::min(tr.endpos[2], topf);
		}

		/* We found the ceiling. */
		top = tr.endpos[2];

		/* exit the infinite while loop */
		break;
	}

	UFO_assert(bottom <= top, "\nassert(bottom <= top): x=%i y=%i bottom=%f top=%f\n", x, y, bottom, top);

	/* top and bottom are absolute model heights.  Find the actual cell z coordinates for these heights.
	 * ...but before rounding, give back the DIST_EPSILON that was added by the trace.
	 * Actually, we have to give back two DIST_EPSILON to prevent rounding issues */
	bottom -= 2 * DIST_EPSILON;
	top += 2 * DIST_EPSILON;
	bottomQ = ModelFloorToQuant(bottom); /* Convert to QUANT units to ensure the floor is rounded up to the correct value. */
	topQ = ModelCeilingToQuant(top); /* Convert to QUANT units to ensure the floor is rounded down to the correct value. */
	fz = floorf(bottomQ / CELL_HEIGHT); /* Ensure we round down to get the bottom-most affected cell */
	/** @note Remember that ceiling values of 1-16 belong to a cell.  We need to adjust topQ by 1 to round to the correct z value. */
	cz = std::min(z, (int)(floorf((topQ - 1) / CELL_HEIGHT))); /* Use the lower of z or the calculated ceiling */

	assert(fz <= cz);

	/* Last, update the floors and ceilings of cells from (x, y, fz) to (x, y, cz) */
	for (i = fz; i <= cz; i++) {
		/* Round up floor to keep feet out of model. */
		routing.setFloor(actorSize, x, y, i, bottomQ - i * CELL_HEIGHT);
		/* Round down ceiling to heep head out of model.  Also offset by floor and max at 255. */
		routing.setCeiling(actorSize, x, y, i, topQ - i * CELL_HEIGHT);
	}

	/* Also, update the floors of any filled cells immediately above the ceiling up to our original cell. */
	routing.setFilled(actorSize, x, y, cz + 1, z);

	/* Return the lowest z coordinate that we updated floors for. */
	return fz;
}


/**
 * @brief Performs traces to find a passage between two points given an upper and lower bound.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] dir Direction of movement
 * @param[in] x,y,z Grid coordinates of the starting position
 * @param[in] openingSize Absolute height in QUANT units of the opening.
 * @param[in] openingBase Absolute height in QUANT units of the bottom of the opening.
 * @param[in] stepup Required stepup to travel in this direction.
 */
static int RT_FillPassageData (RoutingData* rtd, const int dir, const int  x, const int y, const int z, const int openingSize, const int openingBase, const int stepup)
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
	cz = ceil((float)openingTop / CELL_HEIGHT) - 1;
	cz = std::min(PATHFINDING_HEIGHT - 1, cz);

	/* last chance- if cz < z, then bail (and there is an error with the ceiling data somewhere */
	if (cz < z) {
		/* We can't go this way. */
		RT_ConnSetNoGo(rtd, x, y, z, dir);	/* Passage found but below current cell */
		return z;
	}

	/* Passage found */

	assert(fz <= z && z <= cz);

	/* Last, update the connections of cells from (x, y, fz) to (x, y, cz) for direction dir */
	for (i = fz; i <= cz; i++) {
		int oh;
		/* Offset from the floor or the bottom of the current cell, whichever is higher. */
		oh = openingTop - std::max(openingBase, i * CELL_HEIGHT);
		/* Only if > 0 */
		assert (oh >= 0);
		rtd->routing.setConn(rtd->actorSize, x, y, i, dir, oh);
		/* The stepup is 0 for all cells that are not at the floor. */
		RT_StepupSet(rtd, x, y, i, dir, 0);
	}

	RT_StepupSet(rtd, x, y, z, dir, stepup);

	/*
	 * Return the highest z coordinate scanned- cz if fz==cz, z==cz, or the floor in cz is negative.
	 * Otherwise cz - 1 to recheck cz in case there is a floor in cz with its own ceiling.
	 */
	if (fz == cz || z == cz || rtd->routing.getFloor(rtd->actorSize, x, y, cz) < 0)
		return cz;
	return cz - 1;
}

/**
 * @brief Helper function to trace for walls
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] traceLine The starting point of the trace is at the FLOOR'S CENTER. The end point of the trace is centered x and y at the destination but at the same height as start.
 * @param[in] hi The upper height ABOVE THE FLOOR of the bounding box.
 * @param[in] lo The lower height ABOVE THE FLOOR of the bounding box.
 */
static trace_t RT_ObstructedTrace (const RoutingData* rtd, const Line& traceLine, int hi, int lo)
{
	AABB box; /**< Tracing box extents */
	const float halfActorWidth = UNIT_SIZE * rtd->actorSize / 2 - WALL_SIZE - DIST_EPSILON;

	/* Configure the box trace extents. The box is relative to the original floor. */
	VectorSet(box.maxs, halfActorWidth, halfActorWidth, QuantToModel(hi) - DIST_EPSILON);
	VectorSet(box.mins, -halfActorWidth, -halfActorWidth, QuantToModel(lo)  + DIST_EPSILON);

	/* perform the trace, then return true if the trace was obstructed. */
	return RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, traceLine, &box, rtd->list);
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
static int RT_FindOpeningFloorFrac (const RoutingData* rtd, const vec3_t start, const vec3_t end, const float frac, const int startingHeight)
{
	vec3_t mstart, mend;	/**< Midpoint line to trace across */	/**< Tracing box extents */
	const AABB* box = (rtd->actorSize == ACTOR_SIZE_NORMAL ? &actor1x1Box : &actor2x2Box);

	/* Position mstart and mend at the fraction point */
	VectorInterpolation(start, end, frac, mstart);
	VectorCopy(mstart, mend);
	mstart[2] = QuantToModel(startingHeight) + (QUANT / 2); /* Set at the starting height, plus a little more to keep us off a potential surface. */
	mend[2] = -QUANT;  /* Set below the model. */

	const trace_t tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, Line(mstart, mend), box, rtd->list);

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
static int RT_FindOpeningCeilingFrac (const RoutingData* rtd, const vec3_t start, const vec3_t end, const float frac, const int startingHeight)
{
	vec3_t mstart, mend;	/**< Midpoint line to trace across */
	const AABB* box = (rtd->actorSize == ACTOR_SIZE_NORMAL ? &actor1x1Box : &actor2x2Box);	/**< Tracing box extents */

	/* Position mstart and mend at the midpoint */
	VectorInterpolation(start, end, frac, mstart);
	VectorCopy(mstart, mend);
	mstart[2] = QuantToModel(startingHeight) - (QUANT / 2); /* Set at the starting height, minus a little more to keep us off a potential surface. */
	mend[2] = UNIT_HEIGHT * PATHFINDING_HEIGHT + QUANT;  /* Set above the model. */

	const trace_t tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, Line(mstart, mend), box, rtd->list);

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
static int RT_FindOpeningFloor (const RoutingData* rtd, const vec3_t start, const vec3_t end, const int startingHeight, const int floorLimit)
{
	/* Look for additional space below init_bottom, down to lowest_bottom. */
	int midfloor;

	if (start[0] == end[0] || start[1] == end[1]) {
		/* For orthogonal dirs, find the height at the midpoint. */
		midfloor = RT_FindOpeningFloorFrac(rtd, start, end, 0.5, startingHeight);
	} else {
		int midfloor2;

		/* If this is diagonal, trace the 1/3 and 2/3 points instead. */
		/* 1/3 point */
		midfloor = RT_FindOpeningFloorFrac(rtd, start, end, 0.33, startingHeight);

		/* 2/3 point */
		midfloor2 = RT_FindOpeningFloorFrac(rtd, start, end, 0.66, startingHeight);
		midfloor = std::max(midfloor, midfloor2);
	}

	/* return the highest floor. */
	return std::max(floorLimit, midfloor);
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
static int RT_FindOpeningCeiling (const RoutingData* rtd, const vec3_t start, const vec3_t end, const int startingHeight, const int ceilLimit)
{
	int midceil;

	if (start[0] == end[0] || start[1] == end[1]) {
		/* For orthogonal dirs, find the height at the midpoint. */
		midceil = RT_FindOpeningCeilingFrac(rtd, start, end, 0.5, startingHeight);
	} else {
		int midceil2;

		/* If this is diagonal, trace the 1/3 and 2/3 points instead. */
		/* 1/3 point */
		midceil = RT_FindOpeningCeilingFrac(rtd, start, end, 0.33, startingHeight);

		/* 2/3 point */
		midceil2 = RT_FindOpeningCeilingFrac(rtd, start, end, 0.66, startingHeight);
		midceil = std::min(midceil, midceil2);
	}

	/* return the lowest ceiling. */
	return std::min(ceilLimit, midceil);
}


static int RT_CalcNewZ (const RoutingData* rtd, const int ax, const int ay, const int top, const int hi)
{
	int temp_z, adj_lo;

	temp_z = floorf((hi - 1) / CELL_HEIGHT);
	temp_z = std::min(temp_z, PATHFINDING_HEIGHT - 1);
	adj_lo = rtd->routing.getFloor(rtd->actorSize, ax, ay, temp_z) + temp_z * CELL_HEIGHT;
	if (adj_lo > hi) {
		temp_z--;
		adj_lo = rtd->routing.getFloor(rtd->actorSize, ax, ay, temp_z) + temp_z * CELL_HEIGHT;
	}
	/**
	 * @note Return a value only if there is a floor for the adjacent cell.
	 * Also the found adjacent lo must be at lease MIN_OPENING-MIN_STEPUP below
	 * the top.
	 */
	if (adj_lo >= 0 && top - adj_lo >= PATHFINDING_MIN_OPENING - PATHFINDING_MIN_STEPUP) {
		return floorf(adj_lo / CELL_HEIGHT);	/* Found floor in destination cell */
	}

	return RT_NO_OPENING;	/* Skipping found floor in destination cell- not enough opening */
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
 * @param[out] foundLow Actual height of the bottom of the found passage.
 * @param[out] foundHigh Actual height of the top of the found passage.
 * @return The new z value of the actor after traveling in this direction from the starting location.
 */
static int RT_TraceOpening (const RoutingData* rtd, const vec3_t start, const vec3_t end, const int ax, const int ay, const int bottom, const int top, int lo, int hi, int* foundLow, int* foundHigh)
{
	const trace_t tr = RT_ObstructedTrace(rtd, Line(start, end), hi, lo);
	if (tr.fraction >= 1.0) {
		lo = RT_FindOpeningFloor(rtd, start, end, lo, bottom);
		hi = RT_FindOpeningCeiling(rtd, start, end, hi, top);
		if (hi - lo >= PATHFINDING_MIN_OPENING) {
			int tempZ;
			if (lo == -1) {
				/* Bailing- no floor in destination cell. */
				*foundLow = *foundHigh = 0;
				return RT_NO_OPENING;
			}
			/* This opening works, use it! */
			*foundLow = lo;
			*foundHigh = hi;
			/* Find the floor for the highest adjacent cell in this passage. */
			tempZ = RT_CalcNewZ(rtd, ax, ay, top, hi);
			if (tempZ != RT_NO_OPENING)
				return tempZ;
		}
	}
	*foundLow = *foundHigh = hi;
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
 * @param[out] foundLow Actual height of the bottom of the found passage.
 * @param[out] foundHigh Actual height of the top of the found passage.
 * @return The new z value of the actor after traveling in this direction from the starting location.
 */
static int RT_FindOpening (RoutingData* rtd, const place_t* from, const int ax, const int ay, const int bottom, const int top, int* foundLow, int* foundHigh)
{
	vec3_t start, end;
	pos3_t pos;
	int tempZ;

	if (bottom == -1) {
		/* Bailing- no floor in current cell. */
		*foundLow = *foundHigh = 0;
		return RT_NO_OPENING;
	}

	/* Initialize the starting vector */
	SizedPosToVec(from->cell, rtd->actorSize, start);

	/* Initialize the ending vector */
	VectorSet(pos, ax, ay, from->cell[2]);
	SizedPosToVec(pos, rtd->actorSize, end);

	/* Initialize the z component of both vectors */
	start[2] = end[2] = 0;

	/* ----- sky trace ------ */
	/* shortcut: if both ceilings are the sky, we can check for walls
	 * AND determine the bottom of the passage in just one trace */
	if (from->ceiling >= PATHFINDING_HEIGHT * CELL_HEIGHT
	 && from->cell[2] * CELL_HEIGHT + rtd->routing.getCeiling(rtd->actorSize, ax, ay, from->cell[2]) >= PATHFINDING_HEIGHT * CELL_HEIGHT) {
		vec3_t sky, earth;
		const AABB* box = (rtd->actorSize == ACTOR_SIZE_NORMAL ? &actor1x1Box : &actor2x2Box);
		int tempBottom;

		VectorInterpolation(start, end, 0.5, sky);	/* center it halfway between the cells */
		VectorCopy(sky, earth);
		sky[2] = UNIT_HEIGHT * PATHFINDING_HEIGHT;  /* Set to top of model. */
		earth[2] = QuantToModel(bottom);

		const trace_t tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, Line(sky, earth), box, rtd->list);
		tempBottom = ModelFloorToQuant(tr.endpos[2]);
		if (tempBottom <= bottom + PATHFINDING_MIN_STEPUP) {
			const int hi = bottom + PATHFINDING_MIN_OPENING;
			/* Found opening with sky trace. */
			*foundLow = tempBottom;
			*foundHigh = CELL_HEIGHT * PATHFINDING_HEIGHT;
			return RT_CalcNewZ(rtd, ax, ay, top, hi);
		}
	}
	/* Warning: never try to make this an 'else if', or 'arched entry' situations will fail !! */

	/* ----- guaranteed opening trace ------ */
	/* Now calculate the "guaranteed" opening, if any. If the opening from
	 * the floor to the ceiling is not too tall, there must be a section that
	 * will always be vacant if there is a usable passage of any size and at
	 * any height. */
	if (top - bottom < PATHFINDING_MIN_OPENING * 2) {
		const int lo = top - PATHFINDING_MIN_OPENING;
		const int hi = bottom + PATHFINDING_MIN_OPENING;

		tempZ = RT_TraceOpening(rtd, start, end, ax, ay, bottom, top, lo, hi, foundLow, foundHigh);
	} else {
		/* ----- brute force trace ------ */
		/* There is no "guaranteed" opening, brute force search. */
		int lo = bottom;
		tempZ = 0;
		while (lo <= top - PATHFINDING_MIN_OPENING) {
			/* Check for a 1 QUANT opening. */
			tempZ = RT_TraceOpening(rtd, start, end, ax, ay, bottom, top, lo, lo + 1, foundLow, foundHigh);
			if (tempZ != RT_NO_OPENING)
				break;
			/* Credit to Duke: We skip the minimum opening, as if there is a
			 * viable opening, even one slice above, that opening would be open. */
			lo += PATHFINDING_MIN_OPENING;
		}
	}
	return tempZ;
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
static int RT_MicroTrace (RoutingData* rtd, const place_t* from, const int ax, const int ay, const int az, const int stairwaySituation, opening_t* opening)
{
	/* OK, now we have a viable shot across.  Run microstep tests now. */
	/* Now calculate the stepup at the floor using microsteps. */
	int top = opening->base + opening->size;
	signed char bases[UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE + 1];
	float sx, sy, ex, ey;
	/* Shortcut the value of UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE. */
	const int steps = UNIT_SIZE / PATHFINDING_MICROSTEP_SIZE;
	int i, current_h, highest_h, highest_i = 0, skipped, newBottom;
	vec3_t start, end;
	pos3_t pos;

	/* First prepare the two known end values. */
	bases[0] = from->floor;
	const int floorVal = rtd->routing.getFloor(rtd->actorSize, ax, ay, az);
	bases[steps] = std::max(0, floorVal) + az * CELL_HEIGHT;

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

	newBottom = std::max(bases[0], bases[steps]);
	/* Now calculate the rest of the microheights. */
	for (i = 1; i < steps; i++) {
		start[0] = end[0] = sx + (ex - sx) * (i / (float)steps);
		start[1] = end[1] = sy + (ey - sy) * (i / (float)steps);

		/* perform the trace, then return true if the trace was obstructed. */
		const trace_t tr = RT_COMPLETEBOXTRACE_PASSAGE(rtd->mapTiles, Line(start, end), &footBox, rtd->list);
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

		newBottom = std::max(newBottom, (int)bases[i]);
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
			opening->stepup = std::max(opening->stepup, bases[i] - current_h);
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
	highest_i = steps - 1;	/**< Note that for this part of the code, this is the LOWEST i. */
	opening->invstepup = 0;	/**<  Was originally -CELL_HEIGHT, but stepup is needed to go UP, not down. */
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
			opening->invstepup = std::max(opening->invstepup, bases[i] - current_h);
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
		const int middle = bases[4];		/* terrible hack by Duke. This relies on PATHFINDING_MICROSTEP_SIZE being set to 4 !! */

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
static int RT_TraceOnePassage (RoutingData* rtd, const place_t* from, const place_t* to, opening_t* opening)
{
	int hi; /**< absolute ceiling of the passage found. */
	const int z = from->cell[2];
	int az; /**< z height of the actor after moving in this direction. */
	const int lower = std::max(from->floor, to->floor);
	const int upper = std::min(from->ceiling, to->ceiling);
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
		const int dstFloor = rtd->routing.getFloor(rtd->actorSize, ax, ay, az) + az * CELL_HEIGHT;
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
			opening->stepup = std::max(0, opening->base - srcFloor);
			opening->invstepup = std::max(0, opening->base - dstFloor);
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

	opening->stepup = PATHFINDING_NO_STEPUP;
	opening->invstepup = PATHFINDING_NO_STEPUP;
	return 0;
}

/**
 * @brief Performs traces to find a passage between two points.
 * @param[in] rtd The essential routing data with map, actorsize, ents
 * @param[in] x,y,z Starting coordinates
 * @param[in] ax,ay Ending coordinates (adjacent cell)
 * @param[out] opening descriptor of the opening found, if any
 */
static void RT_TracePassage (RoutingData* rtd, const int x, const int y, const int z, const int ax, const int ay, opening_t* opening)
{
	int aboveCeil, lowCeil;
	/** we don't need the cell below the adjacent cell because we should have already checked it */
	place_t from, to, above;
	const place_t* placeToCheck = nullptr;

	RT_PlaceInit(rtd->routing, rtd->actorSize, &from, x, y, z);
	RT_PlaceInit(rtd->routing, rtd->actorSize, &to, ax, ay, z);

	aboveCeil = (z < PATHFINDING_HEIGHT - 1) ? rtd->routing.getCeiling(rtd->actorSize, ax, ay, z + 1) + (z + 1) * CELL_HEIGHT : to.ceiling;
	lowCeil = std::min(from.ceiling, (rtd->routing.getCeiling(rtd->actorSize, ax, ay, z) == 0 || to.ceiling - from.floor < PATHFINDING_MIN_OPENING) ? aboveCeil : to.ceiling);

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
	if (to.isUsable() && RT_PlaceDoesIntersectEnough(&from, &to)) {
		placeToCheck = &to;
	} else if (z < PATHFINDING_HEIGHT - 1) {
		RT_PlaceInit(rtd->routing, rtd->actorSize, &above, ax, ay, z + 1);
		if (above.isUsable() && RT_PlaceDoesIntersectEnough(&from, &above)) {
			placeToCheck = &above;
		}
	}
	if (!placeToCheck) {
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
	RT_TraceOnePassage(rtd, &from, placeToCheck, opening);
	if (opening->size < PATHFINDING_MIN_OPENING) {
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
 * @param[in] x,y The x/y position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] ax,ay The x/y of the adjacent cell
 * @param[in] z The z position in the routing arrays (0 to PATHFINDING_HEIGHT - 1)
 * @param[in] dir The direction to test for a connection through
 */
static int RT_UpdateConnection (RoutingData* rtd, const int x, const int y, const int ax, const int ay, const int z, const int dir)
{
	/** test if the adjacent cell and the cell above it are blocked by a loaded model */
	const int ceiling = rtd->routing.getCeiling(rtd->actorSize, x, y, z);
	const int adjCeiling = rtd->routing.getCeiling(rtd->actorSize, ax, ay, z);
	const int upperAdjCeiling = (z < PATHFINDING_HEIGHT - 1) ? rtd->routing.getCeiling(rtd->actorSize, ax, ay, z + 1) : adjCeiling;

	if ((adjCeiling == 0 && upperAdjCeiling == 0) || ceiling == 0) {
		/* We can't go this way. */
		RT_ConnSetNoGo(rtd, x, y, z, dir);
		return z;
	}

	/**
	 * @note OK, simple test here.  We know both cells have a ceiling, so they are both open.
	 *  If the absolute ceiling of one is below the absolute floor of the other, then there is no intersection.
	 */
	const int absCeiling = ceiling + z * CELL_HEIGHT;
//	const int absAdjCeiling = adjCeiling + z * CELL_HEIGHT;		/* temporarily unused */
	const int absExtAdjCeiling = (z < PATHFINDING_HEIGHT - 1) ? adjCeiling + (z + 1) * CELL_HEIGHT : absCeiling;
	const int absFloor = rtd->routing.getFloor(rtd->actorSize, x, y, z) + z * CELL_HEIGHT;
	const int absAdjFloor = rtd->routing.getFloor(rtd->actorSize, ax, ay, z) + z * CELL_HEIGHT;

	if (absCeiling < absAdjFloor || absExtAdjCeiling < absFloor) {
		/* We can't go this way. */
		RT_ConnSetNoGo(rtd, x, y, z, dir);
		return z;
	}

	/** Find an opening. */
	opening_t opening;	/** the opening between the two cells */
	RT_TracePassage(rtd, x, y, z, ax, ay, &opening);

	/** Apply the data to the routing table.
	 * We always call the fill function.  If the passage cannot be traveled, the
	 * function fills it in as unpassable. */
	int newZ = RT_FillPassageData(rtd, dir, x, y, z, opening.size, opening.base, opening.stepup);

	return newZ;
}


/**
 * @brief Routing Function to update the connection between two fields
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] routing Routing table of the current loaded map
 * @param[in] actorSize The size of the actor, in units
 * @param[in] x,y The position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] dir The direction to test for a connection through
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 * @param[in] minZ,maxZ Limit the update to a part of the column
 */
void RT_UpdateConnectionColumn (mapTiles_t* mapTiles, Routing& routing, const int actorSize, const int x, const int y, const int dir, const char** list, const int minZ, const int maxZ)
{
	int z = minZ;	/**< The current z value that we are testing. */
	/* the essential data passed down the calltree */
	RoutingData rtd(mapTiles, routing, actorSize, list);

	/* get the neighbor cell's coordinates */
	const int ax = x + dvecs[dir][0];
	const int ay = y + dvecs[dir][1];

	assert(actorSize > ACTOR_SIZE_INVALID && actorSize <= ACTOR_MAX_SIZE);
	assert((x >= 0) && (x <= PATHFINDING_WIDTH - actorSize));
	assert((y >= 0) && (y <= PATHFINDING_WIDTH - actorSize));

#ifdef DEBUG
	/** @todo remove me */
	/* just a place to place a breakpoint */
	if (x == 126 && y == 129 && dir == 2) {
		z = 7;
	}
#endif

	/* if our destination cell is out of bounds, bail. */
	if (ax < 0 || ax > PATHFINDING_WIDTH - actorSize || ay < 0 || ay > PATHFINDING_WIDTH - actorSize) {
		/* We can't go this way. */
		RT_ConnSetNoGo(&rtd, x, y, z, dir);
		return;
	}

	/* Main loop */
	for (z = minZ; z < maxZ; z++) {
		/* The last z value processed by the tracing function.  */
		const int new_z = RT_UpdateConnection(&rtd, x, y, ax, ay, z, dir);
		assert(new_z >= z);
		z = new_z;
	}
}

void RT_WriteCSVFiles (const Routing& routing, const char* baseFilename, const GridBox& box)
{
	char filename[MAX_OSPATH], ext[MAX_OSPATH];
	int x, y, z;

	/* An elevation files- dumps the floor and ceiling levels relative to each cell. */
	for (int i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.elevation.csv", i);
		Com_DefaultExtension(filename, sizeof(filename), ext);
		ScopedFile f;
		FS_OpenFile(filename, &f, FILE_WRITE);
		if (!f)
			Sys_Error("Could not open file %s.", filename);
		FS_Printf(&f, ",");
		for (x = box.getMinX(); x <= box.getMaxX() - i + 1; x++)
			FS_Printf(&f, "x:%i,", x);
		FS_Printf(&f, "\n");
		for (z = box.getMaxZ(); z >= box.getMinZ(); z--) {
			for (y = box.getMaxY(); y >= box.getMinY() - i + 1; y--) {
				FS_Printf(&f, "z:%i  y:%i,", z ,y);
				for (x = box.getMinX(); x <= box.getMaxX() - i + 1; x++) {
					/* compare results */
					FS_Printf(&f, "h:%i c:%i,", routing.getFloor(i, x, y, z), routing.getCeiling(i, x, y, z));
				}
				FS_Printf(&f, "\n");
			}
			FS_Printf(&f, "\n");
		}
	}

	/* Output the walls/passage files. */
	for (int i = 1; i <= ACTOR_MAX_SIZE; i++) {
		strncpy(filename, baseFilename, sizeof(filename) - 1);
		sprintf(ext, ".%i.walls.csv", i);
		Com_DefaultExtension(filename, sizeof(filename), ext);
		ScopedFile f;
		FS_OpenFile(filename, &f, FILE_WRITE);
		if (!f)
			Sys_Error("Could not open file %s.", filename);
		FS_Printf(&f, ",");
		for (x = box.getMinX(); x <= box.getMaxX() - i + 1; x++)
			FS_Printf(&f, "x:%i,", x);
		FS_Printf(&f, "\n");
		for (z = box.getMaxZ(); z >= box.getMinZ(); z--) {
			for (y = box.getMaxY(); y >= box.getMinY() - i + 1; y--) {
				FS_Printf(&f, "z:%i  y:%i,", z, y);
				for (x = box.getMinX(); x <= box.getMaxX() - i + 1; x++) {
					/* compare results */
					FS_Printf(&f, "\"");

					/* NW corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NX_PY(routing, i, x, y, z), RT_STEPUP_NX_PY(routing, i, x, y, z));

					/* N side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PY(routing, i, x, y, z), RT_STEPUP_PY(routing, i, x, y, z));

					/* NE corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PX_PY(routing, i, x, y, z), RT_STEPUP_PX_PY(routing, i, x, y, z));

					FS_Printf(&f, "\n");

					/* W side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NX(routing, i, x, y, z), RT_STEPUP_NX(routing, i, x, y, z));

					/* Center - display floor height */
					FS_Printf(&f, "_%+2i_ ", routing.getFloor(i, x, y, z));

					/* E side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PX(routing, i, x, y, z), RT_STEPUP_PX(routing, i, x, y, z));

					FS_Printf(&f, "\n");

					/* SW corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NX_NY(routing, i, x, y, z), RT_STEPUP_NX_NY(routing, i, x, y, z));

					/* S side */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_NY(routing, i, x, y, z), RT_STEPUP_NY(routing, i, x, y, z));

					/* SE corner */
					FS_Printf(&f, "%3i-%3i ", RT_CONN_PX_NY(routing, i, x, y, z), RT_STEPUP_PX_NY(routing, i, x, y, z));

					FS_Printf(&f, "\",");
				}
				FS_Printf(&f, "\n");
			}
			FS_Printf(&f, "\n");
		}
	}
}

#ifdef DEBUG
/**
 * @brief A debug function to be called from CL_DebugPath_f
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] routing Routing table of the current loaded map
 * @param[in] actorSize The size of the actor, in units
 * @param[in] x,y The x/y position in the routing arrays (0 to PATHFINDING_WIDTH - actorSize)
 * @param[in] dir The direction to test for a connection through
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
int RT_DebugSpecial (mapTiles_t* mapTiles, Routing& routing, const int actorSize, const int x, const int y, const int dir, const char** list)
{
	int z = 0; /**< The current z value that we are testing. */
	int new_z; /**< The last z value processed by the tracing function.  */
	RoutingData rtd(mapTiles, routing, actorSize, list);	/* the essential data passed down the calltree */

	/* get the neighbor cell's coordinates */
	const int ax = x + dvecs[dir][0];
	const int ay = y + dvecs[dir][1];

	new_z = RT_UpdateConnection(&rtd, x, y, ax, ay, z, dir);
	return new_z;
}

/**
 * @brief display pathfinding info to the console. Also useful to
 * directly use the debugger on some vital pathfinding functions.
 * Will probably be removed for the release.
 */
void RT_DebugPathDisplay (Routing& routing, actorSizeEnum_t actorSize, int x, int y, int z)
{
	Com_Printf("data at cursor XYZ(%i, %i, %i) Floor(%i) Ceiling(%i)\n", x, y, z,
		routing.getFloor(actorSize, x, y, z),
		routing.getCeiling(actorSize, x, y, z) );
	Com_Printf("connections ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_CONN_PX(routing, actorSize, x, y, z),		/* dir = 0 */
		RT_CONN_NX(routing, actorSize, x, y, z),		/* 1 */
		RT_CONN_PY(routing, actorSize, x, y, z),		/* 2 */
		RT_CONN_NY(routing, actorSize, x, y, z) );		/* 3 */
	Com_Printf("connections diago: (PX_PY=%i, NX_NY=%i, NX_PY=%i, PX_NY=%i))\n",
		RT_CONN_PX_PY(routing, actorSize, x, y, z),		/* dir = 4 */
		RT_CONN_NX_NY(routing, actorSize, x, y, z),		/* 5 */
		RT_CONN_NX_PY(routing, actorSize, x, y, z),		/* 6 */
		RT_CONN_PX_NY(routing, actorSize, x, y, z) );	/* 7 */
	Com_Printf("stepup ortho: (PX=%i, NX=%i, PY=%i, NY=%i))\n",
		RT_STEPUP_PX(routing, actorSize, x, y, z),		/* dir = 0 */
		RT_STEPUP_NX(routing, actorSize, x, y, z),		/* 1 */
		RT_STEPUP_PY(routing, actorSize, x, y, z),		/* 2 */
		RT_STEPUP_NY(routing, actorSize, x, y, z) );	/* 3 */
}

#endif
