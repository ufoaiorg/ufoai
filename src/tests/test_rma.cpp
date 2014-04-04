/**
 * @file
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

#include "test_rma.h"
#include "test_shared.h"
#include "../server/server.h"
#include "../server/sv_rma.h"
#include "../ports/system.h"

#define MAX_ALLOWED_TIME_TO_ASSEMBLE 30000

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteRandomMapAssembly (void)
{
	TEST_Init();
	Com_ParseScripts(true);

	sv_dumpmapassembly = Cvar_Get("sv_dumpassembly", "0");
	sv_rma = Cvar_Get("sv_rma_tmp", "2");
	sv_rmadisplaythemap = Cvar_Get("sv_rmadisplaythemap", "1");

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteRandomMapAssembly (void)
{
	TEST_Shutdown();
	return 0;
}

static char mapStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
static char posStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

static void testUMPExtends (void)
{
	MapInfo* randomMap;
	char entityString[MAX_TOKEN_CHARS];

	srand(0);
	randomMap = SV_AssembleMap("test_extends", "default", mapStr, posStr, entityString, 0, true);
	CU_ASSERT(randomMap != nullptr);
	Mem_Free(randomMap);
}

static void testAssembly (void)
{
	MapInfo* randomMap;
	char entityString[MAX_TOKEN_CHARS];

	srand(0);
	randomMap = SV_AssembleMap("forest", "large", mapStr, posStr, entityString, 0, true);
	CU_ASSERT(randomMap != nullptr);
	Mem_Free(randomMap);
}

/* timeout version */
static void testMassAssemblyTimeout (void)
{
	char entityString[MAX_TOKEN_CHARS];

	sv_threads->integer = 1;
	for (int i = 0; i < 10; i++) {
		/** @todo the assemble thread sets a different seed */
		srand(i);
		long time = Sys_Milliseconds();
		const char* mapTheme = "forest";
		MapInfo* randomMap = SV_AssembleMap(mapTheme, "large", mapStr, posStr, entityString, i, true);
		CU_ASSERT(randomMap != nullptr);
		time = Sys_Milliseconds() - time;
		UFO_CU_ASSERT_TRUE_MSG(time < MAX_ALLOWED_TIME_TO_ASSEMBLE,
				va("%s fails to assemble in a reasonable time with seed %i (time: %li ms)", mapTheme, i, time));
		Com_Printf("%i: %i %li\n", i, randomMap->numPlaced, time);
		Mem_Free(randomMap);
	}
}

static void testMassAssemblyParallel (void)
{
	char entityString[MAX_TOKEN_CHARS];

	sv_threads->integer = 2;
	for (int i = 0; i < 10; i++) {
		/** @todo the assemble thread sets a different seed */
		srand(i);
		long time = Sys_Milliseconds();
		const char* mapTheme = "forest";
		MapInfo* randomMap = SV_AssembleMap(mapTheme, "large", mapStr, posStr, entityString, i, true);
		CU_ASSERT(randomMap != nullptr);
		time = Sys_Milliseconds() - time;
		UFO_CU_ASSERT_TRUE_MSG(time < MAX_ALLOWED_TIME_TO_ASSEMBLE,
				va("%s fails to assemble in a reasonable time with seed %i (time: %li ms)", mapTheme, i, time));
		Com_Printf("%i: %i %li\n", i, randomMap->numPlaced, time); fflush(stdout);
		Mem_Free(randomMap);
	}
}

/* sequential version */
static void testMassAssemblySequential (void)
{
	char entityString[MAX_TOKEN_CHARS];

	sv_threads->integer = 0;
	for (int i = 0; i < 10; i++) {
		srand(i);
		long time = Sys_Milliseconds();
		const char* mapTheme = "forest";
		MapInfo* randomMap = SV_AssembleMap(mapTheme, "large", mapStr, posStr, entityString, i, true);
		CU_ASSERT_PTR_NOT_NULL(randomMap);
		time = Sys_Milliseconds() - time;
		UFO_CU_ASSERT_TRUE_MSG(time < MAX_ALLOWED_TIME_TO_ASSEMBLE,
				va("%s fails to assemble in a reasonable time with seed %i (time: %li ms)", mapTheme, i, time));
		Com_Printf("%i: %i %li\n", i, randomMap->numPlaced, time);
		Mem_Free(randomMap);
	}
}

/* test the maps that have seedlists */
static void testSeedlists (void)
{
	const char* assNames[][2] = {
		{"farm", "medium"},
		{"farm", "large"},
		{"forest", "large"},
		{"forest", "large_crash"},
		{"oriental", "large"},
		{"village", "commercial"},
		{"village", "small"}
	};
	size_t length = sizeof(assNames) / (2 * sizeof(char*));
	char entityString[MAX_TOKEN_CHARS];

	sv_threads->integer = 0;
	long timeSum = 0;
	for (int n = 0; n < length; n++) {
		for (int i = 1; i < 20; i++) {
			srand(i);
			int time = Sys_Milliseconds();
			Com_Printf("Seed: %i\n", i);
			MapInfo* randomMap = SV_AssembleMap(assNames[n][0], assNames[n][1], mapStr, posStr, entityString, i, true);
			CU_ASSERT(randomMap != nullptr);
			time = Sys_Milliseconds() - time;
			timeSum += time;
			UFO_CU_ASSERT_TRUE_MSG(time < MAX_ALLOWED_TIME_TO_ASSEMBLE,
					va("%s fails to assemble in a reasonable time with seed %i (time: %li ms)", assNames[n][0], i, time));
			if (time > 10000)
				Com_Printf("Seed %i: tiles: %i ms: %li\n", i, randomMap->numPlaced, time);
			Mem_Free(randomMap);
		}
	}
	Com_Printf("TotalTime: %li\n", timeSum);
}

#define SEED_TEST 0
#if SEED_TEST
/**
 * @brief test the maps that have problems with certain seeds
 * this can also be used to produce new seedlists
 */
static void testNewSeedlists (void)
{
	long time, timeSum = 0;
	MapInfo* randomMap;
	char entityString[MAX_TOKEN_CHARS];

	sv_rmadisplaythemap->integer = 1;	/* print out the RMA analysis */
	sv_threads->integer = 0;
	for (int i = 0; i < RMA_HIGHEST_SUPPORTED_SEED; i++) {
		srand(i);
		time = Sys_Milliseconds();
		Com_Printf("Seed: %i\n", i);
		Cvar_Set("rm_drop", Com_GetRandomMapAssemblyNameForCraft("craft_drop_herakles"));
		Cvar_Set("rm_ufo", Com_GetRandomMapAssemblyNameForCraft("craft_ufo_fighter"));
		const char* mapTheme = "industrial";
		const char* asmName = "medium";
#if 0
		mapTheme = "tropic"; asmName = "river";
		mapTheme = "village"; asmName = "large";
		mapTheme = "desert"; asmName = "large";
#endif
		randomMap = SV_AssembleMap(mapTheme, asmName, mapStr, posStr, entityString, i, false);
		CU_ASSERT(randomMap != nullptr);
		time = Sys_Milliseconds() - time;
		timeSum += time;
		UFO_CU_ASSERT_TRUE_MSG(time < MAX_ALLOWED_TIME_TO_ASSEMBLE,
				va("%s fails to assemble in a reasonable time with seed %i (time: %li ms)", mapTheme, i, time));
		if (time > 10000)
			Com_Printf("Seed %i: tiles: %i ms: %li\n", i, randomMap->numPlaced, time);
		Mem_Free(randomMap);
	}
	Com_Printf("TotalTime: %li\n", timeSum);
}
#endif

int UFO_AddRandomMapAssemblyTests (void)
{
	static cvar_t svt;
	static cvar_t maxclients;
	/* add a suite to the registry */
	CU_pSuite RandomMapAssemblySuite = CU_add_suite("RandomMapAssemblyTests", UFO_InitSuiteRandomMapAssembly, UFO_CleanSuiteRandomMapAssembly);

	if (!sv_threads) {
		sv_threads = &svt;
		sv_threads->integer = 0;
	}

	if (!sv_maxclients) {
		sv_maxclients = &maxclients;
		sv_maxclients->integer = 1;
	}

	if (RandomMapAssemblySuite == nullptr)
		return CU_get_error();
#if SEED_TEST
	/* This test should normally (ie. buildbot) not be active */
	if (CU_ADD_TEST(RandomMapAssemblySuite, testNewSeedlists) == nullptr)
		return CU_get_error();
#else
	/* add the tests to the suite */
	if (CU_ADD_TEST(RandomMapAssemblySuite, testSeedlists) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testUMPExtends) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testAssembly) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testMassAssemblyParallel) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testMassAssemblyTimeout) == nullptr)
		return CU_get_error();

	if (CU_ADD_TEST(RandomMapAssemblySuite, testMassAssemblySequential) == nullptr)
		return CU_get_error();
#endif


	return CUE_SUCCESS;
}
