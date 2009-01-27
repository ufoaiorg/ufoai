/**
 * @file cl_hospital.c
 * @brief Most of the hospital related stuff
 * @note Hospital functions prefix: HOS_
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

#include "../client.h"
#include "../cl_global.h"
#include "../cl_team.h"
#include "cl_hospital.h"
#include "../cl_actor.h"
#include "../menu/m_popup.h"

/** @brief This is the current selected employee for the hospital_employee menu. */
static employee_t* currentEmployeeInHospital = NULL;

/** @brief First line in Hospital menu. */
static int hospitalFirstEntry;

/** @brief Number of all entries in Hospital menu. */
static int hospitalNumEntries;

/**
 * @brief Heals character.
 * @param[in] chr Character data of an employee
 * @param[in] hospital True if we call this function for hospital healing (false when autoheal).
 * @sa HOS_HealEmployee
 * @return true if soldiers becomes healed - false otherwise
 */
qboolean HOS_HealCharacter (character_t* chr, qboolean hospital)
{
	float healing = 1.0f;

	if (hospital)
		healing = GET_HP_HEALING(chr->score.skills[ABILITY_POWER]);

	assert(chr);
	if (chr->HP < chr->maxHP) {
		/* if the character has less that 100 hitpoints, he will be disadvantaged by using the percentage
		 * method of allocating hitpoints.  So just give the character "healing" as Hitpoints, otherwise
		 * allocate "healing" as a percentage of the characters total hitpoints. */
		if (chr->maxHP < 100)
			chr->HP = min(chr->HP + healing, chr->maxHP);
		else
			chr->HP = min(chr->HP + ((healing / 100.0f) * chr->maxHP), chr->maxHP);

		if (chr->HP == chr->maxHP)
			return qfalse;
		return qtrue;
	} else
		return qfalse;
}

/**
 * @brief Checks health status of all employees in all bases.
 * @sa CL_CampaignRun
 * @note Called every day.
 */
void HOS_HospitalRun (void)
{
	int type, i;

		for (type = 0; type < MAX_EMPL; type++) {
			for (i = 0; i < gd.numEmployees[type]; i++) {
				employee_t *employee = &gd.employees[type][i];
				if (!employee->baseHired || !employee->hired)
					continue;

				if (B_GetBuildingStatus(employee->baseHired, B_HOSPITAL))
					HOS_HealCharacter(&(employee->chr), qtrue);
				else
					HOS_HealCharacter(&(employee->chr), qfalse);
			}
		}
}

/**
 * @brief Callback for HOS_HealCharacter() in hospital.
 * @param[in] employee Pointer to the employee to heal.
 * @sa HOS_HealCharacter
 * @sa HOS_HealAll
 */
qboolean HOS_HealEmployee (employee_t* employee)
{
	assert(employee);
	return HOS_HealCharacter(&employee->chr, qtrue);
}

/**
 * @brief Heal all employees in the given base
 * @param[in] base The base the employees should become healed
 * @sa HOS_HealEmployee
 */
void HOS_HealAll (const base_t* const base)
{
	int i, type;

	assert(base);

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			if (E_IsInBase(&gd.employees[type][i], base))
				HOS_HealEmployee(&gd.employees[type][i]);
		}
}

/** @brief Maximal entries in hospital menu. */
#define HOS_MENU_MAX_ENTRIES 21

/** @brief Number of entries in a line of the hospital menu. */
#define HOS_MENU_LINE_ENTRIES 3

/**
 * @brief Updates the hospital menu.
 * @sa HOS_Init_f
 */
static void HOS_UpdateMenu (void)
{
	char name[128];
	char rank[128];
	int i, j, type;
	int entry;

	/* Reset list. */
	MN_ExecuteConfunc("hospital_clear");

	for (type = 0, j = 0, entry = 0; type < MAX_EMPL; type++) {
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee_t *employee = &gd.employees[type][i];
			/* Only show those employees, that are in the current base. */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			/* Don't show soldiers who are gone in mission */
			if (E_IsAwayFromBase(employee))
				continue;
			/* Don't show healthy employees */
			if (employee->chr.HP >= employee->chr.maxHP)
				continue;

			if (j >= hospitalFirstEntry && entry < HOS_MENU_MAX_ENTRIES) {
				/* Print name. */
				Q_strncpyz(name, employee->chr.name, sizeof(name));
				/* Print rank for soldiers or type for other personel. */
				if (type == EMPL_SOLDIER)
					Q_strncpyz(rank, _(gd.ranks[employee->chr.score.rank].name), sizeof(rank));
				else
					Q_strncpyz(rank, E_GetEmployeeString(employee->type), sizeof(rank));
				Com_DPrintf(DEBUG_CLIENT, "%s idx: %i entry: %i\n", name, employee->idx, entry);
				/* If the employee is seriously wounded (HP <= 50% maxHP), make him red. */
				if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.5))
					MN_ExecuteConfunc("hospitalserious %i", entry);
				/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */
				else if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.85))
					MN_ExecuteConfunc("hospitalmedium %i", entry);
				else
					MN_ExecuteConfunc("hospitallight %i", entry);

				/* Display name in the correct list-entry. */
				Cvar_Set(va("mn_hos_item%i", entry), name);
				/* Display rank in the correct list-entry. */
				Cvar_Set(va("mn_hos_rank%i", entry), rank);
				/* Prepare the health bar */
				Cvar_Set(va("mn_hos_hp%i", entry), va("%i", employee->chr.HP));
				Cvar_Set(va("mn_hos_hpmax%i", entry), va("%i", employee->chr.maxHP));
				/* Increase the counter of list entries. */
				entry++;
			}
			j++;
		}
	}
	hospitalNumEntries = j;

	/* Set rest of the list-entries to have no text at all. */
	for (; entry < HOS_MENU_MAX_ENTRIES; entry++) {
		Cvar_Set(va("mn_hos_item%i", entry), "");
		Cvar_Set(va("mn_hos_rank%i", entry), "");
		Cvar_Set(va("mn_hos_hp%i", entry), "0");
		Cvar_Set(va("mn_hos_hpmax%i", entry), "1");
	}
}

/**
 * @brief Script command to init the hospital menu.
 * @sa HOS_EmployeeInit_f
 */
static void HOS_Init_f (void)
{
	if (!baseCurrent)
		return;

	if (!B_GetBuildingStatus(baseCurrent, B_HOSPITAL)) {
		MN_PopMenu(qfalse);
		return;
	}

	hospitalFirstEntry = 0;

	HOS_UpdateMenu();

}

#ifdef DEBUG
/**
 * @brief Set the character HP field to maxHP.
 */
static void HOS_HealAll_f (void)
{
	int i, type;
	employee_t* employee;

	if (!baseCurrent)
		return;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* only those employees, that are in the current base */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			employee->chr.HP = employee->chr.maxHP;
		}
}

/**
 * @brief Decrement the character HP field by one.
 */
static void HOS_HurtAll_f (void)
{
	int i, type, amount;
	employee_t* employee;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));
	else
		amount = 1;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* only those employees, that are in the current base */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			employee->chr.HP = max(0, employee->chr.HP - amount);
		}
}
#endif

/**
 * @brief Click function for scrolling up the employee list.
 */
static void HOS_ListUp_f (void)
{
	if (hospitalFirstEntry >= HOS_MENU_LINE_ENTRIES)
		hospitalFirstEntry -= HOS_MENU_LINE_ENTRIES;

	HOS_UpdateMenu();
}

/**
 * @brief Click function for scrolling down the employee list.
 */
static void HOS_ListDown_f (void)
{
	if (hospitalFirstEntry + HOS_MENU_MAX_ENTRIES < hospitalNumEntries)
		hospitalFirstEntry += HOS_MENU_LINE_ENTRIES;

	HOS_UpdateMenu();
}

/**
 * @brief Click function for hospital menu employee list.
 */
static void HOS_ListClick_f (void)
{
	int num, type, i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent) {
		currentEmployeeInHospital = NULL;
		return;
	}

	/* which employee? */
	num = atoi(Cmd_Argv(1)) + hospitalFirstEntry;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee_t *employee = &gd.employees[type][i];
			/* only those employees, that are in the current base */
			if (!E_IsInBase(employee, baseCurrent) || employee->chr.HP >= employee->chr.maxHP)
				continue;
			/* Don't select soldiers who are gone in mission */
			if (E_IsAwayFromBase(employee))
				continue;
			if (!num) {
				currentEmployeeInHospital = employee;
				/* end outer loop, too */
				type = MAX_EMPL + 1;
				/* end inner loop */
				break;
			}
			num--;
		}

	/* open the hospital menu for this employee */
	if (type != MAX_EMPL)
		MN_PushMenu("hospital_employee", NULL);
}

static char employeeDesc[512];

/**
 * @brief This is the init function for the hospital_employee menu
 */
static void HOS_EmployeeInit_f (void)
{
	character_t* c;

	if (!currentEmployeeInHospital) {
		Com_Printf("HOS_EmployeeInit_f: no employee selected.\n");
		return;
	}

	/** @todo */
	mn.menuText[TEXT_STANDARD] = employeeDesc;
	*employeeDesc = '\0';

	c = &currentEmployeeInHospital->chr;
	assert(c);
	CL_CharacterCvars(c);

	Cvar_SetValue("mn_hp", c->HP);
	Cvar_SetValue("mn_hpmax", c->maxHP);
}

/**
 * @brief Initial stuff for hospitals
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_InitStartup resp. CL_InitLocal
 */
void HOS_InitStartup (void)
{
	/* add commands */
	Cmd_AddCommand("hosp_empl_init", HOS_EmployeeInit_f, "Init function for hospital employee menu");
	Cmd_AddCommand("hosp_init", HOS_Init_f, "Init function for hospital menu");
	Cmd_AddCommand("hosp_list_click", HOS_ListClick_f, "Click function for hospital employee list");
	Cmd_AddCommand("hosp_list_up", HOS_ListUp_f, "Scroll up function for hospital employee list");
	Cmd_AddCommand("hosp_list_down", HOS_ListDown_f, "Scroll down function for hospital employee list");
#ifdef DEBUG
	Cmd_AddCommand("debug_hosp_hurt_all", HOS_HurtAll_f, "Debug function to hurt all employees in the current base by one");
	Cmd_AddCommand("debug_hosp_heal_all", HOS_HealAll_f, "Debug function to heal all employees in the current base completely");
#endif
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_Load
 * @sa SAV_GameSave
 */
qboolean HOS_Save (sizebuf_t *sb, void* data)
{
	/* nothing to save here */
	return qtrue;
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_Save
 * @sa SAV_GameLoad
 */
qboolean HOS_Load (sizebuf_t *sb, void* data)
{
	return qtrue;
}

/**
 * @brief Returns true if you can enter in the hospital
 * @sa B_BaseInit_f
 */
qboolean HOS_HospitalAllowed (const base_t* base)
{
	if (base->baseStatus != BASE_UNDER_ATTACK
	 && B_GetBuildingStatus(base, B_HOSPITAL)) {
		return qtrue;
	} else {
		return qfalse;
	}
}
