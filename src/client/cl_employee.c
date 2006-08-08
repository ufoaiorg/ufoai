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
	if (!baseCurrent ||!curCampaign)
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
		if (employee->hired)
			Cbuf_AddText(va("employeeadd%i\n", employeesInCurrentList));

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
 * @brief Creates an entry of a new employee in the global list and assignes it to no building/base.
 *
 * @param[in] type Tell the function what type of employee to create.
 * @return Pointer to the newly created employee in the global list. NULL if something goes wrong.
 * @sa E_DeleteEmployee
 */
employee_t* E_CreateEmployee(employeeType_t type)
{
	employee_t* employee = NULL;

	if (type == MAX_EMPL) return NULL;

	if (gd.numEmployees[type] >= MAX_EMPLOYEES) {
		Com_Printf("E_CreateEmployee: MAX_EMPLOYEES exceeded\n");
		return NULL;
	}

	employee = &gd.employees[type][gd.numEmployees[type]++];

	if (!employee) return NULL;
	employee->hired		= qfalse;
	employee->buildingID	= -1;
	
	switch (type) {
		case EMPL_SOLDIER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = CL_GenerateCharacter(Cvar_VariableString("team"), NULL, ET_ACTOR); */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = CL_GenerateCharacter(Cvar_VariableString("team"), NULL, ET_ACTOR); */
		
			if (employee->type == EMPL_SCIENTIST) {
				/* TODO: create random data for the employees depending on type and skill-min/max */
				employee->speed = 100;
			} else {
				/* TODO: create random data for the employees depending on type and skill-min/max */
				employee->speed = 100;
			}
			break;
		/*case EMPL_MEDIC: break; */
		/*case EMPL_ROBOT: break; */
		default:
			break;
		}
	return employee;
}

/**
 * @brief Force-removes an employee from a building.
 * @warning Use only from within E_DeleteEmployee.
 *
 * @param[in] employee The pointer to the employee you want to remove.
 * @param[in] building The pointer to the building the employee shoudl be removed from.
 * @return True if the employee was removed sucessfully, otherwise false.
 * @sa E_DeleteEmployee
 */
qboolean E_DeleteEmployeeFromBuilding(employee_t * employee, int building )
{
	int i;
	employees_t *employees_in_building = NULL;
	qboolean found = qfalse;
	building_t * building_ptr = NULL;

	if (!employee) return qfalse;

	building_ptr = &gd.buildings[employee->base_idx][building];
	employees_in_building = &building_ptr->assigned_employees;

	found = qfalse;

	for (i = 0; i < (employees_in_building->numEmployees - 1); i++) {
		if (employees_in_building->assigned[i] == employee->idx)
			found = qtrue;
		if (found)
			employees_in_building->assigned[i] = employees_in_building->assigned[i + 1];
	}
	if (found)
		employees_in_building->numEmployees--;
	else
		return qfalse;

	return qtrue;
}

/**
 * @brief Removes the employee compeltely from the game (buildings + global list).
 *
 * @param[in] employee The pointer to the employee you want to remove.
 * @return True if the employee was removed sucessfully, otherwise false.
 * @sa E_CreateEmployee
 */
qboolean E_DeleteEmployee(employee_t * employee)
{
	int i;
	qboolean found;

	if (!employee) return qfalse;

	if (employee->lab) {
		if (E_DeleteEmployeeFromBuilding(employee, employee->lab))
			employee->lab = -1;
	}

	if (employee->workshop) {
		if (E_DeleteEmployeeFromBuilding(employee, employee->workshop))
			employee->workshop = -1;
	}

	if (employee->quarters) {
		if (E_DeleteEmployeeFromBuilding(employee, employee->quarters))
			employee->quarters = -1;
	}

	employee->base_idx = -1;

	/* remove the employee from the global list */
	for (i = 0; i < (gd.numEmployees - 1); i++) {
		if (gd.employees[i].idx == employee->idx)
			found = qtrue;
		if (found)
			gd.employees[i] = gd.employees[i + 1];
	}

	if (found) {
		gd.numEmployees--;
	} else {
		Com_DPrintf("E_DeleteEmployee: Employee wasn't in the global list.\n");
		return qfalse;
	}

	return qtrue;
}

/**
 * @brief Assigns an employee to a building.
 *
 * There are two cases of assignment:
 *	1.) From global list to quarters (building_dest is a quarter). This will search for compeltely unassigned employee of the given type in the global list gd.employees and assign them to the quarter if it has free space. The employee will only be linked to a quarter.
 *	2.) From quarters to 'any' other building (i.e. lab + workshop for now). This will search for a free (i.e not yet assigned to a building other than quarters) employee of the given type in a quarter in the same base building_dest is located in and. The employee will be linked to its quarter and the assinged building.
 *
 * @todo Add check for destination building vs. employee_type and abort if they do not match.
 *
 * @param[in] building_dest Which building to assign the employee to.
 * @param[in] employee_type	What type of employee to assign to the building.
 * @sa E_RemoveEmployee
 * @return Returns true if adding was possible/sane otherwise false. In the later case nothing will be changed.
 */
qboolean E_AssignEmployee(building_t * building_dest, employeeType_t employee_type)
{
	int i, j;
	employee_t *employee = NULL;
	building_t *building_source = NULL;
	employees_t *employees_in_building_dest = NULL;
	employees_t *employees_in_building_source = NULL;

	/* maybe no quarters in base? */
	if (!building_dest)
		return qfalse;

	if (!baseCurrent) {
		Com_DPrintf("E_AssignEmployee: No Base set\n");
		return qfalse;
	}

	employees_in_building_dest = &building_dest->assigned_employees;
	employee = NULL;
	/* Check if there is enough space to add one employee in the destination building. */
	if (employees_in_building_dest->numEmployees < employees_in_building_dest->maxEmployees) {
		if (building_dest->buildingType == B_QUARTERS) {
			/* Get unassigned employee with correct type from global list. */
			for (i = 0; i < gd.numEmployees; i++) {
				employee = &gd.employees[i];
				if ((employee->type == employee_type) && E_EmployeeIsUnassinged(employee)) {
					break;
				} else {
					employee = NULL;
				}
			}

			/* If an employee was found add it to to the destination building (quarters) */
			if (employee) {
				employees_in_building_dest->assigned[employees_in_building_dest->numEmployees++] = employee->idx;
				employee->quarters = building_dest->idx;
				employee->base_idx = building_dest->base_idx;
				return qtrue;
			}
		} else {
			/* Get free employee from quarters in current base. */
			for (i = 0; i < gd.numBuildings[building_dest->base_idx]; i++) {
				building_source = &gd.buildings[building_dest->base_idx][i];

				/* check if there is a free employee in the quarters. */
				if (building_source->buildingType == B_QUARTERS) {
					employees_in_building_source = &building_source->assigned_employees;
					for (j = 0; j < employees_in_building_source->numEmployees; j++) {
						employee = &gd.employees[employees_in_building_source->assigned[j]];
						if ((employee->type == employee_type) && E_EmployeeIsFree(employee))
							break;
						else
							employee = NULL;
					}
				}
			}

			/* if an employee was found add it to to the destination building */
			if (employee) {
				employees_in_building_dest->assigned[employees_in_building_dest->numEmployees++] = employee->idx;
				employee->lab = building_dest->idx;
				return qtrue;
			} else {
				Com_Printf("No employee available in this base.\n");
			}
		}
	} else {
		Com_Printf("No free room in destination building \"%s\".\n", building_dest->id);
	}
	return qfalse;
}

/**
 * @brief Remove one employee from building.
 *
 * @todo Add check for destination building vs. employee_type and abort if they do not match.
 * @sa E_AssignEmployee
 * @param[in] building Which building to remove the employee from. Can be any type of building that has employees in it. If quarters are given the employee will be removed from every other building as well.
 *
 * @return Returns true if adding was possible/sane otherwise false.
 */
qboolean E_RemoveEmployee(building_t * building)
{
	/* TODO
	int i;
	*/
	employee_t *employee = NULL;
	employees_t *employees_in_building = NULL;

	/* TODO
	building_t *building_temp = NULL;
	qboolean found = qfalse;
	*/

	employees_in_building = &building->assigned_employees;

	if (employees_in_building->numEmployees <= 0) {
		Com_DPrintf("E_RemoveEmployee: No employees in building. Can't remove one. %s\n", building->id);
		return qfalse;
	}

	/* Check where else (which buildings) the employee needs to be removed. */
	switch (building->buildingType) {
	case B_LAB:
		employees_in_building->numEmployees--;	/* remove the employee from the list of assigned workers in the building. */
		employee = &gd.employees[employees_in_building->assigned[employees_in_building->numEmployees]];	/* get the last employee in the building. */
		Com_DPrintf("E_RemoveEmployee: %s\n", building->id);
		/* unlink the employee from lab (the current building). */
		employee->lab = -1;
		Com_DPrintf("E_RemoveEmployee: %s 2\n", building->id);
		return qtrue;
		/* break; */
#if 0
		/* TODO */
	case B_WORKSHOP:
		/* unlink the employee from workshop (the current building). */
		employee->workshop = NULL;
		return qtrue;
		break;
		EMPL_MEDIC EMPL_ROBOT
#endif
	default:
		break;
	}

	return qfalse;
}


/**
 * @brief Gets an unassigned employee of a given type from the global list.
 *
 * @todo E_GetFreeEmployee
 * @param[in] type The type of employee to search.
 * @return employee_t
 * @sa E_EmployeeIsUnassinged
 * @sa E_EmployeesInBase2
 */
employee_t * E_GetUnassingedEmployee(employeeType_t type)
{
	int i;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees; i++) {
		employee = &gd.employees[i];
		if ((employee->type == type) && E_EmployeeIsUnassinged(employee)) {
			break;
		} else {
			employee = NULL;
		}
	}
	return employee;
}

/**
 * @brief Gets the number of unassigned employees of a given type from the global list.
 *
 * @param[in] type The type of employee to search.
 * @return int number of found employees.
 * @sa E_EmployeeIsUnassinged
 * @sa E_GetUnassingedEmployee
*/
int E_GetUnassingedEmployeesByType(employeeType_t type)
{
	int i;
	int amount;
	employee_t *employee = NULL;

	for (i = 0; i < gd.numEmployees; i++) {
		employee = &gd.employees[i];
		if ((employee->type == type) && E_EmployeeIsUnassinged(employee)) {
			amount++;
		}
	}
	return amount;
}

/**
 * @brief Function that does the real employee adding
 * @note This will only add to quarters right now.
 * @param[in] b Pointer to building
 * @param[in] amount Amount of employees to assign
 * @sa E_BuildingAddEmployees
 * @sa E_BuildingAddEmployees_f
 * @todo This function should look for unassinged employees for the give type in the global list (abort if not enough are found) and assign them to the building.
 */
qboolean E_BuildingAddEmployees(building_t *building, employeeType_t type, int amount)
{
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;
	int unassinged_empl;
	int i;

	if (!building) return qfalse;

	if ( building->type != B_QUARTERS) {
		Com_Printf("Buulding is not a quarter.\n");
		return qfalse;
	}

	if ( type >= MAX_EMPL )
		return qfalse;

	unassinged_empl = E_GetUnassingedEmployeesByType(type);

	if ( unassinged_empl < amount) {
		Com_Printf("Not enough employees free of the given type (only %i instead of %i).\n", unassinged_empl, amount);
		return qfalse;
	}

	employees_in_building = &building->assigned_employees;
	if (employees_in_building->maxEmployees <= 0) {
		Com_Printf("No employees for this building: '%s'\n", building->id);
		return qfalse;
	}

	if ((employees_in_building->numEmployees + amount) >  employees_in_building->maxEmployees) {
		Com_Printf("The given amount of employees will not fit into this building: '%s'\n", building->id);
		return qfalse;
	}

	for (i = 0; i < amount; i++) {
		/* Get unassigned empl of given type. */
		employee = E_GetUnassingedEmployee(type)
		/* Assign empl to building. */
		if (!employee || !E_AssignEmployee(building,type)) {
			Com_Printf("Assignment of one employee went wrong: '%s'\n", building->id);
		}
	}
	return qtrue;
}

/**
 * @brief Script function for adding new employees to a building.
 * @note This will only add to quarters right now.
 * @sa E_BuildingAddEmployees
 */
void E_BuildingAddEmployees_f ( void )
{
	int i;
	int amount;
	employeeType_t type;

	Com_DPrintf("E_BuildingAddEmployees_f started\n");
	/* can be called from everywhere - so make a sanity check here */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: building_add_employees <type> <amount>\n");
		return;
	}

	type = E_GetEmployeeType(Cmd_Argv(1));
	Com_DPrintf("E_BuildingAddEmployees_f %i/%i\n", type, MAX_EMPL);
	if (type == MAX_EMPL)
		return;

	amount = atoi(Cmd_Argv(2));

	if ( (gd.numEmployees + amount) < gd.maxEmployees ) {
		for (i = 0; i < amount; i++) {
			E_CreateEmployee(type);
		}

		if (!E_BuildingAddEmployees(baseCurrent->buildingCurrent, type, amount))
			Com_DPrintf("No Employees added.\n");
		Com_DPrintf("E_BuildingAddEmployees_f finished.\n");
	} else {
		Com_DPrintf("E_BuildingAddEmployees_f: maxEmployees reached (num=$i, max=$i, new=$i).\n",gd.numEmployees, gd.maxEmployees, ammount);
	}
}

/**
 * @brief
 */
void E_EmployeeHire_f (void)
{
	int num;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: hire <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	/* some sanity checks */
	if (employeeCategory >= MAX_EMPL && employeeCategory < EMPL_SOLDIER )
		return;

	if (num >= employeesInCurrentList || num < 0)
		return;

	if (gd.employees[employeeCategory][num].hired)
		gd.employees[employeeCategory][num].hired = qfalse;
	else
		gd.employees[employeeCategory][num].hired = qfalse;
	/* update list */
	E_EmployeeList();
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
