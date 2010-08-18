/**
 * @file e_time.c
 * @brief Battlescape event timing code
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "e_time.h"
#include "e_main.h"

#define OLDEVENTTIME

#ifdef OLDEVENTTIME
/** @brief CL_ParseEvent timers and vars */
static int nextTime;	/**< time when the next event should be executed */
static int shootTime;	/**< time when the shoot was fired */
static int impactTime;	/**< time when the shoot hits the target */
static qboolean parsedDeath;	/**< extra delay caused by death - @sa @c impactTime */
#endif

/**
 * @brief Calculates the time the event should get executed. If two events return the same time,
 * they are going to be executed in the order the were parsed.
 * @param[in] eType The event type
 * @param[in,out] msg The message buffer that can be modified to get the event time
 * @param[in] dt Delta time in msec since the last event was parsed
 */
int CL_GetEventTime (const event_t eType, struct dbuffer *msg, const int dt)
{
	const eventRegister_t *eventData = CL_GetEvent(eType);

#ifdef OLDEVENTTIME
	/* the time the event should be executed. This value is used to sort the
	 * event chain to determine which event must be executed at first. This
	 * value also ensures, that the events are executed in the correct
	 * order. E.g. @c impactTime is used to delay some events in case the
	 * projectile needs some time to reach its target. */
	int eventTime;

	if (eType == EV_RESET) {
		parsedDeath = qfalse;
		nextTime = 0;
		shootTime = 0;
		impactTime = 0;
	} else if (eType == EV_ACTOR_DIE)
		parsedDeath = qtrue;

	/* get event time */
	if (nextTime < cl.time)
		nextTime = cl.time;
	if (impactTime < cl.time)
		impactTime = cl.time;

	if (eType == EV_ACTOR_DIE || eType == EV_MODEL_EXPLODE)
		eventTime = impactTime;
	else if (eType == EV_ACTOR_SHOOT || eType == EV_ACTOR_SHOOT_HIDDEN)
		eventTime = shootTime;
	else if (eType == EV_RESULTS)
		eventTime = nextTime + 1400;
	else
		eventTime = nextTime;

	if (eType == EV_ENT_APPEAR || eType == EV_INV_ADD || eType == EV_PARTICLE_APPEAR || eType == EV_PARTICLE_SPAWN) {
		if (parsedDeath) { /* drop items after death (caused by impact) */
			eventTime = impactTime + 400;
			/* EV_INV_ADD messages are the last events sent after a death */
			if (eType == EV_INV_ADD)
				parsedDeath = qfalse;
		} else if (impactTime > cl.time) { /* item thrown on the ground */
			eventTime = impactTime + 75;
		}
	}

	/* calculate time interval before the next event */
	switch (eType) {
	case EV_ACTOR_APPEAR:
		if (cl.actTeam != cls.team)
			nextTime += 600;
		break;
	case EV_INV_RELOAD:
		/* let the reload sound play */
		nextTime += 600;
		break;
	case EV_ACTOR_START_SHOOT:
		nextTime += 300;
		shootTime = nextTime;
		break;
	case EV_ACTOR_SHOOT_HIDDEN:
		{
			int first;
			int objIdx;
			const objDef_t *obj;
			weaponFireDefIndex_t weapFdsIdx;
			fireDefIndex_t fireDefIndex;

			NET_ReadFormat(msg, eventData->formatString, &first, &objIdx, &weapFdsIdx, &fireDefIndex);

			obj = INVSH_GetItemByIDX(objIdx);
			if (first) {
				nextTime += 500;
				impactTime = shootTime = nextTime;
			} else {
				const fireDef_t *fd = FIRESH_GetFiredef(obj, weapFdsIdx, fireDefIndex);
				/* impact right away - we don't see it at all
				 * bouncing is not needed here, too (we still don't see it) */
				impactTime = shootTime;
				nextTime = shootTime + 1400;
				if (fd->delayBetweenShots)
					shootTime += 1000 / fd->delayBetweenShots;
			}
			parsedDeath = qfalse;
		}
		break;
	case EV_ACTOR_MOVE:
		{
			le_t *le;
			int number, i;
			int time = 0;
			int pathLength;
			byte crouchingState;
			pos3_t pos, oldPos;

			number = NET_ReadShort(msg);
			/* get le */
			le = LE_Get(number);
			if (!le)
				LE_NotFoundError(number);

			pathLength = NET_ReadByte(msg);

			/* Also skip the final position */
			NET_ReadByte(msg);
			NET_ReadByte(msg);
			NET_ReadByte(msg);

			VectorCopy(le->pos, pos);
			crouchingState = LE_IsCrouched(le) ? 1 : 0;

			for (i = 0; i < pathLength; i++) {
				const byte fulldv = NET_ReadByte(msg);
				const byte dir = getDVdir(fulldv);
				VectorCopy(pos, oldPos);
				PosAddDV(pos, crouchingState, fulldv);
				time += LE_ActorGetStepTime(le, pos, oldPos, dir, NET_ReadShort(msg));
				NET_ReadShort(msg);
			}
			nextTime += time + 400;
		}
		break;
	case EV_ACTOR_SHOOT:
		{
			const fireDef_t	*fd;
			int flags, dummy;
			int objIdx, surfaceFlags;
			objDef_t *obj;
			int weap_fds_idx, fd_idx;
			shoot_types_t shootType;
			vec3_t muzzle, impact;

			/* read data */
			NET_ReadFormat(msg, eventData->formatString, &dummy, &dummy, &dummy, &objIdx, &weap_fds_idx, &fd_idx, &shootType, &flags, &surfaceFlags, &muzzle, &impact, &dummy);

			obj = INVSH_GetItemByIDX(objIdx);
			fd = FIRESH_GetFiredef(obj, weap_fds_idx, fd_idx);

			if (!(flags & SF_BOUNCED)) {
				/* shooting */
				if (fd->speed && !CL_OutsideMap(impact, UNIT_SIZE * 10)) {
					impactTime = shootTime + 1000 * VectorDist(muzzle, impact) / fd->speed;
				} else {
					impactTime = shootTime;
				}
				if (cl.actTeam != cls.team)
					nextTime = impactTime + 1400;
				else
					nextTime = impactTime + 400;
				if (fd->delayBetweenShots)
					shootTime += 1000 / fd->delayBetweenShots;
			} else {
				/* only a bounced shot */
				eventTime = impactTime;
				if (fd->speed) {
					impactTime += 1000 * VectorDist(muzzle, impact) / fd->speed;
					nextTime = impactTime;
				}
			}
			parsedDeath = qfalse;
		}
		break;
	case EV_ACTOR_THROW:
		nextTime += NET_ReadShort(msg);
		impactTime = shootTime = nextTime;
		parsedDeath = qfalse;
		break;
	default:
		break;
	}

	Com_DPrintf(DEBUG_EVENTSYS, "%s => eventTime: %i, nextTime: %i, impactTime: %i, shootTime: %i\n",
			eventData->name, eventTime, nextTime, impactTime, shootTime);

	return eventTime;
#else
	if (!eventData->timeCallback)
		return cl.time;

	return eventData->timeCallback(eventData, msg, dt);
#endif
}
