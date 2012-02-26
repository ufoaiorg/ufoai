/**
 * @file e_event_actorshoothidden.c
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

#include "../../../../client.h"
#include "e_event_actorshoothidden.h"

/**
 * @brief Decides if following events should be delayed
 */
int CL_ActorShootHiddenTime (const eventRegister_t *self, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	int first;
	int objIdx;
	const objDef_t *obj;
	weaponFireDefIndex_t weapFdsIdx;
	fireDefIndex_t fireDefIndex;
	const int eventTime = eventTiming->shootTime;

	NET_ReadFormat(msg, self->formatString, &first, &objIdx, &weapFdsIdx, &fireDefIndex);

	obj = INVSH_GetItemByIDX(objIdx);
	if (first) {
		eventTiming->nextTime += 500;
		eventTiming->impactTime = eventTiming->shootTime = eventTiming->nextTime;
	} else {
		const fireDef_t *fd = FIRESH_GetFiredef(obj, weapFdsIdx, fireDefIndex);
		/* impact right away - we don't see it at all
		 * bouncing is not needed here, too (we still don't see it) */
		eventTiming->impactTime = eventTiming->shootTime;
		eventTiming->nextTime = CL_GetNextTime(self, eventTiming, eventTiming->shootTime + 1400);
		if (fd->delayBetweenShots > 0.0)
			eventTiming->shootTime += 1000 / fd->delayBetweenShots;
	}
	eventTiming->parsedDeath = qfalse;

	return eventTime;
}

/**
 * @brief Shoot with weapon but don't bother with animations - actor is hidden.
 * @sa CL_ActorShoot
 */
void CL_ActorShootHidden (const eventRegister_t *self, struct dbuffer *msg)
{
	const fireDef_t *fd;
	int first;
	int objIdx;
	const objDef_t *obj;
	weaponFireDefIndex_t weapFdsIdx;
	fireDefIndex_t fdIdx;

	NET_ReadFormat(msg, self->formatString, &first, &objIdx, &weapFdsIdx, &fdIdx);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* start the sound */
	if ((first || !fd->soundOnce) && fd->fireSound != NULL)
		S_StartLocalSample(fd->fireSound, SND_VOLUME_WEAPONS);
}
