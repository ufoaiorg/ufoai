/**
 * @file
 * @brief Menu related callback functions for the team menu
 */

/*
All original material Copyright (C) 2002-2013 UFO: Alien Invasion.

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
#include "../../ui/ui_dataids.h"

#include "cp_campaign.h"
#include "cp_team.h"
#include "cp_team_callbacks.h"
#ifdef DEBUG
#include "cp_geoscape.h" /* GEO_GetSelectedAircraft */
#endif

/**
 * @brief Adds or removes a soldier to/from an aircraft using his/her UCN as reference.
 */
static void CP_TEAM_AssignSoldierByUCN_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	aircraft_t *aircraft;
	int ucn;
	const employeeType_t employeeType = EMPL_SOLDIER;
	Employee *employee;

	/* check syntax */
	if (cgi->Cmd_Argc() < 1 ) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	ucn = atoi(cgi->Cmd_Argv(1));
	if (ucn < 0)
		return;

	aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		cgi->Com_Error(ERR_DROP, "CP_TEAM_SelectActorByUCN_f: No employee with UCN %i", ucn);

	if (AIR_IsEmployeeInAircraft(employee, aircraft)) {
		AIR_RemoveEmployee(employee, aircraft);
	} else {
		if (employee->isPilot())
			AIR_SetPilot(aircraft, employee);
		else
			AIR_AddToAircraftTeam(aircraft, employee);
	}

	CP_UpdateActorAircraftVar(aircraft, employeeType);
	cgi->Cvar_SetValue("cpteam_size", AIR_GetTeamSize(aircraft));
	cgi->UI_ExecuteConfunc("aircraft_status_change");
}

/**
 * @brief Set up equip (floor) container for soldiers
 * @param[in,out] chr Pointer to soldiers character structure
 */
static void CP_TEAM_SetEquipContainer (character_t *chr)
{
	inventory_t *uiInv = *cgi->ui_inventory;
	if (uiInv && uiInv != &chr->inv) {
		chr->inv.setContainer(CID_EQUIP, uiInv->getContainer2(CID_EQUIP));
		/* set 'old' CID_EQUIP to NULL */
		uiInv->resetContainer(CID_EQUIP);
	}
	*cgi->ui_inventory = &chr->inv;
}

/**
 * @brief Selects a soldier by his/her Unique Character Number on team UI
 */
static void CP_TEAM_SelectActorByUCN_f (void)
{
	Employee *employee;
	character_t *chr;
	int ucn;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* check syntax */
	if (cgi->Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	ucn = atoi(cgi->Cmd_Argv(1));
	if (ucn < 0) {
		cgi->UI_ExecuteConfunc("reset_character_cvars");
		return;
	}

	employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		cgi->Com_Error(ERR_DROP, "CP_TEAM_SelectActorByUCN_f: No employee with UCN %i", ucn);

	chr = &employee->chr;

	/* update menu inventory */
	CP_TEAM_SetEquipContainer(chr);

	/* set info cvars */
	CL_UpdateCharacterValues(chr);
}

/**
 * @brief Removes every item from a soldier
 */
static void CP_TEAM_DeEquipActor_f (void)
{
	Employee *employee;
	character_t *chr;
	int ucn;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* check syntax */
	if (cgi->Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	ucn = atoi(cgi->Cmd_Argv(1));
	if (ucn < 0) {
		cgi->UI_ExecuteConfunc("reset_character_cvars");
		return;
	}

	employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		cgi->Com_Error(ERR_DROP, "CP_TEAM_DeEquipActor_f: No employee with UCN %i", ucn);

	chr = &employee->chr;

	cgi->INV_DestroyInventory(&chr->inv);

	CP_CleanTempInventory(base);
	equipDef_t unused = base->storage;
	CP_CleanupTeam(base, &unused);
	cgi->UI_ContainerNodeUpdateEquipment(&base->bEquipment, &unused);

	/* set info cvars */
	CL_UpdateCharacterValues(chr);
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

	aircraft = GEO_GetSelectedAircraft();
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
		const Employee *employee = (const Employee *)l->data;
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

	if (cgi->Cmd_Argc() <= 1 ) {
		Com_Printf("Usage: %s <soldier|pilot> [aircraftIDX]\n", cgi->Cmd_Argv(0));
		return;
	}
	Q_strncpyz(typeId, cgi->Cmd_Argv(1), lengthof(typeId));
	employeeType = E_GetEmployeeType(typeId);

	if (employeeType == MAX_EMPL) {
		Com_Printf("Invalid employeeType: %s\n", typeId);
		return;
	}

	if (cgi->Cmd_Argc() > 2 ) {
		aircraft = AIR_AircraftGetFromIDX(atoi(cgi->Cmd_Argv(2)));
		if (!aircraft) {
			Com_Printf("No aircraft exist with global idx %i\n", atoi(cgi->Cmd_Argv(2)));
			return;
		}
		base = aircraft->homebase;
	}
	if (!aircraft)
		return;

	cgi->UI_ExecuteConfunc("aircraft_soldierlist_clear");
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

		cgi->UI_ExecuteConfunc("aircraft_soldierlist_add %d \"%s\" \"%s\" %d \"%s\"", employee->chr.ucn, typeId, employee->chr.name, assignedCraft == aircraft, tooltip);
	}
}

/**
 * @brief Fill the employee list for the in-base soldier equip screen and initialize the inventory
 */
static void CP_TEAM_FillEquipSoldierList_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	aircraft_t *aircraft = base->aircraftCurrent;

	if (cgi->Cmd_Argc() > 1 ) {
		int idx = atoi(cgi->Cmd_Argv(1));

		if (idx >= 0) {
			aircraft = AIR_AircraftGetFromIDX(idx);
			if (!aircraft) {
				Com_Printf("No aircraft exist with global idx %i\n", idx);
				return;
			}
			base = aircraft->homebase;
			assert(base);
		} else {
			aircraft = NULL;
		}
	}

	/* add soldiers to list */
	int count = 0;
	cgi->UI_ExecuteConfunc("equipment_soldierlist_clear");
	if (aircraft) {
		LIST_Foreach(aircraft->acTeam, Employee, employee) {
			character_t *chr = &employee->chr;
			CP_TEAM_SetEquipContainer(chr);
			cgi->UI_ExecuteConfunc("equipment_soldierlist_add %d \"%s\"", chr->ucn, chr->name);
			count++;
		}
	} else {
		E_Foreach(EMPL_SOLDIER, employee) {
			if (!E_IsInBase(employee, base))
				continue;
			if (employee->transfer)
				continue;
			if (E_IsAwayFromBase(employee))
				continue;
			character_t *chr = &employee->chr;
			CP_TEAM_SetEquipContainer(chr);
			cgi->UI_ExecuteConfunc("equipment_soldierlist_add %d \"%s\"", chr->ucn, chr->name);
			count++;
		}
	}
	/* clean up aircraft crew for upcoming mission */
	CP_CleanTempInventory(base);
	equipDef_t unused = base->storage;
	CP_CleanupTeam(base, &unused);
	cgi->UI_ContainerNodeUpdateEquipment(&base->bEquipment, &unused);
	if (count == 0)
		cgi->UI_PopWindow(false);
}

/**
 * @brief Fill the employee list for Base defence mission
 */
static void CP_TEAM_FillBDEFEmployeeList_f (void)
{
	const base_t *base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	const aircraft_t *aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	cgi->UI_ExecuteConfunc("soldierlist_clear");
	const int teamSize = AIR_GetTeamSize(aircraft);
	const int maxTeamSize = aircraft->maxTeamSize;
	E_Foreach(EMPL_SOLDIER, employee) {
		if (!E_IsInBase(employee, base))
			continue;
		if (employee->transfer)
			continue;

		const char *tooltip;
		const bool isInTeam = AIR_IsEmployeeInAircraft(employee, aircraft) != NULL;
		if (E_IsAwayFromBase(employee))
			tooltip = _("Employee is away from base");
		else if (!isInTeam && teamSize >= maxTeamSize)
			tooltip = _("No more employee can be assigned to this team");
		else
			tooltip = "";

		const rank_t *rank = CL_GetRankByIdx(employee->chr.score.rank);
		cgi->UI_ExecuteConfunc("soldierlist_add %d \"%s %s\" %d \"%s\"", employee->chr.ucn, (rank) ? _(rank->shortname) : "", employee->chr.name, isInTeam, tooltip);
	}
}

/**
 * @brief Change the skin of a soldier
 */
static void CP_TEAM_ChangeSkin_f (void)
{
	if (cgi->Cmd_Argc() < 3 ) {
		Com_Printf("Usage: %s <ucn> <bodyskinidx>\n", cgi->Cmd_Argv(0));
		return;
	}
	int ucn = atoi(cgi->Cmd_Argv(1));
	int bodySkinIdx = atoi(cgi->Cmd_Argv(2));

	Employee *soldier = E_GetEmployeeFromChrUCN(ucn);
	if (soldier == NULL || !soldier->isSoldier()) {
		Com_Printf("Invalid soldier UCN: %i\n", ucn);
		return;
	}

	cgi->Cvar_SetValue("mn_body_skin", bodySkinIdx);
	soldier->chr.bodySkin = bodySkinIdx;
}

/**
 * @brief Function that registers team (UI) callbacks
 */
void CP_TEAM_InitCallbacks (void)
{
	cgi->Cmd_AddCommand("ui_team_select_ucn", CP_TEAM_SelectActorByUCN_f, "Select a soldier in the team menu by his/her UCN");
	cgi->Cmd_AddCommand("ui_team_assign_ucn", CP_TEAM_AssignSoldierByUCN_f, "Add/remove soldier to the aircraft");
	cgi->Cmd_AddCommand("ui_team_fill", CP_TEAM_FillEmployeeList_f, "Fill the Team assignment UI with employee");
	cgi->Cmd_AddCommand("ui_team_fillbdef", CP_TEAM_FillBDEFEmployeeList_f, "Fill the Team assignment UI with employee for base defence");
	cgi->Cmd_AddCommand("ui_team_fillequip", CP_TEAM_FillEquipSoldierList_f, "Fill the employee list for the in-base soldier equip screen and initialize the inventory");
	cgi->Cmd_AddCommand("ui_team_deequip", CP_TEAM_DeEquipActor_f, "De-equip soldier");
	cgi->Cmd_AddCommand("ui_team_changeskin", CP_TEAM_ChangeSkin_f, "Change the skin of a soldier");
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_teamlist", CP_TeamListDebug_f, "Debug function to show all hired and assigned teammembers");
#endif
}

/**
 * @brief Function that unregisters team (UI) callbacks
 */
void CP_TEAM_ShutdownCallbacks (void)
{
	cgi->Cmd_RemoveCommand("ui_team_changeskin");
	cgi->Cmd_RemoveCommand("ui_team_deequip");
	cgi->Cmd_RemoveCommand("ui_team_fillequip");
	cgi->Cmd_RemoveCommand("ui_team_fillbdef");
	cgi->Cmd_RemoveCommand("ui_team_fill");
	cgi->Cmd_RemoveCommand("ui_team_assign_ucn");
	cgi->Cmd_RemoveCommand("ui_team_select_ucn");
#ifdef DEBUG
	cgi->Cmd_RemoveCommand("debug_teamlist");
#endif
}
