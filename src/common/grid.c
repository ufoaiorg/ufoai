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
			continue;
		}

		forbiddenSize = *(p + 1);
		memcpy(&size, forbiddenSize, sizeof(size));
		fx = (*p)[0];
		fy = (*p)[1];
		fz = (*p)[2];

		if (fx + size <= x || x + actorSize <= fx)
			continue; /* x bounds do not intersect */
		if (fy + size <= y || y + actorSize <= fy)
			continue; /* y bounds do not intersect */
		if (z == fz) {
			return qtrue; /* confirmed intersection */
		}
	}
	return qfalse;
}

static void Grid_SetMoveData (pathing_t *path, const pos3_t toPos, const int c, const byte length, const int dir, const int oz, const int oc, priorityQueue_t *pqueue)
{
	pos4_t dummy;

	RT_AREA_TEST_POS(path, toPos, c);
	RT_AREA_POS(path, toPos, c) = length;	/**< Store TUs for this square. */
	RT_AREA_FROM_POS(path, toPos, c) = makeDV(dir, oz); /**< Store origination information for this square. */

	Vector4Set(dummy, toPos[0], toPos[1], toPos[2], c);
	/** @todo add heuristic for A* algorithm */
	PQueuePush(pqueue, dummy, length);
}

/**
 * @brief a struct holding the relevant data to check if we can move between two adjacent cells
 */
typedef struct step_s {
	const routing_t *map;
	qboolean flier;

	/** @todo has_ladder_climb should return true if
	 *  1) There is a ladder in the new cell in the specified direction. */
	qboolean hasLadderToClimb;	/**< Indicates if there is a ladder present providing ability to climb. */

	/** @todo has_ladder_support should return true if
	 *  1) There is a ladder in the new cell in the specified direction or
	 *  2) There is a ladder in any direction in the cell below the new cell and no ladder in the new cell itself. */
	qboolean hasLadderSupport;	/**< Indicates if there is a ladder present providing support. */

	actorSizeEnum_t actorSize;
	int actorHeight;		/**< The actor's height in QUANT units. */
	int actorCrouchedHeight;

} step_t;

/**
 * @brief Initialize the step_t data
 * @param[in] step The struct describing the move
 * @param[in] map Pointer to client or server side routing table (clMap, svMap)
 * @param[in] actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @return false if dir is irrelevant or something went wrong
 */
static qboolean Grid_StepInit (step_t *step, const routing_t *map, const actorSizeEnum_t actorSize, const byte crouchingState, const int dir)
{
	step->map = map;
	/** @todo flier should return true if the actor can fly. */
	step->flier = qfalse; /**< This can be keyed into whether an actor can fly or not to allow flying */
	step->hasLadderToClimb = qfalse;
	step->hasLadderSupport = qfalse;
	step->actorSize = actorSize;
	/** @note This is the actor's height in QUANT units. */
	/** @todo actor_height is currently the fixed height of a 1x1 actor.  This needs to be adjusted
	 *  to the actor's actual height. */
	step->actorHeight = ModelCeilingToQuant((float)(crouchingState ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT)); /**< The actor's height */
	step->actorCrouchedHeight = ModelCeilingToQuant((float)(PLAYER_CROUCHING_HEIGHT));

	/* Ensure that dir is in bounds. */
	assert(dir >= 0 && dir < PATHFINDING_DIRECTIONS);

	/* IMPORTANT: only fliers can use directions higher than NON_FLYING_DIRECTIONS. */
	if (!step->flier && dir >= FLYING_DIRECTIONS) {
		return qfalse;
	}

	/* We cannot fly and crouch at the same time. This will also cause an actor to stand to fly. */
	if (crouchingState && dir >= FLYING_DIRECTIONS) {
		return qfalse;
	}

	return qtrue;
}


/**
 * @brief Calculate the cell the we end up in if moving in the give dir
 * @param[in] step The struct describing the move
 * @param[in] pos Current location in the map.
 * @param[in] toPos The position we are moving to with this step.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @return false if we can't fly there
 */
static qboolean Grid_StepCalcNewPos (step_t *step, const pos3_t pos, pos3_t toPos, const int dir)
{
	toPos[0] = pos[0] + dvecs[dir][0];	/**< "new" x value = starting x value + difference from chosen direction */
	toPos[1] = pos[1] + dvecs[dir][1];	/**< "new" y value = starting y value + difference from chosen direction */
	toPos[2] = pos[2] + dvecs[dir][2];	/**< "new" z value = starting z value + difference from chosen direction */

	/* Connection checks.  If we cannot move in the desired direction, then bail. */
	/* Range check of new values (all sizes) */
	/* "comparison is always false due to limited range of data type" */
	/* Only activate this check if PATHFINDING_WIDTH or pos3_t changes */
/*	if (toPos[0] < 0 || toPos[0] > PATHFINDING_WIDTH - actorSize
	 || toPos[1] < 0 || toPos[1] > PATHFINDING_WIDTH - actorSize
	 || toPos[2] < 0  {
		return qfalse;
	} */
	if (toPos[2] > PATHFINDING_HEIGHT) {
		return qfalse;
	}
	return qtrue;
}

/**
 * @brief Checks if we can walk in the given direction
 * First test for opening height availablilty. Then test for stepup compatibility. Last test for fall.
 * @note Fliers use this code only when they are walking.
 * @param[in] step The struct describing the move
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @param[in] pos Current location in the map.
 * @param[in] toPos The position we are moving to with this step.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @return false if we can't fly there
 */
static qboolean Grid_StepCheckWalkingDirections (step_t *step, pathing_t *path, const pos3_t pos, pos3_t toPos, const int dir, const byte crouchingState)
{
	int nx, ny, nz;
	int passageHeight;
	const actorSizeEnum_t actorSize = step->actorSize;
	/** @todo falling_height should be replaced with an arbitrary max falling height based on the actor. */
	const int fallingHeight = PATHFINDING_MAX_FALL;/**<This is the maximum height that an actor can fall. */
	const int stepup = RT_STEPUP_POS(step->map, actorSize, pos, dir); /**< The stepup needed to get to/through the passage */
	const int stepupHeight = stepup & ~(PATHFINDING_BIG_STEPDOWN | PATHFINDING_BIG_STEPUP); /**< The actual stepup height without the level flags */
	int heightChange;
	/** @todo actor_stepup_height should be replaced with an arbitrary max stepup height based on the actor. */
	int actorStepupHeight = PATHFINDING_MAX_STEPUP;

	/* This is the standard passage height for all units trying to move horizontally. */
	RT_CONN_TEST_POS(step->map, actorSize, pos, dir);
	passageHeight = RT_CONN_POS(step->map, actorSize, pos, dir);
	if (passageHeight < step->actorHeight) {
#if 0
/** I know this code could be streamlined, but until I understand it myself, plz leave it like it is !*/
		int dvFlagsNew = 0;
		if (!crouchingState									/* not in std crouch mode */
		 && passageHeight >= step->actorCrouchedHeight)	{	/* and passage is tall enough for crouching ? */
															/* we should try autocrouching */
			int dvFlagsOld = getDVflags(RT_AREA_POS(path, pos, crouchingState));
			int toHeight = RT_CEILING_POS(step->map, actorSize, toPos) - RT_FLOOR_POS(step->map, actorSize, toPos);
			int tuCr = Grid_GetTUsForDirection(dir, 1);		/* 1 means crouched */
			int newTUs = 0;

			if (toHeight >= step->actorHeight) {			/* can we stand in the new cell ? */
				if ((dvFlagsOld & DV_FLAG_AUTOCROUCH)		/* already in auto-crouch mode ? */
				 || (dvFlagsOld & DV_FLAG_AUTOCROUCHED)) {
					dvFlagsNew |= DV_FLAG_AUTOCROUCHED;		/* keep that ! */
					newTUs = tuCr + TU_CROUCH;				/* TUs for crouching plus getting up */
				}
				else {
					dvFlagsNew |= DV_FLAG_AUTODIVE;
					newTUs = tuCr + 2 * TU_CROUCH;			/* TUs for crouching plus getting down and up */
				}
			}
			else {											/* we can't stand there */
				if (dvFlagsOld & DV_FLAG_AUTOCROUCHED) {
					dvFlagsNew |= DV_FLAG_AUTOCROUCHED;		/* keep that ! */
					newTUs = tuCr;							/* TUs just for crouching */
				}
				else {
					dvFlagsNew |= DV_FLAG_AUTOCROUCH;		/* get down ! */
					newTUs = tuCr + TU_CROUCH;				/* TUs for crouching plus getting down */
				}
			}
		}
		else
#endif
			return qfalse;	/* Passage is not tall enough. */
	}

	/* If we are moving horizontally, use the stepup requirement of the floors.
	 * The new z coordinate may need to be adjusted from stepup.
	 * Also, actor_stepup_height must be at least the cell's positive stepup value to move that direction. */
	/* If the actor cannot reach stepup, then we can't go this way. */
	if (actorStepupHeight < stepupHeight) {
		return qfalse;	/* Actor cannot stepup high enough. */
	}

	nx = toPos[0];
	ny = toPos[1];
	nz = toPos[2];

	if ((stepup & PATHFINDING_BIG_STEPUP) && toPos[2] < PATHFINDING_HEIGHT - 1) {
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Stepping up into higher cell.\n");
		toPos[2]++;
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
	} else if ((stepup & PATHFINDING_BIG_STEPDOWN) && toPos[2] > 0
		&& actorStepupHeight >= (RT_STEPUP(step->map, actorSize, nx, ny, nz - 1, dir ^ 1) & ~(PATHFINDING_BIG_STEPDOWN | PATHFINDING_BIG_STEPUP))) {
		toPos[2]--;		/* Stepping down into lower cell. */
	}

	heightChange = RT_FLOOR_POS(step->map, actorSize, toPos) - RT_FLOOR_POS(step->map, actorSize, pos) + (toPos[2] - pos[2]) * CELL_HEIGHT;

	/* If the actor tries to fall more than falling_height, then prohibit the move. */
	if (heightChange < -fallingHeight && !step->hasLadderSupport) {
		return qfalse;		/* Too far a drop without a ladder. */
	}

	/* If we are walking normally, we can fall if we move into a cell that does not
	 * have its STEPDOWN flag set and has a negative floor:
	 * Set heightChange to 0.
	 * The actor enters the cell.
	 * The actor will be forced to fall (dir 13) from the destination cell to the cell below. */
	if (RT_FLOOR_POS(step->map, actorSize, toPos) < 0) {
		/* We cannot fall if STEPDOWN is defined. */
		if (stepup & PATHFINDING_BIG_STEPDOWN) {
			return qfalse;		/* There is stepdown from here. */
		}
		Com_DPrintf(DEBUG_PATHING, "Grid_MoveMark: Preparing for a fall. change:%i fall:%i\n", heightChange, -actorStepupHeight);
		heightChange = 0;
		toPos[2]--;
	}
	return qtrue;
}

/**
 * @brief Checks if we can move in the given flying direction
 * @param[in] step The struct describing the move
 * @param[in] pos Current location in the map.
 * @param[in] toPos The position we are moving to with this step.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @return false if we can't fly there
 */
static qboolean Grid_StepCheckFlyingDirections (step_t *step, const pos3_t pos, const pos3_t toPos, const int dir)
{
	const int coreDir = dir % CORE_DIRECTIONS;	/**< The compass direction of this flying move */
	int neededHeight;
	int passageHeight;
	const actorSizeEnum_t actorSize = step->actorSize;

	if (toPos[2] > pos[2]) {
		/* If the actor is moving up, check the passage at the current cell.
		 * The minimum height is the actor's height plus the distance from the current floor to the top of the cell. */
		neededHeight = step->actorHeight + CELL_HEIGHT - max(0, RT_FLOOR_POS(step->map, actorSize, pos));
		RT_CONN_TEST_POS(step->map, actorSize, pos, coreDir);
		passageHeight = RT_CONN_POS(step->map, actorSize, pos, coreDir);
	} else if (toPos[2] < pos[2]) {
		/* If the actor is moving down, check from the destination cell back. *
		 * The minimum height is the actor's height plus the distance from the destination floor to the top of the cell. */
		neededHeight = step->actorHeight + CELL_HEIGHT - max(0, RT_FLOOR_POS(step->map, actorSize, toPos));
		RT_CONN_TEST_POS(step->map, actorSize, toPos, coreDir ^ 1);
		passageHeight = RT_CONN_POS(step->map, actorSize, toPos, coreDir ^ 1);
	} else {
		neededHeight = step->actorHeight;
		RT_CONN_TEST_POS(step->map, actorSize, pos, coreDir);
		passageHeight = RT_CONN_POS(step->map, actorSize, pos, coreDir);
	}
	if (passageHeight < neededHeight) {
		return qfalse;
	}
	return qtrue;
}

/**
 * @brief Checks if we can move in the given vertical direction
 * @param[in] step The struct describing the move
 * @param[in] pos Current location in the map.
 * @param[in] dir Direction vector index (see DIRECTIONS and dvecs)
 * @return false if we can't move there
 */
static qboolean Grid_StepCheckVerticalDirections (step_t *step, const pos3_t pos, const int dir)
{
	if (dir == DIRECTION_FALL) {
		if (step->flier) {
			/* Fliers cannot fall intentionally. */
			return qfalse;
		} else if (RT_FLOOR_POS(step->map, step->actorSize, pos) >= 0) {
			/* We cannot fall if there is a floor in this cell. */
			return qfalse;
		} else if (step->hasLadderSupport) {
			/* The actor can't fall if there is ladder support. */
			return qfalse;
		}
	} else if (dir == DIRECTION_CLIMB_UP) {
		if (step->flier && QuantToModel(RT_CEILING_POS(step->map, step->actorSize, pos)) < UNIT_HEIGHT * 2 - PLAYER_HEIGHT) { /* Not enough headroom to fly up. */
			return qfalse;
		}
		/* If the actor is not a flyer and tries to move up, there must be a ladder. */
		if (dir == DIRECTION_CLIMB_UP && !step->hasLadderToClimb) {
			return qfalse;
		}
	} else if (dir == DIRECTION_CLIMB_DOWN) {
		if (step->flier) {
			if (RT_FLOOR_POS(step->map, step->actorSize, pos) >= 0 ) { /* Can't fly down through a floor. */
				return qfalse;
			}
		} else {
			/* If the actor is not a flyer and tries to move down, there must be a ladder. */
			if (!step->hasLadderToClimb) {
				return qfalse;
			}
		}
	}
	return qtrue;
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
	step_t step_;
	step_t *step = &step_;	/* temporary solution */
	pos3_t toPos;
	byte TUsSoFar, TUsForMove, TUsAfter;

	if (!Grid_StepInit(step, map, actorSize, crouchingState, dir))
		return;		/* either dir is irrelevant or something worse happened */

	TUsSoFar = RT_AREA_POS(path, pos, crouchingState);
	/* Find the number of TUs used (normally) to move in this direction. */
	TUsForMove = Grid_GetTUsForDirection(dir, crouchingState);

	/* calculate the position we would normally end up if moving in the given dir. */
	if (!Grid_StepCalcNewPos(step, pos, toPos, dir)) {
		return;
	}
	/* If there is no passageway (or rather lack of a wall) to the desired cell, then return. */
	/* If the flier is moving up or down diagonally, then passage height will also adjust */
	if (dir >= FLYING_DIRECTIONS) {
		if (!Grid_StepCheckFlyingDirections(step, pos, toPos, dir)) {
			return;
		}
	} else if (dir < CORE_DIRECTIONS) {
		/** note that this function may modify toPos ! */
		if (!Grid_StepCheckWalkingDirections(step, path, pos, toPos, dir, crouchingState)) {
			return;
		}
	} else {
		/* else there is no movement that uses passages. */
		/* If we are falling, the height difference is the floor value. */
		if (!Grid_StepCheckVerticalDirections(step, pos, dir)) {
			return;
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
	if (toPos[2] >= PATHFINDING_HEIGHT)
		toPos[2] = PATHFINDING_HEIGHT - 1;

	/* Now add the TUs needed to get to the originating cell. */
	TUsAfter = TUsSoFar + TUsForMove;

	/* Is this a better move into this cell? */
	RT_AREA_TEST_POS(path, toPos, crouchingState);
	if (RT_AREA_POS(path, toPos, crouchingState) <= TUsAfter) {
		return;	/* This move is not optimum. */
	}

	/* Test for forbidden (by other entities) areas. */
	if (Grid_CheckForbidden(exclude, actorSize, path, toPos[0], toPos[1], toPos[2])) {
		return;		/* That spot is occupied. */
	}

	/* Store move. */
	if (pqueue) {
		Grid_SetMoveData(path, toPos, crouchingState, TUsAfter, dir, pos[2], crouchingState, pqueue);
	}
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
	OBJSET(path->area,     ROUTING_NOT_REACHABLE);
	OBJSET(path->areaFrom, ROUTING_NOT_REACHABLE);
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

	/* set starting position to 0 TUs.*/
	RT_AREA_POS(path, from, crouchingState) = 0;

	Com_DPrintf(DEBUG_PATHING, "Grid_MoveCalc: Start at (%i %i %i) c:%i\n", from[0], from[1], from[2], crouchingState);

	count = 0;
	while (!PQueueIsEmpty(&pqueue)) {
		byte TUsSoFar;
		PQueuePop(&pqueue, epos);
		VectorCopy(epos, pos);
		count++;

		/* if reaching that square already took too many TUs,
		 * don't bother to reach new squares *from* there. */
		TUsSoFar = RT_AREA_POS(path, pos, crouchingState);
		if (TUsSoFar >= distance || TUsSoFar >= MAX_MOVELENGTH)
			continue;

		for (dir = 0; dir < PATHFINDING_DIRECTIONS; dir++) {
			/* Directions 12, 14, and 15 are currently undefined. */
			if (dir == 12 || dir == 14 || dir == 15)
				continue;
			/* If this is a crouching or crouching move, forget it. */
			if (dir == DIRECTION_STAND_UP || dir == DIRECTION_CROUCH)
				continue;

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
		return RT_AREA_POS(path, to, crouchingState);
	else
		return RT_SAREA(path, to[0], to[1], to[2], crouchingState);
}


/**
 * @brief Get the direction to use to move to a position (used to reconstruct the path)
 * @param[in] path Pointer to client or server side pathing table (le->PathMap, svPathMap)
 * @param[in] toPos The desired location
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @return a direction vector (see dvecs and DIRECTIONS)
 * @sa Grid_MoveCheck
 */
int Grid_MoveNext (const pathing_t *path, const pos3_t toPos, byte crouchingState)
{
	const pos_t l = RT_AREA_POS(path, toPos, crouchingState); /**< Get TUs for this square */

	/* Check to see if the TUs needed to move here are greater than 0 and less then ROUTING_NOT_REACHABLE */
	if (!l || l == ROUTING_NOT_REACHABLE) {
		/* ROUTING_UNREACHABLE means, not possible/reachable */
		return ROUTING_UNREACHABLE;
	}

	/* Return the information indicating how the actor got to this cell */
	return RT_AREA_FROM_POS(path, toPos, crouchingState);
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
 * @param[in] crouched The crouching state of the actor
 * @return The TUs needed to move there.
 */
int Grid_GetTUsForDirection (const int dir, const int crouched)
{
	assert(dir >= 0 && dir < PATHFINDING_DIRECTIONS);
	if (crouched && dir < CORE_DIRECTIONS)
		return TUsUsed[dir] * TU_CROUCH_MOVING_FACTOR;
	else
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
 * @brief This function recalculates the routing in and around the box bounded by min and max.
 * @sa CMod_LoadRouting
 * @sa Grid_RecalcRouting
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] map The routing map (either server or client map)
 * @param[in] min The lower extents of the box to recalc routing for
 * @param[in] max The upper extents of the box to recalc routing for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
static void Grid_RecalcBoxRouting (mapTiles_t *mapTiles, routing_t *map, const pos3_t min, const pos3_t max, const char **list)
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
					/* for places outside the model box, skip dirs that can not be affected by the model */
					if (x > max[0] && dir != 1 && dir != 5 && dir != 6)
						continue;
					if (y > max[1] && dir != 3 && dir != 5 && dir != 7)
						continue;
					if (actorSize == ACTOR_SIZE_NORMAL) {
						if (x < min[0] && dir != 0 && dir != 4 && dir != 7)
							continue;
						if (y < min[1] && dir != 2 && dir != 4 && dir != 6)
							continue;
					} else {
						/* the position of 2x2 actors is their lower left cell */
						if (x < min[0] - 1 && dir != 0 && dir != 4 && dir != 7)
							continue;
						if (y < min[1] - 1 && dir != 2 && dir != 4 && dir != 6)
							continue;
					}
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
	max[0] = min(max[0], PATHFINDING_WIDTH - 1);
	max[1] = min(max[1], PATHFINDING_WIDTH - 1);
	max[2] = min(max[2], PATHFINDING_HEIGHT - 1);
	for (i = 0; i < 3; i++)
		min[i] = max(min[i], 0);

	/* We now have the dimensions, call the generic rerouting function. */
	Grid_RecalcBoxRouting(mapTiles, map, min, max, list);

	end = time(NULL);
	Com_DPrintf(DEBUG_ROUTING, "Retracing for model %s between (%i, %i, %i) and (%i, %i %i) in %5.1fs\n",
			name, min[0], min[1], min[2], max[0], max[1], max[2], end - start);
}
