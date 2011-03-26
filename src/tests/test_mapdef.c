/**
 * @file test_mapdef.c
 * @brief Test cases for the mapdef code and loading the related maps
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

#include "test_shared.h"
#include "test_mapdef.h"
#include "../client/client.h"
#include "../client/cl_shared.h"
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"
#include "../server/server.h"

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteMapDef (void)
{
	TEST_Init();

	NET_Init();

	sv_genericPool = Mem_CreatePool("mapdef-test");
	com_networkPool = Mem_CreatePool("Network");
	vid_imagePool = Mem_CreatePool("Vid: Image system");
	sv_dumpmapassembly = Cvar_Get("sv_dumpassembly", "0", 0, NULL);
	sv_public = Cvar_Get("sv_public", "0", 0, NULL);
	port = Cvar_Get("testport", "27909", 0, NULL);

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
			/* use a know seed to allow reproductible error */
			unsigned int seed;
			if (TEST_ExistsProperty("mapdef-seed")) {
				seed = TEST_GetLongProperty("mapdef-seed");
			} else {
				seed = (unsigned int) time(NULL);
			}
			srand(seed);

			Com_Printf("testMapDefsSingleplayer: Mapdef %s (seed %u)\n", md->id, seed);
			SV_Map(qtrue, md->map, md->param);
			CU_PASS(md->map);
		}
	}
}

static void testMapDefsMultiplayer (void)
{
	int i;
	const char *filterId = TEST_GetStringProperty("mapdef-id");

	CU_ASSERT_TRUE(cls.numMDs > 0);

	masterserver_url = Cvar_Get("noname", "", 0, NULL);
	http_timeout = Cvar_Get("noname", "", 0, NULL);
	http_proxy = Cvar_Get("noname", "", 0, NULL);
	Cvar_Set("sv_maxclients", "2");

	for (i = 0; i < cls.numMDs; i++) {
		const mapDef_t* md = &cls.mds[i];
		if (filterId && strcmp(filterId, md->id) != 0)
			continue;

		if (md->multiplayer) {
			/* use a know seed to allow reproductible error */
			unsigned int seed;
			if (TEST_ExistsProperty("mapdef-seed")) {
				seed = TEST_GetLongProperty("mapdef-seed");
			} else {
				seed = (unsigned int) time(NULL);
			}
			srand(seed);

			Com_Printf("testMapDefsMultiplayer: Mapdef %s (seed %u)\n", md->id, seed);
			SV_Map(qtrue, md->map, md->param);
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

	/* add the tests to the suite */
	if (CU_ADD_TEST(mapDefSuite, testMapDefsSingleplayer) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(mapDefSuite, testMapDefsMultiplayer) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
