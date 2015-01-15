/**
 * @file
 * @brief func_* edicts
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

#include "g_func.h"
#include "g_actor.h"
#include "g_ai.h"
#include "g_match.h"
#include "g_move.h"
#include "g_spawn.h"
#include "g_trigger.h"
#include "g_utils.h"
#include "g_vis.h"

/**
 * @brief If an actor was standing on the breakable that is going to get destroyed, we have to let him fall to the ground
 * @param self The breakable edict
 * @param activator The touching edict
 * @note This touch function is only executed if the func_breakable edict has a HP level of 0 (e.g. it is already destroyed)
 * @return false because this is no client action
 */
static bool Touch_Breakable (Edict* self, Edict* activator)
{
	/* not yet broken */
	if (self->HP != 0)
		return false;

	/** @todo check that the actor is standing upon the breakable */
	if (G_IsActor(activator))
		G_ActorFall(activator);

	return false;
}

static bool Destroy_Breakable (Edict* self)
{
	vec3_t origin;
	const char* model = self->model;

	self->absBox.getCenter(origin);

	const char* breakSound = nullptr;
	switch (self->material) {
	case MAT_GLASS:
		breakSound = "misc/breakglass+";
		break;
	case MAT_METAL:
		breakSound = "misc/breakmetal+";
		break;
	case MAT_ELECTRICAL:
		breakSound = "misc/breakelectric+";
		break;
	case MAT_WOOD:
		breakSound = "misc/breakwood+";
		break;
	case MAT_MAX:
		break;
	}

	/* the HP value is used to decide whether this was a triggered call or a
	 * call during a fight - a triggered call will be handled differently in
	 * terms of timing and the related particle effects in the client code */
	if (self->HP == 0)
		G_EventModelExplodeTriggered(*self, breakSound);
	else
		G_EventModelExplode(*self, breakSound);

	if (self->particle)
		G_SpawnParticle(origin, self->spawnflags, self->particle);

	G_TouchEdicts(self, 10.0f);

	/* destroy the door trigger */
	if (self->child()) {
		Edict* trigger = self->child();
		/* Remove all activators and reset client actions before removing the trigger */
		linkedList_t* list = trigger->touchedList;
		while (list) {
			linkedList_t* next = list->next;
			G_TriggerRemoveFromList(trigger, static_cast<Edict*>(list->data));
			if (trigger->reset != nullptr)
				trigger->reset(trigger, static_cast<Edict*>(list->data));
			list = next;
		}
		G_FreeEdict(trigger);
	}

	/* now we can destroy the edict completely */
	G_FreeEdict(self);

	AABB oldAABB;
	gi.GetInlineModelAABB(model, oldAABB);
	GridBox rerouteOldBox;
	rerouteOldBox.set(oldAABB);
	G_RecalcRouting(model, rerouteOldBox);

	return true;
}

static bool Use_Breakable (Edict* self, Edict* activator)
{
	self->HP = 0;
	return Destroy_Breakable(self);
}

/**
 * @brief func_breakable (0.3 0.3 0.3) ?
 * Used for breakable objects.
 * @note These edicts are added client side as local models,
 * they are stored in the lmList (because they are inline models)
 * for tracing (see inlineList in cmodel.c)
 * @sa CM_EntTestLine
 * @sa LM_AddModel
 * @sa SV_SetModel
 * @sa G_SendEdictsAndBrushModels
 */
void SP_func_breakable (Edict* ent)
{
	ent->classname = "breakable";
	ent->type = ET_BREAKABLE;

	ent->flags |= FL_DESTROYABLE;

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);

	char boxStr[AABB_STRING];
	ent->entBox.asIntString(boxStr, sizeof(boxStr));
	Com_DPrintf(DEBUG_GAME, "func_breakable: model (%s) num: %i %s origin: %i %i %i\n",
			ent->model, ent->mapNum, boxStr,
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);

	ent->destroy = Destroy_Breakable;
	ent->use = Use_Breakable;
	ent->setTouch(Touch_Breakable);
}

/*
=============================================================================
DOOR FUNCTIONS
=============================================================================
*/

/**
 * @brief Slides a door
 * @note The new door state must already be set
 * @param[in,out] door The entity of the inline model. The aabb of this bmodel will get updated
 * in this function to reflect the new door position in the world
 * @sa LET_SlideDoor
 */
static void Door_SlidingUse (Edict* door)
{
	const bool open = door->doorState == STATE_OPENED;

	/* get the movement angle vector - a negative speed value will close the door*/
	vec3_t moveAngles;
	GET_SLIDING_DOOR_SHIFT_VECTOR(door->dir, open ? 1 : -1, moveAngles);

	/* get the direction vector from the movement angles that were set on the entity */
	vec3_t moveDir;
	AngleVectors(moveAngles, moveDir, nullptr, nullptr);
	VectorAbs(moveDir);

	/* calculate the distance from the movement angles and the entity size. This is the
	 * distance the door has to slide to fully open or close */
	const int distance = DotProduct(moveDir, door->size);

	/* the door is moved in one step on the server side - lerping is not needed here - so we
	 * perform the scalar multiplication with the distance the door must move in order to
	 * fully close/open */
	vec3_t distanceVec;
	VectorMul(distance, moveAngles, distanceVec);

	/* set the updated position. The bounding boxes that are used for tracing must be
	 * shifted when the door state changes. As the mins and maxs of the aabb are absolute
	 * world coordinates in the map we have to translate the position by the above
	 * calculated movement vector */
	VectorAdd(door->origin, distanceVec, door->origin);						/* calc new model position */
//	gi.SetInlineModelOrientation(door->model, door->origin, door->angles);	/* move the model out of the way */
}

/**
 * @brief Opens/closes a door
 * @note Use function for func_door
 * @todo Check if the door can be opened or closed - there should not be
 * anything in the way (e.g. an actor)
 */
static bool Door_Use (Edict* door, Edict* activator)
{
	int opening = 1;

	if (door->doorState == STATE_CLOSED) {
		door->doorState = STATE_OPENED;
		opening = 1;
	} else if (door->doorState == STATE_OPENED) {
		door->doorState = STATE_CLOSED;
		opening = -1;
	} else {
		return false;
	}

	/* remember the old location */
	AABB oldAABB;
	gi.GetInlineModelAABB(door->model, oldAABB);
	GridBox rerouteOldBox;
	rerouteOldBox.set(oldAABB);

	/* change rotation and relink */
	if (door->type == ET_DOOR) {
		if (door->dir & DOOR_OPEN_REVERSE)
			opening *= -1;
		door->angles[door->dir & 3] += DOOR_ROTATION_ANGLE * opening;
	} else if (door->type == ET_DOOR_SLIDING) {
		Door_SlidingUse(door);
	}
	gi.LinkEdict(door);

	/* maybe the server called this because the door starts opened */
	if (G_MatchIsRunning()) {
		/* let everybody know, that the door moves */
		if (door->doorState == STATE_OPENED)
			G_EventDoorOpen(*door);
		else
			G_EventDoorClose(*door);
		if (Q_strvalid(door->noise)) {
			const playermask_t playerMask = G_GetClosePlayerMask(door->origin, UNIT_SIZE * 10);
			G_EventSpawnSound(playerMask, *door, door->origin, door->noise);
		}
	}

	/* Update model orientation */
	gi.SetInlineModelOrientation(door->model, door->origin, door->angles);
	AABB newAabb;
	gi.GetInlineModelAABB(door->model, newAabb);
	GridBox rerouteNewBox;
	rerouteNewBox.set(newAabb);
	Com_DPrintf(DEBUG_GAME, "Server processed door movement.\n");

	/* Update path finding table for the new location of the model */
	G_RecalcRouting(door->model, rerouteOldBox);							/* Update path finding table */
	G_RecalcRouting(door->model, rerouteNewBox);

	if (activator && G_IsLivingActor(activator)) {
		/* Check if the player appears/perishes, seen from other teams. */
		G_CheckVis(activator);

		/* Calc new vis for the activator. */
		G_CheckVisTeamAll(activator->getTeam(), 0, activator);
	}

	return true;
}

/**
 * @brief Trigger to open the door we are standing in front of it
 * @sa CL_DoorOpen
 * @sa LE_CloseOpen
 * @sa CL_ActorDoorAction
 * @sa AI_CheckUsingDoor
 */
static bool Touch_DoorTrigger (Edict* self, Edict* activator)
{
	if (!self->owner())
		return false;
	if (!self->owner()->inuse)
		return false;
	if (!G_IsActor(activator))
		return false;

	Actor* actor = makeActor(activator);
	if (G_IsAI(actor)) {
		/* let the ai interact with the door */
		if (self->flags & FL_GROUPSLAVE)
			self = self->groupMaster;
		if (AI_CheckUsingDoor(actor, self->owner()))
			G_ActorUseDoor(actor, self->owner());
		/* we don't want the client action stuff to be send for ai actors */
		return false;
	}

	/* prepare for client action */
	G_ActorSetClientAction(actor, self->owner());
	return true;
}

/**
 * @brief Left the door trigger zone - reset the client action
 * @param self The trigger
 * @param activator The edict that left the trigger zone
 */
static void Reset_DoorTrigger (Edict* self, Edict* activator)
{
	if (activator->clientAction == self->owner())
		G_ActorSetClientAction(activator, nullptr);
}

#define REVERSE		0x00000200

/**
 * @brief func_door (0 .5 .8) ?
 * "health" if set, door is destroyable
 * @sa SV_SetModel
 * @sa LM_AddModel
 * @sa G_SendEdictsAndBrushModels
 */
void SP_func_door (Edict* ent)
{
	ent->classname = "door";
	ent->type = ET_DOOR;
	if (!ent->noise)
		ent->noise = "doors/open_close";

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);
	ent->doorState = STATE_CLOSED;
	ent->dir = YAW;

	if (ent->spawnflags & REVERSE)
		ent->dir |= DOOR_OPEN_REVERSE;

	if (ent->HP)
		ent->flags |= FL_DESTROYABLE;
	ent->flags |= FL_CLIENTACTION;

	/* spawn the trigger entity */
	Edict* other = G_TriggerSpawn(ent);
	other->setTouch(Touch_DoorTrigger);
	other->reset = Reset_DoorTrigger;
	ent->setChild(other);

	G_ActorSetTU(ent, TU_DOOR_ACTION);
	if (!ent->speed)
		ent->speed = 10;
	ent->use = Door_Use;

	/* the door should start opened */
	if (ent->spawnflags & FL_TRIGGERED)
		G_UseEdict(ent, nullptr);

	ent->destroy = Destroy_Breakable;
}

void SP_func_door_sliding (Edict* ent)
{
	ent->classname = "doorsliding";
	ent->type = ET_DOOR_SLIDING;
	if (!ent->noise)
		ent->noise = "doors/slide";

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);

	if (ent->spawnflags & REVERSE)
		ent->dir |= DOOR_OPEN_REVERSE;

	if (ent->HP)
		ent->flags |= FL_DESTROYABLE;

	ent->doorState = STATE_CLOSED;
	if (!ent->speed)
		ent->speed = 10;
	ent->use = Door_Use;

	ent->destroy = Destroy_Breakable;
}

/**
 * @brief Spawns a rotating solid inline brush model
 * @sa SV_SetModel
 * @sa LM_AddModel
 */
void SP_func_rotating (Edict* ent)
{
	ent->classname = "rotating";
	ent->type = ET_ROTATING;

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);

	/* the lower, the faster */
	if (!ent->speed)
		ent->speed = 50;

	if (ent->HP)
		ent->flags |= FL_DESTROYABLE;

	ent->destroy = Destroy_Breakable;
}
