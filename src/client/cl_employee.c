/**
 * @file cl_employee.c
 * @brief Single player employee stuff.
 * @note Employee related functions prefix: E_
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

#include "client.h"
#include "cl_global.h"
#include "cl_team.h"
#include "cl_hospital.h"
#include "cl_actor.h"
#include "menu/m_popup.h"

/* holds the current active employee category */
static int employeeCategory = 0;
/* the menu node of the employee list */
static menuNode_t *employeeListNode;

/* how many employees in current list (changes on every catergory change, too) */
int employeesInCurrentList = 0;

/*****************************************************
VISUAL/GUI STUFF
*****************************************************/

/**
 * @brief Click function for employee_list node
 * @sa E_EmployeeList_f
 */
static void E_EmployeeListClick_f (void)
{
	int num; /* clicked at which position? determined by node format string */

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	if (num < 0 || num >= employeesInCurrentList)
		return;

	/* the + indicates, that values bigger than cl_numnames could be possible */
	Cbuf_AddText(va("employee_select +%i\n", num));
}

/**
 * @brief Click function for employee_list node
 * @sa E_EmployeeList_f
 */
static void E_EmployeeListScroll_f (void)
{
	int i, j, cnt = 0;
	employee_t* employee;

	j = employeeListNode->textScroll;

	for (i = 0, employee = &(gd.employees[employeeCategory][0]); i < gd.numEmployees[employeeCategory]; i++, employee++) {
		/* don't show employees of other bases */
		if (employee->baseIDHired != baseCurrent->idx && employee->hired)
			continue;

		/* drop the first j entries */
		if (j) {
			j--;
			continue;
		}
		/* change the buttons */
		if (employee->hired) {
			if (employee->baseIDHired == baseCurrent->idx)
				Cbuf_AddText(va("employeeadd%i\n", cnt));
			else
				Cbuf_AddText(va("employeedisable%i\n", cnt));
		} else
			Cbuf_AddText(va("employeedel%i\n", cnt));

		cnt++;

		/* only 19 buttons */
		if (cnt >= cl_numnames->integer)
			break;
	}

	for (;cnt < cl_numnames->integer; cnt++) {
		Cvar_ForceSet(va("mn_name%i", cnt), "");
		Cbuf_AddText(va("employeedisable%i\n", cnt));
	}
}

/**
 * @brief Will fill the list with employees
 * @note this is the init function in employee menu
 */
static void E_EmployeeList_f (void)
{
	int i, j;
	employee_t* employee;
	int employeeIdx;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category> <employeeid>\n", Cmd_Argv(0));
		return;
	}

	employeeCategory = atoi(Cmd_Argv(1));
	if (employeeCategory >= MAX_EMPL || employeeCategory < 0)
		employeeCategory = EMPL_SOLDIER;

	if (Cmd_Argc() == 3)
		employeeIdx = atoi(Cmd_Argv(2));
	else
		employeeIdx = -1;

	/* reset the employee count */
	employeesInCurrentList = 0;

	if (employeeIdx < 0) {
		/* Reset scrolling when no specific entry was given. */
		/** @todo Is there a case where employeeIdx is <0 and the textScroll must be reset? */
		employeeListNode->textScroll = 0;
	} else {
		/** @todo If employee is given but outside the current visible list (defined by employeeListNode->textScroll) we need to calculate the new employeeListNode->textScroll */
	}

	/* make sure, that we are using the linked list */
	MN_MenuTextReset(TEXT_LIST);

	for (j = 0, employee = gd.employees[employeeCategory]; j < gd.numEmployees[employeeCategory]; j++, employee++) {
		/* don't show employees of other bases */
		if (employee->baseIDHired != baseCurrent->idx && employee->hired)
			continue;

		LIST_AddPointer(&mn.menuTextLinkedList[TEXT_LIST], employee->chr.name);
		/* Change/Display the buttons if the employee is currently displayed (i.e. visible in the on-screen list) .*/
		/** @todo Check if the "textScroll % cl_numnames->integer" calculation is still ok when _very_ long lists (i.e. more than 2x19 right now) are used. */
		if ((employeesInCurrentList >= employeeListNode->textScroll)
		&& (employeesInCurrentList < (employeeListNode->textScroll + cl_numnames->integer))) {
			if (employee->hired) {
				if (employee->baseIDHired == baseCurrent->idx) {
					Cvar_ForceSet(va("mn_name%i", employeesInCurrentList), employee->chr.name);
					Cbuf_AddText(va("employeeadd%i\n", employeesInCurrentList - (employeeListNode->textScroll % cl_numnames->integer)));
				} else
					Cbuf_AddText(va("employeedisable%i\n", employeesInCurrentList - (employeeListNode->textScroll % cl_numnames->integer)));
			} else
				Cbuf_AddText(va("employeedel%i\n", employeesInCurrentList));
		}
		employeesInCurrentList++;
	}

	/* If the list is empty OR we are in medics/scientists/workers-mode: don't show the model&stats. */
	/** @note
	 * 0 == nothing is displayed
	 * 1 == all is displayed
	 * 2 == only stuff wanted for scientists/workers/medics are displayed
	 */
	if (employeesInCurrentList == 0) {
		Cvar_Set("mn_show_employee", "0");
	} else {
		if ((employeeCategory == EMPL_MEDIC)
		 || (employeeCategory == EMPL_SCIENTIST)
		 || (employeeCategory == EMPL_WORKER)) {
			Cvar_Set("mn_show_employee", "2");
		} else {
			Cvar_Set("mn_show_employee", "1");
		}
	}

	i = employeesInCurrentList;
	for (;i < cl_numnames->integer;i++) {
		Cvar_ForceSet(va("mn_name%i", i), "");
		Cbuf_AddText(va("employeedisable%i\n", i));
	}

	/* Select the current employee if name was changed or first one. Use the direct string
	 * execution here - otherwise the employeeCategory might be out of sync */
	if (employeeIdx < 0)
		Cmd_ExecuteString("employee_select 0\n");
	else
		Cmd_ExecuteString(va("employee_select %i;", Cvar_VariableInteger("mn_employee_idx")));
}


/*****************************************************
EMPLOYEE BACKEND STUFF
*****************************************************/

/**
 * @brief Checks whether the given employee is in the given base
 */
qboolean E_IsInBase (employee_t* empl, const base_t* const base)
{
	if (empl->baseIDHired == base->idx)
		return qtrue;
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
	case EMPL_MEDIC:
		return _("Medic");
	case EMPL_ROBOT:
		return _("UGV");
	default:
		Sys_Error("Unknown employee type '%i'\n", type);
		return "";
	}
}

/**
 * @brief Convert string to employeeType_t
 * @param type Pointer to employee type string
 * @return employeeType_t
 */
employeeType_t E_GetEmployeeType (const char* type)
{
	assert(type);
	if (!Q_strncmp(type, "EMPL_SCIENTIST", 14))
		return EMPL_SCIENTIST;
	else if (!Q_strncmp(type, "EMPL_SOLDIER", 12))
		return EMPL_SOLDIER;
	else if (!Q_strncmp(type, "EMPL_WORKER", 11))
		return EMPL_WORKER;
	else if (!Q_strncmp(type, "EMPL_MEDIC", 10))
		return EMPL_MEDIC;
	else if (!Q_strncmp(type, "EMPL_ROBOT", 10))
		return EMPL_ROBOT;
	else {
		Sys_Error("Unknown employee type '%s'\n", type);
		return MAX_EMPL; /* never reached */
	}
}

/**
 * @brief Set the employeelist node for faster lookups
 * @sa CL_InitAfter
 */
void E_Init (void)
{
	menu_t* menu = MN_GetMenu("employees");
	if (!menu)
		Sys_Error("Could not find the employees menu\n");

	employeeListNode = MN_GetNode(menu, "employee_list");
	if (!employeeListNode)
		Sys_Error("Could not find the employee_list node in employees menu\n");
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
		if (gd.numEmployees[i]) {
			memset(gd.employees[i], 0, sizeof(gd.employees[i]));
			gd.numEmployees[i] = 0;
		}
}

#if 0
/**
 * @brief Returns true if the employee is only assigned to quarters but not to labs and workshops.
 * @param[in] employee The employee_t pointer to check
 * @return qboolean
 * @sa E_EmployeeIsUnassigned
 */
static qboolean E_EmployeeIsFree (employee_t * employee)
{
	if (!employee)
		Sys_Error("E_EmployeeIsFree: Employee is NULL.\n");

	return (employee->buildingID < 0 && employee->hired);
}
#endif

/**
 * @brief Return a given employee pointer in the given base of a given type.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type Which employee type do we search.
 * @param[in] idx Which employee id (in global employee array)
 * @return employee_t pointer or NULL
 */
employee_t* E_GetEmployee (const base_t* const base, employeeType_t type, int idx)
{
	int i;

	if (!base || type >= MAX_EMPL || idx < 0)
		return NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		if (i == idx && (!gd.employees[type][i].hired || gd.employees[type][i].baseIDHired == base->idx))
			return &gd.employees[type][i];
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
	int i;
	int j = -1;	/* The number of found unhired employees. Ignore the minus. */
	employee_t *employee;

	if (type >= MAX_EMPL) {
		Com_Printf("E_GetUnhiredEmployee: Unknown EmployeeType: %i\n", type);
		return NULL;
	}

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];

		if (i == idx) {
			if (employee->hired) {
				Com_Printf("E_GetUnhiredEmployee: Warning: employee is already hired!\n");
				return NULL;
			}
			return employee;
		} else if (idx < 0 && !employee->hired) {
			if (idx == j)
				return employee;
			j--;
		}
	}
	Com_Printf("Could not get unhired employee with index: %i of type %i (available: %i)\n", idx, type, gd.numEmployees[type]);
	return NULL;
}

/**
 * @brief Return a "not hired" ugv-employee pointer of a given ugv-type.
 * @param[in] ugvType What type of robot we want.
 * @param[in] hired Do we want a hire or unhired robot?
 * @sa E_GetHiredRobot
 */
employee_t* E_GetUnhiredRobot (const ugv_t *ugvType)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		employee = &gd.employees[EMPL_ROBOT][i];

		/* If no type was given we return the first ugv we find. */
		if (!ugvType)
			return employee;

		if (employee->ugv == ugvType && !employee->hired)
			return employee;
	}
	Com_Printf("Could not get unhired ugv/robot.\n");
	return NULL;
}

/**
 * @brief Return a given hired employee pointer in the given base of a given type
 * @param[in] base Which base the employee should be searched in.
 * @param[in] type Which employee type to search for.
 * @param[in] idx Which employee id (in global employee array). Use -1, -2, etc.. to return the first/ second, etc... "hired" employee.
 * @return employee_t pointer or NULL
 * @sa E_GetUnhiredEmployee
 * @sa E_HireEmployee
 * @sa E_UnhireEmployee
 * @sa E_GetHiredCharacter
 */
employee_t* E_GetHiredEmployee (const base_t* const base, employeeType_t type, int idx)
{
	int i;
	int j = -1;	/* The number of found hired employees. Ignore the minus. */
	employee_t *employee;

	if (!base)
		return NULL;

	if (type >= MAX_EMPL) {
		Com_Printf("E_GetHiredEmployee: Unknown EmployeeType: %i\n", type);
		return NULL;
	}

#ifdef PARANOID
	i = E_CountHired(base, type);
	if (idx < 0) {
		if (i < abs(idx))
			Sys_Error("Trying to get the %ith employee - but you only have %i hired in the base.\n", abs(idx), i);
	} else {
		if (gd.employees[type][idx].baseIDHired != base->idx)
			Sys_Error("Trying to get hired employee %i (base %i) in base %i - but he is not hired in this base.\n", idx, gd.employees[type][idx].baseIDHired, base->idx);
	}
#endif

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->baseIDHired == base->idx) {
			if (i == idx)
				return employee;
			if (idx < 0) {
				if (idx == j)
					return employee;
				j--;
			}
		} else
			continue;
	}
	return NULL;
}

/**
 * @brief Return a "hired" ugv-employee pointer of a given ugv-type in a given base.
 * @param[in] base Which base the ugv should be searched in.c
 * @param[in] ugvType What type of robot we want.
 * @sa E_GetUnhiredRobot
 */
employee_t* E_GetHiredRobot (const base_t* const base, const ugv_t *ugvType)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		employee = &gd.employees[EMPL_ROBOT][i];

		/* If no type was given we return the first ugv we find. */
		if (!ugvType)
			return employee;

		if (((employee->ugv == ugvType) || !ugvType)
		&& (employee->baseIDHired == base->idx)) {
			assert(employee->hired);
			return employee;
		}
	}
	Com_Printf("Could not get unhired ugv/robot.\n");
	return NULL;
}

/**
 * @brief Return a given character pointer of a hired employee in the given base of a given type
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] employee_idx Which employee idx (index in global employee array)
 * @return character_t pointer or NULL
 * @sa E_GetHiredEmployee
 */
character_t* E_GetHiredCharacter (const base_t* const base, employeeType_t type, int employee_idx)
{
	employee_t* employee = E_GetHiredEmployee(base, type, employee_idx);	 /* Parameter sanity is checked here. */
	if (employee)
		return &(employee->chr);

	return NULL;
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
		Sys_Error("E_EmployeeIsUnassigned: Employee is NULL.\n");

	return (employee->buildingID < 0);
}

/**
 * @brief Gets an unassigned employee of a given type from the given base.
 *
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassigned
 * @sa E_EmployeesInBase
 * @note assigned is not hired - they are already hired in a base, in a quarter _and_ working in another building.
 */
employee_t* E_GetAssignedEmployee (const base_t* const base, employeeType_t type)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->baseIDHired == base->idx
			&& !E_EmployeeIsUnassigned(employee))
			return employee;
	}
	return NULL;
}

/**
 * @brief Gets an assigned employee of a given type from the given base.
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassigned
 * @sa E_EmployeesInBase
 * @note unassigned is not unhired - they are already hired in a base but are at quarters
 */
employee_t* E_GetUnassignedEmployee (const base_t* const base, employeeType_t type)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->baseIDHired == base->idx
			&& E_EmployeeIsUnassigned(employee))
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
		MN_Popup(_("Not enough quarters"), _("You don't have enough quarters for your employees.\nBuild more quarters."));
		return qfalse;
	}

	if (employee) {
		/* Now uses quarter space. */
		employee->hired = qtrue;
		employee->baseIDHired = base->idx;
		/* Update capacity. */
		base->capacities[CAP_EMPLOYEES].cur++;
		switch (employee->type) {
		case EMPL_WORKER:
			if (base->capacities[CAP_WORKSPACE].cur < base->capacities[CAP_WORKSPACE].max)
				base->capacities[CAP_WORKSPACE].cur++;
			break;
		/* @todo special cases for robots/ugvs? */
		case EMPL_ROBOT:
			break;
		/* don't handle these types here */
		case EMPL_MEDIC:
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			break;
		/* shut up compiler */
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
 * @param[in] idx Which employee id (in global employee array) See E_GetUnhiredEmployee for usage.
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
 * @param[in] ugvType What type of ugv/robot whoudl be hired.
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
 * and is also used when you completly unhire an employee.
 * @sa E_UnhireEmployee
 */
void E_ResetEmployee (employee_t *employee)
{
	base_t* base;

	assert(employee);
	assert(employee->hired);
	assert(employee->baseIDHired != -1);

	base = B_GetBase(employee->baseIDHired);

	/* Remove employee from building (and tech/production). */
	E_RemoveEmployeeFromBuilding(employee);
	/* Destroy the inventory of the employee (carried items will remain in base->storage) */
	INVSH_DestroyInventory(&employee->inv);
	/* Remove employee from hospital if needed */
	HOS_RemoveFromList(employee, base);
}

/**
 * @brief Fires an employee.
 * @note also remove him from the aircraft
 * @param[in] employee The employee who will be fired
 * @sa E_HireEmployee
 * @sa E_HireEmployeeByType
 * @sa CL_RemoveSoldierFromAircraft
 * @sa E_ResetEmployee
 * @todo handle EMPL_ROBOT capacities here?
 */
qboolean E_UnhireEmployee (employee_t* employee)
{
	if (employee && employee->hired) {
		base_t *base = B_GetBase(employee->baseIDHired);
		/* Update capacity of Living Quarters. */
		base->capacities[CAP_EMPLOYEES].cur--;

		E_ResetEmployee(employee);
		/* Set all employee-tags to 'unhired'. */
		employee->hired = qfalse;
		employee->baseIDHired = -1;

		return qtrue;
	} else
		Com_Printf("Could not fire employee\n");
	return qfalse;
}

/**
 * @brief Reset the hired flag for all employees of a given type in a given base
 * @param[in] base Which base the employee should be fired from.
 * @param[in] type Which employee type do we search.
 */
void E_UnhireAllEmployees (base_t* base, employeeType_t type)
{
	int i;
	employee_t *employee;

	if (!base)
		return;

	assert(type >= 0);
	assert(type < MAX_EMPL);

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->hired && employee->baseIDHired == base->idx)
			E_UnhireEmployee(employee);
	}
}

/**
 * @brief Creates an entry of a new employee in the global list and assignes it to no building/base.
 * @param[in] type Tell the function what type of employee to create.
 * @param[in] type Tell the function what nation the employee (mainly used for soldiers in singleplayer) comes from.
 * @return Pointer to the newly created employee in the global list. NULL if something goes wrong.
 * @sa E_DeleteEmployee
 */
employee_t* E_CreateEmployee (employeeType_t type, nation_t *nation, ugv_t *ugvType)
{
	employee_t* employee;

	if (type >= MAX_EMPL)
		return NULL;

	if (gd.numEmployees[type] >= MAX_EMPLOYEES) {
		Com_DPrintf(DEBUG_CLIENT, "E_CreateEmployee: MAX_EMPLOYEES exceeded for type %i\n", type);
		return NULL;
	}

	employee = &gd.employees[type][gd.numEmployees[type]];
	memset(employee, 0, sizeof(employee_t));

	employee->idx = gd.numEmployees[type];
	employee->hired = qfalse;
	employee->baseIDHired = -1;
	employee->buildingID = -1;
	employee->type = type;
	employee->nation = nation;

	switch (type) {
	case EMPL_SOLDIER:
		CL_GenerateCharacter(employee, cl_team->string, type, NULL);
		break;
	case EMPL_SCIENTIST:
	case EMPL_MEDIC:
	case EMPL_WORKER:
		CL_GenerateCharacter(employee, cl_team->string, type, NULL);
		employee->speed = 100;
		break;
	case EMPL_ROBOT:
		if (!ugvType) {
			Com_DPrintf(DEBUG_CLIENT, "E_CreateEmployee: No ugvType given!\n");
			return NULL;
		}
		CL_GenerateCharacter(employee, cl_team->string, type, ugvType);
		employee->ugv = ugvType;
		break;
	default:
		break;
	}
	gd.numEmployees[type]++;
	return employee;
}

/**
 * @brief Removes the employee completely from the game (buildings + global list).
 * @param[in] employee The pointer to the employee you want to remove.
 * @return True if the employee was removed successfully, otherwise false.
 * @sa E_CreateEmployee
 * @sa E_ResetEmployees
 * @sa E_UnhireEmployee
 */
qboolean E_DeleteEmployee (employee_t *employee, employeeType_t type)
{
	int i, idx;
	qboolean found = qfalse;

	if (!employee)
		return qfalse;

	idx = employee->idx;
	/* Fire the employee. This will also:
	 * 1) remove him from buildings&work
	 * 2) remove his inventory */

	if (employee->baseIDHired >= 0) {
		HOS_RemoveDeadEmployeeFromLists(employee);
		E_UnhireEmployee(employee);
	}

	/* Remove the employee from the global list. */
	for (i = 0; i < gd.numEmployees[type]; i++) {
		if (gd.employees[type][i].idx == employee->idx)
			found = qtrue;

		if (found) {
			if (i < MAX_EMPLOYEES - 1) { /* Just in case we have _that much_ employees. :) */
				/* Move all the following employees in the list one place forward and correct its index. */
				gd.employees[type][i] = gd.employees[type][i + 1];
				gd.employees[type][i].idx = i;
				gd.employees[type][i].chr.empl_idx = i;
				gd.employees[type][i].chr.inv = &gd.employees[type][i].inv;
			}
		}
	}
	if (found) {
		gd.numEmployees[type]--;

		if (type == EMPL_SOLDIER || type == EMPL_ROBOT) {
 			for (i = 0; i < gd.numAircraft; i++)
				AIR_DecreaseAircraftTeamIdxGreaterThan(AIR_AircraftGetFromIdx(i),idx, EMPL_SOLDIER);
		}
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
	employee_t *employee;

	if (!base)
		return;
	Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: starting ...\n");
	for (type = EMPL_SOLDIER; type < MAX_EMPL; type++) {
		Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: Removing empl-type %i | num %i\n", type, gd.numEmployees[type]);
		/* Attention:
		 * gd.numEmployees[type] is changed in E_DeleteAllEmployees! (it's decreased by 1 per call)
		 * For this reason we start this loop from the back of the empl-list. toward 0. */
		for (i = gd.numEmployees[type]-1; i >= 0; i--) {
			Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: %i\n", i);
			employee = &gd.employees[type][i];
			if (employee->baseIDHired == base->idx) {
				E_DeleteEmployee(employee, type);
				Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees:	 Removing empl.\n");
			} else if (employee->baseIDHired >= 0) {
				Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees:	 Not removing empl. (other base)\n");
			}
		}
	}
	Com_DPrintf(DEBUG_CLIENT, "E_DeleteAllEmployees: ... finished\n");
}

#if 0
/************************
@todo: Will later on be used in e.g RS_AssignScientist_f
*********************************/
/**
 * @brief Assigns an employee to a building.
 *
 * @param[in] building The building the employee is assigned to.
 * @param[in] employee_type	What type of employee to assign to the building.
 * @sa E_RemoveEmployeeFromBuilding
 * @return Returns true if adding was possible/sane otherwise false. In the later case nothing will be changed.
 */
qboolean E_AssignEmployeeToBuilding (building_t *building, employeeType_t type)
{
	employee_t * employee;

	switch (type) {
	case EMPL_SOLDIER:
		break;
	case EMPL_SCIENTIST:
		employee = E_GetUnassignedEmployee(B_GetBase(building->base_idx), type);
		if (employee) {
			employee->buildingID = building->idx;
		} else {
			/* @todo: message -> no employee available */
		}
		break;
	default:
		Com_DPrintf(DEBUG_CLIENT, "E_AssignEmployee: Unhandled employee type: %i\n", type);
		break;
	}
	return qfalse;
}
#endif

/**
 * @brief Remove one employee from building.
 * @todo Add check for base vs. employee_type and abort if they do not match.
 * @param[in] employee What employee to remove from its building.
 * @return Returns true if removing was possible/sane otherwise false.
 * @sa E_AssignEmployeeToBuilding
 */
qboolean E_RemoveEmployeeFromBuilding (employee_t *employee)
{
	technology_t *tech;
	base_t *base;

	assert(employee);

	/* not assigned to any building */
	/* FIXME: are soldiers assigned to a building, too? quarters? */
	if (employee->buildingID < 0 && employee->type != EMPL_SOLDIER)
		return qfalse;

	/* we can assume this because otherwise there should be no buildingID */
	assert(employee->baseIDHired != -1);
	base = B_GetBase(employee->baseIDHired);

	assert(employee->type == employee->chr.empl_type);

	switch (employee->type) {
	case EMPL_SCIENTIST:
		/* Update current capacity for lab if scientist is being counter there. */
		if (E_CountHired(base, employee->type) == base->capacities[CAP_LABSPACE].cur) {
			base->capacities[CAP_LABSPACE].cur--;
		}

		/* Get technology with highest scientist-count and remove one scientist. */
		if (employee->baseIDHired >= 0)
			tech = RS_GetTechWithMostScientists(B_GetBase(employee->baseIDHired));
		else
			tech = NULL;

		if (tech) {
			/* Try to assign replacement scientist */
			RS_AssignScientist(tech);
			RS_RemoveScientist(tech);
		} else {
			assert(employee->buildingID != -1);
		}
		break;

	case EMPL_SOLDIER:
		/* Remove soldier from aircraft/team if he was assigned to one. */
		if (CL_SoldierInAircraft(employee->idx, employee->type, -1)) {
			CL_RemoveSoldierFromAircraft(employee->idx, employee->type, -1, base);
		}
		break;

	case EMPL_MEDIC:
		/* @todo: Check if they are linked to anywhere and remove them there. */
		break;

	case EMPL_WORKER:
		/* Update current capacity and production times if worker is being counted there. */
		if (E_CountHired(base, employee->type) == base->capacities[CAP_WORKSPACE].cur) {
			base->capacities[CAP_WORKSPACE].cur--;
		}
		break;

	case EMPL_ROBOT:
		/* @todo: Check if they are linked to anywhere and remove them there. */
		break;

	/* otherwise the compiler would print a warning */
	case MAX_EMPL:
		break;
	}

	return qtrue;
}

/**
 * @brief Counts hired employees of a given type in a given base
 * @param[in] base The base where we count.
 * @param[in] type The type of employee to search.
 * @return count of hired employees of a given type in a given base
 */
int E_CountHired (const base_t* const base, employeeType_t type)
{
	int count = 0, i;
	employee_t *employee;

	if (!base)
		return 0;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->hired && employee->baseIDHired == base->idx)
			count++;
	}
	return count;
}

/**
 * @brief Counts 'hired' (i.e. bought or produced UGVs and other robots of a giben ugv-type in a given base.
 * @param[in] base The base where we count.
 * @param[in] ugvType What type of robot/ugv we are looking for.
 * @return Count of Robots/UGVs.
 */
int E_CountHiredRobotByType (const base_t* const base, const ugv_t *ugvType)
{
	int count = 0, i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		employee = &gd.employees[EMPL_ROBOT][i];
		if (employee->hired
		&&  employee->baseIDHired == base->idx
		&& employee->ugv == ugvType)
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
	int count = 0;

	if (!base)
		return 0;

	for (type = 0; type < MAX_EMPL; type++)
		count += E_CountHired(base, type);

	return count;
}

/**
 * @brief Counts unhired employees of a given type in a given base
 *
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 * @return count of hired employees of a given type in a given base
 */
int E_CountUnhired (employeeType_t type)
{
	int count = 0, i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (!employee->hired)
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
	int count = 0, i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[EMPL_ROBOT]; i++) {
		employee = &gd.employees[EMPL_ROBOT][i];
		if (!employee->hired && employee->ugv == ugvType)
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
	int count = 0, i;
	employee_t *employee;

	if (!base)
		return 0;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->buildingID < 0 && employee->baseIDHired == base->idx)
			count++;
	}
	return count;
}

/**
 * @brief Find an hired or free employee by the menu index
 * @param[in] num The index from the hire menu screen.
 * @param[in] category The employee category (e.g EMPL_SOLDIER)
 */
static inline employee_t* E_GetEmployeeByMenuIndex (const base_t* base, int num, int category)
{
	int i, j;
	employee_t* employee;

	assert(base);

	/* Some sanity checks (is category valid?). */
	if (category < EMPL_SOLDIER || category >= MAX_EMPL)
		return NULL;

	if (num >= employeesInCurrentList || num < 0)
		return NULL;

	for (i = 0, j = 0, employee = &(gd.employees[category][0]); i < gd.numEmployees[category]; i++, employee++) {
		/* don't count employees of other bases */
		if (employee->baseIDHired != base->idx && employee->hired)
			continue;

		if (num == j)
			return employee;

		j++;
	}
	return NULL;
}

/**
 * @brief This removes an employee from the global list so that
 * he/she is no longer hireable.
 */
static void E_EmployeeDelete_f (void)
{
	/* num - menu index (line in text), button - number of button */
	int num, button;
	employee_t* employee;

	if (!baseCurrent)
		return;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	button = num - employeeListNode->textScroll;

	employee = E_GetEmployeeByMenuIndex(baseCurrent, num, employeeCategory);
	/* empty slot selected */
	if (!employee)
		return;

	if (employee->hired) {
		if (!E_UnhireEmployee(employee)) {
			/* @todo: message - Couldn't fire employee. */
			Com_DPrintf(DEBUG_CLIENT, "Couldn't fire employee\n");
			return;
		}
	}
	E_DeleteEmployee(employee, employee->type);
	Cbuf_AddText(va("employee_init %i\n", employeeCategory));
}

/**
 * @brief Callback for employee_hire command
 * @sa CL_AssignSoldier_f
 */
static void E_EmployeeHire_f (void)
{
	/* num - menu index (line in text), button - number of button */
	int num, button;
	const char *arg;
	employee_t* employee;

	if (!baseCurrent)
		return;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <+num>\n", Cmd_Argv(0));
		return;
	}

	arg = Cmd_Argv(1);

	/* check whether this is called with the text node click function
	 * with values from 0 - #available employees (bigger values than
	 * cl_numnames [19]) possible ... */
	if (*arg == '+') {
		num = atoi(arg + 1);
		button = num - employeeListNode->textScroll;
	/* ... or with the hire pictures that are using only values from
	 * 0 - cl_numnames [19] */
	} else {
		button = atoi(Cmd_Argv(1));
		num = button + employeeListNode->textScroll;
	}

	employee = E_GetEmployeeByMenuIndex(baseCurrent, num, employeeCategory);
	/* empty slot selected */
	if (!employee)
		return;

	if (employee->hired) {
		if (!E_UnhireEmployee(employee)) {
			/* @todo: message - Couldn't fire employee. */
			Com_DPrintf(DEBUG_CLIENT, "Couldn't fire employee\n");
		} else
			Cbuf_AddText(va("employeedel%i\n", button));
	} else {
		if (!E_HireEmployee(baseCurrent, employee)) {
			/* @todo: message - Couldn't hire employee. */
			Com_DPrintf(DEBUG_CLIENT, "Couldn't hire employee\n");
		} else
			Cbuf_AddText(va("employeeadd%i\n", button));
	}
	Cbuf_AddText(va("employee_select %i\n", num));
}

/**
 * @brief Callback function that updates the character cvars when calling employee_select
 */
static void E_EmployeeSelect_f (void)
{
	int num;
	employee_t* employee;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= gd.numEmployees[employeeCategory])
		return;

	employee = E_GetEmployeeByMenuIndex(baseCurrent, num, employeeCategory);
	if (employee) {
		/* mn_employee_hired is needed to allow renaming */
		Cvar_SetValue("mn_employee_hired", employee->hired? 1: 0);

		/* set info cvars */
		CL_CharacterCvars(&(employee->chr));

		/* Set the value of the selected line in the scroll-text to the correct number. */
		employeeListNode->textLineSelected = num;
	}
}

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
void E_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("employee_init", E_EmployeeList_f, "Init function for employee hire menu");
	Cmd_AddCommand("employee_delete", E_EmployeeDelete_f, "Removed an employee from the global employee list");
	Cmd_AddCommand("employee_hire", E_EmployeeHire_f, NULL);
	Cmd_AddCommand("employee_select", E_EmployeeSelect_f, NULL);
	Cmd_AddCommand("employee_scroll", E_EmployeeListScroll_f, "Scroll callback for employee list");
	Cmd_AddCommand("employee_list_click", E_EmployeeListClick_f, "Callback for employee_list click function");
}

/**
 * @brief Searches all soldiers employees for the ucn (character id)
 */
employee_t* E_GetEmployeeFromChrUCN (int ucn)
{
	int i;

	/* MAX_EMPLOYEES and not numWholeTeam - maybe some other soldier died */
	for (i = 0; i < MAX_EMPLOYEES; i++)
		if (gd.employees[EMPL_SOLDIER][i].chr.ucn == ucn)
			return &(gd.employees[EMPL_SOLDIER][i]);

	return NULL;
}

/**
 * @brief Save callback for savegames
 * @sa E_Load
 * @sa SAV_GameSave
 * @sa G_SendCharacterData
 * @sa CL_ParseCharacterData
 * @sa CL_SendCurTeamInfo
 */
qboolean E_Save (sizebuf_t* sb, void* data)
{
	int i, j, k;
	employee_t* e;

	for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
		MSG_WriteShort(sb, gd.numEmployees[j]);
		for (i = 0; i < gd.numEmployees[j]; i++) {
			e = &gd.employees[j][i];
			MSG_WriteByte(sb, e->type);
			MSG_WriteByte(sb, e->hired);
			MSG_WriteShort(sb, e->idx);
			MSG_WriteShort(sb, e->baseIDHired);
			MSG_WriteShort(sb, e->buildingID);
			/* 2.3++ Store the nations identifier string. */
			if (e->nation)
				MSG_WriteString(sb, e->nation->id);
			else
				MSG_WriteString(sb, "NULL");

			/* 2.3++ Store the ugv-type identifier string. (Only exists for EMPL_ROBOT). */
			if (e->ugv)
				MSG_WriteString(sb, e->ugv->id);
			else
				MSG_WriteString(sb, "NULL");

			/* Store the character data */
			MSG_WriteString(sb, e->chr.name);
			MSG_WriteString(sb, e->chr.body);
			MSG_WriteString(sb, e->chr.path);
			MSG_WriteString(sb, e->chr.head);
			MSG_WriteByte(sb, e->chr.skin);
			MSG_WriteByte(sb, e->chr.armour);
			MSG_WriteByte(sb, e->chr.weapons);
			MSG_WriteByte(sb, e->chr.teamDef ? e->chr.teamDef->idx : NONE);
			MSG_WriteByte(sb, e->chr.gender);
			MSG_WriteShort(sb, e->chr.ucn);
			MSG_WriteShort(sb, e->chr.maxHP);
			MSG_WriteShort(sb, e->chr.HP);
			MSG_WriteByte(sb, e->chr.STUN);
			MSG_WriteByte(sb, e->chr.morale);
			MSG_WriteByte(sb, e->chr.fieldSize);

			/* Store reaction-firemode */
			MSG_WriteShort(sb, e->chr.RFmode.hand);
			MSG_WriteShort(sb, e->chr.RFmode.fmIdx);
			MSG_WriteShort(sb, e->chr.RFmode.wpIdx);

			/* Store reserved Tus and additional info (i.e. the cl_reserved_tus_t struct */
			MSG_WriteShort(sb, e->chr.reservedTus.reserveReaction);
			MSG_WriteShort(sb, e->chr.reservedTus.reaction);
			MSG_WriteShort(sb, e->chr.reservedTus.reserveCrouch);
			MSG_WriteShort(sb, e->chr.reservedTus.crouch);
			MSG_WriteShort(sb, e->chr.reservedTus.shot);
			MSG_WriteShort(sb, e->chr.reservedTus.shotSettings.hand);
			MSG_WriteShort(sb, e->chr.reservedTus.shotSettings.fmIdx);
			MSG_WriteShort(sb, e->chr.reservedTus.shotSettings.wpIdx);

#if 0 /** old way (pre 2.3) */
			MSG_WriteShort(sb, e->chr.assigned_missions);

			for (k = 0; k < presaveArray[PRE_KILLTP]; k++)
				MSG_WriteShort(sb, e->chr.kills[k]);
			for (k = 0; k < presaveArray[PRE_SKILTP]; k++)
				MSG_WriteShort(sb, e->chr.skills[k]);

			MSG_WriteByte(sb, e->chr.chrscore.alienskilled);
			MSG_WriteByte(sb, e->chr.chrscore.aliensstunned);
			MSG_WriteByte(sb, e->chr.chrscore.civilianskilled);
			MSG_WriteByte(sb, e->chr.chrscore.civiliansstunned);
			MSG_WriteByte(sb, e->chr.chrscore.teamkilled);
			MSG_WriteByte(sb, e->chr.chrscore.teamstunned);
			MSG_WriteByte(sb, e->chr.chrscore.closekills);
			MSG_WriteByte(sb, e->chr.chrscore.heavykills);
			MSG_WriteByte(sb, e->chr.chrscore.assaultkills);
			MSG_WriteByte(sb, e->chr.chrscore.sniperkills);
			MSG_WriteByte(sb, e->chr.chrscore.explosivekills);
			MSG_WriteByte(sb, e->chr.chrscore.accuracystat);
			MSG_WriteByte(sb, e->chr.chrscore.powerstat);
#endif

			/** Store character stats/score @sa inv_shared.h:chrScoreGlobal_t */
			for (k = 0; k < presaveArray[PRE_SKILTP]+1; k++)
				MSG_WriteLong(sb, e->chr.score.experience[k]);
			for (k = 0; k < presaveArray[PRE_SKILTP]; k++)
				MSG_WriteByte(sb, e->chr.score.skills[k]);
			for (k = 0; k < presaveArray[PRE_SKILTP]; k++)
				MSG_WriteByte(sb, e->chr.score.initialSkills[k]);
			for (k = 0; k < presaveArray[PRE_KILLTP]; k++)
				MSG_WriteShort(sb, e->chr.score.kills[k]);
			for (k = 0; k < presaveArray[PRE_KILLTP]; k++)
				MSG_WriteShort(sb, e->chr.score.stuns[k]);
			MSG_WriteShort(sb, e->chr.score.assignedMissions);
			MSG_WriteByte(sb, e->chr.score.rank);

			/* Store inventories */
			CL_SaveInventory(sb, &e->inv);
		}
	}

	return qtrue;
}

/**
 * @brief Load callback for savegames
 * @sa E_Save
 * @sa SAV_GameLoad
 */
qboolean E_Load (sizebuf_t* sb, void* data)
{
	int i, j, k;
	int td;	/**< Team-definition index */
	employee_t* e;

	/* load inventories */
	for (j = 0; j < presaveArray[PRE_EMPTYP]; j++) {
		gd.numEmployees[j] = MSG_ReadShort(sb);
		for (i = 0; i < gd.numEmployees[j]; i++) {
			e = &gd.employees[j][i];
			e->type = MSG_ReadByte(sb);
			if (e->type != j)
				Com_Printf("......error in loading employee - type values doesn't match\n");
			e->hired = MSG_ReadByte(sb);
			e->idx = MSG_ReadShort(sb);
			e->baseIDHired = MSG_ReadShort(sb);
			e->buildingID = MSG_ReadShort(sb);

			if (((saveFileHeader_t *)data)->version >= 3) { /* (2.3+) */
				/* Read the nations identifier string, get the matching nation_t pointer.
				 * We can do this because nations are already parsed .. will break if the parse-order is changed.
				 * Same for the ugv string below.
				 * We would need a Post-Load init funtion in that case. See cl_save.c:SAV_GameActionsAfterLoad */
				e->nation = CL_GetNationByID(MSG_ReadString(sb));
				/* Read the UGV-Type identifier and get the matching ugv_t pointer.  */
				e->ugv = CL_GetUgvById(MSG_ReadString(sb));
			}

			/* Load the character data */
			Q_strncpyz(e->chr.name, MSG_ReadStringRaw(sb), sizeof(e->chr.name));
			Q_strncpyz(e->chr.body, MSG_ReadString(sb), sizeof(e->chr.body));
			Q_strncpyz(e->chr.path, MSG_ReadString(sb), sizeof(e->chr.path));
			Q_strncpyz(e->chr.head, MSG_ReadString(sb), sizeof(e->chr.head));
			e->chr.skin = MSG_ReadByte(sb);
			e->chr.empl_idx = i;
			e->chr.empl_type = j;
			e->chr.armour = MSG_ReadByte(sb);
			e->chr.weapons = MSG_ReadByte(sb);
			e->chr.teamDef = NULL;
			td = MSG_ReadByte(sb);
			if (td != NONE)
				e->chr.teamDef = &csi.teamDef[td];
			e->chr.gender = MSG_ReadByte(sb);
			e->chr.ucn = MSG_ReadShort(sb);
			e->chr.maxHP = MSG_ReadShort(sb);
			e->chr.HP = MSG_ReadShort(sb);
			e->chr.STUN = MSG_ReadByte(sb);
			e->chr.morale = MSG_ReadByte(sb);

			if (((saveFileHeader_t *)data)->version < 3) {
				/** This was moved into chr->score for 2.3 and up */
				e->chr.score.rank = MSG_ReadByte(sb);
			}

			e->chr.fieldSize = MSG_ReadByte(sb);

			if (((saveFileHeader_t *)data)->version >= 3) {
				/* Load reaction-firemode */
				e->chr.RFmode.hand = MSG_ReadShort(sb);
				e->chr.RFmode.fmIdx = MSG_ReadShort(sb);
				e->chr.RFmode.wpIdx = MSG_ReadShort(sb);

				/* Read reserved Tus and additional info (i.e. the cl_reserved_tus_t struct */
				e->chr.reservedTus.reserveReaction = MSG_ReadShort(sb);
				e->chr.reservedTus.reaction = MSG_ReadShort(sb);
				e->chr.reservedTus.reserveCrouch = MSG_ReadShort(sb);
				e->chr.reservedTus.crouch = MSG_ReadShort(sb);
				e->chr.reservedTus.shot = MSG_ReadShort(sb);

				if (e->chr.reservedTus.shot == -1)	/** @todo Check for dummy value. I think it's save to remove this later on (e.g. before 2.3 release). */
					e->chr.reservedTus.shot = 0;
				e->chr.reservedTus.shotSettings.hand = MSG_ReadShort(sb);
				e->chr.reservedTus.shotSettings.fmIdx = MSG_ReadShort(sb);
				e->chr.reservedTus.shotSettings.wpIdx = MSG_ReadShort(sb);
			}

			if (((saveFileHeader_t *)data)->version < 3) {
				/* Load character stats/score (before 2.3) */
				e->chr.score.assignedMissions = MSG_ReadShort(sb);

				/* "experience[]" didn't exist in 2.2. This'll set them to the maximum expected values for 50 missions. */
				for (k = 0; k < presaveArray[PRE_SKILTP]+1; k++)
					e->chr.score.experience[k] = CHRSH_CharGetMaxExperiencePerMission(k) * 50;

				for (k = 0; k < presaveArray[PRE_KILLTP]; k++)
					e->chr.score.kills[k] = MSG_ReadShort(sb);
				for (k = 0; k < presaveArray[PRE_SKILTP]; k++) {
					e->chr.score.skills[k] = MSG_ReadShort(sb);
					e->chr.score.initialSkills[k] = e->chr.score.skills[k];	/**< This didn't exist in 2.2 but is now used for calc of "skills[]". */
				}

				e->chr.score.kills[KILLED_ALIENS] = MSG_ReadByte(sb);
				e->chr.score.stuns[KILLED_ALIENS] = MSG_ReadByte(sb);
				e->chr.score.kills[KILLED_CIVILIANS] = MSG_ReadByte(sb);
				e->chr.score.stuns[KILLED_CIVILIANS] = MSG_ReadByte(sb);
				e->chr.score.kills[KILLED_TEAM] = MSG_ReadByte(sb);
 				e->chr.score.stuns[KILLED_TEAM] = MSG_ReadByte(sb);
				MSG_ReadByte(sb);	/* e->chr.score.skillKills[SKILL_CLOSE] = ... skillKills is now in scoremission and not saved. */
				MSG_ReadByte(sb);	/* e->chr.score.skillKills[SKILL_HEAVY] =  */
				MSG_ReadByte(sb);	/* e->chr.score.skillKills[SKILL_ASSAULT] = */
				MSG_ReadByte(sb);	/* e->chr.score.skillKills[SKILL_SNIPER] = */
				MSG_ReadByte(sb);	/* e->chr.score.skillKills[SKILL_EXPLOSIVE] = */
				MSG_ReadByte(sb);	/* accuracystat ... doesn't exist any more.*/
				MSG_ReadByte(sb);	/* powerstat ... doesn't exist any more. */
			} else {
				/** Load character stats/score (starting with 2.3 and up)
				 * @sa inv_shared.h:chrScoreGlobal_t */

				for (k = 0; k < presaveArray[PRE_SKILTP]+1; k++)
					e->chr.score.experience[k] = MSG_ReadLong(sb);
				for (k = 0; k < presaveArray[PRE_SKILTP]; k++)
					e->chr.score.skills[k] = MSG_ReadByte(sb);
				for (k = 0; k < presaveArray[PRE_SKILTP]; k++)
					e->chr.score.initialSkills[k] = MSG_ReadByte(sb);
				for (k = 0; k < presaveArray[PRE_KILLTP]; k++)
					e->chr.score.kills[k] = MSG_ReadShort(sb);
				for (k = 0; k < presaveArray[PRE_KILLTP]; k++)
					e->chr.score.stuns[k] = MSG_ReadShort(sb);
				e->chr.score.assignedMissions = MSG_ReadShort(sb);
				e->chr.score.rank = MSG_ReadByte(sb);
			}

			/* clear the mess of stray loaded pointers */
			memset(&gd.employees[j][i].inv, 0, sizeof(inventory_t));
			CL_LoadInventory(sb, &e->inv);
			gd.employees[j][i].chr.inv = &gd.employees[j][i].inv;
		}
	}

	return qtrue;
}

/**
 * @brief Returns true if the current base is able to handle employees
 * @sa B_BaseInit_f
 */
qboolean E_HireAllowed (const base_t* base)
{
	if (base->baseStatus != BASE_UNDER_ATTACK && base->hasBuilding[B_QUARTERS]) {
		return qtrue;
	} else {
		return qfalse;
	}
}
