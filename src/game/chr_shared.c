/**
 * @file chr_shared.c
 * @note Shared character generating functions prefix: CHRSH_
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

/**
 * @brief Check if a team definition is a robot.
 * @param[in] td Pointer to the team definition to check.
 */
qboolean CHRSH_IsTeamDefRobot (const teamDef_t* const td)
{
	return td->race == RACE_ROBOT || td->race == RACE_BLOODSPIDER;
}

/**
 * @brief Templates for the different unit types. Each element of the array is a tuple that
 * indicates the minimum and the maximum value for the relevant ability or skill.
 */
static const int commonSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {15, 25}, /* Close */
	 {15, 25}, /* Heavy */
	 {15, 25}, /* Assault */
	 {15, 25}, /* Sniper */
	 {15, 25}, /* Explosives */
	 {80, 110}}; /* Health */

static const int closeSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {25, 40}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int heavySoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {25, 40}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int assaultSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {25, 40}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int sniperSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {25, 40}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int blastSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {25, 40}, /* Explosives */
	 {80, 110}}; /* Health */

static const int eliteSoldier[][2] =
	{{25, 35}, /* Strength */
	 {25, 35}, /* Speed */
	 {30, 40}, /* Accuracy */
	 {30, 45}, /* Mind */
	 {25, 40}, /* Close */
	 {25, 40}, /* Heavy */
	 {25, 40}, /* Assault */
	 {25, 40}, /* Sniper */
	 {25, 40}, /* Explosives */
	 {100, 130}}; /* Health */

static const int civilSoldier[][2] =
	{{5, 10}, /* Strength */
	 {5, 10}, /* Speed */
	 {10, 15}, /* Accuracy */
	 {10, 15}, /* Mind */
	 {5, 10}, /* Close */
	 {5, 10}, /* Heavy */
	 {5, 10}, /* Assault */
	 {5, 10}, /* Sniper */
	 {5, 10}, /* Explosives */
	 {5, 10}}; /* Health */

static const int tamanSoldier[][2] =
	{{25, 35}, /* Strength */
	 {25, 35}, /* Speed */
	 {40, 50}, /* Accuracy */
	 {50, 85}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {100, 130}}; /* Health */

static const int ortnokSoldier[][2] =
	{{45, 65}, /* Strength */
	 {20, 30}, /* Speed */
	 {30, 45}, /* Accuracy */
	 {20, 40}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {150, 190}}; /* Health */

static const int shevaarSoldier[][2] =
	{{30, 40}, /* Strength */
	 {30, 40}, /* Speed */
	 {40, 70}, /* Accuracy */
	 {30, 60}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {120, 160}}; /* Health */

static const int bloodSoldier[][2] =
	{{55, 55}, /* Strength */
	 {50, 50}, /* Speed */
	 {50, 50}, /* Accuracy */
	 {0, 0}, /* Mind */
	 {50, 50}, /* Close */
	 {50, 50}, /* Heavy */
	 {50, 50}, /* Assault */
	 {50, 50}, /* Sniper */
	 {50, 50}, /* Explosives */
	 {150, 150}}; /* Health */

static const int robotSoldier[][2] =
	{{55, 55}, /* Strength */
	 {40, 40}, /* Speed */
	 {50, 50}, /* Accuracy */
	 {0, 0}, /* Mind */
	 {50, 50}, /* Close */
	 {50, 50}, /* Heavy */
	 {50, 50}, /* Assault */
	 {50, 50}, /* Sniper */
	 {50, 50}, /* Explosives */
	 {200, 200}}; /* Health */

/** @brief For multiplayer characters ONLY! */
static const int MPSoldier[][2] =
	{{25, 75}, /* Strength */
	 {25, 35}, /* Speed */
	 {20, 75}, /* Accuracy */
	 {30, 75}, /* Mind */
	 {20, 75}, /* Close */
	 {20, 75}, /* Heavy */
	 {20, 75}, /* Assault */
	 {20, 75}, /* Sniper */
	 {20, 75}, /* Explosives */
	 {80, 130}}; /* Health */

/**
 * @brief Generates a skill and ability set for any character.
 * @param[in] chr Pointer to the character, for which we generate stats.
 * @param[in] multiplayer If this is true we use the skill values from @c MPSoldier
 * mulitplayer is a special case here
 */
void CHRSH_CharGenAbilitySkills (character_t * chr, qboolean multiplayer)
{
	int i;
	const int (*chrTemplate)[2];

	/* Add modifiers for difficulty setting here! */
	switch (chr->teamDef->race) {
	case RACE_TAMAN:
		chrTemplate = tamanSoldier;
		break;
	case RACE_ORTNOK:
		chrTemplate = ortnokSoldier;
		break;
	case RACE_BLOODSPIDER:
		chrTemplate = bloodSoldier;
		break;
	case RACE_SHEVAAR:
		chrTemplate = shevaarSoldier;
		break;
	case RACE_CIVILIAN:
		chrTemplate = civilSoldier;
		break;
	case RACE_PHALANX_HUMAN: {
		if (multiplayer) {
			chrTemplate = MPSoldier;
		} else {
			/* Determine which soldier template to use.
			 * 25% of the soldiers will be specialists (5% chance each).
			 * 1% of the soldiers will be elite.
			 * 74% of the soldiers will be common. */
			const float soldierRoll = frand();
			if (soldierRoll <= 0.01f)
				chrTemplate = eliteSoldier;
			else if (soldierRoll <= 0.06)
				chrTemplate = closeSoldier;
			else if (soldierRoll <= 0.11)
				chrTemplate = heavySoldier;
			else if (soldierRoll <= 0.16)
				chrTemplate = assaultSoldier;
			else if (soldierRoll <= 0.22)
				chrTemplate = sniperSoldier;
			else if (soldierRoll <= 0.26)
				chrTemplate = blastSoldier;
			else
				chrTemplate = commonSoldier;
		}
		break;
	}
	case RACE_ROBOT:
		chrTemplate = robotSoldier;
		break;
	default:
		Sys_Error("CHRSH_CharGenAbilitySkills: unexpected race '%i'", chr->teamDef->race);
	}

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
	if (INVSH_HasArmour(&chr->i) && chr->teamDef->race != RACE_ROBOT) {
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
