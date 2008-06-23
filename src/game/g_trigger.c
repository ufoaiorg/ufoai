/**
 * @file g_trigger.c
 * @brief Trigger functions
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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
 * @note No new event in the trigger functions!!!! They are called while moving
 */
static qboolean Touch_HurtTrigger (edict_t *self, edict_t *activator)
{
	/* these actors should really not be able to trigger this - they don't move anymore */
	assert(!(activator->state & STATE_DEAD));
	assert(!(activator->state & STATE_STUN));

	if (self->spawnflags & 2) {
		activator->STUN += self->dmg;
		if (activator->HP <= activator->STUN)
			activator->state |= STATE_STUN;
	} else if (self->spawnflags & 4) {
		/** @todo Handle dazed via trigger_hurt */
	} else {
		activator->HP = max(activator->HP - self->dmg, 0);
		if (activator->HP == 0)
			activator->state |= STATE_DEAD;
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

