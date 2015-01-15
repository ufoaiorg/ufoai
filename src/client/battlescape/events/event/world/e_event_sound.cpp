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
#include "../../../cl_actor.h"
#include "e_event_sound.h"

int CL_SoundEventTime (const struct eventRegister_s* self, dbuffer* msg, eventTiming_t* eventTiming)
{
	char sound[MAX_QPATH];
	vec3_t origin;
	int number;
	int step;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &number, &origin, &step, &sound, sizeof(sound));

	const le_t* le = LE_Get(number);
	if (!le)
		LE_NotFoundError(number);
	if (step >= MAX_ROUTE)
		return eventTiming->nextTime;
	const int stepTime = CL_GetStepTime(eventTiming, le, step);
	if (eventTiming->shootTime > stepTime)
		return eventTiming->impactTime;
	return stepTime;
}

/**
 * @brief Play a sound on the client side
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg holds the network data
 * @sa EV_SOUND
 * @note if the last character of the sound file string that was sent by the
 * server is a '+', we will select a random sound file by replacing the '+'
 * character with a number between 01..99
 */
void CL_SoundEvent (const eventRegister_t* self, dbuffer* msg)
{
	char sound[MAX_QPATH];
	vec3_t origin;
	int number;
	int step;

	/* read data */
	NET_ReadFormat(msg, self->formatString, &number, &origin, &step, &sound, sizeof(sound));

	le_t* le = LE_Get(number);
	if (le) {
		if (LE_IsLivingActor(le) && le->team != cls.team) {
			/** @todo render */
		} else if (LE_IsDoor(le) || LE_IsBreakable(le)) {
			/** @todo render */
		}
	}

	const char* file = CL_ConvertSoundFromEvent(sound, sizeof(sound));
	Com_DPrintf(DEBUG_SOUND, "Play network sample %s at (%f:%f:%f)\n", file, origin[0], origin[1], origin[2]);
	if (step >= 0 && step < MAX_ROUTE) {
		le_t* closest = CL_ActorGetClosest(origin, cls.team);
		if (closest != nullptr) {
			vec3_t tmp;
			VectorCopy(cl.cam.camorg, tmp);
			VectorCopy(closest->origin, cl.cam.camorg);
			S_LoadAndPlaySample(file, origin, SOUND_ATTN_NORM, SND_VOLUME_DEFAULT);
			VectorCopy(tmp, cl.cam.camorg);
		}
		return;
	}
	S_LoadAndPlaySample(file, origin, SOUND_ATTN_NORM, SND_VOLUME_DEFAULT);
}
