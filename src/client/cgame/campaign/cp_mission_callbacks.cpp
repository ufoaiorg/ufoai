/**
 * @file cp_mission_callbacks.c
 * @brief UI callbacks for missions.
 * @note Automission related function prefix: AM_
 * @note Other mission function prefix: MIS_
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

#include "cp_mission_callbacks.h"
#include "../../cl_shared.h"
#include "../../ui/ui_windows.h"
#include "../../ui/ui_data.h"
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
 * @brief Updates mission result menu text with appropriate values
 * @param[in] missionResults Initialized mission results
 */
void MIS_InitResultScreen (const missionResults_t *results)
{
	linkedList_t *list = NULL;

	/* init result text */
	UI_ResetData(TEXT_LIST2);

	/* aliens */
	LIST_AddString(&list, va("%s\t%i", _("Aliens killed"), results->aliensKilled));
	LIST_AddString(&list, va("%s\t%i", _("Aliens captured"), results->aliensStunned));
	LIST_AddString(&list, va("%s\t%i", _("Alien survivors"), results->aliensSurvived));
	LIST_AddString(&list, "");
	/* pahanx */
	LIST_AddString(&list, va("%s\t%i", _("PHALANX soldiers killed by Aliens"), results->ownKilled));
	LIST_AddString(&list, va("%s\t%i", _("PHALANX soldiers missing in action"), results->ownStunned));
	LIST_AddString(&list, va("%s\t%i", _("PHALANX friendly fire losses"), results->ownKilledFriendlyFire));
	LIST_AddString(&list, va("%s\t%i", _("PHALANX survivors"), results->ownSurvived));
	LIST_AddString(&list, "");

	LIST_AddString(&list, va("%s\t%i", _("Civilians killed by Aliens"), results->civiliansKilled));
	LIST_AddString(&list, va("%s\t%i", _("Civilians killed by friendly fire"), results->civiliansKilledFriendlyFire));
	LIST_AddString(&list, va("%s\t%i", _("Civilians saved"), results->civiliansSurvived));
	LIST_AddString(&list, "");

	if (results->itemTypes > 0 && results->itemAmount > 0)
		LIST_AddString(&list, va("%s\t%i/%i", _("Gathered items (types/all)"), results->itemTypes, results->itemAmount));

	UI_RegisterLinkedListText(TEXT_LIST2, list);
}

/**
 * @brief Init UI callbacks for missions-subsystem
 */
void MIS_InitCallbacks (void)
{
	Cmd_AddCommand("cp_missionauto_check", AM_Check_f, "Checks whether this mission can be done automatically");
	Cmd_AddCommand("cp_mission_autogo", AM_Go_f, "Let the current selection mission be done automatically");
}

/**
 * @brief Close UI callbacks for missions-subsystem
 */
void MIS_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("cp_missionauto_check");
	Cmd_RemoveCommand("cp_mission_autogo");
}
