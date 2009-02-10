/**
 * @file cp_employee_callbacks.c
 * @brief Header file for menu callback functions used for hire/employee menu
 */

/*
All original materal Copyright (C) 2002-2009 UFO: Alien Invasion team.

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
#include "../client.h"
#include "../cl_game.h"
#include "cp_employee_callbacks.h"
#include "cl_employee.h"
#include "../menu/m_nodes.h"
#include "../menu/node/m_node_window.h"
#include "../menu/m_draw.h"
#include "../cl_actor.h"


/** Currently selected employee. @sa cl_employee.h */
employee_t *selectedEmployee = NULL;
/* Holds the current active employee category */
static int employeeCategory = 0;

static const int maxEmployeesPerPage = 15;

/* the menu node of the employee list */
static menuNode_t *employeeListNode;

/* List of (hired) employees in the current category (employeeType). */
linkedList_t *employeeList;	/** @sa E_GetEmployeeByMenuIndex */

/* How many employees in current list (changes on every catergory change, too) */
int employeesInCurrentList;

/**
 * @brief Update GUI with the current number of employee per category
 */
static void E_UpdateGUICount_f (void)
{
	int max;
	assert(baseCurrent);
	max = baseCurrent->capacities[CAP_EMPLOYEES].max;
	Cvar_SetValue("mn_hiresoldiers", E_CountHired(baseCurrent, EMPL_SOLDIER));
	Cvar_SetValue("mn_hireworkers", E_CountHired(baseCurrent, EMPL_WORKER));
	Cvar_SetValue("mn_hirescientists", E_CountHired(baseCurrent, EMPL_SCIENTIST));
	Cvar_SetValue("mn_hirepilots", E_CountHired(baseCurrent, EMPL_PILOT));
	Cvar_Set("mn_hirepeople", va("%d/%d", E_CountAllHired(baseCurrent), max));
}

/**
 * @brief Click function for employee_list node
 * @sa E_EmployeeList_f
 */
static void E_EmployeeListClick_f (void)
{
	int num; /* clicked at which position? determined by node format string */

	if (Cmd_Argc() < 2)
		return;

	num = atoi(Cmd_Argv(1));

	if (num < 0 || num >= employeesInCurrentList)
		return;

	/* the + indicates, that values bigger than maxEmployeesPerPage could be possible */
	Cmd_ExecuteString(va("employee_select +%i\n", num));
}

/**
 * @brief Click function for employee_list node
 * @sa E_EmployeeList_f
 */
static void E_EmployeeListScroll_f (void)
{
	int i, j, cnt = 0;
	employee_t* employee;

	if (!baseCurrent)
		return;

	j = employeeListNode->u.text.textScroll;

	for (i = 0, employee = &(ccs.employees[employeeCategory][0]); i < ccs.numEmployees[employeeCategory]; i++, employee++) {
		/* don't show employees of other bases */
		if (employee->baseHired != baseCurrent && employee->hired)
			continue;

		/* drop the first j entries */
		if (j) {
			j--;
			continue;
		}
		/* change the buttons */
		if (employee->hired) {
			if (employee->baseHired == baseCurrent)
				MN_ExecuteConfunc("employeeadd %i", cnt);
			else
				MN_ExecuteConfunc("employeedisable %i", cnt);
		} else
			MN_ExecuteConfunc("employeedel %i", cnt);

		cnt++;

		/* only 19 buttons */
		if (cnt >= maxEmployeesPerPage)
			break;
	}

	for (;cnt < maxEmployeesPerPage; cnt++) {
		Cvar_ForceSet(va("mn_name%i", cnt), "");
		MN_ExecuteConfunc("employeedisable %i", cnt);
	}

	MN_ExecuteConfunc("hire_fix_scroll %i", employeeListNode->u.text.textScroll);
}

/**
 * @brief Will fill the list with employees
 * @note this is the init function in the employee hire menu
 */
static void E_EmployeeList_f (void)
{
	int i, j;
	employee_t* employee;
	int hiredEmployeeIdx;
	linkedList_t *employeeListName;

	/* can be called from everywhere without a started game */
	if (!baseCurrent || !GAME_CP_IsRunning())
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <category> <employeeid>\n", Cmd_Argv(0));
		return;
	}

	employeeCategory = atoi(Cmd_Argv(1));
	if (employeeCategory >= MAX_EMPL || employeeCategory < 0)
		employeeCategory = EMPL_SOLDIER;

	if (Cmd_Argc() == 3)
		hiredEmployeeIdx = atoi(Cmd_Argv(2));
	else
		hiredEmployeeIdx = -1;

	/* reset the employee count */
	employeesInCurrentList = 0;

	LIST_Delete(&employeeList);

	if (hiredEmployeeIdx < 0) {
		/* Reset scrolling when no specific entry was given. */
		/** @todo Is there a case where hiredEmployeeIdx is < 0 and the
		 * textScroll must be reset? */
		employeeListNode->u.text.textScroll = 0;
	} else {
		/** @todo If employee is given but outside the current visible list
		 * (defined by employeeListNode->textScroll) we need to calculate the
		 * new employeeListNode->textScroll */
	}

	/* make sure, that we are using the linked list */
	MN_MenuTextReset(TEXT_LIST);
	employeeListName = NULL;

	/** @todo Use CL_GetTeamList and reduce code duplication */
	for (j = 0, employee = ccs.employees[employeeCategory]; j < ccs.numEmployees[employeeCategory]; j++, employee++) {
		/* don't show employees of other bases */
		if (employee->baseHired != baseCurrent && employee->hired)
			continue;

		LIST_AddPointer(&employeeListName, employee->chr.name);
		LIST_AddPointer(&employeeList, employee);
		/* Change/Display the buttons if the employee is currently displayed (i.e. visible in the on-screen list) .*/
		/** @todo Check if the "textScroll % maxEmployeesPerPage" calculation
		 * is still ok when _very_ long lists (i.e. more than 2x19 right now) are used. */
		if (employeesInCurrentList >= employeeListNode->u.text.textScroll
		 && employeesInCurrentList < employeeListNode->u.text.textScroll + maxEmployeesPerPage) {
			if (employee->hired) {
				if (employee->baseHired == baseCurrent) {
					if (employee->transfer)
						Cvar_ForceSet(va("mn_name%i", employeesInCurrentList), va(_("%s [Transferred]"), employee->chr.name));
					else
						Cvar_ForceSet(va("mn_name%i", employeesInCurrentList), employee->chr.name);
					Cbuf_AddText(va("employeeadd %i\n", employeesInCurrentList - (employeeListNode->u.text.textScroll % maxEmployeesPerPage)));
				} else
					Cbuf_AddText(va("employeedisable %i\n", employeesInCurrentList - (employeeListNode->u.text.textScroll % maxEmployeesPerPage)));
			} else
				Cbuf_AddText(va("employeedel %i\n", employeesInCurrentList));
		}
		employeesInCurrentList++;
	}
	MN_RegisterLinkedListText(TEXT_LIST, employeeListName);

	/* If the list is empty OR we are in pilots/scientists/workers-mode: don't show the model&stats. */
	/** @note
	 * 0 == nothing is displayed
	 * 1 == all is displayed
	 * 2 == only stuff wanted for scientists/workers/pilots are displayed
	 */
	if (employeesInCurrentList == 0) {
		Cvar_Set("mn_show_employee", "0");
	} else {
		if ((employeeCategory == EMPL_PILOT)
		 || (employeeCategory == EMPL_SCIENTIST)
		 || (employeeCategory == EMPL_WORKER)) {
			Cvar_Set("mn_show_employee", "2");
		} else {
			Cvar_Set("mn_show_employee", "1");
		}
	}

	i = employeesInCurrentList;
	for (;i < maxEmployeesPerPage;i++) {
		Cvar_ForceSet(va("mn_name%i", i), "");
		Cbuf_AddText(va("employeedisable %i\n", i));
	}

	/* Select the current employee if name was changed or first one. Use the direct string
	 * execution here - otherwise the employeeCategory might be out of sync */
	if (hiredEmployeeIdx < 0)
		Cmd_ExecuteString("employee_select 0\n");
	else
		Cmd_ExecuteString(va("employee_select %i\n", selectedEmployee ? selectedEmployee->idx : 0));

	/* update scroll */
	MN_ExecuteConfunc("hire_update_number %i", employeesInCurrentList);
}


/**
 * @brief Change the name of the selected actor.
 * @sa CL_MessageMenu_f
 */
static void E_ChangeName_f (void)
{
	employee_t *employee = selectedEmployee;

	/* Maybe called without base initialized or active. */
	if (!baseCurrent || !GAME_IsCampaign())
		return;

	if (employee)
		Q_strncpyz(employee->chr.name, Cvar_VariableString("mn_name"), sizeof(employee->chr.name));
}

/**
 * @brief Fill employeeList with a list of employees in the current base (i.e. they are hired and not transferred)
 * @note Depends on cls.displayHeavyEquipmentList to be set correctly.
 * @sa E_GetEmployeeByMenuIndex - It is used to get a specific entry from the generated employeeList.
 * @note If base is NULL all employees of all bases are added to the list - especially useful for none-campaign mode games
 */
int E_GenerateHiredEmployeesList (const base_t *base)
{
	const employeeType_t employeeType =
		cls.displayHeavyEquipmentList
			? EMPL_ROBOT
			: EMPL_SOLDIER;

	employeesInCurrentList = E_GetHiredEmployees(base, employeeType, &employeeList);
	return employeesInCurrentList;
}


/**
 * @brief Find an hired or free employee by the menu index
 * @param[in] num The index from the hire menu screen (index inemployeeList).
 */
employee_t* E_GetEmployeeByMenuIndex (int num)
{
	int i;
	linkedList_t *emplList = employeeList;

	if (num >= employeesInCurrentList || num < 0)
		return NULL;

	i = 0;
	while (emplList) {
		employee_t* employee = (employee_t*)emplList->data;
		if (i == num)
			return employee;
		i++;
		emplList = emplList->next;
	}

	return NULL;
}


/**
 * @brief This removes an employee from the global list so that
 * he/she is no longer hireable.
 */
static void E_EmployeeDelete_f (void)
{
	/* num - menu index (line in text) */
	int num;
	const int scroll = employeeListNode->u.text.textScroll;
	employee_t* employee;

	if (!baseCurrent)
		return;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	num += employeeListNode->u.text.textScroll;

	employee = E_GetEmployeeByMenuIndex(num);
	/* empty slot selected */
	if (!employee)
		return;

	if (employee->hired) {
		if (!E_UnhireEmployee(employee)) {
			/** @todo message - Couldn't fire employee. */
			Com_DPrintf(DEBUG_CLIENT, "Couldn't fire employee\n");
			return;
		}
	}
	E_DeleteEmployee(employee, employee->type);
	Cbuf_AddText(va("employee_init %i\n", employeeCategory));
	employeeListNode->u.text.textScroll = scroll;
}

/**
 * @brief Callback for employee_hire command
 * @note a + as parameter indicates, that more than @c maxEmployeesPerPage are possible
 * @sa CL_AssignSoldier_f
 */
static void E_EmployeeHire_f (void)
{
	/* num - menu index (line in text), button - number of button */
	int num, button;
	const char *arg;
	employee_t* employee;

	if (!baseCurrent)
		return;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <+num>\n", Cmd_Argv(0));
		return;
	}

	arg = Cmd_Argv(1);

	/* check whether this is called with the text node click function
	 * with values from 0 - #available employees (bigger values than
	 * maxEmployeesPerPage) possible ... */
	if (arg[0] == '+') {
		num = atoi(arg + 1);
		button = num - employeeListNode->u.text.textScroll;
	/* ... or with the hire pictures that are using only values from
	 * 0 - maxEmployeesPerPage */
	} else {
		button = atoi(Cmd_Argv(1));
		num = button + employeeListNode->u.text.textScroll;
	}

	employee = E_GetEmployeeByMenuIndex(num);
	/* empty slot selected */
	if (!employee)
		return;

	if (employee->hired) {
		if (!E_UnhireEmployee(employee)) {
			Com_DPrintf(DEBUG_CLIENT, "Couldn't fire employee\n");
			MN_DisplayNotice(_("Could not fire employee"), 2000);
		} else
			Cbuf_AddText(va("employeedel %i\n", button));
	} else {
		if (!E_HireEmployee(baseCurrent, employee)) {
			Com_DPrintf(DEBUG_CLIENT, "Couldn't hire employee\n");
			MN_DisplayNotice(_("Could not hire employee"), 2000);
		} else
			Cbuf_AddText(va("employeeadd %i\n", button));
	}
	Cbuf_AddText(va("employee_select %i\n", num));
	E_UpdateGUICount_f();
}

/**
 * @brief Callback function that updates the character cvars when calling employee_select
 */
static void E_EmployeeSelect_f (void)
{
	int num;

	/* Check syntax. */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= employeesInCurrentList)
		return;

	selectedEmployee = E_GetEmployeeByMenuIndex(num);
	if (selectedEmployee) {
		/* mn_employee_hired is needed to allow renaming */
		Cvar_SetValue("mn_employee_hired", selectedEmployee->hired ? 1 : 0);

		/* set info cvars */
		CL_CharacterCvars(&(selectedEmployee->chr));

		/* Set the value of the selected line in the scroll-text to the correct number. */
		MN_TextNodeSelectLine(employeeListNode, num);
	}
}

void E_InitCallbacks (void)
{
	employeeListNode = MN_GetNodeByPath("employees.employee_list");
	if (!employeeListNode)
		Sys_Error("Could not find the 'employee_list' node in 'employees' menu\n");

	Cmd_AddCommand("employee_update_count", E_UpdateGUICount_f, "Callback to update the employee count of the current GUI");
	Cmd_AddCommand("employee_list_click", E_EmployeeListClick_f, "Callback for employee_list click function");

	/* add commands */
	Cmd_AddCommand("employee_init", E_EmployeeList_f, "Init function for employee hire menu");
	Cmd_AddCommand("employee_delete", E_EmployeeDelete_f, "Removed an employee from the global employee list");
	Cmd_AddCommand("employee_hire", E_EmployeeHire_f, NULL);
	Cmd_AddCommand("employee_select", E_EmployeeSelect_f, NULL);
	Cmd_AddCommand("employee_changename", E_ChangeName_f, "Change the name of an employee");
	Cmd_AddCommand("employee_scroll", E_EmployeeListScroll_f, "Scroll callback for employee list");

}

void E_ShutdownCallbacks (void)
{
	Cmd_RemoveCommand("employee_update_count");
	Cmd_RemoveCommand("employee_list_click");
	Cmd_RemoveCommand("employee_init");
	Cmd_RemoveCommand("employee_delete");
	Cmd_RemoveCommand("employee_hire");
	Cmd_RemoveCommand("employee_select");
	Cmd_RemoveCommand("employee_changename");
	Cmd_RemoveCommand("employee_scroll");

}
