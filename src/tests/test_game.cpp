/**
 * @file
 * @brief Test cases for code about server game logic
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include "../shared/ufotypes.h"
#include "../game/g_local.h"
#include "../game/g_actor.h"
#include "../game/g_client.h"
#include "../game/g_edicts.h"
#include "../game/g_inventory.h"
#include "../game/g_move.h"
#include "../server/server.h"
#include "../client/renderer/r_state.h"

class GameTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		/* we need the teamdefs for spawning ai actors */
		Com_ParseScripts(true);
		Cvar_Set("sv_threads", "0");

		sv_genericPool = Mem_CreatePool("server-gametest");
		r_state.active_texunit = &r_state.texunits[0];
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}

	void SetUp() {
		OBJZERO(*sv);
	}

	void TearDown() {
		SV_ShutdownGameProgs();
	}
};

TEST_F(GameTest, SpawnAndConnect)
{
	char userinfo[MAX_INFO_STRING];
	player_t* player;
	const char* name = "name";
	bool day = true;
	byte* buf;
	/* this entity string may not contain any inline models, we don't have the bsp tree loaded here */
	const int size = FS_LoadFile("game/entity.txt", &buf);
	Edict* e = nullptr;
	int cnt = 0;

	ASSERT_NE(size, -1);
	ASSERT_TRUE(size > 0);

	SV_InitGameProgs();
	/* otherwise we can't link the entities */
	SV_ClearWorld();

	player = G_PlayerGetNextHuman(0);
	svs.ge->SpawnEntities(name, day, (const char*)buf);
	ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));
	ASSERT_FALSE(svs.ge->RunFrame());

	while ((e = G_EdictsGetNextInUse(e))) {
		Com_Printf("entity %i: %s\n", cnt, e->classname);
		cnt++;
	}

	ASSERT_EQ(cnt, 45);

	FS_FreeFile(buf);
}

TEST_F(GameTest, CountSpawnpoints)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;
	int mapCount = 0;				// the number of maps read
	const int skipCount = 20;		// to skip the first n mapDefs

	Cvar_Set("rm_drop", "+craft_drop_herakles");

	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '.')
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;
		if (md->aircraft)	/* if the mapdef has a list of dropships, let's assume they bring their own spawnpoints */
			continue;

		mapCount++;
		if (mapCount <= skipCount)
			continue;
		/* use a known seed to reproduce an error */
		unsigned int seed;
		if (TEST_ExistsProperty("mapdef-seed")) {
			seed = TEST_GetLongProperty("mapdef-seed");
		} else {
			seed = (unsigned int) time(nullptr);
		}
		srand(seed);

		Com_Printf("testCountSpawnpoints: Mapdef %s (seed %u)\n", md->id, seed);

		const char* asmName = (const char*)LIST_GetByIdx(md->params, 0);
		SV_Map(true, md->mapTheme, asmName, false);

		Com_Printf("Map: %s Mapdef %s Spawnpoints: %i\n", md->mapTheme, md->id, level.num_spawnpoints[TEAM_PHALANX]);
		// Com_Printf("Map: %s Mapdef %s Seed %u Spawnpoints: %i\n", md->mapTheme, md->id, seed, level.num_spawnpoints[TEAM_PHALANX]);
		if (level.num_spawnpoints[TEAM_PHALANX] < 12)
			Com_Printf("Map %s: only %i spawnpoints !\n", md->mapTheme, level.num_spawnpoints[TEAM_PHALANX]);
	}
}

TEST_F(GameTest, DoorTrigger)
{
	const char* mapName = "test_game";
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	Edict* e = nullptr;
	int cnt = 0;
	int doors = 0;

	SV_Map(true, mapName, nullptr);
	while ((e = G_EdictsGetNextInUse(e))) {
		cnt++;
		if (e->type == ET_DOOR) {
			if (Q_streq(e->targetname, "left-0")) {
				ASSERT_TRUE(e->doorState) << "this one is triggered by an actor standing inside of a trigger_touch";
			} else if (Q_streq(e->targetname, "right-0")) {
				ASSERT_FALSE(e->doorState) << "this one has a trigger_touch, too - but nobody is touching that trigger yet";
			} else {
				ASSERT_TRUE(false) << "both of the used doors have a targetname set";
			}
			doors++;
		}
	}

	ASSERT_TRUE(cnt > 0);
	ASSERT_TRUE(doors == 2);
}

TEST_F(GameTest, Shooting)
{
	const char* mapName = "test_game";
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	SV_Map(true, mapName, nullptr);
	/** @todo equip the soldier */
	/** @todo set the input variables -- gi.ReadFormat(format, &pos, &i, &firemode, &from); */
	/** @todo do the shot -- G_ClientShoot(player, ent, pos, i, firemode, &mock, true, from); */
	/** @todo implement the test here - e.g. extend shot_mock_t */
	SV_ShutdownGameProgs();
}

static int GAMETEST_GetItemCount (const Edict* ent, containerIndex_t container)
{
	const Item* invlist = ent->getContainer(container);
	int count = 0;
	while (invlist != nullptr) {
		count += invlist->getAmount();
		invlist = invlist->getNext();
	}

	return count;
}

TEST_F(GameTest, VisFlags)
{
	const char* mapName = "test_game";
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	int num;

	SV_Map(true, mapName, nullptr);

	num = 0;
	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, TEAM_ALIEN))) {
		const teammask_t teamMask = G_TeamToVisMask(actor->getTeam());
		const bool visible = actor->visflags & teamMask;
		char* visFlagsBuf = Mem_StrDup(Com_UnsignedIntToBinary(actor->visflags));
		char* teamMaskBuf = Mem_StrDup(Com_UnsignedIntToBinary(teamMask));
		ASSERT_EQ(actor->getTeam(), TEAM_ALIEN);
		ASSERT_TRUE(visible) << "visflags: " << visFlagsBuf << ", teamMask: " << teamMaskBuf;
		Mem_Free(visFlagsBuf);
		Mem_Free(teamMaskBuf);
		num++;
	}

	ASSERT_TRUE(num > 0);
}

TEST_F(GameTest, InventoryForDiedAlien)
{
	const char* mapName = "test_game";
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	Actor* diedEnt;
	Actor* actor;
	Edict* floorItems;
	Item* invlist;
	int count;
	SV_Map(true, mapName, nullptr);
	level.activeTeam = TEAM_ALIEN;

	/* first alien that should die and drop its inventory */
	diedEnt = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != diedEnt);
	diedEnt->HP = 0;
	ASSERT_TRUE(G_ActorDieOrStun(diedEnt, nullptr));
	ASSERT_TRUE(diedEnt->isDead());

	/* now try to collect the inventory with a second alien */
	actor = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != actor);

	/* move to the location of the first died alien to drop the inventory into the same floor container */
	Player& player = actor->getPlayer();
	ASSERT_TRUE(G_IsAIPlayer(&player));
	G_ClientMove(player, 0, actor, diedEnt->pos);
	ASSERT_TRUE(VectorCompare(actor->pos, diedEnt->pos));

	floorItems = G_GetFloorItems(actor);
	ASSERT_TRUE(nullptr != floorItems);
	ASSERT_EQ(floorItems->getFloor(), actor->getFloor());

	/* drop everything to floor to make sure we have space in the backpack */
	G_InventoryToFloor(actor);
	ASSERT_EQ(GAMETEST_GetItemCount(actor, CID_BACKPACK), 0);

	invlist = actor->getContainer(CID_BACKPACK);
	ASSERT_TRUE(nullptr != invlist);
	count = GAMETEST_GetItemCount(actor, CID_FLOOR);
	if (count > 0) {
		Item* entryToMove = actor->getFloor();
		int tx, ty;
		actor->chr.inv.findSpace(INVDEF(CID_BACKPACK), entryToMove, &tx, &ty, entryToMove);
		if (tx != NONE) {
			Com_Printf("trying to move item %s from floor into backpack to pos %i:%i\n", entryToMove->def()->name, tx, ty);
			ASSERT_TRUE(G_ActorInvMove(actor, INVDEF(CID_FLOOR), entryToMove, INVDEF(CID_BACKPACK), tx, ty, false));
			ASSERT_EQ(GAMETEST_GetItemCount(actor, CID_FLOOR), count - 1) << "item " << entryToMove->def()->name << " could not get moved successfully from floor into backpack";
			Com_Printf("item %s was removed from floor\n", entryToMove->def()->name);
			ASSERT_EQ(GAMETEST_GetItemCount(actor, CID_BACKPACK), 1) << "item " << entryToMove->def()->name << " could not get moved successfully from floor into backpack";
			Com_Printf("item %s was moved successfully into the backpack\n", entryToMove->def()->name);
			invlist = actor->getContainer(CID_BACKPACK);
			ASSERT_TRUE(nullptr != invlist);
		}
	}
}

TEST_F(GameTest, InventoryWithTwoDiedAliensOnTheSameGridTile)
{
	const char* mapName = "test_game";
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	Actor* diedEnt;
	Actor* diedEnt2;
	Actor* actor;
	Edict* floorItems;
	Item* invlist;
	int count;
	SV_Map(true, mapName, nullptr);
	level.activeTeam = TEAM_ALIEN;

	/* first alien that should die and drop its inventory */
	diedEnt = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != diedEnt);
	diedEnt->HP = 0;
	G_ActorDieOrStun(diedEnt, nullptr);
	ASSERT_TRUE(diedEnt->isDead());

	/* second alien that should die and drop its inventory */
	diedEnt2 = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != diedEnt2);

	/* move to the location of the first died alien to drop the inventory into the same floor container */
	Player& player = diedEnt2->getPlayer();
	ASSERT_TRUE(G_IsAIPlayer(&player));
	G_ClientMove(player, 0, diedEnt2, diedEnt->pos);
	ASSERT_TRUE(VectorCompare(diedEnt2->pos, diedEnt->pos));

	diedEnt2->HP = 0;
	G_ActorDieOrStun(diedEnt2, nullptr);
	ASSERT_TRUE(diedEnt2->isDead());

	/* now try to collect the inventory with a third alien */
	actor = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != actor);

	player = actor->getPlayer();
	ASSERT_TRUE(G_IsAIPlayer(&player));

	G_ClientMove(player, 0, actor, diedEnt->pos);
	ASSERT_TRUE(VectorCompare(actor->pos, diedEnt->pos));

	floorItems = G_GetFloorItems(actor);
	ASSERT_TRUE(nullptr != floorItems);
	ASSERT_EQ(floorItems->getFloor(), actor->getFloor());

	/* drop everything to floor to make sure we have space in the backpack */
	G_InventoryToFloor(actor);
	ASSERT_EQ(GAMETEST_GetItemCount(actor, CID_BACKPACK), 0);

	invlist = actor->getContainer(CID_BACKPACK);
	ASSERT_TRUE(nullptr == invlist);

	count = GAMETEST_GetItemCount(actor, CID_FLOOR);
	if (count > 0) {
		Item* entryToMove = actor->getFloor();
		int tx, ty;
		actor->chr.inv.findSpace(INVDEF(CID_BACKPACK), entryToMove, &tx, &ty, entryToMove);
		if (tx != NONE) {
			Com_Printf("trying to move item %s from floor into backpack to pos %i:%i\n", entryToMove->def()->name, tx, ty);
			ASSERT_TRUE(G_ActorInvMove(actor, INVDEF(CID_FLOOR), entryToMove, INVDEF(CID_BACKPACK), tx, ty, false));
			ASSERT_EQ(GAMETEST_GetItemCount(actor, CID_FLOOR), count - 1) << "item " << entryToMove->def()->name << " could not get moved successfully from floor into backpack";
			Com_Printf("item %s was removed from floor\n", entryToMove->def()->name);
			ASSERT_EQ(GAMETEST_GetItemCount(actor, CID_BACKPACK), 1) << "item " << entryToMove->def()->name << " could not get moved successfully from floor into backpack";
			Com_Printf("item %s was moved successfully into the backpack\n", entryToMove->def()->name);
			invlist = actor->getContainer(CID_BACKPACK);
			ASSERT_TRUE(nullptr == invlist);
		}
	}
}

TEST_F(GameTest, InventoryTempContainerLinks)
{
	const char* mapName = "test_game";
	ASSERT_NE(-1, FS_CheckFile("maps/%s.bsp", mapName)) << "Map resource '" << mapName << ".bsp' for test is missing.";
	SV_Map(true, mapName, nullptr);
	level.activeTeam = TEAM_ALIEN;

	/* first alien that should die and drop its inventory */
	Actor* actor = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	int nr = 0;
	const Container* cont = nullptr;
	while ((cont = actor->chr.inv.getNextCont(cont, true))) {
		if (cont->id == CID_ARMOUR || cont->id == CID_FLOOR)
			continue;
		nr += cont->countItems();
	}
	ASSERT_TRUE(nr > 0);

	ASSERT_TRUE(nullptr == actor->getFloor());
	G_InventoryToFloor(actor);
	ASSERT_TRUE(nullptr != actor->getFloor());
	ASSERT_EQ(G_GetFloorItemFromPos(actor->pos)->getFloor(), actor->getFloor());

	nr = 0;
	cont = nullptr;
	while ((cont = actor->chr.inv.getNextCont(cont, true))) {
		if (cont->id == CID_ARMOUR || cont->id == CID_FLOOR)
			continue;
		nr += cont->countItems();
	}
	ASSERT_EQ(nr, 0);
}
