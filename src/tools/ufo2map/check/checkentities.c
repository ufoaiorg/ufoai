/**
 * @file checkentities.c
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

static int numToMoveToWorldspawn = 0;

static void Check_MapSize (vec3_t mapSize);
mapbrush_t **Check_ExtraBrushesForWorldspawn (int *numBrushes);


/**
 * @brief see if the entity is am actor start point
 * @note starts with "info_" and contains "_start"
 * @return qtrue if this is a start point
 */
static qboolean Check_IsInfoStart(const char *classname)
{
	return Q_strstart(classname, "info_") && strstr(classname, "_start");
}

/**
 * @brief check alignment using abstract size and mandatory origin
 * @return qtrue if OK
 * @todo check for brush intersection as well as alignment, and move
 * to a good position if bad.
 */
static qboolean Check_InfoStartAligned (const entityDef_t *ed, const entity_t *e)
{
	static int size[6];
	const entityKeyDef_t *sizeKd = ED_GetKeyDefEntity(ed, "size", 1); /* 1 means find abstract version of key */
	if (ED_GetIntVector(sizeKd, size, (int)(sizeof(size) / sizeof(int))) == ED_ERROR)
		Sys_Error("%s", ED_GetLastError());

	return (((int)e->origin[0] - size[0]) % UNIT_SIZE == 0)
		&& (((int)e->origin[1] - size[1]) % UNIT_SIZE == 0);
}

/**
 * @brief check targets exist (targetname), and check targetnames are targetted (target)
 * @return qfalse if there is a problem.
 */
static qboolean Check_TargetExists (const epair_t *kvp)
{
	const char *thisKey = kvp->key;
	const char *value = kvp->value;
	const char *otherKey = Q_streq("target", thisKey) ? "targetname" : "target";
	int i;

	for (i = 0; i < num_entities; i++) {
		const entity_t *e = &entities[i];
		const char *searchVal = ValueForKey(e, otherKey);

		if (searchVal && Q_streq(searchVal, value))
			return qtrue;
	}

	return qfalse;
}

static void Check_EntityWithBrushes (entity_t *e, const char *classname, int entnum)
{
	if (!e->numbrushes) {
		Check_Printf(VERB_CHECK, qtrue, entnum, -1, "%s with no brushes given - will be deleted\n", classname);
		e->skip = qtrue;
		return;
	}

	if (e->numbrushes > 1 && Q_streq(classname, "func_breakable")) {
		Check_Printf(VERB_CHECK, qfalse, entnum, -1, "func_breakable with more than one brush given (might break pathfinding)\n");
	}

	if (e->numbrushes == 1 && Q_streq(classname, "func_group")) {
		Check_Printf(VERB_CHECK, qtrue, entnum, -1, "%s with one brush only - will be moved to worldspawn\n", classname);
		numToMoveToWorldspawn++;
		e->skip = qtrue;
	}
}

/**
 * @brief Perform an entity check
 */
void CheckEntities (void)
{
	int i;

	Check_InitEntityDefs();

	for (i = 0; i < num_entities; i++) {
		entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		const entityDef_t *ed = ED_GetEntityDef(name);
		const epair_t *kvp;
		const entityKeyDef_t *kd;

		if (!ed) { /* check that a definition exists */
			Check_Printf(VERB_CHECK, qfalse, i, -1, "Not defined in entities.ufo: %s\n", name);
			continue;
		}

		/* check alignment of info_.+_start */
		if (Check_IsInfoStart(name) && !Check_InfoStartAligned(ed, e))
			Check_Printf(VERB_CHECK, qfalse, i, -1, "Misaligned %s\n", name);

		if (Q_strstart(name, "func_")) /* func_* entities should have brushes */
			Check_EntityWithBrushes(e, name, i);

		/* check all keys in the entity - make sure they are OK */
		for (kvp = e->epairs; kvp; kvp = kvp->next) {
			kd = ED_GetKeyDefEntity(ed, kvp->key, 0); /* zero means ignore abstract (radiant only) keys */

			if (!kd) { /* make sure it has a definition */
				Check_Printf(VERB_CHECK, qfalse, i, -1, "Not defined in entities.ufo: %s in %s\n", kvp->key,name);
				continue;
			}

			if (ED_CheckKey(kd, kvp->value) == ED_ERROR) { /* check values against type and range definitions in entities.ufo */
				Check_Printf(VERB_CHECK, qfalse, i, -1, "%s\n", ED_GetLastError());
				continue;
			}

			if (Q_streq("target", kvp->key) || Q_streq("targetname", kvp->key)) {
				if (!Check_TargetExists(kvp)) {
					Check_Printf(VERB_CHECK, qfalse, i, -1,
						"%s with %s of %s: no corresponding entity with %s with matching value\n",
						ed->classname, kvp->key, kvp->value, Q_streq("target", kvp->key) ? "targetname" : "target");
				}
			}
		}

		/* check keys in the entity definition - make sure mandatory ones are present */
		for (kd = ed->keyDefs; kd->name; kd++) {
			if (kd->flags & ED_MANDATORY) {
				const char *keyNameInEnt = ValueForKey(e, kd->name);
				if (keyNameInEnt[0] == '\0') {
					const char *defaultVal = kd->defaultVal;
					const qboolean hasDefault = defaultVal ? qtrue : qfalse;
					Check_Printf(VERB_CHECK, hasDefault, i, -1, "Mandatory key missing from entity: %s in %s", kd->name, name);
					if (defaultVal) {
						Check_Printf(VERB_CHECK, hasDefault, i, -1, ", supplying default: %s", defaultVal);
						SetKeyValue(e, kd->name, defaultVal);
					}
					Check_Printf(VERB_CHECK, hasDefault, i, -1, "\n");
				}
			}
		}
	}
}


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
			if (Q_streq(name, entityDefs[j].classname)) {
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

/**
 * @brief single brushes in func_groups are moved to worldspawn. this function allocates space for
 * pointers to those brushes. called when the .map is written back in map.c
 * @return a pointer to the array of pointers to brushes to be included in worldspawn.
 * @param[out] numBrushes the number of brushes
 */
mapbrush_t **Check_ExtraBrushesForWorldspawn (int *numBrushes)
{
	int i, j;
	mapbrush_t **brushesToMove;

	*numBrushes = numToMoveToWorldspawn;

	if (!numToMoveToWorldspawn)
		return NULL;

	brushesToMove = (mapbrush_t **)Mem_Alloc(numToMoveToWorldspawn * sizeof(*brushesToMove));
	if (!brushesToMove)
		Sys_Error("Check_ExtraBrushesForWorldspawn: out of memory");

	/* 0 is the world - start at 1 */
	for (i = 1, j = 0; i < num_entities; i++) {
		const entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");

		if (e->numbrushes == 1 && Q_streq(name, "func_group")) {
			assert(j < numToMoveToWorldspawn);
			brushesToMove[j++] = &mapbrushes[e->firstbrush];
		}
	}

	return brushesToMove;
}
