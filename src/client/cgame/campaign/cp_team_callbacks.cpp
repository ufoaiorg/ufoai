/**
 * @file
 * @brief Menu related callback functions for the team menu
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "cp_hospital.h" /* HOS_GetInjuryLevel */
#ifdef DEBUG
#include "cp_geoscape.h" /* GEO_GetSelectedAircraft */
#endif

/**
 * @brief Adds or removes a soldier to/from an aircraft using his/her UCN as reference.
 */
static void CP_TEAM_AssignSoldierByUCN_f (void)
{
	/* check syntax */
	if (cgi->Cmd_Argc() < 1 ) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int ucn = atoi(cgi->Cmd_Argv(1));
	if (ucn < 0)
		return;

	const base_t* base = B_GetCurrentSelectedBase();
	const employeeType_t employeeType = EMPL_SOLDIER;
	aircraft_t* aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	Employee* employee = E_GetEmployeeFromChrUCN(ucn);
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
 * @brief Selects a soldier by his/her Unique Character Number on team UI
 */
static void CP_TEAM_SelectActorByUCN_f (void)
{
	/* check syntax */
	if (cgi->Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	const base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	const int ucn = atoi(cgi->Cmd_Argv(1));
	if (ucn < 0) {
		cgi->UI_ExecuteConfunc("reset_character_cvars");
		return;
	}

	Employee* employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		cgi->Com_Error(ERR_DROP, "CP_TEAM_SelectActorByUCN_f: No employee with UCN %i", ucn);

	character_t* chr = &employee->chr;

	/* update menu inventory */
	CP_SetEquipContainer(chr);

	/* set info cvars */
	cgi->CL_UpdateCharacterValues(chr);
}

/**
 * @brief Removes every item from a soldier
 */
static void CP_TEAM_DeEquipActor_f (void)
{
	/* check syntax */
	if (cgi->Cmd_Argc() < 1) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	const int ucn = atoi(cgi->Cmd_Argv(1));
	if (ucn < 0) {
		cgi->UI_ExecuteConfunc("reset_character_cvars");
		return;
	}

	Employee* employee = E_GetEmployeeFromChrUCN(ucn);
	if (!employee)
		cgi->Com_Error(ERR_DROP, "CP_TEAM_DeEquipActor_f: No employee with UCN %i", ucn);

	character_t* chr = &employee->chr;

	cgi->INV_DestroyInventory(&chr->inv);

	CP_CleanTempInventory(base);
	equipDef_t unused = base->storage;
	CP_CleanupTeam(base, &unused);
	cgi->UI_ContainerNodeUpdateEquipment(&base->bEquipment, &unused);

	/* set info cvars */
	cgi->CL_UpdateCharacterValues(chr);
}

#ifdef DEBUG
/**
 * @brief Debug function to list the actual team
 */
static void CP_TeamListDebug_f (void)
{
	const aircraft_t* aircraft = GEO_GetSelectedAircraft();
	if (!aircraft) {
		Com_Printf("Buy/build an aircraft first.\n");
		return;
	}

	const base_t* base = aircraft->homebase;
	if (!base) {
		Com_Printf("Build and select a base first\n");
		return;
	}

	Com_Printf("%i members in the current team", AIR_GetTeamSize(aircraft));
	for (linkedList_t* list = aircraft->acTeam; list != nullptr; list = list->next) {
		const Employee* employee = (const Employee*)list->data;
		Com_Printf("ucn %i - name: %s\n", employee->chr.ucn, employee->chr.name);
	}
}
#endif

/**
 * @brief Fill the employee list for Soldier/Pilot assignment
 */
static void CP_TEAM_FillEmployeeList_f (void)
{
	if (cgi->Cmd_Argc() <= 1 ) {
		Com_Printf("Usage: %s <soldier|pilot> [aircraftIDX]\n", cgi->Cmd_Argv(0));
		return;
	}

	char typeId[MAX_VAR];
	Q_strncpyz(typeId, cgi->Cmd_Argv(1), lengthof(typeId));
	const employeeType_t employeeType = E_GetEmployeeType(typeId);

	if (employeeType == MAX_EMPL) {
		Com_Printf("Invalid employeeType: %s\n", typeId);
		return;
	}

	const base_t* base = B_GetCurrentSelectedBase();
	const aircraft_t* aircraft = base->aircraftCurrent;
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
	const int teamSize = employeeType == EMPL_PILOT ? (AIR_GetPilot(aircraft) != nullptr ? 1 : 0) : AIR_GetTeamSize(aircraft);
	const int maxTeamSize = employeeType == EMPL_PILOT ? 1 : aircraft->maxTeamSize;
	E_Foreach(employeeType, employee) {
		const aircraft_t* assignedCraft;
		const char* tooltip;

		if (!employee->isHiredInBase(base))
			continue;
		if (employee->transfer)
			continue;

		assignedCraft = AIR_IsEmployeeInAircraft(employee, nullptr);
		if (assignedCraft == nullptr) {
			/* employee unassigned */
			if (teamSize >= maxTeamSize)
				/* aircraft is full */
				tooltip = _("No more employees can be assigned to this aircraft");
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

		const int needsHealing = HOS_NeedsHealing(employee->chr) ? 1 : 0;
		cgi->UI_ExecuteConfunc("aircraft_soldierlist_add %d \"%s\" \"%s\" %d %d \"%s\"", employee->chr.ucn, typeId, employee->chr.name, assignedCraft == aircraft, needsHealing, tooltip);
	}
}

/**
 * @brief Fill the employee list for the in-base soldier equip screen and initialize the inventory
 */
static void CP_TEAM_FillEquipSoldierList_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	const aircraft_t* aircraft = base->aircraftCurrent;

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
			aircraft = nullptr;
		}
	}

	/* add soldiers to list */
	int count = 0;
	cgi->UI_ExecuteConfunc("equipment_soldierlist_clear");
	if (aircraft) {
		LIST_Foreach(aircraft->acTeam, Employee, employee) {
			character_t* chr = &employee->chr;
			CP_SetEquipContainer(chr);
			const int needsHealing = HOS_NeedsHealing(*chr) ? 1 : 0;
			cgi->UI_ExecuteConfunc("equipment_soldierlist_add %d \"%s\" %d \"\"", chr->ucn, chr->name, needsHealing);
			count++;
		}
	} else {
		E_Foreach(EMPL_SOLDIER, employee) {
			if (!employee->isHiredInBase(base))
				continue;
			if (employee->transfer)
				continue;
			if (employee->isAwayFromBase())
				continue;
			character_t* chr = &employee->chr;
			CP_SetEquipContainer(chr);
			const int needsHealing = HOS_NeedsHealing(*chr) ? 1 : 0;
			const aircraft_t* assignedCraft = AIR_IsEmployeeInAircraft(employee, nullptr);
			cgi->UI_ExecuteConfunc("equipment_soldierlist_add %d \"%s\" %d \"%s\"",
					chr->ucn, chr->name, needsHealing, assignedCraft ? assignedCraft->name : "");
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
	const base_t* base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	const aircraft_t* aircraft = base->aircraftCurrent;
	if (!aircraft)
		return;

	cgi->UI_ExecuteConfunc("soldierlist_clear");
	const int teamSize = AIR_GetTeamSize(aircraft);
	const int maxTeamSize = aircraft->maxTeamSize;
	E_Foreach(EMPL_SOLDIER, employee) {
		if (!employee->isHiredInBase(base))
			continue;
		if (employee->transfer)
			continue;

		const char* tooltip;
		const bool isInTeam = AIR_IsEmployeeInAircraft(employee, aircraft) != nullptr;
		if (employee->isAwayFromBase())
			tooltip = _("Employee is away from base");
		else if (!isInTeam && teamSize >= maxTeamSize)
			tooltip = _("No more employee can be assigned to this team");
		else
			tooltip = "";

		const rank_t* rank = CL_GetRankByIdx(employee->chr.score.rank);
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
	const int ucn = atoi(cgi->Cmd_Argv(1));
	const int bodySkinIdx = atoi(cgi->Cmd_Argv(2));

	Employee* soldier = E_GetEmployeeFromChrUCN(ucn);
	if (soldier == nullptr || !soldier->isSoldier()) {
		Com_Printf("Invalid soldier UCN: %i\n", ucn);
		return;
	}

	cgi->Cvar_SetValue("mn_body_skin", bodySkinIdx);
	soldier->chr.bodySkin = bodySkinIdx;
}

static const cmdList_t teamCallbacks[] = {
	{"ui_team_select_ucn", CP_TEAM_SelectActorByUCN_f, "Select a soldier in the team menu by his/her UCN"},
	{"ui_team_assign_ucn", CP_TEAM_AssignSoldierByUCN_f, "Add/remove soldier to the aircraft"},
	{"ui_team_fill", CP_TEAM_FillEmployeeList_f, "Fill the Team assignment UI with employee"},
	{"ui_team_fillbdef", CP_TEAM_FillBDEFEmployeeList_f, "Fill the Team assignment UI with employee for base defence"},
	{"ui_team_fillequip", CP_TEAM_FillEquipSoldierList_f, "Fill the employee list for the in-base soldier equip screen and initialize the inventory"},
	{"ui_team_deequip", CP_TEAM_DeEquipActor_f, "De-equip soldier"},
	{"ui_team_changeskin", CP_TEAM_ChangeSkin_f, "Change the skin of a soldier"},
#ifdef DEBUG
	{"debug_teamlist", CP_TeamListDebug_f, "Debug function to show all hired and assigned teammembers"},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief Function that registers team (UI) callbacks
 */
void CP_TEAM_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(teamCallbacks);
}

/**
 * @brief Function that unregisters team (UI) callbacks
 */
void CP_TEAM_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(teamCallbacks);
}
