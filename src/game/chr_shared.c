/**
 * @file chr_shared.c
 * @note Shared character generating functions prefix: CHRSH_
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "q_shared.h"
#include "chr_shared.h"

/**
 * @brief Check if a team definition is alien.
 * @param[in] td Pointer to the team definition to check.
 */
qboolean CHRSH_IsTeamDefAlien (const teamDef_t* const td)
{
	return td->race == RACE_TAMAN || td->race == RACE_ORTNOK
		|| td->race == RACE_BLOODSPIDER || td->race == RACE_SHEVAAR;
}

qboolean CHRSH_IsArmourUseableForTeam (const objDef_t *od, const teamDef_t *teamDef)
{
	assert(teamDef);
	assert(INV_IsArmour(od));

	if (!teamDef->armour)
		return qfalse;

	if (CHRSH_IsTeamDefAlien(teamDef))
		return od->useable == TEAM_ALIEN;
	else if (teamDef->race == RACE_PHALANX_HUMAN)
		return od->useable == TEAM_PHALANX;
	else if (teamDef->race == RACE_CIVILIAN)
		return od->useable == TEAM_CIVILIAN;

	return qfalse;
}

/**
 * @brief Check if a team definition is a robot.
 * @param[in] td Pointer to the team definition to check.
 */
qboolean CHRSH_IsTeamDefRobot (const teamDef_t* const td)
{
	return td->race == RACE_ROBOT || td->race == RACE_BLOODSPIDER;
}

/**
 * @brief Generates a skill and ability set for any character.
 * @param[in] chr Pointer to the character, for which we generate stats.
 * @param[in] multiplayer If this is true we use the skill values from @c soldier_mp
 * mulitplayer is a special case here
 */
void CHRSH_CharGenAbilitySkills (character_t * chr, qboolean multiplayer)
{
	int i;
	const int (*chrTemplate)[2];
	const teamDef_t *td;
	const chrTemplate_t *ct;

	td = chr->teamDef;

	/* Add modifiers for difficulty setting here! */
	if (multiplayer && td->race == RACE_PHALANX_HUMAN) {
		for (i = 0; i < td->numTemplates; i++) {
			if (Q_streq(td->characterTemplates[i]->id, "soldier_mp")) {
				ct = td->characterTemplates[i];
				break;
			}
		}
		if (i >= td->numTemplates)
			Sys_Error("CHRSH_CharGenAbilitySkills: No multiplayer character template found (soldier_mp)");
	} else if (td->characterTemplates[0]) {
		if (td->numTemplates > 1) {
			float sumRate = 0.0;
			for (i = 0; i < td->numTemplates; i++) {
				ct = td->characterTemplates[i];
				sumRate += ct->rate;
			}
			if (sumRate) {
				const float soldierRoll = frand();
				float curRate = 0.0;
				for (ct = td->characterTemplates[0]; ct->id; ct++) {
					curRate += ct->rate;
					if (curRate && soldierRoll <= (curRate / sumRate))
						break;
				}
			} else
				/* No rates or all set to 0 default to first template */
				ct = td->characterTemplates[0];
		} else
			/* Only one template */
			ct = td->characterTemplates[0];
	} else
		Sys_Error("CHRSH_CharGenAbilitySkills: No character template for team %s!", td->id);
	chrTemplate = ct->skills;

	assert(chrTemplate);

	/* Abilities and skills -- random within the range */
	for (i = 0; i < SKILL_NUM_TYPES; i++) {
		const int abilityWindow = chrTemplate[i][1] - chrTemplate[i][0];
		/* Reminder: In case if abilityWindow==0 the ability will be set to the lower limit. */
		const int temp = (frand() * abilityWindow) + chrTemplate[i][0];
		chr->score.skills[i] = temp;
		chr->score.initialSkills[i] = temp;
	}

	{
		/* Health. */
		const int abilityWindow = chrTemplate[i][1] - chrTemplate[i][0];
		const int temp = (frand() * abilityWindow) + chrTemplate[i][0];
		chr->score.initialSkills[SKILL_NUM_TYPES] = temp;
		chr->maxHP = temp;
		chr->HP = temp;
	}

	/* Morale */
	chr->morale = GET_MORALE(chr->score.skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;

	/* Initialize the experience values */
	for (i = 0; i <= SKILL_NUM_TYPES; i++) {
		chr->score.experience[i] = 0;
	}
}

/**
 * @brief Returns the body model for the soldiers for armoured and non armoured soldiers
 * @param[in] chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 * @return the character body model (from a static buffer)
 */
const char *CHRSH_CharGetBody (const character_t * const chr)
{
	static char returnModel[MAX_VAR];

	/* models of UGVs don't change - because they are already armoured */
	if (INVSH_HasArmour(&chr->i) && !CHRSH_IsTeamDefRobot(chr->teamDef)) {
		const objDef_t *od = INVSH_HasArmour(&chr->i)->item.t;
		const char *id = od->armourPath;
		if (!INV_IsArmour(od))
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->body);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->body);
	return returnModel;
}

/**
 * @brief Returns the head model for the soldiers for armoured and non armoured soldiers
 * @param[in] chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 */
const char *CHRSH_CharGetHead (const character_t * const chr)
{
	static char returnModel[MAX_VAR];

	/* models of UGVs don't change - because they are already armoured */
	if (INVSH_HasArmour(&chr->i) && chr->teamDef->race != RACE_ROBOT) {
		const objDef_t *od = INVSH_HasArmour(&chr->i)->item.t;
		const char *id = od->armourPath;
		if (!INV_IsArmour(od))
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->head);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->head);
	return returnModel;
}
