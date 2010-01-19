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
 * @brief Allocate space for the entity pointers.
 * @note No need to set it to zero, G_TagMalloc will do that for us
 */
edict_t* G_EdictsInit (void)
{
	g_edicts = G_TagMalloc(game.sv_maxentities * sizeof(g_edicts[0]), TAG_GAME);
	return g_edicts;
}

/**
 * @brief Reset the entity pointers for eg. a new game.
 */
void G_EdictsReset (void)
{
	memset(g_edicts, 0, game.sv_maxentities * sizeof(g_edicts[0]));
}

/**
 * @brief Get an entity by it's number
 * @param idx The entity's index in the array of entities
 */
edict_t* G_EdictsGetByNum (const int idx)
{
	assert(idx >= 0 && idx < globals.num_edicts);
	return g_edicts + idx;
}

/**
 * @brief Returns the world entity
 * @todo This might not always be the first edict - in case of rma they might be spread over the array.
 */
edict_t* G_EdictsGetWorld (void)
{
	return &g_edicts[0];
}

/**
 * @brief Iterate through the list of entities
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNext (edict_t* lastEnt)
{
	edict_t* endOfEnts = &g_edicts[globals.num_edicts];
	edict_t* ent;

	if (!lastEnt)
		lastEnt = g_edicts;
	assert(lastEnt >= g_edicts);
	assert(lastEnt < endOfEnts);

	ent = lastEnt;

	ent++;
	if (ent >= endOfEnts)
		return NULL;
	else
		return ent;
}

/**
 * @brief Iterate through the living actor entities
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNextLivingActor (edict_t* lastEnt)
{
	edict_t* ent = lastEnt;

	while ((ent = G_EdictsGetNext(ent))) {
		if (ent->inuse && G_IsLivingActor(ent))
			break;
	}
	return ent;
}

/**
 * @brief Iterate through the actor entities (even the dead!)
 * @param lastEnt The entity found in the previous iteration; if NULL, we start at the beginning
 */
edict_t* G_EdictsGetNextActor (edict_t* lastEnt)
{
	edict_t* ent = lastEnt;

	while ((ent = G_EdictsGetNext(ent))) {
		if (ent->inuse && G_IsActor(ent))
			break;
	}
	return ent;
}

/**
 * @brief Marks the edict as free
 * @sa G_Spawn
 */
void G_FreeEdict (edict_t *ent)
{
	G_EventDestroyEdict(ent);

	/* unlink from world */
	gi.UnlinkEdict(ent);

	memset(ent, 0, sizeof(*ent));
	ent->classname = "freed";
	ent->inuse = qfalse;
}

/**
 * @brief Searches an edict of the given type at the given grid location.
 * @param pos The grid location to look for an edict.
 * @param type The type of the edict to look for or @c -1 to look for any type in the search.
 * @return @c NULL if nothing was found, otherwise the entity located at the given grid position.
 */
edict_t *G_GetEdictFromPos (const pos3_t pos, const int type)
{
	edict_t *floor = NULL;

	while ((floor = G_EdictsGetNext(floor))) {
		if (!floor->inuse || (type > 0 && floor->type != type))
			continue;
		if (!VectorCompare(pos, floor->pos))
			continue;

		return floor;
	}
	/* nothing found at this pos */
	return NULL;
}

/**
 * @brief Call the 'use' function for the given edict and all its group members
 * @param[in] ent The edict to call the use function for
 * @param[in] activator The edict that uses ent
 * @return qtrue when there is possibility to use edict being parameter.
 * @sa G_ClientUseEdict
 */
qboolean G_UseEdict (edict_t *ent, edict_t* activator)
{
	if (!ent)
		return qfalse;

	/* no use function assigned */
	if (!ent->use)
		return qfalse;

	if (!ent->use(ent, activator))
		return qfalse;

	/* only the master edict is calling the opening for the other group parts */
	if (!(ent->flags & FL_GROUPSLAVE)) {
		edict_t* chain = ent->groupChain;
		while (chain) {
			G_UseEdict(chain, activator);
			chain = chain->groupChain;
		}
	}

	return qtrue;
}

/**
 * @brief Searches for the obj that has the given firedef.
 * @param[in] fd Pointer to fire definition, for which item is wanted.
 * @return @c od to which fire definition belongs or @c NULL when no object found.
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
 * @brief Returns the corresponding weapon name for a given fire definition.
 * @param[in] fd Pointer to fire definition, for which item is wanted.
 * @return id of the item to which fire definition belongs or "unknown" when no object found.
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
 * @brief Gets player for given team.
 * @param[in] team The team the player data should be searched for
 * @return The inuse player for the given team or @c NULL when no player found.
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
 * @brief Applies the given damage value to an edict that is either an actor or has
 * the @c FL_DESTROYABLE flag set.
 * @param ent The edict to apply the damage to.
 * @param damage The damage value.
 * @note This function assures, that the health points of the edict are never
 * getting negative.
 * @sa G_Damage
 */
void G_TakeDamage (edict_t *ent, int damage)
{
	if (G_IsBreakable(ent) || G_IsActor(ent))
		ent->HP = max(ent->HP - damage, 0);
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
 * @param[in] victim Pointer to edict being a victim.
 * @param[in] attacker Pointer to edict being an attacker.
 * @param[in] fd Pointer to fire definition used in attack.
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
 * @param[in] from If this is @c NULL all edicts will be searched, otherwise the given
 * edict will be the start of the search
 * @param[in] fieldofs The field offset of the struct member (returned by the offsetof macro)
 * of the field to search the given value for
 * @param[in] match The value of the field
 * @note Searches beginning at the edict after from, or the beginning if NULL
 * @code
 * while ((spot = G_Find(spot, FOFS(classname), "misc_whatever")) != NULL) {
 *   [..]
 * }
 * @endcode
 * @return NULL will be returned if the end of the list is reached.
 */
edict_t *G_Find (edict_t * from, int fieldofs, char *match)
{
	const char *s;
	edict_t *ent = from;

	while ((ent = G_EdictsGetNext(ent))) {
		if (!ent->inuse)
			continue;
		s = *(const char **) ((byte *) ent + fieldofs);
		if (!s)
			continue;
		if (!Q_strcasecmp(s, match))
			return ent;
	}

	return NULL;
}

/**
 * @brief Searches the edict that has the given target as @c targetname set
 * @param target The target name of the edict that you are searching
 * @return @c NULL if no edict with the given target name was found, otherwise
 * the edict that has hte targetname set you were looking for.
 */
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
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNext(ent)))
		if (IS_BMODEL(ent))
			G_RecalcRouting(ent);
}

/**
 * @brief Check the world against triggers for the current entity (actor)
 */
int G_TouchTriggers (edict_t *ent)
{
	int i, num, usedNum = 0;
	edict_t *touch[MAX_EDICTS];

	if (!G_IsLivingActor(ent))
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
 * @param[in] ent The edict to check.
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
