/**
 * @file e_event_startgame.c
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
#include "../../../cl_view.h"
#include "../../../cl_hud.h"
#include "../../../../cgame/cl_game.h"
#include "../../../cl_localentity.h"
#include "../../../cl_actor.h"
#include "e_event_startgame.h"

/**
 * @brief Activates the map render screen (ca_active)
 * @sa SCR_EndLoadingPlaque
 * @sa G_ClientBegin
 * @note EV_START
 */
void CL_StartGame (const eventRegister_t *self, struct dbuffer *msg)
{
	const int isTeamPlay = NET_ReadByte(msg);

	/* init camera position and angles */
	OBJZERO(cl.cam);
	VectorSet(cl.cam.angles, 60.0, 60.0, 0.0);
	VectorSet(cl.cam.omega, 0.0, 0.0, 0.0);
	cl.cam.zoom = 1.25;
	CL_ViewCalcFieldOfViewX();

	Com_Printf("Starting the game...\n");

	/* make sure selActor is null (after reconnect or server change this is needed) */
	CL_ActorSelect(NULL);

	/* center on first actor */
	cl_worldlevel->modified = qtrue;
	if (cl.numTeamList) {
		const le_t *le = cl.teamList[0];
		CL_ViewCenterAtGridPosition(le->pos);
	}

	/* activate the renderer */
	CL_SetClientState(ca_active);

	GAME_StartBattlescape(isTeamPlay);
}
