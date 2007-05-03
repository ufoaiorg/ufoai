/**
 * @file cl_hospital.c
 * @brief Most of the hospital related stuff
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

#include "client.h"
#include "cl_global.h"

#define MAX_EMPLOYEES_IN_HOSPITAL 64
#define MAX_EMPLOYEES_PER_HOSPITAL 5

/** @brief This is the current selected employee for the hospital_employee menu */
static employee_t* currentEmployeeInHospital = NULL;

/** @brief This is a list of all employees that are in active healing - null terminated! */
static employee_t* employeesInHospitalList[MAX_EMPLOYEES_IN_HOSPITAL];
static int employeesInHospitalListCount = 0;

/** @brief This is a list of all employees that were healing in hospital but leave hospital during a mission */
static employee_t* employeesHealingInMissionList[MAX_EMPLOYEES_IN_HOSPITAL];
static int employeesHealingInMissionListCount = 0;

/** @brief Hospital employee list in Hospital menu. */
static employee_t *hospitalList[MAX_EMPLOYEES_IN_HOSPITAL];

/** @brief Length of list in Hospital menu. */
static int HOSPITAL_LIST_LENGHT;

/**
 * @brief Remove an employee from a list (employeesInHospitalList or employeesHealingInMissionList)
 * @return qtrue is the removal is OK
 * @return qfalse else
 */
qboolean HOS_RemoveFromList (employee_t* employee, employee_t** listEmployee, int counter)
{
	int i = 0, j = 0;
	qboolean test = qfalse;

	for (; j < counter; i++, j++) {
		if (listEmployee[i] == employee) {
			j++;
			test = qtrue;
		}
		listEmployee[i] = listEmployee[j];
	}
	for (; i < MAX_EMPLOYEES_IN_HOSPITAL; i++)
		listEmployee[i] = NULL;

	if (test)
		return qtrue;
	else
		return qfalse;
}

/**
 * @brief Checks whether an employee should or can (in case of full healing) be removed
 * from the employeesInHospitalList array
 */
void HOS_CheckRemovalFromEmployeeList (employee_t* employee)
{
	base_t *base;
	char messageBuffer[256];

	if (!employeesInHospitalListCount)
		return;

	if (employee->chr.HP >= employee->chr.maxHP) {
		employee->chr.HP = employee->chr.maxHP;
		if (HOS_RemoveFromList (employee, employeesInHospitalList, employeesInHospitalListCount)) {
			employeesInHospitalListCount--;
			Com_sprintf(messageBuffer, sizeof(messageBuffer), ngettext("Healing of %s completed - %i active healing left\n", "Healing of %s completed - %i active healings left\n", employeesInHospitalListCount), employee->chr.name, employeesInHospitalListCount);
			MN_AddNewMessage(_("Healing complete"), messageBuffer, qfalse, MSG_STANDARD, NULL);
			base = &gd.bases[employee->baseIDHired];
			assert (base);
			base->capacities[CAP_HOSPSPACE].cur--;
		} else {
			Com_Printf("Didn't find employee %s in employeesInHospitalList\n",employee->chr.name);
		}
#ifdef DEBUG
	} else {
		Com_DPrintf("character with %i hp\n", employee->chr.HP);
#endif
	}
}

/**
 * @brief Remove dead employees from employeesHealingInMissionList
 */
void HOS_RemoveDeadEmployees (void)
{
	int i = 0, j = 0;
	int type;
	employee_t* employee = NULL;
	qboolean test = qtrue;

	for (; i < employeesHealingInMissionListCount; i++) {
		test = qtrue;
		type = employeesHealingInMissionList[i]->type;
		for (j = 0; j < gd.numEmployees[type]; j++) {
			employee = &gd.employees[type][j];
			if (employee->idx == employeesHealingInMissionList[i]->idx) {
				test = qfalse;
				break;
			}
		}
		if (test) {
			if (HOS_RemoveFromList(employeesHealingInMissionList[i], employeesHealingInMissionList, employeesHealingInMissionListCount))
				employeesHealingInMissionListCount--;
			else
				Com_Printf("Didn't find employee %s in employeesHealingInMissionList\n",employee->chr.name);
		}
	}
}

/**
 * @brief Adds an employee to the employeesInHospitalList array.
 * @param[in] *base Pointer to the base with hospital, where the given employee will be healed.
 * @param[in] *employee Pointer to the employee being added to hospital.
 */
void HOS_AddToEmployeeList (base_t *base, employee_t* employee)
{
	int i;
	/* already in our list? */
	for (i = 0; i < employeesInHospitalListCount; i++) {
		if (employeesInHospitalList[i] == employee)
			return;
	}

	employeesInHospitalList[employeesInHospitalListCount++] = employee;
	base->capacities[CAP_HOSPSPACE].cur++;
	return;

#if 0
	/* If I am not wrong, we don't need this - base capacities won't allow to add
	   more employees than max capacity. 01052007 Zenerka. */
	/* overflow saftly */
	if (employeesInHospitalListCount < MAX_EMPLOYEES_IN_HOSPITAL - 1)
		employeesInHospitalList[employeesInHospitalListCount++] = employee;
#endif
}

/**
 * @brief Adds an employee to the employeesHealingInMissionList array
 */
void HOS_AddToInMissionEmployeeList (employee_t* employee)
{
	int i;
	/* already in our list? */
	for (i = 0; i < employeesHealingInMissionListCount; i++) {
		if (employeesHealingInMissionList[i] == employee)
			return;
	}
	/* overflow saftly */
	if (employeesHealingInMissionListCount < MAX_EMPLOYEES_IN_HOSPITAL - 1)
		employeesHealingInMissionList[employeesHealingInMissionListCount++] = employee;
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
	if (chr->HP < chr->maxHP) {
		chr->HP = min(chr->HP + healing, chr->maxHP);
		if (chr->HP == chr->maxHP)
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

#if 0
/* Not needed - the amount of employees being healed is stored in
   base->capacities[CAP_HOSPSPACE].cur 01052007 Zenerka */
/**
 * @brief Count all employees in hospital of baseCurrent
 */
static int HOS_CountHealing (base_t* base)
{
	int i, cnt = 0;

	assert(base);

	for (i = 0; i < employeesInHospitalListCount; i++) {
		if (!E_IsInBase(employeesInHospitalList[i], base))
			continue;
		cnt++;
	}
	return cnt;
}
#endif

static char hospitalText[1024];

/**
 * @brief Script command to init the hospital menu
 * @sa HOS_EmployeeInit_f
 */
static void HOS_Init_f (void)
{
	char name[128];
	int i, j, k, type;
	employee_t* employee = NULL;

	if (!baseCurrent)
		return;

	if (!baseCurrent->hasHospital) {
		MN_PopMenu(qfalse);
		return;
	}

	for (type = 0, j = 0; type < MAX_EMPL; type++) {
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* Only show those employees, that are in the current base. */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			if (employee->chr.HP < employee->chr.maxHP) {
				/* Print rank for soldiers or type for other personel. */
				if (type == EMPL_SOLDIER)
					Com_sprintf(name, sizeof(name), _(gd.ranks[employee->chr.rank].shortname));
				else
					Com_sprintf(name, sizeof(name), E_GetEmployeeString(employee->type));
				Q_strcat(name, " ", sizeof(name));
				/* Print name. */
				Q_strcat(name, employee->chr.name, sizeof(name));
				Q_strcat(name, " ", sizeof(name));
				/* Print HP stats. */
				Q_strcat(name, va("(%i/%i)", employee->chr.HP, employee->chr.maxHP), sizeof(name));
				Com_Printf("%s j: %i\n", name, j);
				/* If the employee is seriously wounded (HP <= 50% maxHP), make him red. */
				if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.5))
					Cbuf_AddText(va("hospitalserious%i\n", j));
				/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */	
				else if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.85))
					Cbuf_AddText(va("hospitallight%i\n", j));
				/* If the employee is currently being healed, make him blue. */
				for (k = 0; k < employeesInHospitalListCount; k++) {
					if (employeesInHospitalList[k] == employee) {
						Cbuf_AddText(va("hospitalheal%i\n", j));
						break;
					}
				}
				/* Display text in the correct list-entry. */
				Cvar_Set(va("mn_hos_item%i", j), name);	
				/* Assign the employee to proper position on the list. */
				hospitalList[j] = &gd.employees[type][i];
				/* Increase the counter of list entries. */
				j++;
			}
		}
	}
	
	HOSPITAL_LIST_LENGHT = j;

	/* Set rest of the list-entries to have no text at all. */
	for (; j < 27; j++) {
		Cvar_Set(va("mn_hos_item%i", j), "");
	}

	Cvar_SetValue("mn_hosp_medics", E_CountHired(baseCurrent, EMPL_MEDIC));
	Cvar_SetValue("mn_hosp_heal_limit", baseCurrent->capacities[CAP_HOSPSPACE].max);
	Cvar_SetValue("mn_hosp_healing", baseCurrent->capacities[CAP_HOSPSPACE].cur);
}

#ifdef DEBUG
/**
 * @brief Set the character HP field to maxHP
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
			employee->chr.HP = employee->chr.maxHP;
		}
}

/**
 * @brief Decrement the character HP field by one
 */
static void HOS_HurtAll_f (void)
{
	int i, type, amount = 1;
	employee_t* employee = NULL;

	if (!baseCurrent)
		return;

	if (Cmd_Argc() == 2)
		amount = atoi(Cmd_Argv(1));

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
			if (!E_IsInBase(employee, baseCurrent) || employee->chr.HP >= employee->chr.maxHP)
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

	/* @todo */
	menuText[TEXT_STANDARD] = employeeDesc;
	*employeeDesc = '\0';

	c = &currentEmployeeInHospital->chr;

	CL_CharacterCvars(c);

	Cvar_SetValue("mn_hp", c->HP);
	Cvar_SetValue("mn_hpmax", c->maxHP);
}

/**
 * @brief Starts the healing process via hospital menu button
 */
static void HGS_StartHealing_f (void)
{
	if (!baseCurrent || !currentEmployeeInHospital)
		return;

	assert (baseCurrent);

	/* Add to the hospital if there is a room for the employee. */
	if (baseCurrent->capacities[CAP_HOSPSPACE].cur < baseCurrent->capacities[CAP_HOSPSPACE].max) {
		HOS_AddToEmployeeList(baseCurrent, currentEmployeeInHospital);
		Cbuf_AddText("hosp_init;mn_pop\n");
	} else {
		memset(popupText, 0, sizeof(popupText));
		/* No room for employee. */
		if (currentEmployeeInHospital->type == EMPL_SOLDIER) {
			Q_strcat(popupText, va("%s %s",
			_(gd.ranks[currentEmployeeInHospital->chr.rank].shortname),
			currentEmployeeInHospital->chr.name),
			sizeof(popupText));
		} else {
			Q_strcat(popupText, va("%s %s",
			E_GetEmployeeString(currentEmployeeInHospital->type),
			currentEmployeeInHospital->chr.name),
			sizeof(popupText));
		}
		Q_strcat(popupText, _(" needs to be placed in hospital,\nbut there is not enough room!\n\nBuild more hospitals.\n"),
		sizeof(popupText));

		MN_Popup(_("Not enough hospital space"), popupText);
		return;
	}
}

/**
 * @brief Move soldiers leaving base in aircraft from employeesInHospitalList to employeesHealingInMissionList
 * @sa AIM_AircraftStart_f
 * @sa AIR_SendAircraftToMission
 * @todo Soldiers should also be removed from employeesInHospitalList during transfers
 * @todo All employees of a base should also be moved from employeesInHospitalList to employeesHealingInMissionList during base attack.
 */
extern void CL_RemoveEmployeesInHospital (aircraft_t *aircraft)
{
	int i;
	int j;
	employee_t* employee;

	/* select all soldiers from aircraft who are healing in an hospital */
	for (i = 0; i < aircraft->size; i++) {
		if (aircraft->teamIdxs[i] > -1) {
			for (j = 0; j < employeesInHospitalListCount; j++) {
				employee = employeesInHospitalList[j];
				if ((employee->type == EMPL_SOLDIER) && (aircraft->teamIdxs[i] == employee->idx)) {
					HOS_AddToInMissionEmployeeList(employee);
					if (HOS_RemoveFromList (employee, employeesInHospitalList, employeesInHospitalListCount))
						employeesInHospitalListCount--;
				}
			}
		}
	}
}

/**
 * @brief Move soldiers leaving base in aircraft from employeesHealingInMissionList to employeesInHospitalList
 * @sa CL_CampaignRunAircraft
 * @todo All alive employees of a base should be moved from employeesHealingInMissionList to employeesInHospitalList after base attack
 */
extern void CL_ReaddEmployeesInHospital (aircraft_t *aircraft)
{
	int i;
	int j;
	employee_t* employee;

	/* select all soldiers from aircraft who were healing in an hospital */
	for (i = 0; i < aircraft->size; i++) {
		if (aircraft->teamIdxs[i] > -1) {
			for (j = 0; j < employeesHealingInMissionListCount; j++) {
				employee = employeesHealingInMissionList[j];
				if ((employee->type == EMPL_SOLDIER) && (aircraft->teamIdxs[i] == employee->idx)) {
					/* Soldier goes back to hospital only if he hasn't recover all his HP during mission (medikit) */
					if (employee->chr.HP < employee->chr.maxHP)
						HOS_AddToEmployeeList(aircraft->homebase, employee);

					if (HOS_RemoveFromList (employee, employeesHealingInMissionList, employeesHealingInMissionListCount))
						employeesHealingInMissionListCount--;
				}
			}
		}
	}

	/* Clean up employeesHealingInMissionList of dead people */
	HOS_RemoveDeadEmployees();
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
	memset(employeesHealingInMissionList, 0, sizeof(employeesHealingInMissionList));
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_Load
 * @sa SAV_GameSave
 */
extern qboolean HOS_Save (sizebuf_t *sb, void* data)
{
	int i;

	/* how many employees */
	MSG_WriteShort(sb, employeesInHospitalListCount);

	/* now store the employee id for each patient */
	for (i = 0; i < employeesInHospitalListCount; i++) {
		MSG_WriteByte(sb, employeesInHospitalList[i]->baseIDHired);
		MSG_WriteByte(sb, employeesInHospitalList[i]->type);
		MSG_WriteShort(sb, employeesInHospitalList[i]->idx);
	}
	return qtrue;
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_Save
 * @sa SAV_GameLoad
 */
extern qboolean HOS_Load (sizebuf_t *sb, void* data)
{
	int i, baseID, type;

	employeesInHospitalListCount = MSG_ReadShort(sb);

	for (i = 0; i < employeesInHospitalListCount; i++) {
		baseID = MSG_ReadByte(sb);
		type = MSG_ReadByte(sb);
		employeesInHospitalList[i] = &(gd.employees[type][MSG_ReadShort(sb)]);
	}
	return qtrue;
}
