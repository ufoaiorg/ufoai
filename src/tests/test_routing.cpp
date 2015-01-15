/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "test_shared.h"

#include "../common/common.h"
#include "../common/cmodel.h"
#include "../common/grid.h"
#include "../game/g_local.h"
#include "../game/g_edicts.h"
#include "../game/g_utils.h"
#include "../server/server.h"

class RoutingTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		Com_ParseScripts(true);
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

static mapData_t mapData;
static mapTiles_t mapTiles;
static const char* mapName = "test_routing";

TEST_F(RoutingTest, MapLoading)
{
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	char entityString[MAX_TOKEN_CHARS];
	CM_LoadMap(mapName, true, "", entityString, &mapData, &mapTiles);
	CM_LoadMap(mapName, true, "", entityString, &mapData, &mapTiles);
}

TEST_F(RoutingTest, Move)
{
	vec3_t vec;
	pos3_t pos;
	pos_t gridPos;

	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	char entityString[MAX_TOKEN_CHARS];
	CM_LoadMap(mapName, true, "", entityString, &mapData, &mapTiles);
	CM_LoadMap(mapName, true, "", entityString, &mapData, &mapTiles);

	VectorSet(vec, 16, 16, 48);
	VecToPos(vec, pos);
	ASSERT_EQ(pos[0], 128);
	ASSERT_EQ(pos[1], 128);
	ASSERT_EQ(pos[2], 0);

	VectorSet(vec, 80, 16, 80);
	VecToPos(vec, pos);
	ASSERT_EQ(pos[0], 130);
	ASSERT_EQ(pos[1], 128);
	ASSERT_EQ(pos[2], 1);

	gridPos = Grid_Fall(mapData.routing, ACTOR_SIZE_NORMAL, pos);
	ASSERT_EQ(gridPos, 1);

	{
		const byte crouchingState = 0;
		const int maxTUs = MAX_ROUTE_TUS;
		int lengthStored;
		pos3_t to;
		pathing_t* path = Mem_AllocType(pathing_t);

		VectorSet(vec, 80, 80, 32);
		VecToPos(vec, pos);

		Grid_CalcPathing(mapData.routing, ACTOR_SIZE_NORMAL, path, pos, maxTUs, nullptr);
		Grid_MoveStore(path);

		/* move downwards */
		{
			int lengthUnstored;
			VectorSet(vec, 80, 48, 32);
			VecToPos(vec, to);

			lengthUnstored = Grid_MoveLength(path, to, crouchingState, false);
			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthUnstored, lengthStored);
			ASSERT_EQ(lengthStored, TU_MOVE_STRAIGHT);
		}
		/* try to move three steps upwards - there is a brush*/
		{
			VectorSet(vec, 80, 176, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, ROUTING_NOT_REACHABLE);
		}
		/* try move into the nodraw */
		{
			VectorSet(vec, 48, 16, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, ROUTING_NOT_REACHABLE);
		}
		/* move into the lightclip */
		{
			VectorSet(vec, 48, 48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, TU_MOVE_DIAGONAL);
		}
		/* move into the passable */
		{
			VectorSet(vec, 144, 48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, TU_MOVE_DIAGONAL + TU_MOVE_STRAIGHT);
		}
		/* go to the other side - diagonal, followed by six straight moves */
		{
			VectorSet(vec, -16, 48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 6 * TU_MOVE_STRAIGHT + TU_MOVE_DIAGONAL);
		}
		/* try to walk out of the map */
		{
			VectorSet(vec, 48, 272, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, ROUTING_NOT_REACHABLE);
		}
		/* walk to the map border */
		{
			VectorSet(vec, 48, 240, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 4 * TU_MOVE_STRAIGHT + TU_MOVE_DIAGONAL);
		}
		/* walk a level upwards */
		{
			VectorSet(vec, 240, 80, 96);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 5 * TU_MOVE_STRAIGHT);
		}
		/* move to the door (not a func_door) */
		{
			VectorSet(vec, 176, -80, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 4 * TU_MOVE_STRAIGHT + 2 * TU_MOVE_DIAGONAL);
		}
		/* move into the trigger_touch */
		{
			VectorSet(vec, -48, -80, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 5 * TU_MOVE_STRAIGHT + 3 * TU_MOVE_DIAGONAL);
		}
		/* try to walk into the actorclip */
		{
			VectorSet(vec, -48, -48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, ROUTING_NOT_REACHABLE);
		}
	}
}

TEST_F(RoutingTest, MoveEntities)
{
	pos3_t pos;
	vec3_t vec;
	pathing_t* path = Mem_AllocType(pathing_t);
	forbiddenList_t forbiddenList;
	const byte crouchingState = 0;
	const int maxTUs = MAX_ROUTE_TUS;
	forbiddenList.reset();

	SV_Map(true, mapName, nullptr);

	/* starting point */
	VectorSet(vec, 240, -144, 32);
	VecToPos(vec, pos);

	G_CompleteRecalcRouting();

	{
		Edict* ent = nullptr;
		while ((ent = G_EdictsGetNextInUse(ent))) {
			/* Dead 2x2 unit will stop walking, too. */
			if (ent->type == ET_SOLID) {
				for (int j = 0; j < ent->forbiddenListSize; j++) {
					forbiddenList.add(ent->forbiddenListPos[j], (byte*) &ent->fieldSize);
				}
			}
		}
	}

	{
		int lengthStored;
		pos3_t to;

		Grid_CalcPathing(sv->mapData.routing, ACTOR_SIZE_NORMAL, path, pos, maxTUs, &forbiddenList);
		Grid_MoveStore(path);

		/* walk onto the func_breakable */
		{
			VectorSet(vec, 112, -144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 4 * TU_MOVE_STRAIGHT);
		}
		/* walk over the func_breakable */
		{
			VectorSet(vec, 80, -144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 5 * TU_MOVE_STRAIGHT);
		}
		/* walk over the func_breakable */
		{
			VectorSet(vec, 16, -144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 7 * TU_MOVE_STRAIGHT);
		}
	}

	/* starting point */
	VectorSet(vec, 144, 144, 32);
	VecToPos(vec, pos);

	{
		int lengthStored;
		pos3_t to;

		Grid_CalcPathing(sv->mapData.routing, ACTOR_SIZE_NORMAL, path, pos, maxTUs, &forbiddenList);
		Grid_MoveStore(path);

		/* walk through the opened door */
		{
			VectorSet(vec, 112, 144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, TU_MOVE_STRAIGHT);
		}

		/* walk around the opened door */
		{
			VectorSet(vec, 144, 208, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, true);
			ASSERT_EQ(lengthStored, 2 * TU_MOVE_STRAIGHT + TU_MOVE_DIAGONAL);
		}
	}

	SV_ShutdownGameProgs();
}

/* tests for the new dvec format */
TEST_F(RoutingTest, Dvec)
{
	short dv1  = 0x0724;
	ASSERT_EQ(getDVdir(dv1), 0x07);
	ASSERT_EQ(getDVflags(dv1), 0x02);
	ASSERT_EQ(getDVz(dv1), 0x04);

	short dv2 = makeDV(6, 3);
	ASSERT_EQ(dv2, 0x0603);

	dv2 = setDVz(dv2, 4);
	ASSERT_EQ(dv2, 0x0604);
}

TEST_F(RoutingTest, TUsForDir)
{
	ASSERT_EQ(Grid_GetTUsForDirection(0, 0), 2);
	ASSERT_EQ(Grid_GetTUsForDirection(2, 0), 2);
	ASSERT_EQ(Grid_GetTUsForDirection(5, 0), 3);
	ASSERT_EQ(Grid_GetTUsForDirection(5, false), 3);
	ASSERT_EQ(Grid_GetTUsForDirection(0, 1), 3);	/* now crouching */
	ASSERT_EQ(Grid_GetTUsForDirection(5, 1), 4);
	ASSERT_EQ(Grid_GetTUsForDirection(5, true), 4);
	ASSERT_EQ(Grid_GetTUsForDirection(16, 0), 4);	/* flying takes twice as much */
	ASSERT_EQ(Grid_GetTUsForDirection(16, 1), 4);	/* flying & crouching is still the same */
}
