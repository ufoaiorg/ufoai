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

#include "../../../../client.h"
#include "../../../cl_localentity.h"
#include "e_event_actormove.h"

/**
 * @brief Decides if following events should be delayed. The delay is the amount of time the actor needs to walk
 * from the start to the end pos.
 */
int CL_ActorDoMoveTime (const eventRegister_t* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	int time = 0;

	const int eventTime = eventTiming->nextTime;
	const int number = NET_ReadShort(msg);
	/* get le */
	le_t* le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);

	pos3_t pos;
	VectorCopy(le->pos, pos);
	byte crouchingState = LE_IsCrouched(le) ? 1 : 0;

	leStep_t* newStep = Mem_AllocType(leStep_t);
	if (le->stepList == nullptr) {
		le->stepList = newStep;
		le->stepIndex = 0;
	} else {
		/* append to the list */
		leStep_t* step = le->stepList;
		while (step) {
			if (step->next == nullptr) {
				step->next = newStep;
				le->stepIndex++;
				break;
			}
			step = step->next;
		}
	}

	/* the end of this event is marked with a 0 */
	while (NET_PeekLong(msg) != 0) {
		newStep->steps = NET_ReadByte(msg);
		if (newStep->steps >= MAX_ROUTE || newStep->steps < 0)
			Com_Error(ERR_DROP, "CL_ActorDoMoveTime: Invalid step number: %i", newStep->steps);
		const dvec_t dvec = NET_ReadShort(msg);
		const byte dir = getDVdir(dvec);
		pos3_t oldPos;
		VectorCopy(pos, oldPos);
		PosAddDV(pos, crouchingState, dvec);
		const int stepTime = LE_ActorGetStepTime(le, pos, oldPos, dir, NET_ReadShort(msg));
		newStep->stepTimes[newStep->steps] = stepTime;
		time += stepTime;
		NET_ReadShort(msg);
	}

	/* skip the end of move marker */
	NET_ReadLong(msg);

	/* Also skip the final position */
	NET_ReadByte(msg);
	NET_ReadByte(msg);
	NET_ReadByte(msg);

	assert(NET_PeekByte(msg) == EV_NULL);

	eventTiming->nextTime += time + 400;
	newStep->lastMoveTime = eventTime;
	newStep->lastMoveDuration = time;
	return eventTime;
}

/**
 * @brief Moves actor.
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The netchannel message
 * @sa LET_PathMove
 * @note EV_ACTOR_MOVE
 */
void CL_ActorDoMove (const eventRegister_t* self, dbuffer* msg)
{
	const int number = NET_ReadShort(msg);

	/* get le */
	le_t* le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);

	if (!LE_IsActor(le))
		Com_Error(ERR_DROP, "Can't move, LE doesn't exist or is not an actor (entnum: %i, type: %i)\n",
			number, le->type);

	if (LE_IsDead(le))
		Com_Error(ERR_DROP, "Can't move, actor on team %i dead (entnum: %i)", le->team, number);

	/* lock this le for other events, the corresponding unlock is in LE_DoEndPathMove() */
	LE_Lock(le);
	if (le->isMoving()) {
		if (le->pathLength == le->pathPos) {
			LE_DoEndPathMove(le);
		} else {
			Com_Error(ERR_DROP, "Actor (entnum: %i) on team %i is still moving (%i steps left).  Times: %i, %i, %i",
					le->entnum, le->team, le->pathLength - le->pathPos, le->startTime, le->endTime, cl.time);
		}
	}

	int i = 0;
	/* the end of this event is marked with a 0 */
	while (NET_PeekLong(msg) != 0) {
		NET_ReadByte(msg);
		le->dvtab[i] = NET_ReadShort(msg); /** Don't adjust dv values here- the whole thing is needed to move the actor! */
		le->speed[i] = NET_ReadShort(msg);
		le->pathContents[i] = NET_ReadShort(msg);
		i++;
	}
	le->pathLength = i;

	if (le->pathLength > MAX_ROUTE)
		Com_Error(ERR_DROP, "Overflow in pathLength (entnum: %i)", number);

	/* skip the end of move marker */
	NET_ReadLong(msg);
	/* Also get the final position */
	NET_ReadGPos(msg, le->newPos);

	if (VectorCompare(le->newPos, le->pos))
		Com_Error(ERR_DROP, "start and end pos are the same (entnum: %i)", number);

	/* activate PathMove function */
	le->resetFloor();
	if (LE_IsInvisible(le))
		/* Hack: this relies on the visibility events interrupting the EV_ACTOR_MOVE event */
		LE_SetThink(le, LET_HiddenMove);
	else
		LE_SetThink(le, LET_StartPathMove);
	le->pathPos = 0;
	le->startTime = cl.time;
	le->endTime = cl.time;
}
