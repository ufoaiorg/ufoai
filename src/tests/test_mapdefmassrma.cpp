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

class MapDefMassRMATest: public ::testing::Test {
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

char mapStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
char posStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and builds each map with each ufo and every assembly for all valid seeds.
 * In other words: this a FULL test to check if some seed causes problems in any of the
 * possible combinations, so it should not be run on the buildserver on a daily basis.
 */
TEST_F(MapDefMassRMATest, DISABLED_MapDefsMassRMA)
{
	/** You can test a certain assembly by passing "-Dmapdef-id=assembly" to testall. */
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;
	int mapCount = 0;

	ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '.')
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;
		if (++mapCount <= 0)		/* change 0 to n to skip the first n assemblies */
			continue;

		{
			char* p = md->mapTheme;

			if (*p == '+')
				p++;
			else
				continue;

			const char* asmName = (const char*)LIST_GetByIdx(md->params, 0);
			Com_Printf("\nMap: %s Assembly: %s AssNr: %i\n", p, asmName, mapCount);

			sv_threads->integer = 0;

			/* This is tricky. Some maps don't have any ufo on them and thus in the mapdef.
			 * That would cause a LIST_Foreach macro to never run it's body. That's why these
			 * for-loops seem to have two termination conditions. In fact, we have to manually
			 * exit the for-loops if we ran it just once (without ufos nor dropships).
			 */
			bool didItOnce = false;
			linkedList_t* iterDrop;
			for (iterDrop = md->aircraft; iterDrop || !didItOnce; iterDrop = iterDrop->next) {
				const char* craft = nullptr;
				if (iterDrop)
					craft = (const char*) (iterDrop->data);

				if (craft)
					Cvar_Set("rm_drop", "%s", Com_GetRandomMapAssemblyNameForCraft(craft));
				else
					Cvar_Set("rm_drop", "+craft_drop_firebird");

				linkedList_t* iterUfo;
				for (iterUfo = md->ufos; iterUfo || !didItOnce; iterUfo = iterUfo->next) {
					const char* ufo = nullptr;
					if (iterUfo)
						ufo = (const char*) (iterUfo->data);
					if (ufo)
						Cvar_Set("rm_ufo", "%s", Com_GetRandomMapAssemblyNameForCraft(ufo));
					else
						Cvar_Set("rm_ufo", "+craft_ufo_scout");

					Com_Printf("\nDrop: %s Ufo: %s", craft, ufo);
					Com_Printf("\nSeed:");
					for (int i = 0; i < RMA_HIGHEST_SUPPORTED_SEED; i++) {
						asmName = nullptr;
						srand(i);
						long time = Sys_Milliseconds();
						Com_Printf(" %i", i);
#if 0

						typedef struct skip_info {
							int         seed;
							char const* map;
							char const* params;
							char const* craft;
							char const* ufo;
						} skip_info;
						/* if we have known problems with some combinations, we can skip them */
						skip_info const skip_list[] = {
							/* examples: */
						//	{ 20, "forest",		"large",		"craft_drop_raptor",   0                     },
						//	{ 12, "forest",		"large"			"craft_drop_herakles", "craft_ufo_harvester" },
							{ -1, "frozen",		"nature_medium",0,                     0                     },
							{ 11, "village",	"medium",		0,                     0					 },
							{ 19, "village",	"medium",		0,                     0					 },
							{ -1, "village",	"medium_noufo",	0,                     0					 },
							{ -1, "village",	"small",		0,                     0					 },
						};

						bool skip = false;
						for (skip_info const* e = skip_list; e != endof(skip_list); ++e) {
							if (e->seed >= 0 && i != e->seed)
								continue;
							if (e->map && !Q_streq(p, e->map))
								continue;
							if (e->params && !Q_streq(md->params, e->params))
								continue;
							if (e->craft && !Q_streq(craft, e->craft))
								continue;
							if (e->ufo && !Q_streq(ufo, e->ufo))
								continue;
							skip = true;
							break;
						}
						if (skip)
							continue;
#endif
						/* for ufocrash map, the ufoname is the assemblyame */
						if (Q_streq(p, "ufocrash"))
							asmName = Com_GetRandomMapAssemblyNameForCraft(ufo) + 1;	/* +1 = get rid of the '+' */
						else
							asmName = (const char*)LIST_GetByIdx(md->params, 0);


						char* entityString = SV_GetConfigString(CS_ENTITYSTRING);
						int numPlaced = SV_AssembleMap(p, asmName, mapStr, posStr, entityString, i, false);
						ASSERT_TRUE(numPlaced != 0);
						time = (Sys_Milliseconds() - time);
						ASSERT_TRUE(time < 30000);
						if (time > 10000)
							Com_Printf("\nMap: %s Assembly: %s Seed: %i tiles: %i ms: %li\n", p, asmName, i, numPlaced, time);
					}
					didItOnce = true;
					if (!iterUfo)
						break;
				}
				if (!iterDrop)
					break;
			}
		}
	}
}
