/**
 * @file
 * @brief Test cases for code about server game logic
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
		sv_maxclients = Cvar_Get("sv_maxclients", "1", CVAR_SERVERINFO, "Max. connected clients for test");
		port = Cvar_Get("port", DOUBLEQUOTE(PORT_SERVER), CVAR_NOSET);
		masterserver_url = Cvar_Get("masterserver_url", MASTER_SERVER, CVAR_ARCHIVE, "URL of UFO:AI masterserver");

		sv_genericPool = Mem_CreatePool("server-gametest");
		com_networkPool = Mem_CreatePool("server-gametest-network");
		r_state.active_texunit = &r_state.texunits[0];
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
	}

	void testCountSpawnpointsForMap(unsigned int seed, const mapDef_t *md);
	void testCountSpawnpointsForMapWithAssembly(unsigned int seed, const mapDef_t *md, const char *asmName);
	void testCountSpawnpointsForMapWithAssemblyAndAircraft(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft);
	void testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo);

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

	ASSERT_NE(size, -1) << "could not load game/entity.txt.";
	ASSERT_TRUE(size > 0) << "game/entity.txt is empty.";

	SV_InitGameProgs();
	/* otherwise we can't link the entities */
	SV_ClearWorld();

	player = G_PlayerGetNextHuman(0);
	svs.ge->SpawnEntities(name, day, (const char*)buf);
	ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo))) << "Failed to connect the client";
	ASSERT_FALSE(svs.ge->RunFrame()) << "Failed to run the server logic frame tick";

	while ((e = G_EdictsGetNextInUse(e))) {
		Com_Printf("entity %i: %s\n", cnt, e->classname);
		cnt++;
	}

	ASSERT_EQ(cnt, 45);

	FS_FreeFile(buf);
}

void GameTest::testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	SCOPED_TRACE(va("seed: %u", seed));

	// TODO: somehow fix these magic values here
	int maxPlayers;
	if (Q_strnull(aircraft)) {
		Cvar_Set("rm_drop", "");
		maxPlayers = 12;
	} else {
		Cvar_Set("rm_drop", "%s", Com_GetRandomMapAssemblyNameForCraft(aircraft));
		if (Q_streq(aircraft, "craft_drop_firebird"))
			maxPlayers = 8;
		else if (Q_streq(aircraft, "craft_drop_raptor"))
			maxPlayers = 10;
		else if (Q_streq(aircraft, "craft_drop_herakles"))
			maxPlayers = 12;
		else {
			ADD_FAILURE() << "Map " << md->mapTheme << " from mapdef " << md->id << " with unexpected aircraft";
			return;
		}
	}
	if (Q_strnull(ufo)) {
		Cvar_Set("rm_ufo", "");
	} else {
		Cvar_Set("rm_ufo", "%s", Com_GetRandomMapAssemblyNameForCraft(ufo));
	}

	if (md->multiplayer) {
		Cvar_Set("sv_maxclients", DOUBLEQUOTE(MAX_CLIENTS));
	} else {
		Cvar_Set("sv_maxclients", "1");
	}
	Cvar_Set("sv_maxsoldiersperteam", "256");
	Cvar_Set("ai_multiplayeraliens", "256");
	Cvar_Set("ai_singleplayeraliens", "256");
	Cvar_Set("ai_numcivilians", "256");

	try {
		SV_Map(true, md->mapTheme, asmName, false);
	} catch (comDrop_t&) {
		ADD_FAILURE() << "failed to load map " << md->mapTheme << " from mapdef " << md->id;
		return;
	}

	if (md->multiplayer) {
		int maxAliensForCoop = 0;
		int expectedMultiplayerSpawnPoints = 0;
		ASSERT_FALSE(LIST_IsEmpty(md->gameTypes)) << "No gametypes set for mapdef " << md->id;
		LIST_Foreach(md->gameTypes, const char, gameType) {
			for (int i = 0; i < csi.numGTs; i++) {
				const gametype_t* gt = &csi.gts[i];
				if (!Q_streq(gt->id, gameType))
					continue;
				const cvarlist_t* list = gt->cvars;
				for (int j = 0; j < gt->num_cvars; j++, list++) {
					if (Q_streq(list->name, "ai_multiplayeraliens")) {
						maxAliensForCoop = std::max(maxAliensForCoop, atoi(list->value));
					} else if (Q_streq(list->name, "sv_maxsoldiersperteam")) {
						expectedMultiplayerSpawnPoints = std::max(expectedMultiplayerSpawnPoints, atoi(list->value));
					}
				}
			}
		}

		const int startTeam = TEAM_CIVILIAN + 1;
		for (int i = startTeam; i < startTeam + md->teams; ++i) {
			ASSERT_TRUE(i <= TEAM_MAX_HUMAN) << "Map " << md->mapTheme << " from mapdef " << md->id << " has too many team set";
			const int spawnPoints = static_cast<int>(level.num_spawnpoints[i]);
			Com_Printf("Map: %s Mapdef %s Spawnpoints: %i\n", md->mapTheme, md->id, spawnPoints);
			EXPECT_GE(spawnPoints, maxPlayers) << "Map " << md->mapTheme
					<< " from mapdef " << md->id << " only " << spawnPoints << " spawnpoints for team " << i << " (aircraft: "
					<< aircraft << ") (ufo: " << ufo << ") => multiplayer mode";
			EXPECT_GE(spawnPoints, expectedMultiplayerSpawnPoints) << "Map " << md->mapTheme
					<< " from mapdef " << md->id << " only " << spawnPoints << " spawnpoints for team " << i << " (aircraft: "
					<< aircraft << ") (ufo: " << ufo << ") (gametype wants more spawn positions) => multiplayer mode";
		}
		const int alienSpawnPoints = static_cast<int>(level.num_spawnpoints[TEAM_ALIEN]);
		EXPECT_GE(alienSpawnPoints, maxAliensForCoop) << "Map " << md->mapTheme
							<< " from mapdef " << md->id << " defines a coop game mode but does not have enough alien spawn positions for that. We would need "
							<< maxAliensForCoop << " spawn positions for aliens => multiplayer mode";
	} else {
		const int spawnPoints = static_cast<int>(level.num_spawnpoints[TEAM_PHALANX]);
		Com_Printf("Map: %s Mapdef %s Spawnpoints: %i\n", md->mapTheme, md->id, spawnPoints);
		EXPECT_GE(spawnPoints, maxPlayers) << "Map " << md->mapTheme
				<< " from mapdef " << md->id << " only " << spawnPoints << " human spawnpoints (aircraft: "
				<< aircraft << ") (ufo: " << ufo << ") => singleplayer mode";
		const int alienSpawnPoints = static_cast<int>(level.num_spawnpoints[TEAM_ALIEN]);
		EXPECT_GE(alienSpawnPoints, 1) << "Map " << md->mapTheme
				<< " from mapdef " << md->id << " only " << alienSpawnPoints << " alien spawnpoints (aircraft: "
				<< aircraft << ") (ufo: " << ufo << ") => singleplayer mode";
		EXPECT_GE(alienSpawnPoints, md->maxAliens) << "Map " << md->mapTheme
				<< " from mapdef " << md->id << " only " << alienSpawnPoints << " alien spawnpoints but " << md->maxAliens
				<< " expected (aircraft: " << aircraft << ") (ufo: " << ufo << ") => singleplayer mode";
	}
}

void GameTest::testCountSpawnpointsForMapWithAssemblyAndAircraft(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft)
{
	if (LIST_IsEmpty(md->ufos)) {
		testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(seed, md, asmName, aircraft, nullptr);
	} else {
		LIST_Foreach(md->ufos, const char, ufo) {
			testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(seed, md, asmName, aircraft, ufo);
		}
	}
}

void GameTest::testCountSpawnpointsForMapWithAssembly(unsigned int seed, const mapDef_t *md, const char *asmName)
{
	if (LIST_IsEmpty(md->aircraft)) {
		const humanAircraftType_t types[] = { DROPSHIP_FIREBIRD, DROPSHIP_HERAKLES, DROPSHIP_RAPTOR };
		for (int i = 0; i < lengthof(types); ++i) {
			const humanAircraftType_t t = types[i];
			const char *aircraft = Com_DropShipTypeToShortName(t);
			testCountSpawnpointsForMapWithAssemblyAndAircraft(seed, md, asmName, aircraft);
		}
	} else {
		LIST_Foreach(md->aircraft, const char, aircraft) {
			testCountSpawnpointsForMapWithAssemblyAndAircraft(seed, md, asmName, aircraft);
		}
	}
}

void GameTest::testCountSpawnpointsForMap(unsigned int seed, const mapDef_t *md)
{
	if (md->mapTheme[0] == '.')
		return;

	const char* filterId = TEST_GetStringProperty("mapdef-id");
	if (filterId && !Q_streq(filterId, md->id))
		return;

	if (LIST_IsEmpty(md->params)) {
		testCountSpawnpointsForMapWithAssembly(seed, md, nullptr);
	} else {
		LIST_Foreach(md->params, const char, asmName) {
			testCountSpawnpointsForMapWithAssembly(seed, md, asmName);
		}
	}
}

TEST_F(GameTest, CountSpawnpointsStatic)
{
	/* use a known seed to reproduce an error */
	unsigned int seed;
	if (TEST_ExistsProperty("mapdef-seed")) {
		seed = TEST_GetLongProperty("mapdef-seed");
	} else {
		seed = (unsigned int) time(nullptr);
	}
	srand(seed);

	const mapDef_t* md;
	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '+')
			continue;
		testCountSpawnpointsForMap(seed, md);
	}
}

TEST_F(GameTest, CountSpawnpointsRMA)
{
	/* use a known seed to reproduce an error */
	unsigned int seed;
	if (TEST_ExistsProperty("mapdef-seed")) {
		seed = TEST_GetLongProperty("mapdef-seed");
	} else {
		seed = (unsigned int) time(nullptr);
	}
	srand(seed);

	const mapDef_t* md;
	MapDef_Foreach(md) {
		if (md->mapTheme[0] != '+')
			continue;
		testCountSpawnpointsForMap(seed, md);
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
		if (e->type != ET_DOOR)
			continue;
		if (Q_streq(e->targetname, "left-0")) {
			ASSERT_TRUE(e->doorState) << "this one is triggered by an actor standing inside of a trigger_touch";
		} else if (Q_streq(e->targetname, "right-0")) {
			ASSERT_FALSE(e->doorState) << "this one has a trigger_touch, too - but nobody is touching that trigger yet";
		} else {
			ASSERT_TRUE(false) << "both of the used doors have a targetname set";
		}
		doors++;
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

	ASSERT_TRUE(num > 0) << "No alien actors found";
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
	ASSERT_EQ(0, GAMETEST_GetItemCount(actor, CID_BACKPACK));

	invlist = actor->getContainer(CID_BACKPACK);
	ASSERT_TRUE(nullptr == invlist);
	count = GAMETEST_GetItemCount(actor, CID_FLOOR);
	if (count > 0) {
		Item* entryToMove = actor->getFloor();
		int tx, ty;
		actor->chr.inv.findSpace(INVDEF(CID_BACKPACK), entryToMove, &tx, &ty, entryToMove);
		if (tx == NONE)
			return;
		Com_Printf("trying to move item %s from floor into backpack to pos %i:%i\n", entryToMove->def()->name, tx, ty);
		ASSERT_TRUE(G_ActorInvMove(actor, INVDEF(CID_FLOOR), entryToMove, INVDEF(CID_BACKPACK), tx, ty, false));
		ASSERT_EQ(count - 1, GAMETEST_GetItemCount(actor, CID_FLOOR)) << "item " << entryToMove->def()->name << " could not get moved successfully from floor into backpack";
		Com_Printf("item %s was removed from floor\n", entryToMove->def()->name);
		ASSERT_EQ(1, GAMETEST_GetItemCount(actor, CID_BACKPACK)) << "item " << entryToMove->def()->name << " could not get moved successfully from floor into backpack";
		Com_Printf("item %s was moved successfully into the backpack\n", entryToMove->def()->name);
		invlist = actor->getContainer(CID_BACKPACK);
		ASSERT_TRUE(nullptr != invlist);
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
	ASSERT_TRUE(nullptr != diedEnt) << "No living actor in the alien team";
	diedEnt->HP = 0;
	G_ActorDieOrStun(diedEnt, nullptr);
	ASSERT_TRUE(diedEnt->isDead()) << "Actor is not dead";

	/* second alien that should die and drop its inventory */
	diedEnt2 = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != diedEnt2) << "No living actor in the alien team";

	/* move to the location of the first died alien to drop the inventory into the same floor container */
	Player& player = diedEnt2->getPlayer();
	ASSERT_TRUE(G_IsAIPlayer(&player));
	G_ClientMove(player, 0, diedEnt2, diedEnt->pos);
	ASSERT_TRUE(VectorCompare(diedEnt2->pos, diedEnt->pos));

	diedEnt2->HP = 0;
	G_ActorDieOrStun(diedEnt2, nullptr);
	ASSERT_TRUE(diedEnt2->isDead()) << "Actor is not dead";

	/* now try to collect the inventory with a third alien */
	actor = G_EdictsGetNextLivingActorOfTeam(nullptr, TEAM_ALIEN);
	ASSERT_TRUE(nullptr != actor) << "No living actor in the alien team";

	player = actor->getPlayer();
	ASSERT_TRUE(G_IsAIPlayer(&player)) << "Player is not ai controlled";

	G_ClientMove(player, 0, actor, diedEnt->pos);
	ASSERT_TRUE(VectorCompare(actor->pos, diedEnt->pos)) << "Actor is not at the same position as the died entity";

	floorItems = G_GetFloorItems(actor);
	ASSERT_TRUE(nullptr != floorItems);
	ASSERT_EQ(floorItems->getFloor(), actor->getFloor());

	/* drop everything to floor to make sure we have space in the backpack */
	G_InventoryToFloor(actor);
	ASSERT_EQ(0, GAMETEST_GetItemCount(actor, CID_BACKPACK));

	invlist = actor->getContainer(CID_BACKPACK);
	ASSERT_TRUE(nullptr == invlist);

	count = GAMETEST_GetItemCount(actor, CID_FLOOR);
	if (count > 0) {
		Item* entryToMove = actor->getFloor();
		int tx, ty;
		actor->chr.inv.findSpace(INVDEF(CID_BACKPACK), entryToMove, &tx, &ty, entryToMove);
		if (tx == NONE)
			return;
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
	ASSERT_TRUE(nr > 0) << "Could not find any items in the inventory of the actor";

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
	ASSERT_EQ(0, nr);
}
