/**
 * @file
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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
#include "cp_hospital_callbacks.h"

/** @brief This is the current selected employee for the hospital_employee menu. */
static Employee* currentEmployeeInHospital = NULL;

/** @brief First line in Hospital menu. */
static int hospitalFirstEntry;

/** @brief Number of all entries in Hospital menu. */
static int hospitalNumEntries;

/** @brief Maximal entries in hospital menu. */
#define HOS_MENU_MAX_ENTRIES 21

/** @brief Number of entries in a line of the hospital menu. */
#define HOS_MENU_LINE_ENTRIES 3

static void HOS_EntryWoundData (const character_t *const chr, const int entry)
{
	const BodyData *bodyData = chr->teamDef->bodyTemplate;
	int bodyPart;
	const woundInfo_t *const wounds = &chr->wounds;

	for (bodyPart = 0; bodyPart < bodyData->numBodyParts(); ++bodyPart) {
		if (wounds->treatmentLevel[bodyPart] != 0) {
			const float severity = static_cast<float>(wounds->treatmentLevel[bodyPart]) / chr->maxHP;
			char text[MAX_VAR];
			Com_sprintf(text, lengthof(text), CHRSH_IsTeamDefRobot(chr->teamDef) ? _("Damaged %s (damage: %i)") :
					_("Wounded %s (damage: %i)"), _(bodyData->name(bodyPart)), wounds->treatmentLevel[bodyPart]);
			cgi->UI_ExecuteConfunc("hospital_wounds %i %s %f \"%s\"", entry, bodyData->id(bodyPart), severity, text);
		}
	}
}

static float HOS_InjuryLevel(const character_t *const chr)
{
	float injuryLevel = 0;
	int i;

	for (i = 0; i < chr->teamDef->bodyTemplate->numBodyParts(); ++i)
		injuryLevel += static_cast<float>(chr->wounds.treatmentLevel[i]) / chr->maxHP;

	return injuryLevel;
}

/**
 * @brief Updates the hospital menu.
 * @sa HOS_Init_f
 */
static void HOS_UpdateMenu (void)
{
	char name[128];
	char rank[128];
	int j, type;
	int entry;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	/* Reset list. */
	cgi->UI_ExecuteConfunc("hospital_clear");

	for (type = 0, j = 0, entry = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			float injuryLevel = HOS_InjuryLevel(&employee->chr);
			if (!E_IsInBase(employee, base))
				continue;
			/* Don't show soldiers who are gone in mission */
			if (E_IsAwayFromBase(employee))
				continue;
			/* Don't show healthy employees */
			if (employee->chr.HP >= employee->chr.maxHP && injuryLevel <= 0)
				continue;

			if (j >= hospitalFirstEntry && entry < HOS_MENU_MAX_ENTRIES) {
				/* Print name. */
				Q_strncpyz(name, employee->chr.name, sizeof(name));
				/* Print rank for soldiers or type for other personel. */
				if (type == EMPL_SOLDIER) {
					const rank_t *rankPtr = CL_GetRankByIdx(employee->chr.score.rank);
					Q_strncpyz(rank, _(rankPtr->name), sizeof(rank));
				} else
					Q_strncpyz(rank, E_GetEmployeeString(employee->type, 1), sizeof(rank));
				Com_DPrintf(DEBUG_CLIENT, "%s ucn: %i entry: %i\n", name, employee->chr.ucn, entry);
				/* If the employee is seriously wounded (HP <= 50% maxHP), make him red. */
				if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.5) || injuryLevel >= 0.5)
					cgi->UI_ExecuteConfunc("hospitalserious %i", entry);
				/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */
				else if (employee->chr.HP <= (int) (employee->chr.maxHP * 0.85) || injuryLevel >= 0.15)
					cgi->UI_ExecuteConfunc("hospitalmedium %i", entry);
				else
					cgi->UI_ExecuteConfunc("hospitallight %i", entry);

				/* Display name in the correct list-entry. */
				cgi->Cvar_Set(va("mn_hos_item%i", entry), name);
				/* Display rank in the correct list-entry. */
				cgi->Cvar_Set(va("mn_hos_rank%i", entry), rank);
				/* Prepare the health bar */
				cgi->Cvar_Set(va("mn_hos_hp%i", entry), va("%i", employee->chr.HP));
				cgi->Cvar_Set(va("mn_hos_hpmax%i", entry), va("%i", employee->chr.maxHP));

				/* Send wound info */
				HOS_EntryWoundData(&employee->chr, entry);

				/* Increase the counter of list entries. */
				entry++;
			}
			j++;
		}
	}
	hospitalNumEntries = j;

	/* Set rest of the list-entries to have no text at all. */
	for (; entry < HOS_MENU_MAX_ENTRIES; entry++) {
		cgi->Cvar_Set(va("mn_hos_item%i", entry), "");
		cgi->Cvar_Set(va("mn_hos_rank%i", entry), "");
		cgi->Cvar_Set(va("mn_hos_hp%i", entry), "0");
		cgi->Cvar_Set(va("mn_hos_hpmax%i", entry), "1");
		cgi->UI_ExecuteConfunc("hospitalunused %i", entry);
	}
}

/**
 * @brief Script command to init the hospital menu.
 * @sa HOS_EmployeeInit_f
 */
static void HOS_Init_f (void)
{
	base_t *base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	if (!B_GetBuildingStatus(base, B_HOSPITAL)) {
		cgi->UI_PopWindow(false);
		return;
	}

	hospitalFirstEntry = 0;

	HOS_UpdateMenu();
}

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
	int num, type;
	base_t *base = B_GetCurrentSelectedBase();

	if (!base) {
		currentEmployeeInHospital = NULL;
		return;
	}

	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <arg>\n", cgi->Cmd_Argv(0));
		return;
	}

	/* which employee? */
	num = atoi(cgi->Cmd_Argv(1)) + hospitalFirstEntry;

	for (type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			if (!E_IsInBase(employee, base))
				continue;
			/* only those that need healing */
			if (employee->chr.HP >= employee->chr.maxHP && HOS_InjuryLevel(&employee->chr) <= 0)
				continue;
			/* Don't select soldiers that are gone to a mission */
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
	}

	/* open the hospital menu for this employee */
	if (type != MAX_EMPL)
		cgi->UI_PushWindow("hospital_employee");
}

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

	cgi->UI_ResetData(TEXT_STANDARD);

	c = &currentEmployeeInHospital->chr;
	CL_UpdateCharacterValues(c);

	cgi->Cvar_SetValue("mn_hp", c->HP);
	cgi->Cvar_SetValue("mn_hpmax", c->maxHP);
}


void HOS_InitCallbacks(void)
{
	cgi->Cmd_AddCommand("hosp_empl_init", HOS_EmployeeInit_f, "Init function for hospital employee menu");
	cgi->Cmd_AddCommand("hosp_init", HOS_Init_f, "Init function for hospital menu");
	cgi->Cmd_AddCommand("hosp_list_click", HOS_ListClick_f, "Click function for hospital employee list");
	cgi->Cmd_AddCommand("hosp_list_up", HOS_ListUp_f, "Scroll up function for hospital employee list");
	cgi->Cmd_AddCommand("hosp_list_down", HOS_ListDown_f, "Scroll down function for hospital employee list");
}

void HOS_ShutdownCallbacks(void)
{
	cgi->Cmd_RemoveCommand("hosp_empl_init");
	cgi->Cmd_RemoveCommand("hosp_init");
	cgi->Cmd_RemoveCommand("hosp_list_click");
	cgi->Cmd_RemoveCommand("hosp_list_up");
	cgi->Cmd_RemoveCommand("hosp_list_down");
}
