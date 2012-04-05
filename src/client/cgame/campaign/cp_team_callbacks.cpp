/**
 * @file
 * @brief Menu related callback functions for the team menu
 */

/*
All original material Copyright (C) 2002-2012 UFO: Alien Invasion.

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
#include "../../cl_team.h"
#include "../../cgame/cl_game_team.h"
#include "../../ui/ui_main.h"

#include "cp_campaign.h"
#include "cp_team.h"
#include "cp_team_callbacks.h"
#include "cp_map.h" /* MAP_GetSelectedAircraft */

/**
 * @brief Displays actor equipment and unused items in proper (filter) category.
 * @note This function is called everytime the equipment screen for the team pops up.
 * @todo Do we allow EMPL_ROBOTs to be equipable? Or is simple buying of ammo enough (similar to original UFO/XCOM)?
 * In the first case the EMPL_SOLDIER stuff below needs to be changed.
 * @todo merge with GAME_GetEquipment
 */
static void CP_UpdateEquipmentMenuParameters_f (void)
{
	aircraft_t *aircraft;
	equipDef_t unused;
	base_t *base = B_GetCurrentSelectedBase();
	int i;
	size_t size;

	if (!base)
		return;

	aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	/* no soldiers are assigned to the current aircraft. */
	if (AIR_GetTeamSize(aircraft) == 0) {
		UI_PopWindow(qfalse);
		return;
	}

	Cvar_ForceSet("cl_selected", "0");

	/** @todo Skip EMPL_ROBOT (i.e. ugvs) for now . */
	CP_UpdateActorAircraftVar(aircraft, EMPL_SOLDIER);

	size = lengthof(chrDisplayList.chr);
	for (i = 0; i < size; i++) {
		if (i < chrDisplayList.num)
			UI_ExecuteConfunc("equipenable %i", i);
		else
			UI_ExecuteConfunc("equipdisable %i", i);
	}

	/* clean up aircraft crew for upcoming mission */
	CP_CleanTempInventory(aircraft->homebase);
	unused = aircraft->homebase->storage;

	AIR_Foreach(aircraftInBase) {
		if (aircraftInBase->homebase == base)
			CP_CleanupAircraftCrew(aircraftInBase, &unused);
	}

	GAME_UpdateInventory(&aircraft->homebase->bEquipment, &unused);
}

/**
 * @brief Adds or removes a soldier to/from an aircraft using his/her UCN as reference.
 */
static void CP_TEAM_AssignSoldierByUCN_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	aircraft_t *aircraft;
	int ucn;
	const employeeType_t employeeType = EMPL_SOLDIER;
	employee_t *employee;

	/* check syntax */
	if (Cmd_Argc() < 1 ) {
		Com_Printf("Usage: %s <ucn>\n", Cmd_Argv(0));
		return;
	}

	ucn = atoi(Cmd_Argv(1));
	if (ucn < 0)
		return;

	aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		Com_Error(ERR_DROP, "CP_TEAM_SelectActorByUCN_f: No employee with UCN %i", ucn);

	if (AIR_IsEmployeeInAircraft(employee, aircraft)) {
		AIR_RemoveEmployee(employee, aircraft);
	} else {
		if (employee->type == EMPL_PILOT)
			AIR_SetPilot(aircraft, employee);
		else
			AIR_AddToAircraftTeam(aircraft, employee);
	}

	CP_UpdateActorAircraftVar(aircraft, employeeType);
	Cvar_SetValue("cpteam_size", AIR_GetTeamSize(aircraft));
	UI_ExecuteConfunc("aircraft_status_change");
}

/**
 * @brief Selects a soldier by his/her Unique Character Number on team UI
 */
static void CP_TEAM_SelectActorByUCN_f (void)
{
	employee_t *employee;
	character_t *chr;
	int ucn;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* check syntax */
	if (Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <ucn>\n", Cmd_Argv(0));
		return;
	}

	ucn = atoi(Cmd_Argv(1));
	if (ucn < 0) {
		UI_ExecuteConfunc("reset_character_cvars");
		return;
	}

	employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		Com_Error(ERR_DROP, "CP_TEAM_SelectActorByUCN_f: No employee with UCN %i", ucn);

	chr = &employee->chr;

	/* set info cvars */
	CL_UpdateCharacterValues(chr, "mn_");
}

#ifdef DEBUG
/**
 * @brief Debug function to list the actual team
 */
static void CP_TeamListDebug_f (void)
{
	base_t *base;
	aircraft_t *aircraft;
	linkedList_t *l;

	aircraft = MAP_GetSelectedAircraft();
	if (!aircraft) {
		Com_Printf("Buy/build an aircraft first.\n");
		return;
	}

	base = aircraft->homebase;
	if (!base) {
		Com_Printf("Build and select a base first\n");
		return;
	}

	Com_Printf("%i members in the current team", AIR_GetTeamSize(aircraft));
	for (l = aircraft->acTeam; l != NULL; l = l->next) {
		const employee_t *employee = (const employee_t *)l->data;
		Com_Printf("ucn %i - name: %s\n", employee->chr.ucn, employee->chr.name);
	}
}
#endif

/**
 * @brief Fill the employee list for Soldier/Pilot assignment
 */
static void CP_TEAM_FillEmployeeList_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	aircraft_t *aircraft = base->aircraftCurrent;
	employeeType_t employeeType;
	char typeId[MAX_VAR];

	if (Cmd_Argc() <= 1 ) {
		Com_Printf("Usage: %s <soldier|pilot> [aircraftIDX]\n", Cmd_Argv(0));
		return;
	}
	Q_strncpyz(typeId, Cmd_Argv(1), lengthof(typeId));
	employeeType = E_GetEmployeeType(typeId);

	if (employeeType == MAX_EMPL) {
		Com_Printf("Invalid employeeType: %s\n", typeId);
		return;
	}

	if (Cmd_Argc() > 2 ) {
		aircraft = AIR_AircraftGetFromIDX(atoi(Cmd_Argv(2)));
		if (!aircraft) {
			Com_Printf("No aircraft exist with global idx %i\n", atoi(Cmd_Argv(2)));
			return;
		}
		base = aircraft->homebase;
	}
	if (!aircraft)
		return;

	UI_ExecuteConfunc("aircraft_soldierlist_clear");
	const int teamSize = employeeType == EMPL_PILOT ? (AIR_GetPilot(aircraft) != NULL ? 1 : 0) : AIR_GetTeamSize(aircraft);
	const int maxTeamSize = employeeType == EMPL_PILOT ? 1 : aircraft->maxTeamSize;
	E_Foreach(employeeType, employee) {
		const aircraft_t *assignedCraft;
		const char *tooltip;

		if (!E_IsInBase(employee, base))
			continue;
		if (employee->transfer)
			continue;

		assignedCraft = AIR_IsEmployeeInAircraft(employee, NULL);
		if (assignedCraft == NULL) {
			/* employee unassigned */
			if (teamSize >= maxTeamSize)
				/* aircraft is full */
				tooltip = _("No more employee can be assigned to this aircraft");
			else
				/* aircraft has free space */
				tooltip = "";
		} else {
			/* employee assigned */
			if (assignedCraft == aircraft)
				/* assigned to this aircraft */
				tooltip = "";
			else
				/* assigned to another aircraft */
				tooltip = _("Employee is assigned to another aircraft");
		}

		UI_ExecuteConfunc("aircraft_soldierlist_add %d \"%s\" \"%s\" %d \"%s\"", employee->chr.ucn, typeId, employee->chr.name, assignedCraft == aircraft, tooltip);
	}
}


/**
 * @brief Fill the employee list for Base defence mission
 */
static void CP_TEAM_FillBDEFEmployeeList_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	aircraft_t *aircraft = base->aircraftCurrent;

	if (!aircraft)
		return;

	UI_ExecuteConfunc("soldierlist_clear");
	const int teamSize = AIR_GetTeamSize(aircraft);
	const int maxTeamSize = aircraft->maxTeamSize;
	E_Foreach(EMPL_SOLDIER, employee) {
		const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
		const char *tooltip;
		qboolean isInTeam;

		if (!E_IsInBase(employee, base))
			continue;
		if (employee->transfer)
			continue;

		isInTeam = AIR_IsEmployeeInAircraft(employee, aircraft) != NULL;
		if (E_IsAwayFromBase(employee))
				tooltip = _("Employee is away from base");
		else if (!isInTeam && teamSize >= maxTeamSize)
				tooltip = _("No more employee can be assigned to this team");
		else
			tooltip = "";

		UI_ExecuteConfunc("soldierlist_add %d \"%s %s\" %d \"%s\"", employee->chr.ucn, (rank) ? _(rank->shortname) : "", employee->chr.name, isInTeam, tooltip);
	}
}

/**
 * @brief Function that registers team (UI) callbacks
 */
void CP_TEAM_InitCallbacks (void)
{
	Cmd_AddCommand("team_updateequip", CP_UpdateEquipmentMenuParameters_f);

	Cmd_AddCommand("ui_team_select_ucn", CP_TEAM_SelectActorByUCN_f, "Select a soldier in the team menu by his/her UCN");
	Cmd_AddCommand("ui_team_assign_ucn", CP_TEAM_AssignSoldierByUCN_f, "Add/remove soldier to the aircraft");
	Cmd_AddCommand("ui_team_fill", CP_TEAM_FillEmployeeList_f, "Fill the Team assignment UI with employee");
	Cmd_AddCommand("ui_team_fillbdef", CP_TEAM_FillBDEFEmployeeList_f, "Fill the Team assignment UI with employee for base defence");
#ifdef DEBUG
	Cmd_AddCommand("debug_teamlist", CP_TeamListDebug_f, "Debug function to show all hired and assigned teammembers");
#endif
}

/**
 * @brief Function that unregisters team (UI) callbacks
 */
void CP_TEAM_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("ui_team_fillbdef");
	Cmd_RemoveCommand("ui_team_fill");
	Cmd_RemoveCommand("ui_team_assign_ucn");
	Cmd_RemoveCommand("ui_team_select_ucn");

	Cmd_RemoveCommand("team_updateequip");
#ifdef DEBUG
	Cmd_RemoveCommand("debug_teamlist");
#endif
}
