/**
 * @file
 * @brief Test cases for the mapdef code and loading the related maps
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
#include "../client/client.h"
#include "../client/cl_shared.h"
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"
#include "../common/routing.h"
#include "../server/server.h"
#include "../server/sv_rma.h"

#define RMA_HIGHEST_SUPPORTED_SEED 50

class MapDefTest: public ::testing::Test {
protected:
	static void SetUpTestCase() {
		TEST_Init();

		sv_genericPool = Mem_CreatePool("mapdef-test");
		com_networkPool = Mem_CreatePool("Network");
		vid_imagePool = Mem_CreatePool("Vid: Image system");
		sv_dumpmapassembly = Cvar_Get("sv_dumpassembly", "0");
		sv_public = Cvar_Get("sv_public", "0");
		port = Cvar_Get("testport", "27909");
		masterserver_url = Cvar_Get("masterserver_test", "http://localhost");

		cl_genericPool = Mem_CreatePool("Client: Generic");

		r_state.active_texunit = &r_state.texunits[0];
		R_FontInit();
		UI_Init();

		OBJZERO(cls);
		Com_ParseScripts(false);
	}

	static void TearDownTestCase() {
		TEST_Shutdown();
		NET_Shutdown();
	}
};

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and tries to load (and build in case of RMA) each map once (with a random seed).
 */
TEST_F(MapDefTest, MapDefsSingleplayer)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;

	ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '.')
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;

		/* use a known seed to reproduce an error */
		unsigned int seed;
		if (TEST_ExistsProperty("mapdef-seed")) {
			seed = TEST_GetLongProperty("mapdef-seed");
		} else {
			seed = (unsigned int) time(nullptr);
		}
		srand(seed);

		Com_Printf("testMapDefsSingleplayer: Mapdef %s (seed %u)\n", md->id, seed);
		if (LIST_IsEmpty(md->params)) {
			SV_Map(true, md->mapTheme, nullptr);
			SV_ShutdownGameProgs();
		} else {
			LIST_Foreach(md->params, const char, param) {
				Com_Printf("testMapDefsSingleplayer: Mapdef %s (param %s)\n", md->id, param);
				SV_Map(true, md->mapTheme, param);
				SV_ShutdownGameProgs();
			}
		}
	}
}

TEST_F(MapDefTest, MapDefsMultiplayer)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	char userinfo[MAX_INFO_STRING];
	const mapDef_t* md;

	ASSERT_TRUE(csi.numMDs > 0);

	Cvar_Set("sv_maxclients", "2");

	MapDef_ForeachCondition(md, md->multiplayer) {
		unsigned int seed;
		SrvPlayer* player;

		if (filterId && !Q_streq(filterId, md->id))
			continue;

		/* use a known seed to reproduce an error */
		if (TEST_ExistsProperty("mapdef-seed")) {
			seed = TEST_GetLongProperty("mapdef-seed");
		} else {
			seed = (unsigned int) time(nullptr);
		}
		srand(seed);

		Com_Printf("testMapDefsMultiplayer: Mapdef %s (seed %u)\n", md->id, seed);
		if (LIST_IsEmpty(md->params)) {
			SV_Map(true, md->mapTheme, nullptr);

			player = PLAYER_NUM(0);
			Info_SetValueForKey(userinfo, sizeof(userinfo), "cl_teamnum", "-1");
			ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));

			SV_ShutdownGameProgs();
		} else {
			LIST_Foreach(md->params, const char, param) {
				Com_Printf("testMapDefsMultiplayer: Mapdef %s (param %s)\n", md->id, param);
				SV_Map(true, md->mapTheme, param);

				player = PLAYER_NUM(0);
				Info_SetValueForKey(userinfo, sizeof(userinfo), "cl_teamnum", "-1");
				ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));

				SV_ShutdownGameProgs();
			}
		}
	}
}
