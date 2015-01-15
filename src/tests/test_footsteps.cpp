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

class FootStepTest: public ::testing::Test {
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

#define FOOTSTEPS_FULL 0
/**
 * @brief This test cycles through the list of map definitions found in the maps.ufo script
 * and tries to find surfaces to stand on with no sound assigned to them.
 *
 * This test takes too long to be run every time testall is run. So it's set up almost like a game:
 * After 5 maps (the first of them is fully checked) with missing sounds, the test stops.
 * If we manage to 'clean' one of those 5 maps, the next map will show up in the next run.
 */
TEST_F(FootStepTest, DISABLED_MapDefsFootSteps)
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
	ASSERT_TRUE(csi.numMDs > 0);

	MapDef_Foreach(md) {
		if (md->mapTheme[0] == '.')
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

		const char* asmName = (const char*)LIST_GetByIdx(md->params, 0);
		SV_Map(true, md->mapTheme, asmName);

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
					const pos3_t cellPos = {(pos_t)x, (pos_t)y, (pos_t)z};			// the cell in question
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
			Com_Printf("In map %s, asm %s: Nothing detected\n", md->mapTheme, asmName);
		} else {
			++badMapCount;
			for (int i = 0; i < texCountMax; ++i) {
				if (texNames[i][0]) {
					Com_Printf("In map %s, asm %s: No sound for: %s\n", md->mapTheme, asmName, texNames[i]);
				}
			}
		}
		OBJZERO(texNames);
		SV_ShutdownGameProgs();

		if (done || mapCount >= mapCountMax || badMapCount >= badMapCountMax)
			break;
	}
}
