/**
 * @file
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

#define RT_AREA_POS(path, p, state)					((path)->area[(state)][(p)[2]][(p)[1]][(p)[0]])
#define RT_AREA_FROM_POS(path, p, state)			((path)->areaFrom[(state)][(p)[2]][(p)[1]][(p)[0]])
#define RT_SAREA(path, x, y, z, state)				((path)->areaStored[(state)][(z)][(y)][(x)])
#define RT_AREA_TEST_POS(path, p, state)			assert((p)[2] < PATHFINDING_HEIGHT);\
														assert((state) == 0 || (state) == 1);
														/* assuming p is a pos3_t, we don't need to check for p[n] >= 0 here because it's unsigned.
														 * also we don't need to check against PATHFINDING_WIDTH because it's always greater than a 'byte' type. */

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
* @brief Checks one cell on the grid against the 'forbiddenList' (i.e. cells blocked by other entities).
 * @param[in] exclude Exclude this position from the forbidden list check
 * @param[in] actorSize width of the actor in cells
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @param[in] x,y,z Grid position of the cell
 * @sa G_BuildForbiddenList
 * @sa CL_BuildForbiddenList
 * @return true if one can't walk there (i.e. the field [and attached fields for e.g. 2x2 units] is/are blocked by entries in
 * the forbidden list) otherwise false.
 */
static bool Grid_CheckForbidden (const pos3_t exclude, const actorSizeEnum_t actorSize, pathing_t* path, int x, int y, int z)
{
	pos_t** p;
	int i;
	actorSizeEnum_t size;

	for (i = 0, p = path->fblist; i < path->fblength / 2; i++, p += 2) {
		/* Skip initial position. */
		if (VectorCompare((*p), exclude)) {
			continue;
		}

		/* extract the forbidden coordinates */
		byte* forbiddenSize = *(p + 1);
		memcpy(&size, forbiddenSize, sizeof(size));
		const int fx = (*p)[0];
		const int fy = (*p)[1];
		const int fz = (*p)[2];

		if (fx + size <= x || x + actorSize <= fx)
			continue; /* x bounds do not intersect */
		if (fy + size <= y || y + actorSize <= fy)
			continue; /* y bounds do not intersect */
		if (z == fz)
			return true; /* confirmed intersection */
	}
	return false;
}

static void Grid_SetMoveData (pathing_t* path, const pos3_t toPos, const int crouch, const byte length, const int dir, const int oldZ)
{
	RT_AREA_TEST_POS(path, toPos, crouch);
	RT_AREA_POS(path, toPos, crouch) = length;	/**< Store TUs for this square. */
	RT_AREA_FROM_POS(path, toPos, crouch) = makeDV(dir, oldZ); /**< Store origination information for this square. */
}

/**
 * @brief a struct holding the relevant data to check if we can move between two adjacent cells
 */
class Step {
private:
	/** @todo flier should return true if the actor can fly. */
	bool flier; /**< This can be keyed into whether an actor can fly or not to allow flying */

	/** @todo has_ladder_climb should return true if
	 *  1) There is a ladder in the new cell in the specified direction. */
	bool hasLadderToClimb;	/**< Indicates if there is a ladder present providing ability to climb. */

	/** @todo has_ladder_support should return true if
	 *  1) There is a ladder in the new cell in the specified direction or
	 *  2) There is a ladder in any direction in the cell below the new cell and no ladder in the new cell itself. */
	bool hasLadderSupport;	/**< Indicates if there is a ladder present providing support. */

	int actorHeight;		/**< The actor's height in QUANT units. */
	int actorCrouchedHeight;
	const Routing& routing;
public:
	const int dir;
	pos3_t fromPos;
	pos3_t toPos;	/* The position we are moving to with this step. */
	actorSizeEnum_t actorSize;
	const byte crouchingState;
	byte TUsAfter;

	Step (const Routing& r, const pos3_t fromPos, const actorSizeEnum_t actorSize, const byte crouchingState, const int dir);
	bool init ();
	bool calcNewPos ();
	void calcNewTUs (const pathing_t* path);
	bool checkWalkingDirections (const pathing_t* path);
	bool checkFlyingDirections () const;
	bool checkVerticalDirections () const;
	bool isPossible (const pathing_t* path);
};

/**
 * @brief Constructor
 * @param[in] r Reference to client or server side routing table (clMap, svMap)
 * @param[in] _fromPos Position where we start this step
 * @param[in] _actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @param[in] _crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] _dir Direction vector index (see DIRECTIONS and dvecs)
 */
Step::Step (const Routing& r, const pos3_t _fromPos, const actorSizeEnum_t _actorSize, const byte _crouchingState, const int _dir) :
		flier(false), hasLadderToClimb(false), hasLadderSupport(false), actorHeight(0), actorCrouchedHeight(0), routing(
				r), dir(_dir), actorSize(_actorSize), crouchingState(_crouchingState), TUsAfter(0)
{
	VectorCopy(_fromPos, fromPos);
}

/**
 * @brief Initialize the other Step data
 * @return false if dir is irrelevant or something went wrong
 */
bool Step::init ()
{
	/** @note This is the actor's height in QUANT units. */
	/** @todo actor_height is currently the fixed height of a 1x1 actor.  This needs to be adjusted
	 *  to the actor's actual height. */
	actorHeight = ModelCeilingToQuant((float)(crouchingState ? PLAYER_CROUCHING_HEIGHT : PLAYER_STANDING_HEIGHT)); /**< The actor's height */
	actorCrouchedHeight = ModelCeilingToQuant((float)(PLAYER_CROUCHING_HEIGHT));

	/* Ensure that dir is in bounds. */
	assert(dir >= 0 && dir < PATHFINDING_DIRECTIONS);

	/* IMPORTANT: only fliers can use directions higher than NON_FLYING_DIRECTIONS. */
	if (!flier && dir >= FLYING_DIRECTIONS) {
		return false;
	}

	/* We cannot fly and crouch at the same time. This will also cause an actor to stand to fly. */
	if (crouchingState && dir >= FLYING_DIRECTIONS) {
		return false;
	}

	return true;
}


/**
 * @brief Calculate the cell the we end up in if moving in the give dir
 * @return false if we can't move there
 */
bool Step::calcNewPos (void)
{
	toPos[0] = fromPos[0] + dvecs[dir][0];	/**< "new" x value = starting x value + difference from chosen direction */
	toPos[1] = fromPos[1] + dvecs[dir][1];	/**< "new" y value = starting y value + difference from chosen direction */
	toPos[2] = fromPos[2] + dvecs[dir][2];	/**< "new" z value = starting z value + difference from chosen direction */

	/* Connection checks.  If we cannot move in the desired direction, then bail. */
	/* Range check of new values (all sizes) */
	/* "comparison is always false due to limited range of data type" */
	/* Only activate this check if PATHFINDING_WIDTH or pos3_t changes */
/*	if (toPos[0] < 0 || toPos[0] > PATHFINDING_WIDTH - actorSize
	 || toPos[1] < 0 || toPos[1] > PATHFINDING_WIDTH - actorSize
	 || toPos[2] < 0  {
		return false;
	} */
	if (toPos[2] > PATHFINDING_HEIGHT) {
		return false;
	}
	return true;
}

/**
 * @brief Calculate the TUs after we in the given dir
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 */
void Step::calcNewTUs (const pathing_t* path)
{
	const byte TUsSoFar = RT_AREA_POS(path, fromPos, crouchingState);
	/* Find the number of TUs used (normally) to move in this direction. */
	const byte TUsForMove = Grid_GetTUsForDirection(dir, crouchingState);

	/* Now add the TUs needed to get to the originating cell. */
	TUsAfter = TUsSoFar + TUsForMove;
}

/**
 * @brief Checks if we can walk in the given direction
 * First test for opening height availability. Then test for stepup compatibility. Last test for fall.
 * @note Fliers use this code only when they are walking.
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @return false if we can't fly there
 */
bool Step::checkWalkingDirections (const pathing_t* path)
{
	int nx, ny, nz;
	int passageHeight;
	/** @todo falling_height should be replaced with an arbitrary max falling height based on the actor. */
	const int fallingHeight = PATHFINDING_MAX_FALL;/**<This is the maximum height that an actor can fall. */
	const int stepupHeight = routing.getStepupHeight(actorSize, fromPos[0], fromPos[1], fromPos[2], dir);		/**< The actual stepup height without the level flags */
	int heightChange;
	/** @todo actor_stepup_height should be replaced with an arbitrary max stepup height based on the actor. */
	int actorStepupHeight = PATHFINDING_MAX_STEPUP;

	/* This is the standard passage height for all units trying to move horizontally. */
	passageHeight = routing.getConn(actorSize, fromPos, dir);
	if (passageHeight < actorHeight) {
#if 0
/** I know this code could be streamlined, but until I understand it myself, plz leave it like it is !*/
		int dvFlagsNew = 0;
		if (!crouchingState									/* not in std crouch mode */
		 && passageHeight >= actorCrouchedHeight) {			/* and passage is tall enough for crouching ? */
															/* we should try autocrouching */
			int dvFlagsOld = getDVflags(RT_AREA_POS(path, fromPos, crouchingState));
			int toHeight = routing.getCeiling(actorSize, toPos) - routing.getFloor(actorSize, toPos);
			int tuCr = Grid_GetTUsForDirection(dir, 1);		/* 1 means crouched */
			int newTUs = 0;

			if (toHeight >= actorHeight) {					/* can we stand in the new cell ? */
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
			return false;	/* Passage is not tall enough. */
	}

	/* If we are moving horizontally, use the stepup requirement of the floors.
	 * The new z coordinate may need to be adjusted from stepup.
	 * Also, actor_stepup_height must be at least the cell's positive stepup value to move that direction. */
	/* If the actor cannot reach stepup, then we can't go this way. */
	if (actorStepupHeight < stepupHeight) {
		return false;	/* Actor cannot stepup high enough. */
	}

	nx = toPos[0];
	ny = toPos[1];
	nz = toPos[2];

	if (routing.isStepUpLevel(actorSize, fromPos, dir) && toPos[2] < PATHFINDING_HEIGHT - 1) {
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
	} else if (routing.isStepDownLevel(actorSize, fromPos, dir) && toPos[2] > 0
		&& actorStepupHeight >= routing.getStepupHeight(actorSize, nx, ny, nz - 1, dir ^ 1)) {
		toPos[2]--;		/* Stepping down into lower cell. */
	}

	heightChange = routing.getFloor(actorSize, toPos) - routing.getFloor(actorSize, fromPos) + (toPos[2] - fromPos[2]) * CELL_HEIGHT;

	/* If the actor tries to fall more than falling_height, then prohibit the move. */
	if (heightChange < -fallingHeight && !hasLadderSupport) {
		return false;		/* Too far a drop without a ladder. */
	}

	/* If we are walking normally, we can fall if we move into a cell that does not
	 * have its STEPDOWN flag set and has a negative floor:
	 * Set heightChange to 0.
	 * The actor enters the cell.
	 * The actor will be forced to fall (dir 13) from the destination cell to the cell below. */
	if (routing.getFloor(actorSize, toPos) < 0) {
		/* We cannot fall if STEPDOWN is defined. */
		if (routing.isStepDownLevel(actorSize, fromPos, dir)) {
			return false;		/* There is stepdown from here. */
		}
		heightChange = 0;
		toPos[2]--;
	}
	return true;
}

/**
 * @brief Checks if we can move in the given flying direction
 * @return false if we can't fly there
 */
bool Step::checkFlyingDirections () const
{
	const int coreDir = dir % CORE_DIRECTIONS;	/**< The compass direction of this flying move */
	int neededHeight;
	int passageHeight;

	if (toPos[2] > fromPos[2]) {
		/* If the actor is moving up, check the passage at the current cell.
		 * The minimum height is the actor's height plus the distance from the current floor to the top of the cell. */
		neededHeight = actorHeight + CELL_HEIGHT - std::max((const signed char)0, routing.getFloor(actorSize, fromPos));
		passageHeight = routing.getConn(actorSize, fromPos, coreDir);
	} else if (toPos[2] < fromPos[2]) {
		/* If the actor is moving down, check from the destination cell back. *
		 * The minimum height is the actor's height plus the distance from the destination floor to the top of the cell. */
		neededHeight = actorHeight + CELL_HEIGHT - std::max((const signed char)0, routing.getFloor(actorSize, toPos));
		passageHeight = routing.getConn(actorSize, toPos, coreDir ^ 1);
	} else {
		neededHeight = actorHeight;
		passageHeight = routing.getConn(actorSize, fromPos, coreDir);
	}
	if (passageHeight < neededHeight) {
		return false;
	}
	return true;
}

/**
 * @brief Checks if we can move in the given vertical direction
 * @return false if we can't move there
 */
bool Step::checkVerticalDirections () const
{
	if (dir == DIRECTION_FALL) {
		if (flier) {
			/* Fliers cannot fall intentionally. */
			return false;
		} else if (routing.getFloor(actorSize, fromPos) >= 0) {
			/* We cannot fall if there is a floor in this cell. */
			return false;
		} else if (hasLadderSupport) {
			/* The actor can't fall if there is ladder support. */
			return false;
		}
	} else if (dir == DIRECTION_CLIMB_UP) {
		if (flier && QuantToModel(routing.getCeiling(actorSize, fromPos)) < UNIT_HEIGHT * 2 - PLAYER_HEIGHT) { /* Not enough headroom to fly up. */
			return false;
		}
		/* If the actor is not a flyer and tries to move up, there must be a ladder. */
		if (dir == DIRECTION_CLIMB_UP && !hasLadderToClimb) {
			return false;
		}
	} else if (dir == DIRECTION_CLIMB_DOWN) {
		if (flier) {
			if (routing.getFloor(actorSize, fromPos) >= 0 ) { /* Can't fly down through a floor. */
				return false;
			}
		} else {
			/* If the actor is not a flyer and tries to move down, there must be a ladder. */
			if (!hasLadderToClimb) {
				return false;
			}
		}
	}
	return true;
}

/**
 * @param[in] path Pointer to client or server side pathing table (clMap, svMap)
 */
bool Step::isPossible (const pathing_t* path)
{
	/* calculate the position we would normally end up if moving in the given dir. */
	if (!calcNewPos()) {
		return false;
	}
	calcNewTUs(path);

	/* If there is no passageway (or rather lack of a wall) to the desired cell, then return. */
	/* If the flier is moving up or down diagonally, then passage height will also adjust */
	if (dir >= FLYING_DIRECTIONS) {
		if (!checkFlyingDirections()) {
			return false;
		}
	} else if (dir < CORE_DIRECTIONS) {
		/** note that this function may modify toPos ! */
		if (!checkWalkingDirections(path)) {
			return false;
		}
	} else {
		/* else there is no movement that uses passages. */
		/* If we are falling, the height difference is the floor value. */
		if (!checkVerticalDirections()) {
			return false;
		}
	}

	/* OK, at this point we are certain of a few things:
	 * There is not a wall obstructing access to the destination cell.
	 * If the actor is not a flier, the actor will not rise more than actor_stepup_height or fall more than
	 *    falling_height, unless climbing.
	 *
	 * If the actor is a flier, as long as there is a passage, it can be moved through.
	 * There are no floor difference restrictions for fliers, only obstructions. */

	return true;
}


/**
 * @brief Recalculate the pathing table for the given actor(-position)
 *
 * We calculate the table for ALL possible movement states (atm stand and crouch)
 * to be able to propose smart things like autostand.
 * @param[in] routing Reference to client or server side routing table (clMap, svMap)
 * @param[in] actorSize The size of thing to calc the move for (e.g. size=2 means 2x2).
 * The plan is to have the 'origin' in 2x2 units in the bottom-left (towards the lower coordinates) corner of the 2x2 square.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] from The position to start the calculation from.
 * @param[in] maxTUs The maximum TUs away from 'from' to calculate move-information for
 * @param[in] fb_list Forbidden list (entities are standing at those points)
 * @param[in] fb_length Length of forbidden list
 * @sa G_MoveCalc
 * @sa CL_ConditionalMoveCalc
 */
void Grid_CalcPathing (const Routing& routing, const actorSizeEnum_t actorSize, pathing_t* path, const pos3_t from, int maxTUs, byte**  fb_list, int fb_length)
{
	priorityQueue_t pqueue;
	pos4_t epos; /**< Extended position; includes crouching state */
	pos3_t pos;
	int amst; /* acronym for actor movement state */
	/* this is the position of the current actor- so the actor can stand in the cell it is in when pathfinding */
	pos3_t excludeFromForbiddenList;

	/* Confirm bounds */
	assert((from[2]) < PATHFINDING_HEIGHT);

	/* reset move data */
	OBJSET(path->area,     ROUTING_NOT_REACHABLE);
	OBJSET(path->areaFrom, ROUTING_NOT_REACHABLE);
	path->fblist = fb_list;
	path->fblength = fb_length;

	maxTUs = std::min(maxTUs, MAX_ROUTE_TUS);

	/* Prepare exclusion of starting-location (i.e. this should be ent-pos or le-pos) in Grid_CheckForbidden */
	VectorCopy(from, excludeFromForbiddenList);

	for (amst = 0; amst < ACTOR_MAX_STATES; amst++) {
		/* set starting position to 0 TUs.*/
		RT_AREA_POS(path, from, amst) = 0;

		PQueueInitialise(&pqueue, 1024);
		Vector4Set(epos, from[0], from[1], from[2], amst);
		PQueuePush(&pqueue, epos, 0);

		int count = 0;
		while (!PQueueIsEmpty(&pqueue)) {
			int dir;
			PQueuePop(&pqueue, epos);
			VectorCopy(epos, pos);
			count++;

			/* if reaching that square already took too many TUs,
			 * don't bother to reach new squares *from* there. */
			const byte usedTUs = RT_AREA_POS(path, pos, amst);
			if (usedTUs >= maxTUs)
				continue;

			for (dir = 0; dir < PATHFINDING_DIRECTIONS; ++dir) {
				Step step(routing, pos, actorSize, amst, dir);
				/* Directions 12, 14, and 15 are currently undefined. */
				if (dir == 12 || dir == 14 || dir == 15)
					continue;
				/* If this is a crouching or crouching move, forget it. */
				if (dir == DIRECTION_STAND_UP || dir == DIRECTION_CROUCH)
					continue;

				if (!step.init())
					continue; /* either dir is irrelevant or something worse happened */

				if (step.isPossible(path)) {
					/* Is this a better move into this cell? */
					RT_AREA_TEST_POS(path, step.toPos, step.crouchingState);
					if (RT_AREA_POS(path, step.toPos, step.crouchingState) <= step.TUsAfter) {
						continue; /* This move is not optimum. */
					}

					/* Test for forbidden (by other entities) areas. */
					if (Grid_CheckForbidden(excludeFromForbiddenList, step.actorSize, path, step.toPos[0],
							step.toPos[1], step.toPos[2])) {
						continue; /* That spot is occupied. */
					}

					/* Store move in pathing table. */
					Grid_SetMoveData(path, step.toPos, step.crouchingState, step.TUsAfter, step.dir, step.fromPos[2]);

					pos4_t dummy;
					Vector4Set(dummy, step.toPos[0], step.toPos[1], step.toPos[2], step.crouchingState);
					PQueuePush(&pqueue, dummy, step.TUsAfter);
				}
			}
		}
		/* Com_Printf("Loop: %i", count); */
		PQueueFree(&pqueue);
	}
}

/**
 * @brief Tries to find a path from the given actor(-position) to a given target position
 *
 * Unlike Grid_CalcPathing, this function does not neccessarily calculate the TU values for
 * all positions reachable from 'from'. Instead it tries to find the shortest/fastest path to
 * the target position. There is no limit to maxTUs.
 *
 * @param[in] routing Reference to client or server side routing table (clMap, svMap)
 * @param[in] actorSize The size of thing to calc the move for (e.g. size=2 means 2x2).
 * The plan is to have the 'origin' in 2x2 units in the bottom-left (towards the lower coordinates) corner of the 2x2 square.
 * @param[in,out] path Pointer to client or server side pathing table (clMap, svMap)
 * @param[in] from The position to start the calculation from.
 * @param[in] targetPos The position where we want to end up.
 * @param[in] maxTUs The maximum TUs away from 'from' to calculate move-information for
 * @param[in] crouchingState Whether the actor is currently crouching, 1 is yes, 0 is no.
 * @param[in] fb_list Forbidden list (entities are standing at those points)
 * @param[in] fb_length Length of forbidden list
 */
bool Grid_FindPath (const Routing& routing, const actorSizeEnum_t actorSize, pathing_t* path, const pos3_t from, const pos3_t targetPos, byte crouchingState, int maxTUs, byte**  fb_list, int fb_length)
{
	bool found = false;
	int count;
	priorityQueue_t pqueue;
	pos4_t epos; /**< Extended position; includes crouching state */
	pos3_t pos;
	/* this is the position of the current actor- so the actor can stand in the cell it is in when pathfinding */
	pos3_t excludeFromForbiddenList;

	/* Confirm bounds */
	assert((from[2]) < PATHFINDING_HEIGHT);
	assert(crouchingState == 0 || crouchingState == 1);	/* s.a. ACTOR_MAX_STATES */

	/* reset move data */
	OBJSET(path->area,     ROUTING_NOT_REACHABLE);
	OBJSET(path->areaFrom, ROUTING_NOT_REACHABLE);
	path->fblist = fb_list;
	path->fblength = fb_length;

	/* Prepare exclusion of starting-location (i.e. this should be ent-pos or le-pos) in Grid_CheckForbidden */
	VectorCopy(from, excludeFromForbiddenList);
	/* set starting position to 0 TUs.*/
	RT_AREA_POS(path, from, crouchingState) = 0;

	PQueueInitialise(&pqueue, 1024);
	Vector4Set(epos, from[0], from[1], from[2], crouchingState);
	PQueuePush(&pqueue, epos, 0);

	count = 0;
	while (!PQueueIsEmpty(&pqueue)) {
		PQueuePop(&pqueue, epos);
		VectorCopy(epos, pos);
		count++;

		/* if reaching that square already took too many TUs,
		 * don't bother to reach new squares *from* there. */
		const byte usedTUs = RT_AREA_POS(path, pos, crouchingState);
		if (usedTUs >= maxTUs)
			continue;

		for (int dir = 0; dir < PATHFINDING_DIRECTIONS; dir++) {
			Step step(routing, pos, actorSize, crouchingState, dir);
			/* Directions 12, 14, and 15 are currently undefined. */
			if (dir == 12 || dir == 14 || dir == 15)
				continue;
			/* If this is a crouching or crouching move, forget it. */
			if (dir == DIRECTION_STAND_UP || dir == DIRECTION_CROUCH)
				continue;

			if (!step.init())
				continue;		/* either dir is irrelevant or something worse happened */

			if (!step.isPossible(path))
				continue;

			/* Is this a better move into this cell? */
			RT_AREA_TEST_POS(path, step.toPos, step.crouchingState);
			if (RT_AREA_POS(path, step.toPos, step.crouchingState) <= step.TUsAfter) {
				continue;	/* This move is not optimum. */
			}

			/* Test for forbidden (by other entities) areas. */
			/* Do NOT check the forbiddenList. We might find a multi-turn path. */
#if 0
			if (Grid_CheckForbidden(excludeFromForbiddenList, step.actorSize, path, step.toPos[0], step.toPos[1], step.toPos[2])) {
				continue;		/* That spot is occupied. */
			}
#endif

			/* Store move in pathing table. */
			Grid_SetMoveData(path, step.toPos, step.crouchingState, step.TUsAfter, step.dir, step.fromPos[2]);

			pos4_t dummy;
			const int dist = step.TUsAfter + (int) (2 * VectorDist(step.toPos, targetPos));
			Vector4Set(dummy, step.toPos[0], step.toPos[1], step.toPos[2], step.crouchingState);
			PQueuePush(&pqueue, dummy, dist);

			if (VectorEqual(step.toPos, targetPos)) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	/* Com_Printf("Loop: %i", count); */
	PQueueFree(&pqueue);
	return found;
}

/**
 * @brief Caches the calculated move
 * @param[in] path Pointer to client or server side pathing table (clPathMap, svPathMap)
 * @sa AI_ActorThink
 */
void Grid_MoveStore (pathing_t* path)
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
pos_t Grid_MoveLength (const pathing_t* path, const pos3_t to, byte crouchingState, bool stored)
{
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
int Grid_MoveNext (const pathing_t* path, const pos3_t toPos, byte crouchingState)
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
 * @param[in] routing Reference to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's ceiling.
 */
unsigned int Grid_Ceiling (const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	assert(pos[2] < PATHFINDING_HEIGHT);
	return QuantToModel(routing.getCeiling(actorSize, pos[0], pos[1], pos[2] & 7));
}

/**
 * @brief Returns the height of the floor in a cell.
 * @param[in] routing Reference to client or server side routing table (clMap, svMap)
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos Position in the map to check the height
 * @return The actual model height of the cell's floor.
 */
int Grid_Floor (const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	assert(pos[2] < PATHFINDING_HEIGHT);
	return QuantToModel(routing.getFloor(actorSize, pos[0], pos[1], pos[2] & (PATHFINDING_HEIGHT - 1)));
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
 * @brief Calculated the new height level when something falls down from a certain position.
 * @param[in] routing Reference to client or server side routing table (clMap, svMap)
 * @param[in] pos Position in the map to start the fall (starting height is the z-value in this position)
 * @param[in] actorSize Give the field size of the actor (e.g. for 2x2 units) to check linked fields as well.
 * @return New z (height) value.
 * @return 0xFF if an error occurred.
 */
pos_t Grid_Fall (const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos)
{
	int z = pos[2], base, diff;
	bool flier = false; /** @todo if an actor can fly, then set this to true. */

	/* Is z off the map? */
	if (z >= PATHFINDING_HEIGHT) {
		return 0xFF;
	}

	/* If we can fly, then obviously we won't fall. */
	if (flier)
		return z;

	/* Easy math- get the floor, integer divide by CELL_HEIGHT, add to z.
	 * If z < 0, we are going down.
	 * If z >= CELL_HEIGHT, we are going up.
	 * If 0 <= z <= CELL_HEIGHT, then z / 16 = 0, no change. */
	base = routing.getFloor(actorSize, pos[0], pos[1], z);
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
 * @brief Checks if a crouched actor could save TUs by standing up, walking and crouching again.
 * @param[in] path Pointer to client or server side pathing table
 * @param[in] toPos Desired position
 */
bool Grid_ShouldUseAutostand (const pathing_t* path, const pos3_t toPos)
{
	const int tusCrouched = RT_AREA_POS(path, toPos, 1);
	const int tusUpright = RT_AREA_POS(path, toPos, 0);
	return tusUpright + 2 * TU_CROUCH < tusCrouched;
}

/**
 * @brief Converts a grid position to world coordinates
 * @param[in] routing The routing map
 * @param[in] actorSize width of the actor in cells
 * @param[in] pos The grid position
 * @param[out] vec The world vector
 */
void Grid_PosToVec (const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec)
{
	SizedPosToVec(pos, actorSize, vec);
#ifdef PARANOID
	if (pos[2] >= PATHFINDING_HEIGHT)
		Com_Printf("Grid_PosToVec: Warning - z level bigger than 7 (%i - source: %.02f)\n", pos[2], vec[2]);
#endif
	/* Clamp the floor value between 0 and UNIT_HEIGHT */
	const int gridFloor = Grid_Floor(routing, actorSize, pos);
	vec[2] += std::max(0, std::min(UNIT_HEIGHT, gridFloor));
}


/**
 * @brief This function recalculates the routing in and around the box bounded by min and max.
 * @sa CMod_LoadRouting
 * @sa Grid_RecalcRouting
 * @param[in] mapTiles List of tiles the current (RMA-)map is composed of
 * @param[in] routing The routing map (either server or client map)
 * @param[in] box The box to recalc routing for
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void Grid_RecalcBoxRouting (mapTiles_t* mapTiles, Routing& routing, const GridBox& box, const char** list)
{
	int x, y, z, actorSize, dir;

	/* check unit heights */
	for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) {
		GridBox rBox(box);	/* the box we will actually reroute */
		/* Offset the initial X and Y to compensate for larger actors when needed. */
		rBox.expandXY(actorSize - 1);
		/* also start one level above the box to measure high floors correctly */
		rBox.addOneZ();
		for (y = rBox.getMinY(); y <= rBox.getMaxY(); y++) {
			for (x = rBox.getMinX(); x <= rBox.getMaxX(); x++) {
				/** @note RT_CheckCell goes from top (7) to bottom (0) */
				for (z = rBox.getMaxZ(); z >= 0; z--) {
					const int newZ = RT_CheckCell(mapTiles, routing, actorSize, x, y, z, list);
					assert(newZ <= z);
					z = newZ;
				}
			}
		}
	}

	/* check connections */
	for (actorSize = 1; actorSize <= ACTOR_MAX_SIZE; actorSize++) {
		GridBox rBox(box);			/* the box we will actually reroute */
		rBox.expandXY(actorSize);	/* for connections, expand by the full size of the actor */
		rBox.addOneZ();
		for (y = rBox.getMinY(); y <= rBox.getMaxY(); y++) {
			for (x = rBox.getMinX(); x <= rBox.getMaxX(); x++) {
				for (dir = 0; dir < CORE_DIRECTIONS; dir++) {
					/* for places outside the model box, skip dirs that can not be affected by the model */
					if (x > box.getMaxX() && dir != 1 && dir != 5 && dir != 6)
						continue;
					if (y > box.getMaxY() && dir != 3 && dir != 5 && dir != 7)
						continue;
					if (actorSize == ACTOR_SIZE_NORMAL) {
						if (x < box.getMinX() && dir != 0 && dir != 4 && dir != 7)
							continue;
						if (y < box.getMinY() && dir != 2 && dir != 4 && dir != 6)
							continue;
					} else {
						/* the position of 2x2 actors is their lower left cell */
						if (x < box.getMinX() - 1 && dir != 0 && dir != 4 && dir != 7)
							continue;
						if (y < box.getMinY() - 1 && dir != 2 && dir != 4 && dir != 6)
							continue;
					}
				//	RT_UpdateConnectionColumn(mapTiles, routing, actorSize, x, y, dir, list);
					RT_UpdateConnectionColumn(mapTiles, routing, actorSize, x, y, dir, list, rBox.getMinZ(), rBox.getMaxZ());
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
 * @param[in] routing The routing map (either server or client map)
 * @param[in] name Name of the inline model to compute the mins/maxs for
 * @param[in] box The box around the inline model (alternative to name)
 * @param[in] list The local models list (a local model has a name starting with * followed by the model number)
 */
void Grid_RecalcRouting (mapTiles_t* mapTiles, Routing& routing, const char* name, const GridBox& box, const char** list)
{
	if (box.isZero()) {
		/* get inline model, if it is one */
		if (*name != '*') {
			Com_Printf("Called Grid_RecalcRouting with no inline model\n");
			return;
		}
		const cBspModel_t* model = CM_InlineModel(mapTiles, name);
		if (!model) {
			Com_Printf("Called Grid_RecalcRouting with invalid inline model name '%s'\n", name);
			return;
		}

		AABB absbox;
#if 1
		/* An attempt to fix the 'doors starting opened' bug (# 3456).
		 * The main difference is the (missing) rotation of the halfVec.
		 * The results are better, but do not fix the problem. */
		CalculateMinsMaxs(model->angles, model->cbmBox, model->origin, absbox);
#else
		/* get the target model's dimensions */
		if (VectorNotEmpty(model->angles)) {
			vec3_t minVec, maxVec;
			vec3_t centerVec, halfVec, newCenterVec;
			vec3_t m[3];

			/* Find the center of the extents. */
			model->cbmBox.getCenter(centerVec);

			/* Find the half height and half width of the extents. */
			VectorSubtract(model->cbmBox.maxs, centerVec, halfVec);

			/* Rotate the center about the origin. */
			VectorCreateRotationMatrix(model->angles, m);
			VectorRotate(m, centerVec, newCenterVec);

			/* Set minVec and maxVec to bound around newCenterVec at halfVec size. */
			VectorSubtract(newCenterVec, halfVec, minVec);
			VectorAdd(newCenterVec, halfVec, maxVec);

			/* Now offset by origin then convert to position (Doors do not have 0 origins) */
			absbox.set(minVec, maxVec);
			absbox.shift(model->origin);
		} else {  /* normal */
			/* Now offset by origin then convert to position (Doors do not have 0 origins) */
			absbox.set(model->cbmBox);
			absbox.shift(model->origin);
		}
#endif
		GridBox rerouteBox(absbox);

		/* fit min/max into the world size */
		rerouteBox.clipToMaxBoundaries();

		/* We now have the dimensions, call the generic rerouting function. */
		Grid_RecalcBoxRouting(mapTiles, routing, rerouteBox, list);
	} else {
		/* use the passed box */
		Grid_RecalcBoxRouting(mapTiles, routing, box, list);
	}
}
