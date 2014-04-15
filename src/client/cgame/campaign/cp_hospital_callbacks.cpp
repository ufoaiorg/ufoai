/**
 * @file
 */

/*
Copyright (C) 2002-2014 UFO: Alien Invasion.

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

static float HOS_GetInjuryLevel (const character_t* const chr)
{
	float injuryLevel = 0.0f;

	for (int i = 0; i < chr->teamDef->bodyTemplate->numBodyParts(); ++i)
		injuryLevel += static_cast<float>(chr->wounds.treatmentLevel[i]) / chr->maxHP;

	return injuryLevel;
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
	if (employee.chr.HP <= (int) (employee.chr.maxHP * 0.5) || injuryLevel >= 0.5)
		return "serious";

	/* If the employee is semi-seriously wounded (HP <= 85% maxHP), make him yellow. */
	else if (employee.chr.HP <= (int) (employee.chr.maxHP * 0.85) || injuryLevel >= 0.15)
		return "medium";

	/* no wounds and full hp */
	else if (employee.chr.HP >= employee.chr.maxHP && injuryLevel <= 0)
		return "healty";

	return "light";
}

static inline void HOS_Entry (const Employee& employee, float injuryLevel)
{
	const char* rank = HOS_GetRank(employee);
	const char* level = HOS_GetInjuryLevelString(employee, injuryLevel);
	const character_t& chr = employee.chr;

	if (!employee.isSoldier() && employee.chr.HP >= employee.chr.maxHP && injuryLevel <= 0)
		return;
	cgi->UI_ExecuteConfunc("hospitaladd %i \"%s\" %i %i \"%s\" \"%s\"", chr.ucn, level, chr.HP, chr.maxHP, chr.name, rank);
	HOS_EntryWoundData(chr);
}

/**
 * @brief Calls all the ui confuncs to show the implants of the given character
 * @param[in] c The character to show the implants for
 */
static void HOS_UpdateCharacterImplantList (const character_t& c)
{
	cgi->UI_ExecuteConfunc("hospital_employee_clear_implants");
	for (int i = 0; i < lengthof(c.implants); i++) {
		const implant_t& implant = c.implants[i];
		const implantDef_t* def = implant.def;
		if (def == nullptr)
			continue;
		const objDef_t& od = *def->item;
		cgi->UI_ExecuteConfunc("hospital_employee_add_implant %i \"%s\" \"%s\" %i %i %i %i",
				def->idx, _(od.name), od.image, def->installationTime,
				implant.installedTime, def->removeTime, implant.removedTime);
	}
}

static Item* HOS_GetImplant (const character_t& chr, const implantDef_t& def)
{
	Item* item = chr.inv.getContainer2(CID_IMPLANT);
	while (item) {
		if (item->def() == def.item) {
			return item;
		}
		item = item->getNext();
	}
	return nullptr;
}

static void HOS_ImplantChange_f (void)
{
	if (cgi->Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <ucn> <implantid>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int ucn = atoi(cgi->Cmd_Argv(1));
	Employee* e = E_GetEmployeeFromChrUCN(ucn);
	if (e == nullptr) {
		Com_Printf("No employee for ucn %i found\n", ucn);
		return;
	}

	character_t& chr = e->chr;
	const int odIdx = atoi(cgi->Cmd_Argv(2));
	const objDef_t* od = INVSH_GetItemByIDX(odIdx);
	if (od == nullptr)
		return;
	const implantDef_t* def = INVSH_GetImplantForObjDef(od);
	if (def == nullptr)
		return;
	const implant_t* implant = e->isSoldier() ? CHRSH_ApplyImplant(chr, *def) : nullptr;
	if (implant == nullptr) {
		Item* item = HOS_GetImplant(chr, *def);
		if (item != nullptr) {
			const Container& container = chr.inv.getContainer(CID_IMPLANT);
			cgi->INV_RemoveFromInventory(&chr.inv, container.def(), item);
			return;
		}
	}
	HOS_UpdateCharacterImplantList(chr);
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

	for (int type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			/* Don't show soldiers who are not in this base or gone in mission */
			if (!employee->isHiredInBase(base) || employee->isAwayFromBase())
				continue;
			const float injuryLevel = HOS_GetInjuryLevel(&employee->chr);
			HOS_Entry(*employee, injuryLevel);
		}
	}
}

/**
 * @brief This is the initialization function for the hospital_employee menu
 */
static void HOS_EmployeeInit_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <ucn>\n", cgi->Cmd_Argv(0));
		return;
	}

	const int ucn = atoi(cgi->Cmd_Argv(1));
	Employee* emp = E_GetEmployeeFromChrUCN(ucn);
	if (emp == nullptr)
		return;

	character_t& echr = emp->chr;
	const chrScoreGlobal_t& score = echr.score;
	const char* rank = "";
	const char* rankImage = "";

	if (score.rank >= 0) {
		const rank_t* r = CL_GetRankByIdx(score.rank);
		rank = va(_("Rank: %s"), _(r->name));
		rankImage = r->image;
	}

	base_t* base = emp->baseHired;
	CP_CleanTempInventory(base);
	equipDef_t unused = base->storage;
	CP_CleanupTeam(base, &unused);
	CP_SetEquipContainer(&echr);
	cgi->UI_ContainerNodeUpdateEquipment(&base->bEquipment, &unused);

	cgi->UI_ExecuteConfunc("hospital_employee_show %i \"%s\" \"%s\" %i \"%s\" %i %i %i \"%s\" \"%s\" %i",
			ucn, echr.name, CHRSH_CharGetBody(&echr), echr.bodySkin, CHRSH_CharGetHead(&echr),
			echr.headSkin, echr.HP, echr.maxHP, rank, rankImage, emp->isSoldier() ? 1 : 0);

	const abilityskills_t list[] = { ABILITY_POWER, ABILITY_SPEED, ABILITY_ACCURACY, ABILITY_MIND };
	const int n = lengthof(list);
	for (int i = 0; i < n; i++) {
		cgi->UI_ExecuteConfunc("hospital_employee_set_values %i %i \"%s\"",
				i, score.skills[i], cgi->CL_ActorGetSkillString(score.skills[i]));
	}

	HOS_UpdateCharacterImplantList(echr);
}

static const cmdList_t hospitalCmds[] = {
	{"hosp_empl_init", HOS_EmployeeInit_f, "Init function for hospital employee menu"},
	{"hosp_init", HOS_Init_f, "Init function for hospital menu"},
	{"hosp_implant_change", HOS_ImplantChange_f, "Assign or remove an implant to an employee"},
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
