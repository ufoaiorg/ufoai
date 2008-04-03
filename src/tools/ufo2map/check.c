/**
 * @file check.c
 * @brief Performs check on a loaded mapfile
 */

/*
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

#include "common/shared.h"
#include "common/bspfile.h"
#include "check.h"

static int checkLight (const entity_t *e, int entnum)
{
	return 0;
}

static int checkFuncRotating(const entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val)
		Com_Printf("func_rotating with no levelflags given - entnum: %i\n", entnum);
	return 0;
}

static int checkFuncDoor (const entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val)
		Com_Printf("func_door with no levelflags given - entnum: %i\n", entnum);
	return 0;
}

static int checkMiscModel (const entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val)
		Com_Printf("misc_model with no levelflags given - entnum: %i\n", entnum);
	return 0;
}

static int checkInfoPlayerStart (const entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "team");
	if (!*val)
		Com_Printf("info_player_start with no team given - entnum: %i\n", entnum);
	return 0;
}

typedef struct entityCheck_s {
	const char *name;
	int (*checkCallback)(const entity_t* e, int entnum);
} entityCheck_t;

static const entityCheck_t checkArray[] = {
	{"light", checkLight},
	{"func_door", checkFuncDoor},
	{"func_rotating", checkFuncRotating},
	{"misc_model", checkMiscModel},
	{"info_player_start", checkInfoPlayerStart},

	{NULL, NULL}
};

/**
 * @brief Perform an entity check
 */
void CheckEntities (void)
{
	int i;
	const entityCheck_t *v;

	/* 0 is the world - start at 1 */
	for (i = 0; i < num_entities; i++) {
		const entity_t *e = &entities[i];
		const char *name = ValueForKey(e, "classname");
		for (v = checkArray; v->name; v++)
			if (!strncmp(name, v->name, strlen(v->name))) {
				v->checkCallback(e, i);
				break;
			}
	}
}

void CheckBrushes (void)
{

}

void FixErrors (void)
{
	/* @todo fix brushes */
	/* @todo fix entities */

	/* update dentdata */
	UnparseEntities();

	/* @todo write new map file */
}
