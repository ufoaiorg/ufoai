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
 * @brief Prints information about the current employee
 */
static void E_EmployeeInfo (void)
{
	static char employeeInfo[512];
	menuText[TEXT_EMPLOYEE] = employeeInfo;
}

/**
 * @brief Will fill the list with employees
 */
static void E_EmployeeList (void)
{
	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_init <category>\n");
		return;
	}
	employeeCategory = atoi(Cmd_Argv(1));

	/*
	employeesInCurrentList++;
	*/

	/* now print the information about the current employee */
	E_EmployeeInfo();
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
	int i, j;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	/* Loop trough the buildings to assign the type of employee. */
	/* TODO: this right now assumes that there are not more employees than free quarter space ... but it will not puke if there are. */
	for (i = 0; i < gd.numBuildingTypes; i++) {
		building = &gd.buildingTypes[i];
		employees_in_building = &building->assigned_employees;
		/* TODO: fixed value right now, needs a configureable one. */
		employees_in_building->cost_per_employee = 100;
		if (employees_in_building->maxEmployees <= 0)
			employees_in_building->maxEmployees = MAX_EMPLOYEES_IN_BUILDING;
		for (j = 0; j < employees_in_building->numEmployees; j++) {
			employee = &gd.employees[employees_in_building->assigned[j]];
			switch (building->buildingType) {
			case B_QUARTERS:
				employee->type = EMPL_SOLDIER;
				break;
			case B_LAB:
				employee->type = EMPL_SCIENTIST;
				break;
			case B_WORKSHOP:
				employee->type = EMPL_WORKER;
				break;
				/*EMPL_MEDIC */
				/*EMPL_ROBOT */
			default:
				break;
			}
		}
	}
	Com_DPrintf("E_InitEmployees: Existing building types: 0 to %i\n", i);

	building = NULL;
	/* Generate stats for employees and assign the quarter-less to quarters. */
	for (i = 0; i < gd.numEmployees; i++) {
		employee = &gd.employees[i];
		switch (employee->type) {
		case EMPL_SOLDIER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			employee->lab = -1;
			employee->workshop = -1;
			if (employee->type == EMPL_SCIENTIST) {
				/* TODO: create random data for the employees depending on type and skill-min/max */
				employee->speed = 100;
			} else {
				/* TODO: create random data for the employees depending on type and skill-min/max */
				employee->speed = 100;
			}
			building = B_GetFreeBuildingType(B_QUARTERS);
			if (!building)
				break;
			employees_in_building = &building->assigned_employees;
			employees_in_building->assigned[employees_in_building->numEmployees++] = employee->idx;
			break;
			/*case EMPL_MEDIC: break; */
			/*case EMPL_ROBOT: break; */
		default:
			break;
		}
	}

	/* Remove them all from their assigned buildings except quarters .. this was just needed for firstbase. */
	for (i = 0; i < gd.numBuildingTypes; i++) {
		building = &gd.buildingTypes[i];
		employees_in_building = &building->assigned_employees;
		if (building->buildingType != B_QUARTERS) {
			employees_in_building->numEmployees = 0;
		}
	}
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
	return (
		(employee->lab < 0) &&
		(employee->workshop < 0)
	);
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
	return (
		(employee->lab < 0) &&
		(employee->workshop < 0) &&
		(employee->quarters < 0) &&
		(employee->base_idx < 0)
	);
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
	/* TODO: check for maxemployees? */
	employee = &gd.employees[gd.numEmployees++];

	if (!employee) return NULL;

	switch (type) {
		case EMPL_SOLDIER:
			/* TODO: create random data for the employees depending on type and skill-min/max */
			/* employee->combat_stats = CL_GenerateCharacter(Cvar_VariableString("team"), NULL, ET_ACTOR); */
			break;
		case EMPL_SCIENTIST:
		case EMPL_WORKER:
			employee->base_idx	= -1;
			employee->quarters	= -1;
			employee->lab		= -1;
			employee->workshop	= -1;
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
 * @brief Function that does the real employee adding
 * @param[in] b Pointer to building
 * @param[in] amount Amount of employees to assign
 * @sa E_BuildingAddEmployees
 * @sa E_BuildingRemoveEmployees
 * @sa E_BuildingAddEmployees_f
 */
int E_BuildingAddEmployees(building_t *b, employeeType_t type, int amount)
{
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if ( gd.numEmployees >= MAX_EMPLOYEES ) {
		Com_Printf("Employee limit hit\n");
		return 0;
	}

	if ( type >= MAX_EMPL )
		return 0;

	employees_in_building = &b->assigned_employees;
	if (employees_in_building->maxEmployees <= 0) {
		Com_Printf("No employees for this building: '%s'\n", b->id);
		return 0;
	}

	for (employees_in_building->numEmployees = 0; employees_in_building->numEmployees < amount;) {
		/* check for overflow */
		if (employees_in_building->numEmployees >= MAX_EMPLOYEES_IN_BUILDING)
			break;
		/* assign random employee infos. */
		/* link this employee in the building to the global employee-list. */
		employees_in_building->assigned[employees_in_building->numEmployees] = gd.numEmployees;
		employee = &gd.employees[gd.numEmployees];
		memset(employee, 0, sizeof(employee_t));
		employee->idx = gd.numEmployees;
		employee->type = type;

		employees_in_building->numEmployees++;
		gd.numEmployees++;
	}
	return 0;
}

/**
 * @brief
 * @sa E_BuildingAddEmployees
 * Script function for adding new employees to a building
 */
void E_BuildingAddEmployees_f ( void )
{
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

	if (!E_BuildingAddEmployees(baseCurrent->buildingCurrent, type, atoi(Cmd_Argv(2))))
		Com_DPrintf("Employees not added - at least not all\n");
	Com_DPrintf("E_BuildingAddEmployees_f finished.\n");
}

/**
 * @brief Remove some employees from the current building
 * @param amount[in] Amount of employees you would like to remove from the current building
 * @sa E_BuildingAddEmployees
 * @sa E_BuildingRemoveEmployees_f
 *
 * @note baseCurrent and baseCurrent->buildingCurrent may not be NULL
 */
static int E_BuildingRemoveEmployees (building_t *b, employeeType_t type, int amount)
{
	if ( type >= MAX_EMPL )
		return 0;
	/* TODO: Implement me */
	return 0;
}

/**
 * @brief
 * @sa E_BuildingRemoveEmployees
 * Script function for removing employees from a building
 */
static void E_BuildingRemoveEmployees_f ( void )
{
	employeeType_t type;
	/* can be called from everywhere - so make a sanity check here */
	if (!baseCurrent || !baseCurrent->buildingCurrent)
		return;

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: building_remove_employees <type> <amount>\n");
		return;
	}

	type = E_GetEmployeeType(Cmd_Argv(1));
	if (type == MAX_EMPL)
		return;

	if (!E_BuildingRemoveEmployees(baseCurrent->buildingCurrent, type, atoi(Cmd_Argv(2))))
		Com_DPrintf("Employees not removed - at least not all\n");
}


/**
 * @brief Returns the number of employees in the given base (in the quaters) of the given type.
 * @sa E_EmployeesInBase
 * You can choose (free_only) if you want the number of free employees or the total number.
 * If you call the function with employee_type set to MAX_EMPL it will return every type of employees.
 */
int E_EmployeesInBase2(int base_idx, employeeType_t employee_type, qboolean free_only)
{
	int i, j;
	int numEmployeesInBase = 0;
	building_t *building = NULL;
	employees_t *employees_in_building = NULL;
	employee_t *employee = NULL;

	if (!baseCurrent) {
		Com_DPrintf("B_EmployeesInBase2: No Base set.\n");
		return 0;
	}

	for (i = 0; i < gd.numBuildings[base_idx]; i++) {
		building = &gd.buildings[base_idx][i];
		if (building->buildingType == B_QUARTERS) {
			/* quarters found */
			employees_in_building = &building->assigned_employees;

			/*loop trough building and add to numEmployeesInBase if a match is found. */
			for (j = 0; j < employees_in_building->numEmployees; j++) {
				employee = &gd.employees[employees_in_building->assigned[j]];
				if (((employee_type == employee->type) || (employee_type == MAX_EMPL))
					&& (E_EmployeeIsFree(employee) || !free_only))
					numEmployeesInBase++;
			}
		}
	}
	return numEmployeesInBase;
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

	if (num >= employeesInCurrentList)
		return;
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

	/* console commands */
	Cbuf_AddText(va("employeedeselect%i\n", (int) cl_selected->value));
	Cbuf_AddText(va("employeeselect%i\n", num));
	Cvar_ForceSet("cl_selected", va("%i", num));

	/* set info cvars */
	/* TODO: */
	/*CL_CharacterCvars(&baseCurrent->wholeTeam[num]);*/
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
	Cmd_AddCommand("building_add_employees", E_BuildingAddEmployees_f );
	Cmd_AddCommand("building_remove_employees", E_BuildingRemoveEmployees_f );
	Cmd_AddCommand("employee_hire", E_EmployeeHire_f);
	Cmd_AddCommand("employee_select", E_EmployeeSelect_f);
}
