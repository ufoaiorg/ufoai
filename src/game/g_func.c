/**
 * @file g_func.c
 * @brief func_* edicts
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

/**
 * @brief QUAKED func_breakable (0.3 0.3 0.3) ?
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

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);

	Com_DPrintf(DEBUG_GAME, "func_breakable: model (%s) num: %i mins: %i %i %i maxs: %i %i %i origin: %i %i %i\n",
			ent->model, ent->mapNum, (int)ent->mins[0], (int)ent->mins[1], (int)ent->mins[2],
			(int)ent->maxs[0], (int)ent->maxs[1], (int)ent->maxs[2],
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);
}

/*
=============================================================================
DOOR FUNCTIONS
=============================================================================
*/

/**
 * @brief Trigger to open the door we are standing in front of it
 * @sa LE_DoorOpen
 * @sa LE_CloseOpen
 * @sa CL_ActorDoorAction
 */
static qboolean Touch_DoorTrigger (edict_t *self, edict_t *activator)
{
	if (!self->owner) {
		Com_DPrintf(DEBUG_GAME, "Touch_DoorTrigger: No owner for door trigger\n");
		return qfalse;
	}

	if (activator->team == TEAM_CIVILIAN || activator->team == TEAM_ALIEN) {
		/* let the ai interact with the door */
		return G_ClientUseDoor(game.players + activator->pnum, activator->number, self->number);
	} else {
		if (activator->client_action != self->owner) {
			/* prepare for client action */
			activator->client_action = self->owner;
			/* tell the hud to show the door buttons */
			gi.AddEvent(G_TeamToPM(activator->team), EV_DOOR_ACTION);
			gi.WriteShort(activator->number);
			gi.WriteShort(activator->client_action->number);
			gi.EndEvents();
			Com_DPrintf(DEBUG_GAME, "Touch_DoorTrigger: Assigned door as the current client action\n");
		} else {
			Com_DPrintf(DEBUG_GAME, "Touch_DoorTrigger: Door is already the current client action\n");
		}
		return qtrue;
	}
}

/**
 * @brief QUAKED func_door (0 .5 .8) ?
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

	/* set an inline model */
	gi.SetModel(ent, ent->model);
	ent->solid = SOLID_BSP;
	gi.LinkEdict(ent);
	ent->moveinfo.state = STATE_CLOSED;

	/* spawn the trigger entity */
	other = G_TriggerSpawn(ent);
	other->touch = Touch_DoorTrigger;

	ent->child = other;

	Com_DPrintf(DEBUG_GAME, "func_door: model (%s) num: %i solid:%i mins: %i %i %i maxs: %i %i %i absmins: %i %i %i absmaxs: %i %i %i origin: %i %i %i\n",
			ent->model, ent->mapNum, ent->solid,
			(int)ent->mins[0], (int)ent->mins[1], (int)ent->mins[2],
			(int)ent->maxs[0], (int)ent->maxs[1], (int)ent->maxs[2],
			(int)ent->absmin[0], (int)ent->absmin[1], (int)ent->absmin[2],
			(int)ent->absmax[0], (int)ent->absmax[1], (int)ent->absmax[2],
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);
}

/**
 * @brief QUAKED func_door (0 .5 .8) ?
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

	Com_DPrintf(DEBUG_GAME, "func_rotating: model (%s) num: %i mins: %i %i %i maxs: %i %i %i origin: %i %i %i\n",
			ent->model, ent->mapNum, (int)ent->mins[0], (int)ent->mins[1], (int)ent->mins[2],
			(int)ent->maxs[0], (int)ent->maxs[1], (int)ent->maxs[2],
			(int)ent->origin[0], (int)ent->origin[1], (int)ent->origin[2]);
}

