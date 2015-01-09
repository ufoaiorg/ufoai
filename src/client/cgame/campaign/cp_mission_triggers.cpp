/**
 * @file
 * @brief Campaign mission triggers
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

#include "../../cl_shared.h"
#include "../../ui/ui_dataids.h" /* TEXT_STANDARD */
#include "cp_campaign.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "cp_event_callbacks.h"

/**
 * @brief Executes console commands after a mission
 *
 * @param[in] mission Pointer to the mission to execute the triggers for
 * @param[in] won Int value that is one when you've won the game, and zero when the game was lost
 * Can execute console commands (triggers) on win and lose
 * This can be used for story dependent missions
 */
void CP_ExecuteMissionTrigger (const mission_t* mission, bool won)
{
	Com_DPrintf(DEBUG_CLIENT, "Execute mission triggers\n");

	if (mission == nullptr)
		return;

	/* we add them only here - and remove them afterwards to prevent cheating */
	CP_CampaignTriggerFunctions(true);

	if (won) {
		if (Q_strvalid(mission->onwin)) {
			Com_DPrintf(DEBUG_CLIENT, "...won - executing '%s'\n", mission->onwin);
			cgi->Cmd_ExecuteString("%s", mission->onwin);
		}
		if (mission->mapDef && Q_strvalid(mission->mapDef->onwin)) {
			Com_DPrintf(DEBUG_CLIENT, "...won - executing '%s'\n", mission->mapDef->onwin);
			cgi->Cmd_ExecuteString("%s", mission->mapDef->onwin);
		}
	} else {
		if (Q_strvalid(mission->onlose)) {
			Com_DPrintf(DEBUG_CLIENT, "...lost - executing '%s'\n", mission->onlose);
			cgi->Cmd_ExecuteString("%s", mission->onlose);
		}
		if (mission->mapDef && Q_strvalid(mission->mapDef->onlose)) {
			Com_DPrintf(DEBUG_CLIENT, "...lost - executing '%s'\n", mission->mapDef->onlose);
			cgi->Cmd_ExecuteString("%s", mission->mapDef->onlose);
		}
	}

	CP_CampaignTriggerFunctions(false);
}
