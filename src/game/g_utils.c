/**
 * @file g_utils.c
 * @brief Misc utility functions for game module.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
	G_EventDestroyEdict(ent);

	/* unlink from world */
	gi.UnlinkEdict(ent);

	OBJZERO(*ent);
	ent->classname = "freed";
	ent->inuse = qfalse;
}

/**
 * @brief Searches an actor at the given grid location.
 * @param pos The grid location to look for an edict.
 * @return @c NULL if nothing was found, otherwise the actor located at the given grid position.
 */
edict_t *G_GetLivingActorFromPos (const pos3_t pos)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		if (!VectorCompare(pos, ent->pos))
			continue;

		return ent;
	}
	/* nothing found at this pos */
	return NULL;
}

/**
 * @brief Searches an edict of the given type at the given grid location.
 * @param pos The grid location to look for an edict.
 * @param type The type of the edict to look for or @c -1 to look for any type in the search.
 * @return @c NULL if nothing was found, otherwise the entity located at the given grid position.
 */
edict_t *G_GetEdictFromPos (const pos3_t pos, const entity_type_t type)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		if (type > ET_NULL && ent->type != type)
			continue;
		if (!VectorCompare(pos, ent->pos))
			continue;

		return ent;
	}
	/* nothing found at this pos */
	return NULL;
}

/**
 * @brief Searches an edict that is not of the given types at the given grid location.
 * @param pos The grid location to look for an edict.
 * @param n The amount of given entity_type_t values that are given via variadic arguments to this function.
 * @return @c NULL if nothing was found, otherwise the entity located at the given grid position.
 */
edict_t *G_GetEdictFromPosExcluding (const pos3_t pos, const int n, ...)
{
	edict_t *ent = NULL;
	entity_type_t types[ET_MAX];
	va_list ap;
	int i;

	assert(n > 0);
	assert(n < sizeof(types));

	va_start(ap, n);

	for (i = 0; i < n; i++) {
		types[i] = va_arg(ap, entity_type_t);
	}

	while ((ent = G_EdictsGetNextInUse(ent))) {
		for (i = 0; i < n; i++)
			if (ent->type == types[i])
				break;
		if (i != n)
			continue;
		if (VectorCompare(pos, ent->pos))
			return ent;
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
 * @todo What if there are multiple players for a team (multiplayer coop match)
 */
player_t* G_GetPlayerForTeam (int team)
{
	player_t *p;

	/* search corresponding player (even ai players) */
	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p)))
		if (p->pers.team == team)
			/* found player */
			return p;

	p = NULL;
	while ((p = G_PlayerGetNextActiveAI(p)))
		if (p->pers.team == team)
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
 * @brief Renders all the traces on the client side if the cvar @c g_drawtraces is activated
 * @param start The start vector of the trace
 * @param end The end vector of the trace
 */
static inline void G_TraceDraw (const vec3_t start, const vec3_t end)
{
	if (g_drawtraces->integer)
		G_EventParticleSpawn(PM_ALL, "fadeTracerDebug", TRACING_ALL_VISIBLE_LEVELS, start, end, vec3_origin);
}

/**
 * @brief fast version of a line trace including entities
 * @param start The start vector of the trace
 * @param end The end vector of the trace
 * @return @c false if not blocked
 */
qboolean G_TestLineWithEnts (const vec3_t start, const vec3_t end)
{
	const char *entList[MAX_EDICTS];
	/* generate entity list */
	G_GenerateEntList(entList);
	G_TraceDraw(start, end);
	/* test for visibility */
	return gi.TestLineWithEnt(start, end, TL_FLAG_NONE, entList);
}

/**
 * @brief fast version of a line trace but without including entities
 * @param start The start vector of the trace
 * @param end The end vector of the trace
 * @return @c false if not blocked
 */
qboolean G_TestLine (const vec3_t start, const vec3_t end)
{
	G_TraceDraw(start, end);
	return gi.TestLine(start, end, TL_FLAG_NONE);
}

/**
 * @brief collision detection - this version is more accurate and includes entity tests
 * @note traces a box from start to end, ignoring entities passent, stopping if it hits an object of type specified
 * via contentmask (MASK_*).
 * @return The trace result
 */
trace_t G_Trace (const vec3_t start, const vec3_t end, const edict_t * passent, int contentmask)
{
	G_TraceDraw(start, end);
	return gi.Trace(start, NULL, NULL, end, passent, contentmask);
}

/**
 * @brief Returns the player name for a give player number
 */
const char* G_GetPlayerName (int pnum)
{
	if (pnum >= game.sv_maxplayersperteam)
		return "";
	return game.players[pnum].pers.netname;
}

/**
 * @brief Prints stats to game console and stats log file
 * @sa G_PrintActorStats
 */
void G_PrintStats (const char *buffer)
{
	gi.DPrintf("[STATS] %s\n", buffer);
	if (logstatsfile) {
		struct tm *t;
		char tbuf[32];
		time_t aclock;

		time(&aclock);
		t = localtime(&aclock);

		Com_sprintf(tbuf, sizeof(tbuf), "%4i/%02i/%02i %02i:%02i:%02i", t->tm_year + 1900,
				t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

		fprintf(logstatsfile, "[STATS] %s - %s\n", tbuf, buffer);
	}
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

	if (attacker != NULL && fd != NULL) {
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
				Com_sprintf(buffer, sizeof(buffer), "%s (%s) %s %s (%s) with %s of %s (entnum: %i)",
					attackerName, attacker->chr.name,
					(victim->HP == 0 ? "kills" : "stuns"),
					victimName, victim->chr.name, fd->name, G_GetWeaponNameForFiredef(fd), victim->number);
			} else {
				Com_sprintf(buffer, sizeof(buffer), "%s (%s) %s %s (%s) (teamkill) with %s of %s (entnum: %i)",
					attackerName, attacker->chr.name,
					(victim->HP == 0 ? "kills" : "stuns"),
					victimName, victim->chr.name, fd->name, G_GetWeaponNameForFiredef(fd), victim->number);
			}
		} else {
			const char *attackerName = G_GetPlayerName(attacker->pnum);
			Com_sprintf(buffer, sizeof(buffer), "%s %s %s (own team) with %s of %s (entnum: %i)",
				attackerName, (victim->HP == 0 ? "kills" : "stuns"),
				victim->chr.name, fd->name, G_GetWeaponNameForFiredef(fd), victim->number);
		}
	} else {
		const char *victimName = G_GetPlayerName(victim->pnum);
		Com_sprintf(buffer, sizeof(buffer), "%s (%s) was %s (entnum: %i)",
			victimName, victim->chr.name, (victim->HP == 0 ? "killed" : "stunned"), victim->number);
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
	edict_t *ent = from;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		const char *s = *(const char **) ((byte *) ent + fieldofs);
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
 * the edict that has the targetname set you were looking for.
 */
edict_t *G_FindTargetEntity (const char *target)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNext(ent))) {
		const char *n = ent->targetname;
		if (n && Q_streq(n, target))
			return ent;
	}

	return NULL;
}

/**
 * @brief Returns entities that have origins within a spherical area.
 * @param[in] from The entity to start the search from. @c NULL will start from the beginning.
 * @param[in] org The origin that is the center of the circle.
 * @param[in] rad radius to search an edict in.
 * @param[in] type Type of entity. @c ET_NULL to ignore the type.
 * @code
 * edict_t* ent = NULL;
 * while ((ent = G_FindRadius(ent, origin, rad, type)) != NULL) {
 *   [...]
 * }
 * @endcode
 */
edict_t *G_FindRadius (edict_t * from, const vec3_t org, float rad, entity_type_t type)
{
	edict_t *ent = from;

	while ((ent = G_EdictsGetNextInUse(ent))) {
		int j;
		vec3_t eorg;
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (ent->origin[j] + (ent->mins[j] + ent->maxs[j]) * 0.5);
		if (VectorLength(eorg) > rad)
			continue;
		if (type != ET_NULL && ent->type != type)
			continue;
		return ent;
	}

	return NULL;
}

#define IS_BMODEL(ent) ((ent)->model && (ent)->model[0] == '*' && (ent)->solid == SOLID_BSP)

/**
 * @brief creates an entity list
 * @param[out] entList A list of all active inline model entities
 * @sa G_RecalcRouting
 * @sa G_LineVis
 */
void G_GenerateEntList (const char *entList[MAX_EDICTS])
{
	int i = 0;
	edict_t *ent = NULL;

	/* generate entity list */
	while ((ent = G_EdictsGetNextInUse(ent)))
		if (IS_BMODEL(ent))
			entList[i++] = ent->model;
	entList[i] = NULL;
}

/**
 * @sa G_CompleteRecalcRouting
 * @sa Grid_RecalcRouting
 */
void G_RecalcRouting (const char* model)
{
	const char *entList[MAX_EDICTS];
	/* generate entity list */
	G_GenerateEntList(entList);
	/* recalculate routing */
	gi.GridRecalcRouting(gi.routingMap, model, entList);
}

/**
 * @sa G_RecalcRouting
 */
void G_CompleteRecalcRouting (void)
{
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextInUse(ent)))
		if (IS_BMODEL(ent))
			G_RecalcRouting(ent->model);
}

/**
 * @brief Call the reset function for those triggers that are no longer touched (left the trigger zone)
 * @param ent The edict that is leaving the trigger area
 * @param touched The edicts that the activating ent currently touches
 * @param num The amount of edicts in the @c touched list
 */
static void G_ResetTriggers (edict_t *ent, edict_t **touched, int num)
{
	edict_t *trigger = NULL;

	/* check all edicts to find all triggers */
	while ((trigger = G_EdictsGetNextInUse(trigger))) {
		if (trigger->solid == SOLID_TRIGGER) {
			/* check if our edict is among the known triggerers of this trigger */
			if (G_TriggerIsInList(trigger, ent)) {
				/* if so, check if it still touches it */
				int i;
				for (i = 0; i < num; i++) {
					if (touched[i] == trigger)
						break;	/* Yes ! */
				}
				if (i == num) {	/* No ! */
					G_TriggerRemoveFromList(trigger, ent);
					/* the ent left the trigger area */
					if (trigger->reset != NULL)
						trigger->reset(trigger, ent);
				}
			}
		}
	}
}

/**
 * @brief Check the world against triggers for the current entity
 * @param[in,out] ent The entity that maybe touches others
 * @return Returns the number of associated client actions
 */
int G_TouchTriggers (edict_t *ent)
{
	int i, num, usedNum = 0;
	edict_t *touch[MAX_EDICTS];

	if (!G_IsLivingActor(ent))
		return 0;

	num = gi.TouchEdicts(ent->absmin, ent->absmax, touch, lengthof(touch), ent);

	G_ResetTriggers(ent, touch, num);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t *hit = touch[i];
		if (hit->solid != SOLID_TRIGGER)
			continue;
		if (!hit->touch)
			continue;
		if (hit->touch(hit, ent))
			usedNum++;
		/* now after the use function was executed, we can add the ent to
		 * the touched list of the trigger. We do this because we want to be
		 * able to check whether another call changes the triggered state for
		 * the added entity. We have to do this after the use function was
		 * called, because there are triggers that may only be triggered once
		 * if someone touches it. */
		G_TriggerAddToList(hit, ent);
	}
	return usedNum;
}

/**
 * @brief Call after making a step to a new grid tile to immediately touch edicts whose
 * bbox intersects with the edicts' bbox
 * @return the amount of touched edicts
 */
int G_TouchSolids (edict_t *ent, float extend)
{
	int i, num, usedNum = 0;
	vec3_t absmin, absmax;
	edict_t *touch[MAX_EDICTS];

	if (!G_IsLivingActor(ent))
		return 0;

	for (i = 0; i < 3; i++) {
		absmin[i] = ent->absmin[i] - extend;
		absmax[i] = ent->absmax[i] + extend;
	}

	num = gi.TouchEdicts(absmin, absmax, touch, lengthof(touch), ent);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t* hit = touch[i];
		if (hit->solid == SOLID_TRIGGER)
			continue;
		if (!hit->inuse)
			continue;
		if (!hit->touch)
			continue;
		hit->touch(hit, ent);
		usedNum++;
	}
	return usedNum;
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

	num = gi.TouchEdicts(absmin, absmax, touch, lengthof(touch), ent);
	Com_DPrintf(DEBUG_GAME, "G_TouchEdicts: Entities touching %s: %i (%f extent).\n", entName, num, extend);

	/* be careful, it is possible to have an entity in this
	 * list removed before we get to it (killtriggered) */
	for (i = 0; i < num; i++) {
		edict_t* hit = touch[i];
		if (!hit->inuse)
			continue;
		if (ent->touch)
			ent->touch(ent, hit);
	}
}

/**
 * @brief Calculates the level flags for a given grid position.
 */
uint32_t G_GetLevelFlagsFromPos (const pos3_t pos)
{
	uint32_t levelflags = 0;
	int i;
	for (i = 0; i < PATHFINDING_HEIGHT; i++) {
		if (i >= pos[2]) {
			levelflags |= (1 << i);
		}
	}

	return levelflags;
}
