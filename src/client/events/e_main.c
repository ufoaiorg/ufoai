/**
 * @file e_main.c
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../client.h"
#include "e_main.h"
#include "event/actor/e_event_actorappear.h"
#include "event/actor/e_event_actoradd.h"
#include "event/actor/e_event_actormove.h"
#include "event/actor/e_event_actorturn.h"
#include "event/actor/e_event_actorstartshoot.h"
#include "event/actor/e_event_actorshoot.h"
#include "event/actor/e_event_actorshoothidden.h"
#include "event/actor/e_event_actorthrow.h"
#include "event/actor/e_event_actordie.h"
#include "event/actor/e_event_actorstats.h"
#include "event/actor/e_event_actorstatechange.h"
#include "event/actor/e_event_actordooraction.h"
#include "event/actor/e_event_actorresetclientaction.h"
#include "event/inventory/e_event_invadd.h"
#include "event/inventory/e_event_invdel.h"
#include "event/inventory/e_event_invammo.h"
#include "event/inventory/e_event_invreload.h"
#include "event/inventory/e_event_invcheckhands.h"
#include "event/player/e_event_reset.h"
#include "event/player/e_event_startgame.h"
#include "event/player/e_event_startgamedone.h"
#include "event/player/e_event_doendround.h"
#include "event/player/e_event_endroundannounce.h"
#include "event/player/e_event_results.h"
#include "event/player/e_event_centerview.h"
#include "event/world/e_event_entappear.h"
#include "event/world/e_event_entperish.h"
#include "event/world/e_event_addbrushmodel.h"
#include "event/world/e_event_addedict.h"
#include "event/world/e_event_explode.h"
#include "event/world/e_event_particleappear.h"
#include "event/world/e_event_dooropen.h"
#include "event/world/e_event_doorclose.h"

/**
 * @brief List of functions to register nodes
 * @note Functions must be sorted by node name
 */
const eventRegister_t events[] = {
	{EV_NULL, "EV_NULL", "", NULL, NULL},
	{EV_RESET, "EV_RESET", "bb", CL_Reset, NULL},
	{EV_START, "EV_START", "b", CL_StartGame, NULL},
	{EV_START_DONE, "EV_START_DONE", "", CL_StartingGameDone, NULL},
	{EV_ENDROUND, "EV_ENDROUND", "b", CL_DoEndRound, NULL},
	{EV_ENDROUNDANNOUNCE, "EV_ENDROUNDANNOUNCE", "bb", CL_EndRoundAnnounce, NULL},

	{EV_RESULTS, "EV_RESULTS", "", CL_ParseResults, NULL}, /* manually parsed */
	{EV_CENTERVIEW, "EV_CENTERVIEW", "g", CL_CenterView, NULL},

	{EV_ENT_APPEAR, "EV_ENT_APPEAR", "sbg", CL_EntAppear, NULL},
	{EV_ENT_PERISH, "EV_ENT_PERISH", "s", CL_EntPerish, NULL},
	{EV_ADD_BRUSH_MODEL, "EV_ADD_BRUSH_MODEL", "sssbppsb", CL_AddBrushModel, NULL},
	{EV_ADD_EDICT, "EV_ADD_EDICT", "sspp", CL_AddEdict, NULL},

	{EV_ACTOR_APPEAR, "EV_ACTOR_APPEAR", "!s!sbbbbgbssssbsbbbs", CL_ActorAppear, NULL},
	{EV_ACTOR_ADD, "EV_ACTOR_ADD", "!sbbbbgsb", CL_ActorAdd, NULL},
	{EV_ACTOR_TURN, "EV_ACTOR_TURN", "sb", CL_ActorDoTurn, NULL},
	{EV_ACTOR_MOVE, "EV_ACTOR_MOVE", "!sbbs", CL_ActorDoMove, NULL}, /* Don't use this format string - see CL_ActorDoMove for more info */

	{EV_ACTOR_START_SHOOT, "EV_ACTOR_START_SHOOT", "ssbbbgg", CL_ActorStartShoot, NULL},
	{EV_ACTOR_SHOOT, "EV_ACTOR_SHOOT", "sssbbbbbppb", CL_ActorDoShoot, CL_ActorDoShootTime},
	{EV_ACTOR_SHOOT_HIDDEN, "EV_ACTOR_SHOOT_HIDDEN", "bsbb", CL_ActorShootHidden, CL_ActorShootHiddenTime},
	{EV_ACTOR_THROW, "EV_ACTOR_THROW", "ssbbbpp", CL_ActorDoThrow, CL_ActorDoThrowTime},

	{EV_ACTOR_DIE, "EV_ACTOR_DIE", "ss", CL_ActorDie, NULL},
	{EV_ACTOR_STATS, "EV_ACTOR_STATS", "!sbsbb", CL_ActorStats, NULL},
	{EV_ACTOR_STATECHANGE, "EV_ACTOR_STATECHANGE", "ss", CL_ActorStateChange, NULL},

	{EV_INV_ADD, "EV_INV_ADD", "s*", CL_InvAdd, NULL},
	{EV_INV_DEL, "EV_INV_DEL", "sbbb", CL_InvDel, NULL},
	{EV_INV_AMMO, "EV_INV_AMMO", "sbbbbb", CL_InvAmmo, NULL},
	{EV_INV_RELOAD, "EV_INV_RELOAD", "sbbbbb", CL_InvReload, NULL},
	{EV_INV_HANDS_CHANGED, "EV_INV_HANDS_CHANGED", "ss", CL_InvCheckHands, NULL},
	{EV_INV_TRANSFER, "EV_INV_TRANSFER", "sbsbbbb", NULL, NULL},

	{EV_MODEL_EXPLODE, "EV_MODEL_EXPLODE", "s", CL_Explode, NULL},
	{EV_MODEL_EXPLODE_TRIGGERED, "EV_MODEL_EXPLODE_TRIGGERED", "s", CL_Explode, NULL},

	{EV_SPAWN_PARTICLE, "EV_SPAWN_PARTICLE", "ss&", CL_ParticleAppear, NULL},

	{EV_DOOR_OPEN, "EV_DOOR_OPEN", "s", CL_DoorOpen, NULL},
	{EV_DOOR_CLOSE, "EV_DOOR_CLOSE", "s", CL_DoorClose, NULL},
	{EV_DOOR_ACTION, "EV_DOOR_ACTION", "ss", CL_ActorDoorAction, NULL},
	{EV_RESET_CLIENT_ACTION, "EV_RESET_CLIENT_ACTION", "s", CL_ActorResetClientAction, NULL},
};
CASSERT(lengthof(events) == EV_NUM_EVENTS);

const eventRegister_t *CL_GetEvent (const int eType)
{
	int i;

	for (i = 0; i < EV_NUM_EVENTS; i++) {
		if (events[i].type == eType)
			return &events[i];
	}

	Com_Error(ERR_DROP, "Could not get format string for event type %i", eType);
}
