/**
 * @file
 * @brief Test cases for code about server game logic
 */

/*
Copyright (C) 2002-2019 UFO: Alien Invasion.

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

	void testCountSpawnpointsForMap(bool verbose, const mapDef_t *md);
	void testCountSpawnpointsForMapWithAssembly(bool verbose, const mapDef_t *md, const char *asmName);
	void testCountSpawnpointsForMapWithAssemblyAndAircraft(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft);
	void testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo);
	void testCountSpawnpointsForMapInSingleplayerMode(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo);
	void testCountSpawnpointsForMapInMultiplayerMode(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo);
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

void GameTest::testCountSpawnpointsForMapInMultiplayerMode(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	if (verbose) {
		std::cout << "[          ] adding test parameter: gamemode multiplayer" << std::endl;
		Com_Printf("CountSpawnpoints - adding test parameter: gamemode multiplayer\n");
	}

	if (LIST_IsEmpty(md->gameTypes)) {
		ADD_FAILURE() << "Error: Multiplayer enabled, but no gametypes defined in mapdef " << md->id;
		Com_Printf("CountSpawnpoints - error: Multiplayer enabled, but no gametypes defined in mapdef.\n");
		return;
	}

	if (md->teams < 1) {
		ADD_FAILURE() << "Error: Multiplayer enabled, but number of teams is " << md->teams;
		Com_Printf("CountSpawnpoints - error: Multiplayer enabled, but number of teams is %i.\n", md->teams);
		return;
	}

	/* Set initial values. */
	/* Load map in multiplayer mode. */
	Cvar_Set("sv_maxclients", DOUBLEQUOTE(MAX_CLIENTS));
	/* It seems, because of reaction-fire limitations we cannot spawn more than 128 actors total. */
	Cvar_Set("sv_maxsoldiersperteam", "12");
	Cvar_Set("ai_multiplayeraliens", "64");
	Cvar_Set("ai_numcivilians", "16");

	Com_Printf("CountSpawnpoints - loading map: mode multiplayer mapdef %s map %s assembly %s dropship %s ufo %s\n", md->id, md->mapTheme, asmName, aircraft, ufo);
	long time = Sys_Milliseconds();

	try {
		SV_Map(true, md->mapTheme, asmName, true);
	} catch (comDrop_t&) {
		ADD_FAILURE() << "Error: Failed to load assembly " << asmName << " from mapdef " << md->id << ", map "
			<< md->mapTheme << " aircraft: " << aircraft << ", ufo: " << ufo << " => multiplayer mode.";
			Com_Printf("CountSpawnpoints - error: Multiplayer enabled, but no gametypes defined in mapdef.\n");
		Com_Printf("CountSpawnpoints - error: Failed to load map.\n");
		SV_ShutdownGameProgs();
		return;
	}

	time = Sys_Milliseconds() - time;
	Com_Printf("CountSpawnpoints - result: %li ms\n", time);
	SV_ShutdownGameProgs();
	mapCount++;

	/* Print report to log. */
	Com_Printf("CountSpawnpoints - map: mode multiplayer\n");
	Com_Printf("CountSpawnpoints - map: mapdef %s\n", md->id);
	Com_Printf("CountSpawnpoints - map: map %s\n", md->mapTheme);
	Com_Printf("CountSpawnpoints - map: assembly %s\n", asmName);
	Com_Printf("CountSpawnpoints - map: aircraft %s \n", aircraft);
	Com_Printf("CountSpawnpoints - map: ufo %s\n", ufo);

	/* Check if one of the gametypes available in the mapdef defines a coop mode,
	   in which case we will need aliens on the map. */
	int coop = 0;
	/* The number of alien spawnpoints required on the map. In PvP gamemodes this is zero,
	   while in coop games we check for the number given as 'maxaliens' in the mapdef. */
	int minAliens = 0;
	/* The number of player spawnpoints required for each team is determined
	   by the value of sv_maxsoldiersperteam given in the gametype def. */
	int minMP = 0;

	/* Count spawnpoints for TEAM_CIVILIAN. */
	const int spawnCivs = static_cast<int>(level.num_spawnpoints[TEAM_CIVILIAN]);
	Com_Printf("CountSpawnpoints - count spawnpoints: civilian %i\n", spawnCivs);

	/* Find the highest numbers for alien and player spawnpoints needed in the map. */
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

	/* If the mapdef does not define a coop mode, we do not need aliens. */
	if (coop)
		minAliens = std::min(md->maxAliens, testCountSpawnpointsGetNumteamValueForUFO(ufo));

	const int startTeam = TEAM_CIVILIAN + 1;
	/* For every single mp team defined in the mapdef - check if there are enough spawnpoints available. */
	for (int currTeamNum = startTeam; currTeamNum < startTeam + md->teams; ++currTeamNum) {
		if (currTeamNum > TEAM_MAX_HUMAN) {
			ADD_FAILURE() << "Error: Mapdef " << md->id << " has too many teams set.";
			Com_Printf("CountSpawnpoints - error: Too many teams set.\n");
			break;
		}
		const int spawnTeam = static_cast<int>(level.num_spawnpoints[currTeamNum]);
		/* Make gtest report back in case there are not enough spawnpoints available for the team. */
		EXPECT_GE(spawnTeam, minMP) << "Error: Assembly " << asmName << " from mapdef " << md->id << ", map " << md->mapTheme
			<< "(aircraft: " << aircraft << ", ufo: " << ufo << ") in multiplayer mode: Only " << spawnTeam
			<< " spawnpoints for team " << currTeamNum << " but " << minMP << " expected.";
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

	SV_ShutdownGameProgs();
}

void GameTest::testCountSpawnpointsForMapInSingleplayerMode(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	if (verbose) {
		std::cout << "[          ] adding test parameter: gamemode singleplayer" << std::endl;
		Com_Printf("CountSpawnpoints - adding test parameter: gamemode singleplayer\n");
	}

	/* Set initial values. */
	/* load singleplayer map */
	Cvar_Set("sv_maxclients", "1");
	/* It seems, because of reaction-fire limitations we cannot spawn more than 128 actors total. */
	Cvar_Set("sv_maxsoldiersperteam", "12");
	Cvar_Set("ai_singleplayeraliens", "64");
	Cvar_Set("ai_numcivilians", "16");

	Com_Printf("CountSpawnpoints - loading map: mode singleplayer mapdef %s map %s assembly %s dropship %s ufo %s\n", md->id, md->mapTheme, asmName, aircraft, ufo);
	long time = Sys_Milliseconds();

	try {
		SV_Map(true, md->mapTheme, asmName, true);
	} catch (comDrop_t&) {
		ADD_FAILURE() << "Error: Failed to load assembly " << asmName << " from mapdef " << md->id << ", map "
			<< md->mapTheme << " aircraft: " << aircraft << ", ufo: " << ufo << " => singleplayer mode.";
			Com_Printf("CountSpawnpoints - error: Failed to load map.\n");
		SV_ShutdownGameProgs();
		return;
	}
	time = Sys_Milliseconds() - time;
	Com_Printf("CountSpawnpoints - result: %li ms\n", time);
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
	if (ufo) {
		EXPECT_GE(md->maxAliens, numteamUFO) << "Error: Assembly " << asmName << " in mapdef " << md->id
			<< " from map " << md->mapTheme << " in singleplayer mode (aircraft: " << aircraft
			<< " ufo: " << ufo << "): The number of aliens allowed on the map is smaller than expected.";
	}

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

	SV_ShutdownGameProgs();
}

void GameTest::testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft, const char *ufo)
{
	if (verbose && ufo) {
		std::cout << "[          ] adding test parameter: ufo " << ufo << std::endl;
		Com_Printf("CountSpawnpoints - adding test parameter: ufo %s\n", ufo);
	}

	/* The ufocrash map is a special one. The mapdef should not define single- nor
	multiplayer mode. It uses one assembly for each ufo defined in the mapdef,
	where the assembly name is equal the name of the UFO. */
	if (Q_streq(md->id, "ufocrash")) {
		testCountSpawnpointsForMapInSingleplayerMode(verbose, md, ufo, aircraft, ufo);
		return;
	}

	/* Check if we are manually testing a certain gamemode. */
	if (TEST_ExistsProperty("mode")) {
		const char *mode = TEST_GetStringProperty("mode");
		if (Q_streq(mode, "sp")) {
			if (md->singleplayer || md->campaign) {
				testCountSpawnpointsForMapInSingleplayerMode(verbose, md, asmName, aircraft, ufo);
			} else {
				Com_Printf("CountSpawnpoints - error: Gamemode not defined in mapdef: %s\n", mode);
				ADD_FAILURE() << "Error: Gamemode not defined in mapdef: " << mode;
			}
		} else if (Q_streq(mode, "mp")) {
			if (md->multiplayer) {
				testCountSpawnpointsForMapInMultiplayerMode(verbose, md, asmName, aircraft, ufo);
			} else {
				Com_Printf("CountSpawnpoints - error: Gamemode not defined in mapdef: %s\n", mode);
				ADD_FAILURE() << "Error: Gamemode not defined in mapdef: " << mode;
			}
		} else {
			Com_Printf("CountSpawnpoints - error: Not a valid gamemode: %s\n", mode);
			ADD_FAILURE() << "Error: Not a valid gamemode: " << mode;
		}
	} else {
		/* Test every gamemode defined in the mapdef. */
		if (md->singleplayer || md->campaign) {
			testCountSpawnpointsForMapInSingleplayerMode(verbose, md, asmName, aircraft, ufo);
		}
		if (md->multiplayer) {
			testCountSpawnpointsForMapInMultiplayerMode(verbose, md, asmName, aircraft, ufo);
		}
	}
}

void GameTest::testCountSpawnpointsForMapWithAssemblyAndAircraft(bool verbose, const mapDef_t *md, const char *asmName, const char *aircraft)
{
	if (verbose && aircraft) {
		std::cout << "[          ] adding test parameter: aircraft " << aircraft << std::endl;
		Com_Printf("CountSpawnpoints - adding test parameter: aircraft %s\n", aircraft);
	}

	/* Check if we are manually testing a certain UFO type. */
	if (TEST_ExistsProperty("ufo")) {
		const char *ufo = TEST_GetStringProperty("ufo");
		int tested = 0;
		LIST_Foreach(md->ufos, const char, s) {
			if (Q_streq(ufo, s)) {
				Cvar_Set("rm_ufo", "%s", Com_GetRandomMapAssemblyNameForCraft(ufo));
				testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(verbose, md, asmName, aircraft, ufo);
				tested += 1;
			}
		}
		if (tested < 1) {
			Com_Printf("CountSpawnpoints - error: Not a valid UFO id: %s\n", ufo);
			ADD_FAILURE() << "Error: Not a valid ufo id: " << ufo;
		}
	} else if (LIST_IsEmpty(md->ufos)) {
		/* The mapdef defines no UFOs. */
		Cvar_Set("rm_ufo", "");
		testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(verbose, md, asmName, aircraft, nullptr);
	} else {
		/* Test every UFO defined in the mapdef. */
		LIST_Foreach(md->ufos, const char, ufo) {
			Cvar_Set("rm_ufo", "%s", Com_GetRandomMapAssemblyNameForCraft(ufo));
			testCountSpawnpointsForMapWithAssemblyAndAircraftAndUfo(verbose, md, asmName, aircraft, ufo);
		}
	}
}

void GameTest::testCountSpawnpointsForMapWithAssembly(bool verbose, const mapDef_t *md, const char *asmName)
{
	if (asmName) {
		std::cout << "[          ] testing mapdef: " << md->id << " assembly: " << asmName << std::endl;
		Com_Printf("CountSpawnpoints - adding test parameter: assembly %s\n", asmName);
	} else {
		std::cout << "[          ] testing mapdef: " << md->id << std::endl;
	}

	/* Check if we are manually testing a certain aircraft. */
	if (TEST_ExistsProperty("aircraft")) {
		const char *aircraft = TEST_GetStringProperty("aircraft");
		int tested = 0;
		LIST_Foreach(md->aircraft, const char, s) {
			if (Q_streq(aircraft, s)) {
				Cvar_Set("rm_drop", "%s", Com_GetRandomMapAssemblyNameForCraft(aircraft));
				testCountSpawnpointsForMapWithAssemblyAndAircraft(verbose, md, asmName, aircraft);
				tested += 1;
			}
		}
		if (tested < 1) {
			Com_Printf("CountSpawnpoints - error: Not a valid aircraft id: %s\n", aircraft);
			ADD_FAILURE() << "Error: Not a valid aircraft id: " << aircraft;
		}
	} else if (LIST_IsEmpty(md->aircraft)) {
		/* There is no aircraft defined in the mapdef. */
		Cvar_Set("rm_drop", "");
		testCountSpawnpointsForMapWithAssemblyAndAircraft(verbose, md, asmName, nullptr);
	} else {
		/* Testing every aircraft defined in the mapdef. */
		LIST_Foreach(md->aircraft, const char, aircraft) {
			Cvar_Set("rm_drop", "%s", Com_GetRandomMapAssemblyNameForCraft(aircraft));
			testCountSpawnpointsForMapWithAssemblyAndAircraft(verbose, md, asmName, aircraft);
		}
	}
}

void GameTest::testCountSpawnpointsForMap(bool verbose, const mapDef_t *md)
{
	Com_Printf("CountSpawnpoints - test start: mapdef %s\n", md->id);

	/* Check if we are manually testing a certain assembly. */
	if (TEST_ExistsProperty("assembly")) {
		const char *asmName = TEST_GetStringProperty("assembly");
		int tested = 0;
		LIST_Foreach(md->params, const char, s) {
			if (Q_streq(asmName, s)) {
				testCountSpawnpointsForMapWithAssembly(verbose, md, asmName);
				tested += 1;
			}
		}
		if (tested < 1) {
			Com_Printf("CountSpawnpoints - error: Not a valid assembly: %s\n", asmName);
			ADD_FAILURE() << "Error: Not a valid assembly: " << asmName;
		}
	} else if (LIST_IsEmpty(md->params)) {
		/* This is for static maps. */
		std::cout << "[          ] testing mapdef: " << md->id << std::endl;
		testCountSpawnpointsForMapWithAssembly(verbose, md, nullptr);
	} else {
		/* This is for RMA. */
		LIST_Foreach(md->params, const char, asmName) {
			testCountSpawnpointsForMapWithAssembly(verbose, md, asmName);
		}
	}
}

TEST_F(GameTest, CountSpawnpointsStatic)
{
	bool verbose = false;
	if (TEST_ExistsProperty("verbose"))
		verbose = true;

	mapCount = 0;
	const mapDef_t* md;
	MapDef_Foreach(md) {
		/* Exlude RMA maps. */
		if (md->mapTheme[0] == '+')
			continue;
		/* Exclude .baseattack. */
		if (md->mapTheme[0] == '.')
			return;
		testCountSpawnpointsForMap(verbose, md);
	}
	std::cout << "[          ] Static maps tested: " << mapCount << std::endl;
	Com_Printf("CountSpawnpoints - static maps tested: %i\n", mapCount);
}

TEST_F(GameTest, CountSpawnpointsRMA)
{
	bool verbose = false;
	if (TEST_ExistsProperty("verbose"))
		verbose = true;

	mapCount = 0;
	const mapDef_t* md;

	/* Check if we are manually testing a certain mapdef. */
	if (TEST_ExistsProperty("mapdef-id")) {
		const char* mapdefId = TEST_GetStringProperty("mapdef-id");
		int tested = 0;
		MapDef_Foreach(md) {
			if (Q_streq(md->id, mapdefId)) {
				testCountSpawnpointsForMap(verbose, md);
				tested += 1;
			}
		}
		if (tested < 1) {
			Com_Printf("CountSpawnpoints - error: Not a valid mapdef: %s\n", mapdefId);
			ADD_FAILURE() << "Error: Not a valid mapdef: " << mapdefId;
		}
	} else {
		MapDef_Foreach(md) {
			/* Exlude static maps. */
			if (md->mapTheme[0] != '+')
				continue;
			/* Exclude .baseattack. */
			if (md->mapTheme[0] == '.')
				return;

			testCountSpawnpointsForMap(verbose, md);
		}
	}
	Com_Printf("CountSpawnpoints - RMA combinations tested: RMA %i\n", mapCount);
	std::cout << "[          ] RMA combinations tested: " << mapCount << std::endl;
	SV_ShutdownGameProgs();
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
