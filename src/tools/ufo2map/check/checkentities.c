/**
 * @file checkentities.c
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Copyright (C) 1997-2001 Id Software, Inc.

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

#include "checkentities.h"
#include "../../../shared/mathlib.h"
#include "../../../shared/entitiesdef.h"
#include "../common/shared.h"
#include "../bsp.h"
#include "checklib.h"

#define MIN_TILE_SIZE 256 /**< @todo take this datum from the correct place */

static void Check_MapSize (vec3_t mapSize);

/**
 * @brief print map stats on -stats
 */
void Check_Stats(void) {
	vec3_t worldSize;
	int i, j;
	int *entNums;

	Check_InitEntityDefs();

	entNums = (int *)Mem_Alloc(numEntityDefs * sizeof(int));

	Check_MapSize(worldSize);
	Verb_Printf(VERB_NORMAL, "        Number of brushes: %i\n",nummapbrushes);
	Verb_Printf(VERB_NORMAL, "         Number of planes: %i\n",nummapplanes);
	Verb_Printf(VERB_NORMAL, "    Number of brush sides: %i\n",nummapbrushsides);
	Verb_Printf(VERB_NORMAL, "         Map size (units): %.0f %.0f %.0f\n", worldSize[0], worldSize[1], worldSize[2]);
	Verb_Printf(VERB_NORMAL, "        Map size (fields): %.0f %.0f %.0f\n", worldSize[0] / UNIT_SIZE, worldSize[1] / UNIT_SIZE, worldSize[2] / UNIT_HEIGHT);
	Verb_Printf(VERB_NORMAL, "         Map size (tiles): %.0f %.0f %.0f\n", worldSize[0] / (MIN_TILE_SIZE), worldSize[1] / (MIN_TILE_SIZE), worldSize[2] / UNIT_HEIGHT);
	Verb_Printf(VERB_NORMAL, "       Number of entities: %i\n", num_entities);

	/* count number of each type of entity */
	for (i = 0; i < num_entities; i++) {
		const char *name = ValueForKey(&entities[i], "classname");

		for (j = 0; j < numEntityDefs; j++)
			if (!strncmp(name, entityDefs[j].classname, strlen(entityDefs[j].classname))) {
				entNums[j]++;
				break;
			}
		if (j == numEntityDefs) {
			Com_Printf("Check_Stats: entity '%s' not recognised\n", name);
		}

	}

	/* print number of each type of entity */
	for (j = 0; j < numEntityDefs; j++)
		if (entNums[j])
			Com_Printf("%27s: %i\n", entityDefs[j].classname, entNums[j]);

	Mem_Free(entNums);
}


/** needs to be done here, on map brushes as worldMins and worldMaxs from levels.c
 * are only calculated on BSPing
 * @param[out] mapSize the returned size in map units
 */
static void Check_MapSize (vec3_t mapSize)
{
	int i, bi, vi;
	vec3_t mins, maxs;

	VectorSet(mins, 0, 0, 0);
	VectorSet(maxs, 0, 0, 0);

	for (i = 0; i < nummapbrushes; i++) {
		const mapbrush_t *brush = &mapbrushes[i];

		for (bi = 0; bi < brush->numsides; bi++) {
			const winding_t *winding = brush->original_sides[bi].winding;

			for (vi = 0; vi < winding->numpoints; vi++)
				AddPointToBounds(winding->p[vi], mins, maxs);
		}
	}

	VectorSubtract(maxs, mins, mapSize);
}
