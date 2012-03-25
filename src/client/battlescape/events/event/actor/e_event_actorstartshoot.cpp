/**
 * @file e_event_actorstartshoot.c
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
#include "../../../cl_localentity.h"
#include "../../../../renderer/r_mesh_anim.h"
#include "e_event_actorstartshoot.h"

/**
 * @brief Decides if following events should be delayed
 */
int CL_ActorStartShootTime (const eventRegister_t *self, struct dbuffer *msg, eventTiming_t *eventTiming)
{
	const int eventTime = eventTiming->nextTime;

	eventTiming->nextTime += 300;
	eventTiming->shootTime = eventTiming->nextTime;

	return eventTime;
}

/**
 * @brief Starts shooting with actor.
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The netchannel message
 * @sa CL_ActorShootHidden
 * @sa CL_ActorShoot
 * @sa CL_ActorDoShoot
 * @todo Improve detection of left- or right animation.
 * @sa EV_ACTOR_START_SHOOT
 */
void CL_ActorStartShoot (const eventRegister_t *self, struct dbuffer *msg)
{
	le_t *le;
	pos3_t from, target;
	int entnum;
	shoot_types_t shootType;

	NET_ReadFormat(msg, self->formatString, &entnum, &shootType, &from, &target);

	/* shooting actor */
	le = LE_Get(entnum);

	/* center view (if wanted) */
	if (cl.actTeam != cls.team)
		CL_CameraRoute(from, target);

	/* actor dependent stuff following */
	if (!le)
		/* it's OK, the actor is not visible */
		return;

	if (!LE_IsLivingActor(le) || LE_IsStunned(le)) {
		Com_Printf("CL_ActorStartShoot: LE (%i) not a living actor (type: %i)\n", entnum, le->type);
		return;
	}

	/* ET_ACTORHIDDEN actors don't have a model yet */
	if (le->type == ET_ACTORHIDDEN)
		return;

	/* Animate - we have to check if it is right or left weapon usage. */
	if (IS_SHOT_RIGHT(shootType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->right, le->left, le->state));
	} else if (IS_SHOT_LEFT(shootType)) {
		R_AnimChange(&le->as, le->model1, LE_GetAnim("move", le->left, le->right, le->state));
	} else if (!IS_SHOT_HEADGEAR(shootType)) {
		/* no animation for headgear (yet) */
		Com_Error(ERR_DROP, "CL_ActorStartShoot: Invalid shootType given (entnum: %i, shootType: %i).\n", shootType, entnum);
	}
}
