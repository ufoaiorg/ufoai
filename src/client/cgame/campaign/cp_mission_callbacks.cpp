/**
 * @file
 * @brief UI callbacks for missions.
 * @note Automission related function prefix: AM_
 * @note Other mission function prefix: MIS_
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

#include "cp_mission_callbacks.h"
#include "../../cl_shared.h"
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_missions.h"
#include "cp_auto_mission.h"

/**
 * @brief Callback function to start automission
 */
static void AM_Go_f (void)
{
	mission_t* mission = GEO_GetSelectedMission();
	missionResults_t* results = &ccs.missionResults;
	battleParam_t* battleParam = &ccs.battleParameters;

	if (!mission) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoGo_f: No update after automission\n");
		return;
	}

	if (GEO_GetMissionAircraft() == nullptr) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No aircraft at target pos\n");
		return;
	}

	if (GEO_GetInterceptorAircraft() == nullptr) {
		Com_Printf("GAME_CP_MissionAutoGo_f: No intercept aircraft given\n");
		return;
	}

	cgi->UI_PopWindow(false);

	if (mission->stage != STAGE_BASE_ATTACK) {
		if (!mission->active) {
			MS_AddNewMessage(_("Notice"), _("Your dropship is not near the landing zone"));
			return;
		} else if (mission->mapDef->storyRelated) {
			Com_DPrintf(DEBUG_CLIENT, "You have to play this mission, because it's story related\n");
			/* ensure, that the automatic button is no longer visible */
			cgi->Cvar_Set("cp_mission_autogo_available", "0");
			return;
		}
	}

	/* start the map */
	CP_CreateBattleParameters(mission, battleParam, GEO_GetMissionAircraft());
	battleParam->retriable = false;

	results->state = LOST;
	AM_Go(mission, GEO_GetInterceptorAircraft(), ccs.curCampaign, battleParam, results);
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
	const mission_t* mission = GEO_GetSelectedMission();

	if (!mission || GEO_GetInterceptorAircraft() == nullptr) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: No update after automission\n");
		return;
	}

	if (mission->mapDef->storyRelated) {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: story related - auto mission is disabled\n");
		cgi->Cvar_Set("cp_mission_autogo_available", "0");
	} else {
		Com_DPrintf(DEBUG_CLIENT, "GAME_CP_MissionAutoCheck_f: auto mission is enabled\n");
		cgi->Cvar_Set("cp_mission_autogo_available", "1");
	}
}

/**
 * @brief Updates mission result menu text with appropriate values
 * @param[in] results Initialized mission results
 */
void MIS_InitResultScreen (const missionResults_t* results)
{
	linkedList_t* list = nullptr;

	/* init result text */
	cgi->UI_ResetData(TEXT_LIST2);

	/* aliens */
	cgi->LIST_AddString(&list, va("%s\t%i", _("Aliens killed"), results->aliensKilled));
	cgi->LIST_AddString(&list, va("%s\t%i", _("Aliens captured"), results->aliensStunned));
	cgi->LIST_AddString(&list, va("%s\t%i", _("Alien survivors"), results->aliensSurvived));
	cgi->LIST_AddString(&list, "");
	/* phalanx */
	cgi->LIST_AddString(&list, va("%s\t%i", _("PHALANX soldiers killed in action"), results->ownKilled));
	/* @todo FIXME right now stunned soldiers are shown as MIA when the game ends in a draw or the
	 * mission is aborted and they are in the rescue zone, and in both cases they aren't MIA they'll,
	 * return safely to base, on the other hand stunned soldiers left behind when aborting are
	 * killed at mission end, so there's never a valid case of soldiers going MIA */
	cgi->LIST_AddString(&list, va("%s\t%i", _("PHALANX soldiers missing in action"), results->ownStunned));
	cgi->LIST_AddString(&list, va("%s\t%i", _("PHALANX friendly fire losses"), results->ownKilledFriendlyFire));
	cgi->LIST_AddString(&list, va("%s\t%i", _("PHALANX survivors"), results->ownSurvived));
	cgi->LIST_AddString(&list, "");

	cgi->LIST_AddString(&list, va("%s\t%i", _("Civilians killed"), results->civiliansKilled));
	cgi->LIST_AddString(&list, va("%s\t%i", _("Civilians killed by friendly fire"), results->civiliansKilledFriendlyFire));
	cgi->LIST_AddString(&list, va("%s\t%i", _("Civilians saved"), results->civiliansSurvived));
	cgi->LIST_AddString(&list, "");

	if (results->itemTypes > 0 && results->itemAmount > 0)
		cgi->LIST_AddString(&list, va("%s\t%i/%i", _("Gathered items (types/all)"), results->itemTypes, results->itemAmount));

	cgi->UI_RegisterLinkedListText(TEXT_LIST2, list);
}

static const cmdList_t missionCallbacks[] = {
	{"cp_missionauto_check", AM_Check_f, "Checks whether this mission can be done automatically"},
	{"cp_mission_autogo", AM_Go_f, "Let the current selection mission be done automatically"},
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Init UI callbacks for missions-subsystem
 */
void MIS_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(missionCallbacks);
}

/**
 * @brief Close UI callbacks for missions-subsystem
 */
void MIS_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(missionCallbacks);
}
