/**
 * @file cl_employee.h
 * @brief Header for employee related stuff.
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

#ifndef CLIENT_CL_EMPLOYEE
#define CLIENT_CL_EMPLOYEE

/******* GUI STUFF ********/

void E_Reset(void);

/******* BACKEND STUFF ********/

/* TODO: */
/* MAX_EMPLOYEES_IN_BUILDING should be redefined by a config variable that is lab/workshop/quarters-specific */
/* e.g.: */
/* if ( !maxEmployeesInQuarter ) maxEmployeesInQuarter = MAX_EMPLOYEES_IN_BUILDING; */
/* if ( !maxEmployeesWorkersInLab ) maxEmployeesWorkersInLab = MAX_EMPLOYEES_IN_BUILDING; */
/* if ( !maxEmployeesInWorkshop ) maxEmployeesInWorkshop = MAX_EMPLOYEES_IN_BUILDING; */

/* The types of employees */
typedef enum {
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,				/* unused right now */
	EMPL_MEDIC,					/* unused right now */
	EMPL_ROBOT,					/* unused right now */
	MAX_EMPL					/* for counting over all available enums */
} employeeType_t;

/* The definition of an employee */
typedef struct employee_s {
	int idx;					/* self link in global employee-list. */

	/* this is true if the employee was already hired */
	qboolean hired;				/* default is qfalse */
	int baseIDHired;			/* baseID where the soldier is hired atm */

	char speed;					/* Speed of this Worker/Scientist at research/construction. */

	int buildingID;				/* assigned to this building in gd.buildings[baseIDHired][buildingID] */

	character_t chr;		/* Soldier stats (scis/workers/etc... as well ... e.g. if the base is attacked) */
	inventory_t inv;			/* employee inventory */
} employee_t;

void E_ResetEmployees(void);
employee_t * E_CreateEmployee(employeeType_t type);
qboolean E_DeleteEmployee(employee_t *employee, employeeType_t type);
qboolean E_HireEmployee(const base_t* const base, employeeType_t type, int num);
qboolean E_UnhireEmployee(const base_t* const base, employeeType_t type, int num);
qboolean E_RemoveEmployeeFromBuilding(employee_t *employee);

employeeType_t E_GetEmployeeType(char* type);
int E_EmployeesInBase(const base_t* const base, employeeType_t type, qboolean free_only);

employee_t * E_GetEmployee(const base_t* const base, employeeType_t type, int num);
character_t * E_GetCharacter(const base_t* const base, employeeType_t type, int num);
employee_t * E_GetHiredEmployee(const base_t* const base, employeeType_t type, int num);
character_t * E_GetHiredCharacter(const base_t* const base, employeeType_t type, int num);
employee_t * E_GetUnassignedEmployee(const base_t* const base, employeeType_t type);
employee_t * E_GetAssignedEmployee(const base_t* const base, employeeType_t type);

int E_CountHired(const base_t* const base, employeeType_t type);
int E_CountUnhired(const base_t* const base, employeeType_t type);
int E_CountUnassigned(const base_t* const base, employeeType_t type);
void E_UnhireAllEmployees(const base_t* const base, employeeType_t type);
void E_DeleteAllEmployees(const base_t* const base);

#endif /* CLIENT_CL_EMPLOYEE */
