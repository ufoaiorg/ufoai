/**
 * @file cp_auto_mission.c
 * @brief single player automatic (quick, simulated) missions, without going to the battlescape.
 */

/*
Copyright (C) 2002-2012 UFO: Alien Invasion.

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
#include "../../cl_inventory.h"
#include "cp_auto_mission.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "../../ui/ui_windows.h"
#include "../../../shared/mathlib_extra.h"
#include "math.h"
#include "cp_mission_callbacks.h"

/**
 * @brief Possible types of teams that can fight in an auto mission battle.
 */
typedef enum autoMission_teamType_s {
	AUTOMISSION_TEAM_TYPE_PLAYER,				/**< Human player-controlled team.  Includes soldiers as well as downed pilots. */
	AUTOMISSION_TEAM_TYPE_ALIEN,				/**< AI-controlled alien team. */
	AUTOMISSION_TEAM_TYPE_CIVILIAN,				/**< AI-controlled civilians that can be healthy or infected. */

	AUTOMISSION_TEAM_TYPE_MAX
} autoMissionTeamType_t;

#define MAX_SOLDIERS_AUTOMISSION MAX_TEAMS * AUTOMISSION_TEAM_TYPE_MAX

/**
 * @brief One unit (soldier/alien/civilian) of the autobattle
 * @todo add attack and defence scores
 */
typedef struct autoUnit_s {
	int idx;
	autoMissionTeamType_t team;					/**< Team of the unit */
	character_t *chr;							/**< Character */
	double attackStrength;						/**< How good at attacking a unit is, from 0.0 (can't fight at all) to 1.0 (Higher is better) */
	double defendStrength;						/**< A unit's armor or protection, from 0.0 (no armor, can't even dodge/stationary) to 1.0 (Higher is better) */
} autoUnit_t;

/**
 * @brief Data structure for a simulated or auto mission.
 */
typedef struct autoMissionBattle_s {
	qboolean isHostile[AUTOMISSION_TEAM_TYPE_MAX][AUTOMISSION_TEAM_TYPE_MAX];		/**< Friendly or hostile?  Params are [Each team] [Each other team] */
	short nUnits[AUTOMISSION_TEAM_TYPE_MAX];							/**< Number of units (soldiers, aliens, UGVs, whatever) on each team, hardcoded MAX of 64 per team */
	short actUnits[AUTOMISSION_TEAM_TYPE_MAX];							/**< Number of active units (soldiers, aliens, UGVs, whatever) on each team, hardcoded MAX of 64 per team */

	double scoreTeamEquipment[AUTOMISSION_TEAM_TYPE_MAX];			/**< Number from 0.f to 1.f, represents how good a team's equipment is (higher is better) */
	double scoreTeamSkill[AUTOMISSION_TEAM_TYPE_MAX];				/**< Number from 0.f to 1.f, represents how good a team's abilities are (higher is better) */
	double scoreTeamDifficulty[AUTOMISSION_TEAM_TYPE_MAX];		/**< Number from 0.f to 1.f, represents a team's global fighting ability, difficulty, or misc. adjustments (higher is better) */

	autoUnit_t units[AUTOMISSION_TEAM_TYPE_MAX][MAX_SOLDIERS_AUTOMISSION];		/**< Units data */

	int teamAccomplishment[AUTOMISSION_TEAM_TYPE_MAX];							/**< Used for calculating experience gain, and for friendly fire (including hit civilians) */

	int winningTeam;								/**< Which team is victorious */
	missionResults_t *results;						/**< Manual mission "compatible" result structure */
} autoMissionBattle_t;

/**
 * @brief Constants for automission experience gain factors
 * @todo make these scripted in campaign definitions maybe
*/
#define SKILL_AWARD_SCALE 1.0f
#define ABILITY_AWARD_SCALE 1.0f

#define AM_IsPlayer(type) ((type) == AUTOMISSION_TEAM_TYPE_PLAYER)
#define AM_IsAlien(type)  ((type) == AUTOMISSION_TEAM_TYPE_ALIEN)
#define AM_IsCivilian(type) ((type) == AUTOMISSION_TEAM_TYPE_CIVILIAN)
#define AM_SetHostile(battle, team, otherTeam, value) (battle)->isHostile[(team)][(otherTeam)] = (value)
#define AM_IsHostile(battle, team, otherTeam) ((battle)->isHostile[(team)][(otherTeam)])

#define AM_GetUnit(battle, teamIdx, unitIdx) (&battle->units[teamIdx][unitIdx])
#define AM_IsUnitActive(unit) (((unit)->chr->HP > 0) && ((unit)->chr->HP > (unit)->chr->STUN))

/**
 * @brief Clears, initializes, or resets a single auto mission, sets default values.
 * @param[in,out] battle The battle that should be initialized to defaults
 */
static void AM_ClearBattle (autoMissionBattle_t *battle)
{
	int team;

	assert(battle != NULL);

	OBJZERO(*battle);

	for (team = 0; team < AUTOMISSION_TEAM_TYPE_MAX; team++) {
		int otherTeam;

		battle->scoreTeamDifficulty[team] = 0.5;
		battle->scoreTeamEquipment[team] = 0.5;
		battle->scoreTeamSkill[team] = 0.5;

		for (otherTeam = 0; otherTeam < AUTOMISSION_TEAM_TYPE_MAX; otherTeam++) {
			/* If you forget to set this and run a battle, everyone will just kill each other by default */
			battle->isHostile[team][otherTeam] = qtrue;
		}
	}

	battle->winningTeam = -1;
	battle->results = NULL;
}

/**
 * @brief Adds team data for a specified team in an auto-mission object, from a (player) aircraft.
 * @param[in,out] battle The auto mission battle to add team data to
 * @param[in] teamNum The team number in the auto mission instance to update
 * @param[in] aircraft The aircraft to get data from
 * @param[in] campaign The current campaign (for retrieving difficulty level)
 * @note This function actually gets the data from the campaign object, using the aircraft data to
 * find out which of all the employees are on the aircraft (in the mission)
 */
static void AM_FillTeamFromAircraft (autoMissionBattle_t *battle, const autoMissionTeamType_t teamNum, const aircraft_t *aircraft, const campaign_t *campaign)
{
	int teamSize;
	int unitsAlive;

	assert(teamNum < AUTOMISSION_TEAM_TYPE_MAX);
	assert(battle != NULL);
	assert(aircraft != NULL);

	teamSize = 0;
	unitsAlive = 0;
	LIST_Foreach(aircraft->acTeam, employee_t, employee) {
		autoUnit_t *unit = AM_GetUnit(battle, teamNum, teamSize);

		unit->chr = &employee->chr;
		unit->team = teamNum;
		unit->idx = teamSize;

		teamSize++;
		if (employee->chr.HP > 0)
			unitsAlive++;

		if (teamSize >= MAX_SOLDIERS_AUTOMISSION)
			break;
	}
	battle->nUnits[teamNum] = teamSize;
	battle->actUnits[teamNum] = unitsAlive;

	if (teamSize == 0) {
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add soldiers to an auto-mission from an aircraft with no soldiers onboard.\n");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: Aliens might win this mission by default because they are un-challenged, with no resistance!\n");
	}
	if (unitsAlive == 0) {
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add team to auto battle where all the units on the team are DEAD!\n");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: This team will LOSE the battle by default.\n");
	}

	/* NOTE:  For now these are hard-coded to values based upon general campaign difficulty.
	 * --- In the future, it might be easier to set this according to a scripted value in a .ufo
	 * --- file, with other campaign info.  Reminder:  Higher floating point values mean better
	 * --- soldiers, and therefore make an easier fight for the player. */
	switch (campaign->difficulty) {
	case 4:
		battle->scoreTeamDifficulty[teamNum] = 0.30;
		break;
	case 3:
		battle->scoreTeamDifficulty[teamNum] = 0.35;
		break;
	case 2:
		battle->scoreTeamDifficulty[teamNum] = 0.40;
		break;
	case 1:
		battle->scoreTeamDifficulty[teamNum] = 0.45;
		break;
	case 0:
		battle->scoreTeamDifficulty[teamNum] = 0.50;
		break;
	case -1:
		battle->scoreTeamDifficulty[teamNum] = 0.55;
		break;
	case -2:
		battle->scoreTeamDifficulty[teamNum] = 0.60;
		break;
	case -3:
		battle->scoreTeamDifficulty[teamNum] = 0.65;
		break;
	case -4:
		battle->scoreTeamDifficulty[teamNum] = 0.70;
		break;
	default:
		battle->scoreTeamDifficulty[teamNum] = 0.50;
	}
}

/**
 * @brief Create character for a Unit
 * @param[out] unit The unit to create character for
 * @param[in] teamDef The team definition of the unit
 * @param[in] eq The equipment to use
 * @sa AM_DestroyUnitChr
 */
static void AM_CreateUnitChr (autoUnit_t *unit, const teamDef_t *teamDef, const equipDef_t *ed)
{
	unit->chr = (character_t*)Mem_PoolAlloc(sizeof(character_t), cp_campaignPool, 0);
	cgi->CL_GenerateCharacter(unit->chr, teamDef->id);

	if (ed) {
		/* Pack equipment. */
		if (teamDef->weapons)
			cgi->INV_EquipActor(&unit->chr->i, ed, teamDef);
		else if (teamDef->onlyWeapon)
			/* actor cannot handle weapons but a particular item */
			cgi->INV_EquipActorMelee(&unit->chr->i, teamDef);
	}
}

/**
 * @brief Destroys character of a Unit
 * @param[out] unit The unit to create character for
 * @param[in] teamDef The team definition of the unit
 * @param[in] eq The equipment to use in case the @c type is @c AUTOMISSION_TEAM_TYPE_ALIEN
 * @sa AM_CreateUnitChr
 */
static void AM_DestroyUnitChr (autoUnit_t *unit)
{
	cgi->INV_DestroyInventory(&unit->chr->i);
	Mem_Free(unit->chr);
}

/**
 * @brief Creates team data for alien and civilian teams based on the mission parameters data.
 * @param[in,out] battle The auto mission battle to add team data to
 * @param[in] missionParam Mission parameters data to use
 */
static void AM_FillTeamFromBattleParams (autoMissionBattle_t *battle, const battleParam_t *missionParams)
{
	assert(battle);
	assert(missionParams);

	/* Aliens */
	battle->nUnits[AUTOMISSION_TEAM_TYPE_ALIEN] = missionParams->aliens;
	battle->actUnits[AUTOMISSION_TEAM_TYPE_ALIEN] = missionParams->aliens;
	if (missionParams->aliens > 0) {
		const equipDef_t *ed = INV_GetEquipmentDefinitionByID(missionParams->alienEquipment);
		const alienTeamGroup_t *alienTeamGroup = missionParams->alienTeamGroup;
		int unitIDX;
		for (unitIDX = 0; unitIDX < missionParams->aliens; unitIDX++) {
			const teamDef_t *teamDef = alienTeamGroup->alienTeams[rand() % alienTeamGroup->numAlienTeams];
			autoUnit_t *unit = AM_GetUnit(battle, AUTOMISSION_TEAM_TYPE_ALIEN, unitIDX);

			AM_CreateUnitChr(unit, teamDef, ed);
			unit->team = AUTOMISSION_TEAM_TYPE_ALIEN;
			unit->idx = unitIDX;
		}
		battle->scoreTeamSkill[AUTOMISSION_TEAM_TYPE_ALIEN] = (frand() * 0.6f) + 0.2f;
	}

	/* Civilians (if any) */
	battle->nUnits[AUTOMISSION_TEAM_TYPE_CIVILIAN] = missionParams->civilians;
	battle->actUnits[AUTOMISSION_TEAM_TYPE_CIVILIAN] = missionParams->civilians;
	if (missionParams->civilians > 0) {
		const teamDef_t *teamDef = Com_GetTeamDefinitionByID(missionParams->civTeam);
		int unitIDX;
		for (unitIDX = 0; unitIDX < missionParams->civilians; unitIDX++) {
			autoUnit_t *unit = AM_GetUnit(battle, AUTOMISSION_TEAM_TYPE_CIVILIAN, unitIDX);

			AM_CreateUnitChr(unit, teamDef, NULL);
			unit->team = AUTOMISSION_TEAM_TYPE_CIVILIAN;
			unit->idx = unitIDX;
		}
		battle->scoreTeamSkill[AUTOMISSION_TEAM_TYPE_CIVILIAN] = (frand() * 0.5f) + 0.05f;
	}
}

/**
 * @brief Run this on an auto mission battle before the battle is actually simulated, to set
 * default values for who will attack which team. If you forget to do this before battle simulation, all teams will default
 * to "free for all" (Everyone will try to kill everyone else).
 * @param[in, out] battle The battle to set up team hostility values for.
 * @param[in] civsInfected Set to @c true if civs have XVI influence, otherwise @c false for a normal mission.
 */
static void AM_SetDefaultHostilities (autoMissionBattle_t *battle, const qboolean civsInfected)
{
	int i;
	qboolean civsInverted = !civsInfected;

	for (i = AUTOMISSION_TEAM_TYPE_PLAYER; i < AUTOMISSION_TEAM_TYPE_MAX; i++) {
		int j;
		const autoMissionTeamType_t team = (autoMissionTeamType_t)i;
		if (battle->actUnits[team] <= 0)
			continue;

		for (j = AUTOMISSION_TEAM_TYPE_PLAYER; j < AUTOMISSION_TEAM_TYPE_MAX; j++) {
			const autoMissionTeamType_t otherTeam = (autoMissionTeamType_t)j;
			if (battle->actUnits[otherTeam] <= 0)
				continue;

			if (AM_IsPlayer(team)) {
				if (AM_IsAlien(otherTeam))
					AM_SetHostile(battle, team, otherTeam, qtrue);
				else if (AM_IsPlayer(otherTeam))
					AM_SetHostile(battle, team, otherTeam, qfalse);
				else if (AM_IsCivilian(otherTeam))
					AM_SetHostile(battle, team, otherTeam, civsInfected);
			} else if (AM_IsAlien(team)) {
				if (AM_IsAlien(otherTeam))
					AM_SetHostile(battle, team, otherTeam, qfalse);
				else if (AM_IsPlayer(otherTeam))
					AM_SetHostile(battle, team, otherTeam, qtrue);
				else if (AM_IsCivilian(otherTeam))
					AM_SetHostile(battle, team, otherTeam, civsInverted);
			} else if (AM_IsCivilian(team)) {
				if (AM_IsAlien(otherTeam))
					AM_SetHostile(battle, team, otherTeam, civsInverted);
				else if (AM_IsPlayer(otherTeam))
					AM_SetHostile(battle, team, otherTeam, civsInfected);
				else if (AM_IsCivilian(otherTeam))
					AM_SetHostile(battle, team, otherTeam, qfalse);
			}
		}
	}
}

/**
 * @brief Calcuates Team strength scores for autobattle
 * @param[in, out] battle The battle to set up teamscores for
 */
static void AM_CalculateTeamScores (autoMissionBattle_t *battle)
{
	int unitTotal = 0;
	int isHostileTotal = 0;
	int totalActiveTeams = 0;
	int lastActiveTeam = -1;
	int isHostileCount;
	int team;
	/* Sums of various values */
	double teamPooledHealth[AUTOMISSION_TEAM_TYPE_MAX];
	double teamPooledHealthMax[AUTOMISSION_TEAM_TYPE_MAX];
	double teamPooledUnitsHealthy[AUTOMISSION_TEAM_TYPE_MAX];
	double teamPooledUnitsTotal[AUTOMISSION_TEAM_TYPE_MAX];
	/* Ratios */
	double teamRatioHealthyUnits[AUTOMISSION_TEAM_TYPE_MAX];
	double teamRatioHealthTotal[AUTOMISSION_TEAM_TYPE_MAX];
	int currentUnit;

	for (team = 0; team < AUTOMISSION_TEAM_TYPE_MAX; team++) {
		unitTotal += battle->nUnits[team];

		if (battle->actUnits[team] > 0) {
			lastActiveTeam = team;
			totalActiveTeams++;
		}
		for (isHostileCount = 0; isHostileCount < AUTOMISSION_TEAM_TYPE_MAX; isHostileCount++) {
			if (battle->nUnits[isHostileCount] <= 0)
				continue;

			if (battle->isHostile[team][isHostileCount] && battle->actUnits[team] > 0)
				isHostileTotal++;
		}
	}

	/* sanity checks */
	if (unitTotal == 0)
		Com_Error(ERR_DROP, "Grand total of ZERO units are fighting in auto battle, something is wrong.");

	if (unitTotal < 0)
		Com_Error(ERR_DROP, "Negative number of total units are fighting in auto battle, something is VERY wrong!");

	if (isHostileTotal <= 0)
		Com_Error(ERR_DROP, "No team has any other team hostile toward it, no battle is possible!");

	if (totalActiveTeams <= 0)
		Com_Error(ERR_DROP, "No Active teams detected in Auto Battle!");

	if (totalActiveTeams == 1) {
		Com_DPrintf(DEBUG_CLIENT, "Note: Only one active team detected, this team will win the auto mission battle by default.\n");
		battle->winningTeam = lastActiveTeam;
		return;
	}

	/* Set up teams */
	for (team = 0; team < AUTOMISSION_TEAM_TYPE_MAX; team++) {
		teamPooledHealth[team] = 0.0;
		teamPooledHealthMax[team] = 0.0;
		teamPooledUnitsHealthy[team] = 0.0;
		teamPooledUnitsTotal[team] = 0.0;

		if (battle->actUnits[team] > 0) {
			double skillAdjCalc;
			double skillAdjCalcAbs;

			for (currentUnit = 0; currentUnit < battle->nUnits[team]; currentUnit++) {
				autoUnit_t *unit = AM_GetUnit(battle, team, currentUnit);
				const character_t *chr = unit->chr;

				if (chr->HP <= 0)
					continue;

				teamPooledHealth[team] += chr->HP;
				teamPooledHealthMax[team] += chr->maxHP;
				teamPooledUnitsTotal[team] += 1.0;
				if (chr->HP == chr->maxHP)
					teamPooledUnitsHealthy[team] += 1.0;
			}
			/* We shouldn't be dividing by zero here. */
			assert(teamPooledHealthMax[team] > 0.0);
			assert(teamPooledUnitsTotal[team] > 0.0);

			teamRatioHealthTotal[team] = teamPooledHealth[team] / teamPooledHealthMax[team];
			teamRatioHealthyUnits[team] = teamPooledUnitsHealthy[team] / teamPooledUnitsTotal[team];

			/* In DEBUG mode, these should help with telling where things are at what time, for bug-hunting purposes. */
			/* Note (Destructavator):  Is there a better way to implement this?  Is there a set protocol for this type of thing? */
			Com_DPrintf(DEBUG_CLIENT, "Team %i has calculated ratio of healthy units of %f.\n",
					team, teamRatioHealthyUnits[team]);
			Com_DPrintf(DEBUG_CLIENT, "Team %i has calculated ratio of health values of %f.\n",
					team, teamRatioHealthTotal[team]);

			/** @todo speaking names please */
			skillAdjCalc = teamRatioHealthyUnits[team] + teamRatioHealthTotal[team];
			skillAdjCalc *= 0.50;
			skillAdjCalc = FpCurve1D_u_in(skillAdjCalc, 0.50, 0.50);
			skillAdjCalc -= 0.50;
			skillAdjCalcAbs = fabs(skillAdjCalc);
			if (skillAdjCalc > 0.0)
				battle->scoreTeamSkill[team] = ChkDNorm_Inv (FpCurveUp (battle->scoreTeamSkill[team], skillAdjCalcAbs) );
			else if (skillAdjCalc < 0.0)
				battle->scoreTeamSkill[team] = ChkDNorm (FpCurveDn (battle->scoreTeamSkill[team], skillAdjCalcAbs) );
			/* if (skillAdjCalc == exact 0.0), no change to team's skill. */

			Com_DPrintf(DEBUG_CLIENT, "Team %i has adjusted skill rating of %f.\n",
					team, battle->scoreTeamSkill[team]);
		}
	}
}

/**
 * @brief returns a randomly selected active team
 * @param[in] battle The battle we fight
 * @param[in] currTeam Current team we search for
 * @param[in] enemy If the team should be enemy or friendly
 */
static int AM_GetRandomTeam (autoMissionBattle_t *battle, int currTeam, qboolean enemy)
{
	int eTeam;

	assert(battle);
	assert(currTeam >= 0 && currTeam < AUTOMISSION_TEAM_TYPE_MAX);

	/* select a team randomly */
	eTeam = rand () % AUTOMISSION_TEAM_TYPE_MAX;
	/* if selected team is active and it's hostility match, we're ready */
	if (battle->actUnits[eTeam] > 0 && AM_IsHostile(battle, currTeam, eTeam) == enemy) {
		return eTeam;
	} else {
		int nextTeam;

		/* if not, check next */
		for (nextTeam = (eTeam + 1) % AUTOMISSION_TEAM_TYPE_MAX; nextTeam != eTeam; nextTeam = (nextTeam + 1) % AUTOMISSION_TEAM_TYPE_MAX) {
			if (battle->actUnits[nextTeam] > 0 && AM_IsHostile(battle, currTeam, nextTeam) == enemy)
				return nextTeam;
		}
		/* none found */
		return AUTOMISSION_TEAM_TYPE_MAX;
	}
}

/**
 * @brief returns a randomly selected alive unit from a team
 * @param[in] battle The battle we fight
 * @param[in] team Team to get unit from
 */
static autoUnit_t *AM_GetRandomActiveUnitOfTeam (autoMissionBattle_t *battle, int team)
{
	int idx;
	autoUnit_t *unit;

	assert(battle);
	if (team < 0 || team >= AUTOMISSION_TEAM_TYPE_MAX)
		return NULL;
	if (battle->actUnits[team] <= 0)
		return NULL;
	if (battle->nUnits[team] <= 0)
		return NULL;

	/* select a unit randomly */
	idx = rand() % battle->nUnits[team];
	unit = AM_GetUnit(battle, team, idx);

	/* if (s)he is active (alive, not stunned), we're ready */
	if (AM_IsUnitActive(unit)) {
		return unit;
	} else {
		int nextIdx;

		/* if not active, check next */
		for (nextIdx = (idx + 1) % battle->nUnits[team]; nextIdx != idx; nextIdx = (nextIdx + 1) % battle->nUnits[team]) {
			unit = AM_GetUnit(battle, team, nextIdx);
			if (AM_IsUnitActive(unit))
				return unit;
		}
		/* none found */
		return NULL;
	}
}

/**
 * @brief returns a randomly selected active unit
 * @param[in] battle The battle we fight
 * @param[in] currTeam Current team we search for
 * @param[in] enemy If the team should be enemy or friendly
 */
static autoUnit_t *AM_GetRandomActiveUnit (autoMissionBattle_t *battle, int currTeam, qboolean enemy)
{
	int eTeam;
	int nextTeam;
	autoUnit_t *unit;

	assert(battle);
	assert(currTeam >= 0 && currTeam < AUTOMISSION_TEAM_TYPE_MAX);

	eTeam = AM_GetRandomTeam(battle, currTeam, enemy);
	if (eTeam >= AUTOMISSION_TEAM_TYPE_MAX)
		return NULL;

	unit = AM_GetRandomActiveUnitOfTeam(battle, eTeam);
	if (unit)
		return unit;

	/* if not, check next */
	for (nextTeam = (eTeam + 1) % AUTOMISSION_TEAM_TYPE_MAX; nextTeam != eTeam; nextTeam = (nextTeam + 1) % AUTOMISSION_TEAM_TYPE_MAX) {
		if (battle->actUnits[nextTeam] > 0 && AM_IsHostile(battle, currTeam, nextTeam) == enemy) {
			unit = AM_GetRandomActiveUnitOfTeam(battle, nextTeam);
			if (unit)
				return unit;
		}
	}
	/* none found */
	return NULL;
}

/**
 * @brief Check and do attack on a team
 * @param[in, out] battle The battle we fight
 * @param[in] eTeam Team idx to attack
 * @param[in] currTeam Team idx that attacks
 * @param[in] currUnit Soldier idx who attacks
 * @param[in] effective Effectiveness of the attack
 */
static qboolean AM_CheckFire (autoMissionBattle_t *battle, autoUnit_t *currUnit, autoUnit_t *eUnit, const double effective)
{
	character_t *currChr = currUnit->chr;
	chrScoreGlobal_t *score = &currChr->score;
	character_t *eChr = eUnit->chr;
	double calcRand = frand();
	int strikeDamage;

	if (AM_IsHostile(battle, currUnit->team, eUnit->team)) {
		if (calcRand > effective)
			return qfalse;
		strikeDamage = (int) (100.0 * battle->scoreTeamDifficulty[currUnit->team] * (effective - calcRand) / effective);
		battle->teamAccomplishment[currUnit->team] += strikeDamage;
	} else {
		if (calcRand >= (0.050 - (effective * 0.050)))
			return qfalse;
		strikeDamage = (int) (100.0 * (1.0 - battle->scoreTeamDifficulty[currUnit->team]) * calcRand);
		battle->teamAccomplishment[currUnit->team] -= strikeDamage;
	}

	eChr->HP = max(0, eChr->HP - strikeDamage);

	/* If target is still active, continue */
	if (AM_IsUnitActive(eUnit))
		return qtrue;

#if DEBUG
	Com_Printf("AutoBattle: Team: %d Unit: %d killed Team: %d Unit: %d\n", currUnit->team, currUnit->idx, eUnit->team, eUnit->idx);
#endif
	battle->actUnits[eUnit->team]--;

	switch (currUnit->team) {
	case AUTOMISSION_TEAM_TYPE_PLAYER:
		switch (eUnit->team) {
		case AUTOMISSION_TEAM_TYPE_PLAYER:
			battle->results->ownSurvived--;
			battle->results->ownKilledFriendlyFire++;
			score->kills[KILLED_TEAM] += 1;
			break;
		case AUTOMISSION_TEAM_TYPE_ALIEN:
			battle->results->aliensSurvived--;
			battle->results->aliensKilled++;
			score->kills[KILLED_ENEMIES] += 1;
			break;
		case AUTOMISSION_TEAM_TYPE_CIVILIAN:
			battle->results->civiliansSurvived--;
			battle->results->civiliansKilledFriendlyFire++;
			score->kills[KILLED_CIVILIANS] += 1;
			break;
		default:
			break;
		}
		break;
	case AUTOMISSION_TEAM_TYPE_ALIEN:
		switch (eUnit->team) {
		case AUTOMISSION_TEAM_TYPE_PLAYER:
			battle->results->ownSurvived--;
			battle->results->ownKilled++;
			break;
		case AUTOMISSION_TEAM_TYPE_ALIEN:
			battle->results->aliensSurvived--;
			battle->results->aliensKilled++;
			break;
		case AUTOMISSION_TEAM_TYPE_CIVILIAN:
			battle->results->civiliansSurvived--;
			battle->results->civiliansKilled++;
			break;
		default:
			break;
		}
		break;
	case AUTOMISSION_TEAM_TYPE_CIVILIAN:
		switch (eUnit->team) {
		case AUTOMISSION_TEAM_TYPE_PLAYER:
			battle->results->ownSurvived--;
			battle->results->ownKilledFriendlyFire++;
			break;
		case AUTOMISSION_TEAM_TYPE_ALIEN:
			battle->results->aliensSurvived--;
			battle->results->aliensKilled++;
			break;
		case AUTOMISSION_TEAM_TYPE_CIVILIAN:
			battle->results->civiliansSurvived--;
			battle->results->civiliansKilledFriendlyFire++;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return qtrue;
}

/**
 * @brief Make Unit attack his enemies (or friends)
 * @param[in, out] battle The battle we fight
 * @param[in] currUnit Unit that attacks
 * @param[in] effective Effectiveness of the attack
 */
static qboolean AM_UnitAttackEnemy (autoMissionBattle_t *battle, autoUnit_t *currUnit, const double effective)
{
	autoUnit_t *eUnit;

	eUnit = AM_GetRandomActiveUnit(battle, currUnit->team, qtrue);
	/* no more enemies */
	if (eUnit == NULL)
		return qfalse;

	/* shot an enemy */
	if (!AM_CheckFire(battle, currUnit, eUnit, effective)) {
		/* if failed, attack a friendly */
		eUnit = AM_GetRandomActiveUnit(battle, currUnit->team, qfalse);
		if (eUnit != NULL)
			AM_CheckFire(battle, currUnit, eUnit, effective);
	}

	return qtrue;
}

/**
 * @brief Main Battle loop function
 * @param[in, out] battle The battle we fight
 */
static void AM_DoFight (autoMissionBattle_t *battle)
{
	qboolean combatActive = qtrue;

#ifdef DEBUG
	int teamID;

	Com_Printf("Auto battle started\n");
	for (teamID = 0; teamID < AUTOMISSION_TEAM_TYPE_MAX; teamID++) {
		Com_Printf("Team %d Units: %d\n", teamID, battle->nUnits[teamID]);
	}
#endif

	while (combatActive) {
		int team;
		for (team = 0; team < AUTOMISSION_TEAM_TYPE_MAX; team++) {
			int aliveUnits;
			int currentUnit;

			if (battle->actUnits[team] <= 0)
				continue;

			aliveUnits = 0;
			/* Is this unit still alive (has any health left?) */
			for (currentUnit = 0; currentUnit < battle->nUnits[team]; currentUnit++) {
				autoUnit_t *unit = AM_GetUnit(battle, team, currentUnit);
				character_t *chr = unit->chr;
				/* Wounded units don't fight quite as well */
				const double hpLeftRatio = chr->HP / chr->maxHP;
				const double effective = FpCurveDn(battle->scoreTeamSkill[team], hpLeftRatio * 0.50);

				if (!AM_IsUnitActive(unit))
					continue;

				Com_DPrintf(DEBUG_CLIENT, "Unit %i on team %i has adjusted attack rating of %f.\n",
						currentUnit, team, battle->scoreTeamSkill[team]);

				aliveUnits++;
				combatActive = AM_UnitAttackEnemy(battle, unit, effective);
			}
		}
	}

	/* Set results */
	if (battle->actUnits[AUTOMISSION_TEAM_TYPE_PLAYER] <= 0) {
		battle->results->won = qfalse;
		battle->winningTeam = AUTOMISSION_TEAM_TYPE_ALIEN;
	} else {
		battle->winningTeam = AUTOMISSION_TEAM_TYPE_PLAYER;
		battle->results->won = qtrue;
	}
}

/**
 * @brief This will display on-screen, for the player, results of the auto mission.
 * @param[in] battle Autobattle structure with the results
 * @todo results should be set in missionResult and this code should be merged with manual
 * mission result screen code, possibly in a new file: cp_mission_callbacks.c/h
 */
static void AM_DisplayResults (const autoMissionBattle_t *battle)
{
	assert(battle);

	Cvar_SetValue("cp_mission_tryagain", 0);
	if (battle->results->won) {
		UI_PushWindow("won", NULL, NULL);
		if (battle->teamAccomplishment[AUTOMISSION_TEAM_TYPE_PLAYER] > battle->teamAccomplishment[AUTOMISSION_TEAM_TYPE_ALIEN])
			MS_AddNewMessage(_("Notice"), _("You've won the battle"), qfalse, MSG_STANDARD, NULL);
		else
			MS_AddNewMessage(_("Notice"), _("You've defeated the enemy, but did poorly, and many civialians were killed"), qfalse, MSG_STANDARD, NULL);
	} else {
		UI_PushWindow("lost", NULL, NULL);
		MS_AddNewMessage(_("Notice"), _("You've lost the battle"), qfalse, MSG_STANDARD, NULL);
	}
}

/**
 * @brief Move equipment carried by the soldier/alien to the aircraft's itemcargo bay
 * @param[in, out] aircraft The craft with the team (and thus equipment) onboard.
 * @param[in, out] chr The character whose inventory should be moved
 */
static void AM_MoveCharacterInventoryIntoItemCargo (aircraft_t *aircraft, character_t *chr)
{
	containerIndex_t container;

	assert(aircraft != NULL);
	assert(chr != NULL);

	/* add items to itemcargo */
	for (container = 0; container < csi.numIDs; container++) {
		const invList_t *ic = CONTAINER(chr, container);

		while (ic) {
			const item_t *item = &ic->item;
			const invList_t *next = ic->next;

			if (item->t) {
				AII_CollectItem(aircraft, item->t, 1);

				if (item->a && item->m)
					AII_CollectItem(aircraft, item->m, 1);
			}
			ic = next;
		}
	}
}

/**
 * @brief Collect alien bodies and items after battle
 * @param[out] aircraft Mission aircraft that will bring stuff home
 * @param[in] battle The battle we fought
 */
static void AM_AlienCollect (aircraft_t *aircraft, const autoMissionBattle_t *battle)
{
	int unitIDX;
	int collected = 0;

	assert(aircraft);
	assert(battle);

	/* Aliens */
	for (unitIDX = 0; unitIDX < battle->nUnits[AUTOMISSION_TEAM_TYPE_ALIEN]; unitIDX++) {
		const autoUnit_t *unit = AM_GetUnit(battle, AUTOMISSION_TEAM_TYPE_ALIEN, unitIDX);

		if (AM_IsUnitActive(unit))
			continue;

		AM_MoveCharacterInventoryIntoItemCargo(aircraft, unit->chr);
		AL_AddAlienTypeToAircraftCargo(aircraft, unit->chr->teamDef, 1, unit->chr->HP <= 0);
		collected++;
	}

	if (collected > 0)
		MS_AddNewMessage(_("Notice"), _("Collected alien bodies"), qfalse, MSG_STANDARD, NULL);
}

/**
 * @brief This looks at a finished auto battle, and uses values from it to kill or lower health of surviving soldiers on a
 * mission drop ship as appropriate.  It also hands out some experience to soldiers that survive.
 * @param[in] battle The battle we fought
 * @param[in, out] aircraft Dropship soldiers are on
 */
static void AM_UpdateSurivorsAfterBattle (const autoMissionBattle_t *battle, struct aircraft_s *aircraft)
{
	int unit = 0;
	int battleExperience;

	assert(battle);
	assert(battle->results);

	battleExperience = max(0, battle->teamAccomplishment[AUTOMISSION_TEAM_TYPE_PLAYER]);

	LIST_Foreach(aircraft->acTeam, employee_t, soldier) {
		character_t *chr = &soldier->chr;
		chrScoreGlobal_t *score = &chr->score;
		int expCount;

		if (unit >= MAX_SOLDIERS_AUTOMISSION)
			break;

		unit++;

		/* dead soldiers are removed in CP_MissionEnd, just move their inventory to itemCargo */
		if (chr->HP <= 0) {
			if (battle->results->won)
				AM_MoveCharacterInventoryIntoItemCargo(aircraft, &soldier->chr);
			E_RemoveInventoryFromStorage(soldier);
		}

		for (expCount = 0; expCount < ABILITY_NUM_TYPES; expCount++)
			score->experience[expCount] += (int) (battleExperience * ABILITY_AWARD_SCALE * frand());

		for (expCount = ABILITY_NUM_TYPES; expCount < SKILL_NUM_TYPES; expCount++)
			score->experience[expCount] += (int) (battleExperience * SKILL_AWARD_SCALE * frand());
	}
}

/**
 * @brief Clean up alien and civilian teams
 * @param[in,out] battle The common autobattle descriptor structure
 */
static void AM_CleanBattleParameters (autoMissionBattle_t *battle)
{
	int unitIDX;

	assert(battle);

	/* Aliens */
	for (unitIDX = 0; unitIDX < battle->nUnits[AUTOMISSION_TEAM_TYPE_ALIEN]; unitIDX++) {
		autoUnit_t *unit = AM_GetUnit(battle, AUTOMISSION_TEAM_TYPE_ALIEN, unitIDX);

		AM_DestroyUnitChr(unit);
	}

	/* Civilians */
	for (unitIDX = 0; unitIDX < battle->nUnits[AUTOMISSION_TEAM_TYPE_CIVILIAN]; unitIDX++) {
		autoUnit_t *unit = AM_GetUnit(battle, AUTOMISSION_TEAM_TYPE_CIVILIAN, unitIDX);

		AM_DestroyUnitChr(unit);
	}
}

/**
 * @brief Handles the auto mission
 * @param[in,out] mission The mission to auto play
 * @param[in,out] aircraft The aircraft (or fake aircraft in case of a base attack)
 * @param[in] campaign The campaign data structure
 * @param[in] battleParameters Structure that holds the battle related parameters
 * @param[out] results Result of the mission
 */
void AM_Go (mission_t *mission, aircraft_t *aircraft, const campaign_t *campaign, const battleParam_t *battleParameters, missionResults_t *results)
{
	autoMissionBattle_t autoBattle;

	assert(mission);
	assert(aircraft);
	assert(aircraft->homebase);

	if (mission && mission->mapDef && mission->mapDef->storyRelated) {
		Com_Printf("Story-related mission cannot be done via automission\n");
		return;
	}

	AM_ClearBattle(&autoBattle);
	autoBattle.results = results;
	AM_FillTeamFromAircraft(&autoBattle, AUTOMISSION_TEAM_TYPE_PLAYER, aircraft, campaign);
	AM_FillTeamFromBattleParams(&autoBattle, battleParameters);
	AM_SetDefaultHostilities(&autoBattle, qfalse);
	AM_CalculateTeamScores(&autoBattle);

	OBJZERO(*results);
	results->ownSurvived = autoBattle.nUnits[AUTOMISSION_TEAM_TYPE_PLAYER];
	results->aliensSurvived = autoBattle.nUnits[AUTOMISSION_TEAM_TYPE_ALIEN];
	results->civiliansSurvived = autoBattle.nUnits[AUTOMISSION_TEAM_TYPE_CIVILIAN];
	results->mission = mission;

	/**
	 * @todo find out why this black-magic with inventory is needed and clean up
	 * @sa CP_StartSelectedMission
	 * @sa CP_MissionEnd
	 */
	ccs.eMission = aircraft->homebase->storage; /* copied, including arrays inside! */

	AM_DoFight(&autoBattle);

	AM_UpdateSurivorsAfterBattle(&autoBattle, aircraft);
	if (results->won)
		AM_AlienCollect(aircraft, &autoBattle);

	MIS_InitResultScreen(results);
	if (ccs.missionResultCallback) {
		ccs.missionResultCallback(results);
	}

	AM_DisplayResults(&autoBattle);
	AM_CleanBattleParameters(&autoBattle);
}

/**
 * @brief Init actions for automission-subsystem
 */
void AM_InitStartup (void)
{
}

/**
 * @brief Closing actions for automission-subsystem
 */
void AM_Shutdown (void)
{
}
