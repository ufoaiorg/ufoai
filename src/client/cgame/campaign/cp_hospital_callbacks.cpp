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
#include "../../cl_team.h" /* CL_ActorGetSkillString */
#include "../../ui/ui_dataids.h"
#include "cp_campaign.h"
#include "cp_hospital_callbacks.h"

static void HOS_EntryWoundData (const character_t& chr)
{
	const BodyData *bodyData = chr.teamDef->bodyTemplate;
	const woundInfo_t& wounds = chr.wounds;

	for (int bodyPart = 0; bodyPart < bodyData->numBodyParts(); ++bodyPart) {
		if (wounds.treatmentLevel[bodyPart] == 0)
			continue;
		const float severity = static_cast<float>(wounds.treatmentLevel[bodyPart]) / chr.maxHP;
		char text[MAX_VAR];
		Com_sprintf(text, lengthof(text), CHRSH_IsTeamDefRobot(chr.teamDef) ? _("Damaged %s (damage: %i)") :
				_("Wounded %s (damage: %i)"), _(bodyData->name(bodyPart)), wounds.treatmentLevel[bodyPart]);
		cgi->UI_ExecuteConfunc("hospital_wounds %i %s %f \"%s\"", chr.ucn, bodyData->id(bodyPart), severity, text);
	}
}

static float HOS_InjuryLevel (const character_t *const chr)
{
	float injuryLevel = 0.0f;

	for (int i = 0; i < chr->teamDef->bodyTemplate->numBodyParts(); ++i)
		injuryLevel += static_cast<float>(chr->wounds.treatmentLevel[i]) / chr->maxHP;

	return injuryLevel;
}

static const char* HOS_GetRank (const Employee &employee)
{
	/* Print rank for soldiers or type for other employees. */
	if (employee.isSoldier()) {
		const rank_t *rankPtr = CL_GetRankByIdx(employee.chr.score.rank);
		return _(rankPtr->name);
	}
	return E_GetEmployeeString(employee.getType(), 1);
}

static const char *HOS_GetInjuryLevel (const Employee &employee, float injuryLevel)
{
	/* If the employee is seriously wounded (HP <= 50% maxHP), make him red. */
	if (employee.chr.HP <= (int) (employee.chr.maxHP * 0.5) || injuryLevel >= 0.5)
		return "serious";

	/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */
	else if (employee.chr.HP <= (int) (employee.chr.maxHP * 0.85) || injuryLevel >= 0.15)
		return "medium";

	return "light";
}

static inline void HOS_Entry (const Employee& employee, float injuryLevel)
{
	const char *rank = HOS_GetRank(employee);
	const char *level = HOS_GetInjuryLevel(employee, injuryLevel);
	const character_t& chr = employee.chr;
	cgi->UI_ExecuteConfunc("hospitaladd %i \"%s\" %i %i \"%s\" \"%s\"", chr.ucn, level, chr.HP, chr.maxHP, chr.name, rank);
	HOS_EntryWoundData(chr);
}

/**
 * @brief Script command to init the hospital menu.
 * @sa HOS_EmployeeInit_f
 */
static void HOS_Init_f (void)
{
	const base_t *base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	if (!B_GetBuildingStatus(base, B_HOSPITAL)) {
		cgi->UI_PopWindow(false);
		return;
	}

	for (int type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			/* Don't show soldiers who are not in this base or gone in mission */
			if (!employee->isHiredInBase(base) || employee->isAwayFromBase())
				continue;
			/* Don't show healthy employees */
			const float injuryLevel = HOS_InjuryLevel(&employee->chr);
			if (employee->chr.HP >= employee->chr.maxHP && injuryLevel <= 0)
				continue;

			HOS_Entry(*employee, injuryLevel);
		}
	}
}

/**
 * @brief This is the init function for the hospital_employee menu
 */
static void HOS_EmployeeInit_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int ucn = atoi(cgi->Cmd_Argv(1));
	const Employee *e = E_GetEmployeeFromChrUCN(ucn);
	if (e == nullptr)
		return;

	cgi->UI_ResetData(TEXT_STANDARD);

	const character_t& c = e->chr;
	CL_UpdateCharacterValues(&c);

	cgi->Cvar_SetValue("mn_hp", c.HP);
	cgi->Cvar_SetValue("mn_hpmax", c.maxHP);
}


void HOS_InitCallbacks(void)
{
	cgi->Cmd_AddCommand("hosp_empl_init", HOS_EmployeeInit_f, "Init function for hospital employee menu");
	cgi->Cmd_AddCommand("hosp_init", HOS_Init_f, "Init function for hospital menu");
}

void HOS_ShutdownCallbacks(void)
{
	cgi->Cmd_RemoveCommand("hosp_empl_init");
	cgi->Cmd_RemoveCommand("hosp_init");
}
