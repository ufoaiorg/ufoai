/**
 * @file g_trigger.c
 * @brief Trigger functions
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

	/* link into the world */
	gi.LinkEdict(trigger);

	return trigger;
}

/**
 * @brief Hurt trigger
 * @sa SP_trigger_hurt
 */
static qboolean Touch_HurtTrigger (edict_t *self, edict_t *activator)
{
	/* these actors should really not be able to trigger this - they don't move anymore */
	assert(!G_IsDead(activator));
	assert(!G_IsStunned(activator));

	if (self->spawnflags & 2) {
		activator->STUN += self->dmg;
		if (activator->HP <= activator->STUN)
			G_SetStunned(activator);
	} else if (self->spawnflags & 4) {
		/** @todo Handle dazed via trigger_hurt */
	} else {
		G_TakeDamage(activator, self->dmg);
		if (activator->HP == 0)
			G_SetDead(activator);
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
	ent->child = NULL;

	gi.LinkEdict(ent);
}

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

	if (!self->owner->use) {
		gi.DPrintf("Owner of %s doesn't have a use function\n", self->classname);
		G_FreeEdict(self);
		return qfalse;
	}

	self->owner->use(self->owner, activator);

	return qfalse;
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
	ent->classname = "trigger_rescue";
	ent->type = ET_TRIGGER_RESCUE;

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	ent->touch = Touch_RescueTrigger;
	ent->reset = Reset_RescueTrigger;
	ent->child = NULL;

	gi.LinkEdict(ent);
}
