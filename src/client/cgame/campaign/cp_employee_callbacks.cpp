/**
 * @file
 * @brief Header file for menu callback functions used for hire/employee menu
 */

/*
All original material Copyright (C) 2002-2014 UFO: Alien Invasion.

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
#include "../../cl_team.h" /* CL_UpdateCharacterValues */
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_employee_callbacks.h"
#include "cp_employee.h"


/** Currently selected employee. */
static Employee* selectedEmployee = nullptr;
/* Holds the current active employee category */
static int employeeCategory = 0;

static const int maxEmployeesPerPage = 14;

static int employeeScrollPos = 0;

/* List of (hired) emplyoees in the current category (employeeType). */
static linkedList_t* employeeList;	/** @sa E_GetEmployeeByMenuIndex */
/* How many employees in current list (changes on every category change, too) */
static int employeesInCurrentList;

/**
 * @brief Update GUI with the current number of employee per category
 */
static void E_UpdateGUICount_f (void)
{
	int max;
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	max = CAP_GetMax(base, CAP_EMPLOYEES);
	cgi->Cvar_SetValue("mn_hiresoldiers", E_CountHired(base, EMPL_SOLDIER));
	cgi->Cvar_SetValue("mn_hireworkers", E_CountHired(base, EMPL_WORKER));
	cgi->Cvar_SetValue("mn_hirescientists", E_CountHired(base, EMPL_SCIENTIST));
	cgi->Cvar_SetValue("mn_hirepilots", E_CountHired(base, EMPL_PILOT));
	cgi->Cvar_Set("mn_hirepeople", "%d/%d", E_CountAllHired(base), max);
}

static void E_EmployeeSelect (Employee* employee)
{
	const base_t* base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	selectedEmployee = employee;
	if (selectedEmployee) {
		const character_t* chr = &selectedEmployee->chr;
		/* mn_employee_hired is needed to allow renaming */
		cgi->Cvar_SetValue("mn_employee_hired", selectedEmployee->isHired() ? 1 : 0);
		cgi->Cvar_SetValue("mn_ucn", chr->ucn);

		/* set info cvars */
		CL_UpdateCharacterValues(chr);
	}
}

/**
 * @brief Click function for employee_list node
 * @sa E_EmployeeList_f
 */
static void E_EmployeeListScroll_f (void)
{
	int j, cnt = 0;
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2)
		return;

	employeeScrollPos = atoi(cgi->Cmd_Argv(1));
	j = employeeScrollPos;

	E_Foreach(employeeCategory, employee) {
		/* don't show employees of other bases */
		if (employee->isHired() && !employee->isHiredInBase(base))
			continue;
		/* don't show employees being transferred to other bases */
		if (employee->transfer)
			continue;

		/* drop the first j entries */
		if (j) {
			j--;
			continue;
		}
		/* change the buttons */
		if (employee->isHired()) {
			if (employee->isAwayFromBase())
				cgi->UI_ExecuteConfunc("employeedisable %i", cnt);
			else
				cgi->UI_ExecuteConfunc("employeefire %i", cnt);
		} else {
			cgi->UI_ExecuteConfunc("employeehire %i", cnt);
		}

		cnt++;

		/* only 19 buttons */
		if (cnt >= maxEmployeesPerPage)
			break;
	}

	for (;cnt < maxEmployeesPerPage; cnt++) {
		cgi->Cvar_Set(va("mn_name%i", cnt), "");
		cgi->UI_ExecuteConfunc("employeehide %i", cnt);
	}

	cgi->UI_ExecuteConfunc("hire_fix_scroll %i", employeeScrollPos);
}

/**
 * @brief Will fill the list with employees
 * @note this is the init function in the employee hire menu
 */
static void E_EmployeeList_f (void)
{
	Employee* employee;
	int hiredEmployeeIdx;
	linkedList_t* employeeListName;
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category> <employeeid>\n", cgi->Cmd_Argv(0));
		return;
	}

	employeeCategory = atoi(cgi->Cmd_Argv(1));
	if (employeeCategory >= MAX_EMPL || employeeCategory < 0)
		employeeCategory = EMPL_SOLDIER;

	if (cgi->Cmd_Argc() == 3)
		hiredEmployeeIdx = atoi(cgi->Cmd_Argv(2));
	else
		hiredEmployeeIdx = -1;

	/* reset the employee count */
	employeesInCurrentList = 0;

	cgi->LIST_Delete(&employeeList);
	/* make sure, that we are using the linked list */
	cgi->UI_ResetData(TEXT_LIST);
	employeeListName = nullptr;

	E_Foreach(employeeCategory, e) {
		/* don't show employees of other bases */
		if (e->isHired() && !e->isHiredInBase(base))
			continue;
		/* don't show employees being transferred to other bases */
		if (e->transfer)
			continue;
		cgi->LIST_AddPointer(&employeeListName, e->chr.name);
		cgi->LIST_AddPointer(&employeeList, e);
		employeesInCurrentList++;
	}
	cgi->UI_RegisterLinkedListText(TEXT_LIST, employeeListName);

	/* If the list is empty OR we are in pilots/scientists/workers-mode: don't show the model&stats. */
	/** @note
	 * 0 == nothing is displayed
	 * 1 == all is displayed
	 * 2 == only stuff wanted for scientists/workers/pilots are displayed
	 */
	/** @todo replace magic numbers - use confuncs */
	if (employeesInCurrentList == 0) {
		cgi->Cvar_Set("mn_show_employee", "0");
	} else {
		if (employeeCategory == EMPL_SCIENTIST || employeeCategory == EMPL_WORKER)
			cgi->Cvar_Set("mn_show_employee", "3");
		else if (employeeCategory == EMPL_PILOT)
			cgi->Cvar_Set("mn_show_employee", "2");
		else
			cgi->Cvar_Set("mn_show_employee", "1");
	}
	/* Select the current employee if name was changed or first one. Use the direct string
	 * execution here - otherwise the employeeCategory might be out of sync */
	if (hiredEmployeeIdx < 0 || selectedEmployee == nullptr)
		employee = E_GetEmployeeByMenuIndex(0);
	else
		employee = selectedEmployee;

	E_EmployeeSelect(employee);

	/* update scroll */
	cgi->UI_ExecuteConfunc("hire_update_number %i", employeesInCurrentList);
	cgi->UI_ExecuteConfunc("employee_scroll 0");
}


/**
 * @brief Change the name of the selected actor.
 * @note called via "employee_changename"
 */
static void E_ChangeName_f (void)
{
	Employee* employee = E_GetEmployeeFromChrUCN(cgi->Cvar_GetInteger("mn_ucn"));
	if (!employee)
		return;

	/* employee name should not contain " */
	if (!Com_IsValidName(cgi->Cvar_GetString("mn_name"))) {
		cgi->Cvar_ForceSet("mn_name", employee->chr.name);
		return;
	}

	Q_strncpyz(employee->chr.name, cgi->Cvar_GetString("mn_name"), sizeof(employee->chr.name));
}

/**
 * @brief Fill employeeList with a list of employees in the current base (i.e. they are hired and not transferred)
 * @sa E_GetEmployeeByMenuIndex - It is used to get a specific entry from the generated employeeList.
 */
int E_GenerateHiredEmployeesList (const base_t* base)
{
	assert(base);
	employeesInCurrentList = E_GetHiredEmployees(base, EMPL_SOLDIER, &employeeList);
	return employeesInCurrentList;
}

/**
 * @brief Find an hired or free employee by the menu index
 * @param[in] num The index from the hire menu screen (index inemployeeList).
 */
Employee* E_GetEmployeeByMenuIndex (int num)
{
	return static_cast<Employee*>(cgi->LIST_GetByIdx(employeeList, num));
}

/**
 * @brief This removes an employee from the global list so that
 * he/she is no longer hireable.
 */
static void E_EmployeeDelete_f (void)
{
	/* Check syntax. */
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* num - menu index (line in text) */
	int num = employeeScrollPos + atoi(cgi->Cmd_Argv(1));

	Employee* employee = E_GetEmployeeByMenuIndex(num);
	/* empty slot selected */
	if (!employee)
		return;

	if (employee->isHired()) {
		if (!employee->unhire()) {
			cgi->UI_DisplayNotice(_("Could not fire employee"), 2000, "employees");
			Com_DPrintf(DEBUG_CLIENT, "Couldn't fire employee\n");
			return;
		}
	}
	E_DeleteEmployee(employee);
	cgi->Cbuf_AddText("employee_init %i\n", employeeCategory);

	num = std::max(0, num - 1);
	cgi->Cbuf_AddText("employee_select %i\n", num);
	cgi->Cbuf_AddText("hire_select %i\n", num);

	cgi->Cbuf_AddText("employee_update_count\n");
}

/**
 * @brief Callback for employee_hire command
 * @note a + as parameter indicates, that more than @c maxEmployeesPerPage are possible
 * @sa CL_AssignSoldier_f
 */
static void E_EmployeeHire_f (void)
{
	base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* Check syntax. */
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <+num>\n", cgi->Cmd_Argv(0));
		return;
	}

	const char* arg = cgi->Cmd_Argv(1);

	/* check whether this is called with the text node click function
	 * with values from 0 - #available employees (bigger values than
	 * maxEmployeesPerPage) possible ... *
	 * num - menu index (line in text), button - number of button */
	int num, button;
	if (arg[0] == '+') {
		num = atoi(arg + 1);
		button = num - employeeScrollPos;
	/* ... or with the hire pictures that are using only values from
	 * 0 - maxEmployeesPerPage */
	} else {
		button = atoi(cgi->Cmd_Argv(1));
		num = button + employeeScrollPos;
	}

	Employee* employee = E_GetEmployeeByMenuIndex(num);
	/* empty slot selected */
	if (!employee)
		return;

	if (employee->isHired()) {
		if (!employee->unhire()) {
			Com_DPrintf(DEBUG_CLIENT, "Couldn't fire employee\n");
			cgi->UI_DisplayNotice(_("Could not fire employee"), 2000, "employees");
		} else {
			cgi->UI_ExecuteConfunc("employeehire %i", button);
		}
	} else {
		if (!E_HireEmployee(base, employee)) {
			Com_DPrintf(DEBUG_CLIENT, "Couldn't hire employee\n");
			cgi->UI_DisplayNotice(_("Could not hire employee"), 2000, "employees");
			cgi->UI_ExecuteConfunc("employeehire %i", button);
		} else {
			cgi->UI_ExecuteConfunc("employeefire %i", button);
		}
	}
	E_EmployeeSelect(employee);
	cgi->Cbuf_AddText("hire_select %i\n", num);

	E_UpdateGUICount_f();
}

/**
 * @brief Callback function that updates the character cvars when calling employee_select
 */
static void E_EmployeeSelect_f (void)
{
	/* Check syntax. */
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int num = atoi(cgi->Cmd_Argv(1));
	if (num < 0 || num >= employeesInCurrentList)
		return;

	E_EmployeeSelect(E_GetEmployeeByMenuIndex(num));
}

static const cmdList_t employeeCmds[] = {
	{"employee_update_count", E_UpdateGUICount_f, "Callback to update the employee count of the current GUI"},
	{"employee_init", E_EmployeeList_f, "Init function for employee hire menu"},
	{"employee_delete", E_EmployeeDelete_f, "Removed an employee from the global employee list"},
	{"employee_hire", E_EmployeeHire_f, nullptr},
	{"employee_select", E_EmployeeSelect_f, nullptr},
	{"employee_changename", E_ChangeName_f, "Change the name of an employee"},
	{"employee_scroll", E_EmployeeListScroll_f, "Scroll callback for employee list"},
	{nullptr, nullptr, nullptr}
};

void E_InitCallbacks (void)
{
	Cmd_TableAddList(employeeCmds);
}

void E_ShutdownCallbacks (void)
{
	Cmd_TableRemoveList(employeeCmds);
}
