/**
 * @file test_routing.c
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "CUnit/Basic.h"
#include "test_routing.h"
#include "test_shared.h"

#include "../common/common.h"
#include "../common/cmodel.h"
#include "../common/grid.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRouting (void)
{
	TEST_Init();
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
		CU_FAIL("Map resource for test is missing.");
	}
}

/**
 * @todo func_door, func_breakable, ...
 */
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
		CU_FAIL("Map resource for test is missing.");
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
		int lengthStored, lengthUnstored;
		pos3_t to;
		pathing_t *path = Mem_AllocType(pathing_t);

		VectorSet(vec, 80, 80, 32);
		VecToPos(vec, pos);

		Grid_MoveCalc(routing, ACTOR_SIZE_NORMAL, path, pos, crouchingState, distance, NULL, 0);
		Grid_MoveStore(path);

		/* move downwards */
		{
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
	}
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

	return CUE_SUCCESS;
}
