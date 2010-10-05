/**
 * @file cp_employee.c
 * @brief Single player employee stuff.
 * @note Employee related functions prefix: E_
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../client.h" /* cls */
#include "../cl_game.h" /* GAME_GetTeamDef */
#include "../cl_team.h"
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "cp_campaign.h"
#include "cp_employee_callbacks.h"
#include "cp_rank.h"
#include "save/save_employee.h"

/**
 * @brief Iterates through employees
 * @param[in] type Employee type to look for
 * @param[in] lastEmployee Pointer of the employee to iterate from. call with NULL to get the first one.
 * @sa employeeType_t
 */
employee_t* E_GetNext (employeeType_t type, employee_t *lastEmployee)
{
	employee_t* endOfEmployees = &ccs.employees[type][ccs.numEmployees[type]];
	employee_t* employee;

	if (!ccs.numEmployees[type])
		return NULL;

	if (!lastEmployee)
		return ccs.employees[type];
	assert(lastEmployee >= ccs.employees[type]);
	assert(lastEmployee < endOfEmployees);

	employee = lastEmployee;

	employee++;
	if (employee >= endOfEmployees)
		return NULL;
	else
		return employee;
}


/**
 * @brief Iterates through employees on a base
 * @param[in] type Employee type to look for
 * @param[in] lastEmployee Pointer of the employee to iterate from. call with NULL to get the first one.
 * @param[in] base Pointer to the base where employee hired at
 * @sa employeeType_t
 */
employee_t* E_GetNextFromBase (employeeType_t type, employee_t *lastEmployee, const base_t *base)
{
	employee_t* employee = lastEmployee;

	while ((employee = E_GetNext(type, employee))) {
		if (E_IsInBase(employee, base))
			break;
	}
	return employee;
}


/**
 * @brief Iterates through hired employees
 * @param[in] type Employee type to look for
 * @param[in] lastEmployee Pointer of the employee to iterate from. call with NULL to get the first one.
 * @sa employeeType_t
 */
employee_t* E_GetNextHired (employeeType_t type, employee_t *lastEmployee)
{
	employee_t* employee = lastEmployee;

	while ((employee = E_GetNext(type, employee))) {
		if (E_IsHired(employee))
			break;
	}
	return employee;
}

/**
 * @brief Tells you if a employee is away from his home base (gone in mission).
 * @param[in] employee Pointer to the employee.
 * @return qboolean qtrue if the employee is away in mission, qfalse if he is not or he is unhired.
 */
qboolean E_IsAwayFromBase (const employee_t *employee)
{
	const base_t *base;
	aircraft_t *aircraft;

	assert(employee);

	/* Check that employee is hired */
	if (!E_IsHired(employee))
		return qfalse;

	/* Check if employee is currently transferred. */
	if (employee->transfer)
		return qtrue;

	/* for now only soldiers, ugvs and pilots can be assigned to an aircraft */
	if (employee->type != EMPL_SOLDIER && employee->type != EMPL_ROBOT
	 && employee->type != EMPL_PILOT)
		return qfalse;

	base = employee->baseHired;

	aircraft = NULL;
	while ((aircraft = AIR_GetNextFromBase(base, aircraft)) != NULL)
		if (!AIR_IsAircraftInBase(aircraft) && AIR_IsInAircraftTeam(aircraft, employee))
			return qtrue;

	return qfalse;
}

/**
 * @brief  Hires some employees of appropriate type for a building
 * @param[in] base Which base the employee should be hired in.
 * @param[in] building in which building
 * @param[in] num how many employees, if -1, hire building->maxEmployees
 * @sa B_SetUpBase
 */
void E_HireForBuilding (base_t* base, building_t * building, int num)
{
	if (num < 0)
		num = building->maxEmployees;

	if (num) {
		employeeType_t employeeType;
		switch (building->buildingType) {
		case B_WORKSHOP:
			employeeType = EMPL_WORKER;
			break;
		case B_LAB:
			employeeType = EMPL_SCIENTIST;
			break;
		case B_HANGAR: /* the Dropship Hangar */
			employeeType = EMPL_SOLDIER;
			break;
		case B_MISC:
			Com_DPrintf(DEBUG_CLIENT, "E_HireForBuilding: Misc building type: %i with employees: %i.\n", building->buildingType, num);
			return;
		default:
			Com_DPrintf(DEBUG_CLIENT, "E_HireForBuilding: Unknown building type: %i.\n", building->buildingType);
			return;
		}
		/* don't try to hire more that available - see E_CreateEmployee */
		if (num > ccs.numEmployees[employeeType])
			num = ccs.numEmployees[employeeType];
		for (;num--;) {
			assert(base);
			if (!E_HireEmployeeByType(base, employeeType)) {
				Com_DPrintf(DEBUG_CLIENT, "E_HireForBuilding: Hiring %i employee(s) of type %i failed.\n", num, employeeType);
				return;
			}
		}
	}
}

/**
 * @brief Checks whether the given employee is in the given base
 * @sa E_EmployeeIsCurrentlyInBase
 */
qboolean E_IsInBase (const employee_t* empl, const base_t* const base)
{
	assert(empl != NULL);
	assert(base != NULL);

	if (!E_IsHired(empl))
		return qfalse;
	if (empl->baseHired == base)
		return qtrue;
	return qfalse;
}

/**
 * Will change the base where the employee is located in and will also update the
 * capacity in the affected bases.
 * @note Doesn't make any capacity checks and the employee must be hired already.
 * @param employee The employee to change the base for
 * @param newBase The base where the employee should be located at
 * @return @c false if @c employee was a @c NULL pointer
 */
qboolean E_MoveIntoNewBase (employee_t *employee, base_t *newBase)
{
	if (employee) {
		base_t *oldBase = employee->baseHired;
		assert(oldBase);
		employee->baseHired = newBase;
		/* Remove employee from corresponding capacity */
		switch (employee->type) {
		case EMPL_PILOT:
		case EMPL_WORKER:
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			oldBase->capacities[CAP_EMPLOYEES].cur--;
			newBase->capacities[CAP_EMPLOYEES].cur++;
			break;
		case EMPL_ROBOT:
			oldBase->capacities[CAP_ITEMS].cur -= UGV_SIZE;
			newBase->capacities[CAP_ITEMS].cur += UGV_SIZE;
			break;
		case MAX_EMPL:
			break;
		}
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Convert employeeType_t to translated string
 * @param type employeeType_t value
 * @return translated employee string
 */
const char* E_GetEmployeeString (employeeType_t type)
{
	switch (type) {
	case EMPL_SOLDIER:
		return _("Soldier");
	case EMPL_SCIENTIST:
		return _("Scientist");
	case EMPL_WORKER:
		return _("Worker");
	case EMPL_PILOT:
		return _("Pilot");
	case EMPL_ROBOT:
		return _("UGV");
	default:
		Com_Error(ERR_DROP, "Unknown employee type '%i'\n", type);
	}
}

/**
 * @brief Convert string to employeeType_t
 * @param type Pointer to employee type string
 * @return employeeType_t
 * @todo use Com_ConstInt*
 */
employeeType_t E_GetEmployeeType (const char* type)
{
	if (!type)
		return MAX_EMPL;

	if (!strcmp(type, "EMPL_SCIENTIST"))
		return EMPL_SCIENTIST;
	else if (!strcmp(type, "EMPL_SOLDIER"))
		return EMPL_SOLDIER;
	else if (!strcmp(type, "EMPL_WORKER"))
		return EMPL_WORKER;
	else if (!strcmp(type, "EMPL_PILOT"))
		return EMPL_PILOT;
	else if (!strcmp(type, "EMPL_ROBOT"))
		return EMPL_ROBOT;

	return MAX_EMPL;
}

/**
 * @brief Clears the employees list for loaded and new games
 * @sa CL_ResetSinglePlayerData
 * @sa E_DeleteEmployee
 */
void E_ResetEmployees (void)
{
	int i;

	Com_DPrintf(DEBUG_CLIENT, "E_ResetEmployees: Delete all employees\n");
	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++)
		if (ccs.numEmployees[i]) {
			memset(ccs.employees[i], 0, sizeof(ccs.employees[i]));
			ccs.numEmployees[i] = 0;
		}
}

/**
 * @brief Return a given employee pointer in the given base of a given type. Also
 * returns those employees, that are not yet hired.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type Which employee type do we search.
 * @param[in] idx Which employee id (in global employee array)
 * @return employee_t pointer or NULL
 */
employee_t* E_GetEmployee (const base_t* const base, employeeType_t type, int idx)
{
	employee_t *employee = NULL;

	if (!base || type >= MAX_EMPL || idx < 0)
		return NULL;

	while ((employee = E_GetNext(type, employee))) {
		if (employee->idx == idx && (!E_IsHired(employee) || E_IsInBase(employee, base)))
			return employee;
	}

	return NULL;
}

/**
 * @brief Return a given "not hired" employee pointer in the given base of a given type.
 * @param[in] type Which employee type to search for.
 * @param[in] idx Which employee id (in global employee array). Use -1, -2, etc.. to return the first/ second, etc... "unhired" employee.
 * @return employee_t pointer or NULL
 * @sa E_GetHiredEmployee
 * @sa E_UnhireEmployee
 * @sa E_HireEmployee
 */
static employee_t* E_GetUnhiredEmployee (employeeType_t type, int idx)
{
	int j = -1;	/* The number of found unhired employees. Ignore the minus. */
	employee_t *employee = NULL;

	if (type >= MAX_EMPL) {
		Com_Printf("E_GetUnhiredEmployee: Unknown EmployeeType: %i\n", type);
		return NULL;
	}

	while ((employee = E_GetNext(type, employee))) {
		if (employee->idx == idx) {
			if (E_IsHired(employee)) {
				Com_Printf("E_GetUnhiredEmployee: Warning: employee is already hired!\n");
				return NULL;
			}
			return employee;
		} else if (idx < 0 && !E_IsHired(employee)) {
			if (idx == j)
				return employee;
			j--;
		}
	}
	Com_Printf("Could not get unhired employee with index: %i of type %i (available: %i)\n", idx, type, ccs.numEmployees[type]);
	return NULL;
}

/**
 * @brief Return a "not hired" ugv-employee pointer of a given ugv-type.
 * @param[in] ugvType What type of robot we want.
 * @return employee_t pointer on success or NULL on error.
 * @sa E_GetHiredRobot
 */
employee_t* E_GetUnhiredRobot (const ugv_t *ugvType)
{
	employee_t *employee = NULL;

	while ((employee = E_GetNext(EMPL_ROBOT, employee))) {
		if (!E_IsHired(employee)) {
			/* If no type was given we return the first ugv we find. */
			if (!ugvType || employee->ugv == ugvType)
				return employee;
		}
	}
	Com_Printf("Could not get unhired ugv/robot.\n");
	return NULL;
}

/**
 * @brief Return a list of hired employees in the given base of a given type
 * @param[in] base Which base the employee should be searched in. If NULL is given employees in all bases will be listed.
 * @param[in] type Which employee type to search for.
 * @param[out] hiredEmployees Linked list of hired employees in the base.
 * @return Number of hired employees in the base that are currently not on a transfer.
 */
int E_GetHiredEmployees (const base_t* const base, employeeType_t type, linkedList_t **hiredEmployees)
{
	employee_t *employee;

	if (type >= MAX_EMPL) {
		Com_Printf("E_GetHiredEmployees: Unknown EmployeeType: %i\n", type);
		*hiredEmployees = NULL;
		return -1;
	}

	LIST_Delete(hiredEmployees);

	employee = NULL;
	while ((employee = E_GetNextHired(type, employee))) {
		if (!employee->transfer && (!base || E_IsInBase(employee, base))) {
			LIST_AddPointer(hiredEmployees, employee);
		}
	}

	if (hiredEmployees == NULL)
		return 0;
	return LIST_Count(*hiredEmployees);
}

/**
 * @brief Return a "hired" ugv-employee pointer of a given ugv-type in a given base.
 * @param[in] base Which base the ugv should be searched in.c
 * @param[in] ugvType What type of robot we want.
 * @return employee_t pointer on success or NULL on error.
 * @sa E_GetUnhiredRobot
 */
employee_t* E_GetHiredRobot (const base_t* const base, const ugv_t *ugvType)
{
	linkedList_t *hiredEmployees = NULL;
	linkedList_t *hiredEmployeesTemp;
	employee_t *employee;

	E_GetHiredEmployees(base, EMPL_ROBOT, &hiredEmployees);
	hiredEmployeesTemp = hiredEmployees;

	employee = NULL;
	while (hiredEmployeesTemp) {
		employee = (employee_t*)hiredEmployeesTemp->data;

		if ((employee->ugv == ugvType || !ugvType)	/* If no type was given we return the first ugv we find. */
		 && E_IsInBase(employee, base)) {		/* It has to be in the defined base. */
			assert(E_IsHired(employee));
			break;
		}

		hiredEmployeesTemp = hiredEmployeesTemp->next;
	}

	LIST_Delete(&hiredEmployees);

	if (!employee)
		Com_DPrintf(DEBUG_CLIENT, "Could not get unhired ugv/robot.\n");

	return employee;
}

/**
 * @brief Returns true if the employee is _only_ listed in the global list.
 * @param[in] employee The employee_t pointer to check
 * @return qboolean
 * @sa E_EmployeeIsFree
 */
static inline qboolean E_EmployeeIsUnassigned (const employee_t * employee)
{
	if (!employee)
		Com_Error(ERR_DROP, "E_EmployeeIsUnassigned: Employee is NULL.\n");

	return !employee->assigned;
}

/**
 * @brief Gets an unassigned employee of a given type from the given base.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassigned
 * @note assigned is not hired - they are already hired in a base, in a quarter _and_ working in another building.
 */
employee_t* E_GetAssignedEmployee (const base_t* const base, const employeeType_t type)
{
	employee_t *employee = NULL;
	while ((employee = E_GetNextFromBase(type, employee, base))) {
		if (!E_EmployeeIsUnassigned(employee))
			return employee;
	}
	return NULL;
}

/**
 * @brief Gets an assigned employee of a given type from the given base.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassigned
 * @note unassigned is not unhired - they are already hired in a base but are at quarters
 */
employee_t* E_GetUnassignedEmployee (const base_t* const base, const employeeType_t type)
{
	employee_t *employee = NULL;
	while ((employee = E_GetNextFromBase(type, employee, base))) {
		if (E_EmployeeIsUnassigned(employee))
			return employee;
	}
	return NULL;
}

/**
 * @brief Hires the employee in a base.
 * @param[in] base Which base the employee should be hired in
 * @param[in] employee Which employee to hire
 * @sa E_HireEmployeeByType
 * @sa E_UnhireEmployee
 * @todo handle EMPL_ROBOT capacities here?
 */
qboolean E_HireEmployee (base_t* base, employee_t* employee)
{
	if (base->capacities[CAP_EMPLOYEES].cur >= base->capacities[CAP_EMPLOYEES].max) {
		UI_Popup(_("Not enough quarters"), _("You don't have enough quarters for your employees.\nBuild more quarters."));
		return qfalse;
	}

	if (employee) {
		/* Now uses quarter space. */
		employee->baseHired = base;
		/* Update other capacities */
		switch (employee->type) {
		case EMPL_WORKER:
			base->capacities[CAP_EMPLOYEES].cur++;
			PR_UpdateProductionCap(base);
			break;
		case EMPL_PILOT:
			AIR_AutoAddPilotToAircraft(base, employee);
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			base->capacities[CAP_EMPLOYEES].cur++;
			break;
		case EMPL_ROBOT:
			base->capacities[CAP_ITEMS].cur += UGV_SIZE;
			break;
		case MAX_EMPL:
			break;
		}
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Hires the first free employee of that type.
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @sa E_HireEmployee
 * @sa E_UnhireEmployee
 */
qboolean E_HireEmployeeByType (base_t* base, employeeType_t type)
{
	employee_t* employee = E_GetUnhiredEmployee(type, -1);
	return employee ? E_HireEmployee(base, employee) : qfalse;
}

/**
 * @brief Hires the first free employee of that type.
 * @param[in] base  Which base the ugv/robot should be hired in.
 * @param[in] ugvType What type of ugv/robot should be hired.
 * @return qtrue if everything went ok (the ugv was added), otherwise qfalse.
 */
qboolean E_HireRobot (base_t* base, const ugv_t *ugvType)
{
	employee_t* employee = E_GetUnhiredRobot(ugvType);
	return employee ? E_HireEmployee(base, employee) : qfalse;
}

/**
 * @brief Removes the inventory of the employee and also removes him from buildings
 * @note This is used in the transfer start function (when you transfer an employee
 * this must be called for him to make him no longer useable in the current base)
 * and is also used when you completely unhire an employee.
 * @sa E_UnhireEmployee
 */
void E_ResetEmployee (employee_t *employee)
{
	assert(employee);
	assert(E_IsHired(employee));

	/* Remove employee from building (and tech/production). */
	E_RemoveEmployeeFromBuildingOrAircraft(employee);
	/* Destroy the inventory of the employee (carried items will remain in base->storage) */
	cls.i.DestroyInventory(&cls.i, &employee->chr.i);
}

/**
 * @brief Fires an employee.
 * @note also remove him from the aircraft
 * @param[in] employee The employee who will be fired
 * @sa E_HireEmployee
 * @sa E_HireEmployeeByType
 * @sa CL_RemoveSoldierFromAircraft
 * @sa E_ResetEmployee
 * @sa E_RemoveEmployeeFromBuildingOrAircraft
 * @todo handle EMPL_ROBOT capacities here?
 */
qboolean E_UnhireEmployee (employee_t* employee)
{
	if (employee && E_IsHired(employee) && !employee->transfer) {
		base_t *base = employee->baseHired;

		/* Any effect of removing an employee (e.g. removing a scientist from a research project)
		 * should take place in E_RemoveEmployeeFromBuildingOrAircraft */
		E_ResetEmployee(employee);
		/* Set all employee-tags to 'unhired'. */
		employee->baseHired = NULL;

		/* Remove employee from corresponding capacity */
		switch (employee->type) {
		case EMPL_PILOT:
		case EMPL_WORKER:
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			base->capacities[CAP_EMPLOYEES].cur--;
			break;
		case EMPL_ROBOT:
			base->capacities[CAP_ITEMS].cur -= UGV_SIZE;
			break;
		case MAX_EMPL:
			break;
		}

		return qtrue;
	} else
		Com_DPrintf(DEBUG_CLIENT, "Could not fire employee\n");
	return qfalse;
}

/**
 * @brief Reset the hired flag for all employees of a given type in a given base
 * @param[in] base Which base the employee should be fired from.
 * @param[in] type Which employee type do we search.
 */
void E_UnhireAllEmployees (base_t* base, employeeType_t type)
{
	employee_t *employee;

	if (!base)
		return;

	assert(type < MAX_EMPL);

	employee = NULL;
	while ((employee = E_GetNextFromBase(type, employee, base))) {
		E_UnhireEmployee(employee);
	}
}

/**
 * @brief Creates an entry of a new employee at the passed index location in the global list and assignes it to no building/base.
 * @param[in] type Tell the function what type of employee to create.
 * @param[in] nation Tell the function what nation the employee (mainly used for soldiers in singleplayer) comes from.
 * @param[in] ugvType Tell the function what type of ugv this employee is.
 * @param[in] emplIdx the index of the employee to create. -1 will use the next available index, >=0 will be used as the index of the employee.
 * @return Pointer to the newly created employee in the global list. NULL if something goes wrong.
 * @sa E_DeleteEmployee
 */
static employee_t* E_CreateEmployeeAtIndex (employeeType_t type, const nation_t *nation, const ugv_t *ugvType, const int emplIdx)
{
	employee_t* employee;
	int curEmployeeIdx;
	const char *teamID;
	char teamDefName[MAX_VAR];
	const char* rank;

	if (type >= MAX_EMPL)
		return NULL;

	if (emplIdx >= 0) {
		curEmployeeIdx = emplIdx;
	} else {
		curEmployeeIdx = ccs.numEmployees[type];
	}

	if (curEmployeeIdx >= MAX_EMPLOYEES) {
		Com_DPrintf(DEBUG_CLIENT, "E_CreateEmployee: MAX_EMPLOYEES exceeded for type %i\n", type);
		return NULL;
	}

	employee = &ccs.employees[type][curEmployeeIdx];
	memset(employee, 0, sizeof(*employee));

	employee->idx = curEmployeeIdx;
	employee->baseHired = NULL;
	employee->assigned = qfalse;
	employee->type = type;
	employee->nation = nation;
	employee->ugv = ugvType;

	if (ccs.curCampaign->team != TEAM_ALIEN)
		teamID = Com_ValueToStr(&ccs.curCampaign->team, V_TEAM, 0);
	else
		teamID = GAME_GetTeamDef();

	/* Generate character stats, models & names. */
	switch (type) {
	case EMPL_SOLDIER:
		rank = "rifleman";
		Q_strncpyz(teamDefName, teamID, sizeof(teamDefName));
		break;
	case EMPL_SCIENTIST:
		rank = "scientist";
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_scientist", teamID);
		break;
	case EMPL_PILOT:
		rank = "pilot";
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_pilot", teamID);
		break;
	case EMPL_WORKER:
		rank = "worker";
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_worker", teamID);
		break;
	case EMPL_ROBOT:
		if (!ugvType)
			Com_Error(ERR_DROP, "CL_GenerateCharacter: no type given for generation of EMPL_ROBOT employee.");

		rank = "ugv";

		Com_sprintf(teamDefName, sizeof(teamDefName), "%s%s", teamID, ugvType->actors);
		break;
	default:
		Com_Error(ERR_DROP, "Unknown employee type\n");
	}

	CL_GenerateCharacter(&employee->chr, teamDefName);

	employee->chr.score.rank = CL_GetRankIdx(rank);

	Com_DPrintf(DEBUG_CLIENT, "Generate character for type: %i\n", type);

	if (emplIdx < 0)
		ccs.numEmployees[type]++;

	return employee;
}

/**
 * @brief Creates an entry of a new employee in the global list and assignes it to no building/base.
 * @param[in] type What type of employee to create.
 * @param[in] nation What nation the employee (mainly used for soldiers in singleplayer) comes from.
 * @param[in] ugvType What type of ugv this employee is.
 * @return Pointer to the newly created employee in the global list. NULL if something goes wrong.
 * @sa E_DeleteEmployee
 */
employee_t* E_CreateEmployee (employeeType_t type, const nation_t *nation, const ugv_t *ugvType)
{
	/* Runs the create employee function with a -1 index parameter, which means at to end of list. */
	return E_CreateEmployeeAtIndex(type, nation, ugvType, -1);
}

/**
 * @brief Removes the employee completely from the game (buildings + global list).
 * @param[in] employee The pointer to the employee you want to remove.
 * @param[in] type What type of employee to delete.
 * @return True if the employee was removed successfully, otherwise false.
 * @sa E_CreateEmployee
 * @sa E_ResetEmployees
 * @sa E_UnhireEmployee
 * @note This function has the side effect, that the global employee number for
 * the given employee type is reduced by one, also the ccs.employees pointers are
 * moved to fill the gap of the removed employee. Thus pointers like acTeam in
 * the aircraft can point to wrong employees now. This has to be taken into
 * account
 */
qboolean E_DeleteEmployee (employee_t *employee)
{
	int i;
	qboolean found = qfalse;
	employeeType_t type;

	if (!employee)
		return qfalse;

	type = employee->type;

	/* Fire the employee. This will also:
	 * 1) remove him from buildings&work
	 * 2) remove his inventory */

	if (employee->baseHired) {
		/* make sure that this employee is really unhired */
		employee->transfer = qfalse;
		E_UnhireEmployee(employee);
	}

	/* Remove the employee from the global list. */
	for (i = 0; i < ccs.numEmployees[type]; i++) {
		if (&ccs.employees[type][i] == employee)
			found = qtrue;

		if (found) {
			if (i < MAX_EMPLOYEES - 1) { /* Just in case we have _that much_ employees. :) */
				/* Move all the following employees in the list one place forward and correct its index. */
				ccs.employees[type][i] = ccs.employees[type][i + 1];
				ccs.employees[type][i].idx = i;
			}
		}
	}

	if (found) {
		transfer_t *transfer;
		int j;

		for (j = 0; j < MAX_BASES; j++) {
			base_t *base = B_GetFoundedBaseByIDX(j);
			aircraft_t *aircraft;

			aircraft = NULL;
			while ((aircraft = AIR_GetNextFromBase(base, aircraft)) != NULL) {
				linkedList_t* l;

				for (l = aircraft->acTeam; l != NULL; l = l->next) {
					/* no need to check for == here, the employee should
					 * no longer be in this list, due to the E_UnhireEmployee
					 * call which will also remove any assignments for the
					 * aircraft - checking >= because the employee after the
					 * removed on in ccs.employees is now on the same position
					 * where the removed employee was before */
					/** @todo remove this once the employees are a linked list, too */
					if ((employee_t *)l->data >= employee)
						l->data -= sizeof(*employee);
				}

				if (type == EMPL_PILOT) {
					const employee_t *pilot = AIR_GetPilot(aircraft);
					if (pilot > employee)
						aircraft->pilot--;
					else if (pilot == employee)
						AIR_SetPilot(aircraft, NULL);
				}
			}
		}
		for (i = 0, transfer = ccs.transfers; i < ccs.numTransfers; i++, transfer++) {
			if (!transfer->active)
				continue;
			for (j = 0; j < MAX_EMPL; j++) {
				int k;
				for (k = 0; k < MAX_EMPLOYEES; k++) {
					if (transfer->employeeArray[j][k] > employee)
						transfer->employeeArray[j][k]--;
					else if (transfer->employeeArray[j][k] == employee)
						/** @todo This might not work as expected - maybe we
						 * have to memmove the array */
						transfer->employeeArray[j][k] = NULL;
				}
			}
		}

		ccs.numEmployees[type]--;
	} else {
		Com_DPrintf(DEBUG_CLIENT, "E_DeleteEmployee: Employee wasn't in the global list.\n");
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Removes all employees completely from the game (buildings + global list) from a given base.
 * @note Used if the base e.g is destroyed by the aliens.
 * @param[in] base Which base the employee should be fired from.
 */
void E_DeleteAllEmployees (base_t* base)
{
	int i;
	employeeType_t type;

	if (!base)
		return;

	Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: starting ...\n");

	for (type = EMPL_SOLDIER; type < MAX_EMPL; type++) {
		Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: Removing empl-type %i | num %i\n", type, ccs.numEmployees[type]);
		/* Warning:
		 * ccs.numEmployees[type] is changed in E_DeleteAllEmployees! (it's decreased by 1 per call)
		 * For this reason we start this loop from the back of the empl-list. toward 0. */
		for (i = ccs.numEmployees[type] - 1; i >= 0; i--) {
			employee_t *employee = &ccs.employees[type][i];
			Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: %i\n", i);
			if (E_IsInBase(employee, base)) {
				E_DeleteEmployee(employee);
				Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees:	 Removing empl.\n");
			} else if (employee->baseHired) {
				Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees:	 Not removing empl. (other base)\n");
			}
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: ... finished\n");
}


/**
 * @brief Removes employee until all employees fit in quarters capacity.
 * @param[in] base Pointer to the base where the number of employees should be updated.
 * @note employees are killed, and not just unhired (if base is destroyed, you can't recruit the same employees elsewhere)
 *	if you want to unhire employees, you should do it before calling this function.
 * @note employees are not randomly chosen. Reason is that all Quarter will be destroyed at the same time,
 *	so all employees are going to be killed anyway.
 */
void E_DeleteEmployeesExceedingCapacity (base_t *base)
{
	int type, i;

	/* Check if there are too many employees */
	if (base->capacities[CAP_EMPLOYEES].cur <= base->capacities[CAP_EMPLOYEES].max)
		return;

	/* do a reverse loop in order to finish by soldiers (the most important employees) */
	for (type = MAX_EMPL - 1; type >= 0; type--) {
		/* UGV are not stored in Quarters */
		if (type == EMPL_ROBOT)
			continue;

		/* Warning:
		 * ccs.numEmployees[type] is changed in E_DeleteAllEmployees! (it's decreased by 1 per call)
		 * For this reason we start this loop from the back of the empl-list. toward 0. */
		for (i = ccs.numEmployees[type] - 1; i >= 0 && ccs.numEmployees[type] >= 0; i--) {
			employee_t *employee = &ccs.employees[type][i];

			/* check if the employee is hired on this base */
			if (!E_IsInBase(employee, base))
				continue;

			E_DeleteEmployee(employee);
			if (base->capacities[CAP_EMPLOYEES].cur <= base->capacities[CAP_EMPLOYEES].max)
				return;
		}
	}

	Com_Printf("E_DeleteEmployeesExceedingCapacity: Warning, removed all employees from base '%s', but capacity is still > 0\n", base->name);
}

/**
 * @brief Recreates all the employees for a particular employee type in the global list.
 * But it does not overwrite any employees already hired.
 * @param[in] type The type of the employee list to process.
 * @param[in] excludeUnhappyNations True if a nation is unhappy then they wont
 * send any pilots, false if happiness of nations in not considered.
 * @sa CP_NationHandleBudget
 */
void E_RefreshUnhiredEmployeeGlobalList (const employeeType_t type, const qboolean excludeUnhappyNations)
{
	const nation_t *happyNations[MAX_NATIONS];
	int numHappyNations = 0;
	int idx, nationIdx;

	happyNations[0] = NULL;
	/* get a list of nations,  if excludeHappyNations is qtrue then also exclude
	 * unhappy nations (unhappy nation: happiness <= 0) from the list */
	for (idx = 0; idx < ccs.numNations; idx++) {
		const nation_t *nation = &ccs.nations[idx];
		if (nation->stats[0].happiness > 0 || !excludeUnhappyNations) {
			happyNations[numHappyNations] = nation;
			numHappyNations++;
		}
	}

	if (!numHappyNations)
		return;

	nationIdx = 0;
	/* Fill the global data employee list with employees, evenly distributed
	 * between nations in the happyNations list */
	for (idx = 0; idx < MAX_EMPLOYEES; idx++) {
		const employee_t *employee = &ccs.employees[type][idx];

		/* we dont want to overwrite employees that have already been hired */
		if (!E_IsHired(employee)) {
			E_CreateEmployeeAtIndex(type, happyNations[nationIdx], NULL, idx);
			nationIdx = (nationIdx + 1) % numHappyNations;
		}
	}
}


/**
 * @brief Remove one employee from building.
 * @todo Add check for base vs. employee_type and abort if they do not match.
 * @param[in] employee What employee to remove from its building.
 * @return Returns true if removing was possible/sane otherwise false.
 * @sa E_AssignEmployeeToBuilding
 * @todo are soldiers and pilots assigned to a building, too? quarters?
 */
qboolean E_RemoveEmployeeFromBuildingOrAircraft (employee_t *employee)
{
	base_t *base;

	assert(employee);

	/* not assigned to any building... */
	if (E_EmployeeIsUnassigned(employee)) {
		/* ... and no aircraft handling needed? */
		if (employee->type != EMPL_SOLDIER && employee->type != EMPL_ROBOT
		 && employee->type != EMPL_PILOT)
			return qfalse;
	}

	/* get the base where the employee is hired in */
	base = employee->baseHired;
	if (!base)
		Com_Error(ERR_DROP, "Employee (type: %i) is not hired", employee->type);

	switch (employee->type) {
	case EMPL_SCIENTIST:
		RS_RemoveFiredScientist(base, employee);
		break;

	case EMPL_ROBOT:
	case EMPL_SOLDIER:
		/* Remove soldier from aircraft/team if he was assigned to one. */
		if (AIR_IsEmployeeInAircraft(employee, NULL))
			AIR_RemoveEmployee(employee, NULL);
		break;

	case EMPL_PILOT:
		AIR_RemovePilotFromAssignedAircraft(base, employee);
		break;

	case EMPL_WORKER:
		/* Update current capacity and production times if worker is being counted there. */
		PR_UpdateProductionCap(base);
		break;

	/* otherwise the compiler would print a warning */
	case MAX_EMPL:
		break;
	}

	return qtrue;
}

/**
 * @brief Counts hired employees of a given type in a given base
 * @param[in] base The base where we count (@c NULL to count all).
 * @param[in] type The type of employee to search.
 * @return count of hired employees of a given type in a given base
 */
int E_CountHired (const base_t* const base, employeeType_t type)
{
	int count = 0;
	employee_t *employee = NULL;

	while ((employee = E_GetNextHired(type, employee))) {
		if (!base || E_IsInBase(employee, base))
			count++;
	}
	return count;
}

/**
 * @brief Counts 'hired' (i.e. bought or produced UGVs and other robots of a given ugv-type in a given base.
 * @param[in] base The base where we count (@c NULL to count all).
 * @param[in] ugvType What type of robot/ugv we are looking for.
 * @return Count of Robots/UGVs.
 */
int E_CountHiredRobotByType (const base_t* const base, const ugv_t *ugvType)
{
	int count = 0;
	employee_t *employee = NULL;

	while ((employee = E_GetNextHired(EMPL_ROBOT, employee))) {
		if (employee->ugv == ugvType && (!base || E_IsInBase(employee, base)))
			count++;
	}
	return count;
}


/**
 * @brief Counts all hired employees of a given base
 * @param[in] base The base where we count
 * @return count of hired employees of a given type in a given base
 * @note must not return 0 if hasBuilding[B_QUARTER] is qfalse: used to update capacity
 * @todo What about EMPL_ROBOT?
 */
int E_CountAllHired (const base_t* const base)
{
	employeeType_t type;
	int count;

	if (!base)
		return 0;

	count = 0;
	for (type = 0; type < MAX_EMPL; type++)
		count += E_CountHired(base, type);

	return count;
}

/**
 * @brief Counts unhired employees of a given type in a given base
 * @param[in] type The type of employee to search.
 * @return count of hired employees of a given type in a given base
 */
int E_CountUnhired (employeeType_t type)
{
	int count = 0;
	employee_t *employee = NULL;
	while ((employee = E_GetNext(type, employee))) {
		if (!E_IsHired(employee))
			count++;
	}
	return count;
}

/**
 * @brief Counts all available Robots/UGVs that are for sale.
 * @param[in] ugvType What type of robot/ugv we are looking for.
 * @return count of available robots/ugvs.
 */
int E_CountUnhiredRobotsByType (const ugv_t *ugvType)
{
	int count = 0;
	employee_t *employee = NULL;

	while ((employee = E_GetNext(EMPL_ROBOT, employee))) {
		if (!E_IsHired(employee) && employee->ugv == ugvType)
			count++;
	}
	return count;
}

/**
 * @brief Counts unassigned employees of a given type in a given base
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 */
int E_CountUnassigned (const base_t* const base, employeeType_t type)
{
	int count;
	employee_t *employee;

	if (!base)
		return 0;

	count = 0;
	employee = NULL;
	while ((employee = E_GetNextFromBase(type, employee, base))) {
		if (E_EmployeeIsUnassigned(employee))
			count++;
	}
	return count;
}

/**
 * @brief Hack to get a random nation for the initial
 */
static inline const nation_t *E_RandomNation (void)
{
	const int nationIndex = rand() % ccs.numNations;
	return &ccs.nations[nationIndex];
}

void E_InitialEmployees (void)
{
	campaign_t *campaign = ccs.curCampaign;
	int i;
	int j = 0;

	/* setup initial employee count */
	for (i = 0; i < campaign->soldiers; i++)
		E_CreateEmployee(EMPL_SOLDIER, E_RandomNation(), NULL);
	for (i = 0; i < campaign->scientists; i++)
		E_CreateEmployee(EMPL_SCIENTIST, E_RandomNation(), NULL);
	for (i = 0; i < campaign->ugvs; i++) {
		if (frand() > 0.5)
			E_CreateEmployee(EMPL_ROBOT, E_RandomNation(), Com_GetUGVByID("ugv_ares_w"));
		else
			E_CreateEmployee(EMPL_ROBOT, E_RandomNation(), Com_GetUGVByID("ugv_phoenix"));
	}
	for (i = 0; i < campaign->workers; i++)
		E_CreateEmployee(EMPL_WORKER, E_RandomNation(), NULL);

	/* Fill the global data employee list with pilots, evenly distributed between nations */
	for (i = 0; i < MAX_EMPLOYEES; i++) {
		const nation_t *nation = &ccs.nations[++j % ccs.numNations];
		if (!E_CreateEmployee(EMPL_PILOT, nation, NULL))
			break;
	}
}

#ifdef DEBUG
/**
 * @brief Debug command to list all hired employee
 */
static void E_ListHired_f (void)
{
	employeeType_t emplType;

	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		employee_t *employee = NULL;
		while ((employee = E_GetNextHired(emplType, employee))) {
			Com_Printf("Employee: %s (idx: %i) %s at %s\n", E_GetEmployeeString(employee->type), employee->idx,
					employee->chr.name, employee->baseHired->name);
			if (employee->type != emplType)
				Com_Printf("Warning: EmployeeType mismatch: %i != %i\n", emplType, employee->type);
		}
	}
}
#endif

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from UI_InitStartup resp. CL_InitLocal
 */
void E_InitStartup (void)
{
#ifdef DEBUG
	Cmd_AddCommand("debug_listhired", E_ListHired_f, "Debug command to list all hired employee");
#endif
}

/**
 * @brief Searches all employee for the ucn (character id)
 * @param[in] uniqueCharacterNumber unique character number (UCN)
 */
employee_t* E_GetEmployeeFromChrUCN (int uniqueCharacterNumber)
{
	int j;

	/* MAX_EMPLOYEES and not numWholeTeam - maybe some other soldier died */
	for (j = 0; j < MAX_EMPL; j++) {
		employee_t *employee = NULL;
		while ((employee = E_GetNext(j, employee))) {
			if (employee->chr.ucn == uniqueCharacterNumber)
				return employee;
		}
	}

	return NULL;
}


/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @sa E_LoadXML
 * @sa SAV_GameSaveXML
 * @sa G_SendCharacterData
 * @sa CP_ParseCharacterData
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
qboolean E_SaveXML (mxml_node_t *p)
{
	employeeType_t j;

	Com_RegisterConstList(saveEmployeeConstants);
	for (j = 0; j < MAX_EMPL; j++) {
		mxml_node_t *snode = mxml_AddNode(p, SAVE_EMPLOYEE_EMPLOYEES);
		employee_t *e = NULL;

		mxml_AddString(snode, SAVE_EMPLOYEE_TYPE, Com_GetConstVariable(SAVE_EMPLOYEETYPE_NAMESPACE, j));
		while ((e = E_GetNext(j, e))) {
			mxml_node_t * chrNode;
			mxml_node_t *ssnode = mxml_AddNode(snode, SAVE_EMPLOYEE_EMPLOYEE);

			/** @note e->transfer is not saved here because it'll be restored via TR_Load. */
			mxml_AddInt(ssnode, SAVE_EMPLOYEE_IDX, e->idx);
			if (e->baseHired)
				mxml_AddInt(ssnode, SAVE_EMPLOYEE_BASEHIRED, e->baseHired->idx);
			if (e->assigned)
				mxml_AddBool(ssnode, SAVE_EMPLOYEE_ASSIGNED, e->assigned);
			/* Store the nations identifier string. */
			assert(e->nation);
			mxml_AddString(ssnode, SAVE_EMPLOYEE_NATION, e->nation->id);
			/* Store the ugv-type identifier string. (Only exists for EMPL_ROBOT). */
			if (e->ugv)
				mxml_AddString(ssnode, SAVE_EMPLOYEE_UGV, e->ugv->id);
			/* Character Data */
			chrNode = mxml_AddNode(ssnode, SAVE_EMPLOYEE_CHR);
			CL_SaveCharacterXML(chrNode, &e->chr);
		}
	}
	Com_UnregisterConstList(saveEmployeeConstants);

	return qtrue;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] p XML Node structure, where we get the information from
 */
qboolean E_LoadXML (mxml_node_t *p)
{
	mxml_node_t * snode;
	qboolean success = qtrue;

	Com_RegisterConstList(saveEmployeeConstants);
	for (snode = mxml_GetNode(p, SAVE_EMPLOYEE_EMPLOYEES); snode;
			snode = mxml_GetNextNode(snode, p , SAVE_EMPLOYEE_EMPLOYEES)) {
		int i;
		mxml_node_t * ssnode;
		employeeType_t emplType;
		const char *type = mxml_GetString(snode, SAVE_EMPLOYEE_TYPE);

		if (!Com_GetConstIntFromNamespace(SAVE_EMPLOYEETYPE_NAMESPACE, type, (int*) &emplType)) {
			Com_Printf("Invalid employee type '%s'\n", type);
			success = qfalse;
			break;
		}

		for (ssnode = mxml_GetNode(snode, SAVE_EMPLOYEE_EMPLOYEE), i = 0; i < MAX_EMPLOYEES && ssnode;
				ssnode = mxml_GetNextNode(ssnode, snode, SAVE_EMPLOYEE_EMPLOYEE), i++) {
			int baseIDX;
			mxml_node_t * chrNode;
			employee_t *e = &ccs.employees[emplType][i];

			/** @note e->transfer is restored in cl_transfer.c:TR_Load */
			e->idx = mxml_GetInt(ssnode, SAVE_EMPLOYEE_IDX, MAX_EMPLOYEES);
			if (e->idx >= MAX_EMPLOYEES) {
				Com_Printf("Invalid employeeIDX\n");
				success = qfalse;
				break;
			}
			e->type = emplType;
			/* base */
			assert(ccs.numBases);	/* Just in case the order is ever changed. */
			baseIDX = mxml_GetInt(ssnode, SAVE_EMPLOYEE_BASEHIRED, -1);
			e->baseHired = (baseIDX >= 0) ? B_GetBaseByIDX(baseIDX) : NULL;
			/* assigned to a building? */
			/** @todo compatibility code - remove me */
			if (mxml_GetInt(ssnode, SAVE_EMPLOYEE_BUILDING, -1) != -1)
				e->assigned = qtrue;
			else
				e->assigned = mxml_GetBool(ssnode, SAVE_EMPLOYEE_ASSIGNED, qfalse);
			/* nation */
			e->nation = NAT_GetNationByID(mxml_GetString(ssnode, SAVE_EMPLOYEE_NATION));
			if (!e->nation) {
				Com_Printf("No nation defined for employee\n");
				success = qfalse;
				break;
			}
			/* UGV-Type */
			e->ugv = Com_GetUGVByIDSilent(mxml_GetString(ssnode, SAVE_EMPLOYEE_UGV));
			/* Character Data */
			chrNode = mxml_GetNode(ssnode, SAVE_EMPLOYEE_CHR);
			if (!chrNode) {
				Com_Printf("No character definition found for employee\n");
				success = qfalse;
				break;
			}
			if (!CL_LoadCharacterXML(chrNode, &e->chr)) {
				Com_Printf("Error loading character definition for employee\n");
				success = qfalse;
				break;
			}
		}
		if (!success)
			break;
		ccs.numEmployees[emplType] = i;
	}
	Com_UnregisterConstList(saveEmployeeConstants);

	return success;
}

/**
 * @brief Returns true if the current base is able to handle employees
 * @sa B_BaseInit_f
 */
qboolean E_HireAllowed (const base_t* base)
{
	if (!B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_QUARTERS))
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief Removes the items of an employee (soldier) from the base storage (s)he is hired at
 * @param[in] employee Pointer to the soldier whose items should be removed
 */
void E_RemoveInventoryFromStorage (employee_t *employee)
{
	containerIndex_t container;
	const character_t *chr = &employee->chr;

	assert(employee->baseHired);

	for (container = 0; container < csi.numIDs; container++) {
		const invList_t *invList = CONTAINER(chr, container);

		if (INVDEF(container)->temp)
			continue;

		while (invList) {
			/* Remove ammo */
			if (invList->item.m && invList->item.m != invList->item.t)
				B_UpdateStorageAndCapacity(employee->baseHired, invList->item.m, -1, qfalse, qfalse);
			/* Remove Item */
			if (invList->item.t)
				B_UpdateStorageAndCapacity(employee->baseHired, invList->item.t, -1, qfalse, qfalse);

			invList = invList->next;
		}
	}
}
