/**
 * @file e_event_doendround.c
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
#include "../../../cl_localentity.h"
#include "../../../cl_actor.h"
#include "../../../cl_particle.h"
#include "../../../cl_hud.h"
#include "e_event_doendround.h"

/**
 * @brief Performs end-of-turn processing.
 * @param[in] self Pointer to the event structure that is currently executed
 * @param[in] msg The netchannel message
 * @sa CL_EndRoundAnnounce
 */
void CL_DoEndRound (const eventRegister_t *self, struct dbuffer *msg)
{
	/* hud changes */
	if (cls.team == cl.actTeam)
		UI_ExecuteConfunc("endround");

	refdef.rendererFlags &= ~RDF_IRGOGGLES;

	/* change active player */
	Com_Printf("Team %i ended round\n", cl.actTeam);
	cl.actTeam = NET_ReadByte(msg);
	Com_Printf("Team %i's round started!\n", cl.actTeam);

	/* hud changes */
	if (cls.team == cl.actTeam) {
		/* check whether a particle has to go */
		CL_ParticleCheckRounds();
		UI_ExecuteConfunc("startround");
		HUD_DisplayMessage(_("Your round started!\n"));
		S_StartLocalSample("misc/roundstart", SND_VOLUME_DEFAULT);
		CL_ActorConditionalMoveCalc(selActor);
	}
}
