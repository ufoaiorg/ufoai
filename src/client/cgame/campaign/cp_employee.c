/**
 * @file cp_employee.c
 * @brief Single player employee stuff.
 * @note Employee related functions prefix: E_
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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
#include "../cl_game.h" /* GAME_GetTeamDef */
#include "../cl_game_team.h" /* character xml loading */
#include "cp_campaign.h"
#include "cp_employee_callbacks.h"
#include "cp_rank.h"
#include "cp_popup.h"
#include "save/save_employee.h"

/**
 * @brief Returns number of employees of a type
 * @param[in] type Employeetype to check
 */
int E_CountByType (employeeType_t type)
{
	return LIST_Count(ccs.employees[type]);
}

/**
 * @brief Iterates through unhired employees
 * @param[in] type Employee type to look for
 * @sa employeeType_t
 */
employee_t* E_GetUnhired (employeeType_t type)
{
	E_Foreach(type, employee) {
		if (!E_IsHired(employee))
			return employee;
	}

	return NULL;
}

/**
 * @brief Tells you if a employee is away from his home base (gone in mission).
 * @param[in] employee Pointer to the employee.
 * @return qboolean qtrue if the employee is away in mission, qfalse if he is not or he is unhired.
 */
qboolean E_IsAwayFromBase (const employee_t *employee)
{
	const base_t *base;

	assert(employee);

	/* Check that employee is hired */
	if (!E_IsHired(employee))
		return qfalse;

	/* Check if employee is currently transferred. */
	if (employee->transfer)
		return qtrue;

	/* for now only soldiers and pilots can be assigned to an aircraft */
	if (employee->type != EMPL_SOLDIER && employee->type != EMPL_PILOT)
		return qfalse;

	base = employee->baseHired;

	/* Crashed aircraft no longer belongs to any base but poor pilot/soldiers assigned
	 * to it are definetly away from the base so we need to iterate trought all aircraft */
	AIR_Foreach(aircraft) {
		if (aircraft->homebase != base)
			continue;
		if (!AIR_IsAircraftInBase(aircraft) && AIR_IsEmployeeInAircraft(employee, aircraft))
			return qtrue;
	}
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
		num = min(num, E_CountByType(employeeType));
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
			CAP_AddCurrent(oldBase, CAP_EMPLOYEES, -1);
			CAP_AddCurrent(newBase, CAP_EMPLOYEES, 1);
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
const char* E_GetEmployeeString (employeeType_t type, int n)
{
	switch (type) {
	case EMPL_SOLDIER:
		return ngettext("Soldier", "Soldiers", n);
	case EMPL_SCIENTIST:
		return ngettext("Scientist", "Scientists", n);
	case EMPL_WORKER:
		return ngettext("Worker", "Workers", n);
	case EMPL_PILOT:
		return ngettext("Pilot", "Pilots", n);
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

	if (Q_streq(type, "EMPL_SCIENTIST"))
		return EMPL_SCIENTIST;
	else if (Q_streq(type, "EMPL_SOLDIER"))
		return EMPL_SOLDIER;
	else if (Q_streq(type, "EMPL_WORKER"))
		return EMPL_WORKER;
	else if (Q_streq(type, "EMPL_PILOT"))
		return EMPL_PILOT;

	return MAX_EMPL;
}

/**
 * @brief Clears the employees list for loaded and new games
 * @sa CP_ResetCampaignData
 * @sa E_DeleteEmployee
 */
void E_ResetEmployees (void)
{
	int i;

	Com_DPrintf(DEBUG_CLIENT, "E_ResetEmployees: Delete all employees\n");
	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++)
		LIST_Delete(&ccs.employees[i]);
}

/**
 * @brief Return a list of hired employees in the given base of a given type
 * @param[in] base Which base the employee should be searched in. If NULL is given employees in all bases will be listed.
 * @param[in] type Which employee type to search for.
 * @param[out] hiredEmployees Linked list of hired employees in the base.
 * @return Number of hired employees in the base that are currently not on a transfer. Or @c -1 in case of an error.
 */
int E_GetHiredEmployees (const base_t* const base, employeeType_t type, linkedList_t **hiredEmployees)
{
	if (type >= MAX_EMPL) {
		Com_Printf("E_GetHiredEmployees: Unknown EmployeeType: %i\n", type);
		*hiredEmployees = NULL;
		return -1;
	}

	LIST_Delete(hiredEmployees);

	E_Foreach(type, employee) {
		if (!E_IsHired(employee))
			continue;
		if (!employee->transfer && (!base || E_IsInBase(employee, base))) {
			LIST_AddPointer(hiredEmployees, employee);
		}
	}

	if (hiredEmployees == NULL)
		return 0;
	return LIST_Count(*hiredEmployees);
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
	E_Foreach(type, employee) {
		if (!E_IsInBase(employee, base))
			continue;
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
	E_Foreach(type, employee) {
		if (!E_IsInBase(employee, base))
			continue;
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
 */
qboolean E_HireEmployee (base_t* base, employee_t* employee)
{
	if (CAP_GetFreeCapacity(base, CAP_EMPLOYEES) <= 0) {
		CP_Popup(_("Not enough quarters"), _("You don't have enough quarters for your employees.\nBuild more quarters."));
		return qfalse;
	}

	if (employee) {
		/* Now uses quarter space. */
		employee->baseHired = base;
		/* Update other capacities */
		switch (employee->type) {
		case EMPL_WORKER:
			CAP_AddCurrent(base, CAP_EMPLOYEES, 1);
			PR_UpdateProductionCap(base);
			break;
		case EMPL_PILOT:
			AIR_AutoAddPilotToAircraft(base, employee);
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			CAP_AddCurrent(base, CAP_EMPLOYEES, 1);
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
	employee_t* employee = E_GetUnhired(type);
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
	cgi->INV_DestroyInventory(&employee->chr.i);
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
			CAP_AddCurrent(base, CAP_EMPLOYEES, -1);
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
	if (!base)
		return;

	assert(type < MAX_EMPL);

	E_Foreach(type, employee) {
		if (!E_IsInBase(employee, base))
			continue;
		E_UnhireEmployee(employee);
	}
}

/**
 * @brief Creates an entry of a new employee in the global list and assignes it to no building/base.
 * @param[in] type What type of employee to create.
 * @param[in] nation What nation the employee (mainly used for soldiers in singleplayer) comes from.
 * @return Pointer to the newly created employee in the global list. NULL if something goes wrong.
 * @sa E_DeleteEmployee
 */
employee_t* E_CreateEmployee (employeeType_t type, const nation_t *nation)
{
	employee_t employee;
	const char *teamID;
	char teamDefName[MAX_VAR];
	const char* rank;

	if (type >= MAX_EMPL)
		return NULL;

	OBJZERO(employee);

	employee.baseHired = NULL;
	employee.assigned = qfalse;
	employee.type = type;
	employee.nation = nation;

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
	default:
		Com_Error(ERR_DROP, "E_CreateEmployee: Unknown employee type\n");
	}

	cgi->CL_GenerateCharacter(&employee.chr, teamDefName);
	employee.chr.score.rank = CL_GetRankIdx(rank);

	Com_DPrintf(DEBUG_CLIENT, "Generate character for type: %i\n", type);

	return (employee_t*) LIST_Add(&ccs.employees[type], (void*) &employee, sizeof(employee))->data;
}

/**
 * @brief Removes the employee completely from the game (buildings + global list).
 * @param[in] employee The pointer to the employee you want to remove.
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
	return LIST_Remove(&ccs.employees[type], (void*) employee);
}

/**
 * @brief Removes all employees completely from the game (buildings + global list) from a given base.
 * @note Used if the base e.g is destroyed by the aliens.
 * @param[in] base Which base the employee should be fired from.
 */
void E_DeleteAllEmployees (base_t* base)
{
	employeeType_t type;

	for (type = EMPL_SOLDIER; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			if (base == NULL || E_IsInBase(employee, base))
				E_DeleteEmployee(employee);
		}
	}
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
	int type;

	/* Check if there are too many employees */
	if (CAP_GetFreeCapacity(base, CAP_EMPLOYEES) >= 0)
		return;

	/* do a reverse loop in order to finish by soldiers (the most important employees) */
	for (type = MAX_EMPL - 1; type >= 0; type--) {

		E_Foreach(type, employee) {
			if (E_IsInBase(employee, base))
				E_DeleteEmployee(employee);

			if (CAP_GetFreeCapacity(base, CAP_EMPLOYEES) >= 0)
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
int E_RefreshUnhiredEmployeeGlobalList (const employeeType_t type, const qboolean excludeUnhappyNations)
{
	const nation_t *happyNations[MAX_NATIONS];
	int numHappyNations = 0;
	int idx, nationIdx, cnt;

	happyNations[0] = NULL;
	/* get a list of nations,  if excludeHappyNations is qtrue then also exclude
	 * unhappy nations (unhappy nation: happiness <= 0) from the list */
	for (idx = 0; idx < ccs.numNations; idx++) {
		const nation_t *nation = NAT_GetNationByIDX(idx);
		const nationInfo_t *stats = NAT_GetCurrentMonthInfo(nation);
		if (stats->happiness > 0 || !excludeUnhappyNations) {
			happyNations[numHappyNations] = nation;
			numHappyNations++;
		}
	}

	if (!numHappyNations)
		return 0;

	idx = 0;
	/* Fill the global data employee list with employees, evenly distributed
	 * between nations in the happyNations list */
	E_Foreach(type, employee) {
		/* we don't want to overwrite employees that have already been hired */
		if (!E_IsHired(employee)) {
			E_DeleteEmployee(employee);
			idx++;
		}
	}

	nationIdx = 0;
	cnt = 0;
	while (idx-- > 0) {
		if (E_CreateEmployee(type, happyNations[nationIdx]) != NULL)
			cnt++;
		nationIdx = (nationIdx + 1) % numHappyNations;
	}

	return cnt;
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
		if (employee->type != EMPL_SOLDIER && employee->type != EMPL_PILOT)
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

	E_Foreach(type, employee) {
		if (!E_IsHired(employee))
			continue;
		if (!base || E_IsInBase(employee, base))
			count++;
	}
	return count;
}

/**
 * @brief Counts all hired employees of a given base
 * @param[in] base The base where we count
 * @return count of hired employees of a given type in a given base
 * @note must not return 0 if hasBuilding[B_QUARTER] is qfalse: used to update capacity
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

	E_Foreach(type, employee) {
		if (!E_IsHired(employee))
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

	if (!base)
		return 0;

	count = 0;

	E_Foreach(type, employee) {
		if (!E_IsInBase(employee, base))
			continue;
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
	return NAT_GetNationByIDX(nationIndex);
}

/**
 * @brief Create initial hireable employees
 */
void E_InitialEmployees (const campaign_t *campaign)
{
	int i;

	/* setup initial employee count */
	for (i = 0; i < campaign->soldiers; i++)
		E_CreateEmployee(EMPL_SOLDIER, E_RandomNation());
	for (i = 0; i < campaign->scientists; i++)
		E_CreateEmployee(EMPL_SCIENTIST, E_RandomNation());
	for (i = 0; i < campaign->workers; i++)
		E_CreateEmployee(EMPL_WORKER, E_RandomNation());
	for (i = 0; i < campaign->pilots; i++)
		E_CreateEmployee(EMPL_PILOT, E_RandomNation());
}

#ifdef DEBUG
/**
 * @brief Debug command to list all hired employee
 */
static void E_ListHired_f (void)
{
	employeeType_t emplType;

	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		E_Foreach(emplType, employee) {
			Com_Printf("Employee: %s (ucn: %i) %s at %s\n", E_GetEmployeeString(employee->type, 1), employee->chr.ucn,
					employee->chr.name, employee->baseHired->name);
			if (employee->type != emplType)
				Com_Printf("Warning: EmployeeType mismatch: %i != %i\n", emplType, employee->type);
		}
	}
}

/**
 * @brief Debug function to add 5 new unhired employees of each type
 * @note called with debug_addemployees
 */
static void CL_DebugNewEmployees_f (void)
{
	int j;
	nation_t *nation = &ccs.nations[0];	/**< This is just a debugging function, nation does not matter */

	for (j = 0; j < 5; j++)
		/* Create a scientist */
		E_CreateEmployee(EMPL_SCIENTIST, nation);

	for (j = 0; j < 5; j++)
		/* Create a pilot. */
		E_CreateEmployee(EMPL_PILOT, nation);

	for (j = 0; j < 5; j++)
		/* Create a soldier. */
		E_CreateEmployee(EMPL_SOLDIER, nation);

	for (j = 0; j < 5; j++)
		/* Create a worker. */
		E_CreateEmployee(EMPL_WORKER, nation);
}
#endif

/**
 * @brief Searches employee from a type for the ucn (character id)
 * @param[in] type employee type
 * @param[in] uniqueCharacterNumber unique character number (UCN)
 */
employee_t* E_GetEmployeeByTypeFromChrUCN (employeeType_t type, int uniqueCharacterNumber)
{
	E_Foreach(type, employee) {
		if (employee->chr.ucn == uniqueCharacterNumber)
			return employee;
	}

	return NULL;
}

/**
 * @brief Searches all employee for the ucn (character id)
 * @param[in] uniqueCharacterNumber unique character number (UCN)
 */
employee_t* E_GetEmployeeFromChrUCN (int uniqueCharacterNumber)
{
	employeeType_t emplType;

	for (emplType = EMPL_SOLDIER; emplType < MAX_EMPL; emplType++) {
		employee_t *employee = E_GetEmployeeByTypeFromChrUCN(emplType, uniqueCharacterNumber);
		if (employee)
			return employee;
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
qboolean E_SaveXML (xmlNode_t *p)
{
	employeeType_t emplType;

	Com_RegisterConstList(saveEmployeeConstants);
	for (emplType = 0; emplType < MAX_EMPL; emplType++) {
		xmlNode_t *snode = XML_AddNode(p, SAVE_EMPLOYEE_EMPLOYEES);

		XML_AddString(snode, SAVE_EMPLOYEE_TYPE, Com_GetConstVariable(SAVE_EMPLOYEETYPE_NAMESPACE, emplType));
		E_Foreach(emplType, employee) {
			xmlNode_t * chrNode;
			xmlNode_t *ssnode = XML_AddNode(snode, SAVE_EMPLOYEE_EMPLOYEE);

			/** @note e->transfer is not saved here because it'll be restored via TR_Load. */
			if (employee->baseHired)
				XML_AddInt(ssnode, SAVE_EMPLOYEE_BASEHIRED, employee->baseHired->idx);
			if (employee->assigned)
				XML_AddBool(ssnode, SAVE_EMPLOYEE_ASSIGNED, employee->assigned);
			/* Store the nations identifier string. */
			assert(employee->nation);
			XML_AddString(ssnode, SAVE_EMPLOYEE_NATION, employee->nation->id);
			/* Character Data */
			chrNode = XML_AddNode(ssnode, SAVE_EMPLOYEE_CHR);
			GAME_SaveCharacter(chrNode, &employee->chr);
		}
	}
	Com_UnregisterConstList(saveEmployeeConstants);

	return qtrue;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] p XML Node structure, where we get the information from
 */
qboolean E_LoadXML (xmlNode_t *p)
{
	xmlNode_t * snode;
	qboolean success = qtrue;

	Com_RegisterConstList(saveEmployeeConstants);
	for (snode = XML_GetNode(p, SAVE_EMPLOYEE_EMPLOYEES); snode;
			snode = XML_GetNextNode(snode, p , SAVE_EMPLOYEE_EMPLOYEES)) {
		xmlNode_t * ssnode;
		employeeType_t emplType;
		const char *type = XML_GetString(snode, SAVE_EMPLOYEE_TYPE);

		if (!Com_GetConstIntFromNamespace(SAVE_EMPLOYEETYPE_NAMESPACE, type, (int*) &emplType)) {
			Com_Printf("Invalid employee type '%s'\n", type);
			continue;
		}

		for (ssnode = XML_GetNode(snode, SAVE_EMPLOYEE_EMPLOYEE); ssnode;
				ssnode = XML_GetNextNode(ssnode, snode, SAVE_EMPLOYEE_EMPLOYEE)) {
			int baseIDX;
			xmlNode_t * chrNode;
			employee_t e;

			OBJZERO(e);
			/** @note e->transfer is restored in cl_transfer.c:TR_Load */
			e.type = emplType;
			/* base */
			assert(B_AtLeastOneExists());	/* Just in case the order is ever changed. */
			baseIDX = XML_GetInt(ssnode, SAVE_EMPLOYEE_BASEHIRED, -1);
			e.baseHired = B_GetBaseByIDX(baseIDX);
			/* assigned to a building? */
			e.assigned = XML_GetBool(ssnode, SAVE_EMPLOYEE_ASSIGNED, qfalse);
			/* nation */
			e.nation = NAT_GetNationByID(XML_GetString(ssnode, SAVE_EMPLOYEE_NATION));
			if (!e.nation) {
				Com_Printf("No nation defined for employee\n");
				success = qfalse;
				break;
			}
			/* Character Data */
			chrNode = XML_GetNode(ssnode, SAVE_EMPLOYEE_CHR);
			if (!chrNode) {
				Com_Printf("No character definition found for employee\n");
				success = qfalse;
				break;
			}
			if (!GAME_LoadCharacter(chrNode, &e.chr)) {
				Com_Printf("Error loading character definition for employee\n");
				success = qfalse;
				break;
			}
			LIST_Add(&ccs.employees[emplType], (void*) &e, sizeof(e));
		}
		if (!success)
			break;
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
				B_UpdateStorageAndCapacity(employee->baseHired, invList->item.m, -1, qfalse);
			/* Remove Item */
			if (invList->item.t)
				B_UpdateStorageAndCapacity(employee->baseHired, invList->item.t, -1, qfalse);

			invList = invList->next;
		}
	}
}

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 */
void E_InitStartup (void)
{
	E_InitCallbacks();
#ifdef DEBUG
	Cmd_AddCommand("debug_listhired", E_ListHired_f, "Debug command to list all hired employee");
	Cmd_AddCommand("debug_addemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type");
#endif
}

/**
 * @brief Closing actions for employee-subsystem
 */
void E_Shutdown (void)
{
	employeeType_t employeeType;

	for (employeeType = EMPL_SOLDIER; employeeType < MAX_EMPL; employeeType++)
		LIST_Delete(&ccs.employees[employeeType]);

	E_ShutdownCallbacks();
#ifdef DEBUG
	Cmd_RemoveCommand("debug_listhired");
	Cmd_RemoveCommand("debug_addemployees");
#endif
}
