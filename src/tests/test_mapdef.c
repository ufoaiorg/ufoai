/**
 * @file test_mapdef.c
 * @brief Test cases for the mapdef code and loading the related maps
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
#include "test_mapdef.h"
#include "../client/client.h"
#include "../client/cl_shared.h"
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"
#include "../server/server.h"
#include "../server/sv_rma.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteMapDef (void)
{
	TEST_Init();

	sv_genericPool = Mem_CreatePool("mapdef-test");
	com_networkPool = Mem_CreatePool("Network");
	vid_imagePool = Mem_CreatePool("Vid: Image system");
	sv_dumpmapassembly = Cvar_Get("sv_dumpassembly", "0", 0, NULL);
	sv_public = Cvar_Get("sv_public", "0", 0, NULL);
	port = Cvar_Get("testport", "27909", 0, NULL);
	masterserver_url = Cvar_Get("masterserver_test", "http://localhost", 0, NULL);

	cl_genericPool = Mem_CreatePool("Client: Generic");

	r_state.active_texunit = &r_state.texunits[0];
	R_FontInit();
	UI_Init();

	OBJZERO(cls);
	Com_ParseScripts(qfalse);

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteMapDef (void)
{
	TEST_Shutdown();

	NET_Shutdown();

	return 0;
}

#define SEED_TEST 1
#if SEED_TEST
char mapStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
char posStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and builds each map mutiple times with different seeds.
 */
static void testMapDefsMassRMA (void)
{
	int i;
	const char *filterId = TEST_GetStringProperty("mapdef-id");

	CU_ASSERT_TRUE(cls.numMDs > 0);

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t* md = &cls.mds[i];
		if (md->map[0] == '.')
			continue;
		if (filterId && strcmp(filterId, md->id) != 0)
			continue;

		{
			int i;
			long time;
			mapInfo_t *randomMap;
			char *p = md->map;

			if (*p == '+')
				p++;
			else
				continue;

			Com_Printf("Map: %s Assembly: %s\n", p, md->param);

			sv_threads->integer = 0;
			for (i = 0; i < 50; i++) {
				srand(i);
				time = Sys_Milliseconds();
				Com_Printf("Seed: %i\n", i);
				randomMap = SV_AssembleMap(p, md->param, mapStr, posStr, i);
				CU_ASSERT(randomMap != NULL);
				time = (Sys_Milliseconds() - time);
				CU_ASSERT(time < 30000);
				if (time > 10000)
					Com_Printf("Map: %s Assembly: %s Seed: %i tiles: %i ms: %li\n", p, md->param, i, randomMap->numPlaced, time);
				Mem_Free(randomMap);
			}
		}
	}
}
#endif

#define MAP_STATISTIC 0
#if MAP_STATISTIC
char mapStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
char posStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and builds each map mutiple times with different seeds.
 */
static void testMapDefStatistic (void)
{
	int i, j, k;
	int required, solids;
	const char *filterId = TEST_GetStringProperty("mapdef-id");

	CU_ASSERT_TRUE(cls.numMDs > 0);

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t* md = &cls.mds[i];
		if (md->map[0] == '.')
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;

		{
			mapInfo_t *theMap = Mem_AllocType(mapInfo_t);
			char mapAssName[80];
			const char *p = md->map;

			if (*p == '+')
				p++;
			else
				continue;
			SV_ParseUMP(p, theMap, qfalse);
			theMap->mAsm = 0;
			/* overwrite with specified, if any */
			if (md->param && md->param[0]) {
				for (j = 0; j < theMap->numAssemblies; j++)
					if (Q_streq(md->param, theMap->mAssembly[j].id)) {
						theMap->mAsm = j;
						break;
					}
				if (j == theMap->numAssemblies) {
					Com_Printf("SV_AssembleMap: Map assembly '%s' not found\n", md->param);
				}
			}

			SV_PrepareTilesToPlace(theMap);
			mAssembly_t *ass = &theMap->mAssembly[theMap->mAsm];

			required = 0;
			solids = 0;
			for (k = 0; k < theMap->numToPlace; k++) {
				required += theMap->mToPlace[k].min;
				solids += theMap->mToPlace[k].max * theMap->mToPlace[k].tile->area;
			}

			Com_sprintf(mapAssName, sizeof(mapAssName), "%s %s", p, md->param);
			Com_Printf("%22.22s %2.i %2.i %2.i %2.i %3.i %3.i \n", mapAssName, theMap->numTiles, theMap->numToPlace, required, ass->numSeeds, ass->size, solids);
		}
	}
}
#endif

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and tries to load (and build in case of RMA) each map.
 */
static void testMapDefsSingleplayer (void)
{
	int i;
	const char *filterId = TEST_GetStringProperty("mapdef-id");

	CU_ASSERT_TRUE(cls.numMDs > 0);

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t* md = &cls.mds[i];
		if (md->map[0] == '.')
			continue;
		if (filterId && strcmp(filterId, md->id) != 0)
			continue;

		{
			/* use a known seed to reproduce an error */
			unsigned int seed;
			if (TEST_ExistsProperty("mapdef-seed")) {
				seed = TEST_GetLongProperty("mapdef-seed");
			} else {
				seed = (unsigned int) time(NULL);
			}
			srand(seed);

			Com_Printf("testMapDefsSingleplayer: Mapdef %s (seed %u)\n", md->id, seed);
			SV_Map(qtrue, md->map, md->param);
			SV_ShutdownGameProgs();
			CU_PASS(md->map);
		}
	}
}

static void testMapDefsMultiplayer (void)
{
	int i;
	const char *filterId = TEST_GetStringProperty("mapdef-id");
	char userinfo[MAX_INFO_STRING];

	CU_ASSERT_TRUE(cls.numMDs > 0);

	Cvar_Set("sv_maxclients", "2");

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t* md = &cls.mds[i];
		if (filterId && strcmp(filterId, md->id) != 0)
			continue;

		if (md->multiplayer) {
			/* use a known seed to reproduce an error */
			unsigned int seed;
			player_t *player;
			if (TEST_ExistsProperty("mapdef-seed")) {
				seed = TEST_GetLongProperty("mapdef-seed");
			} else {
				seed = (unsigned int) time(NULL);
			}
			srand(seed);

			Com_Printf("testMapDefsMultiplayer: Mapdef %s (seed %u)\n", md->id, seed);
			SV_Map(qtrue, md->map, md->param);

			player = PLAYER_NUM(0);
			Info_SetValueForKey(userinfo, sizeof(userinfo), "cl_teamnum", "-1");
			CU_ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));

			SV_ShutdownGameProgs();
			CU_PASS(md->map);
		}
	}
}

int UFO_AddMapDefTests (void)
{
	/* add a suite to the registry */
	CU_pSuite mapDefSuite = CU_add_suite("MapDefTests", UFO_InitSuiteMapDef, UFO_CleanSuiteMapDef);

	if (mapDefSuite == NULL)
		return CU_get_error();

#if MAP_STATISTIC
	/* add the tests to the suite */
	if (CU_ADD_TEST(mapDefSuite, testMapDefStatistic) == NULL)
		return CU_get_error();
#else
#if SEED_TEST
	/* add the tests to the suite */
	if (CU_ADD_TEST(mapDefSuite, testMapDefsMassRMA) == NULL)
		return CU_get_error();
#else
	if (CU_ADD_TEST(mapDefSuite, testMapDefsSingleplayer) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(mapDefSuite, testMapDefsMultiplayer) == NULL)
		return CU_get_error();
#endif
#endif
	return CUE_SUCCESS;
}
