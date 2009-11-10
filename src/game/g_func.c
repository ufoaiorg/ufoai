/**
 * @file g_func.c
 * @brief func_* edicts
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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

static qboolean Destroy_Breakable (edict_t *self)
{
	vec3_t origin;
	VectorCenterFromMinsMaxs(self->absmin, self->absmax, origin);

	/* the HP value is used to decide whether this was a triggered call or a
	 * call during a fight - a triggered call will be handled differently in
	 * terms of timing and the related particle effects in the client code */
	if (self->HP == 0)
		gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE_TRIGGERED);
	else
		gi.AddEvent(PM_ALL, EV_MODEL_EXPLODE);
	gi.WriteShort(self->number);
	if (self->particle)
		G_ParticleSpawn(origin, self->spawnflags, self->particle);

	switch (self->material) {
	case MAT_GLASS:
		gi.PositionedSound(PM_ALL, origin, self, "misc/breakglass");
		break;
	case MAT_METAL:
		gi.PositionedSound(PM_ALL, origin, self, "misc/breakmetal");
		break;
	case MAT_ELECTRICAL:
		gi.PositionedSound(PM_ALL, origin, self, "misc/breakelectric");
		break;
	case MAT_WOOD:
		gi.PositionedSound(PM_ALL, origin, self, "misc/breakwood");
		break;
	case MAT_MAX:
		break;
	}
	/* unlink to update the routing */
	gi.UnlinkEdict(self);
	self->inuse = qfalse;
	self->HP = 0;
	G_RecalcRouting(self);
	/* now we can destroy the edict completely */
	G_FreeEdict(self);

	return qtrue;
}

/**
 * @brief func_breakable (0.3 0.3 0.3) ?
 * Used for breakable objects.
 * SPAWNFLAGS bit values:
 *  0x0001 Visible from layer 1
 *  0x0002 Visible from layer 2
 *  0x0004 Visible from layer 3
 *  0x0008 Visible from layer 4
 *  0x0010 Visible from layer 5
 *  0x0020 Visible from layer 6
 *  0x0040 Visible from layer 7
 *  0x0080 Visible from layer 8
 *  0x0100 Not present during day (defined in src/client/cl_view.c)
 *  0x0200 Glows (defined in src/client/cl_view.c)
 * @note These edicts are added client side as local models
 * the are stored in the lmList (because they are inline models)
 * for tracing (see inlineList in cmodel.c)
 * @sa CM_EntTestLine
 * @sa LM_AddModel
 * @sa SV_SetModel
 * @sa G_SendEdictsAndBrushModels
 */
void SP_func_breakable (edict_t *ent)
{
	ent->classname = "breakable";
	ent->type = ET_BREAKABLE;

	ent->flags |= FL_DESTROYABLE;

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);

	Com_DPrintf(DEBUG_GAME, "func_breakable: model (%s) num: %i mins: %i %i %i maxs: %i %i %i origin: %i %i %i\n",
			ent->model, ent->mapNum, (int)ent->mins[0], (int)ent->mins[1], (int)ent->mins[2],
			(int)ent->maxs[0], (int)ent->maxs[1], (int)ent->maxs[2],
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);

	ent->destroy = Destroy_Breakable;
}

/*
=============================================================================
DOOR FUNCTIONS
=============================================================================
*/

/**
 * @brief Opens/closes a door
 * @note Use function for func_door
 * @todo Check if the door can be opened or closed - there should not be
 * anything in the way (e.g. an actor)
 */
static qboolean Door_Use (edict_t *door)
{
	if (door->doorState == STATE_CLOSED) {
		door->doorState = STATE_OPENED;

		/* change rotation and relink */
		door->angles[YAW] += DOOR_ROTATION_ANGLE;
		gi.LinkEdict(door);

		/* maybe the server called this because the door starts opened */
		if (G_MatchIsRunning()) {
			/* let everybody know, that the door opens */
			gi.AddEvent(PM_ALL, EV_DOOR_OPEN);
			gi.WriteShort(door->number);
			if (door->noise[0] != '\0')
				gi.PositionedSound(PM_ALL, door->origin, door, door->noise);
		}
	} else if (door->doorState == STATE_OPENED) {
		door->doorState = STATE_CLOSED;

		/* change rotation and relink */
		door->angles[YAW] -= DOOR_ROTATION_ANGLE;
		gi.LinkEdict(door);

		/* closed is the standard, opened is handled above - we need an active
		 * team here already, otherwise this is a bug */
		assert(G_MatchIsRunning());
		/* let everybody know, that the door closes */
		gi.AddEvent(PM_ALL, EV_DOOR_CLOSE);
		gi.WriteShort(door->number);
		if (door->noise[0] != '\0')
			gi.PositionedSound(PM_ALL, door->origin, door, door->noise);
	} else
		return qfalse;

	/* Update model orientation */
	gi.SetInlineModelOrientation(door->model, door->origin, door->angles);
	Com_DPrintf(DEBUG_GAME, "Server processed door movement.\n");

	/* Update path finding table */
	G_RecalcRouting(door);

	G_SendTriggerBoundingBoxes(PM_ALL, door);

	return qtrue;
}

/**
 * @brief Trigger to open the door we are standing in front of it
 * @sa CL_DoorOpen
 * @sa LE_CloseOpen
 * @sa CL_ActorDoorAction
 * @sa AI_CheckUsingDoor
 */
static qboolean Touch_DoorTrigger (edict_t *self, edict_t *activator)
{
	if (!self->owner)
		return qfalse;

	if (activator->team == TEAM_CIVILIAN || activator->team == TEAM_ALIEN) {
		/* let the ai interact with the door */
		if (self->flags & FL_GROUPSLAVE)
			self = self->groupMaster;
		if (AI_CheckUsingDoor(activator, self->owner))
			G_ClientUseEdict(G_PLAYER_FROM_ENT(activator), activator, self->owner);
		/* we don't want the client action stuff to be send for ai actors */
		return qfalse;
	} else {
		if (activator->clientAction != self->owner) {
			/* prepare for client action */
			activator->clientAction = self->owner;
			/* tell the hud to show the door buttons */
			gi.AddEvent(G_TeamToPM(activator->team), EV_DOOR_ACTION);
			gi.WriteShort(activator->number);
			gi.WriteShort(activator->clientAction->number);
			gi.EndEvents();
		}
		return qtrue;
	}
}

/**
 * @brief func_door (0 .5 .8) ?
 * "health" if set, door is destroyable
 * @sa SV_SetModel
 * @sa LM_AddModel
 * @sa G_SendEdictsAndBrushModels
 */
void SP_func_door (edict_t *ent)
{
	edict_t *other;

	ent->classname = "door";
	ent->type = ET_DOOR;
	if (!ent->noise)
		ent->noise = "doors/open_close";

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);
	ent->doorState = STATE_CLOSED;

	if (ent->HP)
		ent->flags |= FL_DESTROYABLE;

	/* spawn the trigger entity */
	other = G_TriggerSpawn(ent);
	other->touch = Touch_DoorTrigger;

	ent->TU = TU_DOOR_ACTION;
	ent->use = Door_Use;
	ent->child = other;

	/* the door should start opened */
	if (ent->spawnflags & FL_TRIGGERED)
		G_UseEdict(ent);

	Com_DPrintf(DEBUG_GAME, "func_door: model (%s) num: %i solid:%i mins: %i %i %i maxs: %i %i %i absmins: %i %i %i absmaxs: %i %i %i origin: %i %i %i\n",
			ent->model, ent->mapNum, ent->solid,
			(int)ent->mins[0], (int)ent->mins[1], (int)ent->mins[2],
			(int)ent->maxs[0], (int)ent->maxs[1], (int)ent->maxs[2],
			(int)ent->absmin[0], (int)ent->absmin[1], (int)ent->absmin[2],
			(int)ent->absmax[0], (int)ent->absmax[1], (int)ent->absmax[2],
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);

	ent->destroy = Destroy_Breakable;
}

/**
 * @brief func_door (0 .5 .8) ?
 * "health"	if set, door must be shot open
 * @sa SV_SetModel
 * @sa LM_AddModel
 */
void SP_func_rotating (edict_t *ent)
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

	Com_DPrintf(DEBUG_GAME, "func_rotating: model (%s) num: %i mins: %i %i %i maxs: %i %i %i origin: %i %i %i\n",
			ent->model, ent->mapNum, (int)ent->mins[0], (int)ent->mins[1], (int)ent->mins[2],
			(int)ent->maxs[0], (int)ent->maxs[1], (int)ent->maxs[2],
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);
}
