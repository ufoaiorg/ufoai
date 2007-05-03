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

#include "client.h"
#include "cl_global.h"

/** @brief This is the current selected employee for the hospital_employee menu. */
static employee_t* currentEmployeeInHospital = NULL;

/** @brief Hospital employee list in Hospital menu. */
static employee_t *hospitalList[MAX_EMPLOYEES];

/** @brief Length of list in Hospital menu. */
static int HOSPITAL_LIST_LENGHT;

/**
 * @brief Remove an employee from a hospitalList.
 * @param[in] *employee Pointer to the employee to remove.
 * @param[in] *base Pointer to the base where the employee is hired.
 * @return qtrue if the removal is OK, qfalse if it is not
 */
qboolean HOS_RemoveFromList (employee_t *employee, base_t *base)
{
	int i = 0, j = 0;
	qboolean test = qfalse;

	assert (base);

	for (; j < base->hospitalListCount; i++, j++) {
		if (base->hospitalList[employee->type][i] == employee->idx) {
			j++;
			test = qtrue;
		}
		base->hospitalList[employee->type][i] = base->hospitalList[employee->type][j];
	}
	for (; i < base->capacities[CAP_HOSPSPACE].cur; i++)
		base->hospitalList[employee->type][i] = -1;

	if (test) {
		base->hospitalListCount--;
		base->capacities[CAP_HOSPSPACE].cur--;
		return qtrue;
	} else
		return qfalse;
}

/**
 * @brief Checks whether an employee should be removed from Hospital healing list.
 * @param[in] *employee Pointer to the employee to check.
 */
void HOS_CheckRemovalFromEmployeeList (employee_t *employee)
{
	base_t *base;
	char messageBuffer[256];

	assert (employee);
	base = &gd.bases[employee->baseIDHired];
	assert (base);

	if (base->hospitalListCount <= 0)
		return;
	if (!base->hasHospital)
		return;

	if (employee->chr.HP >= employee->chr.maxHP) {
		employee->chr.HP = employee->chr.maxHP;
		if (HOS_RemoveFromList(employee, base)) {
			/* @todo: better text handling - "0 current healings" sounds silly. */
			Com_sprintf(messageBuffer, sizeof(messageBuffer), ngettext("Healing of %s completed - %i active healing left\n", "Healing of %s completed - %i active healings left\n", base->hospitalListCount), employee->chr.name, base->hospitalListCount);
			MN_AddNewMessage(_("Healing complete"), messageBuffer, qfalse, MSG_STANDARD, NULL);
		} else {
			Com_Printf("HOS_CheckRemovalFromEmployeeList()... Didn't find employee %s in Hospital healing list\n",employee->chr.name);
		}
#ifdef DEBUG
	} else {
		Com_DPrintf("character with %i hp\n", employee->chr.HP);
#endif
	}
}

/**
 * @brief Adds an employee to the Hospital Healing list.
 * @param[in] *base Pointer to the base with hospital, where the given employee will be healed.
 * @param[in] *employee Pointer to the employee being added to hospital.
 */
void HOS_AddToEmployeeList (base_t *base, employee_t* employee)
{
	int i;
	/* Already in our list? */
	for (i = 0; i < base->hospitalListCount; i++) {
		if (base->hospitalList[employee->type][i] == employee->idx)
			return;
	}
	if (base->capacities[CAP_HOSPSPACE].cur < base->capacities[CAP_HOSPSPACE].max) {
		base->hospitalList[employee->type][base->hospitalListCount] = employee->idx;
		base->hospitalListCount++;
		base->capacities[CAP_HOSPSPACE].cur++;
		return;
	} else {
		/* @todo popup here - for the case of readding soldiers returning from a mission */
		Com_Printf("HOS_AddToEmployeeList(), couldn't add employee to hospital, max capacity reached.\n");
	}
}

/**
 * @brief Adds an employee to the base->hospitalMissionList.
 * @param[in] *employee Pointer to the employee to add.
 * @param[in] *base Pointer to the base where we will add given employee.
 */
void HOS_AddToInMissionEmployeeList (employee_t* employee, base_t *base)
{
	int i;

	assert (base);
	/* Do nothing if the base does not have hospital. */
	if (!base->hasHospital)
		return;

	/* Already in our list? */
	for (i = 0; i < base->hospitalMissionListCount; i++) {
		if (base->hospitalMissionList[i] == employee->idx)
			return;
	}

	/* Add an employee. */
	if (base->hospitalMissionListCount < MAX_TEAMLIST) {
		base->hospitalMissionList[base->hospitalMissionListCount] = employee->idx;
		base->hospitalMissionListCount++;
	}
}

/**
 * @brief Heals character.
 * @param[in] chr Character data of an employee
 * @param[in] hospital True if we call this function for hospital healing (false when autoheal).
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
 * @brief Checks health status of all employees in all bases.
 * @sa CL_CampaignRun
 * @note Called every day.
 */
extern void HOS_HospitalRun (void)
{
	int type, i, j, k;
	employee_t *employee;
	base_t *base;
	qboolean test = qfalse;

	for (j = 0, base = gd.bases; j < gd.numBases; j++, base++) {
		if (base->founded == qfalse)
			continue;

		assert (base);
		for (type = 0; type < MAX_EMPL; type++) {
			for (i = 0; i < gd.numEmployees[type]; i++) {
				employee = &gd.employees[type][i];
				/* Don't try to do anything if employee is not on the list. */
				for (k = 0; k < base->hospitalListCount; k++) {
					if (base->hospitalList[type][k] == employee->idx) {
						if (!HOS_HealCharacter(&(employee->chr), qtrue)) {
							HOS_CheckRemovalFromEmployeeList(employee);
							test = qtrue;
						}
					}
				}
				/* If we did not just remove employee from the list, try to heal. */
				if (!test)
					HOS_HealCharacter(&(gd.employees[type][i].chr), qfalse);
			}
		}
	}
}

/**
 * @brief Callback for HOS_HealCharacter() in hospital.
 * @param[in] *employee Pointer to the employee to heal.
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
 * @brief Script command to init the hospital menu.
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

	/* Prepare default list in default color. */
	Cbuf_AddText("hospital_clear\n");

	for (type = 0, j = 0; type < MAX_EMPL; type++) {
		for (i = 0; i < gd.numEmployees[type]; i++) {
			employee = &gd.employees[type][i];
			/* Only show those employees, that are in the current base. */
			if (!E_IsInBase(employee, baseCurrent))
				continue;
			/* Don't show soldiers which are going to or returning from a mission. */
			/* @todo: implement me. */
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
				Com_DPrintf("%s idx: %i j: %i\n", name, employee->idx, j);
				/* If the employee is seriously wounded (HP <= 50% maxHP), make him red. */
				if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.5))
					Cbuf_AddText(va("hospitalserious%i\n", j));
				/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */
				else if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.85))
					Cbuf_AddText(va("hospitallight%i\n", j));
				/* If the employee is currently being healed, make him blue. */
				for (k = 0; k < baseCurrent->hospitalListCount; k++) {
					if (baseCurrent->hospitalList[type][k] == employee->idx) {
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
 * @brief Set the character HP field to maxHP.
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
 * @brief Decrement the character HP field by one.
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
 * @brief Click function for hospital menu employee list.
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
		Com_Printf("HOS_EmployeeInit_f()... no employee selected.\n");
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
 * @brief Starts the healing process via hospital menu button.
 */
static void HOS_StartHealing_f (void)
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
		/* No room for employee - prepare a popup. */
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
 * @brief Moves soldiers leaving base between hospital arrays in base_t.
 * @param[in] *aircraft Pointer to an aircraft with team onboard.
 * @sa AIM_AircraftStart_f
 * @sa AIR_SendAircraftToMission
 * @todo Soldiers should also be removed from base->hospitalList during transfers
 * @todo All employees of a base should also be moved from base->hospitalList to base->hospitalMissionList during base attack.
 */
extern void HOS_RemoveEmployeesInHospital (aircraft_t *aircraft)
{
	int i;
	int j;
	employee_t *employee;
	base_t *base;

	assert (aircraft);
	base = aircraft->homebase;
	assert (base);

	/* Do nothing if the base does not have hospital. */
	if (!base->hasHospital)
		return;
	/* First of all, make sure that relevant array is empty. */
	memset(base->hospitalMissionList, -1, sizeof(base->hospitalMissionList));
	base->hospitalMissionListCount = 0;

	/* select all soldiers from aircraft who are healing in an hospital */
	for (i = 0; i < aircraft->size; i++) {
		if (aircraft->teamIdxs[i] > -1) {
			/* Check whether any team member is in base->hospitalList. */
			employee = &(gd.employees[EMPL_SOLDIER][i]);
			assert (employee);
			for (j = 0; j < base->hospitalListCount; j++) {
				/* If the soldier is in base->hospitalList, remove him ... */
				if (base->hospitalList[EMPL_SOLDIER][j] == employee->idx) {
					HOS_RemoveFromList(employee, base);
					/* ...and put him to base->hospitalMissionList. */
					if (base->hospitalMissionListCount < MAX_TEAMLIST)
						HOS_AddToInMissionEmployeeList(employee, base);
				}
			}
		}
	}
}

/**
 * @brief Moves soldiers returning from a mission between hospital arrays in base_t.
 * @sa CL_DropshipReturned
 * @todo All alive employees of a base should be moved from base->hospitalMissionList to base->hospitalList after base attack.
 */
extern void HOS_ReaddEmployeesInHospital (aircraft_t *aircraft)
{
	int i;
	int j;
	employee_t* employee;
	base_t *base;

	assert (aircraft);
	base = aircraft->homebase;
	assert (base);

	/* Do nothing if the base does not have hospital. */
	if (!base->hasHospital)
		return;

	for (i = 0; i < aircraft->size; i++) {
		if (aircraft->teamIdxs[i] > -1) {
			for (j = 0; j < base->hospitalMissionListCount; j++) {
				if ((aircraft->teamIdxs[i] == base->hospitalMissionList[j])) {
					employee = &(gd.employees[EMPL_SOLDIER][i]);
					assert (employee);
					/* Soldier goes back to hospital only if he hasn't recover all his HP during mission (medikit). */
					if (employee->chr.HP < employee->chr.maxHP)
						HOS_AddToEmployeeList(base, employee);
				}
			}
		}
	}
	/* Now we can erase the base->hospitalMissionList array - all soldiers has been added to hospital. */
	memset(base->hospitalMissionList, -1, sizeof(base->hospitalMissionList));
	base->hospitalMissionListCount = 0;
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
	Cmd_AddCommand("hosp_start_healing", HOS_StartHealing_f, "Start the healing process for the current selected soldier");
	Cmd_AddCommand("hosp_list_click", HOS_ListClick_f, "Click function for hospital employee list");
#ifdef DEBUG
	Cmd_AddCommand("hosp_hurt_all", HOS_HurtAll_f, "Debug function to hurt all employees in the current base by one");
	Cmd_AddCommand("hosp_heal_all", HOS_HealAll_f, "Debug function to heal all employees in the current base completly");
#endif
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_Load
 * @sa SAV_GameSave
 */
extern qboolean HOS_Save (sizebuf_t *sb, void* data)
{
	/* nothing to save here - all saved in cl_basemanagement.c */
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

	/* version 2.1.1 had some data stored here */
	if (*(int*)data == 1) {	/* 2.1.1 */
#if 0
		employeesInHospitalListCount = MSG_ReadShort(sb);

		for (i = 0; i < employeesInHospitalListCount; i++) {
			baseID = MSG_ReadByte(sb);
			type = MSG_ReadByte(sb);
			employeesInHospitalList[i] = &(gd.employees[type][MSG_ReadShort(sb)]);
		}
#endif
	}
	return qtrue;
}

