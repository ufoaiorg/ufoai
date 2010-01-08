/**
 * @file e_event_endroundannounce.c
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
#include "../../../../cl_game.h"
#include "../../../cl_parse.h"
#include "../../../cl_localentity.h"
#include "../../../cl_hud.h"
#include "e_event_endroundannounce.h"

/**
 * @brief Announces that a player ends his round
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The message buffer to read from
 * @sa CL_DoEndRound
 * @note event EV_ENDROUNDANNOUNCE
 * @todo Build into hud
 */
void CL_EndRoundAnnounce (const eventRegister_t *self, struct dbuffer * msg)
{
	char buf[128];
	/* get the needed values */
	const int playerNum = NET_ReadByte(msg);
	const int team = NET_ReadByte(msg);
	const char *playerName = CL_PlayerGetName(playerNum);

	/* not needed to announce this for singleplayer games */
	if (!GAME_IsMultiplayer())
		return;

	/* it was our own round */
	if (cl.pnum == playerNum) {
		/* add translated message to chat buffer */
		Com_sprintf(buf, sizeof(buf), _("You've ended your round\n"));
	} else {
		/* add translated message to chat buffer */
		Com_sprintf(buf, sizeof(buf), _("%s ended his round (team %i)\n"), playerName, team);
	}
	HUD_DisplayMessage(buf);
}
