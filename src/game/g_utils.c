/**
 * @file g_utils.c
 * @brief Misc utility functions for game module.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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
void G_FreeEdict (edict_t *ent)
{
	/* unlink from world */
	gi.UnlinkEdict(ent);

	memset(ent, 0, sizeof(*ent));
	ent->classname = "freed";
	ent->inuse = qfalse;
}

/**
 * @brief Call the 'use' function for the given edict and all its group members
 * @param[in] ent The edict to call the use function for
 * @sa G_ClientUseEdict
 */
qboolean G_UseEdict (edict_t *ent)
{
	if (!ent)
		return qfalse;

	/* no use function assigned */
	if (!ent->use)
		return qfalse;

	if (!ent->use(ent))
		return qfalse;

	/* only the master edict is calling the opening for the other group parts */
	if (!(ent->flags & FL_GROUPSLAVE)) {
		edict_t* chain = ent->groupChain;
		while (chain) {
			G_UseEdict(chain);
			chain = chain->groupChain;
		}
	}

	return qtrue;
}

/**
 * @brief Searches for the obj that has the given firedef
 */
static const objDef_t* G_GetObjectForFiredef (const fireDef_t* fd)
{
	int i, j, k;

	/* For each object ... */
	for (i = 0; i < gi.csi->numODs; i++) {
		const objDef_t *od = &gi.csi->ods[i];
		/* For each weapon-entry in the object ... */
		for (j = 0; j < od->numWeapons; j++) {
			/* For each fire-definition in the weapon entry  ... */
			for (k = 0; k < od->numFiredefs[j]; k++) {
				const fireDef_t *csiFD = &od->fd[j][k];
				if (csiFD == fd)
					return od;
			}
		}
	}

	return NULL;
}

/**
 * @brief Return the corresponding weapon name for a give firedef
 * @sa G_GetObjectForFiredef
 */
const char* G_GetWeaponNameForFiredef (const fireDef_t *fd)
{
	const objDef_t* obj = G_GetObjectForFiredef(fd);
	if (!obj)
		return "unknown";
	else
		return obj->id;
}

/**
 * @param team The team the player data should be searched for
 * @return The inuse player for the given team
 */
player_t* G_GetPlayerForTeam (int team)
{
	int i;
	player_t *p;

	/* search corresponding player (even ai players) */
	for (i = 0, p = game.players; i < game.sv_maxplayersperteam * 2; i++, p++)
		if (p->inuse && p->pers.team == team)
			/* found player */
			return p;

	return NULL;
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
	gi.dprintf("[STATS] %s - %s\n", tbuf, buffer);
	if (logstatsfile)
		fprintf(logstatsfile, "[STATS] %s - %s\n", tbuf, buffer);
}

/**
 * @brief Prints stats about who killed who with what and how
 * @sa G_Damage
 * @sa G_PrintStats
 */
void G_PrintActorStats (const edict_t *victim, const edict_t *attacker, const fireDef_t *fd)
{
	char buffer[512];

	if (victim->pnum != attacker->pnum) {
		const char *victimName = G_GetPlayerName(victim->pnum);
		const char *attackerName = G_GetPlayerName(attacker->pnum);
		if (victimName[0] == '\0') { /* empty string */
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
		if (attackerName[0] == '\0') { /* empty string */
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
		const char *attackerName = G_GetPlayerName(attacker->pnum);
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
	const char *s;

	if (!from)
		from = g_edicts;
	else
		from++;

	for (; from < &g_edicts[globals.num_edicts]; from++) {
		if (!from->inuse)
			continue;
		s = *(const char **) ((byte *) from + fieldofs);
		if (!s)
			continue;
		if (!Q_strcasecmp(s, match))
			return from;
	}

	return NULL;
}

edict_t *G_FindTargetEntity (const char *target)
{
	int i;

	for (i = 0; i < globals.num_edicts; i++) {
		const char *n = g_edicts[i].targetname;
		if (n && !strcmp(n, target))
			return &g_edicts[i];
	}

	return NULL;
}

/**
 * @brief Returns entities that have origins within a spherical area.
 * @param[in] from The origin that is the center of the circle
 * @param[in] org origin
 * @param[in] rad radius to search an edict in
 * @param[in] type Type of entity.
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

#define IS_BMODEL(ent) ((ent)->inuse && (ent)->model && (ent)->model[0] == '*' && (ent)->solid == SOLID_BSP)

/**
 * @brief creates an entity list
 * @param[out] entList A list of all active inline model entities
 * @sa G_RecalcRouting
 * @sa G_LineVis
 */
void G_GenerateEntList (const char *entList[MAX_EDICTS])
{
	int i;
	edict_t *ent;

	/* generate entity list */
	for (i = 0, ent = g_edicts; ent < &g_edicts[globals.num_edicts]; ent++)
		if (IS_BMODEL(ent))
			entList[i++] = ent->model;
	entList[i] = NULL;
}

/**
 * @sa G_CompleteRecalcRouting
 * @sa Grid_RecalcRouting
 */
void G_RecalcRouting (const edict_t * ent)
{
	const char *entList[MAX_EDICTS];
	/* generate entity list */
	G_GenerateEntList(entList);
	/* recalculate routing */
	gi.GridRecalcRouting(gi.routingMap, ent->model, entList);
}

/**
 * @sa G_RecalcRouting
 */
void G_CompleteRecalcRouting (void)
{
	edict_t *ent;

	/* generate entity list */
	for (ent = g_edicts; ent < &g_edicts[globals.num_edicts]; ent++)
		if (IS_BMODEL(ent)) {
			Com_DPrintf(DEBUG_GAME, "Processing entity %i: inuse:%i model:%s solid:%i\n",
				ent->number, ent->inuse, ent->model ? ent->model : "", ent->solid);
			G_RecalcRouting(ent);
		} else {
			Com_DPrintf(DEBUG_GAME, "Did not process entity %i: inuse:%i model:%s solid:%i\n",
				ent->number, ent->inuse, ent->model ? ent->model : "", ent->solid);
		}
}

/**
 * @brief Check the world against triggers for the current entity (actor)
 */
int G_TouchTriggers (edict_t *ent)
{
	int i, num, usedNum = 0;
	edict_t *touch[MAX_EDICTS];

	if (ent->type != ET_ACTOR || G_IsDead(ent))
		return 0;

	num = gi.BoxEdicts(ent->absmin, ent->absmax, touch, MAX_EDICTS, AREA_TRIGGERS);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t *hit = touch[i];
		if (!hit->inuse)
			continue;
		if (!hit->touch)
			continue;
		if (hit->touch(hit, ent))
			usedNum++;
	}
	return usedNum;
}

/**
 * @brief Call after linking a new trigger in or destroying a bmodel
 * during gameplay to force all entities it covers to immediately touch it
 */
void G_TouchSolids (edict_t *ent)
{
	int i, num;
	edict_t *touch[MAX_EDICTS];

	num = gi.BoxEdicts(ent->absmin, ent->absmax, touch, MAX_EDICTS, AREA_SOLID);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it(killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t* hit = touch[i];
		if (!hit->inuse)
			continue;
		if (ent->touch)
			ent->touch(ent, hit);
		if (!ent->inuse)
			break;
	}
}

/**
 * @brief Call after linking a new trigger in or destroying a bmodel
 * during gameplay to force all entities it covers to immediately touch it
 * @param[in] extend Extend value for the bounding box
 */
void G_TouchEdicts (edict_t *ent, float extend)
{
	int i, num;
	edict_t *touch[MAX_EDICTS];
	vec3_t absmin, absmax;
	const char *entName = (ent->model) ? ent->model : ent->chr.name;

	for (i = 0; i < 3; i++) {
		absmin[i] = ent->absmin[i] - extend;
		absmax[i] = ent->absmax[i] + extend;
	}

	num = gi.TouchEdicts(absmin, absmax, touch, MAX_EDICTS, ent);
	Com_DPrintf(DEBUG_GAME, "G_TouchEdicts: Entities touching %s: %i (%f extent).\n", entName, num, extend);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it(killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t* hit = touch[i];
        const char *hitName = (hit->model) ? hit->model : hit->chr.name;
        Com_DPrintf(DEBUG_GAME, "G_TouchEdicts: %s touching %s.\n", entName, hitName);
		if (!hit->inuse)
			continue;
		if (ent->touch)
			ent->touch(ent, hit);
	}
}
