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

#define MAX_EMPLOYEES_IN_HOSPITAL 64
#define MAX_EMPLOYEES_PER_HOSPITAL 5

/** @brief This is the current selected employee for the hospital_employee menu */
static employee_t* currentEmployeeInHospital = NULL;

/** @brief This is a list of all employees that are in active healing - null terminated! */
static employee_t* employeesInHospitalList[MAX_EMPLOYEES_IN_HOSPITAL];
static int employeesInHospitalListCount = 0;

static cvar_t* mn_hosp_heal_limit = NULL;

/**
 * @brief Checks whether an employee should or can (in case of full healing) be removed
 * from the employeesInHospitalList array
 */
void HOS_CheckRemovalFromEmployeeList (employee_t* employee)
{
	int i = 0, j = 0;
	char messageBuffer[256];

	if (!employeesInHospitalListCount)
		return;

	if (employee->chr.HP >= MAX_HP) {
		for (; i < employeesInHospitalListCount; i++, j++) {
			if (employeesInHospitalList[i] == employee) {
				j++;
				Com_sprintf(messageBuffer, sizeof(messageBuffer), _("Healing of %s completed - %i active healings left\n"), employee->chr.name, employeesInHospitalListCount-1);
				employeesInHospitalListCount--;
				MN_AddNewMessage(_("Healing complete"), messageBuffer, qfalse, MSG_STANDARD, NULL);
			}
			employeesInHospitalList[i] = employeesInHospitalList[j];
		}
		for (; j < MAX_EMPLOYEES_IN_HOSPITAL; j++)
			employeesInHospitalList[j] = NULL;
#ifdef DEBUG
	} else {
		Com_DPrintf("character with %i hp\n", employee->chr.HP);
#endif
	}
}

/**
 * @brief Adds an employee to the employeesInHospitalList array
 */
void HOS_AddToEmployeeList (employee_t* employee)
{
	int i;
	/* alrady in our list? */
	for (i = 0; i < employeesInHospitalListCount; i++) {
		if (employeesInHospitalList[i] == employee)
			return;
	}
	/* overflow saftly */
	if (employeesInHospitalListCount < MAX_EMPLOYEES_IN_HOSPITAL - 1)
		employeesInHospitalList[employeesInHospitalListCount++] = employee;
}

/**
 * @brief
 * @param[in] chr Character data of an employee
 * @sa HOS_HealEmployee
 * @return true if soldiers becomes healed - false otherwise
 */
extern qboolean HOS_HealCharacter (character_t* chr, qboolean hospital)
{
	int healing = 1;

	if (hospital)
		healing = GET_HP_HEALING(chr->skills[ABILITY_POWER]);

	assert(chr);
	if (chr->HP < MAX_HP) {
		chr->HP = min(chr->HP + healing, MAX_HP);
		if (chr->HP == MAX_HP)
			return qfalse;
		return qtrue;
	} else
		return qfalse;
}

/**
 * @brief Checks what the status of a soldier is
 * @sa CL_CampaignRun
 * @note called every day
 */
extern void HOS_HospitalRun (void)
{
	int type, i;
	employee_t** employee;

	employee = employeesInHospitalList;
	/* loop until null is reached (end of list) */
	while (*employee){
		if (!HOS_HealCharacter(&(*employee)->chr, qtrue))
			HOS_CheckRemovalFromEmployeeList(*employee);
		employee++;
	}

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++)
			HOS_HealCharacter(&(gd.employees[type][i].chr), qfalse);
}

/**
 * @brief
 * @param[in] employee Employee to heal
 * @sa HOS_HealCharacter
 * @sa HOS_HealAll
 */
extern qboolean HOS_HealEmployee (employee_t* employee)
{
	assert(employee);
	return HOS_HealCharacter(&employee->chr, qtrue);
}

/**
 * @brief Heal all employees in the given base
 * @param[in] base The base the employees should become healed
 * @sa HOS_HealEmployee
 */
extern void HOS_HealAll (const base_t* const base)
{
	int i, type;

	assert(base);

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			if (E_IsInBase(&gd.employees[type][i], base))
				HOS_HealEmployee(&gd.employees[type][i]);
		}
}

/**
 * @brief Count all employees in hospital of baseCurrent
 */
static int HOS_CountHealing (void)
{
	int i, cnt = 0;

	/* should not be called when no baseCurrent was set */
	assert(baseCurrent);

	for (i = 0; i < employeesInHospitalListCount; i++) {
		if (!E_IsInBase(employeesInHospitalList[i], baseCurrent))
			continue;
		cnt++;
	}
	return cnt;
}

static char hospitalText[1024];

/**
 * @brief Script command to init the hospital menu
 * @sa HOS_EmployeeInit_f
 */
static void HOS_Init_f (void)
{
	int i, type;
	employee_t* employee = NULL;

	if (!baseCurrent)
		return;

	if (!baseCurrent->hasHospital) {
		MN_PopMenu(qfalse);
		return;
	}

	*hospitalText = '\0';
	menuText[TEXT_HOSPITAL] = hospitalText;

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* only show those employees, that are in the current base */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (employee->chr.HP < MAX_HP)
				Q_strcat(hospitalText, va(_("%s\t%s\t(%i/%i)\n"), employee->chr.name, E_GetEmployeeString(type), employee->chr.HP, MAX_HP), sizeof(hospitalText));
		}

	Cvar_SetValue("mn_hosp_medics", E_CountHired(baseCurrent, EMPL_MEDIC));
	Cvar_SetValue("mn_hosp_heal_limit", MAX_EMPLOYEES_PER_HOSPITAL * B_GetNumberOfBuildingsInBaseByType(baseCurrent->idx, B_HOSPITAL));
	Cvar_SetValue("mn_hosp_healing", HOS_CountHealing());
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
			/* only those employees, that are in the current base */
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
			/* only those employees, that are in the current base */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			employee->chr.HP--;
		}
}
#endif

/**
 * @brief Click function for hospital menu employee list
 */
static void HOS_ListClick_f (void)
{
	int num, type, i;
	employee_t* employee;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", Cmd_Argv(0));
		return;
	}

	if (!baseCurrent) {
		currentEmployeeInHospital = NULL;
		return;
	}

	/* which employee? */
	num = atoi(Cmd_Argv(1));

	for (type = 0; type < MAX_EMPL; type++)
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* only those employees, that are in the current base */
			if (!E_IsInBase(employee, baseCurrent) || employee->chr.HP >= MAX_HP)
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
		MN_PushMenu("hospital_employee");
}

static char employeeDesc[512];

/**
 * @brief This is the init function for the hospital_employee menu
 */
static void HOS_EmployeeInit_f (void)
{
	character_t* c;

	if (!currentEmployeeInHospital) {
		Com_Printf("No employee selected\n");
		return;
	}

	/* TODO */
	menuText[TEXT_STANDARD] = employeeDesc;
	*employeeDesc = '\0';

	c = &currentEmployeeInHospital->chr;

	CL_CharacterCvars(c);

	Cvar_SetValue("mn_hp", c->HP);
	Cvar_SetValue("mn_hpmax", MAX_HP);
}

/**
 * @brief Starts the healing process via hospital menu button
 */
static void HGS_StartHealing_f (void)
{
	if (!baseCurrent || !currentEmployeeInHospital)
		return;
	if (mn_hosp_heal_limit->integer > employeesInHospitalListCount)
		HOS_AddToEmployeeList(currentEmployeeInHospital);
}

/**
 * @brief Initial stuff for hospitals
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from MN_ResetMenus resp. CL_InitLocal
 */
extern void HOS_Reset (void)
{
	/* add commands */
	Cmd_AddCommand("hosp_empl_init", HOS_EmployeeInit_f, "Init function for hospital employee menu");
	Cmd_AddCommand("hosp_init", HOS_Init_f, "Init function for hospital menu");
	Cmd_AddCommand("hosp_start_healing", HGS_StartHealing_f, "Start the healing process for the current selected soldier");
	Cmd_AddCommand("hosp_list_click", HOS_ListClick_f, "Click function for hospital employee list");
#ifdef DEBUG
	Cmd_AddCommand("hosp_hurt_all", HOS_HurtAll_f, "Debug function to hurt all employees in the current base by one");
	Cmd_AddCommand("hosp_heal_all", HOS_HealAll_f, "Debug function to heal all employees in the current base completly");
#endif
	memset(hospitalText, 0, sizeof(hospitalText));
	memset(employeesInHospitalList, 0, sizeof(employeesInHospitalList));
	mn_hosp_heal_limit = Cvar_Get("mn_hosp_heal_limit", "0", 0, "Current hospital capacity (for current base)");
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_GameLoad
 * @sa CL_GameSave
 */
extern void HOS_GameSave (sizebuf_t *sb)
{
	int i;

	/* how many employees */
	MSG_WriteByte(sb, employeesInHospitalListCount);

	/* now store the employee id for each patient */
	for (i = 0; i < employeesInHospitalListCount; i++) {
		MSG_WriteByte(sb, employeesInHospitalList[i]->baseIDHired);
		MSG_WriteByte(sb, employeesInHospitalList[i]->type);
		MSG_WriteShort(sb, employeesInHospitalList[i]->idx);
	}
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_GameSave
 * @sa CL_GameLoad
 */
extern void HOS_GameLoad (sizebuf_t *sb)
{
	int i, baseID, type;

	employeesInHospitalListCount = MSG_ReadByte(sb);

	for (i = 0; i < employeesInHospitalListCount; i++) {
		baseID = MSG_ReadByte(sb);
		type = MSG_ReadByte(sb);
		employeesInHospitalList[i] = &(gd.employees[type][MSG_ReadShort(sb)]);
	}
}
