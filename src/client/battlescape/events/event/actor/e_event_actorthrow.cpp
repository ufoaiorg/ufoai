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
#include "e_event_actorthrow.h"

/**
 * @brief Decides if following events should be delayed
 */
int CL_ActorDoThrowTime (const eventRegister_t* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	const int eventTime = eventTiming->nextTime;

	/* the delay is encoded in the message already */
	eventTiming->nextTime += NET_ReadShort(msg);
	eventTiming->impactTime = eventTiming->shootTime = eventTiming->nextTime;
	eventTiming->parsedDeath = false;

	return eventTime;
}

/**
 * @brief Throw item with actor.
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The netchannel message
 * @sa EV_ACTOR_THROW
 */
void CL_ActorDoThrow (const eventRegister_t* self, dbuffer* msg)
{
	vec3_t muzzle, v0;
	int flags;
	int dtime;
	int objIdx;
	weaponFireDefIndex_t weapFdsIdx;
	fireDefIndex_t fdIdx;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &dtime, &objIdx, &weapFdsIdx, &fdIdx, &flags, &muzzle, &v0);

	/* get the fire def */
	const objDef_t* obj = INVSH_GetItemByIDX(objIdx);
	const fireDef_t* fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* add effect le (local entity) */
	/** @todo add victim support for blood and hurt sounds */
	LE_AddGrenade(fd, flags, muzzle, v0, dtime, nullptr);

	/* start the sound */
	if (fd->fireSound != nullptr && !(flags & SF_BOUNCED)) {
		S_LoadAndPlaySample(fd->fireSound, muzzle, SOUND_ATTN_IDLE, SND_VOLUME_DEFAULT);
	}
}
