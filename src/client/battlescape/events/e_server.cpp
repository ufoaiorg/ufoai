/**
 * @file e_server.c
 * @brief Events that are send from the client to the server
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

#include "e_server.h"
#include "../../client.h"
#include "../cl_hud.h"
/**
 * @brief Finishes the current turn of the player in battlescape and starts the turn for the next team.
 */
static void CL_NextRound_f (void)
{
	struct dbuffer *msg;
	/* can't end round if we are not in battlescape */
	if (!CL_BattlescapeRunning())
		return;

	/* can't end round if we're not active */
	if (cls.team != cl.actTeam) {
		HUD_DisplayMessage(_("It is not your turn!"));
		return;
	}

	/* send endround */
	msg = new_dbuffer();
	NET_WriteByte(msg, clc_endround);
	NET_WriteMsg(cls.netStream, msg);
}

void CL_ServerEventsInit (void)
{
	Cmd_AddCommand("nextround", CL_NextRound_f, N_("End current turn."));
}
