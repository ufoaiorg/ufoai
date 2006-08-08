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

void E_ResetEmployee(void);

/******* BACKEND STUFF ********/

/* max values for employee-management */
#define MAX_EMPLOYEES 1024
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

	/* FIXME: Remove me */
	int base_idx;				/* what base this employee is in. */
	int quarters;				/* The quarter this employee is assigned to. (all except EMPL_ROBOT) */
	int lab;					/* The lab this scientist is working in. (only EMPL_SCIENTIST) */
	int workshop;				/* The lab this worker is working in. (only EMPL_WORKER) */
} employee_t;

void E_InitEmployees(void);
qboolean E_AssignEmployee(employeeType_t type, base_t *base);
employee_t* E_CreateEmployee(employeeType_t type, base_t* base);
employeeType_t E_GetEmployeeType(char* type);
int E_EmployeesInBase2(int base_id, employeeType_t employee_type, qboolean free_only);
int E_BuildingAddEmployees(building_t* b, employeeType_t type, int amount);

#endif /* CLIENT_CL_EMPLOYEE */
