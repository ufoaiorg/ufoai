/**
 * @file
 * @brief Test cases for the mapdef code and loading the related maps
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

#include "test_shared.h"
#include "test_mapdef.h"
#include "../client/client.h"
#include "../client/cl_shared.h"
#include "../client/renderer/r_state.h"
#include "../client/ui/ui_main.h"
#include "../common/routing.h"
#include "../server/server.h"
#include "../server/sv_rma.h"

#define RMA_HIGHEST_SUPPORTED_SEED 50

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

char mapStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
char posStr[MAX_TOKEN_CHARS * MAX_TILESTRINGS];

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and builds each map with each ufo and every assembly for all valid seeds.
 * In other words: this a FULL test to check if some seed causes problems in any of the
 * possible combinations, so it should not be run on the buildserver on a daily basis.
 */
static void testMapDefsMassRMA (void)
{
	/** You can test a certain assembly by passing "-Dmapdef-id=assembly" to testall. */
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;
	int mapCount = 0;

	CU_ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->map[0] == '.')
			continue;
		if (md->nocunit)	/* map is WIP and therefore excluded from the tests */
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;
		if (++mapCount <= 0)		/* change 0 to n to skip the first n assemblies */
			continue;

		{
			char* p = md->map;

			if (*p == '+')
				p++;
			else
				continue;

			const char* ass = (const char*)LIST_GetByIdx(md->params, 0);
			Com_Printf("\nMap: %s Assembly: %s AssNr: %i\n", p, ass, mapCount);

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
						ass = nullptr;
						srand(i);
						long time = Sys_Milliseconds();
						Com_Printf(" %i", i);

						typedef struct skip_info {
							int         seed;
							char const* map;
							char const* params;
							char const* craft;
							char const* ufo;
						} skip_info;
#if 0
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
							ass = Com_GetRandomMapAssemblyNameForCraft(ufo) + 1;	/* +1 = get rid of the '+' */
						else
							ass = (const char*)LIST_GetByIdx(md->params, 0);


						char* entityString = SV_GetConfigString(CS_ENTITYSTRING);
						MapInfo* randomMap = SV_AssembleMap(p, ass, mapStr, posStr, entityString, i, false);
						CU_ASSERT(randomMap != nullptr);
						time = (Sys_Milliseconds() - time);
						CU_ASSERT(time < 30000);
						if (time > 10000)
							Com_Printf("\nMap: %s Assembly: %s Seed: %i tiles: %i ms: %li\n", p, ass, i, randomMap->numPlaced, time);
						Mem_Free(randomMap);
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

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and collects some properties of the maps relevant to RMA.
 */
static void testMapDefStatistic (void)
{
	int j, k;
	int required, solids;
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;

	CU_ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->map[0] == '.')
			continue;
		if (md->nocunit)	/* map is WIP and therefore excluded from the tests */
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;

		MapInfo* theMap = Mem_AllocType(MapInfo);
		char mapAssName[80];
		const char* p = md->map;

		if (*p == '+')
			p++;
		else
			continue;
		SV_ParseUMP(p, nullptr, theMap, false);
		theMap->asmIdx = 0;
		/* overwrite with specified, if any */
		const char* assName = (const char*)LIST_GetByIdx(md->params, 0);
		if (assName && assName[0]) {
			for (j = 0; j < theMap->numAssemblies; j++)
				if (Q_streq(assName, theMap->assemblies[j].id)) {
					theMap->asmIdx = j;
					break;
				}
			if (j == theMap->numAssemblies) {
				Com_Printf("SV_AssembleMap: Map assembly '%s' not found\n", assName);
			}
		}

		SV_PrepareTilesToPlace(theMap);
		const Assembly* ass = theMap->getCurrentAssembly();

		required = 0;
		solids = 0;
		for (k = 0; k < theMap->numToPlace; k++) {
			required += theMap->mToPlace[k].min;
			solids += theMap->mToPlace[k].max * theMap->mToPlace[k].tile->area;
		}

		Com_sprintf(mapAssName, sizeof(mapAssName), "%s %s", p, ass);
		Com_Printf("%22.22s %2.i %2.i %2.i %2.i %3.i %3.i \n", mapAssName, theMap->numTiles, theMap->numToPlace, required, ass->numSeeds, ass->size, solids);
	}
}

#define FOOTSTEPS_FULL 0
/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and tries to find surfaces to stand on with no sound assigned to them.
 *
 * This test takes too long to be run every time testall is run. So it's set up almost like a game:
 * After 5 maps (the first of them is fully checked) with missing sounds, the test stops.
 * If we manage to 'clean' one of those 5 maps, the next map will show up in the next run.
 */
static void testMapDefsFootSteps (void)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;
	int mapCount = 0;				// the number of maps read
	int badMapCount = 0;
	const int skipCount = 20;		// to skip the first n mapDefs
	const int badMapCountMax = 25;	// # of maps with missing sounds before this test stops
	const int mapCountMax = 150;	// should of cause be higher than skip + bad
	const int texCountMax = 30;
	char texNames[texCountMax][MAX_QPATH];
	bool done = false;

	OBJZERO(texNames);
	CU_ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->map[0] == '.')
			continue;
		if (md->nocunit)	/* map is WIP and therefore excluded from the tests */
			continue;
		if (filterId && !Q_streq(filterId, md->id))
			continue;

		mapCount++;
		if (mapCount <= skipCount)
			continue;
		/* use a known seed to reproduce an error */
		unsigned int seed;
		if (TEST_ExistsProperty("mapdef-seed")) {
			seed = TEST_GetLongProperty("mapdef-seed");
		} else {
			seed = (unsigned int) time(nullptr);
		}
		srand(seed);

		int count = 0;
		Com_Printf("testMapDefsFootSteps: Mapdef %s (seed %u)\n", md->id, seed);

		const char* ass = (const char*)LIST_GetByIdx(md->params, 0);
		SV_Map(true, md->map, ass);

		/* now that we have loaded the map, check all cells for walkable places */
		GridBox mBox(sv->mapData.mapBox);	// test ALL the cells
#if !FOOTSTEPS_FULL
		if (mapCount >= skipCount + 4) {	// after the first 4 maps, reduce the testing area
			const pos3_t center = {148, 128, 0};
			mBox.set(center, center);		// the box on the map we're testing
			mBox.expandXY(10);				// just test a few cells around the center of the map
			mBox.maxs[2] = 2;				// and 3 levels high
		}
#endif
		mBox.clipToMaxBoundaries();

		for (int x = mBox.getMinX(); x <= mBox.getMaxX() && !done; x++) {
			for (int y = mBox.getMinY(); y <= mBox.getMaxY() && !done; y++) {
				for (int z = mBox.getMinZ(); z <= mBox.getMaxZ(); z++) {
					const int floor = sv->mapData.routing.getFloor(1, x, y, z);
					if (floor < 0)						// if we have a floor in that cell
						continue;
					const AABB noBox(vec3_origin, vec3_origin);	// we're doing a point-trace
					const pos3_t cellPos = {x, y, z};			// the cell inquestion
					vec3_t from, to;
					PosToVec(cellPos, from);			// the center of the cell
					VectorCopy(from, to);				// also base for the endpoint of the trace
					from[2] -= UNIT_HEIGHT / 2;			// bottom of the cell
					from[2] += (floor + 2) * QUANT;		// add the height of the floor plus 2 QUANTS
					to[2] -= 2 * UNIT_HEIGHT;			// we should really hit the ground with this
					const trace_t trace = SV_Trace(Line(from, to), noBox, nullptr, MASK_SOLID);
					if (!trace.surface)
						continue;

					const char* snd = SV_GetFootstepSound(trace.surface->name);
					if (snd)
						continue;

					for (int i = 0; i < texCountMax; ++i) {
						if (!texNames[i][0]) {	// found a free slot ?
							Q_strncpyz(texNames[i], trace.surface->name, sizeof(texNames[i]));
							count++;
							break;
						}
						if (Q_streq(trace.surface->name, texNames[i]))	// already there ?
							break;
					}
					if (count > texCountMax) {
						done = true;
						break;	// the z-loop
					}
				}
			}
		}
		if (!texNames[0][0]) {
			Com_Printf("In map %s, ass %s: Nothing detected\n", md->map, ass);
		} else {
			++badMapCount;
			for (int i = 0; i < texCountMax; ++i) {
				if (texNames[i][0]) {
					Com_Printf("In map %s, ass %s: No sound for: %s\n", md->map, ass, texNames[i]);
				}
			}
		}
		OBJZERO(texNames);
		SV_ShutdownGameProgs();
		CU_PASS(md->map);

		if (done || mapCount >= mapCountMax || badMapCount >= badMapCountMax)
			break;
	}
}

/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and tries to load (and build in case of RMA) each map once (with a random seed).
 */
static void testMapDefsSingleplayer (void)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	const mapDef_t* md;

	CU_ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->map[0] == '.')
			continue;
		if (md->nocunit)	/* map is WIP and therefore excluded from the tests */
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
			SV_Map(true, md->map, nullptr);
			SV_ShutdownGameProgs();
		} else {
			LIST_Foreach(md->params, const char, param) {
				Com_Printf("testMapDefsSingleplayer: Mapdef %s (param %s)\n", md->id, param);
				SV_Map(true, md->map, param);
				SV_ShutdownGameProgs();
			}
		}
		CU_PASS(md->map);
	}
}

static void testMapDefsMultiplayer (void)
{
	const char* filterId = TEST_GetStringProperty("mapdef-id");
	char userinfo[MAX_INFO_STRING];
	const mapDef_t* md;

	CU_ASSERT_TRUE(csi.numMDs > 0);

	Cvar_Set("sv_maxclients", "2");

	MapDef_ForeachCondition(md, md->multiplayer) {
		unsigned int seed;
		SrvPlayer* player;

		if (md->nocunit)	/* map is WIP and therefore excluded from the tests */
			continue;
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
			SV_Map(true, md->map, nullptr);

			player = PLAYER_NUM(0);
			Info_SetValueForKey(userinfo, sizeof(userinfo), "cl_teamnum", "-1");
			CU_ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));

			SV_ShutdownGameProgs();
		} else {
			LIST_Foreach(md->params, const char, param) {
				Com_Printf("testMapDefsMultiplayer: Mapdef %s (param %s)\n", md->id, param);
				SV_Map(true, md->map, param);

				player = PLAYER_NUM(0);
				Info_SetValueForKey(userinfo, sizeof(userinfo), "cl_teamnum", "-1");
				CU_ASSERT_TRUE(svs.ge->ClientConnect(player, userinfo, sizeof(userinfo)));

				SV_ShutdownGameProgs();
			}
		}
		CU_PASS(md->map);
	}
}

int UFO_AddMapDefTests (void)
{
	/* add a suite to the registry */
	CU_pSuite mapDefSuite = CU_add_suite("MapDefTests", UFO_InitSuiteMapDef, UFO_CleanSuiteMapDef);

	if (mapDefSuite == nullptr)
		return CU_get_error();

	const char* specialtest = TEST_GetStringProperty("mapspecialtest");
//	const char* specialtest = "seed";
//	const char* specialtest = "stats";
//	const char* specialtest = "steps";
	if (specialtest && Q_streq(specialtest, "seed")) {
		if (CU_ADD_TEST(mapDefSuite, testMapDefsMassRMA) == nullptr)
			return CU_get_error();
	}
	else if (specialtest && Q_streq(specialtest, "stats")) {
		if (CU_ADD_TEST(mapDefSuite, testMapDefStatistic) == nullptr)
			return CU_get_error();
	}
	else if (specialtest && Q_streq(specialtest, "steps")) {
		if (CU_ADD_TEST(mapDefSuite, testMapDefsFootSteps) == nullptr)
			return CU_get_error();
	}
	else {
		/* add the tests to the suite */
		if (CU_ADD_TEST(mapDefSuite, testMapDefsSingleplayer) == nullptr)
			return CU_get_error();

		if (CU_ADD_TEST(mapDefSuite, testMapDefsMultiplayer) == nullptr)
			return CU_get_error();
	}
	return CUE_SUCCESS;
}
