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
			/* TODO: Check whether the employee is in the given base */
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
	menuText[TEXT_HOSPITAL] = NULL;
	/* TODO: Print all hurted employees in the current base */
	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* TODO: Check whether the employee is in the given base */
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
	Cmd_AddCommand("hosp_init", HOS_Init, NULL);

	memset(hospitalText, 0, sizeof(hospitalText));
}
