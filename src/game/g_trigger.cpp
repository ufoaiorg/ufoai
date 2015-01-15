/**
 * @file
 * @brief Trigger functions
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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


#include "g_trigger.h"
#include "g_client.h"
#include "g_combat.h"
#include "g_actor.h"
#include "g_edicts.h"
#include "g_match.h"
#include "g_spawn.h"
#include "g_utils.h"

/**
 * @brief Checks whether the activator of this trigger_touch was already recognized
 * @param self The trigger self pointer
 * @param activator The activating edict (might be nullptr)
 * @return @c true if the activator is already in the list of recognized edicts or no activator
 * was given, @c false if the activator is not yet part of the list
 */
bool G_TriggerIsInList (Edict* self, Edict* activator)
{
	if (activator == nullptr)
		return true;

	const linkedList_t* entry = self->touchedList;
	while (entry) {
		if (entry->data == activator)
			return true;
		entry = entry->next;
	}

	return false;
}

/**
 * @brief Adds the activator to the list of recognized edicts for this trigger_touch edict
 * @param self The trigger self pointer
 * @param activator The activating edict (might be nullptr)
 */
void G_TriggerAddToList (Edict* self, Edict* activator)
{
	if (activator == nullptr)
		return;

	if (G_TriggerIsInList(self, activator))
		return;

	linkedList_t* entry = static_cast<linkedList_t*>(G_TagMalloc(sizeof(linkedList_t), TAG_LEVEL));
	entry->data = activator;
	entry->next = self->touchedList;
	self->touchedList = entry;
}

/**
 * @brief Removes an activator from the list of recognized edicts
 * @param self The trigger self pointer
 * @param activator The activating edict (might be nullptr)
 * @return @c true if removal was successful or not needed, @c false if the activator wasn't found in the list
 */
bool G_TriggerRemoveFromList (Edict* self, Edict* activator)
{
	if (activator == nullptr)
		return true;

	linkedList_t** list = &self->touchedList;
	while (*list) {
		linkedList_t* entry = *list;
		if (entry->data == activator) {
			*list = entry->next;
			G_MemFree(entry);
			return true;
		}
		*list = entry->next;
	}

	return false;
}

Edict* G_TriggerSpawn (Edict* owner)
{
	Edict* trigger = G_Spawn("trigger");
	trigger->type = ET_TRIGGER;
	/* set the owner, e.g. link the door into the trigger */
	trigger->_owner = owner;

	AABB aabb(owner->absBox);
	aabb.expandXY(UNIT_SIZE / 2);	/* expand the trigger box */

	trigger->entBox.set(aabb);

	trigger->solid = SOLID_TRIGGER;
	trigger->reset = nullptr;

	/* link into the world */
	gi.LinkEdict(trigger);

	return trigger;
}

/**
 * @brief Next map trigger that is going to get active once all opponents are killed
 * @sa SP_trigger_nextmap
 */
static bool Touch_NextMapTrigger (Edict* self, Edict* activator)
{
	if (activator != nullptr && activator->isSameTeamAs(self)) {
		char command[MAX_VAR];
		self->inuse = false;
		G_ClientPrintf(activator->getPlayer(), PRINT_HUD, _("Switching map!"));
		Com_sprintf(command, sizeof(command), "map %s %s\n",
				level.day ? "day" : "night", self->nextmap);
		level.mapEndCommand = (char*)G_TagMalloc(strlen(command) + 1, TAG_GAME);
		Q_strncpyz(level.mapEndCommand, command, strlen(command));

		level.nextMapSwitch = true;
		G_MatchEndTrigger(self->getTeam(), 0);
	}
	return true;
}

/**
 * @brief Register this think function once you would like to end the match
 * This think function will register the touch callback and spawns the particles for
 * the client to see the next map trigger.
 */
void Think_NextMapTrigger (Edict* self)
{
	vec3_t center;
	pos3_t centerPos;

	self->absBox.getCenter(center);

	/* spawn the particle to mark the trigger */
	G_SpawnParticle(center, self->spawnflags, self->particle);
	VecToPos(center, centerPos);
	G_EventCenterViewAt(PM_ALL, centerPos);

	gi.BroadcastPrintf(PRINT_HUD, _("You are now ready to switch the map."));

	self->setTouch(Touch_NextMapTrigger);
	self->think = nullptr;
}

void SP_trigger_nextmap (Edict* ent)
{
	/* only used in single player */
	if (G_IsMultiPlayer()) {
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

	ent->reset = nullptr;
	ent->setChild(nullptr);

	gi.LinkEdict(ent);
}

/**
 * @brief Hurt trigger
 * @sa SP_trigger_hurt
 */
bool Touch_HurtTrigger (Edict* self, Edict* activator)
{
	/* Dead actors should really not be able to trigger this - they can't be hurt anymore anyway */
	if (!G_IsLivingActor(activator))
		return false;

	/* If no damage is dealt don't count it as triggered */
	const int damage = G_ApplyProtection(activator, self->dmgtype, self->dmg);
	if (damage == 0)
		return false;

	const bool stunEl = (self->dmgtype == gi.csi->damStunElectro);
	const bool stunGas = (self->dmgtype == gi.csi->damStunGas);
	const bool shock = (self->dmgtype == gi.csi->damShock);
	const bool isRobot = activator->chr.teamDef->robot;
	Actor* actor = makeActor(activator);

	if (stunEl || (stunGas && !isRobot)) {
		actor->addStun(damage);
	} else if (shock) {
		/** @todo Handle dazed via trigger_hurt */
	} else {
		G_TakeDamage(actor, damage);
	}
	/* Play hurt sound unless this is shock damage -- it doesn't do anything
	 * because we don't actually handle it above yet */
	if (!shock) {
		const teamDef_t* teamDef = activator->chr.teamDef;
		const int gender = activator->chr.gender;
		const char* sound = teamDef->getActorSound(gender, SND_HURT);
		G_EventSpawnSound(G_PlayerToPM(activator->getPlayer()), *activator, nullptr, sound);
	}

	G_CheckDeathOrKnockout(actor, nullptr, nullptr, damage);

	return true;
}

/**
 * @brief Trigger for grid fields if they are under fire
 * @note Called once for every step
 * @sa Touch_HurtTrigger
 */
void SP_trigger_hurt (Edict* ent)
{
	ent->classname = "trigger_hurt";
	ent->type = ET_TRIGGER_HURT;

	if (!ent->dmg)
		ent->dmg = 5;
	ent->dmgtype = gi.csi->damFire;

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	ent->setTouch(Touch_HurtTrigger);
	ent->reset = nullptr;
	ent->setChild(nullptr);

	gi.LinkEdict(ent);
}

#define TRIGGER_TOUCH_ONCE (1 << 0)

/**
 * @brief Touch trigger
 * @sa SP_trigger_touch
 */
static bool Touch_TouchTrigger (Edict* self, Edict* activator)
{
	/* these actors should really not be able to trigger this - they don't move anymore */
	assert(!G_IsDead(activator));

	self->_owner = G_EdictsFindTargetEntity(self->target);
	if (!self->owner()) {
		gi.DPrintf("Target '%s' wasn't found for %s\n", self->target, self->classname);
		G_FreeEdict(self);
		return false;
	}

	if (self->owner()->flags & FL_CLIENTACTION) {
		G_ActorSetClientAction(activator, self->owner());
	} else if (!(self->spawnflags & TRIGGER_TOUCH_ONCE) || self->touchedList == nullptr) {
		if (!self->owner()->use) {
			gi.DPrintf("Owner of %s doesn't have a use function\n", self->classname);
			G_FreeEdict(self);
			return false;
		}
		G_UseEdict(self->owner(), activator);
	}

	return false;
}

static void Reset_TouchTrigger (Edict* self, Edict* activator)
{
	/* fire the use function on leaving the trigger area */
	if (activator != nullptr && (self->owner()->flags & FL_CLIENTACTION))
		G_ActorSetClientAction(activator, nullptr);
	else if ((self->spawnflags & TRIGGER_TOUCH_ONCE) && self->touchedList == nullptr)
		G_UseEdict(self->owner(), activator);
}

/**
 * @brief Touch trigger to call the use function of the attached target
 * @note Called once for every step
 * @sa Touch_TouchTrigger
 */
void SP_trigger_touch (Edict* ent)
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

	ent->setTouch(Touch_TouchTrigger);
	ent->reset = Reset_TouchTrigger;
	ent->setChild(nullptr);

	gi.LinkEdict(ent);
}

/**
 * @brief Rescue trigger
 * @sa SP_trigger_resuce
 */
static bool Touch_RescueTrigger (Edict* self, Edict* activator)
{
	/* these actors should really not be able to trigger this - they don't move anymore */
	assert(!G_IsDead(activator));

	if (G_IsActor(activator)) {
		Actor* actor = makeActor(activator);
		if (self->isSameTeamAs(actor))
			actor->setInRescueZone(true);
	}

	return false;
}

static void Reset_RescueTrigger (Edict* self, Edict* activator)
{
	if (G_IsActor(activator)) {
		Actor* actor = makeActor(activator);
		if (self->isSameTeamAs(actor))
			actor->setInRescueZone(false);
	}
}

/**
 * @brief Rescue trigger to mark an actor to be in the rescue
 * zone. Aborting a game would not kill the actors inside this
 * trigger area.
 * @note Called once for every step
 * @sa Touch_RescueTrigger
 */
void SP_trigger_rescue (Edict* ent)
{
	/* only used in single player */
	if (G_IsMultiPlayer()) {
		G_FreeEdict(ent);
		return;
	}

	ent->classname = "trigger_rescue";
	ent->type = ET_TRIGGER_RESCUE;

	ent->solid = SOLID_TRIGGER;
	gi.SetModel(ent, ent->model);

	if (ent->spawnflags == 0)
		ent->spawnflags |= 0xFF;
	ent->setTouch(Touch_RescueTrigger);
	ent->reset = Reset_RescueTrigger;
	ent->setChild(nullptr);

	gi.LinkEdict(ent);
}
