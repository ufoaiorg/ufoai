/**
 * @file cl_hospital.c
 * @brief Most of the hospital related stuff
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

static employee_t* currentEmployeeInHospital = NULL;

/**
 * @brief
 * @param[in] chr Character data of an employee
 * @sa HOS_HealEmployee
 * @return true if soldiers becomes healed - false otherwise
 */
extern qboolean HOS_HealCharacter(character_t* chr)
{
	assert(chr);
	if (chr->HP < MAX_HP) {
		Com_Printf("TODO: heal him\n");
	}
	return qtrue;
}

/**
 * @brief
 * @param[in] employee Employee to heal
 * @sa HOS_HealCharacter
 * @sa HOS_HealAll
 */
extern qboolean HOS_HealEmployee(employee_t* employee)
{
	assert(employee);
	return HOS_HealCharacter(&employee->chr);
}

/**
 * @brief Heal all employees in the given base
 * @param[in] base The base the employees should become healed
 * @sa HOS_HealEmployee
 */
extern void HOS_HealAll(const base_t const* base)
{
	int i, type;

	assert(base);

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			if (E_IsInBase(&gd.employees[type][i], base))
				HOS_HealEmployee(&gd.employees[type][i]);
		}
}

static char hospitalText[1024];

/**
 * @brief Script command to init the hospital menu
 */
static void HOS_Init (void)
{
	int i, type;
	employee_t* employee = NULL;

	if (!baseCurrent)
		return;

	*hospitalText = '\0';
	menuText[TEXT_HOSPITAL] = hospitalText;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (employee->chr.HP < MAX_HP)
				Q_strcat(hospitalText, va(_("%s\t%s\t(%i/%i)\n"), employee->chr.name, E_GetEmployeeString(type), employee->chr.HP, MAX_HP), sizeof(hospitalText));
		}
}

#ifdef DEBUG
/**
 * @brief Set the character HP field to MAX_HP
 */
static void HOS_HealAll_f (void)
{
	int i, type;
	employee_t* employee = NULL;

	if (!baseCurrent)
		return;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			employee->chr.HP = MAX_HP;
		}
}

/**
 * @brief Decrement the character HP field by one
 */
static void HOS_HurtAll_f (void)
{
	int i, type;
	employee_t* employee = NULL;

	if (!baseCurrent)
		return;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			employee->chr.HP--;
		}
}
#endif

/**
 * @brief Click function for hospital menu employee list
 */
void HOS_ListClick_f (void)
{
	int num, type, i;
	employee_t* employee;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent)
		return;

	/* which employee? */
	num = atoi(Cmd_Argv(1));

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			num--;
			if (!num) {
				currentEmployeeInHospital = employee;
				/* end outer loop, too */
				type = MAX_EMPL;
				/* end inner loop */
				break;
			}
		}

	/* open the hospital menu for this employee */
	MN_PushMenu("hospital_employee");
}

/**
 * @brief This is the init function for the hospital_employee menu
 */
void HOS_EmployeeInit (void)
{
	if (!currentEmployeeInHospital) {
		Com_Printf("No employee selected\n");
		return;
	}
}

/**
 * @brief Initial stuff for hospitals
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
extern void HOS_Reset(void)
{
	/* add commands */
	Cmd_AddCommand("hosp_empl_init", HOS_EmployeeInit, "Init function for hospital employee menu");
	Cmd_AddCommand("hosp_init", HOS_Init, "Init function for hospital menu");
	Cmd_AddCommand("hosp_list_click", HOS_ListClick_f, "Click function for hospital employee list");
#ifdef DEBUG
	Cmd_AddCommand("hosp_hurt_all", HOS_HurtAll_f, "Debug function to hurt all employees in the current base by one");
	Cmd_AddCommand("hosp_heal_all", HOS_HealAll_f, "Debug function to heal all employees in the current base completly");
#endif
	memset(hospitalText, 0, sizeof(hospitalText));
}
