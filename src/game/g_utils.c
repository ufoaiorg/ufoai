/**
 * @file g_utils.c
 * @brief Misc utility functions for game module.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
#include <time.h>

/**
 * @brief Marks the edict as free
 * @sa G_Spawn
 */
void G_FreeEdict (edict_t * ed)
{
	/* unlink from world */
	gi.unlinkentity(ed);

	memset(ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = qfalse;
}

/**
 * @brief Searches for the obj that has the given firedef
 */
static const objDef_t* G_GetObjectForFiredef (const fireDef_t* fd)
{
	int i, j, k;
	const fireDef_t *csiFD;
	const objDef_t *od;

	/* For each object ... */
	for (i = 0; i < gi.csi->numODs; i++) {
		od = &gi.csi->ods[i];
		/* For each weapon-entry in the object ... */
		for (j = 0; j < od->numWeapons; j++) {
			/* For each fire-definition in the weapon entry  ... */
			for (k = 0; k < od->numFiredefs[j]; k++) {
				csiFD = &od->fd[j][k];
				if (csiFD == fd)
					return od;
			}
		}
	}

	Com_DPrintf(DEBUG_GAME, "Could nor find a objDef_t for fireDef_t '%s'\n", fd->name);

	return NULL;
}

/**
 * @brief Return the corresponding weapon name for a give firedef
 * @sa G_GetObjectForFiredef
 */
const char* G_GetWeaponNameForFiredef (fireDef_t* fd)
{
	const objDef_t* obj = G_GetObjectForFiredef(fd);
	if (!obj)
		return "unknown";
	else
		return obj->id;
}

/**
 * @brief Returns the player name for a give player number
 */
const char* G_GetPlayerName (int pnum)
{
	if (pnum >= game.sv_maxplayersperteam)
		return "";
	else
		return game.players[pnum].pers.netname;
}

/**
 * @brief Prints stats to game console and stats log file
 * @sa G_PrintActorStats
 */
void G_PrintStats (const char *buffer)
{
	struct tm *t;
	char tbuf[32];
	time_t aclock;

	time(&aclock);
	t = localtime(&aclock);

	Com_sprintf(tbuf, sizeof(tbuf), "%4i/%02i/%02i %02i:%02i:%02i", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	Com_Printf("[STATS] %s - %s\n", tbuf, buffer);
	if (logstatsfile)
		fprintf(logstatsfile, "[STATS] %s - %s\n", tbuf, buffer);
}

/**
 * @brief Prints stats about who killed who with what and how
 * @sa G_Damage
 * @sa G_PrintStats
 */
void G_PrintActorStats (edict_t* victim, edict_t* attacker, fireDef_t* fd)
{
	const char *victimName = NULL, *attackerName = NULL;
	char buffer[512];

	if (victim->pnum != attacker->pnum) {
		victimName = G_GetPlayerName(victim->pnum);
		if (!*victimName) { /* empty string */
			switch (victim->team) {
			case TEAM_CIVILIAN:
				victimName = "civilian";
				break;
			case TEAM_ALIEN:
				victimName = "alien";
				break;
			default:
				victimName = "unknown";
				break;
			}
		}
		attackerName = G_GetPlayerName(attacker->pnum);
		if (!*attackerName) { /* empty string */
			switch (attacker->team) {
			case TEAM_CIVILIAN:
				attackerName = "civilian";
				break;
			case TEAM_ALIEN:
				attackerName = "alien";
				break;
			default:
				attackerName = "unknown";
				break;
			}
		}
		if (victim->team != attacker->team) {
			Com_sprintf(buffer, sizeof(buffer), "%s (%s) %s %s (%s) with %s of %s",
				attackerName, attacker->chr.name,
				(victim->HP == 0 ? "kills" : "stuns"),
				victimName, victim->chr.name, fd->name, G_GetWeaponNameForFiredef(fd));
		} else {
			Com_sprintf(buffer, sizeof(buffer), "%s (%s) %s %s (%s) (teamkill) with %s of %s",
				attackerName, attacker->chr.name,
				(victim->HP == 0 ? "kills" : "stuns"),
				victimName, victim->chr.name, fd->name, G_GetWeaponNameForFiredef(fd));
		}
	} else {
		attackerName = G_GetPlayerName(attacker->pnum);
		Com_sprintf(buffer, sizeof(buffer), "%s %s %s (own team) with %s of %s",
			attackerName, (victim->HP == 0 ? "kills" : "stuns"),
			victim->chr.name, fd->name, G_GetWeaponNameForFiredef(fd));
	}
	G_PrintStats(buffer);
}

/**
 * @brief Searches all active entities for the next one that holds
 * the matching string at fieldofs (use the offsetof() macro) in the structure.
 *
 * @note Searches beginning at the edict after from, or the beginning if NULL
 * @return NULL will be returned if the end of the list is reached.
 */
edict_t *G_Find (edict_t * from, int fieldofs, char *match)
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
edict_t *G_FindRadius (edict_t * from, vec3_t org, float rad, entity_type_t type)
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
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (from->origin[j] + (from->mins[j] + from->maxs[j]) * 0.5);
		if (VectorLength(eorg) > rad)
			continue;
		if (type != ET_NULL && from->type != type)
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

edict_t *G_PickTarget (char *targetname)
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
 * @brief This is just a convenience function for making temporary vectors for function calls
 */
float *tv (float x, float y, float z)
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
char *vtos (vec3_t v)
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

/**
 * @brief Return the yaw component of the angle vector
 */
float vectoyaw (vec3_t vec)
{
	float yaw;

	if (/*vec[YAW] == 0 && */ vec[PITCH] == 0) {
		yaw = 0;
		if (vec[YAW] > 0)
			yaw = 90;
		else if (vec[YAW] < 0)
			yaw = -90;
	} else {
		yaw = (int) (atan2(vec[YAW], vec[PITCH]) * todeg);
		if (yaw < 0)
			yaw += 360;
	}

	return yaw;
}

/**
 * @brief Allocates memory in TAG_LEVEL context for edicts
 */
char *G_CopyString (const char *in)
{
	char *out;
	size_t l = strlen(in);

	out = gi.TagMalloc(l + 1, TAG_LEVEL);
	Q_strncpyz(out, in, l + 1);
	return out;
}

/**
 * @sa G_Spawn
 */
static void G_InitEdict (edict_t * e)
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
 * @sa G_FreeEdict
 */
edict_t *G_Spawn (void)
{
	int i;
	edict_t *e;

	e = &g_edicts[1];
	for (i = 1; i < globals.num_edicts; i++, e++)
		if (!e->inuse) {
			G_InitEdict(e);
			return e;
		}

	if (i == game.sv_maxentities)
		gi.error("G_Spawn: no free edicts");

	globals.num_edicts++;
	G_InitEdict(e);
	return e;
}


#define ENTLIST_LENGTH 256
static const char *entList[ENTLIST_LENGTH];

/**
 * @sa G_CompleteRecalcRouting
 * @sa Grid_RecalcRouting
 */
void G_RecalcRouting (edict_t * self)
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
 * @sa G_RecalcRouting
 */
void G_CompleteRecalcRouting (void)
{
	edict_t *ent;

	/* generate entity list */
	for (ent = g_edicts; ent < &g_edicts[globals.num_edicts]; ent++)
		if (ent->inuse && ent->model && *ent->model == '*')
			G_RecalcRouting(ent);
}

/**
 * @brief Check the world against triggers for the current entity (actor)
 */
int G_TouchTriggers (edict_t *ent)
{
	int i, num;
	edict_t *touch[MAX_EDICTS], *hit;

	if (ent->type != ET_ACTOR || ent->state & STATE_DEAD)
		return 0;

	num = gi.boxedicts(ent->absmin, ent->absmax, touch, MAX_EDICTS, AREA_TRIGGERS);

	/* be careful, it is possible to have an entity in this
	* list removed before we get to it(killtriggered) */
	for (i = 0; i < num; i++) {
		hit = touch[i];
		if (!hit->inuse)
			continue;
		if (!hit->use)
			continue;
		hit->use(hit, ent);
	}
	return num;
}
