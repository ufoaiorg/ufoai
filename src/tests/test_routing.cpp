/**
 * @file test_routing.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "test_routing.h"
#include "test_shared.h"

#include "../common/common.h"
#include "../common/cmodel.h"
#include "../common/grid.h"
#include "../game/g_local.h"
#include "../server/server.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRouting (void)
{
	TEST_Init();
	Com_ParseScripts(qtrue);

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRouting (void)
{
	TEST_Shutdown();
	return 0;
}

static mapData_t mapData;
static mapTiles_t mapTiles;
static const char *mapName = "test_routing";
static void testMapLoading (void)
{
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		CM_LoadMap(mapName, qtrue, "", &mapData, &mapTiles);
		CM_LoadMap(mapName, qtrue, "", &mapData, &mapTiles);
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}

static void testMove (void)
{
	routing_t *routing;
	vec3_t vec;
	pos3_t pos;
	pos_t gridPos;

	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		CM_LoadMap(mapName, qtrue, "", &mapData, &mapTiles);
		CM_LoadMap(mapName, qtrue, "", &mapData, &mapTiles);
	} else {
		UFO_CU_FAIL_MSG_FATAL(va("Map resource '%s.bsp' for test is missing.", mapName));
	}

	VectorSet(vec, 16, 16, 48);
	VecToPos(vec, pos);
	CU_ASSERT_EQUAL(pos[0], 128);
	CU_ASSERT_EQUAL(pos[1], 128);
	CU_ASSERT_EQUAL(pos[2], 0);

	VectorSet(vec, 80, 16, 80);
	VecToPos(vec, pos);
	CU_ASSERT_EQUAL(pos[0], 130);
	CU_ASSERT_EQUAL(pos[1], 128);
	CU_ASSERT_EQUAL(pos[2], 1);

	routing = &mapData.map[ACTOR_SIZE_NORMAL - 1];
	gridPos = Grid_Fall(routing, ACTOR_SIZE_NORMAL, pos);
	CU_ASSERT_EQUAL(gridPos, 1);

	{
		const byte crouchingState = 0;
		const int distance = MAX_ROUTE;
		int lengthStored;
		pos3_t to;
		pathing_t *path = Mem_AllocType(pathing_t);

		VectorSet(vec, 80, 80, 32);
		VecToPos(vec, pos);

		Grid_MoveCalc(routing, ACTOR_SIZE_NORMAL, path, pos, crouchingState, distance, NULL, 0);
		Grid_MoveStore(path);

		/* move downwards */
		{
			int lengthUnstored;
			VectorSet(vec, 80, 48, 32);
			VecToPos(vec, to);

			lengthUnstored = Grid_MoveLength(path, to, crouchingState, qfalse);
			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthUnstored, lengthStored);
			CU_ASSERT_EQUAL(lengthStored, TU_MOVE_STRAIGHT);
		}
		/* try to move three steps upwards - there is a brush*/
		{
			VectorSet(vec, 80, 176, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, ROUTING_NOT_REACHABLE);
		}
		/* try move into the nodraw */
		{
			VectorSet(vec, 48, 16, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, ROUTING_NOT_REACHABLE);
		}
		/* move into the lightclip */
		{
			VectorSet(vec, 48, 48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, TU_MOVE_DIAGONAL);
		}
		/* move into the passable */
		{
			VectorSet(vec, 144, 48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, TU_MOVE_DIAGONAL + TU_MOVE_STRAIGHT);
		}
		/* go to the other side - diagonal, followed by six straight moves */
		{
			VectorSet(vec, -16, 48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 6 * TU_MOVE_STRAIGHT + TU_MOVE_DIAGONAL);
		}
		/* try to walk out of the map */
		{
			VectorSet(vec, 48, 272, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, ROUTING_NOT_REACHABLE);
		}
		/* walk to the map border */
		{
			VectorSet(vec, 48, 240, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 4 * TU_MOVE_STRAIGHT + TU_MOVE_DIAGONAL);
		}
		/* walk a level upwards */
		{
			VectorSet(vec, 240, 80, 96);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 5 * TU_MOVE_STRAIGHT);
		}
		/* move to the door (not a func_door) */
		{
			VectorSet(vec, 176, -80, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 4 * TU_MOVE_STRAIGHT + 2 * TU_MOVE_DIAGONAL);
		}
		/* move into the trigger_touch */
		{
			VectorSet(vec, -48, -80, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 5 * TU_MOVE_STRAIGHT + 3 * TU_MOVE_DIAGONAL);
		}
		/* try to walk into the actorclip */
		{
			VectorSet(vec, -48, -48, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, ROUTING_NOT_REACHABLE);
		}
	}
}

static void testMoveEntities (void)
{
	routing_t *routing;
	pos3_t pos;
	vec3_t vec;
	pathing_t *path = Mem_AllocType(pathing_t);
	pos_t *forbiddenList[MAX_FORBIDDENLIST];
	int forbiddenListLength = 0;
	const byte crouchingState = 0;
	const int distance = MAX_ROUTE;

	SV_Map(qtrue, mapName, NULL);

	/* starting point */
	VectorSet(vec, 240, -144, 32);
	VecToPos(vec, pos);

	routing = &sv->mapData.map[ACTOR_SIZE_NORMAL - 1];

	G_CompleteRecalcRouting();

	{
		edict_t *ent = NULL;
		while ((ent = G_EdictsGetNextInUse(ent))) {
			/* Dead 2x2 unit will stop walking, too. */
			if (ent->type == ET_SOLID) {
				int j;
				for (j = 0; j < ent->forbiddenListSize; j++) {
					forbiddenList[forbiddenListLength++] = ent->forbiddenListPos[j];
					forbiddenList[forbiddenListLength++] = (byte*) &ent->fieldSize;
				}
			}
		}
	}

	{
		int lengthStored;
		pos3_t to;

		Grid_MoveCalc(routing, ACTOR_SIZE_NORMAL, path, pos, crouchingState, distance, forbiddenList, forbiddenListLength);
		Grid_MoveStore(path);

		/* walk onto the func_breakable */
		{
			VectorSet(vec, 112, -144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 4 * TU_MOVE_STRAIGHT);
		}
		/* walk over the func_breakable */
		{
			VectorSet(vec, 80, -144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 5 * TU_MOVE_STRAIGHT);
		}
		/* walk over the func_breakable */
		{
			VectorSet(vec, 16, -144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 7 * TU_MOVE_STRAIGHT);
		}
	}

	/* starting point */
	VectorSet(vec, 144, 144, 32);
	VecToPos(vec, pos);

	{
		int lengthStored;
		pos3_t to;

		Grid_MoveCalc(routing, ACTOR_SIZE_NORMAL, path, pos, crouchingState, distance, forbiddenList, forbiddenListLength);
		Grid_MoveStore(path);

		/* walk through the opened door */
		{
			VectorSet(vec, 112, 144, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, TU_MOVE_STRAIGHT);
		}

		/* walk around the opened door */
		{
			VectorSet(vec, 144, 208, 32);
			VecToPos(vec, to);

			lengthStored = Grid_MoveLength(path, to, crouchingState, qtrue);
			CU_ASSERT_EQUAL(lengthStored, 2 * TU_MOVE_STRAIGHT + TU_MOVE_DIAGONAL);
		}
	}

	SV_ShutdownGameProgs();
}

/* tests for the new dvec format */
static void testDvec (void)
{
	short dv1  = 0x0724;
	CU_ASSERT_EQUAL(getDVdir(dv1), 0x07);
	CU_ASSERT_EQUAL(getDVflags(dv1), 0x02);
	CU_ASSERT_EQUAL(getDVz(dv1), 0x04);

	short dv2 = makeDV(6, 3);
	CU_ASSERT_EQUAL(dv2, 0x0603);

	dv2 = setDVz(dv2, 4);
	CU_ASSERT_EQUAL(dv2, 0x0604);
}

static void testTUsForDir (void)
{
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(0, 0), 2);
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(2, 0), 2);
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(5, 0), 3);
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(5, qfalse), 3);
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(0, 1), 3);	/* now crouching */
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(5, 1), 4);
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(5, qtrue), 4);
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(16, 0), 4);	/* flying takes twice as much */
	CU_ASSERT_EQUAL(Grid_GetTUsForDirection(16, 1), 4);	/* flying & crouching is still the same */
}

int UFO_AddRoutingTests (void)
{
	/* add a suite to the registry */
	CU_pSuite routingSuite = CU_add_suite("RoutingTests", UFO_InitSuiteRouting, UFO_CleanSuiteRouting);
	if (routingSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(routingSuite, testMapLoading) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(routingSuite, testMove) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(routingSuite, testMoveEntities) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(routingSuite, testDvec) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(routingSuite, testTUsForDir) == NULL)
		return CU_get_error();

	/**
	 * @todo Test for: water, func_door_sliding, some terrain brushes to test the max
	 * walkable raising, missing trigger types, autocrouch stuff
	 */

	return CUE_SUCCESS;
}
