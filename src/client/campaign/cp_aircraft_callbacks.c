/**
 * @file cp_aircraft_callbacks.c
 * @brief Menu related console command callbacks
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#include "../client.h"
#include "../cl_global.h"
#include "../menu/m_popup.h"
#include "cl_map.h"
#include "cp_aircraft_callbacks.h"
#include "cp_aircraft.h"
#include "../cl_team.h" /* for CL_UpdateActorAircraftVar */
#include "cl_mapfightequip.h" /* for AII_GetSlotItems */

/**
 * @brief Script function for AIR_AircraftReturnToBase
 * @note Sends the current aircraft back to homebase (called from aircraft menu).
 * @note This function updates some cvars for current aircraft.
 * @sa GAME_CP_MissionAutoGo_f
 * @sa GAME_CP_Results_f
 */
static void AIM_AircraftReturnToBase_f (void)
{
	if (baseCurrent && baseCurrent->aircraftCurrent) {
		AIR_AircraftReturnToBase(baseCurrent->aircraftCurrent);
		AIR_AircraftSelect(baseCurrent->aircraftCurrent);
	}
}


/**
 * @brief Restores aircraft cvars after going back from aircraft buy menu.
 * @sa BS_MarketAircraftDescription
 * @todo check whether this is needed any longer - this is not called anywhere
 */
static void AIM_ResetAircraftCvars_f (void)
{
	if (!baseCurrent || baseCurrent->numAircraftInBase < 0)
		return;

	/* Maybe we sold displayed aircraft ? */
	if (baseCurrent->numAircraftInBase == 0) {
		/* no more aircraft in base */
		Cbuf_AddText("mn_pop\n");
		return;
	}

	AIR_AircraftSelect(baseCurrent->aircraftCurrent);
}

/**
 * @brief Select an aircraft from a base, by ID
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 * @sa AIM_NextAircraft_f
 */
static void AIM_SelectAircraft_f (void)
{
	base_t *base = baseCurrent;
	int i;

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <aircraftID>\n", Cmd_Argv(0));
		return;
	}

	i = atoi(Cmd_Argv(1));
	if (i < 0 || i >= base->numAircraftInBase)
		return;

	AIR_AircraftSelect(&base->aircraft[i]);
}

/**
 * @brief Switch to next aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_PrevAircraft_f
 */
static void AIM_NextAircraft_f (void)
{
	base_t *base = baseCurrent;

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (!base->aircraftCurrent || base->aircraftCurrent == &base->aircraft[base->numAircraftInBase - 1])
		base->aircraftCurrent = &base->aircraft[0];
	else
		base->aircraftCurrent++;

	AIR_AircraftSelect(base->aircraftCurrent);
}

/**
 * @brief Switch to previous aircraft in base.
 * @sa AIR_AircraftSelect
 * @sa AIM_NextAircraft_f
 */
static void AIM_PrevAircraft_f (void)
{
	base_t *base = baseCurrent;

	if (!base || base->numAircraftInBase <= 0)
		return;

	if (!base->aircraftCurrent || base->aircraftCurrent == &base->aircraft[0])
		base->aircraftCurrent = &base->aircraft[base->numAircraftInBase - 1];
	else
		base->aircraftCurrent--;

	AIR_AircraftSelect(base->aircraftCurrent);
}

/**
 * @brief Starts an aircraft or stops the current mission and let the aircraft idle around.
 */
static void AIM_AircraftStart_f (void)
{
	aircraft_t *aircraft;

	if (!baseCurrent)
		return;

	if (!baseCurrent->aircraftCurrent) {
		Com_DPrintf(DEBUG_CLIENT, "Error - there is no current aircraft in this base\n");
		return;
	}

	/* Aircraft cannot start without Command Centre operational. */
	if (!B_GetBuildingStatus(baseCurrent, B_COMMAND)) {
		MN_Popup(_("Notice"), _("No operational Command Centre in this base.\n\nAircraft can not start.\n"));
		return;
	}

	aircraft = baseCurrent->aircraftCurrent;

	/* Aircraft cannot start without a pilot. */
	if (!aircraft->pilot) {
		MN_Popup(_("Notice"), _("There is no pilot assigned to this aircraft.\n\nAircraft can not start.\n"));
		return;
	}

	if (AIR_IsAircraftInBase(aircraft)) {
		/* reload its ammunition */
		AII_ReloadWeapon(aircraft);
	}
	MN_AddNewMessage(_("Notice"), _("Aircraft started"), qfalse, MSG_STANDARD, NULL);
	aircraft->status = AIR_IDLE;

	MAP_SelectAircraft(aircraft);
	/* Return to geoscape. */
	MN_PopMenu(qfalse);
	MN_PopMenu(qfalse);
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
	if (!AIR_IsAircraftInBase(aircraft)) {
		return SOLDIER_EQUIP_MENU_BUTTON_NO_AIRCRAFT_IN_BASE;
	} else {
		if (E_CountHired(aircraft->homebase, EMPL_SOLDIER) <= 0)
			return SOLDIER_EQUIP_MENU_BUTTON_NO_SOLDIES_AVAILABLE;
		else
			return SOLDIER_EQUIP_MENU_BUTTON_OK;
	}
}

/**
 * @brief Console command binding for AIR_AircraftSelect().
 */
static void AIR_AircraftSelect_f (void)
{
	base_t *base = baseCurrent;
	const menu_t menu = *MN_GetActiveMenu();

	/* calling from console? with no baseCurrent? */
	if (!base || !base->numAircraftInBase
	 || (!B_GetBuildingStatus(base, B_HANGAR) && !B_GetBuildingStatus(base, B_SMALL_HANGAR))) {
		if (!Q_strncmp(menu.name, "aircraft", 8))
			MN_PopMenu(qfalse);
		return;
	}

	AIR_AircraftSelect(base->aircraftCurrent);
	if (!base->aircraftCurrent && !Q_strncmp(menu.name, "aircraft", 8))
		MN_PopMenu(qfalse);
}

/**
 * @brief Sets aircraftCurrent and updates related cvars and menutexts.
 * @param[in] aircraft Pointer to given aircraft that should be selected in the menu.
 */
void AIR_AircraftSelect (aircraft_t* aircraft)
{
	menuNode_t *node = NULL;
	static char aircraftInfo[256];
	base_t *base = NULL;
	int id;

	if (aircraft != NULL)
		base = aircraft->homebase;

	if (!base || !base->numAircraftInBase) {
		MN_MenuTextReset(TEXT_AIRCRAFT_INFO);
		return;
	}

	node = MN_GetNodeFromCurrentMenu("aircraft");

	assert(aircraft);
	assert(aircraft->homebase == base);
	CL_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);

	Cvar_SetValue("mn_equipsoldierstate", CL_EquipSoldierState(aircraft));
	Cvar_Set("mn_aircraftstatus", AIR_AircraftStatusToName(aircraft));
	Cvar_Set("mn_aircraftinbase", AIR_IsAircraftInBase(aircraft) ? "1" : "0");
	Cvar_Set("mn_aircraftname", _(aircraft->name));	/**< @todo Comment on the "+1" part here. */
	if (!aircraft->tech)
		Sys_Error("No technology assigned to aircraft '%s'", aircraft->id);
	Cvar_Set("mn_aircraft_model", aircraft->tech->mdl);

	/* generate aircraft info text */
	Com_sprintf(aircraftInfo, sizeof(aircraftInfo), _("Speed:\t%i km/h\n"),
		CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_SPEED], AIR_STATS_SPEED));
	Q_strcat(aircraftInfo, va(_("Fuel:\t%i/%i\n"), CL_AircraftMenuStatsValues(aircraft->fuel, AIR_STATS_FUELSIZE),
		CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE], AIR_STATS_FUELSIZE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Operational range:\t%i km\n"), CL_AircraftMenuStatsValues(aircraft->stats[AIR_STATS_FUELSIZE] *
		aircraft->stats[AIR_STATS_SPEED], AIR_STATS_OP_RANGE)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Weapons:\t%i on %i\n"), AII_GetSlotItems(AC_ITEM_WEAPON, aircraft), aircraft->maxWeapons), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Armour:\t%i on 1\n"), AII_GetSlotItems(AC_ITEM_SHIELD, aircraft)), sizeof(aircraftInfo));
	Q_strcat(aircraftInfo, va(_("Electronics:\t%i on %i"), AII_GetSlotItems(AC_ITEM_ELECTRONICS, aircraft), aircraft->maxElectronics), sizeof(aircraftInfo));

	mn.menuText[TEXT_AIRCRAFT_INFO] = aircraftInfo;

	/* compute the ID and */
	for (id = 0; id < baseCurrent->numAircraftInBase; id++) {
		if (aircraft == &baseCurrent->aircraft[id])
			break;
	}
	assert(id != baseCurrent->numAircraftInBase);
	Cvar_SetValue("mn_aircraft_id", id);

	/* update the GUI */
	MN_ExecuteConfunc("aircraft_change %i", id);

	baseCurrent->aircraftCurrent = aircraft;
}

/**
 * @brief Update TEXT_AIRCRAFT_LIST with the current base aircraft names
 */
static void AIR_AircraftUpdateList_f ()
{
	static char buffer[1024];
	int i;

	if (!baseCurrent)
		return;

	buffer[0] = '\0';

	for (i = 0; i < baseCurrent->numAircraftInBase; i++) {
		const aircraft_t * aircraft = &baseCurrent->aircraft[i];
		Q_strcat(buffer, aircraft->name, sizeof(buffer));
		Q_strcat(buffer, "\n", sizeof(buffer));
	}

	mn.menuText[TEXT_AIRCRAFT_LIST] = buffer;
}

void AIR_InitCallbacks (void)
{
	/* used nowhere ?*/
	Cmd_AddCommand("mn_reset_air", AIM_ResetAircraftCvars_f, NULL);
	/* menu aircraft */
	Cmd_AddCommand("aircraft_start", AIM_AircraftStart_f, NULL);
	/* menu aircraft_equip, aircraft */
	Cmd_AddCommand("mn_next_aircraft", AIM_NextAircraft_f, NULL);
	Cmd_AddCommand("mn_prev_aircraft", AIM_PrevAircraft_f, NULL);
	Cmd_AddCommand("mn_select_aircraft", AIM_SelectAircraft_f, NULL);
	/* menu aircraft, popup_transferbaselist */
	Cmd_AddCommand("aircraft_return", AIM_AircraftReturnToBase_f, "Sends the current aircraft back to homebase");
	/* menu aircraft_equip, aircraft, buy, hangar destroy popup (B_MarkBuildingDestroy)*/
	Cmd_AddCommand("aircraft_select", AIR_AircraftSelect_f, NULL);
	/* menu aircraft, aircraft_equip, aircraft_soldier */
	Cmd_AddCommand("aircraft_update_list", AIR_AircraftUpdateList_f, NULL);

}

void AIR_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("mn_reset_air");
	Cmd_RemoveCommand("aircraft_start");
	Cmd_RemoveCommand("mn_next_aircraft");
	Cmd_RemoveCommand("mn_prev_aircraft");
	Cmd_RemoveCommand("mn_select_aircraft");
	Cmd_RemoveCommand("aircraft_return");
	Cmd_RemoveCommand("aircraft_select");
	Cmd_RemoveCommand("aircraft_update_list");
}
