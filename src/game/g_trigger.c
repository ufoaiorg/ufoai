/**
 * @file g_trigger.c
 * @brief Trigger functions
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_spawn.c
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

/**
 * @brief Checks whether the activator of this trigger_touch was already recognized
 * @param self The trigger self pointer
 * @param activator The activating edict (might be NULL)
 * @return @c true if the activator is already in the list of recognized edicts or no activator
 * was given, @c false if the activator is not yet part of the list
 */
qboolean G_TriggerIsInList (edict_t *self, edict_t *activator)
{
	edict_t *e = self->touchedNext;

	if (activator == NULL)
		return qtrue;

	while (e) {
		if (e == activator)
			return qtrue;
		e = e->touchedNext;
	}

	return qfalse;
}

/**
 * @brief Adds the activator to the list of recognized edicts for this trigger_touch edict
 * @param self The trigger self pointer
 * @param activator The activating edict (might be NULL)
 */
void G_TriggerAddToList (edict_t *self, edict_t *activator)
{
	edict_t *e = self->touchedNext;

	if (activator == NULL)
		return;

	if (G_TriggerIsInList(self, activator))
		return;

	activator->touchedNext = e;
	self->touchedNext = activator;
}

/**
 * @brief Removes an activator from the list or recognized edicts
 * @param self The trigger self pointer
 * @param activator The activating edict (might be NULL)
 * @return @c true if removal was successful or not needed, @c false if the activator wasn't found in the list
 */
qboolean G_TriggerRemoveFromList (edict_t *self, edict_t *activator)
{
	edict_t *prev = self;
	edict_t *e = self->touchedNext;

	if (activator == NULL)
		return qtrue;

	while (e) {
		if (e == activator) {
			prev->touchedNext = e->touchedNext;
			activator->touchedNext = NULL;
			return qtrue;
		}
		prev = e;
		e = e->touchedNext;
	}

	return qfalse;
}

edict_t* G_TriggerSpawn (edict_t *owner)
{
	edict_t* trigger;
	vec3_t mins, maxs;

	trigger = G_Spawn();
	trigger->classname = "trigger";
	trigger->type = ET_TRIGGER;
	/* e.g. link the door into the trigger */
	trigger->owner = owner;

	VectorCopy(owner->absmin, mins);
	VectorCopy(owner->absmax, maxs);

	/* expand the trigger box */
	mins[0] -= (UNIT_SIZE / 2);
	mins[1] -= (UNIT_SIZE / 2);
	maxs[0] += (UNIT_SIZE / 2);
	maxs[1] += (UNIT_SIZE / 2);

	VectorCopy(mins, trigger->mins);
	VectorCopy(maxs, trigger->maxs);

	trigger->solid = SOLID_TRIGGER;
	trigger->reset = NULL;

	/* link into the world */
	gi.LinkEdict(trigger);

	return trigger;
}

/**
 * @brief Next map trigger that is going to get active once all opponents are killed
 * @sa SP_trigger_nextmap
 */
static qboolean Touch_NextMapTrigger (edict_t *self, edict_t *activator)
{
	if (activator != NULL && activator->team == self->team) {
		char command[MAX_VAR];
		self->inuse = qfalse;
		G_ClientPrintf(G_PLAYER_FROM_ENT(activator), PRINT_HUD, _("Switching map!\n"));
		Com_sprintf(command, sizeof(command), "map %s %s\n",
				level.day ? "day" : "night", self->nextmap);
		level.mapEndCommand = (char *)G_TagMalloc(strlen(command) + 1, TAG_GAME);
		Q_strncpyz(level.mapEndCommand, command, strlen(command));

		level.nextMapSwitch = qtrue;
		G_MatchEndTrigger(self->team, 0);
	}
	return qtrue;
}

/**
 * @brief Register this think function once you would like to end the match
 * This think function will register the touch callback and spawns the particles for
 * the client to see the next map trigger.
 */
void Think_NextMapTrigger (edict_t *self)
{
	vec3_t center;
	pos3_t centerPos;

	VectorCenterFromMinsMaxs(self->absmin, self->absmax, center);

	/* spawn the particle to mark the trigger */
	G_SpawnParticle(center, self->spawnflags, self->particle);
	VecToPos(center, centerPos);
	G_EventCenterViewAt(PM_ALL, centerPos);

	gi.BroadcastPrintf(PRINT_HUD, _("You are now ready to switch the map"));

	self->touch = Touch_NextMapTrigger;
	self->think = NULL;
}

void SP_trigger_nextmap (edict_t *ent)
{
	/* only used in single player */
	if (sv_maxclients->integer > 1) {
		G_FreeEdict(ent);
		return;
	}

	if (!ent->particle) {
		gi.DPrintf("particle isn't set for %s\n", ent->classname);
		G_FreeEdict(ent);
		return;
	}
	if (!ent->nextmap) {
		gi.DPrintf("nextmap isn't set for %s\n", ent->classname);
		G_FreeEdict(ent);
		return;
	}

	if (Q_streq(ent->nextmap, level.mapname)) {
		gi.DPrintf("nextmap loop detected\n");
		G_FreeEdict(ent);
		return;
	}

	ent->classname = "trigger_nextmap";
	ent->type = ET_TRIGGER_NEXTMAP;

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	ent->reset = NULL;
	ent->child = NULL;

	gi.LinkEdict(ent);
}

/**
 * @brief Hurt trigger
 * @sa SP_trigger_hurt
 */
qboolean Touch_HurtTrigger (edict_t *self, edict_t *activator)
{
	const int damage = self->dmg;

	/* these actors should really not be able to trigger this - they don't move anymore */
	if (G_IsDead(activator))
		return qfalse;
	if (G_IsStunned(activator))
		return qfalse;

	if (self->spawnflags & 2) {
		activator->STUN += damage;
	} else if (self->spawnflags & 4) {
		/** @todo Handle dazed via trigger_hurt */
	} else {
		G_TakeDamage(activator, damage);
	}

	return qtrue;
}

/**
 * @brief Trigger for grid fields if they are under fire
 * @note Called once for every step
 * @sa Touch_HurtTrigger
 */
void SP_trigger_hurt (edict_t *ent)
{
	ent->classname = "trigger_hurt";
	ent->type = ET_TRIGGER_HURT;

	if (!ent->dmg)
		ent->dmg = 5;

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	ent->touch = Touch_HurtTrigger;
	ent->reset = NULL;
	ent->child = NULL;

	gi.LinkEdict(ent);
}

#define TRIGGER_TOUCH_ONCE (1 << 0)

/**
 * @brief Touch trigger
 * @sa SP_trigger_touch
 */
static qboolean Touch_TouchTrigger (edict_t *self, edict_t *activator)
{
	/* these actors should really not be able to trigger this - they don't move anymore */
	assert(!G_IsDead(activator));
	assert(!G_IsDead(activator));

	self->owner = G_FindTargetEntity(self->target);
	if (!self->owner) {
		gi.DPrintf("Target '%s' wasn't found for %s\n", self->target, self->classname);
		G_FreeEdict(self);
		return qfalse;
	}

	if (self->owner->flags & FL_CLIENTACTION) {
		G_ActorSetClientAction(activator, self->owner);
	} else if (!(self->spawnflags & TRIGGER_TOUCH_ONCE) || self->touchedNext == NULL) {
		if (!self->owner->use) {
			gi.DPrintf("Owner of %s doesn't have a use function\n", self->classname);
			G_FreeEdict(self);
			return qfalse;
		}
		G_UseEdict(self->owner, activator);
	}

	return qfalse;
}

static void Reset_TouchTrigger (edict_t *self, edict_t *activator)
{
	/* fire the use function on leaving the trigger area */
	if (activator != NULL && (self->owner->flags & FL_CLIENTACTION))
		G_ActorSetClientAction(activator, NULL);
	else if ((self->spawnflags & TRIGGER_TOUCH_ONCE) && self->touchedNext == NULL)
		G_UseEdict(self->owner, activator);
}

/**
 * @brief Touch trigger to call the use function of the attached target
 * @note Called once for every step
 * @sa Touch_TouchTrigger
 */
void SP_trigger_touch (edict_t *ent)
{
	ent->classname = "trigger_touch";
	ent->type = ET_TRIGGER_TOUCH;

	if (!ent->target) {
		gi.DPrintf("No target given for %s\n", ent->classname);
		G_FreeEdict(ent);
		return;
	}

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	ent->touch = Touch_TouchTrigger;
	ent->reset = Reset_TouchTrigger;
	ent->child = NULL;

	gi.LinkEdict(ent);
}

/**
 * @brief Rescue trigger
 * @sa SP_trigger_resuce
 */
static qboolean Touch_RescueTrigger (edict_t *self, edict_t *activator)
{
	/* these actors should really not be able to trigger this - they don't move anymore */
	assert(!G_IsDead(activator));
	assert(!G_IsDead(activator));

	if (self->team == activator->team)
		G_ActorSetInRescueZone(activator, qtrue);

	return qfalse;
}

static void Reset_RescueTrigger (edict_t *self, edict_t *activator)
{
	if (self->team == activator->team)
		G_ActorSetInRescueZone(activator, qfalse);
}

/**
 * @brief Rescue trigger to mark an actor to be in the rescue
 * zone. Aborting a game would not kill the actors inside this
 * trigger area.
 * @note Called once for every step
 * @sa Touch_RescueTrigger
 */
void SP_trigger_rescue (edict_t *ent)
{
	/* only used in single player */
	if (sv_maxclients->integer > 1) {
		G_FreeEdict(ent);
		return;
	}

	ent->classname = "trigger_rescue";
	ent->type = ET_TRIGGER_RESCUE;

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	if (ent->spawnflags == 0)
		ent->spawnflags |= 0xFF;
	ent->touch = Touch_RescueTrigger;
	ent->reset = Reset_RescueTrigger;
	ent->child = NULL;

	gi.LinkEdict(ent);
}
