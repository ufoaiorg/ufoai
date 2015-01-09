/**
 * @file
 * @brief Header for employee related stuff.
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#pragma once

/**
 * @brief The types of employees.
 */
typedef enum {
	EMPL_SOLDIER,
	EMPL_SCIENTIST,
	EMPL_WORKER,
	EMPL_PILOT,
	EMPL_ROBOT,
	MAX_EMPL		/**< For counting over all available enums. */
} employeeType_t;

/** The definition of an employee */
class Employee {
private:
	const employeeType_t _type;		/**< employee type */
	bool _assigned;					/**< Assigned to a building - currently only used for scientists */
	const struct nation_s* _nation;	/**< What nation this employee came from. This is nullptr if the nation is unknown for some (code-related) reason. */
	const struct ugv_s* _ugv;		/**< if this is an employee of type EMPL_ROBOT then this is a pointer to the matching ugv_t struct. For normal employees this is nullptr. */

public:
	Employee (employeeType_t type, const struct nation_s* nation, const struct ugv_s* ugv) :
			_type(type), _assigned(false), _nation(nation), _ugv(ugv), baseHired(nullptr), transfer(false) {
	}

	virtual ~Employee () {
	}

	/**
	 * @return @c true if the employee is not yet assigned to a building
	 */
	inline bool isAssigned () const {
		return _assigned;
	}

	inline void setAssigned(bool assigned) {
		_assigned = assigned;
	}

	inline bool isPilot () const {
		return _type == EMPL_PILOT;
	}

	inline bool isRobot () const {
		return _type == EMPL_ROBOT;
	}

	inline bool isScientist () const {
		return _type == EMPL_SCIENTIST;
	}

	inline bool isWorker () const {
		return _type == EMPL_WORKER;
	}

	inline bool isSoldier () const {
		return _type == EMPL_SOLDIER;
	}

	inline bool isHired () const {
		return baseHired != nullptr;
	}

	/**
	 * @brief Checks whether the given employee is in the given base
	 * @param[in] base The base the employee must be hired in for this function to return @c true.
	 */
	inline bool isHiredInBase (const base_t* const base) const {
		assert(base != nullptr);
		return baseHired == base;
	}

	inline employeeType_t getType() const {
		return _type;
	}

	inline const struct nation_s* getNation() const {
		return _nation;
	}

	inline const struct ugv_s* getUGV() const {
		return _ugv;
	}

	bool isAwayFromBase() const;
	bool unassign();
	bool unhire();
	void unequip();

	base_t* baseHired;				/**< Base where the soldier is hired it atm. */
	bool transfer;				/**< Is this employee currently transferred? */
	character_t chr;				/**< employee stats */
};

#define E_Foreach(employeeType, var) LIST_Foreach(ccs.employees[employeeType], Employee, var)

Employee* E_CreateEmployee(employeeType_t type, const struct nation_s* nation, const struct ugv_s* ugvType = nullptr);
bool E_DeleteEmployee(Employee* employee);

/* count */
int E_CountByType(employeeType_t type);
int E_CountHired(const base_t* const base, employeeType_t type);
int E_CountHiredRobotByType(const base_t* const base, const struct ugv_s* ugvType);
int E_CountAllHired(const base_t* const base, const bool peopleOnly = false);
int E_CountUnhired(employeeType_t type);
int E_CountUnhiredRobotsByType(const struct ugv_s* ugvType);
int E_CountUnassigned(const base_t* const base, employeeType_t type);

bool E_HireEmployee(base_t* base, Employee* employee);
bool E_HireEmployeeByType(base_t* base, employeeType_t type);
bool E_HireRobot(base_t* base, const struct ugv_s* ugvType);
bool E_UnhireEmployee(Employee* employee);

int E_RefreshUnhiredEmployeeGlobalList(const employeeType_t type, const bool excludeUnhappyNations);

employeeType_t E_GetEmployeeType(const char* type);
extern const char* E_GetEmployeeString(employeeType_t type, int n);

Employee* E_GetUnhired(employeeType_t type);
Employee* E_GetUnhiredRobot(const struct ugv_s* ugvType);

int E_GetHiredEmployees(const base_t* const base, employeeType_t type, linkedList_t** hiredEmployees);
Employee* E_GetHiredRobot(const base_t* const base, const struct ugv_s* ugvType);

Employee* E_GetUnassignedEmployee(const base_t* const base, employeeType_t type);
Employee* E_GetAssignedEmployee(const base_t* const base, employeeType_t type);

Employee* E_GetEmployeeFromChrUCN(int uniqueCharacterNumber);
Employee* E_GetEmployeeByTypeFromChrUCN(employeeType_t type, int uniqueCharacterNumber);

void E_UnhireAllEmployees(base_t* base, employeeType_t type);
void E_DeleteAllEmployees(base_t* base);

void E_HireForBuilding(base_t* base, building_t* building, int num);
void E_InitialEmployees(const struct campaign_s* campaign);

bool E_MoveIntoNewBase(Employee* employee, base_t* newBase);
void E_RemoveInventoryFromStorage(Employee* employee);

void E_InitStartup(void);
void E_Shutdown(void);
