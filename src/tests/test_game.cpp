/**
 * @file test_game.c
 * @brief Test cases for code about server game logic
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

#include "test_shared.h"
#include "test_game.h"
#include "../shared/ufotypes.h"
#include "../game/g_local.h"
#include "../server/server.h"
#include "../client/renderer/r_state.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteGame (void)
{
	TEST_Init();
	/* we need the teamdefs for spawning ai actors */
	Com_ParseScripts(qtrue);
	Cvar_Set("sv_threads", "0");

	sv_genericPool = Mem_CreatePool("server-gametest");
	r_state.active_texunit = &r_state.texunits[0];
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
	edict_t *e = NULL;
	int cnt = 0;

	CU_ASSERT_NOT_EQUAL_FATAL(size, -1);
	CU_ASSERT_FATAL(size > 0);

	SV_InitGameProgs();
	/* otherwise we can't link the entities */
	SV_ClearWorld();

	player = PLAYER_NUM(0);
	svs.ge->SpawnEntities(name, day, (const char *)buf);
	CU_ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));
	CU_ASSERT_FALSE(svs.ge->RunFrame());

	while ((e = G_EdictsGetNextInUse(e))) {
		Com_Printf("entity %i: %s\n", cnt, e->classname);
		cnt++;
	}

	CU_ASSERT_EQUAL(cnt, 45);

	SV_ShutdownGameProgs();
	FS_FreeFile(buf);
}

static void testDoorTrigger (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		edict_t *e = NULL;
		int cnt = 0;
		int doors = 0;

		/* the other tests didn't call the server shutdown function to clean up */
		OBJZERO(*sv);
		SV_Map(qtrue, mapName, NULL);
		while ((e = G_EdictsGetNextInUse(e))) {
			cnt++;
			if (e->type == ET_DOOR) {
				if (Q_streq(e->targetname, "left-0")) {
					/* this one is triggered by an actor standing inside of a trigger_touch */
					CU_ASSERT_TRUE(e->doorState);
				} else if (Q_streq(e->targetname, "right-0")) {
					/* this one has a trigger_touch, too - but nobody is touching that trigger yet */
					CU_ASSERT_FALSE(e->doorState);
				} else {
					/* both of the used doors have a targetname set */
					CU_ASSERT(qfalse);
				}
				doors++;
			}
		}

		SV_ShutdownGameProgs();

		CU_ASSERT_TRUE(cnt > 0);
		CU_ASSERT_TRUE(doors == 2);
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}

static void testShooting (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		/* the other tests didn't call the server shutdown function to clean up */
		OBJZERO(*sv);
		SV_Map(qtrue, mapName, NULL);
		/** @todo equip the soldier */
		/** @todo set the input variables -- gi.ReadFormat(format, &pos, &i, &firemode, &from); */
		/** @todo do the shot -- G_ClientShoot(player, ent, pos, i, firemode, &mock, qtrue, from); */
		/** @todo implement the test here - e.g. extend shot_mock_t */
		SV_ShutdownGameProgs();
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}

static int GAMETEST_GetItemCount (const edict_t *ent, containerIndex_t container)
{
	const invList_t *invlist = CONTAINER(ent, container);
	int count = 0;
	while (invlist != NULL) {
		count += invlist->item.amount;
		invlist = invlist->next;
	}

	return count;
}

static void testVisFlags (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		edict_t *ent;
		int num;

		/* the other tests didn't call the server shutdown function to clean up */
		OBJZERO(*sv);
		SV_Map(qtrue, mapName, NULL);

		num = 0;
		ent = NULL;
		while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, TEAM_ALIEN))) {
			const vismask_t teamMask = G_TeamToVisMask(ent->team);
			const qboolean visible = ent->visflags & teamMask;
			char *visFlagsBuf = Mem_StrDup(Com_UnsignedIntToBinary(ent->visflags));
			char *teamMaskBuf = Mem_StrDup(Com_UnsignedIntToBinary(teamMask));
			CU_ASSERT_EQUAL(ent->team, TEAM_ALIEN);
			UFO_CU_ASSERT_TRUE_MSG(visible, va("visflags: %s, teamMask: %s", visFlagsBuf, teamMaskBuf));
			Mem_Free(visFlagsBuf);
			Mem_Free(teamMaskBuf);
			num++;
		}

		SV_ShutdownGameProgs();
		CU_ASSERT_TRUE(num > 0);
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}
static void testInventoryForDiedAlien (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		edict_t* diedEnt;
		edict_t* ent;
		edict_t* floorItems;
		player_t* player;
		invList_t* invlist;
		int count;
		/* the other tests didn't call the server shutdown function to clean up */
		OBJZERO(*sv);
		SV_Map(qtrue, mapName, NULL);
		level.activeTeam = TEAM_ALIEN;

		/* first alien that should die and drop its inventory */
		diedEnt = G_EdictsGetNextLivingActorOfTeam(NULL, TEAM_ALIEN);
		CU_ASSERT_PTR_NOT_NULL_FATAL(diedEnt);
		diedEnt->HP = 0;
		CU_ASSERT_TRUE(G_ActorDieOrStun(diedEnt, NULL));
		CU_ASSERT_TRUE_FATAL(G_IsDead(diedEnt));

		/* now try to collect the inventory with a second alien */
		ent = G_EdictsGetNextLivingActorOfTeam(NULL, TEAM_ALIEN);
		CU_ASSERT_PTR_NOT_NULL_FATAL(ent);

		/* move to the location of the first died alien to drop the inventory into the same floor container */
		player = G_PLAYER_FROM_ENT(ent);
		CU_ASSERT_TRUE_FATAL(G_IsAIPlayer(player));
		G_ClientMove(player, 0, ent, diedEnt->pos);
		CU_ASSERT_TRUE_FATAL(VectorCompare(ent->pos, diedEnt->pos));

		floorItems = G_GetFloorItems(ent);
		CU_ASSERT_PTR_NOT_NULL_FATAL(floorItems);
		CU_ASSERT_PTR_EQUAL(FLOOR(floorItems), FLOOR(ent));

		/* drop everything to floor to make sure we have space in the backpack */
		G_InventoryToFloor(ent);
		CU_ASSERT_EQUAL(GAMETEST_GetItemCount(ent, csi.idBackpack), 0);

		invlist = CONTAINER(ent, csi.idBackpack);
		CU_ASSERT_PTR_NULL_FATAL(invlist);
		count = GAMETEST_GetItemCount(ent, csi.idFloor);
		if (count > 0) {
			invList_t *entryToMove = FLOOR(ent);
			int tx, ty;
			INVSH_FindSpace(&ent->chr.i, &entryToMove->item, INVDEF(csi.idBackpack), &tx, &ty, entryToMove);
			if (tx != NONE) {
				Com_Printf("trying to move item %s from floor into backpack to pos %i:%i\n", entryToMove->item.t->name, tx, ty);
				CU_ASSERT_TRUE(G_ActorInvMove(ent, INVDEF(csi.idFloor), entryToMove, INVDEF(csi.idBackpack), tx, ty, qfalse));
				UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(GAMETEST_GetItemCount(ent, csi.idFloor), count - 1, va("item %s could not get moved successfully from floor into backpack", entryToMove->item.t->name));
				Com_Printf("item %s was removed from floor\n", entryToMove->item.t->name);
				UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(GAMETEST_GetItemCount(ent, csi.idBackpack), 1, va("item %s could not get moved successfully from floor into backpack", entryToMove->item.t->name));
				Com_Printf("item %s was moved successfully into the backpack\n", entryToMove->item.t->name);
				invlist = CONTAINER(ent, csi.idBackpack);
				CU_ASSERT_PTR_NOT_NULL_FATAL(invlist);
			}
		}

		SV_ShutdownGameProgs();
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}

static void testInventoryWithTwoDiedAliensOnTheSameGridTile (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		edict_t* diedEnt;
		edict_t* diedEnt2;
		edict_t* ent;
		edict_t* floorItems;
		player_t* player;
		invList_t* invlist;
		int count;
		/* the other tests didn't call the server shutdown function to clean up */
		OBJZERO(*sv);
		SV_Map(qtrue, mapName, NULL);
		level.activeTeam = TEAM_ALIEN;

		/* first alien that should die and drop its inventory */
		diedEnt = G_EdictsGetNextLivingActorOfTeam(NULL, TEAM_ALIEN);
		CU_ASSERT_PTR_NOT_NULL_FATAL(diedEnt);
		diedEnt->HP = 0;
		G_ActorDieOrStun(diedEnt, NULL);
		CU_ASSERT_TRUE_FATAL(G_IsDead(diedEnt));

		/* second alien that should die and drop its inventory */
		diedEnt2 = G_EdictsGetNextLivingActorOfTeam(NULL, TEAM_ALIEN);
		CU_ASSERT_PTR_NOT_NULL_FATAL(diedEnt2);

		/* move to the location of the first died alien to drop the inventory into the same floor container */
		player = G_PLAYER_FROM_ENT(diedEnt2);
		CU_ASSERT_TRUE_FATAL(G_IsAIPlayer(player));
		G_ClientMove(player, 0, diedEnt2, diedEnt->pos);
		CU_ASSERT_TRUE_FATAL(VectorCompare(diedEnt2->pos, diedEnt->pos));

		diedEnt2->HP = 0;
		G_ActorDieOrStun(diedEnt2, NULL);
		CU_ASSERT_TRUE_FATAL(G_IsDead(diedEnt2));

		/* now try to collect the inventory with a third alien */
		ent = G_EdictsGetNextLivingActorOfTeam(NULL, TEAM_ALIEN);
		CU_ASSERT_PTR_NOT_NULL_FATAL(ent);

		player = G_PLAYER_FROM_ENT(ent);
		CU_ASSERT_TRUE_FATAL(G_IsAIPlayer(player));

		G_ClientMove(player, 0, ent, diedEnt->pos);
		CU_ASSERT_TRUE_FATAL(VectorCompare(ent->pos, diedEnt->pos));

		floorItems = G_GetFloorItems(ent);
		CU_ASSERT_PTR_NOT_NULL_FATAL(floorItems);
		CU_ASSERT_PTR_EQUAL(FLOOR(floorItems), FLOOR(ent));

		/* drop everything to floor to make sure we have space in the backpack */
		G_InventoryToFloor(ent);
		CU_ASSERT_EQUAL(GAMETEST_GetItemCount(ent, csi.idBackpack), 0);

		invlist = CONTAINER(ent, csi.idBackpack);
		CU_ASSERT_PTR_NULL_FATAL(invlist);

		count = GAMETEST_GetItemCount(ent, csi.idFloor);
		if (count > 0) {
			invList_t *entryToMove = FLOOR(ent);
			int tx, ty;
			INVSH_FindSpace(&ent->chr.i, &entryToMove->item, INVDEF(csi.idBackpack), &tx, &ty, entryToMove);
			if (tx != NONE) {
				Com_Printf("trying to move item %s from floor into backpack to pos %i:%i\n", entryToMove->item.t->name, tx, ty);
				CU_ASSERT_TRUE(G_ActorInvMove(ent, INVDEF(csi.idFloor), entryToMove, INVDEF(csi.idBackpack), tx, ty, qfalse));
				UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(GAMETEST_GetItemCount(ent, csi.idFloor), count - 1, va("item %s could not get moved successfully from floor into backpack", entryToMove->item.t->name));
				Com_Printf("item %s was removed from floor\n", entryToMove->item.t->name);
				UFO_CU_ASSERT_EQUAL_INT_MSG_FATAL(GAMETEST_GetItemCount(ent, csi.idBackpack), 1, va("item %s could not get moved successfully from floor into backpack", entryToMove->item.t->name));
				Com_Printf("item %s was moved successfully into the backpack\n", entryToMove->item.t->name);
				invlist = CONTAINER(ent, csi.idBackpack);
				CU_ASSERT_PTR_NOT_NULL_FATAL(invlist);
			}
		}

		SV_ShutdownGameProgs();
	} else {
		UFO_CU_FAIL_MSG(va("Map resource '%s.bsp' for test is missing.", mapName));
	}
}

static void testInventoryTempContainerLinks (void)
{
	const char *mapName = "test_game";
	if (FS_CheckFile("maps/%s.bsp", mapName) != -1) {
		edict_t* ent;
		int nr;
		containerIndex_t container;
		const invList_t *ic;

		/* the other tests didn't call the server shutdown function to clean up */
		OBJZERO(*sv);
		SV_Map(qtrue, mapName, NULL);
		level.activeTeam = TEAM_ALIEN;

		/* first alien that should die and drop its inventory */
		ent = G_EdictsGetNextLivingActorOfTeam(NULL, TEAM_ALIEN);
		nr = 0;
		for (container = 0; container < gi.csi->numIDs; container++) {
			if (container == gi.csi->idArmour || container == gi.csi->idFloor)
				continue;
			for (ic = CONTAINER(ent, container); ic; ic = ic->next)
				nr++;
		}
		CU_ASSERT_TRUE(nr > 0);

		CU_ASSERT_PTR_NULL(FLOOR(ent));
		G_InventoryToFloor(ent);
		CU_ASSERT_PTR_NOT_NULL(FLOOR(ent));
		CU_ASSERT_PTR_EQUAL(FLOOR(G_GetFloorItemsFromPos(ent->pos)), FLOOR(ent));

		nr = 0;
		for (container = 0; container < gi.csi->numIDs; container++) {
			if (container == gi.csi->idArmour || container == gi.csi->idFloor)
				continue;
			for (ic = CONTAINER(ent, container); ic; ic = ic->next)
				nr++;
		}
		CU_ASSERT_EQUAL(nr, 0);

		SV_ShutdownGameProgs();
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

	if (CU_ADD_TEST(GameSuite, testDoorTrigger) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GameSuite, testShooting) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GameSuite, testVisFlags) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GameSuite, testInventoryForDiedAlien) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GameSuite, testInventoryWithTwoDiedAliensOnTheSameGridTile) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(GameSuite, testInventoryTempContainerLinks) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
