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

/**
 * @brief Prints information about the current employee
 */
static void EM_EmployeeInfo (void)
{
	static char employeeInfo[512];
	menuText[TEXT_EMPLOYEE_INFO] = employeeInfo;
}

/**
 * @brief Click function for employee list
 */
static void EM_EmployeeListRightClick_f (void)
{
}

/**
 * @brief Click function for employee list
 */
static void EM_EmployeeListClick_f (void)
{
}

/**
 * @brief Will fill the list with employees
 */
static void EM_EmployeeList (void)
{
	static char employeeList[1024];
	static char employeeAmount[256];
	employeeAmount[0] = employeeList[0] = '\0';

	/* can be called from everywhere without a started game */
	if (!baseCurrent ||!curCampaign)
		return;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: employee_init <category>\n");
		return;
	}
	employeeCategory = atoi(Cmd_Argv(1));

	/* bind the menu text to our static char array */
	menuText[TEXT_EMPLOYEE_LIST] = employeeList;
	/* bind the amount of available items */
	menuText[TEXT_EMPLOYEE_AMOUNT] = employeeAmount;
	/* now print the information about the current employee */
	EM_EmployeeInfo();
}

/**
  * @brief This is more or less the initial
  * Bind some of the functions in this file to console-commands that you can call ingame.
  * Called from MN_ResetMenus resp. CL_InitLocal
  */
void EM_ResetEmployee(void)
{
	/* add commands */
	Cmd_AddCommand("employee_init", EM_EmployeeList);
	Cmd_AddCommand("employeelist_rclick", EM_EmployeeListRightClick_f);
	Cmd_AddCommand("employeelist_click", EM_EmployeeListClick_f);
}

