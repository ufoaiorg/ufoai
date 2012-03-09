/**
 * @file e_main.c
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "../../client.h"
#include "../cl_localentity.h"
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
#include "event/actor/e_event_actorclientaction.h"
#include "event/actor/e_event_actorreactionfirechange.h"
#include "event/actor/e_event_actorresetclientaction.h"
#include "event/actor/e_event_actorreservationchange.h"
#include "event/actor/e_event_actorrevitalised.h"
#include "event/inventory/e_event_invadd.h"
#include "event/inventory/e_event_invdel.h"
#include "event/inventory/e_event_invammo.h"
#include "event/inventory/e_event_invreload.h"
#include "event/player/e_event_reset.h"
#include "event/player/e_event_startgame.h"
#include "event/player/e_event_doendround.h"
#include "event/player/e_event_endroundannounce.h"
#include "event/player/e_event_results.h"
#include "event/player/e_event_centerview.h"
#include "event/world/e_event_entappear.h"
#include "event/world/e_event_entperish.h"
#include "event/world/e_event_entdestroy.h"
#include "event/world/e_event_addbrushmodel.h"
#include "event/world/e_event_addedict.h"
#include "event/world/e_event_explode.h"
#include "event/world/e_event_particleappear.h"
#include "event/world/e_event_particlespawn.h"
#include "event/world/e_event_dooropen.h"
#include "event/world/e_event_doorclose.h"
#include "event/world/e_event_sound.h"

/**
 * @brief A default check function that assumes the entnum is the first short in msg.
 * @param self The event we're checking.
 * @param msg The buffer containing event information.  It is not changed during the
 *  call, so it doesn't need to be copied before, and can be used as new afterwards.
 */
static qboolean CL_CheckDefault (const eventRegister_t *self, const struct dbuffer *msg)
{
	const int number = NET_PeekShort(msg);
	const qboolean result = LE_IsLocked(number);
	if (result)
		Com_DPrintf(DEBUG_EVENTSYS, "CL_CheckDefault: Delaying event on entnum %i.\n", number);
	return (!result);
}

/**
 * @brief List of functions to register nodes
 * @note Functions must be sorted by node name
 */
const eventRegister_t events[] = {
#define E(x) x, STRINGIFY(x)
	{E(EV_NULL), "", NULL, NULL, NULL},
	{E(EV_RESET), "bb", CL_Reset, NULL, NULL},
	{E(EV_START), "b", CL_StartGame, NULL, NULL},
	{E(EV_ENDROUND), "b", CL_DoEndRound, NULL, NULL},
	{E(EV_ENDROUNDANNOUNCE), "bb", CL_EndRoundAnnounce, NULL, NULL},

	{E(EV_RESULTS), "", CL_ParseResults, CL_ParseResultsTime, NULL}, /* manually parsed */
	{E(EV_CENTERVIEW), "g", CL_CenterView, NULL, NULL},

	{E(EV_ENT_APPEAR), "sbg", CL_EntAppear, CL_EntAppearTime, NULL},
	{E(EV_ENT_PERISH), "sb", CL_EntPerish, NULL, NULL},
	{E(EV_ENT_DESTROY), "s", CL_EntDestroy, NULL, NULL},
	{E(EV_ADD_BRUSH_MODEL), "bssbppsbb", CL_AddBrushModel, NULL, NULL},
	{E(EV_ADD_EDICT), "sbpp", CL_AddEdict, NULL, NULL},

	{E(EV_ACTOR_APPEAR), "!s!sbbbsbgbssssbsbbbs", CL_ActorAppear, CL_ActorAppearTime, CL_CheckDefault},
	{E(EV_ACTOR_ADD), "!sbbbbgsb", CL_ActorAdd, NULL, NULL},
	{E(EV_ACTOR_TURN), "sb", CL_ActorDoTurn, NULL, NULL},
	{E(EV_ACTOR_MOVE), "!sbbs", CL_ActorDoMove, CL_ActorDoMoveTime, CL_CheckDefault}, /* Don't use this format string - see CL_ActorDoMove for more info */
	{E(EV_ACTOR_REACTIONFIRECHANGE), "sbbs", CL_ActorReactionFireChange, NULL, NULL},

	{E(EV_ACTOR_START_SHOOT), "sbgg", CL_ActorStartShoot, CL_ActorStartShootTime, NULL},
	{E(EV_ACTOR_SHOOT), "ssbsbbbbbppb", CL_ActorDoShoot, CL_ActorDoShootTime, NULL}, /**< @sa NET_WriteDir */
	{E(EV_ACTOR_SHOOT_HIDDEN), "bsbb", CL_ActorShootHidden, CL_ActorShootHiddenTime, NULL},
	{E(EV_ACTOR_THROW), "ssbbbpp", CL_ActorDoThrow, CL_ActorDoThrowTime, NULL},

	{E(EV_ACTOR_DIE), "ssb", CL_ActorDie, CL_ActorDieTime, CL_CheckDefault},
	{E(EV_ACTOR_REVITALISED), "ss", CL_ActorRevitalised, NULL, CL_CheckDefault},
	{E(EV_ACTOR_STATS), "!sbsbb", CL_ActorStats, NULL, NULL},
	{E(EV_ACTOR_STATECHANGE), "ss", CL_ActorStateChange, NULL, NULL},
	{E(EV_ACTOR_RESERVATIONCHANGE), "ssss", CL_ActorReservationChange, NULL, NULL},

	{E(EV_INV_ADD), "s*", CL_InvAdd, CL_InvAddTime, NULL},
	{E(EV_INV_DEL), "sbbb", CL_InvDel, NULL, NULL},
	{E(EV_INV_AMMO), "sbbbbb", CL_InvAmmo, NULL, NULL},
	{E(EV_INV_RELOAD), "sbbbbb", CL_InvReload, CL_InvReloadTime, NULL},
	{E(EV_INV_TRANSFER), "sbsbbbbs", NULL, NULL, NULL},

	{E(EV_MODEL_EXPLODE), "s", CL_Explode, CL_ExplodeTime, NULL},
	{E(EV_MODEL_EXPLODE_TRIGGERED), "s", CL_Explode, NULL, NULL},

	{E(EV_PARTICLE_APPEAR), "ssp&", CL_ParticleAppear, CL_ParticleAppearTime, NULL},
	{E(EV_PARTICLE_SPAWN), "bppp&", CL_ParticleSpawnEvent, CL_ParticleSpawnEventTime, NULL},

	{E(EV_SOUND), "sp&", CL_SoundEvent, NULL, NULL},

	{E(EV_DOOR_OPEN), "s", CL_DoorOpen, NULL, NULL},
	{E(EV_DOOR_CLOSE), "s", CL_DoorClose, NULL, NULL},
	{E(EV_CLIENT_ACTION), "ss", CL_ActorClientAction, NULL, NULL},
	{E(EV_RESET_CLIENT_ACTION), "s", CL_ActorResetClientAction, NULL, NULL},
#undef E
};
CASSERT(lengthof(events) == EV_NUM_EVENTS);

const eventRegister_t *CL_GetEvent (const event_t eType)
{
	event_t i;

	for (i = EV_NULL; i < EV_NUM_EVENTS; i++) {
		if (events[i].type == eType)
			return &events[i];
	}

	Com_Error(ERR_DROP, "Could not get format string for event type %i", eType);
}

int CL_GetNextTime (const eventRegister_t *event, eventTiming_t *eventTiming, int nextTime)
{
	if (nextTime < eventTiming->nextTime) {
		Com_DPrintf(DEBUG_EVENTSYS, "CL_GetNextTime: nexttime is invalid (%i/%i): %s\n", nextTime, eventTiming->nextTime, event->name);
		return eventTiming->nextTime;
	}
	return nextTime;
}
