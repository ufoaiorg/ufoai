/**
 * @file
 * @brief Most of the hospital related stuff
 * @note Hospital functions prefix: HOS_
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
#include "cp_campaign.h"
#include "cp_hospital.h"

#define GET_HP_HEALING( ab ) (1 + (ab) * 15/MAX_SKILL)

static void HOS_HealWounds (character_t* chr, int healing)
{
	woundInfo_t& wounds = chr->wounds;

	for (int bodyPart = 0; bodyPart < chr->teamDef->bodyTemplate->numBodyParts(); ++bodyPart) {
		if (wounds.treatmentLevel[bodyPart] <= 0)
			continue;
		wounds.treatmentLevel[bodyPart] = std::max(0, wounds.treatmentLevel[bodyPart] - healing);
	}
}

/**
 * @brief Heals character.
 * @param[in] chr Character data of an employee
 * @param[in] hospital True if we call this function for hospital healing (false when autoheal).
 * @sa HOS_HealEmployee
 * @return true if soldiers becomes healed - false otherwise
 */
bool HOS_HealCharacter (character_t* chr, bool hospital)
{
	assert(chr);
	float healing = ccs.curCampaign->healingRate;

	if (hospital) {
		healing *= GET_HP_HEALING(chr->score.skills[ABILITY_POWER]);
		HOS_HealWounds(chr, healing);
	}

	if (chr->HP < chr->maxHP) {
		/* if the character has less that 100 hitpoints, he will be disadvantaged by using the percentage
		 * method of allocating hitpoints.  So just give the character "healing" as Hitpoints, otherwise
		 * allocate "healing" as a percentage of the characters total hitpoints. */
		if (chr->maxHP < INITIAL_HP)
			chr->HP = std::min(chr->HP + static_cast<int>(healing), chr->maxHP);
		else
			chr->HP = std::min(chr->HP + static_cast<int>(((healing / 100.0f) * chr->maxHP)), chr->maxHP);

		if (chr->HP == chr->maxHP)
			return false;
		return true;
	}
	return false;
}

/**
 * @brief Checks health status of all employees in all bases.
 * @sa CP_CampaignRun
 * @note Called every day.
 */
void HOS_HospitalRun (void)
{
	for (int i = 0; i < MAX_EMPL; i++) {
		const employeeType_t type = (employeeType_t)i;
		E_Foreach(type, employee) {
			if (!employee->isHired())
				continue;
			const bool hospital = B_GetBuildingStatus(employee->baseHired, B_HOSPITAL);
			HOS_HealCharacter(&(employee->chr), hospital);
			CHRSH_UpdateImplants(employee->chr);
		}
	}
}

float HOS_GetInjuryLevel (const character_t& chr)
{
	float injuryLevel = 0.0f;

	for (int i = 0; i < chr.teamDef->bodyTemplate->numBodyParts(); ++i)
		injuryLevel += static_cast<float>(chr.wounds.treatmentLevel[i]) / chr.maxHP;

	return injuryLevel;
}

bool HOS_NeedsHealing (const character_t& chr)
{
	return HOS_GetInjuryLevel(chr) > 0.0001f || chr.HP < chr.maxHP;
}

#ifdef DEBUG
/**
 * @brief Set the character HP field to maxHP.
 */
static void HOS_HealAll_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	for (int type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			if (!employee->isHiredInBase(base))
				continue;
			employee->chr.HP = employee->chr.maxHP;
		}
	}
}

/**
 * @brief Decrement the character HP field by one.
 */
static void HOS_HurtAll_f (void)
{
	const base_t* base = B_GetCurrentSelectedBase();

	if (!base)
		return;

	int amount;
	if (cgi->Cmd_Argc() >= 2)
		amount = atoi(cgi->Cmd_Argv(1));
	else
		amount = 1;

	for (int type = 0; type < MAX_EMPL; type++) {
		E_Foreach(type, employee) {
			/* only those employees, that are in the current base */
			if (!employee->isHiredInBase(base))
				continue;
			employee->chr.HP = std::max(0, employee->chr.HP - amount);
		}
	}
}
#endif

/**
 * @brief Initial stuff for hospitals
 * Bind some of the functions in this file to console-commands that you can call ingame.
 * Called from UI_InitStartup resp. CL_InitLocal
 */
void HOS_InitStartup (void)
{
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_hosp_hurt_all", HOS_HurtAll_f, "Debug function to hurt all employees in the current base by one");
	cgi->Cmd_AddCommand("debug_hosp_heal_all", HOS_HealAll_f, "Debug function to heal all employees in the current base completely");
#endif
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_LoadXML
 * @sa SAV_GameSaveXML
 */
bool HOS_SaveXML (xmlNode_t* p)
{
	/* nothing to save here */
	return true;
}

/**
 * @brief Saving function for hospital related data
 * @sa HOS_SaveXML
 * @sa SAV_GameLoadXML
 */
bool HOS_LoadXML (xmlNode_t* p)
{
	return true;
}

/**
 * @brief Returns true if you can enter in the hospital
 * @sa B_BaseInit_f
 */
bool HOS_HospitalAllowed (const base_t* base)
{
	return !B_IsUnderAttack(base) && B_GetBuildingStatus(base, B_HOSPITAL);
}
