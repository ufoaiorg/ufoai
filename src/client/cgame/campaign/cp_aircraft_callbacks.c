/**
 * @file cp_aircraft_callbacks.c
 * @brief Menu related console command callbacks
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

#include "../../cl_shared.h"
#include "../../ui/ui_main.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_aircraft_callbacks.h"
#include "cp_aircraft.h"
#include "cp_team.h"
#include "cp_mapfightequip.h"
#include "cp_popup.h"

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase (called from aircraft menu).
 * @note This function updates some cvars for current aircraft.
 * @sa GAME_CP_MissionAutoGo_f
 * @sa GAME_CP_Results_f
 */
static void AIM_AircraftReturnToBase_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (base && base->aircraftCurrent) {
		AIR_AircraftReturnToBase(base->aircraftCurrent);
		AIR_AircraftSelect(base->aircraftCurrent);
	}
}

/**
 * @brief Select an aircraft from a base, by ID
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 * @sa AIM_NextAircraft_f
 */
static void AIM_SelectAircraft_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (Cmd_Argc() < 2) {
		if (base->aircraftCurrent)
			AIR_AircraftSelect(base->aircraftCurrent);
	} else {
		const int i = atoi(Cmd_Argv(1));
		aircraft_t *aircraft = AIR_GetAircraftFromBaseByIDXSafe(base, i);
		if (aircraft != NULL)
			AIR_AircraftSelect(aircraft);
	}
}

/**
 * @brief Starts an aircraft or stops the current mission and let the aircraft idle around.
 */
static void AIM_AircraftStart_f (void)
{
	aircraft_t *aircraft;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (!base->aircraftCurrent) {
		Com_DPrintf(DEBUG_CLIENT, "Error - there is no current aircraft in this base\n");
		return;
	}

	/* Aircraft cannot start without Command Centre operational. */
	if (!B_GetBuildingStatus(base, B_COMMAND)) {
		CP_Popup(_("Notice"), _("No operational Command Centre in this base.\n\nAircraft can not start.\n"));
		return;
	}

	aircraft = base->aircraftCurrent;

	/* Aircraft cannot start without a pilot. */
	if (!AIR_GetPilot(aircraft)) {
		CP_Popup(_("Notice"), _("There is no pilot assigned to this aircraft.\n\nAircraft can not start.\n"));
		return;
	}

	if (AIR_IsAircraftInBase(aircraft)) {
		/* reload its ammunition */
		AII_ReloadAircraftWeapons(aircraft);
	}
	MS_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	aircraft->status = AIR_IDLE;

	MAP_SelectAircraft(aircraft);
	/* Return to geoscape. */
	UI_PopWindow(qfalse);
	UI_PopWindow(qfalse);
}

#define SOLDIER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE 1
#define SOLDIER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE 2
#define SOLDIER_EQUIP_MENU_BUTTON_OK 3
/**
 * @brief Determines the state of the equip soldier menu button:
 * @returns SOLIDER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE if no aircraft in base
 * @returns SOLIDER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE if no soldiers available
 * @returns SOLIDER_EQUIP_MENU_BUTTON_OK if none of the above is true
 */
static int CL_EquipSoldierState (const aircraft_t * aircraft)
{
	if (!AIR_IsAircraftInBase(aircraft))
		return SOLDIER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE;

	if (E_CountHired(aircraft->homebase, EMPL_SOLDIER) <= 0)
		return SOLDIER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE;

	return SOLDIER_EQUIP_MENU_BUTTON_OK;
}

/**
 * @brief Returns the amount of assigned items for a given slot of a given aircraft
 * @param[in] type This is the slot type to get the amount of assigned items for
 * @param[in] aircraft The aircraft to count the items for (may not be NULL)
 * @return The amount of assigned items for the given slot
 */
static int AIR_GetSlotItems (aircraftItemType_t type, const aircraft_t *aircraft)
{
	int i, max, cnt = 0;
	const aircraftSlot_t *slot;

	assert(aircraft);

	switch (type) {
	case AC_ITEM_SHIELD:
		if (aircraft->shield.item)
			return 1;
		else
			return 0;
		break;
	case AC_ITEM_WEAPON:
		slot = aircraft->weapons;
		max = MAX_AIRCRAFTSLOT;
		break;
	case AC_ITEM_ELECTRONICS:
		slot = aircraft->electronics;
		max = MAX_AIRCRAFTSLOT;
		break;
	default:
		Com_Printf("AIR_GetSlotItems: Unknown type of slot : %i", type);
		return 0;
	}

	for (i = 0; i < max; i++)
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
	base_t *base;
	int id;

	if (aircraft != NULL)
		base = aircraft->homebase;
	else
		base = NULL;

	if (!AIR_BaseHasAircraft(base)) {
		UI_ResetData(TEXT_AIRCRAFT_INFO);
		return;
	}

	assert(aircraft);
	assert(aircraft->homebase == base);
	CP_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);

	Cvar_SetValue("mn_equipsoldierstate", CL_EquipSoldierState(aircraft));
	Cvar_Set("mn_aircraftstatus", AIR_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftinbase", AIR_IsAircraftInBase(aircraft) ? "1" : "0");
	Cvar_Set("mn_aircraftname", aircraft->name);
	if (!aircraft->tech)
		Com_Error(ERR_DROP, "No technology assigned to aircraft '%s'", aircraft->id);
	Cvar_Set("mn_aircraft_model", aircraft->tech->mdl);
	Cvar_Set("mn_aircraft_health", va("%3.0f" , aircraft->stats[AIR_STATS_DAMAGE] > 0 ? (double)aircraft->damage * 100 / aircraft->stats[AIR_STATS_DAMAGE] : 0));

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%i km/h\n"),
		AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), AIR_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
		AIR_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Operational range:\t%i km\n"), AIR_GetOperationRange(aircraft)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapons:\t%i on %i\n"), AIR_GetSlotItems(AC_ITEM_WEAPON, aircraft), aircraft->maxWeapons), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Armour:\t%i on 1\n"), AIR_GetSlotItems(AC_ITEM_SHIELD, aircraft)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Electronics:\t%i on %i"), AIR_GetSlotItems(AC_ITEM_ELECTRONICS, aircraft), aircraft->maxElectronics), sizeof(aircraftInfo));

	UI_RegisterText(TEXT_AIRCRAFT_INFO, aircraftInfo);

	/** @todo This shouldn't exists. UI should use the global idx as reference */
	/* compute the ID and... */
	id = 0;
	AIR_ForeachFromBase(aircraftInBase, base) {
		if (aircraft == aircraftInBase)
			break;
		id++;
	}

	base->aircraftCurrent = aircraft;
	Cvar_SetValue("mn_aircraft_id", id);

	/* ...update the GUI */
	UI_ExecuteConfunc("aircraft_change %i", id);
}

/**
 * @brief Update TEXT_AIRCRAFT_LIST with the current base aircraft names
 */
static void AIR_AircraftUpdateList_f (void)
{
	linkedList_t *list = NULL;
	base_t *base = B_GetCurrentSelectedBase();

	AIR_ForeachFromBase(aircraft, base) {
		LIST_AddString(&list, aircraft->name);
	}

	UI_RegisterLinkedListText(TEXT_AIRCRAFT_LIST, list);
}

/**
 * @brief Creates console command to change the name of an aircraft.
 * Copies the value of the cvar mn_aircraftname over as the name of the
 * current selected aircraft
 */
static void AIR_ChangeAircraftName_f (void)
{
	const base_t *base = B_GetCurrentSelectedBase();
	aircraft_t *aircraft;

	if (!base)
		return;
	aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	Q_strncpyz(aircraft->name, Cvar_GetString("mn_aircraftname"), sizeof(aircraft->name));
}


void AIR_InitCallbacks (void)
{
	/* menu aircraft */
	Cmd_AddCommand("aircraft_start", AIM_AircraftStart_f, NULL);
	/* menu aircraft_equip, aircraft */
	Cmd_AddCommand("mn_select_aircraft", AIM_SelectAircraft_f, NULL);
	/* menu aircraft, popup_transferbaselist */
	Cmd_AddCommand("aircraft_return", AIM_AircraftReturnToBase_f, "Sends the current aircraft back to homebase");
	/* menu aircraft, aircraft_equip, aircraft_soldier */
	Cmd_AddCommand("aircraft_update_list", AIR_AircraftUpdateList_f, NULL);
	Cmd_AddCommand("aircraft_namechange", AIR_ChangeAircraftName_f, "Callback to change the name of the aircraft.");
}

void AIR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("aircraft_namechange");
	Cmd_RemoveCommand("aircraft_start");
	Cmd_RemoveCommand("mn_select_aircraft");
	Cmd_RemoveCommand("aircraft_return");
	Cmd_RemoveCommand("aircraft_update_list");
}
