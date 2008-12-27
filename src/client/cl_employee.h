/**
 * @file cl_employee.h
 * @brief Header for employee related stuff.
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

#ifndef CLIENT_CL_EMPLOYEE
#define CLIENT_CL_EMPLOYEE

#include "campaign/cl_campaign.h"

/******* GUI STUFF ********/

void E_InitStartup(void);

/******* BACKEND STUFF ********/

/** @todo MAX_EMPLOYEES_IN_BUILDING should be redefined by a config variable that is
 * lab/workshop/quarters-specific, e.g.:
 * @code
 * if (!maxEmployeesInQuarter) maxEmployeesInQuarter = MAX_EMPLOYEES_IN_BUILDING;
 * if (!maxEmployeesWorkersInLab) maxEmployeesWorkersInLab = MAX_EMPLOYEES_IN_BUILDING;
 * if (!maxEmployeesInWorkshop) maxEmployeesInWorkshop = MAX_EMPLOYEES_IN_BUILDING;
 * @endcode
 */

/** The definition of an employee */
typedef struct employee_s {
	int idx;					/**< self link in global employee-list - this should be references only with the variable name emplIdx
								 * to let us find references all over the code easier @sa E_DeleteEmployee */

	qboolean hired;				/**< this is true if the employee was already hired - default is false */
	base_t *baseHired;			/**< Base where the soldier is hired it atm. */

	char speed;					/**< Speed of this Worker/Scientist at research/construction. */

	building_t *building;				/**< Assigned to this building in gd.buildings[baseIDXHired][buildingID] */

	qboolean transfer;			/**< Is this employee currently transferred?
								 * @sa Set in TR_TransferStart_f
								 * @todo make use of this whereever needed. */
	character_t chr;			/**< Soldier stats (scis/workers/etc... as well ... e.g. if the base is attacked) */
	inventory_t inv;			/**< employee inventory */
	employeeType_t type;		/**< back link to employee type in gd.employees */
	struct nation_s *nation;	/**< What nation this employee came from. This is NULL if the nation is unknown for some (code-related) reason. */
	ugv_t *ugv;	/**< if this is an employee of type EMPL_ROBOT then this is a pointer to the matching ugv_t struct. For normal emplyoees this is NULL. */
} employee_t;

/* List of (hired) emplyoees in the current category (employeeType). */
extern linkedList_t *employeeList;	/** @sa E_GetEmployeeByMenuIndex */
/* How many employees in current list (changes on every category change, too) */
extern int employeesInCurrentList;

/* Currently selected employee. */
extern employee_t *selectedEmployee;

void E_ResetEmployees(void);
employee_t* E_CreateEmployee(employeeType_t type, struct nation_s *nation, ugv_t *ugvType);
qboolean E_DeleteEmployee(employee_t *employee, employeeType_t type);
qboolean E_HireEmployee(base_t* base, employee_t* employee);
qboolean E_HireEmployeeByType(base_t* base, employeeType_t type);
qboolean E_HireRobot(base_t* base, const ugv_t *ugvType);
qboolean E_UnhireEmployee(employee_t* employee);
void E_RefreshUnhiredEmployeeGlobalList(const employeeType_t type, const qboolean excludeUnhappyNations);
qboolean E_RemoveEmployeeFromBuildingOrAircraft(employee_t *employee);
void E_ResetEmployee(employee_t *employee);

employeeType_t E_GetEmployeeType(const char* type);
extern const char* E_GetEmployeeString(employeeType_t type);

employee_t* E_GetEmployee(const base_t* const base, employeeType_t type, int num);
employee_t* E_GetUnhiredRobot(const ugv_t *ugvType);
int E_GetHiredEmployees(const base_t* const base, employeeType_t type, linkedList_t **hiredEmployees);
employee_t* E_GetHiredRobot(const base_t* const base, const ugv_t *ugvType);
employee_t* E_GetUnassignedEmployee(const base_t* const base, employeeType_t type);
employee_t* E_GetAssignedEmployee(const base_t* const base, employeeType_t type);
employee_t* E_GetHiredEmployeeByUcn(const base_t* const base, employeeType_t type, int ucn);
employee_t* E_GetEmployeeFromChrUCN(int ucn);
qboolean E_EmployeeIsCurrentlyInBase(const employee_t * employee);

int E_CountHired(const base_t* const base, employeeType_t type);
int E_CountHiredRobotByType(const base_t* const base, const ugv_t *ugvType);
int E_CountAllHired(const base_t* const base);
int E_CountUnhired(employeeType_t type);
int E_CountUnhiredRobotsByType(const ugv_t *ugvType);
int E_CountUnassigned(const base_t* const base, employeeType_t type);
employee_t* E_GetEmployeeByMenuIndex(int num);
void E_UnhireAllEmployees(base_t* base, employeeType_t type);
void E_DeleteAllEmployees(base_t* base);
void E_DeleteEmployeesExceedingCapacity(base_t *base);
qboolean E_IsInBase(const employee_t* empl, const base_t* const base);
void E_Init(void);

#endif /* CLIENT_CL_EMPLOYEE */
