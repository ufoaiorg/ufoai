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

class MapDefStatsTest: public ::testing::Test {
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
 * and collects some properties of the maps relevant to RMA.
 */
TEST_F(MapDefStatsTest, DISABLED_MapDefStatistic)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;

	ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '.')
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;

		const char* asmName = (const char*)LIST_GetByIdx(md->params, 0);
		SV_PrintAssemblyStats(md->mapTheme, asmName);
	}
}

