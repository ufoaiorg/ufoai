/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "event/actor/e_event_actorendshot.h"
#include "event/actor/e_event_actordie.h"
#include "event/actor/e_event_actorstats.h"
#include "event/actor/e_event_actorstatechange.h"
#include "event/actor/e_event_actorclientaction.h"
#include "event/actor/e_event_actorreactionfirechange.h"
#include "event/actor/e_event_actorreactionfireaddtarget.h"
#include "event/actor/e_event_actorreactionfireremovetarget.h"
#include "event/actor/e_event_actorreactionfiretargetupdate.h"
#include "event/actor/e_event_actorreactionfireabortshot.h"
#include "event/actor/e_event_actorresetclientaction.h"
#include "event/actor/e_event_actorreservationchange.h"
#include "event/actor/e_event_actorrevitalised.h"
#include "event/actor/e_event_actorwound.h"
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
#include "event/world/e_event_cameraappear.h"
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
static bool CL_CheckDefault (const eventRegister_t* self, const dbuffer* msg)
{
	const int number = NET_PeekShort(msg);
	const bool result = LE_IsLocked(number);
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
	{E(EV_NULL), "", nullptr, nullptr, nullptr},
	{E(EV_RESET), "bb", CL_Reset, nullptr, nullptr},
	{E(EV_START), "b", CL_StartGame, nullptr, nullptr},
	{E(EV_ENDROUND), "b", CL_DoEndRound, nullptr, nullptr},
	{E(EV_ENDROUNDANNOUNCE), "bb", CL_EndRoundAnnounce, nullptr, nullptr},

	{E(EV_RESULTS), "", CL_ParseResults, CL_ParseResultsTime, nullptr}, /* manually parsed */
	{E(EV_CENTERVIEW), "g", CL_CenterView, nullptr, nullptr},
	{E(EV_MOVECAMERA), "g", CL_MoveView, nullptr, nullptr},

	{E(EV_ENT_APPEAR), "sbg", CL_EntAppear, CL_EntAppearTime, nullptr},
	{E(EV_ENT_PERISH), "sb", CL_EntPerish, nullptr, nullptr},
	{E(EV_ENT_DESTROY), "s", CL_EntDestroy, nullptr, nullptr},
	{E(EV_ADD_BRUSH_MODEL), "sbsbppsbb", CL_AddBrushModel, nullptr, nullptr},
	{E(EV_ADD_EDICT), "sbpp", CL_AddEdict, nullptr, nullptr},

	{E(EV_ACTOR_APPEAR), "!s!sbbbsbgbssssbbsbbbs", CL_ActorAppear, CL_ActorAppearTime, CL_CheckDefault},
	{E(EV_ACTOR_ADD), "!sbbbbgsb", CL_ActorAdd, nullptr, nullptr},
	{E(EV_ACTOR_TURN), "sb", CL_ActorDoTurn, nullptr, nullptr},
	{E(EV_ACTOR_MOVE), "sbsss!lg", CL_ActorDoMove, CL_ActorDoMoveTime, CL_CheckDefault}, /* Don't use this format string - see CL_ActorDoMove for more info */
	{E(EV_ACTOR_REACTIONFIRECHANGE), "sbbs", CL_ActorReactionFireChange, nullptr, nullptr},
	{E(EV_ACTOR_REACTIONFIREADDTARGET), "ssbb", CL_ActorReactionFireAddTarget, CL_ActorReactionFireAddTargetTime, nullptr},
	{E(EV_ACTOR_REACTIONFIREREMOVETARGET), "ssb", CL_ActorReactionFireRemoveTarget, CL_ActorReactionFireRemoveTargetTime, nullptr},
	{E(EV_ACTOR_REACTIONFIRETARGETUPDATE), "ssbb", CL_ActorReactionFireTargetUpdate, CL_ActorReactionFireTargetUpdateTime, nullptr},
	{E(EV_ACTOR_REACTIONFIREABORTSHOT), "ssb", CL_ActorReactionFireAbortShot, CL_ActorReactionFireAbortShotTime, nullptr},

	{E(EV_ACTOR_START_SHOOT), "sbgg", CL_ActorStartShoot, CL_ActorStartShootTime, nullptr},
	{E(EV_ACTOR_SHOOT), "ssbsbbbbbppb", CL_ActorDoShoot, CL_ActorDoShootTime, nullptr}, /**< @sa NET_WriteDir */
	{E(EV_ACTOR_SHOOT_HIDDEN), "bsbb", CL_ActorShootHidden, CL_ActorShootHiddenTime, nullptr},
	{E(EV_ACTOR_THROW), "ssbbbpp", CL_ActorDoThrow, CL_ActorDoThrowTime, nullptr},
	{E(EV_ACTOR_END_SHOOT), "s", CL_ActorEndShoot, CL_ActorEndShootTime, CL_CheckDefault},

	{E(EV_ACTOR_DIE), "ssbb", CL_ActorDie, CL_ActorDieTime, CL_CheckDefault},
	{E(EV_ACTOR_REVITALISED), "ss", CL_ActorRevitalised, nullptr, CL_CheckDefault},
	{E(EV_ACTOR_STATS), "!sbsbb", CL_ActorStats, nullptr, nullptr},
	{E(EV_ACTOR_STATECHANGE), "ss", CL_ActorStateChange, nullptr, CL_CheckDefault},
	{E(EV_ACTOR_RESERVATIONCHANGE), "ssss", CL_ActorReservationChange, nullptr, nullptr},
	{E(EV_ACTOR_WOUND), "sbbb", CL_ActorWound, nullptr, nullptr},

	{E(EV_INV_ADD), "s*", CL_InvAdd, CL_InvAddTime, CL_CheckDefault},
	{E(EV_INV_DEL), "sbbb", CL_InvDel, CL_InvDelTime, CL_CheckDefault},
	{E(EV_INV_AMMO), "sbbbbb", CL_InvAmmo, nullptr, nullptr},
	{E(EV_INV_RELOAD), "sbbbbb", CL_InvReload, CL_InvReloadTime, nullptr},
	/** @sa G_ReadItem, G_WriteItem */
	{E(EV_INV_TRANSFER), "sbsbbbbs", nullptr, nullptr, nullptr},

	{E(EV_MODEL_EXPLODE), "s&", CL_Explode, CL_ExplodeTime, nullptr},
	{E(EV_MODEL_EXPLODE_TRIGGERED), "s&", CL_Explode, nullptr, nullptr},

	{E(EV_PARTICLE_APPEAR), "ssp&", CL_ParticleAppear, CL_ParticleAppearTime, nullptr},
	{E(EV_PARTICLE_SPAWN), "bppp&", CL_ParticleSpawnEvent, CL_ParticleSpawnEventTime, nullptr},

	{E(EV_SOUND), "spb&", CL_SoundEvent, CL_SoundEventTime, nullptr},

	{E(EV_DOOR_OPEN), "s", CL_DoorOpen, nullptr, nullptr},
	{E(EV_DOOR_CLOSE), "s", CL_DoorClose, nullptr, nullptr},
	{E(EV_CLIENT_ACTION), "ss", CL_ActorClientAction, nullptr, nullptr},
	{E(EV_RESET_CLIENT_ACTION), "s", CL_ActorResetClientAction, nullptr, nullptr},
	{E(EV_CAMERA_APPEAR), "spbbbbb", CL_CameraAppear, nullptr, nullptr},
#undef E
};
CASSERT(lengthof(events) == EV_NUM_EVENTS);

const eventRegister_t* CL_GetEvent (const event_t eType)
{
	for (int i = EV_NULL; i < EV_NUM_EVENTS; i++) {
		if (events[i].type == eType)
			return &events[i];
	}

	Com_Error(ERR_DROP, "Could not get format string for event type %i", eType);
}

/**
 * @brief Calculates the time when the given @c step was executed in the event chain
 * @note There is only one movement event which takes some time and delays other events in the queue. If you want
 * to execute any other event during this movement event, use this function to get the proper eventTime for doing
 * so.
 * @param[in] eventTiming The structure to get the needed data from
 * @param[in] le The moving actor
 * @param[in] step The step we want to calculate the time for
 * @return @c -1 on error (invalid input data), otherwise the timestamp the move event was executed for that step
 */
int CL_GetStepTime (const eventTiming_t* eventTiming, const le_t* le, int step)
{
	const leStep_t* list = le->stepList;
	if (list == nullptr)
		return eventTiming->nextTime;
	for (int i = 0; i < le->stepIndex; i++) {
		list = list->next;
	}
	if (step < 0) {
		Com_Printf("invalid step given: %i/%i (entnum: %i with stepindex: %i)\n", step, list->steps, le->entnum, le->stepIndex);
		return list->lastMoveTime;
	}
	if (step > list->steps) {
		Com_Printf("invalid step given: %i/%i (entnum: %i with stepindex: %i)\n", step, list->steps, le->entnum, le->stepIndex);
		return list->lastMoveTime + list->lastMoveDuration;
	}
	int delay = 0;
	for (int i = 0; i < list->steps; i++) {
		if (i > step)
			break;
		delay += list->stepTimes[i];
	}
	const int eventTime = list->lastMoveTime + delay;
	return eventTime;
}

int CL_GetNextTime (const eventRegister_t* event, eventTiming_t* eventTiming, int nextTime)
{
	if (nextTime < eventTiming->nextTime) {
		Com_DPrintf(DEBUG_EVENTSYS, "CL_GetNextTime: nexttime is invalid (%i/%i): %s\n", nextTime, eventTiming->nextTime, event->name);
		return eventTiming->nextTime;
	}
	return nextTime;
}

/**
 * @brief Some sound strings may end on a '+' to indicate to use a random sound
 * which can be identified by replacing the '+' by a number
 * @param[in,out] sound The sound string that we might need to convert
 * @param[in] size The size of the @c sound buffer
 * @return Pointer to the @c sound buffer
 */
const char* CL_ConvertSoundFromEvent (char* sound, size_t size)
{
	const size_t length = strlen(sound) - 1;
	if (sound[length] != '+')
		return sound;

	for (int i = 1; i <= 99; i++) {
		if (!FS_CheckFile("sounds/%s%02i", sound, i) == -1)
			continue;
		sound[length] = '\0';
		Q_strcat(sound, size, "%02i", rand() % i + 1);
		return sound;
	}

	return "";
}
