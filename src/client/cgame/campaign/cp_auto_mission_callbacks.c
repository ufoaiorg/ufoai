/**
 * @file cp_auto_mission_callbacks.c
 * @brief UI callbacks for automissions subsystem.
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

#include "cp_auto_mission_callbacks.h"
#include "../../cl_shared.h"
#include "../../ui/ui_windows.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_missions.h"
#include "cp_auto_mission.h"

/**
 * @brief Callback function to start automission
 */
static void AM_Go_f (void)
{
	mission_t *mission = MAP_GetSelectedMission();
	missionResults_t *results = &ccs.missionResults;
	battleParam_t *battleParam = &ccs.battleParameters;

	if (!mission) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoGo_f: No update after automission\n");
		return;
	}

	if (MAP_GetMissionAircraft() == NULL) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No aircraft at target pos\n");
		return;
	}

	if (MAP_GetInterceptorAircraft() == NULL) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No intercept aircraft given\n");
		return;
	}

	UI_PopWindow(qfalse);

	if (mission->stage != STAGE_BASE_ATTACK) {
		if (!mission->active) {
			MS_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"), qfalse, MSG_STANDARD, NULL);
			return;
		} else if (mission->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			Cvar_Set("cp_mission_autogo_available", "0");
			return;
		}
	}

	/* start the map */
	CP_CreateBattleParameters(mission, battleParam, MAP_GetMissionAircraft());

	results->won = qfalse;
	AM_Go(mission, MAP_GetInterceptorAircraft(), ccs.curCampaign, battleParam, results);
}

/**
 * @brief Checks whether you have to play this mission or can be done via automission
 * You can mark a mission as story related.
 * If a mission is story related the cvar @c cp_mission_autogo_available is set to @c 0
 * If this cvar is @c 1 - the mission dialog will have a auto mission button
 * @sa AM_Go_f
 */
static void AM_Check_f (void)
{
	const mission_t *mission = MAP_GetSelectedMission();

	if (!mission || MAP_GetInterceptorAircraft() == NULL) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: No update after automission\n");
		return;
	}

	if (mission->mapDef->storyRelated) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: story related - auto mission is disabled\n");
		Cvar_Set("cp_mission_autogo_available", "0");
	} else {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: auto mission is enabled\n");
		Cvar_Set("cp_mission_autogo_available", "1");
	}
}

/**
 * @brief Init UI callbacks for automission-subsystem
 */
void AM_InitCallbacks (void)
{
	Cmd_AddCommand("cp_missionauto_check", AM_Check_f, "Checks whether this mission can be done automatically");
	Cmd_AddCommand("cp_mission_autogo", AM_Go_f, "Let the current selection mission be done automatically");
}

/**
 * @brief Close UI callbacks for automission-subsystem
 */
void AM_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("cp_missionauto_check");
	Cmd_RemoveCommand("cp_mission_autogo");
}
