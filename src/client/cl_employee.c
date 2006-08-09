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
	employee_t* employee;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_init <category>\n");
		return;
	}
	employeeCategory = atoi(Cmd_Argv(1));
	if (employeeCategory > MAX_EMPL || employeeCategory < 0)
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
 * @brief Reads in information about employees and assigns them to the correct rooms in a base.
 *
 * This should be called after setting up the first base.
 * @todo This right now assumes that there are not more employees than free quarter space ... but it will not puke if there are.
 * @sa CL_GameInit
 */
void E_InitEmployees(void)
{
}

/**
 * @brief Returns true if the employee is only assigned to quarters but not to labs and workshops.
 *
 * @param[in] employee The employee_t pointer to check
 * @return qboolean
 * @sa E_EmployeeIsUnassinged
 */
qboolean E_EmployeeIsFree(employee_t * employee)
{
	return (employee->buildingID < 0 && employee->hired);
}

/**
 * @brief Return a given employee pointer in the given base of a given type.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type Which employee type do we search.
 * @param[in] idx Which employee id (in global employee array)
 * @return employee_t pointer or NULL
 */
employee_t* E_GetEmployee(base_t* base, employeeType_t type, int idx)
{
	int i;
	for (i=0; i<gd.numEmployees[type]; i++) {
		if (i == idx && (!gd.employees[type][i].hired || gd.employees[type][i].baseIDHired == base->idx))
			return &gd.employees[type][i];
	}
	return NULL;
}

/**
 * @brief Reset the hired flag for all employees of a given type in a given base
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 */
void E_UnhireAllEmployees(base_t* base, employeeType_t type)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->baseIDHired == base->idx) {
			/* TODO: Destroy inventory */
			employee->hired = qfalse;
			employee->buildingID = -1;
			employee->baseIDHired = -1;
		}
	}
	if (type == EMPL_SCIENTIST) {
		/* TODO: Scientists needs to be handled seperate
		   because there may be a tech they are working on */
	}
}

/**
 * @brief Return a given character pointer of an employee in the given base of a given type
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array)
 * @return character_t pointer or NULL
 */
character_t* E_GetCharacter(base_t* base, employeeType_t type, int idx)
{
	employee_t* employee = E_GetEmployee(base, type, idx);
	if (employee)
		return &(employee->chr);

	return NULL;
}

/**
 * @brief Return a given "not hired" employee pointer in the given base of a given type.
 * @param[in] type Which employee type to search for.
 * @param[in] idx Which employee id (in global employee array). Use -1 to return the first "not hired" employee.
 * @return employee_t pointer or NULL
 * @sa E_GetHiredEmployee
 * @sa E_UnhireEmployee
 * @sa E_HireEmployee
 */
employee_t* E_GetUnhiredEmployee(employeeType_t type, int idx)
{
	int i = 0;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
	
		/* Return first unhired employee if no idx is given. */
		if (idx < 0 && !employee->hired) {
			return employee;
		}
		if (i == idx) {
			if (employee->hired) {
				Com_Printf("E_GetUnhiredEmployee: Warning: employee is already hired!\n");
				return NULL;
			}
			return employee;
		}
	}
	return NULL;
}

/**
 * @brief Return a given hired employee pointer in the given base of a given type
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type Which employee type to search for.
 * @param[in] idx Which employee id (in global employee array). Use -1 to return the first "hired" employee.
 * @return employee_t pointer or NULL
 * @sa E_GetUnhiredEmployee
 * @sa E_HireEmployee
 * @sa E_UnhireEmployee
 */
employee_t* E_GetHiredEmployee(base_t* base, employeeType_t type, int idx)
{
	int i = 0;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if (employee->baseIDHired == base->idx) {
			if ((i == idx) || (idx < 0)) {
				return employee;
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
character_t* E_GetHiredCharacter(base_t* base, employeeType_t type, int idx)
{
	employee_t* employee = E_GetHiredEmployee(base, type, idx);
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
qboolean E_EmployeeIsUnassinged(employee_t * employee)
{
	return (employee->buildingID < 0);
}

/**
 * @brief Gets an unassigned employee of a given type from the given base.
 *
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassinged
 * @sa E_EmployeesInBase
 * @note assigned is not hired - they are already hired in a base, in a quarter _and_ working in another building.
 */
employee_t * E_GetAssingedEmployee(base_t* base, employeeType_t type)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if ( employee->baseIDHired == base->idx
			 && !E_EmployeeIsUnassinged(employee) )
			return employee;
	}
	return NULL;
}

/**
 * @brief Gets an assigned employee of a given type from the given base.
 *
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassinged
 * @sa E_EmployeesInBase
 * @note unassigned is not unhired - they are already hired in a base but are at quarters
 */
employee_t * E_GetUnassingedEmployee(base_t* base, employeeType_t type)
{
	int i;
	employee_t *employee;

	for (i = 0; i < gd.numEmployees[type]; i++) {
		employee = &gd.employees[type][i];
		if ( employee->baseIDHired == base->idx
			 && E_EmployeeIsUnassinged(employee) )
			return employee;
	}
	return NULL;
}

/**
 * @brief Hires an employee
 * @note set the hired flag to true
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array)
 * TODO: Check for quarter space
 */
qboolean E_HireEmployee(base_t* base, employeeType_t type, int idx)
{
	employee_t* employee;
	employee = E_GetUnhiredEmployee(type, idx);
	if (employee) {
		/* now uses quarter space */
		employee->hired = qtrue;
		employee->baseIDHired = base->idx;
		return qtrue;
	} else
		Com_Printf("Could not get unhired employee '%i' from base '%i'\n", idx, base->idx);
	return qfalse;
}

/**
 * @brief Hires an employee
 * @note set the hired flag to true
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @param[in] idx Which employee id (in global employee array)
 */
qboolean E_UnhireEmployee(base_t* base, employeeType_t type, int idx)
{
	employee_t* employee;
	employee = E_GetHiredEmployee(base, type, idx);
	if (employee) {
		employee->hired = qfalse;
		employee->baseIDHired = -1;
		employee->buildingID = -1;
		employee->inv.c[csi.idFloor] = NULL;
		Com_DestroyInventory(&employee->inv);
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
	employee_t* employee;

	if (type == MAX_EMPL)
		return NULL;

	if (gd.numEmployees[type] >= MAX_EMPLOYEES) {
		Com_Printf("E_CreateEmployee: MAX_EMPLOYEES exceeded\n");
		return NULL;
	}

	employee = &gd.employees[type][gd.numEmployees[type]];

	if (!employee)
		return NULL;

	employee->idx = gd.numEmployees[type];
	employee->hired = qfalse;
	employee->baseIDHired = -1;
	employee->buildingID = -1;

	switch (type) {
		case EMPL_SOLDIER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = CL_GenerateCharacter(Cvar_VariableString("team"), NULL, ET_ACTOR); */
			CL_GenerateCharacter(employee, Cvar_VariableString("team"), ET_ACTOR);
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = CL_GenerateCharacter(Cvar_VariableString("team"), NULL, ET_ACTOR); */
			CL_GenerateCharacter(employee, Cvar_VariableString("team"), ET_ACTOR);
			employee->speed = 100;
			break;
		/*case EMPL_MEDIC: break; */
		/*case EMPL_ROBOT: break; */
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
 */
qboolean E_DeleteEmployee(employee_t *employee, employeeType_t type)
{
	int i;
	qboolean found;

	if (!employee)
		return qfalse;

	if (employee->buildingID) {
/*		if (E_DeleteEmployeeFromBuilding(employee, employee->quarters))
			employee->quarters = -1;*/
	}

	employee->baseIDHired = -1;
	employee->hired = qfalse;

	/* remove the employee from the global list */
	for (i = 0; i < gd.numEmployees[type] - 1; i++) {
		if (gd.employees[type][i].idx == employee->idx) {
			/* TODO: delete this employee */
			gd.employees[type][i].inv.c[csi.idFloor] = NULL;
			Com_DestroyInventory(&gd.employees[type][i].inv);
			memset(&gd.employees[type][i].inv, 0, sizeof(inventory_t));
			found = qtrue;
		}
		if (found)
			gd.employees[type][i] = gd.employees[type][i + 1];
	}

	if (found) {
		gd.numEmployees[type]--;
	} else {
		Com_DPrintf("E_DeleteEmployee: Employee wasn't in the global list.\n");
		return qfalse;
	}

	return qtrue;
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
 * @sa E_RemoveEmployee
 * @return Returns true if adding was possible/sane otherwise false. In the later case nothing will be changed.
 */
qboolean E_AssignEmployee(building_t *building, employeeType_t type)
{
	employee_t * employee = NULL;
	
	switch (type) {
	case EMPL_SOLDIER:
		break;
	case EMPL_SCIENTIST:
		employee = E_GetUnassingedEmployee(&gd.bases[building->base_idx], type);
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

#if 0
/************************
TODO: Will later on be used in e.g E_UnhireEmployee and  RS_RemoveScientist_f 
Code does not yet reflect that though.
*********************************/
/**
 * @brief Remove one employee from building.
 *
 * @todo Add check for base vs. employee_type and abort if they do not match.
 * @sa E_AssignEmployee
 * @param[in] building Which building to remove the employee from. Can be any type of building that has employees in it. If quarters are given the employee will be removed from every other building as well.
 *
 * @return Returns true if removing was possible/sane otherwise false.
 */
qboolean E_RemoveEmployee(base_t* base, employeeType_t type, int idx)
{
	int i;

	for (i=0; i<gd.numEmployees[type];i++)
		if (i==num && gd.employees[type][i].baseIDHired == base->idx) {

			/* TODO */
			break;
		}

	/* TODO */
	return qfalse;
}
#endif
/**
 * @brief Counts hired employees of a given type in a given base
 *
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 * @return count of hired employees of a given type in a given base
 */
int E_CountHired(base_t* base, employeeType_t type)
{
	int count = 0, i;
	employee_t *employee;

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
int E_CountUnhired(base_t* base, employeeType_t type)
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
 * @brief Counts unassigned employees of a given type in a given base
 * @param[in] type The type of employee to search.
 * @param[in] base The base where we count
 */
int E_CountUnassinged(base_t* base, employeeType_t type)
{
	int count = 0, i;
	employee_t *employee;

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

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_hire <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* some sanity checks */
	if (employeeCategory >= MAX_EMPL && employeeCategory < EMPL_SOLDIER )
		return;

	if (num >= employeesInCurrentList || num < 0)
		return;

	if (gd.employees[employeeCategory][num].hired) {
		gd.employees[employeeCategory][num].hired = qfalse;
		gd.employees[employeeCategory][num].baseIDHired = -1;
		gd.employees[employeeCategory][num].buildingID = -1;
		Cbuf_AddText(va("employeedel%i\n", num));
	} else {
		gd.employees[employeeCategory][num].hired = qtrue;
		gd.employees[employeeCategory][num].baseIDHired = baseCurrent->idx;
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

	/* check syntax */
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
void E_ResetEmployee(void)
{
	/* add commands */
	Cmd_AddCommand("employee_init", E_EmployeeList);
	Cmd_AddCommand("employee_hire", E_EmployeeHire_f);
	Cmd_AddCommand("employee_select", E_EmployeeSelect_f);
}
