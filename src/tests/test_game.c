/**
 * @file test_game.c
 * @brief Test cases for code about server game logic
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
#include "test_shared.h"
#include "test_game.h"
#include "../shared/ufotypes.h"
#include "../server/server.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteGame (void)
{
	TEST_Init();
	/* we need the teamdefs for spawning ai actors */
	Com_ParseScripts(qtrue);

	sv_genericPool = Mem_CreatePool("server-gametest");
	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteGame (void)
{
	TEST_Shutdown();
	return 0;
}

static void testSpawnAndConnect (void)
{
	char userinfo[MAX_INFO_STRING];
	player_t *player;
	const char *name = "name";
	qboolean day = qtrue;
	byte *buf;
	/* this entity string may not contain any inline models, we don't have the bsp tree loaded here */
	const int size = FS_LoadFile("game/entity.txt", &buf);

	CU_ASSERT_NOT_EQUAL_FATAL(size, -1);
	CU_ASSERT_FATAL(size > 0);

	SV_InitGameProgs();
	/* otherwise we can't link the entities */
	SV_ClearWorld();

	player = PLAYER_NUM(0);
	svs.ge->SpawnEntities(name, day, (const char *)buf);
	CU_ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));
	CU_ASSERT_FALSE(svs.ge->RunFrame());

	SV_ShutdownGameProgs();
	FS_FreeFile(buf);
}

static void testShooting (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		/* the other tests didn't call the server shutdown function to clean up */
		memset(&sv, 0, sizeof(sv));
		SV_Map(qtrue, mapName, NULL);
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}

int UFO_AddGameTests (void)
{
	/* add a suite to the registry */
	CU_pSuite GameSuite = CU_add_suite("GameTests", UFO_InitSuiteGame, UFO_CleanSuiteGame);

	if (GameSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(GameSuite, testSpawnAndConnect) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GameSuite, testShooting) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
