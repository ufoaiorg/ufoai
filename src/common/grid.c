/**
 * @file grid.c
 * @brief Grid oriented movement and scanning
 */

/*
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
#include "grid.h"
#include "tracing.h"
#include "routing.h"
#include "pqueue.h"

/** @note these are the TUs used to intentionally move in a given direction.  Falling not included. */
static const int TUsUsed[] = {
	TU_MOVE_STRAIGHT, /* E  */
	TU_MOVE_STRAIGHT, /* W  */
	TU_MOVE_STRAIGHT, /* N  */
	TU_MOVE_STRAIGHT, /* S  */
	TU_MOVE_DIAGONAL, /* NE */
	TU_MOVE_DIAGONAL, /* SW */
	TU_MOVE_DIAGONAL, /* NW */
	TU_MOVE_DIAGONAL, /* SE */
	TU_MOVE_CLIMB,    /* UP     */
	TU_MOVE_CLIMB,    /* DOWN   */
	TU_CROUCH,        /* STAND  */
	TU_CROUCH,        /* CROUCH */
	0,				  /* ???    */
	TU_MOVE_FALL,	  /* FALL   */
	0,				  /* ???    */
	0,				  /* ???    */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY UP & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY UP & SE */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY LEVEL & SE */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & E  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & W  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & N  */
	TU_MOVE_STRAIGHT * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & S  */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NE */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & SW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR, /* FLY DOWN & NW */
	TU_MOVE_DIAGONAL * TU_FLYING_MOVING_FACTOR  /* FLY DOWN & SE */
};
CASSERT(lengthof(TUsUsed) == PATHFINDING_DIRECTIONS);

/**
* @brief Checks one field (square) on the grid of the given routing data (i.e. the map).
 * @param[in] exclude Exclude this position from the forbidden list check
 * @param[in] actorSize width of the actor in cells
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @param[in] x Field in x direction
 * @param[in] y Field in y direction
 * @param[in] z Field in z direction
 * @sa Grid_MoveMark
 * @sa G_BuildForbiddenList
 * @sa CL_BuildForbiddenList
 * @return qtrue if one can't walk there (i.e. the field [and attached fields for e.g. 2x2 units] is/are blocked by entries in
 * the forbidden list) otherwise false.
 */
static qboolean Grid_CheckForbidden (const pos3_t exclude, const actorSizeEnum_t actorSize, pathing_t *path, int x, int y, int z)
{
	pos_t **p;
	int i;
	actorSizeEnum_t size;
	int fx, fy, fz; /**< Holding variables for the forbidden x and y */
	byte *forbiddenSize;

	for (i = 0, p = path->fblist; i < path->fblength / 2; i++, p += 2) {
		/* Skip initial position. */
		if (VectorCompare((*p), exclude)) {
			/* Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: skipping %i|%i|%i\n", (*p)[0], (*p)[1], (*p)[2]); */
			continue;
		}

		forbiddenSize = *(p + 1);
		memcpy(&size, forbiddenSize, sizeof(size));
		fx = (*p)[0];
		fy = (*p)[1];
		fz = (*p)[2];

		/* Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: comparing (%i, %i, %i) * %i to (%i, %i, %i) * %i \n", x, y, z, actorSize, fx, fy, fz, size); */

		if (fx + size <= x || x + actorSize <= fx)
			continue; /* x bounds do not intersect */
		if (fy + size <= y || y + actorSize <= fy)
			continue; /* y bounds do not intersect */
		if (z == fz) {
			Com_DPrintf(DEBUG_PATHING, "Grid_CheckForbidden: Collision (%i, %i, %i) * %i and (%i, %i, %i) * %i \n", x, y, z, actorSize, fx, fy, fz, size);
			return qtrue; /* confirmed intersection */
		}
	}
	return qfalse;
}

static void Grid_SetMoveData (pathing_t *path, const int x, const int y, const int z, const int c, const byte length, const int dir, const int ox, const int oy, const int oz, const int oc, priorityQueue_t *pqueue)
{
	pos4_t dummy;

	RT_AREA_TEST(path, x, y, z, c);
	RT_AREA(path, x, y, z, c) = length;	/**< Store TUs for this square. */
	RT_AREA_FROM(path, x, y, z, c) = makeDV(dir, oz); /**< Store origination information for this square. */
	{
		pos3_t pos, test;
		int crouch = c;
		VectorSet(pos, ox, oy, oz);
		VectorSet(test, x, y, z);
		PosSubDV(test, crouch, RT_AREA_FROM(path, x, y, z, c));
		if (!VectorCompare(test, pos) || crouch != oc) {
			Com_Printf("Grid_SetMoveData: Created faulty DV table.\nx:%i y:%i z:%i c:%i\ndir:%i\nnx:%i ny:%i nz:%i nc:%i\ntx:%i ty:%i tz:%i tc:%i\n",
				ox, oy, oz, oc, dir, x, y, z, c, test[0], test[1], test[2], crouch);

			Com_Error(ERR_DROP, "Grid_SetMoveData: Created faulty DV table.");
		}
	}
	Vector4Set(dummy, x, y, z, c);
	/** @todo add heuristic for A* algorithm */
	PQueuePush(pqueue, dummy, length);
}

/**
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] exclude Exclude this position from the forbidden list check
 * @param[in] actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] pos Current location in the map.
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @param[in,out] pqueue Priority queue (heap) to insert the now reached tiles for reconsidering
 * @sa Grid_CheckForbidden
 */
static void Grid_MoveMark (const routing_t *map, const pos3_t exclude, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t pos, byte crouchingState, const int dir, priorityQueue_t *pqueue)
{
	int x, y, z;
	int nx, ny, nz;
	int dx, dy, dz;
	byte len, oldLen;
	int passageHeight;

	/** @todo flier should return true if the actor can fly. */
	const qboolean flier = qfalse; /**< This can be keyed into whether an actor can fly or not to allow flying */

	/** @todo has_ladder_support should return true if
	 *  1) There is a ladder in the new cell in the specified direction or
	 *  2) There is a ladder in any direction in the cell below the new cell and no ladder in the new cell itself. */
	const qboolean hasLadderSupport = qfalse; /**< Indicates if there is a ladder present providing support. */

	/** @todo has_ladder_climb should return true if
	 *  1) There is a ladder in the new cell in the specified direction. */
	const qboolean hasLadderClimb = qfalse; /**< Indicates if there is a ladder present providing ability to climb. */

	/** @todo falling_height should be replaced with an arbitrary max falling height based on the actor. */
	const int fallingHeight = PATHFINDING_MAX_FALL;/**<This is the maximum height that an actor can fall. */

	/** @note This is the actor's height in QUANT units. */
	const int actorHeight = ModelCeilingToQuant((float)(crouchingState ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT)); /**< The actor's height */

	/* Ensure that dir is in bounds. */
	if (dir < 0 || dir >= PATHFINDING_DIRECTIONS)
		return;

	/* Directions 12, 14, and 15 are currently undefined. */
	if (dir == 12 || dir == 14 || dir == 15)
		return;

	/* IMPORTANT: only fliers can use directions higher than NON_FLYING_DIRECTIONS. */
	if (!flier && dir >= FLYING_DIRECTIONS) {
		return;
	}

	x = pos[0];
	y = pos[1];
	z = pos[2];

	RT_AREA_TEST(path, x, y, z, crouchingState);
	oldLen = RT_AREA(path, x, y, z, crouchingState);

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: (%i %i %i) s:%i dir:%i c:%i ol:%i\n", x, y, z, actorSize, dir, crouchingState, oldLen);

	/* We cannot fly and crouch at the same time. This will also cause an actor to stand to fly. */
	if (crouchingState && dir >= FLYING_DIRECTIONS) {
		return;
	}

	if (oldLen >= MAX_MOVELENGTH && oldLen != ROUTING_NOT_REACHABLE) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Exiting because the TUS needed to move here are already too large. %i %i\n", oldLen , MAX_MOVELENGTH);
		return;
	}

	/* Find the number of TUs used to move in this direction. */
	len = Grid_GetTUsForDirection(dir);

	/* If crouching then multiply by the crouching factor. */
	if (crouchingState == 1)
		len *= TU_CROUCH_MOVING_FACTOR;

	/* Now add the TUs needed to get to the originating cell. */
	len += oldLen;

	/* If this is a crouching or crouching move, then process that motion. */
	if (dir == DIRECTION_STAND_UP || dir == DIRECTION_CROUCH) {
		/* Can't stand up if standing. */
		if (dir == DIRECTION_STAND_UP && crouchingState == 0) {
			return;
		}
		/* Can't stand up if there's not enough head room. */
		if (dir == DIRECTION_STAND_UP && QuantToModel(Grid_Height(map, actorSize, pos)) >= PLAYER_STANDING_HEIGHT) {
			return;
		}
		/* Can't get down if crouching. */
		if (dir == DIRECTION_CROUCH && crouchingState == 1) {
			return;
		}

		/* Since we can toggle crouching, then do so. */
		crouchingState ^= 1;

		/* Is this a better move into this cell? */
		RT_AREA_TEST(path, x, y, z, crouchingState);
		if (RT_AREA(path, x, y, z, crouchingState) <= len) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Toggling crouch is not optimum. %i %i\n", RT_AREA(path, x, y, z, crouchingState), len);
			return;
		}

		/* Store move. */
		if (pqueue)
			Grid_SetMoveData(path, x, y, z, crouchingState, len, dir, x, y, z, crouchingState ^ 1, pqueue);

		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Set move to (%i %i %i) c:%i to %i.\n", x, y, z, crouchingState, len);
		/* We are done, exit now. */
		return;
	}

	dx = dvecs[dir][0];	/**< Get the difference value for x for this direction. (can be pos or neg) */
	dy = dvecs[dir][1];	/**< Get the difference value for y for this direction. (can be pos or neg) */
	dz = dvecs[dir][2];	/**< Get the difference value for z for this direction. (can be pos or neg) */
	nx = x + dx;		/**< "new" x value = starting x value + difference from chosen direction */
	ny = y + dy;		/**< "new" y value = starting y value + difference from chosen direction */
	nz = z + dz;		/**< "new" z value = starting z value + difference from chosen direction */

	/* Connection checks.  If we cannot move in the desired direction, then bail. */
	/* Range check of new values (all sizes) */
	if (nx < 0 || nx > PATHFINDING_WIDTH - actorSize
	 || ny < 0 || ny > PATHFINDING_WIDTH - actorSize
	 || nz < 0 || nz > PATHFINDING_HEIGHT) {
		return;
	}

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: (%i %i %i) s:%i to (%i %i %i)\n", x, y, z, actorSize, nx, ny, nz);

	/* If there is no passageway (or rather lack of a wall) to the desired cell, then return. */
	/** @todo actor_height is currently the fixed height of a 1x1 actor.  This needs to be adjusted
	 *  to the actor's actual height, including crouching. */
	/* If the flier is moving up or down diagonally, then passage height will also adjust */
	if (dir >= FLYING_DIRECTIONS) {
		const int coreDir = dir % CORE_DIRECTIONS;	/**< The compass direction of this flying move */
		int neededHeight;
		if (dz > 0) {
			/* If the actor is moving up, check the passage at the current cell.
			 * The minimum height is the actor's height plus the distance from the current floor to the top of the cell. */
			neededHeight = actorHeight + CELL_HEIGHT - max(0, RT_FLOOR(map, actorSize, x, y, z));
			RT_CONN_TEST(map, actorSize, x, y, z, coreDir);
			passageHeight = RT_CONN(map, actorSize, x, y, z, coreDir);
		} else if (dz < 0) {
			/* If the actor is moving down, check from the destination cell back. *
			 * The minimum height is the actor's height plus the distance from the destination floor to the top of the cell. */
			neededHeight = actorHeight + CELL_HEIGHT - max(0, RT_FLOOR(map, actorSize, nx, ny, nz));
			RT_CONN_TEST(map, actorSize, nx, ny, nz, coreDir ^ 1);
			passageHeight = RT_CONN(map, actorSize, nx, ny, nz, coreDir ^ 1);
		} else {
			neededHeight = actorHeight;
			RT_CONN_TEST(map, actorSize, x, y, z, coreDir);
			passageHeight = RT_CONN(map, actorSize, x, y, z, coreDir);
		}
		if (passageHeight < neededHeight) {
			return;
		}
	} else if (dir < CORE_DIRECTIONS) {
		/**
		 * @note Fliers use this code only when they are walking.
		 * @brief First test for opening height availablilty.  Then test for stepup compatibility.
		 * Last test for fall.
		 */

		const int stepup = RT_STEPUP(map, actorSize, x, y, z, dir); /**< The stepup needed to get to/through the passage */
		const int stepupHeight = stepup & ~(PATHFINDING_BIG_STEPDOWN | PATHFINDING_BIG_STEPUP); /**< The actual stepup height without the level flags */
		int heightChange;

		/** @todo actor_stepup_height should be replaced with an arbitrary max stepup height based on the actor. */
		int actorStepupHeight = PATHFINDING_MAX_STEPUP;

		/* This is the standard passage height for all units trying to move horizontally. */
		RT_CONN_TEST(map, actorSize, x, y, z, dir);
		passageHeight = RT_CONN(map, actorSize, x, y, z, dir);
		if (passageHeight < actorHeight) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Passage is not tall enough. passage:%i actor:%i\n", passageHeight, actorHeight);
			return;
		}

		/* If we are moving horizontally, use the stepup requirement of the floors.
		 * The new z coordinate may need to be adjusted from stepup.
		 * Also, actor_stepup_height must be at least the cell's positive stepup value to move that direction. */
		/* If the actor cannot reach stepup, then we can't go this way. */
		if (actorStepupHeight < stepupHeight) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Actor cannot stepup high enough. passage:%i actor:%i\n", stepupHeight, actorStepupHeight);
			return;
		}
		if ((stepup & PATHFINDING_BIG_STEPUP) && nz < PATHFINDING_HEIGHT - 1) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Stepping up into higher cell.\n");
			nz++;
			/**
			 * @note If you need to know about how pathfinding works,  you need to understand the
			 * following brief.  It may cause nausea, but is an important concept.
			 *
			 * @brief OK, now some crazy tests:
			 * Because of the grid based nature of this game, each cell can have at most only ONE
			 * floor that can be stood upon.  If an actor can walk down a slope that is in the
			 * same level, and actor should be able to walk on (and not fall into) the slope that
			 * decends a game level.  BUT it is possible for an actor to be able to crawl under a
			 * floor that can be stood on, with this opening being in the same cell as the floor.
			 * SO to prevent any conflicts, we will move down a floor under the following conditions:
			 * - The STEPDOWN flag is set
			 * - The floor in the immediately adjacent cell is lower than the current floor, but not
			 *   more than CELL_HEIGHT units (in QUANT units) below the current floor.
			 * - The actor's stepup value is at least the inverse stepup value.  This is the stepup
			 *   FROM the cell we are moving towards back into the cell we are starting in.  This
			 *    ensures that the actor can actually WALK BACK.
			 * If the actor does not have a high enough stepup but meets all the other requirements to
			 * descend the level, the actor will move into a fall state, provided that there is no
			 * floor in the adjacent cell.
			 *
			 * This will prevent actors from walking under a floor in the same cell in order to fall
			 * to the floor beneath.  They will need to be able to step down into the cell or will
			 * not be able to use the opening.
			 */
		} else if ((stepup & PATHFINDING_BIG_STEPDOWN) && nz > 0
			&& actorStepupHeight >= (RT_STEPUP(map, actorSize, nx, ny, nz - 1, dir ^ 1) & ~(PATHFINDING_BIG_STEPDOWN | PATHFINDING_BIG_STEPUP))) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not stepping down into lower cell.\n");
			nz--;
		} else {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not stepping up or down.\n");
		}
		heightChange = RT_FLOOR(map, actorSize, nx, ny, nz) - RT_FLOOR(map, actorSize, x, y, z) + (nz - z) * CELL_HEIGHT;

		/* If the actor tries to fall more than falling_height, then prohibit the move. */
		if (heightChange < -fallingHeight && !hasLadderSupport) {
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Too far a drop without a ladder. change:%i maxfall:%i\n", heightChange, -fallingHeight);
			return;
		}

		/* If we are walking normally, we can fall if we move into a cell that does not
		 * have its STEPDOWN flag set and has a negative floor:
		 * Set heightChange to 0.
		 * The actor enters the cell.
		 * The actor will be forced to fall (dir 13) from the destination cell to the cell below. */
		if (RT_FLOOR(map, actorSize, nx, ny, nz) < 0) {
			/* We cannot fall if STEPDOWN is defined. */
			if (stepup & PATHFINDING_BIG_STEPDOWN) {
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: There is stepdown from here.\n");
				return;
			}
			/* We cannot fall if there is an entity below the cell we want to move to. */
			if (Grid_CheckForbidden(exclude, actorSize, path, nx, ny, nz - 1)) {
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: The fall destination is occupied.\n");
				return;
			}
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Preparing for a fall. change:%i fall:%i\n", heightChange, -actorStepupHeight);
			heightChange = 0;
			nz--;
		}

	}
	/* else there is no movement that uses passages. */
	/* If we are falling, the height difference is the floor value. */
	else if (dir == DIRECTION_FALL) {
		if (flier) {
			/* Fliers cannot fall intentionally. */
			return;
		} else if (RT_FLOOR(map, actorSize, x, y, z) >= 0) {
			/* We cannot fall if there is a floor in this cell. */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fall while supported. floor:%i\n", RT_FLOOR(map, actorSize, x, y, z));
			return;
		} else if (hasLadderSupport) {
			/* The actor can't fall if there is ladder support. */
			return;
		}
	} else if (dir == DIRECTION_CLIMB_UP) {
		if (flier && QuantToModel(RT_CEILING(map, actorSize, x, y, z)) < UNIT_HEIGHT * 2 - PLAYER_HEIGHT) { /* up */
			Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Not enough headroom to fly up. passage:%i actor:%i\n", QuantToModel(RT_CEILING(map, actorSize, x, y, z)), UNIT_HEIGHT * 2 - PLAYER_HEIGHT);
			return;
		}
		/* If the actor is not a flyer and tries to move up, there must be a ladder. */
		if (dir == DIRECTION_CLIMB_UP && !hasLadderClimb) {
			return;
		}
	} else if (dir == DIRECTION_CLIMB_DOWN) {
		if (flier) {
			if (RT_FLOOR(map, actorSize, x, y, z) >= 0 ) { /* down */
				Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Can't fly down through a floor. floor:%i\n", RT_FLOOR(map, actorSize, x, y, z));
				return;
			}
		} else {
			/* If the actor is not a flyer and tries to move down, there must be a ladder. */
			if (!hasLadderClimb) {
				return;
			}
		}
	}

	/* OK, at this point we are certain of a few things:
	 * There is not a wall obstructing access to the destination cell.
	 * If the actor is not a flier, the actor will not rise more than actor_stepup_height or fall more than
	 *    falling_height, unless climbing.
	 *
	 * If the actor is a flier, as long as there is a passage, it can be moved through.
	 * There are no floor difference restrictions for fliers, only obstructions. */

	/* nz can't move out of bounds */
	if (nz < 0)
		nz = 0;
	if (nz >= PATHFINDING_HEIGHT)
		nz = PATHFINDING_HEIGHT - 1;

	/* Is this a better move into this cell? */
	RT_AREA_TEST(path, nx, ny, nz, crouchingState);
	if (RT_AREA(path, nx, ny, nz, crouchingState) <= len) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: This move is not optimum. %i %i\n", RT_AREA(path, nx, ny, nz, crouchingState), len);
		return;
	}

	/* Test for forbidden (by other entities) areas. */
	if (Grid_CheckForbidden(exclude, actorSize, path, nx, ny, nz)) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: That spot is occupied.\n");
		return;
	}

	/* Store move. */
	if (pqueue) {
		Grid_SetMoveData(path, nx, ny, nz, crouchingState, len, dir, x, y, z, crouchingState, pqueue);
	}
	Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Set move to (%i %i %i) c:%i to %i. srcfloor:%i\n", nx, ny, nz, crouchingState, len, RT_FLOOR(map, actorSize, x, y, z));
}


/**
 * @brief Recalculate the pathing table for the given actor(-position)
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize The size of thing to calc the move for (e.g. size=2 means 2x2).
 * The plan is to have the 'origin' in 2x2 units in the bottom-left (towards the lower coordinates) corner of the 2x2 square.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] from The position to start the calculation from.
 * @param[in] distance The maximum TUs away from 'from' to calculate move-information for
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] fb_list Forbidden list (entities are standing at those points)
 * @param[in] fb_length Length of forbidden list
 * @sa Grid_MoveMark
 * @sa G_MoveCalc
 * @sa CL_ConditionalMoveCalc
 */
void Grid_MoveCalc (const routing_t *map, const actorSizeEnum_t actorSize, pathing_t *path, const pos3_t from, byte crouchingState, int distance, byte ** fb_list, int fb_length)
{
	int dir;
	int count;
	priorityQueue_t pqueue;
	pos4_t epos; /**< Extended position; includes crouching state */
	pos3_t pos;
	/* this is the position of the current actor- so the actor can stand in the cell it is in when pathfinding */
	pos3_t excludeFromForbiddenList;

	/* reset move data */
	memset(path->area, ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT * ACTOR_MAX_STATES);
	memset(path->areaFrom, ROUTING_NOT_REACHABLE, PATHFINDING_WIDTH * PATHFINDING_WIDTH * PATHFINDING_HEIGHT * ACTOR_MAX_STATES);
	path->fblist = fb_list;
	path->fblength = fb_length;

	if (distance > MAX_ROUTE + 3)	/* +3 is added to calc at least one square (diagonal) more */
		distance = MAX_ROUTE + 3;	/* and later show one step beyond the walkable path in red */

	/* Prepare exclusion of starting-location (i.e. this should be ent-pos or le-pos) in Grid_CheckForbidden */
	VectorCopy(from, excludeFromForbiddenList);

	PQueueInitialise(&pqueue, 1024);
	Vector4Set(epos, from[0], from[1], from[2], crouchingState);
	PQueuePush(&pqueue, epos, 0);

	/* Confirm bounds */
	assert((from[2]) < PATHFINDING_HEIGHT);
	assert(crouchingState == 0 || crouchingState == 1);	/* s.a. ACTOR_MAX_STATES */

	RT_AREA(path, from[0], from[1], from[2], crouchingState) = 0;

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Start at (%i %i %i) c:%i\n", from[0], from[1], from[2], crouchingState);

	count = 0;
	while (!PQueueIsEmpty(&pqueue)) {
		PQueuePop(&pqueue, epos);
		VectorCopy(epos, pos);
		count++;

		/* for A*
		if pos = goal
			return pos
		*/
		/**< if reaching that square already took too many TUs,
		 * don't bother to reach new squares *from* there. */
		if (RT_AREA(path, pos[0], pos[1], pos[2], crouchingState) >= distance)
			continue;

		for (dir = 0; dir < PATHFINDING_DIRECTIONS; dir++) {
			Grid_MoveMark(map, excludeFromForbiddenList, actorSize, path, pos, epos[3], dir, &pqueue);
		}
	}
	/* Com_Printf("Loop: %i", count); */
	PQueueFree(&pqueue);

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Done\n\n");
}

/**
 * @brief Caches the calculated move
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @sa AI_ActorThink
 */
void Grid_MoveStore (pathing_t *path)
{
	memcpy(path->areaStored, path->area, sizeof(path->areaStored));
}


/**
 * @brief Return the needed TUs to walk to a given position
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @param[in] to Position to walk to
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] stored Use the stored mask (the cached move) of the routing data
 * @return ROUTING_NOT_REACHABLE if the move isn't possible
 * @return length of move otherwise (TUs)
 */
pos_t Grid_MoveLength (const pathing_t *path, const pos3_t to, byte crouchingState, qboolean stored)
{
#ifdef PARANOID
	if (to[2] >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveLength: WARNING to[2] = %i(>= HEIGHT)\n", to[2]);
		return ROUTING_NOT_REACHABLE;
	}
#endif
	/* Confirm bounds */
	assert(to[2] < PATHFINDING_HEIGHT);
	assert(crouchingState == 0 || crouchingState == 1);	/* s.a. ACTOR_MAX_STATES */

	if (!stored)
		return RT_AREA(path, to[0], to[1], to[2], crouchingState);
	else
		return RT_SAREA(path, to[0], to[1], to[2], crouchingState);
}


/**
 * @brief Get the direction to use to move to a position (used to reconstruct the path)
 * @param[in] path Pointer to client or server side pathing table (le->PathMap, svPathMap)
 * @param[in] toPos The desired location
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @return a direction index (see dvecs and DIRECTIONS)
 * @sa Grid_MoveCheck
 */
int Grid_MoveNext (const pathing_t *path, const pos3_t toPos, byte crouchingState)
{
	const pos_t l = RT_AREA(path, toPos[0], toPos[1], toPos[2], crouchingState); /**< Get TUs for this square */

	/* Check to see if the TUs needed to move here are greater than 0 and less then ROUTING_NOT_REACHABLE */
	if (!l || l == ROUTING_NOT_REACHABLE) {
		/* ROUTING_UNREACHABLE means, not possible/reachable */
		return ROUTING_UNREACHABLE;
	}

	/* Return the information indicating how the actor got to this cell */
	return RT_AREA_FROM(path, toPos[0], toPos[1], toPos[2], crouchingState);
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
unsigned int Grid_Ceiling (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return QuantToModel(RT_CEILING(map, actorSize, pos[0], pos[1], pos[2] & 7));
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
int Grid_Height (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Height: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return QuantToModel(RT_CEILING(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1))
		- RT_FLOOR(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1)));
}


/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's floor.
 */
int Grid_Floor (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_Floor: Warning: z level is bigger than %i: %i\n",
			(PATHFINDING_HEIGHT - 1), pos[2]);
	}
	return QuantToModel(RT_FLOOR(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1)));
}


/**
 * @brief Returns the maximum height of an obstruction that an actor can travel over.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @param[in] dir the direction in which we are moving
 * @return The actual model height increase needed to move into an adjacent cell.
 */
pos_t Grid_StepUp (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos, const int dir)
{
	/* max 8 levels */
	if (pos[2] >= PATHFINDING_HEIGHT) {
		Com_Printf("Grid_StepUp: Warning: z level is bigger than 7: %i\n", pos[2]);
	}
	return QuantToModel(RT_STEPUP(map, actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1), dir));
}


/**
 * @brief Returns the amounts of TUs that are needed to perform one step into the given direction.
 * @param[in] dir the direction in which we are moving
 * @return The TUs needed to move there.
 */
int Grid_GetTUsForDirection (int dir)
{
	assert(dir >= 0 && dir < PATHFINDING_DIRECTIONS);
	return TUsUsed[dir];
}


/**
 * @brief Returns non-zero if the cell is filled (solid) and cannot be entered.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check for filling
 * @return 0 if the cell is vacant (of the world model), non-zero otherwise.
 */
int Grid_Filled (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	/* max 8 levels */
	assert(pos[2] < PATHFINDING_HEIGHT);
	return RT_FILLED(map, pos[0], pos[1], pos[2], actorSize);
}


/**
 * @brief Calculated the new height level when something falls down from a certain position.
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to start the fall (starting height is the z-value in this position)
 * @param[in] actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @return New z (height) value.
 * @return 0xFF if an error occurred.
 */
pos_t Grid_Fall (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	int z = pos[2], base, diff;
	qboolean flier = qfalse; /** @todo if an actor can fly, then set this to true. */

	/* Is z off the map? */
	if (z >= PATHFINDING_HEIGHT) {
		Com_DPrintf(DEBUG_PATHING, "Grid_Fall: z (height) out of bounds): z=%i max=%i\n", z, PATHFINDING_HEIGHT);
		return 0xFF;
	}

	/* If we can fly, then obviously we won't fall. */
	if (flier)
		return z;

	/* Easy math- get the floor, integer divide by CELL_HEIGHT, add to z.
	 * If z < 0, we are going down.
	 * If z >= CELL_HEIGHT, we are going up.
	 * If 0 <= z <= CELL_HEIGHT, then z / 16 = 0, no change. */
	base = RT_FLOOR(map, actorSize, pos[0], pos[1], z);
	/* Hack to deal with negative numbers- otherwise rounds toward 0 instead of down. */
	diff = base < 0 ? (base - (CELL_HEIGHT - 1)) / CELL_HEIGHT : base / CELL_HEIGHT;
	z += diff;
	/* The tracing code will set locations without a floor to -1.  Compensate for that. */
	if (z < 0)
		z = 0;
	else if (z >= PATHFINDING_HEIGHT)
		z = PATHFINDING_HEIGHT - 1;
	return z;
}

/**
 * @brief Converts a grid position to world coordinates
 * @sa Grid_Height
 * @param[in] map The routing map
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos The grid position
 * @param[out] vec The world vector
 */
void Grid_PosToVec (const routing_t *map, const actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec)
{
	SizedPosToVec(pos, actorSize, vec);
#ifdef PARANOID
	if (pos[2] >= PATHFINDING_HEIGHT)
		Com_Printf("Grid_PosToVec: Warning - z level bigger than 7 (%i - source: %.02f)\n", pos[2], vec[2]);
#endif
	/* Clamp the floor value between 0 and UNIT_HEIGHT */
	vec[2] += max(0, min(UNIT_HEIGHT, Grid_Floor(map, actorSize, pos)));
}


/**
 * @brief This function recalculates the routing in the box bounded by min and max.
 * @sa CMod_LoadRouting
 * @sa Grid_RecalcRouting
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] map The routing map (either server or client map)
 * @param[in] min The lower extents of the box to recalc routing for
 * @param[in] max The upper extents of the box to recalc routing for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void Grid_RecalcBoxRouting (mapTiles_t *mapTiles, routing_t *map, const pos3_t min, const pos3_t max, const char **list)
{
	int x, y, z, actorSize, dir;

	Com_DPrintf(DEBUG_PATHING, "rerouting (%i %i %i) (%i %i %i)\n",
		(int)min[0], (int)min[1], (int)min[2],
		(int)max[0], (int)max[1], (int)max[2]);

	/* check unit heights */
	for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) {
		const int maxY = max[1] + actorSize;
		const int maxX = max[0] + actorSize;
		/* Offset the initial X and Y to compensate for larger actors when needed. */
		for (y = max(min[1] - actorSize + 1, 0); y < maxY; y++) {
			for (x = max(min[0] - actorSize + 1, 0); x < maxX; x++) {
				/** @note RT_CheckCell goes from top (7) to bottom (0) */
				for (z = max[2]; z >= 0; z--) {
					const int newZ = RT_CheckCell(mapTiles, map, actorSize, x, y, z, list);
					assert(newZ <= z);
					z = newZ;
				}
			}
		}
	}

	/* check connections */
	for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) {
		const int minX = max(min[0] - actorSize, 0);
		const int minY = max(min[1] - actorSize, 0);
		const int maxX = min(max[0] + actorSize, PATHFINDING_WIDTH - 1);
		const int maxY = min(max[1] + actorSize, PATHFINDING_WIDTH - 1);
		/* Offset the initial X and Y to compensate for larger actors when needed.
		 * Also sweep further out to catch the walls back into our box. */
		for (y = minY; y <= maxY; y++) {
			for (x = minX; x <= maxX; x++) {
				for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
					/** @note The new version of RT_UpdateConnectionColumn can work bidirectional, so we can
					 * trace every other dir, unless we are on the edge. */
#if RT_IS_BIDIRECTIONAL == 1
					if ((dir & 1) && x != minX && x != maxX && y != minY && y != maxY)
						continue;
#endif
					RT_UpdateConnectionColumn(mapTiles, map, actorSize, x, y, dir, list);
				}
			}
		}
	}
}


/**
 * @brief This function recalculates the routing surrounding the entity name.
 * @sa CM_InlineModel
 * @sa CM_CheckUnit
 * @sa CM_UpdateConnection
 * @sa CMod_LoadSubmodels
 * @sa Grid_RecalcBoxRouting
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] map The routing map (either server or client map)
 * @param[in] name Name of the inline model to compute the mins/maxs for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void Grid_RecalcRouting (mapTiles_t *mapTiles, routing_t *map, const char *name, const char **list)
{
	const cBspModel_t *model;
	pos3_t min, max;
	unsigned int i;
	double start, end;

	start = time(NULL);

	/* get inline model, if it is one */
	if (*name != '*') {
		Com_Printf("Called Grid_RecalcRouting with no inline model\n");
		return;
	}
	model = CM_InlineModel(mapTiles, name);
	if (!model) {
		Com_Printf("Called Grid_RecalcRouting with invalid inline model name '%s'\n", name);
		return;
	}

	Com_DPrintf(DEBUG_PATHING, "Model:%s origin(%f,%f,%f) angles(%f,%f,%f) mins(%f,%f,%f) maxs(%f,%f,%f)\n", name,
		model->origin[0], model->origin[1], model->origin[2],
		model->angles[0], model->angles[1], model->angles[2],
		model->mins[0], model->mins[1], model->mins[2],
		model->maxs[0], model->maxs[1], model->maxs[2]);

	/* get the target model's dimensions */
	if (VectorNotEmpty(model->angles)) {
		vec3_t minVec, maxVec;
		vec3_t centerVec, halfVec, newCenterVec;
		vec3_t m[3];

		/* Find the center of the extents. */
		VectorCenterFromMinsMaxs(model->mins, model->maxs, centerVec);

		/* Find the half height and half width of the extents. */
		VectorSubtract(model->maxs, centerVec, halfVec);

		/* Rotate the center about the origin. */
		VectorCreateRotationMatrix(model->angles, m);
		VectorRotate(m, centerVec, newCenterVec);

		/* Set minVec and maxVec to bound around newCenterVec at halfVec size. */
		VectorSubtract(newCenterVec, halfVec, minVec);
		VectorAdd(newCenterVec, halfVec, maxVec);

		/* Now offset by origin then convert to position (Doors do not have 0 origins) */
		VectorAdd(minVec, model->origin, minVec);
		VecToPos(minVec, min);
		VectorAdd(maxVec, model->origin, maxVec);
		VecToPos(maxVec, max);
	} else {  /* normal */
		vec3_t temp;
		/* Now offset by origin then convert to position (Doors do not have 0 origins) */
		VectorAdd(model->mins, model->origin, temp);
		VecToPos(temp, min);
		VectorAdd(model->maxs, model->origin, temp);
		VecToPos(temp, max);
	}

	/* fit min/max into the world size */
	max[0] = min(max[0] + 1, PATHFINDING_WIDTH - 1);
	max[1] = min(max[1] + 1, PATHFINDING_WIDTH - 1);
	max[2] = min(max[2] + 1, PATHFINDING_HEIGHT - 1);
	for (i = 0; i < 3; i++)
		min[i] = max(min[i] - 1, 0);

	/* We now have the dimensions, call the generic rerouting function. */
	Grid_RecalcBoxRouting(mapTiles, map, min, max, list);

	end = time(NULL);
	Com_DPrintf(DEBUG_ROUTING, "Retracing for model %s between (%i, %i, %i) and (%i, %i %i) in %5.1fs\n",
			name, min[0], min[1], min[2], max[0], max[1], max[2], end - start);
}
