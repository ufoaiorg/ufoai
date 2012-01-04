/**
 * @file e_event_reset.c
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
#include "../../../../ui/ui_main.h"
#include "../../../../cgame/cl_game.h"
#include "../../../cl_localentity.h"
#include "../../../cl_actor.h" /* CL_ActorSelect */
#include "e_event_reset.h"

/**
 * @sa G_ClientStartMatch
 * @sa EV_RESET
 */
void CL_Reset (const eventRegister_t *self, struct dbuffer *msg)
{
	CL_ActorSelect(NULL);
	cl.numTeamList = 0;

	/* set the active player */
	NET_ReadFormat(msg, self->formatString, &cls.team, &cl.actTeam);

	Com_Printf("(player %i) It's team %i's turn!\n", cl.pnum, cl.actTeam);

	CL_CompleteRecalcRouting();

	UI_ExecuteConfunc("disable_rescuezone");

	if (cls.team == cl.actTeam)
		UI_ExecuteConfunc("startround");
	else
		Com_Printf("You lost the coin-toss for first-turn.\n");
}
