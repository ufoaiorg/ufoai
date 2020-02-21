/**
 * @file
 * @brief Character (soldier, alien) related campaign functions
 */

/*
Copyright (C) 2002-2020 UFO: Alien Invasion.

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
#include "cp_character.h"
#include "cp_campaign.h"

typedef struct {
	int ucn;
	int HP;
	int STUN;
	int morale;
	woundInfo_t wounds;

	chrScoreGlobal_t chrscore;
} updateCharacter_t;

/**
 * @brief Determines the maximum amount of XP per skill that can be gained from any one mission.
 * @param[in] skill The skill for which to fetch the maximum amount of XP.
 * @sa G_UpdateCharacterExperience
 * @sa G_GetEarnedExperience
 * @note Explanation of the values here:
 * There is a maximum speed at which skills may rise over the course of the predicted career length of a veteran soldier.
 * Because the increase is given as experience^0.6, that means that the maximum XP cap x per mission is given as
 * log predictedStatGrowth / log x = 0.6
 * log x = log predictedStatGrowth / 0.6
 * x = 10 ^ (log predictedStatGrowth / 0.6)
 */
int CHAR_GetMaxExperiencePerMission (const abilityskills_t skill)
{
	switch (skill) {
	case ABILITY_POWER:
		return 125;
	case ABILITY_SPEED:
		return 91;
	case ABILITY_ACCURACY:
		return 450;
	case ABILITY_MIND:
		return 450;
	case SKILL_CLOSE:
		return 680;
	case SKILL_HEAVY:
		return 680;
	case SKILL_ASSAULT:
		return 680;
	case SKILL_SNIPER:
		return 680;
	case SKILL_EXPLOSIVE:
		return 680;
	case SKILL_NUM_TYPES: /* This is health. */
		return 360;
	case SKILL_PILOTING:
	case SKILL_TARGETING:
	case SKILL_EVADING:
		return 0;
	default:
		cgi->Com_Error(ERR_DROP, "G_GetMaxExperiencePerMission: invalid skill type");
		return -1;
	}
}

/**
 * @brief Updates the character skills after a mission.
 * @param[in,out] chr Pointer to the character that should get the skills updated.
 */
void CHAR_UpdateSkills (character_t* chr)
{
	for (int i = 0; i < SKILL_NUM_TYPES; ++i)
		chr->score.skills[i] = std::min(MAX_SKILL, chr->score.initialSkills[i] +
			static_cast<int>(pow(static_cast<float>(chr->score.experience[i]) / 10, 0.6f)));

	chr->maxHP = std::min(MAX_MAXHP, chr->score.initialSkills[SKILL_NUM_TYPES] +
		static_cast<int>(pow(static_cast<float>(chr->score.experience[SKILL_NUM_TYPES]) / 10, 0.6f)));
}

/**
 * @brief Transforms the battlescape values to the character
 * @sa CP_ParseCharacterData
 */
void CHAR_UpdateData (linkedList_t* updateCharacters)
{
	LIST_Foreach(updateCharacters, updateCharacter_t, c) {
		Employee* employee = E_GetEmployeeFromChrUCN(c->ucn);

		if (!employee) {
			cgi->Com_Printf("Warning: Could not get character with ucn: %i.\n", c->ucn);
			continue;
		}

		character_t* chr = &employee->chr;
		const bool fullHP = c->HP >= chr->maxHP;
		chr->STUN = c->STUN;
		chr->morale = c->morale;

		memcpy(chr->wounds.treatmentLevel, c->wounds.treatmentLevel, sizeof(chr->wounds.treatmentLevel));
		memcpy(chr->score.kills, c->chrscore.kills, sizeof(chr->score.kills));
		memcpy(chr->score.stuns, c->chrscore.stuns, sizeof(chr->score.stuns));
		chr->score.assignedMissions = c->chrscore.assignedMissions;

		for (int i = ABILITY_POWER; i <= SKILL_NUM_TYPES; ++i) {
			const int maxXP = CHAR_GetMaxExperiencePerMission(static_cast<abilityskills_t>(i));
			const int gainedXP = std::min(maxXP, c->chrscore.experience[i] - chr->score.experience[i]);
			chr->score.experience[i] += gainedXP;
			cgi->Com_DPrintf(DEBUG_CLIENT, "CP_UpdateCharacterData: Soldier %s earned %d experience points in skill #%d (total experience: %d)\n",
				chr->name, gainedXP, i, chr->score.experience[SKILL_NUM_TYPES]);
		}

		CHAR_UpdateSkills(chr);
		/* If character returned unscratched and maxHP just went up due to experience
		 * don't send him/her to the hospital */
		chr->HP = (fullHP ? chr->maxHP : std::min(c->HP, chr->maxHP));
	}
}

/**
 * @brief Parses the character data which was send by G_MatchSendResults using G_SendCharacterData
 * @param[in] msg The network buffer message. If this is nullptr the character is updated, if this
 * is not nullptr the data is stored in a temp buffer because the player can choose to retry
 * the mission and we have to catch this situation to not update the character data in this case.
 * @param updateCharacters A LinkedList where to store the character data. One listitem per character.
 * @sa G_SendCharacterData
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @sa E_Save
 */
void CHAR_ParseData (dbuffer* msg, linkedList_t** updateCharacters)
{
	const int num = cgi->NET_ReadByte(msg);

	if (num < 0)
		cgi->Com_Error(ERR_DROP, "CP_ParseCharacterData: invalid character number found in stream (%i)\n", num);

	for (int i = 0; i < num; i++) {
		updateCharacter_t c;
		OBJZERO(c);
		c.ucn = cgi->NET_ReadShort(msg);
		c.HP = cgi->NET_ReadShort(msg);
		c.STUN = cgi->NET_ReadByte(msg);
		c.morale = cgi->NET_ReadByte(msg);

		int j;
		for (j = 0; j < BODYPART_MAXTYPE; ++j)
			c.wounds.treatmentLevel[j] = cgi->NET_ReadByte(msg);

		for (j = 0; j < SKILL_NUM_TYPES + 1; j++)
			c.chrscore.experience[j] = cgi->NET_ReadLong(msg);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			c.chrscore.kills[j] = cgi->NET_ReadShort(msg);
		for (j = 0; j < KILLED_NUM_TYPES; j++)
			c.chrscore.stuns[j] = cgi->NET_ReadShort(msg);
		c.chrscore.assignedMissions = cgi->NET_ReadShort(msg);
		LIST_Add(updateCharacters, c);
	}
}

/**
 * @brief Checks whether a soldier should be promoted
 * @param[in] rank The rank to check for
 * @param[in] chr The character to check a potential promotion for
 * @todo (Zenerka 20080301) extend ranks and change calculations here.
 */
static bool CHAR_ShouldUpdateSoldierRank (const rank_t* rank, const character_t* chr)
{
	if (rank->type != EMPL_SOLDIER)
		return false;

	/* mind is not yet enough */
	if (chr->score.skills[ABILITY_MIND] < rank->mind)
		return false;

	/* not enough killed enemies yet */
	if (chr->score.kills[KILLED_ENEMIES] < rank->killedEnemies)
		return false;

	/* too many civilians and team kills */
	if (chr->score.kills[KILLED_CIVILIANS] + chr->score.kills[KILLED_TEAM] > rank->killedOthers)
		return false;

	return true;
}

/**
 * @brief Update employees stats after mission.
 * @param[in] base The base where the team lives.
 * @param[in] aircraft The aircraft used for the mission.
 * @note Soldier promotion is being done here.
 */
void CHAR_UpdateStats (const base_t* base, const aircraft_t* aircraft)
{
	assert(aircraft);

	/* only soldiers have stats and ranks, ugvs not */
	E_Foreach(EMPL_SOLDIER, employee) {
		if (!employee->isHiredInBase(aircraft->homebase))
			continue;
		if (!AIR_IsEmployeeInAircraft(employee, aircraft))
			continue;
		character_t* chr = &employee->chr;

		/* Remember the number of assigned mission for this character. */
		chr->score.assignedMissions++;

		/** @todo use chrScore_t to determine negative influence on soldier here,
		 * like killing too many civilians and teammates can lead to unhire and disband
		 * such soldier, or maybe rank degradation. */

		/* Check if the soldier meets the requirements for a higher rank
		 * and do a promotion. */
		if (ccs.numRanks < 2)
			continue;

		for (int j = ccs.numRanks - 1; j > chr->score.rank; j--) {
			const rank_t* rank = CL_GetRankByIdx(j);
			if (!CHAR_ShouldUpdateSoldierRank(rank, chr))
				continue;

			chr->score.rank = j;
			if (chr->HP > 0)
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s has been promoted to %s.\n"), chr->name, _(rank->name));
			else
				Com_sprintf(cp_messageBuffer, sizeof(cp_messageBuffer), _("%s has been awarded the posthumous rank of %s\nfor inspirational gallantry in the face of overwhelming odds.\n"), chr->name, _(rank->name));
			MS_AddNewMessage(_("Soldier promoted"), cp_messageBuffer, MSG_PROMOTION);
			break;
		}
	}
	cgi->Com_DPrintf(DEBUG_CLIENT, "CHAR_UpdateStats: Done\n");
}


#ifdef DEBUG
/**
 * @brief Debug function to increase the kills and test the ranks
 */
static void CHAR_DebugChangeStats_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <baseIDX>\n", cgi->Cmd_Argv(0));
		return;
	}
	base_t* base = B_GetBaseByIDX(atoi(cgi->Cmd_Argv(1)));
	if (!base) {
		cgi->Com_Printf("Invalid base idx\n");
		return;
	}

	E_Foreach(EMPL_SOLDIER, employee) {
		if (!employee->isHiredInBase(base))
			continue;

		character_t* chr = &(employee->chr);
		assert(chr);

		for (int j = 0; j < KILLED_NUM_TYPES; j++)
			chr->score.kills[j]++;
	}
	if (base->aircraftCurrent)
		CHAR_UpdateStats(base, base->aircraftCurrent);
}
#endif /* DEBUG */

/**
 * @brief Campaign initialization actions for the character module
 */
void CHAR_InitStartup (void)
{
#ifdef DEBUG
	cgi->Cmd_AddCommand("debug_statsupdate", CHAR_DebugChangeStats_f, "Debug function to increase the kills and test the ranks");
#endif
}

/**
 * @brief Campaign closing actions for the character module
 */
void CHAR_Shutdown (void)
{
#ifdef DEBUG
	cgi->Cmd_RemoveCommand("debug_statsupdate");
#endif
}
