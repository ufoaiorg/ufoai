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
#include "common/scriplib.h"
#include "check.h"
#include "bsp.h"

static int checkLight (entity_t *e, int entnum)
{
	return 0;
}

static int checkFuncRotating (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("func_rotating with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", CONTENTS_LEVEL_ALL);
		SetKeyValue(e, "spawnflags", buf);
	}
	return 0;
}

static int checkFuncDoor (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("func_door with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", CONTENTS_LEVEL_ALL);
		SetKeyValue(e, "spawnflags", buf);
	}
	return 0;
}

static int checkFuncBreakable (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("func_breakable with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", CONTENTS_LEVEL_ALL);
		SetKeyValue(e, "spawnflags", buf);
	}
	return 0;
}

static int checkMiscModel (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "spawnflags");
	if (!*val) {
		char buf[16];
		Com_Printf("misc_model with no levelflags given - entnum: %i\n", entnum);
		snprintf(buf, sizeof(buf) - 1, "%i", CONTENTS_LEVEL_ALL);
		SetKeyValue(e, "spawnflags", buf);
	}
	return 0;
}

static int checkInfoPlayerStart (entity_t *e, int entnum)
{
	const char *val = ValueForKey(e, "team");
	if (!*val)
		Com_Printf("info_player_start with no team given - entnum: %i\n", entnum);
	return 0;
}

typedef struct entityCheck_s {
	const char *name;
	int (*checkCallback)(entity_t* e, int entnum);
} entityCheck_t;

static const entityCheck_t checkArray[] = {
	{"light", checkLight},
	{"func_breakable", checkFuncBreakable},
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
		entity_t *e = &entities[i];
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
	int i, j;

	for (i = 0; i < nummapbrushes; i++) {
		mapbrush_t *brush = &mapbrushes[i];
		const int contentFlags = brush->original_sides[0].contentFlags;
		for (j = 0; j < brush->numsides; j++) {
			side_t *side = &brush->original_sides[j];
			brush_texture_t *tex = &side_brushtextures[side - brushsides];
			if (contentFlags != side->contentFlags) {
				Com_Printf("Brush %i (entity %i): mixed face contents (f: %i, %i)\n", brush->brushnum, brush->entitynum, brush->contentFlags, side->contentFlags);
			}
			if (!(side->contentFlags & (CONTENTS_WEAPONCLIP | CONTENTS_ORIGIN | CONTENTS_ACTORCLIP | CONTENTS_STEPON))) {
				/* check level 1 - level 8 */
				if (!(side->contentFlags & CONTENTS_LEVEL_ALL)) {
					Com_Printf("Brush %i (entity %i): no levelflags\n", brush->entitynum, brush->brushnum);
					side->contentFlags |= CONTENTS_LEVEL_ALL;
				}
			}
			if (!Q_strcmp(tex->name, "NULL")) {
				Com_Printf("Brush %i (entity %i): replaced NULL with nodraw texture\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
				side->surfaceFlags |= SURF_NODRAW;
			}
			if (side->surfaceFlags & SURF_NODRAW && Q_strcmp(tex->name, "tex_common/nodraw")) {
				Com_Printf("Brush %i (entity %i): set nodraw texture for SURF_NODRAW\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/nodraw", sizeof(tex->name));
			}
			if (side->surfaceFlags & SURF_HINT && Q_strcmp(tex->name, "tex_common/hint")) {
				Com_Printf("Brush %i (entity %i): set hint texture for SURF_HINT\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/hint", sizeof(tex->name));
			}

			if (side->contentFlags & CONTENTS_ACTORCLIP && side->contentFlags & CONTENTS_STEPON) {
				if (!Q_strcmp(tex->name, "tex_common/actorclip")) {
					Com_Printf("Brush %i (entity %i): mixed CONTENTS_STEPON and CONTENTS_ACTORCLIP - removed CONTENTS_STEPON\n", brush->brushnum, brush->entitynum);
					side->contentFlags &= ~CONTENTS_STEPON;
				} else {
					Com_Printf("Brush %i (entity %i): mixed CONTENTS_STEPON and CONTENTS_ACTORCLIP - removed CONTENTS_ACTORCLIP\n", brush->brushnum, brush->entitynum);
					side->contentFlags &= ~CONTENTS_ACTORCLIP;
				}
			}

			if (side->contentFlags & CONTENTS_WEAPONCLIP && Q_strcmp(tex->name, "tex_common/weaponclip")) {
				Com_Printf("Brush %i (entity %i): set weaponclip texture for CONTENTS_WEAPONCLIP\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/weaponclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ACTORCLIP && Q_strcmp(tex->name, "tex_common/actorclip")) {
				Com_Printf("Brush %i (entity %i): set actorclip texture for CONTENTS_ACTORCLIP\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/actorclip", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_STEPON && Q_strcmp(tex->name, "tex_common/stepon")) {
				Com_Printf("Brush %i (entity %i): set stepon texture for CONTENTS_STEPON\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/stepon", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ORIGIN && Q_strcmp(tex->name, "tex_common/origin")) {
				Com_Printf("Brush %i (entity %i): set origin texture for CONTENTS_ORIGIN\n", brush->brushnum, brush->entitynum);
				Q_strncpyz(tex->name, "tex_common/origin", sizeof(tex->name));
			}
			if (side->contentFlags & CONTENTS_ORIGIN && brush->entitynum == 0) {
				Com_Printf("Brush %i (entity %i): origin brush inside worldspawn - removed CONTENTS_ORIGIN\n", brush->brushnum, brush->entitynum);
				side->contentFlags &= ~CONTENTS_ORIGIN;
			}
		}
	}
}

void FixErrors (void)
{
	CheckBrushes();
	CheckEntities();

	/* update dentdata */
	UnparseEntities();

	WriteMapFile(GetScriptFile());
}
