/**
 * @file e_event_actorshoothidden.c
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

#include "../../../../client.h"
#include "e_event_actorshoothidden.h"

int CL_ActorShootHiddenTime (const eventRegister_t *self, struct dbuffer *msg, const int dt)
{
#if 0
	int first;
	int objIdx;
	const objDef_t *obj;
	int weap_fds_idx, fd_idx;

	NET_ReadFormat(msg, self->formatString, &first, &objIdx, &weap_fds_idx, &fd_idx);

	obj = INVSH_GetItemByIDX(objIdx);
	if (first) {
		nextTime += 500;
		impactTime = shootTime = nextTime;
	} else {
		const fireDef_t *fd = FIRESH_GetFiredef(obj, weap_fds_idx, fd_idx);
		/* impact right away - we don't see it at all
		 * bouncing is not needed here, too (we still don't see it) */
		impactTime = shootTime;
		nextTime = shootTime + 1400;
		if (fd->delayBetweenShots)
			shootTime += 1000 / fd->delayBetweenShots;
	}
#else
	return cl.time;
#endif
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
	objDef_t *obj;
	weaponFireDefIndex_t weapFdsIdx;
	fireDefIndex_t fdIdx;

	NET_ReadFormat(msg, self->formatString, &first, &objIdx, &weapFdsIdx, &fdIdx);

	/* get the fire def */
	obj = INVSH_GetItemByIDX(objIdx);
	fd = FIRESH_GetFiredef(obj, weapFdsIdx, fdIdx);

	/* start the sound */
	if ((first || !fd->soundOnce) && fd->fireSound[0])
		S_StartLocalSample(fd->fireSound, SND_VOLUME_WEAPONS);
}
