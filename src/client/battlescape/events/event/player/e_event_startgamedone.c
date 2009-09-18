/**
 * @file e_event_startgamedone.c
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../../../../menu/m_main.h"
#include "../../../cl_localentity.h"
#include "../../../../cl_actor.h"
#include "e_event_startgamedone.h"

/**
 * @brief The server finished sending all init-data to the client. We can now run some client-side initialisation - mostly soldier-related.
 * @sa CL_StartGame
 * @note EV_START_DONE
 * @todo Is there more stuff to initialise when the client is "ready"?
 */
void CL_StartingGameDone (const eventRegister_t *self, struct dbuffer *msg)
{
	int actorIdx;

	/* Set default reaction-firemode (or set already working again one to update TUs) on game-start. */
	for (actorIdx = 0; actorIdx < cl.numTeamList; actorIdx++) {
		le_t *le = cl.teamList[actorIdx];
		if (CL_WorkingFiremode(le, qtrue)) {
			/* Rewrite/-send selected reaction firemode in case reserved-TUs or server is outdated. */
			const character_t *chr = CL_GetActorChr(le);
			if (!chr)
				Com_Error(ERR_DROP, "No character struct assigned to actor");
			CL_SetReactionFiremode(le, chr->RFmode.hand, chr->RFmode.weapon, chr->RFmode.fmIdx);

			/* Reserve Tus for crouching/standing up if player selected this previously. */
			if (chr->reservedTus.reserveCrouch) {
				/** @sa CL_ActorToggleCrouchReservation_f */
				/* Reserve the exact amount for crouching/standing up (if we have enough to do so). */
				if (CL_UsableTUs(le) + CL_ReservedTUs(le, RES_CROUCH) >= TU_CROUCH) {
					CL_ReserveTUs(le, RES_CROUCH, TU_CROUCH);
					MN_ExecuteConfunc("crouch_checkbox_check");
				} else {
					MN_ExecuteConfunc("crouch_checkbox_disable");
				}
			}
		} else {
			CL_SetDefaultReactionFiremode(le, ACTOR_HAND_CHAR_RIGHT);
		}
	}
}
