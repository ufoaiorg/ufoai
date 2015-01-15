/**
 * @file
 * @brief Single player employee stuff.
 * @note Employee related functions prefix: E_
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

#include "../../cl_shared.h"
#include "../cl_game.h" /* GAME_GetTeamDef */
#include "../cl_game_team.h" /* character xml loading */
#include "cp_campaign.h"
#include "cp_employee_callbacks.h"
#include "cp_rank.h"
#include "cp_popup.h"
#include "save/save_employee.h"

/**
 * @brief Returns number of employees of a type
 * @param[in] type Employeetype to check
 */
int E_CountByType (employeeType_t type)
{
	return cgi->LIST_Count(ccs.employees[type]);
}

/**
 * @brief Iterates through unhired employees
 * @param[in] type Employee type to look for
 * @sa employeeType_t
 */
Employee* E_GetUnhired (employeeType_t type)
{
	E_Foreach(type, employee) {
		if (!employee->isHired())
			return employee;
	}

	return nullptr;
}

/**
 * @brief Tells you if a employee is away from his home base (gone in mission).
 * @return bool true if the employee is away in mission, false if he is not or he is unhired.
 */
bool Employee::isAwayFromBase () const
{
	/* Check that employee is hired */
	if (!isHired())
		return false;

	/* Check if employee is currently transferred. */
	if (transfer)
		return true;

	/* for now only soldiers, ugvs and pilots can be assigned to an aircraft */
	if (!isSoldier() && !isRobot() && !isPilot())
		return false;

	/* Crashed aircraft no longer belongs to any base but poor pilot/soldiers assigned
	 * to it are definitely away from the base so we need to iterate through all aircraft */
	AIR_Foreach(aircraft) {
		if (aircraft->homebase != baseHired)
			continue;
		if (!AIR_IsAircraftInBase(aircraft) && AIR_IsEmployeeInAircraft(this, aircraft))
			return true;
	}
	return false;
}

/**
 * @brief  Hires some employees of appropriate type for a building
 * @param[in] base Which base the employee should be hired in.
 * @param[in] building in which building
 * @param[in] num how many employees, if -1, hire building->maxEmployees
 * @sa B_SetUpBase
 */
void E_HireForBuilding (base_t* base, building_t* building, int num)
{
	if (num < 0)
		num = building->maxEmployees;

	if (num) {
		employeeType_t employeeType;
		switch (building->buildingType) {
		case B_WORKSHOP:
			employeeType = EMPL_WORKER;
			break;
		case B_LAB:
			employeeType = EMPL_SCIENTIST;
			break;
		case B_HANGAR: /* the Dropship Hangar */
			employeeType = EMPL_SOLDIER;
			break;
		case B_MISC:
			Com_DPrintf(DEBUG_CLIENT, "E_HireForBuilding: Misc building type: %i with employees: %i.\n", building->buildingType, num);
			return;
		default:
			Com_DPrintf(DEBUG_CLIENT, "E_HireForBuilding: Unknown building type: %i.\n", building->buildingType);
			return;
		}
		/* don't try to hire more that available - see E_CreateEmployee */
		num = std::min(num, E_CountByType(employeeType));
		for (;num--;) {
			assert(base);
			if (!E_HireEmployeeByType(base, employeeType)) {
				Com_DPrintf(DEBUG_CLIENT, "E_HireForBuilding: Hiring %i employee(s) of type %i failed.\n", num, employeeType);
				return;
			}
		}
	}
}

/**
 * Will change the base where the employee is located in and will also update the
 * capacity in the affected bases.
 * @note Doesn't make any capacity checks and the employee must be hired already.
 * @param employee The employee to change the base for
 * @param newBase The base where the employee should be located at
 * @return @c false if @c employee was a @c nullptr pointer
 */
bool E_MoveIntoNewBase (Employee* employee, base_t* newBase)
{
	if (employee) {
		base_t* oldBase = employee->baseHired;
		employee->baseHired = newBase;
		/* Remove employee from corresponding capacity */
		switch (employee->getType()) {
		case EMPL_WORKER:
			if (oldBase != nullptr)
				PR_UpdateProductionCap(oldBase, 0);
		case EMPL_PILOT:
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			if (oldBase != nullptr)
				CAP_AddCurrent(oldBase, CAP_EMPLOYEES, -1);
			CAP_AddCurrent(newBase, CAP_EMPLOYEES, 1);
			break;
		case EMPL_ROBOT:
			if (oldBase != nullptr)
				CAP_AddCurrent(oldBase, CAP_ITEMS, -UGV_SIZE);
			CAP_AddCurrent(newBase, CAP_ITEMS, UGV_SIZE);
			break;
		case MAX_EMPL:
			break;
		}
		return true;
	}

	return false;
}

/**
 * @brief Convert employeeType_t to translated string
 * @param type employeeType_t value
 * @param n number of persons of that kind (for plural detection)
 * @return translated employee string
 */
const char* E_GetEmployeeString (employeeType_t type, int n)
{
	switch (type) {
	case EMPL_SOLDIER:
		return ngettext("Soldier", "Soldiers", n);
	case EMPL_SCIENTIST:
		return ngettext("Scientist", "Scientists", n);
	case EMPL_WORKER:
		return ngettext("Worker", "Workers", n);
	case EMPL_PILOT:
		return ngettext("Pilot", "Pilots", n);
	case EMPL_ROBOT:
		return ngettext("UGV", "UGVs", n);
	default:
		cgi->Com_Error(ERR_DROP, "Unknown employee type '%i'\n", type);
	}
}

/**
 * @brief Convert string to employeeType_t
 * @param type Pointer to employee type string
 * @return employeeType_t
 * @todo use cgi->Com_ConstInt*
 */
employeeType_t E_GetEmployeeType (const char* type)
{
	if (!type)
		return MAX_EMPL;

	/* employee types as string */
	if (Q_streq(type, "EMPL_SCIENTIST"))
		return EMPL_SCIENTIST;
	else if (Q_streq(type, "EMPL_SOLDIER"))
		return EMPL_SOLDIER;
	else if (Q_streq(type, "EMPL_WORKER"))
		return EMPL_WORKER;
	else if (Q_streq(type, "EMPL_PILOT"))
		return EMPL_PILOT;
	else if (Q_streq(type, "EMPL_ROBOT"))
		return EMPL_ROBOT;

	/* human readable employee type strings */
	if (Q_streq(type, "scientist"))
		return EMPL_SCIENTIST;
	else if (Q_streq(type, "soldier"))
		return EMPL_SOLDIER;
	else if (Q_streq(type, "worker"))
		return EMPL_WORKER;
	else if (Q_streq(type, "pilot"))
		return EMPL_PILOT;
	else if (Q_streq(type, "robot"))
		return EMPL_ROBOT;
	else if (Q_streq(type, "ugv"))
		return EMPL_ROBOT;

	return MAX_EMPL;
}

/**
 * @brief Return a "not hired" ugv-employee pointer of a given ugv-type.
 * @param[in] ugvType What type of robot we want.
 * @return Employee pointer on success or nullptr on error.
 * @sa E_GetHiredRobot
 */
Employee* E_GetUnhiredRobot (const ugv_t* ugvType)
{
	E_Foreach(EMPL_ROBOT, employee) {
		if (!employee->isHired()) {
			/* If no type was given we return the first ugv we find. */
			if (!ugvType || employee->getUGV() == ugvType)
				return employee;
		}
	}

	return nullptr;
}

/**
 * @brief Return a list of hired employees in the given base of a given type
 * @param[in] base Which base the employee should be searched in. If nullptr is given employees in all bases will be listed.
 * @param[in] type Which employee type to search for.
 * @param[out] hiredEmployees Linked list of hired employees in the base.
 * @return Number of hired employees in the base that are currently not on a transfer. Or @c -1 in case of an error.
 */
int E_GetHiredEmployees (const base_t* const base, employeeType_t type, linkedList_t** hiredEmployees)
{
	if (type >= MAX_EMPL) {
		Com_Printf("E_GetHiredEmployees: Unknown EmployeeType: %i\n", type);
		*hiredEmployees = nullptr;
		return -1;
	}

	cgi->LIST_Delete(hiredEmployees);

	E_Foreach(type, employee) {
		if (!employee->isHired())
			continue;
		if (!employee->transfer && (!base || employee->isHiredInBase(base))) {
			cgi->LIST_AddPointer(hiredEmployees, employee);
		}
	}

	return cgi->LIST_Count(*hiredEmployees);
}

/**
 * @brief Return a "hired" ugv-employee pointer of a given ugv-type in a given base.
 * @param[in] base Which base the ugv should be searched in.c
 * @param[in] ugvType What type of robot we want.
 * @return Employee pointer on success or nullptr on error.
 * @sa E_GetUnhiredRobot
 */
Employee* E_GetHiredRobot (const base_t* const base, const ugv_t* ugvType)
{
	linkedList_t* hiredEmployees = nullptr;
	Employee* employee;

	E_GetHiredEmployees(base, EMPL_ROBOT, &hiredEmployees);

	employee = nullptr;
	LIST_Foreach(hiredEmployees, Employee, e) {
		if ((e->getUGV() == ugvType || !ugvType)	/* If no type was given we return the first ugv we find. */
		 && e->isHiredInBase(base)) {		/* It has to be in the defined base. */
			assert(e->isHired());
			employee = e;
			break;
		}
	}

	cgi->LIST_Delete(&hiredEmployees);

	if (!employee)
		Com_DPrintf(DEBUG_CLIENT, "Could not get unhired ugv/robot.\n");

	return employee;
}

/**
 * @brief Gets an unassigned employee of a given type from the given base.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type The type of employee to search.
 * @return Employee
 * @sa E_EmployeeIsUnassigned
 * @note assigned is not hired - they are already hired in a base, in a quarter _and_ working in another building.
 */
Employee* E_GetAssignedEmployee (const base_t* const base, const employeeType_t type)
{
	E_Foreach(type, employee) {
		if (!employee->isHiredInBase(base))
			continue;
		if (employee->isAssigned())
			return employee;
	}
	return nullptr;
}

/**
 * @brief Gets an assigned employee of a given type from the given base.
 * @param[in] base Which base the employee should be hired in.
 * @param[in] type The type of employee to search.
 * @return Employee
 * @sa E_EmployeeIsUnassigned
 * @note unassigned is not unhired - they are already hired in a base but are at quarters
 */
Employee* E_GetUnassignedEmployee (const base_t* const base, const employeeType_t type)
{
	E_Foreach(type, employee) {
		if (!employee->isHiredInBase(base))
			continue;
		if (employee->transfer)
			continue;
		if (!employee->isAssigned())
			return employee;
	}
	return nullptr;
}

/**
 * @brief Hires the employee in a base.
 * @param[in] base Which base the employee should be hired in
 * @param[in] employee Which employee to hire
 * @sa E_HireEmployeeByType
 * @todo handle EMPL_ROBOT capacities here?
 */
bool E_HireEmployee (base_t* base, Employee* employee)
{
	if (CAP_GetFreeCapacity(base, CAP_EMPLOYEES) <= 0) {
		CP_Popup(_("Not enough quarters"), _("You don't have enough quarters for your employees.\nBuild more quarters."));
		return false;
	}

	if (employee) {
		/* Now uses quarter space. */
		employee->baseHired = base;
		/* Update other capacities */
		switch (employee->getType()) {
		case EMPL_WORKER:
			CAP_AddCurrent(base, CAP_EMPLOYEES, 1);
			PR_UpdateProductionCap(base);
			break;
		case EMPL_PILOT:
			AIR_AutoAddPilotToAircraft(base, employee);
			/* fall through */
		case EMPL_SCIENTIST:
		case EMPL_SOLDIER:
			CAP_AddCurrent(base, CAP_EMPLOYEES, 1);
			/** @todo shouldn't we link CID_EQUIP here already to the base inventory equip container? */
			break;
		case EMPL_ROBOT:
			CAP_AddCurrent(base, CAP_ITEMS, UGV_SIZE);
			break;
		case MAX_EMPL:
			break;
		}
		return true;
	}
	return false;
}

/**
 * @brief Hires the first free employee of that type.
 * @param[in] base Which base the employee should be hired in
 * @param[in] type Which employee type do we search
 * @sa E_HireEmployee
 */
bool E_HireEmployeeByType (base_t* base, employeeType_t type)
{
	Employee* employee = E_GetUnhired(type);
	return employee ? E_HireEmployee(base, employee) : false;
}

/**
 * @brief Hires the first free employee of that type.
 * @param[in] base  Which base the ugv/robot should be hired in.
 * @param[in] ugvType What type of ugv/robot should be hired.
 * @return true if everything went ok (the ugv was added), otherwise false.
 */
bool E_HireRobot (base_t* base, const ugv_t* ugvType)
{
	Employee* employee = E_GetUnhiredRobot(ugvType);
	return employee ? E_HireEmployee(base, employee) : false;
}

/**
 * @brief Removes the employee from buildings and aircraft
 */
bool Employee::unassign ()
{
	if (!baseHired)
		return false;

	/* Remove employee from building/tech/production/aircraft). */
	switch (_type) {
	case EMPL_SCIENTIST:
		if (isAssigned())
			RS_RemoveFiredScientist(baseHired, this);
		break;
	case EMPL_ROBOT:
	case EMPL_SOLDIER:
		/* Remove soldier from aircraft/team if he was assigned to one. */
		if (AIR_IsEmployeeInAircraft(this, nullptr))
			AIR_RemoveEmployee(this, nullptr);
		break;
	case EMPL_PILOT:
		AIR_RemovePilotFromAssignedAircraft(baseHired, this);
		break;
	case EMPL_WORKER:
	default:
		break;
	}

	return true;
}

/**
 * @brief Removes the equipment from an employee
 */
void Employee::unequip ()
{
	/* Destroy the inventory of the employee (carried items will remain in base->storage) */
	cgi->INV_DestroyInventory(&chr.inv);
}

/**
 * @brief Fires an employee.
 * @note also unassigns him from the aircraft and buildings
 * @sa E_HireEmployee
 * @sa E_HireEmployeeByType
 * @sa CL_RemoveSoldierFromAircraft
 * @todo handle EMPL_ROBOT capacities here?
 */
bool Employee::unhire ()
{
	if (!isHired() || transfer) {
		Com_DPrintf(DEBUG_CLIENT, "Could not fire employee\n");
		return false;
	}

	unassign();
	unequip();

	/* Remove employee from corresponding capacity */
	switch (_type) {
	case EMPL_WORKER:
		PR_UpdateProductionCap(baseHired, -1);
	case EMPL_PILOT:
	case EMPL_SCIENTIST:
	case EMPL_SOLDIER:
		CAP_AddCurrent(baseHired, CAP_EMPLOYEES, -1);
		break;
	case EMPL_ROBOT:
		CAP_AddCurrent(baseHired, CAP_ITEMS, -UGV_SIZE);
		break;
	case MAX_EMPL:
		break;
	}

	/* Set all employee-tags to 'unhired'. */
	baseHired = nullptr;

	return true;
}

/**
 * @brief Reset the hired flag for all employees of a given type in a given base
 * @param[in] base Which base the employee should be fired from.
 * @param[in] type Which employee type do we search.
 */
void E_UnhireAllEmployees (base_t* base, employeeType_t type)
{
	if (!base)
		return;

	assert(type < MAX_EMPL);

	E_Foreach(type, employee) {
		if (!employee->isHiredInBase(base))
			continue;
		employee->unhire();
	}
}

/**
 * @brief Creates an entry of a new employee in the global list and assignes it to no building/base.
 * @param[in] type What type of employee to create.
 * @param[in] nation What nation the employee (mainly used for soldiers in singleplayer) comes from.
 * @param[in] ugvType What type of ugv this employee is.
 * @return Pointer to the newly created employee in the global list. nullptr if something goes wrong.
 * @sa E_DeleteEmployee
 */
Employee* E_CreateEmployee (employeeType_t type, const nation_t* nation, const ugv_t* ugvType)
{
	const char* teamID;
	char teamDefName[MAX_VAR];
	const char* rank;

	if (type >= MAX_EMPL)
		return nullptr;

	Employee employee(type, nation, ugvType);

	teamID = GAME_CP_GetTeamDef();

	/* Generate character stats, models & names. */
	switch (type) {
	case EMPL_SOLDIER:
		rank = "rifleman";
		Q_strncpyz(teamDefName, teamID, sizeof(teamDefName));
		break;
	case EMPL_SCIENTIST:
		rank = "scientist";
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_scientist", teamID);
		break;
	case EMPL_PILOT:
		rank = "pilot";
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_pilot", teamID);
		break;
	case EMPL_WORKER:
		rank = "worker";
		Com_sprintf(teamDefName, sizeof(teamDefName), "%s_worker", teamID);
		break;
	case EMPL_ROBOT:
		if (ugvType == nullptr)
			cgi->Com_Error(ERR_DROP, "E_CreateEmployee: no type given for generation of EMPL_ROBOT employee.");

		rank = "ugv";

		Com_sprintf(teamDefName, sizeof(teamDefName), "%s%s", teamID, ugvType->actors);
		break;
	default:
		cgi->Com_Error(ERR_DROP, "E_CreateEmployee: Unknown employee type\n");
	}

	cgi->CL_GenerateCharacter(&employee.chr, teamDefName);
	employee.chr.score.rank = CL_GetRankIdx(rank);

	Com_DPrintf(DEBUG_CLIENT, "Generate character for type: %i\n", type);

	return &LIST_Add(&ccs.employees[type], employee);
}

/**
 * @brief Removes the employee completely from the game (buildings + global list).
 * @param[in] employee The pointer to the employee you want to remove.
 * @return True if the employee was removed successfully, otherwise false.
 * @sa E_CreateEmployee
 * @note This function has the side effect, that the global employee number for
 * the given employee type is reduced by one, also the ccs.employees pointers are
 * moved to fill the gap of the removed employee. Thus pointers like acTeam in
 * the aircraft can point to wrong employees now. This has to be taken into
 * account
 */
bool E_DeleteEmployee (Employee* employee)
{
	employeeType_t type;

	if (!employee)
		return false;

	type = employee->getType();

	/* Fire the employee. This will also:
	 * 1) remove him from buildings&work
	 * 2) remove his inventory */

	if (employee->baseHired) {
		/* make sure that this employee is really unhired */
		employee->transfer = false;
		employee->unhire();
	}

	/* Remove the employee from the global list. */
	return cgi->LIST_Remove(&ccs.employees[type], (void*) employee);
}

/**
 * @brief Removes all employees completely from the game (buildings + global list) from a given base.
 * @note Used if the base e.g is destroyed by the aliens.
 * @param[in] base Which base the employee should be fired from.
 */
void E_DeleteAllEmployees (base_t* base)
{
	int i;

	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t type = (employeeType_t)i;
		E_Foreach(type, employee) {
			if (base == nullptr || employee->isHiredInBase(base))
				E_DeleteEmployee(employee);
		}
	}
}

/**
 * @brief Recreates all the employees for a particular employee type in the global list.
 * But it does not overwrite any employees already hired.
 * @param[in] type The type of the employee list to process.
 * @param[in] excludeUnhappyNations True if a nation is unhappy then they wont
 * send any pilots, false if happiness of nations in not considered.
 * @sa CP_NationHandleBudget
 */
int E_RefreshUnhiredEmployeeGlobalList (const employeeType_t type, const bool excludeUnhappyNations)
{
	const nation_t* happyNations[MAX_NATIONS];
	int numHappyNations = 0;
	int idx, nationIdx, cnt;

	happyNations[0] = nullptr;
	/* get a list of nations,  if excludeHappyNations is true then also exclude
	 * unhappy nations (unhappy nation: happiness <= 0) from the list */
	for (idx = 0; idx < ccs.numNations; idx++) {
		const nation_t* nation = NAT_GetNationByIDX(idx);
		const nationInfo_t* stats = NAT_GetCurrentMonthInfo(nation);
		if (stats->happiness > 0 || !excludeUnhappyNations) {
			happyNations[numHappyNations] = nation;
			numHappyNations++;
		}
	}

	if (!numHappyNations)
		return 0;

	idx = 0;
	/* Fill the global data employee list with employees, evenly distributed
	 * between nations in the happyNations list */
	E_Foreach(type, employee) {
		/* we don't want to overwrite employees that have already been hired */
		if (!employee->isHired()) {
			E_DeleteEmployee(employee);
			idx++;
		}
	}

	nationIdx = 0;
	cnt = 0;
	while (idx-- > 0) {
		if (E_CreateEmployee(type, happyNations[nationIdx], nullptr) != nullptr)
			cnt++;
		nationIdx = (nationIdx + 1) % numHappyNations;
	}

	return cnt;
}

/**
 * @brief Counts hired employees of a given type in a given base
 * @param[in] base The base where we count (@c nullptr to count all).
 * @param[in] type The type of employee to search.
 * @return count of hired employees of a given type in a given base
 */
int E_CountHired (const base_t* const base, employeeType_t type)
{
	int count = 0;

	E_Foreach(type, employee) {
		if (!employee->isHired())
			continue;
		if (!base || employee->isHiredInBase(base))
			count++;
	}
	return count;
}

/**
 * @brief Counts 'hired' (i.e. bought or produced UGVs and other robots of a given ugv-type in a given base.
 * @param[in] base The base where we count (@c nullptr to count all).
 * @param[in] ugvType What type of robot/ugv we are looking for.
 * @return Count of Robots/UGVs.
 */
int E_CountHiredRobotByType (const base_t* const base, const ugv_t* ugvType)
{
	int count = 0;

	E_Foreach(EMPL_ROBOT, employee) {
		if (!employee->isHired())
			continue;
		if (employee->getUGV() == ugvType && (!base || employee->isHiredInBase(base)))
			count++;
	}
	return count;
}


/**
 * @brief Counts all hired employees of a given base
 * @param[in] base The base where we count
 * @param[in] peopleOnly Count people only (skip robots)
 * @return count of hired employees of a given type in a given base
 */
int E_CountAllHired (const base_t* const base, const bool peopleOnly)
{
	if (!base)
		return 0;

	int count = 0;
	for (int i = 0; i < MAX_EMPL; i++) {
		const employeeType_t type = (employeeType_t)i;
		if (peopleOnly && i == EMPL_ROBOT)
			continue;
		count += E_CountHired(base, type);
	}

	return count;
}

/**
 * @brief Counts unhired employees of a given type in a given base
 * @param[in] type The type of employee to search.
 * @return count of hired employees of a given type in a given base
 */
int E_CountUnhired (employeeType_t type)
{
	int count = 0;

	E_Foreach(type, employee) {
		if (!employee->isHired())
			count++;
	}
	return count;
}

/**
 * @brief Counts all available Robots/UGVs that are for sale.
 * @param[in] ugvType What type of robot/ugv we are looking for.
 * @return count of available robots/ugvs.
 */
int E_CountUnhiredRobotsByType (const ugv_t* ugvType)
{
	int count = 0;

	E_Foreach(EMPL_ROBOT, employee) {
		if (!employee->isHired() && employee->getUGV() == ugvType)
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
	if (!base)
		return 0;

	int count = 0;
	E_Foreach(type, employee) {
		if (!employee->isHiredInBase(base))
			continue;
		if (!employee->isAssigned())
			count++;
	}
	return count;
}

/**
 * @brief Hack to get a random nation for the initial
 */
static inline const nation_t* E_RandomNation (void)
{
	const int nationIndex = rand() % ccs.numNations;
	return NAT_GetNationByIDX(nationIndex);
}

/**
 * @brief Create initial hireable employees
 */
void E_InitialEmployees (const campaign_t* campaign)
{
	int i;

	/* setup initial employee count */
	for (i = 0; i < campaign->soldiers; i++)
		E_CreateEmployee(EMPL_SOLDIER, E_RandomNation(), nullptr);
	for (i = 0; i < campaign->scientists; i++)
		E_CreateEmployee(EMPL_SCIENTIST, E_RandomNation(), nullptr);
	for (i = 0; i < campaign->workers; i++)
		E_CreateEmployee(EMPL_WORKER, E_RandomNation(), nullptr);
	for (i = 0; i < campaign->pilots; i++)
		E_CreateEmployee(EMPL_PILOT, E_RandomNation(), nullptr);
	for (i = 0; i < campaign->ugvs; i++) {
		/** @todo don't use hardcoded UGV ids */
		if (frand() > 0.5)
			E_CreateEmployee(EMPL_ROBOT, E_RandomNation(), cgi->Com_GetUGVByID("ugv_ares_w"));
		else
			E_CreateEmployee(EMPL_ROBOT, E_RandomNation(), cgi->Com_GetUGVByID("ugv_phoenix"));
	}
}

#ifdef DEBUG
/**
 * @brief Debug command to list all hired employee
 */
static void E_ListHired_f (void)
{
	int i;

	for (i = 0; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		E_Foreach(emplType, employee) {
			Com_Printf("Employee: %s (ucn: %i) %s at %s\n", E_GetEmployeeString(employee->getType(), 1), employee->chr.ucn,
					employee->chr.name, employee->baseHired->name);
			if (employee->getType() != emplType)
				Com_Printf("Warning: EmployeeType mismatch: %i != %i\n", emplType, employee->getType());
		}
	}
}

/**
 * @brief Debug function to add 5 new unhired employees of each type
 * @note called with debug_addemployees
 */
static void CL_DebugNewEmployees_f (void)
{
	int j;
	nation_t* nation = &ccs.nations[0];	/**< This is just a debugging function, nation does not matter */

	for (j = 0; j < 5; j++)
		/* Create a scientist */
		E_CreateEmployee(EMPL_SCIENTIST, nation, nullptr);

	for (j = 0; j < 5; j++)
		/* Create a pilot. */
		E_CreateEmployee(EMPL_PILOT, nation, nullptr);

	for (j = 0; j < 5; j++)
		/* Create a soldier. */
		E_CreateEmployee(EMPL_SOLDIER, nation, nullptr);

	for (j = 0; j < 5; j++)
		/* Create a worker. */
		E_CreateEmployee(EMPL_WORKER, nation, nullptr);
}
#endif

/**
 * @brief Searches employee from a type for the ucn (character id)
 * @param[in] type employee type
 * @param[in] uniqueCharacterNumber unique character number (UCN)
 */
Employee* E_GetEmployeeByTypeFromChrUCN (employeeType_t type, int uniqueCharacterNumber)
{
	E_Foreach(type, employee) {
		if (employee->chr.ucn == uniqueCharacterNumber)
			return employee;
	}

	return nullptr;
}

/**
 * @brief Searches all employee for the ucn (character id)
 * @param[in] uniqueCharacterNumber unique character number (UCN)
 */
Employee* E_GetEmployeeFromChrUCN (int uniqueCharacterNumber)
{
	int i;

	for (i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		Employee* employee = E_GetEmployeeByTypeFromChrUCN(emplType, uniqueCharacterNumber);
		if (employee)
			return employee;
	}

	return nullptr;
}


/**
 * @brief Save callback for savegames in XML Format
 * @param[out] p XML Node structure, where we write the information to
 * @sa E_LoadXML
 * @sa SAV_GameSaveXML
 * @sa G_SendCharacterData
 * @sa CP_ParseCharacterData
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
bool E_SaveXML (xmlNode_t* p)
{
	int i;

	cgi->Com_RegisterConstList(saveEmployeeConstants);
	for (i = 0; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		xmlNode_t* snode = cgi->XML_AddNode(p, SAVE_EMPLOYEE_EMPLOYEES);

		cgi->XML_AddString(snode, SAVE_EMPLOYEE_TYPE, cgi->Com_GetConstVariable(SAVE_EMPLOYEETYPE_NAMESPACE, emplType));
		E_Foreach(emplType, employee) {
			xmlNode_t* chrNode;
			xmlNode_t* ssnode = cgi->XML_AddNode(snode, SAVE_EMPLOYEE_EMPLOYEE);

			/** @note e->transfer is not saved here because it'll be restored via TR_Load. */
			if (employee->baseHired)
				cgi->XML_AddInt(ssnode, SAVE_EMPLOYEE_BASEHIRED, employee->baseHired->idx);
			if (employee->isAssigned())
				cgi->XML_AddBool(ssnode, SAVE_EMPLOYEE_ASSIGNED, employee->isAssigned());
			/* Store the nations identifier string. */
			assert(employee->getNation());
			cgi->XML_AddString(ssnode, SAVE_EMPLOYEE_NATION, employee->getNation()->id);
			/* Store the ugv-type identifier string. (Only exists for EMPL_ROBOT). */
			if (employee->getUGV())
				cgi->XML_AddString(ssnode, SAVE_EMPLOYEE_UGV, employee->getUGV()->id);
			/* Character Data */
			chrNode = cgi->XML_AddNode(ssnode, SAVE_EMPLOYEE_CHR);
			cgi->GAME_SaveCharacter(chrNode, &employee->chr);
		}
	}
	cgi->Com_UnregisterConstList(saveEmployeeConstants);

	return true;
}

/**
 * @brief Load callback for savegames in XML Format
 * @param[in] p XML Node structure, where we get the information from
 */
bool E_LoadXML (xmlNode_t* p)
{
	xmlNode_t* snode;
	bool success = true;
	linkedList_t* lastEmployee[MAX_EMPL];

	OBJZERO(lastEmployee);
	cgi->Com_RegisterConstList(saveEmployeeConstants);
	for (snode = cgi->XML_GetNode(p, SAVE_EMPLOYEE_EMPLOYEES); snode;
			snode = cgi->XML_GetNextNode(snode, p, SAVE_EMPLOYEE_EMPLOYEES)) {
		xmlNode_t* ssnode;
		employeeType_t emplType;
		const char* type = cgi->XML_GetString(snode, SAVE_EMPLOYEE_TYPE);

		if (!cgi->Com_GetConstIntFromNamespace(SAVE_EMPLOYEETYPE_NAMESPACE, type, (int*) &emplType)) {
			Com_Printf("Invalid employee type '%s'\n", type);
			success = false;
			break;
		}

		for (ssnode = cgi->XML_GetNode(snode, SAVE_EMPLOYEE_EMPLOYEE); ssnode;
				ssnode = cgi->XML_GetNextNode(ssnode, snode, SAVE_EMPLOYEE_EMPLOYEE)) {
			int baseIDX;
			xmlNode_t* chrNode;

			/* nation */
			const nation_t* nation = NAT_GetNationByID(cgi->XML_GetString(ssnode, SAVE_EMPLOYEE_NATION));
			if (!nation) {
				Com_Printf("No nation defined for employee\n");
				success = false;
				break;
			}
			/* UGV-Type */
			const ugv_t* ugv = cgi->Com_GetUGVByIDSilent(cgi->XML_GetString(ssnode, SAVE_EMPLOYEE_UGV));
			Employee e(emplType, nation, ugv);
			/** @note e->transfer is restored in cl_transfer.c:TR_Load */
			/* base */
			assert(B_AtLeastOneExists());	/* Just in case the order is ever changed. */
			baseIDX = cgi->XML_GetInt(ssnode, SAVE_EMPLOYEE_BASEHIRED, -1);
			e.baseHired = B_GetBaseByIDX(baseIDX);
			/* assigned to a building? */
			e.setAssigned(cgi->XML_GetBool(ssnode, SAVE_EMPLOYEE_ASSIGNED, false));
			/* Character Data */
			chrNode = cgi->XML_GetNode(ssnode, SAVE_EMPLOYEE_CHR);
			if (!chrNode) {
				Com_Printf("No character definition found for employee\n");
				success = false;
				break;
			}
			if (!cgi->GAME_LoadCharacter(chrNode, &e.chr)) {
				Com_Printf("Error loading character definition for employee\n");
				success = false;
				break;
			}
			if (lastEmployee[emplType] == nullptr)
				lastEmployee[emplType] = LIST_Add(&ccs.employees[emplType], (void*) &e, sizeof(e));
			else
				lastEmployee[emplType] = LIST_Add(&lastEmployee[emplType], (void*) &e, sizeof(e));
			if (lastEmployee[emplType] == nullptr) {
				Com_Printf("Could not add employee to the game\n");
				success = false;
				break;
			}
		}
		if (!success)
			break;
	}
	cgi->Com_UnregisterConstList(saveEmployeeConstants);

	return success;
}

/**
 * @brief Returns true if the current base is able to handle employees
 * @sa B_BaseInit_f
 */
bool E_HireAllowed (const base_t* base)
{
	if (!B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_QUARTERS))
		return true;
	return false;
}

/**
 * @brief Removes the items of an employee (soldier) from the base storage (s)he is hired at
 * @param[in] employee Pointer to the soldier whose items should be removed
 */
void E_RemoveInventoryFromStorage (Employee* employee)
{
	const character_t* chr = &employee->chr;

	assert(employee->baseHired);

	const Container* cont = nullptr;
	while ((cont = chr->inv.getNextCont(cont))) {
		Item* item = nullptr;
		while ((item = cont->getNextItem(item))) {
			/* Remove ammo */
			if (item->ammoDef() && item->ammoDef() != item->def())
				B_AddToStorage(employee->baseHired, item->ammoDef(), -1);
			/* Remove Item */
			if (item->def())
				B_AddToStorage(employee->baseHired, item->def(), -1);
		}
	}
}

static const cmdList_t debugEmployeeCmds[] = {
#ifdef DEBUG
	{"debug_listhired", E_ListHired_f, "Debug command to list all hired employee"},
	{"debug_addemployees", CL_DebugNewEmployees_f, "Debug function to add 5 new unhired employees of each type"},
#endif
	{nullptr, nullptr, nullptr}
};
/**
 * @brief This is more or less the initial
 * Bind some of the functions in this file to console-commands that you can call ingame.
 */
void E_InitStartup (void)
{
	E_InitCallbacks();
	cgi->Cmd_TableAddList(debugEmployeeCmds);
}

/**
 * @brief Closing actions for employee-subsystem
 */
void E_Shutdown (void)
{
	for (int i = EMPL_SOLDIER; i < MAX_EMPL; i++) {
		const employeeType_t emplType = (employeeType_t)i;
		cgi->LIST_Delete(&ccs.employees[emplType]);
	}

	E_ShutdownCallbacks();
	cgi->Cmd_TableRemoveList(debugEmployeeCmds);
}
