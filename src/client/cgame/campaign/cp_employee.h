/**
 * @file cp_employee.h
 * @brief Header for employee related stuff.
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

#ifndef CP_EMPLOYEE
#define CP_EMPLOYEE

/** @todo make this scriptable */
#define MAX_EMPLOYEES 512

/** @todo MAX_EMPLOYEES_IN_BUILDING should be redefined by a config variable that is
 * lab/workshop/quarters-specific, e.g.:
 * @code
 * if (!maxEmployeesInQuarter) maxEmployeesInQuarter = MAX_EMPLOYEES_IN_BUILDING;
 * if (!maxEmployeesWorkersInLab) maxEmployeesWorkersInLab = MAX_EMPLOYEES_IN_BUILDING;
 * if (!maxEmployeesInWorkshop) maxEmployeesInWorkshop = MAX_EMPLOYEES_IN_BUILDING;
 * @endcode
 */

#define E_Foreach(employeeType, var) LIST_Foreach(ccs.employees[employeeType], employee_t, var)

/**
 * @brief The types of employees.
 */
typedef enum {
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,
	EMPL_PILOT,
	MAX_EMPL		/**< For counting over all available enums. */
} employeeType_t;

/** The definition of an employee */
typedef struct employee_s {
	base_t *baseHired;				/**< Base where the soldier is hired it atm. */
	qboolean assigned;				/**< Assigned to a building */
	qboolean transfer;				/**< Is this employee currently transferred? */
	character_t chr;				/**< employee stats */
	employeeType_t type;			/**< employee type */
	const struct nation_s *nation;	/**< What nation this employee came from. This is NULL if the nation is unknown for some (code-related) reason. */
} employee_t;

void E_ResetEmployees(void);

employee_t* E_GetUnhired(employeeType_t type);

int E_CountByType(employeeType_t type);

employee_t* E_CreateEmployee(employeeType_t type, const struct nation_s *nation);
qboolean E_DeleteEmployee(employee_t *employee);

qboolean E_HireEmployee(base_t* base, employee_t* employee);
qboolean E_HireEmployeeByType(base_t* base, employeeType_t type);
qboolean E_UnhireEmployee(employee_t* employee);
int E_RefreshUnhiredEmployeeGlobalList(const employeeType_t type, const qboolean excludeUnhappyNations);

qboolean E_RemoveEmployeeFromBuildingOrAircraft(employee_t *employee);
void E_ResetEmployee(employee_t *employee);
int E_GenerateHiredEmployeesList(const base_t *base);
qboolean E_IsAwayFromBase(const employee_t *employee);

employeeType_t E_GetEmployeeType(const char* type);
extern const char* E_GetEmployeeString(employeeType_t type, int n);

int E_GetHiredEmployees(const base_t* const base, employeeType_t type, linkedList_t **hiredEmployees);
employee_t* E_GetUnassignedEmployee(const base_t* const base, employeeType_t type);
employee_t* E_GetAssignedEmployee(const base_t* const base, employeeType_t type);
employee_t* E_GetEmployeeFromChrUCN(int uniqueCharacterNumber);
employee_t* E_GetEmployeeByTypeFromChrUCN(employeeType_t type, int uniqueCharacterNumber);
qboolean E_MoveIntoNewBase(employee_t *employee, base_t *newBase);

int E_CountHired(const base_t* const base, employeeType_t type);
int E_CountAllHired(const base_t* const base);
int E_CountUnhired(employeeType_t type);
int E_CountUnassigned(const base_t* const base, employeeType_t type);
employee_t* E_GetEmployeeByMenuIndex(int num);
void E_UnhireAllEmployees(base_t* base, employeeType_t type);
void E_DeleteAllEmployees(base_t* base);
void E_DeleteEmployeesExceedingCapacity(base_t *base);
qboolean E_IsInBase(const employee_t* empl, const base_t* const base);
void E_HireForBuilding(base_t* base, building_t * building, int num);
void E_InitialEmployees(const struct campaign_s *campaign);
void E_Init(void);

void E_RemoveInventoryFromStorage(employee_t *employee);

#define E_IsHired(employee)	((employee)->baseHired != NULL)

void E_InitStartup(void);
void E_Shutdown(void);

#endif /* CP_EMPLOYEE */
