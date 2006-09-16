/**
 * @file cl_employee.c
 * @brief Single player employee stuff
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

/* holds the current active employee category */
static int employeeCategory = 0;
/* how many employees in current list (changes on every catergory change, too) */
static int employeesInCurrentList = 0;

/*****************************************************
 * VISUAL/GUI STUFF
 *****************************************************/

/**
 * @brief Will fill the list with employees
 * @note is the init function in employee menu
 */
static void E_EmployeeList (void)
{
	int i, j;
	employee_t* employee = NULL;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_init <category>\n");
		return;
	}
	employeeCategory = atoi(Cmd_Argv(1));
	if (employeeCategory >= MAX_EMPL || employeeCategory < 0)
		employeeCategory = EMPL_SOLDIER;

	employeesInCurrentList = 0;

	for (j=0, employee=gd.employees[employeeCategory]; j<gd.numEmployees[employeeCategory]; j++, employee++) {
		Cvar_ForceSet(va("mn_name%i", employeesInCurrentList), employee->chr.name);
		/* TODO; Check whether he is assigned to aircraft and/or carries weapons */
		/* change the buttons */
		if (employee->hired) {
			if (employee->baseIDHired == baseCurrent->idx)
				Cbuf_AddText(va("employeeadd%i\n", employeesInCurrentList));
			else
				Cbuf_AddText(va("employeedisable%i\n", employeesInCurrentList));
		} else
			Cbuf_AddText(va("employeedel%i\n", employeesInCurrentList));

		employeesInCurrentList++;
		/* we can't display more than 19 employees */
		if (employeesInCurrentList>=(int)cl_numnames->value)
			break;
	}
	i = employeesInCurrentList;
	for (;i<(int)cl_numnames->value;i++) {
		Cvar_ForceSet(va("mn_name%i", i), "");
		Cbuf_AddText(va("employeedisable%i\n", i));
	}
	Cbuf_AddText("employee_select 0\n");
}


/*****************************************************
 * EMPLOYEE BACKEND STUFF
 *****************************************************/

/**
 * @brief Convert string to employeeType_t
 * @param type Pointer to employee type string
 * @return employeeType_t
 */
employeeType_t E_GetEmployeeType(char* type)
{
	assert(type);
	if ( Q_strncmp(type, "EMPL_SCIENTIST", 14 ) )
		return EMPL_SCIENTIST;
	else if ( Q_strncmp(type, "EMPL_SOLDIER", 12 ) )
		return EMPL_SOLDIER;
	else if ( Q_strncmp(type, "EMPL_WORKER", 11 ) )
		return EMPL_WORKER;
	else if ( Q_strncmp(type, "EMPL_MEDIC", 10 ) )
		return EMPL_MEDIC;
	else if ( Q_strncmp(type, "EMPL_ROBOT", 10 ) )
		return EMPL_ROBOT;
	else {
		Sys_Error("Unknown employee type '%s'\n", type);
		return MAX_EMPL;
	}
}

/**
 * @brief Clears the employees list for loaded and new games
 * @sa CL_ResetSinglePlayerData
 * @sa E_DeleteEmployee
 */
void E_ResetEmployees(void)
{
	int i;
	Com_DPrintf("E_ResetEmployees: Delete all employees\n");
	for (i=EMPL_SOLDIER;i<MAX_EMPL;i++)
		if (gd.numEmployees[i]) {
			memset(gd.employees[i], 0, sizeof(employee_t)*MAX_EMPLOYEES);
			gd.numEmployees[i] = 0;
		}
}

/**
 * @brief Returns true if the employee is only assigned to quarters but not to labs and workshops.
 *
 * @param[in] employee The employee_t pointer to check
 * @return qboolean
 * @sa E_EmployeeIsUnassigned
 */
qboolean E_EmployeeIsFree(employee_t * employee)
{
	if ( !employee )
		Sys_Error("E_EmployeeIsUnassigned: Employee is NULL.\n");

	return (employee->buildingID < 0 && employee->hired);
}

/**
 * @brief Return a given employee pointer in the given base of a given type.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type Which employee type do we search.
 * @param[in] idx Which employee id (in global employee array)
 * @return employee_t pointer or NULL
 */
employee_t* E_GetEmployee(const base_t* const base, employeeType_t type, int idx)
{
	int i;

	if ( !base || type >= MAX_EMPL || type < 0 || idx < 0 )
		return NULL;

	for (i=0; i<gd.numEmployees[type]; i++) {
		if (i == idx && (!gd.employees[type][i].hired || gd.employees[type][i].baseIDHired == base->idx))
			return &gd.employees[type][i];
	}

	return NULL;
}

/**
 * @brief Reset the hired flag for all employees of a given type in a given base
 * @param[in] base Which base the employee should be fired from.
 * @param[in] type Which employee type do we search.
 */
void E_UnhireAllEmployees(const base_t* const base, employeeType_t type)
{
	int i;
	employee_t *employee = NULL;

	if ( !base || type > MAX_EMPL || type < 0 )
		return;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->baseIDHired == base->idx)
			E_UnhireEmployee(base, type, i);
	}
}

/**
 * @brief Return a given character pointer of an employee in the given base of a given type
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array)
 * @return character_t pointer or NULL
 */
character_t* E_GetCharacter(const base_t* const base, employeeType_t type, int idx)
{
	employee_t* employee = E_GetEmployee(base, type, idx); /* Parameter sanity is checked here. */
	if (employee)
		return &(employee->chr);

	return NULL;
}

/**
 * @brief Return a given "not hired" employee pointer in the given base of a given type.
 * @param[in] type Which employee type to search for.
 * @param[in] idx Which employee id (in global employee array). Use -1, -2, etc.. to return the first/ second, etc... "hired" employee.
 * @return employee_t pointer or NULL
 * @sa E_GetHiredEmployee
 * @sa E_UnhireEmployee
 * @sa E_HireEmployee
 */
employee_t* E_GetUnhiredEmployee(employeeType_t type, int idx)
{
	int i = 0;
	int j = -1;	/* The number of found unhired employees. Ignore the minus. */
	employee_t *employee = NULL;

	if ( type >= MAX_EMPL || type < 0 ) {
		Com_Printf("E_GetUnhiredEmployee: Unknown EmployeeType: %i\n", type );
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
 * @brief Return a given hired employee pointer in the given base of a given type
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type Which employee type to search for.
 * @param[in] idx Which employee id (in global employee array). Use -1, -2, etc.. to return the first/ second, etc... "hired" employee.
 * @return employee_t pointer or NULL
 * @sa E_GetUnhiredEmployee
 * @sa E_HireEmployee
 * @sa E_UnhireEmployee
 */
employee_t* E_GetHiredEmployee(const base_t* const base, employeeType_t type, int idx)
{
	int i = 0;
	int j = -1;	/* The number of found hired employees. Ignore the minus. */
	employee_t *employee = NULL;

	if ( !base)
		return NULL;

	if ( type >= MAX_EMPL || type < 0 ) {
		Com_Printf("E_GetHiredEmployee: Unknown EmployeeType: %i\n", type );
		return NULL;
	}

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
		} else {
			continue;
		}
	}
	return NULL;
}

/**
 * @brief Return a given character pointer of a hired employee in the given base of a given type
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array)
 * @return character_t pointer or NULL
 */
character_t* E_GetHiredCharacter(const base_t* const base, employeeType_t type, int idx)
{
	employee_t* employee = E_GetHiredEmployee(base, type, idx);	 /* Parameter sanity is checked here. */
	if (employee)
		return &(employee->chr);

	return NULL;
}

/**
 * @brief Returns true if the employee is _only_ listed in the global list.
 *
 * @param[in] employee The employee_t pointer to check
 * @return qboolean
 * @sa E_EmployeeIsFree
 */
qboolean E_EmployeeIsUnassigned(employee_t * employee)
{
	if ( !employee )
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
employee_t * E_GetAssignedEmployee(const base_t* const base, employeeType_t type)
{
	int i;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if ( employee->baseIDHired == base->idx
			 && !E_EmployeeIsUnassigned(employee) )
			return employee;
	}
	return NULL;
}

/**
 * @brief Gets an assigned employee of a given type from the given base.
 *
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassigned
 * @sa E_EmployeesInBase
 * @note unassigned is not unhired - they are already hired in a base but are at quarters
 */
employee_t * E_GetUnassignedEmployee(const base_t* const base, employeeType_t type)
{
	int i;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if ( employee->baseIDHired == base->idx
			 && E_EmployeeIsUnassigned(employee) )
			return employee;
	}
	return NULL;
}

/**
 * @brief Hires an employee
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array) See E_GetUnhiredEmployee for usage.
 * @todo Check for quarter space
 * @sa E_UnhireEmployee
 */
qboolean E_HireEmployee(const base_t* const base, employeeType_t type, int idx)
{
	employee_t* employee = NULL;
	employee = E_GetUnhiredEmployee(type, idx);
	if (employee) {
		/* Now uses quarter space. */
		employee->hired = qtrue;
		employee->baseIDHired = base->idx;
		return qtrue;
	}
	return qfalse;
}

/**
 * @brief Fires an employee.
 * @param[in] base Which base the employee was hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array) See E_GetHiredEmployee for usage.
 * @sa E_HireEmployee
 */
qboolean E_UnhireEmployee(const base_t* const base, employeeType_t type, int idx)
{
	employee_t* employee = NULL;
	employee = E_GetHiredEmployee(base, type, idx);
	if (employee) {
		if (employee->buildingID >= 0 ) {
			/* Remove employee from building (and tech/production). */
			E_RemoveEmployeeFromBuilding(employee);
			/* TODO: Assign a new employee to the tech/production) if one is available.
			E_AssignEmployee(employee, building_rom_unhired_employee);
			*/
		}
		/* why not merge it with E_RemoveEmployeeFromBuilding, where the building is Dropship Hangar (unless handgar can hold more than one aircraft... */
		if (type == EMPL_SOLDIER ) {
			/* Remove soldier from aircraft/team if he was assigned to one. */
			if ( CL_SoldierInAircraft(employee->idx, -1) ){
				CL_RemoveSoldierFromAircraft(employee->idx, -1);
			}
		}
		/* Set all employee-tags to 'unhired'. */
		employee->hired = qfalse;
		employee->baseIDHired = -1;
		/* Destroy the inventory of the employee (carried items will remain in base->storage) */
		Com_DestroyInventory(&employee->inv);
		/* unneeded, Com_DestroyInventory does this (more or less)
		memset(&employee->inv, 0, sizeof(inventory_t)); */
		return qtrue;
	} else
		Com_Printf("Could not get hired employee '%i' from base '%i'\n", idx, base->idx);
	return qfalse;
}

/**
 * @brief Creates an entry of a new employee in the global list and assignes it to no building/base.
 *
 * @param[in] type Tell the function what type of employee to create.
 * @return Pointer to the newly created employee in the global list. NULL if something goes wrong.
 * @sa E_DeleteEmployee
 */
employee_t* E_CreateEmployee(employeeType_t type)
{
	employee_t* employee = NULL;

	if ( type >= MAX_EMPL || type < 0 )
		return NULL;

	if (gd.numEmployees[type] >= MAX_EMPLOYEES) {
		Com_Printf("E_CreateEmployee: MAX_EMPLOYEES exceeded\n");
		return NULL;
	}

	employee = &gd.employees[type][gd.numEmployees[type]];
	memset(employee, 0, sizeof(employee_t));

	if (!employee)
		return NULL;

	employee->idx = gd.numEmployees[type];
	employee->hired = qfalse;
	employee->baseIDHired = -1;
	employee->buildingID = -1;

	switch (type) {
		case EMPL_SOLDIER:
			CL_GenerateCharacter(employee, Cvar_VariableString("team"), type);
			break;
		case EMPL_SCIENTIST:
		case EMPL_MEDIC:
		case EMPL_WORKER:
			CL_GenerateCharacter(employee, Cvar_VariableString("team"), type);
			employee->speed = 100;
			break;
		case EMPL_ROBOT:
			CL_GenerateCharacter(employee, Cvar_VariableString("team"), type);
			break;
		default:
			break;
		}
	gd.numEmployees[type]++;
	return employee;
}

/**
 * @brief Removes the employee completely from the game (buildings + global list).
 *
 * @param[in] employee The pointer to the employee you want to remove.
 * @return True if the employee was removed sucessfully, otherwise false.
 * @sa E_CreateEmployee
 * @sa E_ResetEmployees
 */
qboolean E_DeleteEmployee(employee_t *employee, employeeType_t type)
{
	int i, idx;
	qboolean found = qfalse;

	if (!employee)
		return qfalse;

	idx=employee->idx;
	/* Fire the employee. This will also:
		1) remove him from buildings&work
		2) remove his inventory
	*/

	if (employee->baseIDHired >= 0)	{
		E_UnhireEmployee(&gd.bases[employee->baseIDHired], type, employee->idx);
	}

	/* Remove the employee from the global list. */
	for (i = 0; i < gd.numEmployees[type]; i++) {
		if (gd.employees[type][i].idx == employee->idx)	{
			found = qtrue;
		}
		if (found) {

			if ( i < MAX_EMPLOYEES-1) { /* Just in case we have _that much_ employees. :) */
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

		if (type == EMPL_SOLDIER) {
			for ( i = 0; i < MAX_AIRCRAFT; i++ ) 
				CL_DecreaseAircraftTeamIdxGreaterThan(CL_AircraftGetFromIdx(i),idx);
		}
	} else {
		Com_DPrintf("E_DeleteEmployee: Employee wasn't in the global list.\n");
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Removes all employees completely from the game (buildings + global list) from a given base.
 * @note Used if the base e.g is destroyed by the aliens.
 * @param[in] base Which base the employee should be fired from.
 */
void E_DeleteAllEmployees(const base_t* const base)
{
	int i;
	employeeType_t type;
	employee_t *employee = NULL;

	if ( !base )
		return;
	Com_DPrintf("E_DeleteAllEmployees: starting ...\n");
	for ( type = EMPL_SOLDIER; type < MAX_EMPL; type++ ) {
		Com_DPrintf("E_DeleteAllEmployees: Removing empl-type %i | num %i\n", type, gd.numEmployees[type]);
		/* Attention:
			gd.numEmployees[type] is changed in E_DeleteAllEmployees!  (it's decreased by 1 per call)
			For this reason we start this loop from the back of the empl-list. toward 0.
		*/
		for ( i = gd.numEmployees[type]-1; i >= 0 ; i--) {
			Com_DPrintf("E_DeleteAllEmployees: %i\n", i);
			employee = &gd.employees[type][i];
			if (employee->baseIDHired == base->idx) {
				E_DeleteEmployee(employee, type);
				Com_DPrintf("E_DeleteAllEmployees:	 Removing empl.\n");
			} else if (employee->baseIDHired >= 0) {
				Com_DPrintf("E_DeleteAllEmployees:	 Not removing empl. (other base)\n");
			}
		}
	}
	Com_DPrintf("E_DeleteAllEmployees: ... finished\n");
}

#if 0
/************************
TODO: Will later on be used in e.g RS_AssignScientist_f
*********************************/
/**
 * @brief Assigns an employee to a building.
 *
 * @param[in] building The building the employee is assigned to.
 * @param[in] employee_type	What type of employee to assign to the building.
 * @sa E_RemoveEmployeeFromBuilding
 * @return Returns true if adding was possible/sane otherwise false. In the later case nothing will be changed.
 */
qboolean E_AssignEmployeeToBuilding(building_t *building, employeeType_t type)
{
	employee_t * employee = NULL;

	switch (type) {
	case EMPL_SOLDIER:
		break;
	case EMPL_SCIENTIST:
		employee = E_GetUnassignedEmployee(&gd.bases[building->base_idx], type);
		if (employee) {
			employee->buildingID = building->idx;
		} else {
			/* TODO: message -> no employee available */
		}
		break;
	default:
		Com_DPrintf("E_AssignEmployee: Unhandled employee type: %i\n", type);
		break;
	}
	return qfalse;
}
#endif

/**
 * @brief Remove one employee from building.
 *
 * @todo Add check for base vs. employee_type and abort if they do not match.
 * @param[in] employee What employee to remove from its building.
 * @return Returns true if removing was possible/sane otherwise false.
 * @sa E_AssignEmployeeToBuilding
 */
qboolean E_RemoveEmployeeFromBuilding(employee_t *employee)
{
	character_t *chr = NULL;
	technology_t *tech = NULL;

	if (employee) {
		chr = &employee->chr;
		switch (chr->empl_type) {
		case EMPL_SCIENTIST:
			/* Get technology with highest scientist-count and remove one scientist. */
			tech = RS_GetTechWithMostScientists(employee->baseIDHired);
			if (tech) {
				tech->scientists--;
				/* Try to assign replacement scientist */
				RS_AssignScientist(tech);
			}
			employee->buildingID = -1;
			break;

		case EMPL_SOLDIER:
		case EMPL_MEDIC:
		case EMPL_WORKER:
		case EMPL_ROBOT:
			/*TODO: Check if they are linked to anywhere and remove them there. */
			break;
		default:
			Com_DPrintf("E_RemoveEmployeeFromBuilding: Unhandled employee type: %i\n", chr->empl_type);
			break;
		}
	}
	return qfalse;

}

/**
 * @brief Counts hired employees of a given type in a given base
 *
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 * @return count of hired employees of a given type in a given base
 */
int E_CountHired(const base_t* const base, employeeType_t type)
{
	int count = 0, i;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->hired && employee->baseIDHired == base->idx)
			count++;
	}
	return count;
}

/**
 * @brief Counts unhired employees of a given type in a given base
 *
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 * @return count of hired employees of a given type in a given base
 */
int E_CountUnhired(const base_t* const base, employeeType_t type)
{
	int count = 0, i;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (!employee->hired)
			count++;
	}
	return count;
}

/**
 * @brief Counts unassigned employees of a given type in a given base
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 */
int E_CountUnassigned(const base_t* const base, employeeType_t type)
{
	int count = 0, i;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->buildingID < 0 && employee->baseIDHired == base->idx)
			count++;
	}
	return count;
}

/**
 * @brief
 */
void E_EmployeeHire_f (void)
{
	int num;

	if (!baseCurrent)
		return;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_hire <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* Some sanity checks. */
	if (employeeCategory >= MAX_EMPL && employeeCategory < EMPL_SOLDIER )
		return;

	if (num >= employeesInCurrentList || num < 0)
		return;

	/* Already hired in another base. */
	/* TODO: Should hired employees in another base be listed here at all? */
	if (gd.employees[employeeCategory][num].hired
	 && gd.employees[employeeCategory][num].baseIDHired != baseCurrent->idx)
		return;

	if (gd.employees[employeeCategory][num].hired) {
		if (!E_UnhireEmployee(&gd.bases[gd.employees[employeeCategory][num].baseIDHired], employeeCategory, num)) {
			/* TODO: message - Couldn't fire employee. */
		}
		Cbuf_AddText(va("employeedel%i\n", num));
	} else {
		if (!E_HireEmployee(baseCurrent, employeeCategory, num)) {
			/* TODO: message - Couldn't hire employee. */
		}
		Cbuf_AddText(va("employeeadd%i\n", num));
	}
	Cbuf_AddText(va("employee_select %i\n", num));
}

/**
 * @brief
 */
static void E_EmployeeSelect_f(void)
{
	int num;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_select <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	if (num >= gd.numEmployees[employeeCategory])
		return;

	/* console commands */
	Cbuf_AddText(va("employeedeselect%i\n", (int) cl_selected->value));
	Cbuf_AddText(va("employeeselect%i\n", num));
	Cvar_ForceSet("cl_selected", va("%i", num));

	/* set info cvars */
	CL_CharacterCvars(&(gd.employees[employeeCategory][num].chr));
}

/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
void E_Reset(void)
{
	/* add commands */
	Cmd_AddCommand("employee_init", E_EmployeeList, NULL);
	Cmd_AddCommand("employee_hire", E_EmployeeHire_f, NULL);
	Cmd_AddCommand("employee_select", E_EmployeeSelect_f, NULL);
}
