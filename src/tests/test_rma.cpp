/**
 * @file
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion.

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
#include "../server/server.h"
#include "../server/sv_rma.h"
#include "../ports/system.h"

#define MAX_ALLOWED_TIME_TO_ASSEMBLE 30000
#define TEST_THEME "forest"
#define TEST_ASSEMBLY "nature_large_b"

static char mapStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
static char posStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
static cvar_t svt;
static cvar_t maxclients;

class RandomMapAssemblyTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();
		Com_ParseScripts(true);

		sv_dumpmapassembly = Cvar_Get("sv_dumpassembly", "0");
		sv_rma = Cvar_Get("sv_rma_tmp", "2");
		sv_rmadisplaythemap = Cvar_Get("sv_rmadisplaythemap", "1");

		if (!sv_threads) {
			sv_threads = &svt;
			sv_threads->integer = 0;
		}

		if (!sv_maxclients) {
			sv_maxclients = &maxclients;
			sv_maxclients->integer = 1;
		}
	}

	long testAssembly(const char *source, const unsigned int numRuns, const char *mapTheme, const char *mapAssembly);

	static void TearDownTestCase() {
		TEST_Shutdown();
	}
};

long RandomMapAssemblyTest::testAssembly(const char *source, const unsigned int numRuns, const char *mapTheme, const char *mapAssembly)
{
	char entityString[MAX_TOKEN_CHARS];
	long time = 0;

	for (int i = 0; i < numRuns; i++) {
		/** @todo the assemble thread sets a different seed */
		srand(i);
		Com_Printf("%s - assembling map: theme: %s assembly: %s seed: %i\n", source, mapTheme, mapAssembly, i);

		long time = Sys_Milliseconds();
		int numPlaced = SV_AssembleMap(mapTheme, mapAssembly, mapStr, posStr, entityString, i, true);
		time = Sys_Milliseconds() - time;

		if (numPlaced < 1) {
			Com_Printf("%s - error: No tiles placed.\n", source);
			ADD_FAILURE() << source << " - error: No tiles placed (theme: " << mapTheme
				<< " assembly: " << mapAssembly << " seed: " << i << ").";
		}
		if (time > MAX_ALLOWED_TIME_TO_ASSEMBLE) {
			Com_Printf("%s - error: Assembly %s in map theme +%s failed to assemble in a reasonable time with seed %i\n", source, mapTheme, mapAssembly, i);
			ADD_FAILURE() << "Assembly " << mapTheme << "in map theme +" << mapAssembly
				<< " failed " << source << "using seed: %i " << i
				<< "(time measured: " << time << " ms).";
		}

		Com_Printf("%s - result: seed: %i tiles placed: %i time measured: %li ms\n", source, i, numPlaced, time);
		fflush(stdout);
	}
	return time;
}

TEST_F(RandomMapAssemblyTest, UMPExtends)
{
	char entityString[MAX_TOKEN_CHARS];

	srand(0);
	int numPlaced = SV_AssembleMap("test_extends", "default", mapStr, posStr, entityString, 0, true);
	ASSERT_TRUE(numPlaced != 0);
}

TEST_F(RandomMapAssemblyTest, Assembly)
{
	char entityString[MAX_TOKEN_CHARS];

	srand(0);
	int numPlaced = SV_AssembleMap(TEST_THEME, TEST_ASSEMBLY, mapStr, posStr, entityString, 0, true);
	ASSERT_TRUE(numPlaced != 0);
}

/* timeout version */
TEST_F(RandomMapAssemblyTest, MassAssemblyTimeout)
{
	const char *self = "RandomMapAssemblyTest.MassAssemblyTimeout";
	unsigned const int numRuns = 10;
	sv_threads->integer = 1;
	SCOPED_TRACE(va(self));
	testAssembly(self, numRuns, TEST_THEME, TEST_ASSEMBLY);
}

TEST_F(RandomMapAssemblyTest, MassAssemblyParallel)
{
	const char *self = "RandomMapAssemblyTest.MassAssemblyParallel";
	unsigned const int numRuns = 10;
	sv_threads->integer = 2;
	SCOPED_TRACE(va(self));
	testAssembly(self, numRuns, TEST_THEME, TEST_ASSEMBLY);
}

/* sequential version */
TEST_F(RandomMapAssemblyTest, MassAssemblySequential)
{
	const char *self = "RandomMapAssemblyTest.MassAssemblySequential";
	unsigned const int numRuns = 10;
	sv_threads->integer = 0;
	SCOPED_TRACE(va(self));
	testAssembly(self, numRuns, TEST_THEME, TEST_ASSEMBLY);
}

/* test the maps that have seedlists */
TEST_F(RandomMapAssemblyTest, Seedlists)
{
	const char* assNames[][2] = {
		{"farm", "medium"},
		{"farm", "large"},
		{TEST_THEME, TEST_ASSEMBLY},
		{"forest", "nature_medium_b"},
		{"oriental", "large"},
		{"village", "large"},
		{"village", "small"}
	};

	size_t length = sizeof(assNames) / (2 * sizeof(char*));
	const char *self = "RandomMapAssemblyTest.Seedlists";
	unsigned const int numRuns = 20;
	sv_threads->integer = 0;
	long timeSum = 0;

	SCOPED_TRACE(va(self));
	for (int n = 0; n < length; n++) {
		timeSum += testAssembly(self, numRuns, assNames[n][0], assNames[n][1]);
	}
	Com_Printf("%s - result: time total %li ms\n", self, timeSum);
}

#define SEED_TEST 0
#if SEED_TEST
/**
 * @brief test the maps that have problems with certain seeds
 * this can also be used to produce new seedlists
 */
TEST_F(RandomMapAssemblyTest, NewSeedlists)
{
	long time, timeSum = 0;
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
		int numPlaced = SV_AssembleMap(mapTheme, asmName, mapStr, posStr, entityString, i, false);
		ASSERT_TRUE(numPlaced != 0);
		time = Sys_Milliseconds() - time;
		timeSum += time;
		ASSERT_TRUE(time < MAX_ALLOWED_TIME_TO_ASSEMBLE) << mapTheme << " fails to assemble in a reasonable time with seed " << i << "(time: " << time << " ms)";
		if (time > 10000)
			Com_Printf("Seed %i: tiles: %i ms: %li\n", i, numPlaced, time);
	}
	Com_Printf("TotalTime: %li\n", timeSum);
}
#endif
