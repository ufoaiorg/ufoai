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

static int mapCount = 0;

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
	void testCountSpawnpointsForMapInSingleplayerMode(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo);
	void testCountSpawnpointsForMapInMultiplayerMode(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo);
	int testCountSpawnpointsGetNumteamValueForAircraft(const char *aircraft);
	int testCountSpawnpointsGetNumteamValueForUFO(const char *ufo);
	int testCountSpawnpointsGetNum2x2ValueForAircraft(const char *aircraft);

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

	ASSERT_EQ(cnt, 43);

	FS_FreeFile(buf);
}

int GameTest::testCountSpawnpointsGetNum2x2ValueForAircraft(const char *aircraft)
{
	/* Right now, 2x2 units are not yet implemented. But if we start to, it will probably
	   be humans bringing UGVs into battle. So we should make sure there are at least some
	   2x2 spawnpoints for TEAM_HUMAN on each map.
	   The values are arbitrary, as they are not defined within the scripts yet. */

	if (Q_strnull(aircraft)) {
		/* Could be any, so we return the max supported number. */
		return 3;
	} else if (Q_streq(aircraft, "craft_drop_firebird")) {
		return 3;
	} else if (Q_streq(aircraft, "craft_drop_raptor")) {
		return 3;
	} else if (Q_streq(aircraft, "craft_drop_herakles")) {
		return 3;
	} else {
		ADD_FAILURE() << "Error: Mapdef defines unknown aircraft: " << aircraft;
		return 0;
	}
}

int GameTest::testCountSpawnpointsGetNumteamValueForAircraft(const char *aircraft)
{
	// TODO: somehow fix these magic values here
	if (Q_strnull(aircraft)) {
		/* Could be any, so we return the max supported number. */
		return 12;
	} else if (Q_streq(aircraft, "craft_drop_firebird")) {
		return 8;
	} else if (Q_streq(aircraft, "craft_drop_raptor")) {
		return 10;
	} else if (Q_streq(aircraft, "craft_drop_herakles")) {
		return 12;
	} else {
		ADD_FAILURE() << "Error: Mapdef defines unknown aircraft: " << aircraft;
		return 0;
	}
}

int GameTest::testCountSpawnpointsGetNumteamValueForUFO(const char *ufo)
{
	// TODO: somehow fix these magic values here
	if (Q_strnull(ufo)) {
		/* Could be any, or none (ground based mission), so we return the max supported number. */
		return 30;
	} else if (Q_streq(ufo, "craft_ufo_scout") || Q_streq(ufo, "craft_crash_scout")) {
		return 4;
	} else if (Q_streq(ufo, "craft_ufo_fighter") || Q_streq(ufo, "craft_crash_fighter")) {
		return  6;
	} else if (Q_streq(ufo, "craft_ufo_harvester") || Q_streq(ufo, "craft_crash_harvester")) {
		return  8;
	} else if (Q_streq(ufo, "craft_ufo_corrupter") || Q_streq(ufo, "craft_crash_corrupter")) {
		return  10;
	} else if (Q_streq(ufo, "craft_ufo_supply") || Q_streq(ufo, "craft_crash_supply")) {
		return  12;
	} else if (Q_streq(ufo, "craft_ufo_gunboat") || Q_streq(ufo, "craft_crash_gunboat")) {
		return  12;
	} else if (Q_streq(ufo, "craft_ufo_bomber") || Q_streq(ufo, "craft_crash_bomber")) {
		return  18;
	} else if (Q_streq(ufo, "craft_ufo_ripper") || Q_streq(ufo, "craft_crash_ripper")) {
		return  14;
	} else {
		ADD_FAILURE() << "Error: Mapdef defines unknown UFO: " << ufo;
		return 0;
	}
}

void GameTest::testCountSpawnpointsForMapInMultiplayerMode(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	/* Make gtest report back the seed number along with any other message. */
	SCOPED_TRACE(va("seed: %u", seed));

	if (LIST_IsEmpty(md->gameTypes)) {
		ADD_FAILURE() << "Error: Multiplayer enabled, but no gametypes defined in mapdef " << md->id;
		return;
	}
	/* Set initial values. */
	Cvar_Set("sv_maxsoldiersperteam", "128");
	Cvar_Set("ai_multiplayeraliens", "128");
	Cvar_Set("ai_numcivilians", "128");

	/* Load map in multiplayer mode. */
	Cvar_Set("sv_maxclients", DOUBLEQUOTE(MAX_CLIENTS));
	Com_Printf("CountSpawnpoints - loading map: seed %i mode multiplayer mapdef %s map %s assembly %s dropship %s ufo %s\n", seed, md->id, md->mapTheme, asmName, aircraft, ufo);
	try {
		SV_Map(true, md->mapTheme, asmName, false);
	} catch (comDrop_t&) {
		ADD_FAILURE() << "Error: Failed to load assembly " << asmName << " from mapdef " << md->id << ", map "
			<< md->mapTheme << " aircraft: " << aircraft << ", ufo: " << ufo << " => multiplayer mode.";
		return;
	}

	mapCount++;
	const int spawnCivs = static_cast<int>(level.num_spawnpoints[TEAM_CIVILIAN]);

	/* Print report to log. */
	Com_Printf("CountSpawnpoints - map: mode multiplayer\n");
	Com_Printf("CountSpawnpoints - map: mapdef %s\n", md->id);
	Com_Printf("CountSpawnpoints - map: map %s\n", md->mapTheme);
	Com_Printf("CountSpawnpoints - map: assembly %s\n", asmName);
	Com_Printf("CountSpawnpoints - map: aircraft %s \n", aircraft);
	Com_Printf("CountSpawnpoints - map: ufo %s\n", ufo);
	Com_Printf("CountSpawnpoints - count spawnpoints: civilian %i\n", spawnCivs);

	/* Check if one of the gametypes available in the mapdef defines a coop mode,
	   in which case we will need aliens on the map. */
	int coop = 0;
	/* The number of alien spawnpoints required on the map. In PvP gamemodes this is zero,
	   while in coop games we check for the number given as 'maxaliens' in the mapdef. */
	int minAliens = 0;
	/* The number of player spawnpoints required for each team is determined
	   by the value of sv_maxsoldiersperteam given in the gametype def. */
	int minMP = 0;

	LIST_Foreach(md->gameTypes, const char, gameType) {
		for (int i = 0; i < csi.numGTs; i++) {
			const gametype_t* gt = &csi.gts[i];
			if (!Q_streq(gt->id, gameType))
				continue;
			const cvarlist_t* list = gt->cvars;
			for (int j = 0; j < gt->num_cvars; j++, list++) {
				if (Q_streq(list->name, "ai_multiplayeraliens")) {
					coop = std::max(coop, atoi(list->value));
				} else if (Q_streq(list->name, "sv_maxsoldiersperteam")) {
					minMP = std::max(minMP, atoi(list->value));
				}
			}
		}
	}
#if 0
	LIST_Foreach(md->gameTypes, const char, gameType) {
		/* For every mp gametype given in the mapdef ... */
		for (int i = 0; i < csi.numGTs; i++) {
			/* ... loop through the gametype defs to find the matching one. */
			const gametype_t* gt = &csi.gts[i];
			if (!Q_streq(gt->id, gameType))
				continue;
			/* Found the corresponding gametype def. */
			const cvarlist_t* list = gt->cvars;
			for (int j = 0; j < gt->num_cvars; j++, list++) {
				/* Loop through the gametype def and check for relevant keys. */
				if (Q_streq(list->name, "sv_ai")) {
					/* Set coop, if required. */
					coop = std::max(coop, atoi(list->value));
				} else if (Q_streq(list->name, "sv_maxsoldiersperteam")) {
					/* Set minMP to the highest required value. */
					minMP = std::max(minMP, atoi(list->value));
				}
			}
			break;
		}
	}

	coop = 1;
	minMP = 12;
#endif
	if (coop)
		minAliens = std::min(md->maxAliens, testCountSpawnpointsGetNumteamValueForUFO(ufo));

	const int startTeam = TEAM_CIVILIAN + 1;
	/* For every single mp team defined in the mapdef - check if there are enough spawnpoints available. */
	for (int currTeamNum = startTeam; currTeamNum < startTeam + md->teams; ++currTeamNum) {
		if (currTeamNum > TEAM_MAX_HUMAN) {
			ADD_FAILURE() << "Error: Mapdef " << md->id << " has too many teams set.";
			break;
		}
		const int spawnTeam = static_cast<int>(level.num_spawnpoints[currTeamNum]);
		/* Make gtest report back in case there are not enough spawnpoints available for the team. */
		EXPECT_GE(spawnTeam, minMP) << "Error: Assembly " << asmName << " from mapdef " << md->id << ", map " << md->mapTheme
			<< "(aircraft: " << aircraft << ", ufo: " << ufo << ") in multiplayer mode: Only " << spawnTeam
			<< " spawnpoints for team " << currTeamNum << " but " << minMP << "expected.";
		/* Log the result. */
		Com_Printf("CountSpawnpoints - count spawnpoints: player team/needs/found %i/%i/%i\n", currTeamNum, minMP, spawnTeam);
		if (spawnTeam < minMP)
			Com_Printf("CountSpawnpoints - error: missing spawnpoints - player team/needs/found %i/%i/%i\n", currTeamNum, minMP, spawnTeam);

	}
	if (minAliens) {
		const int spawnAliens = static_cast<int>(level.num_spawnpoints[TEAM_ALIEN]);
		/* Make gtest report back in case there are not enough alien spawnpoints available. */
		EXPECT_GE(spawnAliens, minAliens) << "Assembly " << asmName << " from mapdef " << md->id << ", map " << md->mapTheme
			<< "(aircraft: " << aircraft << ", ufo: " << ufo << ") in multiplayer mode defines at least one coop game mode,"
			<< " but does not have enough alien spawn positions for that. We expect at least " << minAliens
			<< " spawn positions for aliens, the map provides " << spawnAliens << ".";
		/* Log the result. */
		Com_Printf("CountSpawnpoints - count spawnpoints: alien needs/found %i/%i\n", minAliens, spawnAliens);
		if (spawnAliens < minAliens)
			Com_Printf("CountSpawnpoints - error: missing spawnpoints - alien needs/found %i/%i\n", minAliens, spawnAliens);
	}
}

void GameTest::testCountSpawnpointsForMapInSingleplayerMode(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	/* Make the test report back the seed number along with any other message. */
	SCOPED_TRACE(va("seed: %u", seed));

	Cvar_Set("sv_maxsoldiersperteam", "128");
	Cvar_Set("ai_singleplayeraliens", "128");
	Cvar_Set("ai_numcivilians", "128");

	/* load singleplayer map */
	Cvar_Set("sv_maxclients", "1");
	Com_Printf("CountSpawnpoints - loading map: seed %i mode singleplayer mapdef %s map %s assembly %s dropship %s ufo %s\n", seed, md->id, md->mapTheme, asmName, aircraft, ufo);
	try {
		SV_Map(true, md->mapTheme, asmName, false);
	} catch (comDrop_t&) {
		ADD_FAILURE() << "Error: Failed to load assembly " << asmName << " from mapdef " << md->id << ", map "
			<< md->mapTheme << " aircraft: " << aircraft << ", ufo: " << ufo << " => singleplayer mode.";
		return;
	}

	mapCount++;

	/* This is the max number of aliens the UFO can bring to the battlefield. */
	const int numteamUFO = testCountSpawnpointsGetNumteamValueForUFO(ufo);

	/* The number of human spawnpoints required on the map depends on the 'numteam' value
	   of the used dropship, if any. */
	const int minHumans = testCountSpawnpointsGetNumteamValueForAircraft(aircraft);

	/* The number of spawnpoints for 2x2 units required on the map depends on the number
	   of UGVs the aircraft can transport. */
	const int minHuman2x2 = testCountSpawnpointsGetNum2x2ValueForAircraft(aircraft);

	/* How many aliens do we need to have on the map, at least?
	   The mapdef defines the map to support up to 'maxaliens' aliens, so the map should have
	   at least this number of spawnpoints available.
	   However, this number must not be higher than the 'numteam' value of the used UFO, if any. */
	const int minAliens = std::min(md->maxAliens, numteamUFO);

	/* Count the spawnpoints available on the map. */
	const int spawnCivs = static_cast<int>(level.num_spawnpoints[TEAM_CIVILIAN]);
	const int spawnHumans = static_cast<int>(level.num_spawnpoints[TEAM_PHALANX]);
	const int spawnAliens = static_cast<int>(level.num_spawnpoints[TEAM_ALIEN]);
	const int spawnHuman2x2 = static_cast<int>(level.num_2x2spawnpoints[TEAM_PHALANX]);

	/* Make gtest report back in case there are not enough human spawnpoints. */
	EXPECT_GE(spawnHumans, minHumans) << "Error: Assembly " << asmName << " in mapdef " << md->id
		<< " from map " << md->mapTheme << " in singleplayer mode (aircraft: " << aircraft
		<< " ufo: " << ufo << "): Only " << spawnHumans << " human spawnpoints but " << minHumans
		<< " expected.";
	/* Make gtest report back in case there are not enough alien spawnpoints. */
	EXPECT_GE(spawnAliens, minAliens) << "Error: Assembly " << asmName << " in mapdef " << md->id
		<< " from map " << md->mapTheme << " in singleplayer mode (aircraft: " << aircraft
		<< " ufo: " << ufo << "): Only " << spawnAliens << " alien spawnpoints but " << minAliens
		<< " expected.";
	/* Make gtest report back in case there are not enough human 2x2 spawnpoints. */
	EXPECT_GE(spawnHuman2x2, minHuman2x2) << "Error: Assembly " << asmName << " in mapdef " << md->id
		<< " from map " << md->mapTheme << " in singleplayer mode (aircraft: " << aircraft
		<< " ufo: " << ufo << "): Only " << spawnHuman2x2 << " human 2x2 spawnpoints but " << minHuman2x2
		<< " expected.";
	/* Make gtest report back if the number of aliens allowed on the map is smaller than the crew number of the used UFO, if any. */
	if (ufo)
		EXPECT_GE(md->maxAliens, numteamUFO) << "Error: Assembly " << asmName << " in mapdef " << md->id
			<< " from map " << md->mapTheme << " in singleplayer mode (aircraft: " << aircraft
			<< " ufo: " << ufo << "): The number of aliens allowed on the map is smaller than expected.";

	/* Print report to log. */
	Com_Printf("CountSpawnpoints - map: mode singleplayer\n");
	Com_Printf("CountSpawnpoints - map: mapdef %s\n", md->id);
	Com_Printf("CountSpawnpoints - map: map %s\n", md->mapTheme);
	Com_Printf("CountSpawnpoints - map: assembly %s\n", asmName);
	Com_Printf("CountSpawnpoints - map: aircraft %s \n", aircraft);
	Com_Printf("CountSpawnpoints - map: ufo %s\n", ufo);
	Com_Printf("CountSpawnpoints - count spawnpoints: civilian %i\n", spawnCivs);
	Com_Printf("CountSpawnpoints - count spawnpoints: singleplayer needs/found %i/%i\n", minHumans, spawnHumans);
	Com_Printf("CountSpawnpoints - count spawnpoints: singleplayer_2x2 needs/found %i/%i\n", minHuman2x2, spawnHuman2x2);
	Com_Printf("CountSpawnpoints - count spawnpoints: alien needs/found %i/%i\n", minAliens, spawnAliens);
	if (spawnHumans < minHumans)
		Com_Printf("CountSpawnpoints - error: missing spawnpoints - singleplayer needs/found %i/%i\n", minHumans, spawnHumans);
	if (spawnHuman2x2 < minHuman2x2)
		Com_Printf("CountSpawnpoints - error: missing spawnpoints - singleplayer_2x2 needs/found %i/%i\n", minHuman2x2, spawnHuman2x2);
	if (spawnAliens < minAliens)
		Com_Printf("CountSpawnpoints - error: missing spawnpoints - alien needs/found %i/%i\n", minAliens, spawnAliens);
	if (ufo) {
		if (md->maxAliens < numteamUFO) {
			Com_Printf("CountSpawnpoints - error: mapdef parameter - maxaliens needs/found %i/%i\n", numteamUFO, md->maxAliens);
		}
	}
}

void GameTest::testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	/* The ufocrash map is a special one. The mapdef should not define single- nor
	multiplayer mode. It uses one assembly for each ufo defined in the mapdef,
	where the assembly name is equal the name of the UFO. */
	if (Q_streq(md->id, "ufocrash")) {
		testCountSpawnpointsForMapInSingleplayerMode(seed, md, ufo, aircraft, ufo);
		return;
	}
	if (md->singleplayer || md->campaign)
		testCountSpawnpointsForMapInSingleplayerMode(seed, md, asmName, aircraft, ufo);

	if (md->multiplayer)
		testCountSpawnpointsForMapInMultiplayerMode(seed, md, asmName, aircraft, ufo);
}

void GameTest::testCountSpawnpointsForMapWithAssemblyAndAircraft(unsigned int seed, const mapDef_t *md, const char *asmName, const char *aircraft)
{
	if (LIST_IsEmpty(md->ufos)) {
		/* The mapdef defines no UFOs */
		Cvar_Set("rm_ufo", "");
		testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(seed, md, asmName, aircraft, nullptr);
	} else {
		LIST_Foreach(md->ufos, const char, ufo) {
			Cvar_Set("rm_ufo", "%s", Com_GetRandomMapAssemblyNameForCraft(ufo));
			testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(seed, md, asmName, aircraft, ufo);
		}
	}
}

void GameTest::testCountSpawnpointsForMapWithAssembly(unsigned int seed, const mapDef_t *md, const char *asmName)
{
	if (LIST_IsEmpty(md->aircraft)) {
		/* There is no aircraft defined in the mapdef. */
		Cvar_Set("rm_drop", "");
		testCountSpawnpointsForMapWithAssemblyAndAircraft(seed, md, asmName, nullptr);
	} else {
		LIST_Foreach(md->aircraft, const char, aircraft) {
			Cvar_Set("rm_drop", "%s", Com_GetRandomMapAssemblyNameForCraft(aircraft));
			testCountSpawnpointsForMapWithAssemblyAndAircraft(seed, md, asmName, aircraft);
		}
	}
}

void GameTest::testCountSpawnpointsForMap(unsigned int seed, const mapDef_t *md)
{
	if (md->mapTheme[0] == '.')
		return;

	/* Check if we are only testing a certain mapdef. */
	char* filterId;
	if (TEST_ExistsProperty("mapdef-id")){
		filterId = strdup(TEST_GetStringProperty("mapdef-id"));
		if (filterId && !Q_streq(filterId, md->id)) {
			return;
		}
	}

	Com_Printf("\nCountSpawnpoints - test start: mapdef %s %s\n", md->mapTheme, md->id);
	/* Calling std::cout also prevents the test from timing out on buildbot. */
	if (LIST_IsEmpty(md->params)) {
		std::cout << "[          ] testing mapdef: " << md->id << std::endl;
		testCountSpawnpointsForMapWithAssembly(seed, md, nullptr);
	} else {
		LIST_Foreach(md->params, const char, asmName) {
			std::cout << "[          ] testing mapdef: " << md->id << ", assembly: "
				<< asmName << std::endl;
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

	mapCount = 0;
	const mapDef_t* md;
	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '+')
			continue;
		testCountSpawnpointsForMap(seed, md);
	}
	Com_Printf("CountSpawnpoints - maps tested: RMA %i\n", mapCount);
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

	mapCount = 0;
	const mapDef_t* md;
	MapDef_Foreach(md) {
		if (md->mapTheme[0] != '+')
			continue;

		testCountSpawnpointsForMap(seed, md);
	}
	Com_Printf("CountSpawnpoints - maps tested: RMA %i\n", mapCount);
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
