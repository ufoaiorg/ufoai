/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "cp_hospital.h"
#include "cp_team.h"

static void HOS_EntryWoundData (const character_t& chr)
{
	const BodyData* bodyData = chr.teamDef->bodyTemplate;
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

static const char* HOS_GetRank (const Employee& employee)
{
	/* Print rank for soldiers or type for other employees. */
	if (employee.isSoldier()) {
		const rank_t* rankPtr = CL_GetRankByIdx(employee.chr.score.rank);
		return _(rankPtr->name);
	}
	return E_GetEmployeeString(employee.getType(), 1);
}

static const char* HOS_GetInjuryLevelString (const Employee& employee, float injuryLevel)
{
	/* If the employee is seriously wounded (HP <= 50% maxHP), make him red. */
	if (employee.chr.HP <= (int) (employee.chr.maxHP * 0.5) || injuryLevel > 0.5f)
		return "serious";

	/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */
	else if (employee.chr.HP <= (int) (employee.chr.maxHP * 0.85) || injuryLevel > 0.15f)
		return "medium";

	/* no wounds and full hp */
	else if (employee.chr.HP >= employee.chr.maxHP && injuryLevel < 0.0001f)
		return "healthy";

	return "light";
}

static inline void HOS_Entry (const Employee& employee)
{
	const character_t& chr = employee.chr;
	const float injuryLevel = HOS_GetInjuryLevel(chr);
	if (!employee.isSoldier() && chr.HP >= chr.maxHP && injuryLevel <= 0.0f)
		return;

	const char* rank = HOS_GetRank(employee);
	const char* level = HOS_GetInjuryLevelString(employee, injuryLevel);
	const aircraft_t* craft = AIR_IsEmployeeInAircraft(&employee, nullptr);

	cgi->UI_ExecuteConfunc("hospitaladd %i \"%s\" %i %i \"%s\" \"%s\" \"%s\"",
			chr.ucn, level, chr.HP, chr.maxHP, chr.name, rank, craft ? craft->name : "");
	HOS_EntryWoundData(chr);
}

static void HOS_ImplantDetails_f (void)
{
	const int odIdx = atoi(cgi->Cmd_Argv(1));
	const objDef_t* od = INVSH_GetItemByIDX(odIdx);
	if (od == nullptr)
		return;
	const implantDef_t* def = INVSH_GetImplantForObjDef(od);
	if (def == nullptr)
		return;
	Com_Printf("implant: %s\n", def->id);
}

/**
 * @brief Script command to initialize the hospital menu.
 * @sa HOS_EmployeeInit_f
 */
static void HOS_Init_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();
	if (!base)
		return;

	if (!B_GetBuildingStatus(base, B_HOSPITAL)) {
		cgi->UI_PopWindow(false);
		return;
	}

	bool containerSet = false;
	for (int type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			/* Don't show soldiers who are not in this base or gone in mission */
			if (!employee->isHiredInBase(base) || employee->isAwayFromBase())
				continue;
			HOS_Entry(*employee);
			if (containerSet)
				continue;
			CP_SetEquipContainer(&employee->chr);
			containerSet = true;
		}
	}
}

static const cmdList_t hospitalCmds[] = {
	{"hosp_init", HOS_Init_f, "Init function for hospital menu"},
	{"hosp_implant_details", HOS_ImplantDetails_f, "Print details for an implant"},
	{nullptr, nullptr, nullptr}
};

void HOS_InitCallbacks (void)
{
	cgi->Cmd_TableAddList(hospitalCmds);
}

void HOS_ShutdownCallbacks (void)
{
	cgi->Cmd_TableRemoveList(hospitalCmds);
}
