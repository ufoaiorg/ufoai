/**
 * @file
 * @brief Menu related console command callbacks
 */

/*
Copyright (C) 2002-2019 UFO: Alien Invasion.

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
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_geoscape.h"
#include "cp_aircraft_callbacks.h"
#include "cp_aircraft.h"
#include "cp_team.h"
#include "cp_mapfightequip.h"
#include "cp_popup.h"
#include "cp_missions.h"

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase (called from aircraft menu).
 * @note This function updates some cvars for current aircraft.
 * @sa GAME_CP_MissionAutoGo_f
 * @sa GAME_CP_Results_f
 */
static void AIM_AircraftReturnToBase_f (void)
{
	aircraft_t* aircraft = nullptr;

	if (cgi->Cmd_Argc() >= 2) {
		const int index = atoi(cgi->Cmd_Argv(1));
		aircraft = AIR_AircraftGetFromIDX(index);
	} else {
		base_t* base = B_GetCurrentSelectedBase();
		if (base && base->aircraftCurrent)
			aircraft = base->aircraftCurrent;
	}
	if (aircraft == nullptr)
		return;

	AIR_AircraftReturnToBase(aircraft);
	AIR_AircraftSelect(aircraft);
}

/**
 * @brief Select an aircraft from a base, by ID
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 * @sa AIM_NextAircraft_f
 */
static void AIM_SelectAircraft_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		if (base->aircraftCurrent)
			AIR_AircraftSelect(base->aircraftCurrent);
	} else {
		const int i = atoi(cgi->Cmd_Argv(1));
		aircraft_t* aircraft = AIR_GetAircraftFromBaseByIDXSafe(base, i);
		if (aircraft != nullptr)
			AIR_AircraftSelect(aircraft);
	}
}

/**
 * @brief Starts an aircraft or stops the current mission and lets the aircraft idle around.
 */
static void AIM_AircraftStart_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (!base->aircraftCurrent) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Error - there is no current aircraft in this base\n");
		return;
	}

	/* Aircraft cannot start without operational Command Centre. */
	if (!B_GetBuildingStatus(base, B_COMMAND)) {
		CP_Popup(_("Notice"), _("No operational Command Centre in this base.\n\nAircraft can not start.\n"));
		return;
	}

	aircraft_t* aircraft = base->aircraftCurrent;

	/* Aircraft cannot start without a pilot. */
	if (!AIR_GetPilot(aircraft)) {
		CP_Popup(_("Notice"), _("There is no pilot assigned to this aircraft.\n\nAircraft can not start.\n"));
		return;
	}

	if (AIR_IsAircraftInBase(aircraft)) {
		/* reload its ammunition */
		AII_ReloadAircraftWeapons(aircraft);
	}
	MS_AddNewMessage(_("Notice"), _("Aircraft started"));
	aircraft->status = AIR_IDLE;

	GEO_SelectAircraft(aircraft);
	/* Return to geoscape. */
	cgi->UI_PopWindow(false);
	cgi->UI_PopWindow(false);
}

/**
 * @brief Returns the amount of assigned items for a given slot of a given aircraft
 * @param[in] type This is the slot type to get the amount of assigned items for
 * @param[in] aircraft The aircraft to count the items for (may not be nullptr)
 * @return The amount of assigned items for the given slot
 */
static int AIR_GetSlotItems (aircraftItemType_t type, const aircraft_t* aircraft)
{
	int max;
	const aircraftSlot_t* slot;

	assert(aircraft);

	switch (type) {
	case AC_ITEM_SHIELD:
		if (aircraft->shield.item)
			return 1;
		return 0;
	case AC_ITEM_WEAPON:
		slot = aircraft->weapons;
		max = MAX_AIRCRAFTSLOT;
		break;
	case AC_ITEM_ELECTRONICS:
		slot = aircraft->electronics;
		max = MAX_AIRCRAFTSLOT;
		break;
	default:
		cgi->Com_Printf("AIR_GetSlotItems: Unknown type of slot : %i", type);
		return 0;
	}

	int cnt = 0;
	for (int i = 0; i < max; i++)
		if (slot[i].item)
			cnt++;

	return cnt;
}

/**
 * @brief Sets aircraftCurrent and updates related cvars and menutexts.
 * @param[in] aircraft Pointer to given aircraft that should be selected in the menu.
 */
void AIR_AircraftSelect (aircraft_t* aircraft)
{
	static char aircraftInfo[256];
	base_t* base;

	if (aircraft != nullptr)
		base = aircraft->homebase;
	else
		base = nullptr;

	if (!AIR_BaseHasAircraft(base)) {
		cgi->UI_ResetData(TEXT_AIRCRAFT_INFO);
		return;
	}

	assert(aircraft);
	assert(aircraft->homebase == base);
	CP_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);

	cgi->Cvar_Set("mn_aircraftinbase", "%s", AIR_IsAircraftInBase(aircraft) ? "1" : "0");
	cgi->Cvar_Set("mn_aircraftname", "%s", aircraft->name);
	if (!aircraft->tech)
		cgi->Com_Error(ERR_DROP, "No technology assigned to aircraft '%s'", aircraft->id);
	cgi->Cvar_Set("mn_aircraft_model", "%s", aircraft->tech->mdl);
	cgi->Cvar_Set("mn_aircraft_health", "%3.0f" , aircraft->stats[AIR_STATS_DAMAGE] > 0 ? (double)aircraft->damage * 100 / aircraft->stats[AIR_STATS_DAMAGE] : 0);

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%i km/h\n"),
		AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
	Q_strcat(aircraftInfo, sizeof(aircraftInfo), _("Fuel:\t%i/%i\n"), AIR_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
		AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE));
	Q_strcat(aircraftInfo, sizeof(aircraftInfo), _("Operational range:\t%i km\n"), AIR_GetOperationRange(aircraft));
	Q_strcat(aircraftInfo, sizeof(aircraftInfo), _("Weapons:\t%i of %i\n"), AIR_GetSlotItems(AC_ITEM_WEAPON, aircraft), aircraft->maxWeapons);
	Q_strcat(aircraftInfo, sizeof(aircraftInfo), _("Armour:\t%i of 1\n"), AIR_GetSlotItems(AC_ITEM_SHIELD, aircraft));
	Q_strcat(aircraftInfo, sizeof(aircraftInfo), _("Electronics:\t%i of %i"), AIR_GetSlotItems(AC_ITEM_ELECTRONICS, aircraft), aircraft->maxElectronics);

	cgi->UI_RegisterText(TEXT_AIRCRAFT_INFO, aircraftInfo);

	/** @todo This shouldn't exist. UI should use the global idx as reference */
	/* compute the ID and... */
	int id = 0;
	AIR_ForeachFromBase(aircraftInBase, base) {
		if (aircraft == aircraftInBase)
			break;
		id++;
	}

	base->aircraftCurrent = aircraft;
	cgi->Cvar_SetValue("mn_aircraft_id", id);

	/* ...update the GUI */
	cgi->UI_ExecuteConfunc("ui_aircraft_selected %i", id);
	cgi->UI_ExecuteConfunc("aircraft_change %i", id);
}

/**
 * @brief Update aircraft selection list with the current base aircraft names
 */
static void AIR_AircraftFillList_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	cgi->UI_ExecuteConfunc("ui_aircraft_clear");
	int idx = 0;
	AIR_ForeachFromBase(aircraft, base) {
		const float health = aircraft->stats[AIR_STATS_DAMAGE] > 0 ? (float)aircraft->damage * 100.0f / aircraft->stats[AIR_STATS_DAMAGE] : 0.0f;
		const char* inBase = AIR_IsAircraftInBase(aircraft) ? "1" : "0";
		char teamStr[MAX_VAR];
		Com_sprintf(teamStr, sizeof(teamStr), _("%i of %i"), AIR_GetTeamSize(aircraft), aircraft->maxTeamSize);
		cgi->UI_ExecuteConfunc("ui_aircraft_add %i \"%s\" %3.0f %s \"%s\" \"%s\"", idx, _(aircraft->name), health, inBase, AIR_AircraftStatusToName(aircraft), teamStr);
		idx++;
	}
}

/**
 * @brief Creates console command to change the name of an aircraft.
 * Copies the value of the cvar mn_aircraftname over as the name of the
 * current selected aircraft
 * @note empty name will reset aircraft's name to the default
 * @note renaming to name with only non printable characters (eg. space) is denied
 * @todo make it not using cvar and default aircraft but get them from parameterlist
 */
static void AIR_ChangeAircraftName_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	aircraft_t* aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	/* set default name on empty new name*/
	const char* newName = cgi->Cvar_GetString("mn_aircraftname");
	if (Q_strnull(newName)) {
		Q_strncpyz(aircraft->name, _(aircraft->defaultName), sizeof(aircraft->name));
		return;
	}

	/* refuse to set name contains only non-printable characters */
	int i;
	for (i = 0; newName[i] != '\0'; i++) {
		if (newName[i] > 0x20)
			break;
	}
	if (newName[i] == '\0')
		return;

	/* aircraft name should not contain " */
	if (!Com_IsValidName(newName))
		return;

	Q_strncpyz(aircraft->name, newName, sizeof(aircraft->name));
}

/**
 * @brief Select aircraft on Geoscape
 */
static void AIR_GeoSelectAircraft_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		return;
	}

	const int index = atoi(cgi->Cmd_Argv(1));
	aircraft_t* aircraft = AIR_AircraftGetFromIDX(index);
	if (aircraft == nullptr)
		return;

	if (!GEO_IsAircraftSelected(aircraft))
		GEO_SelectAircraft(aircraft);

	/** @todo temporary action id should be removed */
	int action_id = 0;
	cgi->UI_ExecuteConfunc("ui_clear_aircraft_action");
	cgi->UI_ExecuteConfunc("ui_add_aircraft_action %i \"%s\" \"%s\"",
		action_id++, _("Change homebase"), va("ui_aircraft_changehomebase %u;", aircraft->idx));
	cgi->UI_ExecuteConfunc("ui_add_aircraft_action %i \"%s\" \"%s\"",
		action_id++, va("%s: %s", _("Back to base"), aircraft->homebase->name), va("aircraft_return %u;", aircraft->idx));
	cgi->UI_ExecuteConfunc("ui_add_aircraft_action %i \"%s\" \"%s\"",
		action_id++, _("Stop"), va("ui_aircraft_stop %u;", aircraft->idx));

	if (AIR_GetTeamSize(aircraft) > 0) {
		MIS_Foreach(tempMission) {
			if (AIR_GetTeamSize(aircraft) == 0)
				continue;
			if (tempMission->stage == STAGE_NOT_ACTIVE || !tempMission->onGeoscape)
				continue;
			if (!tempMission->pos)
				continue;
			if (!AIR_AircraftHasEnoughFuel(aircraft, tempMission->pos))
				continue;
			cgi->UI_ExecuteConfunc("ui_add_aircraft_action %i \"%s\" \"%s\"",
				action_id++, va("%s: %s", _("Mission"), MIS_GetName(tempMission)),
				va("ui_aircraft_to_mission %u %u;", aircraft->idx, tempMission->idx));
		}
	}

	cgi->UI_PushWindow("popup_aircraft_actions");
}

/**
 * @brief Stop aircraft on Geoscape
 */
static void AIR_StopAircraft_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		return;
	}

	const int index = atoi(cgi->Cmd_Argv(1));
	aircraft_t* aircraft = AIR_AircraftGetFromIDX(index);
	if (aircraft == nullptr)
		return;

	aircraft->status = AIR_IDLE;
}


/**
 * @brief Show change homebase popup
 * @todo Temporary callback. Need to rewrite change homebase popup also
 */
static void AIR_ShowChangeHomebaseAircraft_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		return;
	}

	const int index = atoi(cgi->Cmd_Argv(1));
	aircraft_t* aircraft = AIR_AircraftGetFromIDX(index);
	if (aircraft == nullptr)
		return;

	CL_DisplayHomebasePopup(aircraft, true);
}

/**
 * @brief Send aircraft to land on a mission
 */
static void AIR_SendAircraftToMission_f (void)
{
	if (cgi->Cmd_Argc() < 3) {
		return;
	}

	const int aircraftIdx = atoi(cgi->Cmd_Argv(1));
	aircraft_t* aircraft = AIR_AircraftGetFromIDX(aircraftIdx);
	if (aircraft == nullptr)
		return;

	const int missionIdx = atoi(cgi->Cmd_Argv(2));
	mission_t* mission = MIS_GetByIdx(missionIdx);
	if (mission == nullptr)
		return;

	AIR_SendAircraftToMission(aircraft, mission);
}

/**
 * @brief Show aircraft in Base sections
 */
static void AIR_ShowAircraft_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <base_idx>\n", cgi->Cmd_Argv(0));
		return;
	}
	const base_t* const base = B_GetFoundedBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	if (base == nullptr) {
		cgi->Com_Printf("AIR_ShowAircraft_f: Invalid base_idx!\n");
		return;
	}
	if (!AIR_AircraftAllowed(base))
		return;
	int idx_in_base = 0;
	AIR_ForeachFromBase(aircraft, base) {
		cgi->UI_ExecuteConfunc("show_aircraft %i \"%s\" \"%s\" \"%s\" %i %i", aircraft->idx, aircraft->name, aircraft->id, AIR_AircraftStatusToName(aircraft), AIR_IsAircraftInBase(aircraft), idx_in_base++);
	}
}

static const cmdList_t aircraftCallbacks[] = {
	{"aircraft_start", AIM_AircraftStart_f, nullptr},
	{"ui_aircraft_select", AIM_SelectAircraft_f, nullptr},
	{"geo_aircraft_select", AIR_GeoSelectAircraft_f, nullptr},
	{"aircraft_return", AIM_AircraftReturnToBase_f, "Sends the current aircraft back to homebase."},
	{"ui_aircraft_changename", AIR_ChangeAircraftName_f, "Callback to change the name of the aircraft."},
	{"ui_aircraft_fill", AIR_AircraftFillList_f, "Send the data for all the aircraft."},
	{"ui_aircraft_stop", AIR_StopAircraft_f, "Clears an aircraft order when on the geoscape"},
	{"ui_aircraft_to_mission", AIR_SendAircraftToMission_f, "Send aircraft to a misison"},
	{"ui_aircraft_changehomebase", AIR_ShowChangeHomebaseAircraft_f, ""},
	{"ui_show_aircraft", AIR_ShowAircraft_f, "Show aircraft in base sections"},
	{nullptr, nullptr, nullptr}
};

void AIR_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(aircraftCallbacks);
}

void AIR_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(aircraftCallbacks);
}
