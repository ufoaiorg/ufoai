/**
 * @file
 * @brief Battlescape grid functions
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

#pragma once

#define MAX_FORBIDDENLIST (MAX_EDICTS * 4)

/**
 * @brief A list of locations that cannot be moved to.
 * @note Pointer to le->pos or edict->pos followed by pointer to le->fieldSize or edict->fieldSize
 * @see CL_BuildForbiddenList
 */
typedef struct forbiddenList_s {
private:
	pos_t* fbList[MAX_FORBIDDENLIST];
	int fbListLength;	/**< Current length of fbList. */
public:

	forbiddenList_s () {
		reset();
	}
	inline void add(pos3_t pos, byte* entSize) {
		fbList[fbListLength++] = pos;
		fbList[fbListLength++] = entSize;

		if (fbListLength > MAX_FORBIDDENLIST)
			Sys_Error("ForbiddenList.add: list too long\n");
	}
	inline void reset() {
		fbListLength = 0;
	}
	inline pos_t** getNext(pos_t** prev) {
		if (!fbListLength)
			return nullptr;
		if (!prev)
			return fbList;
		prev += 2;
		if (prev >= fbList + fbListLength)
			return nullptr;
		return prev;
	}
	inline actorSizeEnum_t getEntSize(pos_t** current) {
		actorSizeEnum_t entSize;
		byte* forbiddenSize = *(current + 1);
		memcpy(&entSize, forbiddenSize, sizeof(entSize));
		return entSize;
	}
#ifdef DEBUG
	/* this is NOT equivalent to Grid_CheckForbidden() !! Just for debugging purposes */
	inline bool contains(const pos3_t pos) {
		pos_t** p = nullptr;
		while ((p = getNext(p))) {
			if (VectorCompare((*p), pos))
				return true;
		}
		return false;
	}
#endif
} forbiddenList_t;

typedef struct pathing_s {
	/* TUs needed to move to this cell for the current actor */
	byte area[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];
	byte areaStored[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* Indicates where the actor would have moved from to get to the cell */
	dvec_t areaFrom[ACTOR_MAX_STATES][PATHFINDING_HEIGHT][PATHFINDING_WIDTH][PATHFINDING_WIDTH];

	/* forbidden list */
	forbiddenList_t* fbList;	/**< pointer to forbidden list (entities are standing here) */
} pathing_t;

/*==========================================================
GRID ORIENTED MOVEMENT AND SCANNING
==========================================================*/

void Grid_RecalcRouting(mapTiles_t* mapTiles, Routing& routing, const char* name, const GridBox& box, const char** list);
void Grid_RecalcBoxRouting(mapTiles_t* mapTiles, Routing& routing, const GridBox& box, const char** list);
void Grid_CalcPathing(const Routing& routing, const actorSizeEnum_t actorSize, pathing_t* path, const pos3_t from, int distance, forbiddenList_t* forbiddenList);
bool Grid_FindPath(const Routing& routing, const actorSizeEnum_t actorSize, pathing_t* path, const pos3_t from, const pos3_t targetPos, byte crouchingState, int maxTUs, forbiddenList_t* forbiddenList);
void Grid_MoveStore(pathing_t* path);
pos_t Grid_MoveLength(const pathing_t* path, const pos3_t to, byte crouchingState, bool stored);
int Grid_MoveNext(const pathing_t* path, const pos3_t toPos, byte crouchingState);
unsigned int Grid_Ceiling(const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos);
int Grid_Floor(const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos);
int Grid_GetTUsForDirection(const int dir, bool crouched);
pos_t Grid_Fall(const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos);
bool Grid_ShouldUseAutostand (const pathing_t* path, const pos3_t toPos);
void Grid_PosToVec(const Routing& routing, const actorSizeEnum_t actorSize, const pos3_t pos, vec3_t vec);
