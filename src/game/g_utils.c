/**
 * @file g_utils.c
 * @brief Misc utility functions for game module.
 */

/*
All original materal Copyright (C) 2002-2006 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

Original file from Quake 2 v3.21: quake2-2.31/game/g_utils.c
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

#include "g_local.h"

#if 0
/**
 * @brief
 * @note unused
 */
void G_ProjectSource(vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
	result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
	result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
	result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}
#endif

/**
 * @brief Marks the edict as free
 */
void G_FreeEdict(edict_t * ed)
{
	/* unlink from world */
	gi.unlinkentity(ed);

	memset(ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = qfalse;
}


/**
 * @brief Searches all active entities for the next one that holds
 * the matching string at fieldofs (use the offsetof() macro) in the structure.
 *
 * @note Searches beginning at the edict after from, or the beginning if NULL
 * @return NULL will be returned if the end of the list is reached.
 */
edict_t *G_Find(edict_t * from, int fieldofs, char *match)
{
	char *s;

	if (!from)
		from = g_edicts;
	else
		from++;

	for (; from < &g_edicts[globals.num_edicts]; from++) {
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *) from + fieldofs);
		if (!s)
			continue;
		if (!Q_stricmp(s, match))
			return from;
	}

	return NULL;
}


/**
 * @brief Returns entities that have origins within a spherical area
 * @param[in] from The origin that is the center of the circle
 * @param[in] org origin
 * @param[in] rad radius to search an edict in
 */
edict_t *findradius(edict_t * from, vec3_t org, float rad)
{
	vec3_t eorg;
	int j;

	if (!from)
		from = g_edicts;
	else
		from++;
	for (; from < &g_edicts[globals.num_edicts]; from++) {
		if (!from->inuse)
			continue;
		if (from->solid == SOLID_NOT)
			continue;
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (from->origin[j] + (from->mins[j] + from->maxs[j]) * 0.5);
		if (VectorLength(eorg) > rad)
			continue;
		return from;
	}

	return NULL;
}

#if 0
/**
 * @brief Searches all active entities for the next one that holds
 * the matching string at fieldofs (use the offsetof() macro) in the structure.
 *
 * @note Searches beginning at the edict after from, or the beginning if NULL
 * @return NULL will be returned if the end of the list is reached.
 * @note unused
 */
#define MAXCHOICES	8

edict_t *G_PickTarget(char *targetname)
{
	edict_t *ent = NULL;
	int num_choices = 0;
	edict_t *choice[MAXCHOICES];

	if (!targetname) {
		gi.dprintf("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while (1) {
		ent = G_Find(ent, offsetof(edict_t, targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices) {
		gi.dprintf("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}
#endif

/**
 * @brief
 */
void Think_Delay(edict_t * ent)
{
}

/**
 * @brief This is just a convenience function for making temporary vectors for function calls
 */
float *tv(float x, float y, float z)
{
	static int index;
	static vec3_t vecs[8];
	float *v;

	/* use an array so that multiple tempvectors won't collide */
	/* for a while */
	v = vecs[index];
	index = (index + 1) & 7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


/**
 * @brief This is just a convenience function for printing vectors
 */
char *vtos(vec3_t v)
{
	static int index;
	static char str[8][32];
	char *s;

	/* use an array so that multiple vtos won't collide */
	s = str[index];
	index = (index + 1) & 7;

	Com_sprintf(s, 32, "(%i %i %i)", (int) v[0], (int) v[1], (int) v[2]);

	return s;
}


static const vec3_t VEC_UP = { 0, -1, 0 };
static const vec3_t MOVEDIR_UP = { 0, 0, 1 };
static const vec3_t VEC_DOWN = { 0, -2, 0 };
static const vec3_t MOVEDIR_DOWN = { 0, 0, -1 };

#if 0
/**
 * @brief
 * @note unused
 */
void G_SetMovedir(vec3_t angles, vec3_t movedir)
{
	if (VectorCompare(angles, VEC_UP))
		VectorCopy(MOVEDIR_UP, movedir);
	else if (VectorCompare(angles, VEC_DOWN))
		VectorCopy(MOVEDIR_DOWN, movedir);
	else
		AngleVectors(angles, movedir, NULL, NULL);

	VectorClear(angles);
}
#endif

float vectoyaw(vec3_t vec)
{
	float yaw;

	if ( /*vec[YAW] == 0 && */ vec[PITCH] == 0) {
		yaw = 0;
		if (vec[YAW] > 0)
			yaw = 90;
		else if (vec[YAW] < 0)
			yaw = -90;
	} else {
		yaw = (int) (atan2(vec[YAW], vec[PITCH]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}

/**
 * @brief Allocates memory in TAG_LEVEL context for edicts
 */
char *G_CopyString(char *in)
{
	char *out;
	int l = strlen(in);

	out = gi.TagMalloc(l + 1, TAG_LEVEL);
	Q_strncpyz(out, in, l + 1);
	return out;
}

/**
 * @brief
 * @sa G_Spawn
 */
static void G_InitEdict(edict_t * e)
{
	e->inuse = qtrue;
	e->classname = "noclass";
	e->number = e - g_edicts;
}

/**
 * @brief Either finds a free edict, or allocates a new one.
 * @note Try to avoid reusing an entity that was recently freed, because it
 * can cause the player to think the entity morphed into something else
 * instead of being removed and recreated, which can cause interpolated
 * angles and bad trails.
 * @sa G_InitEdict
 */
edict_t *G_Spawn(void)
{
	int i;
	edict_t *e;

	e = &g_edicts[1];
	for (i = 1; i < globals.num_edicts; i++, e++)
		if (!e->inuse) {
			G_InitEdict(e);
			return e;
		}

	if (i == game.maxentities)
		gi.error("ED_Alloc: no free edicts");

	globals.num_edicts++;
	G_InitEdict(e);
	return e;
}


#define ENTLIST_LENGTH 256
static char *entList[ENTLIST_LENGTH];

/**
 * @brief
 * @sa G_CompleteRecalcRouting
 * @sa Grid_RecalcRouting
 */
void G_RecalcRouting(edict_t * self)
{
	int i;
	edict_t *ent;

	/* generate entity list */
	for (i = 0, ent = g_edicts; ent < &g_edicts[globals.num_edicts]; ent++)
		if (ent->inuse && ent->model && *ent->model == '*')
			entList[i++] = ent->model;
	entList[i] = NULL;

	/* recalculate routing */
	gi.GridRecalcRouting(gi.map, self->model, entList);
}

/**
 * @brief
 * @sa G_RecalcRouting
 */
void G_CompleteRecalcRouting(void)
{
	edict_t *ent;

	/* generate entity list */
	for (ent = g_edicts; ent < &g_edicts[globals.num_edicts]; ent++)
		if (ent->inuse && ent->model && *ent->model == '*')
			G_RecalcRouting(ent);
}
