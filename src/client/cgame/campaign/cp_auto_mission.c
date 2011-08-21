/**
 * @file cp_auto_mission.c
 * @brief single player automatic (quick, simulated) missions, without going to the battlescape.
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

#include "../../cl_shared.h"
#include "cp_auto_mission.h"
#include "cp_campaign.h"
#include "cp_map.h"
#include "cp_missions.h"
#include "cp_mission_triggers.h"
#include "../../ui/ui_windows.h"
#include "../../../shared/mathlib_extra.h"
#include "math.h"
#include "cp_auto_mission_callbacks.h"

/** @brief Possible types of teams that can fight in an auto mission battle.
 * @note This is independent of (int) teamID in the auto mission battle struct.  This allows, in the future, for multiple player teams, or multiple
 * alien teams, etc., to fight in a simulated battle.  (Different teams can have the same TYPE as listed here, yet have different teamID values.)
 */
typedef enum autoMission_teamType_s {
	AUTOMISSION_TEAM_TYPE_PLAYER,				/**< Human player-controlled team.  Includes soldiers as well as downed pilots. */
	AUTOMISSION_TEAM_TYPE_ALIEN,				/**< AI-controlled alien team. */
	AUTOMISSION_TEAM_TYPE_CIVILIAN,				/**< AI-controlled civilians that can be healthy or infected. */

	AUTOMISSION_TEAM_TYPE_MAX
} autoMissionTeamType_t;

#define MAX_SOLDIERS_AUTOMISSION MAX_TEAMS * MAX_ACTIVETEAM

/** @brief Data structure for a simulated or auto mission.
 * @note Supports a calculated max of so many teams that can be simulated in a battle, to
 * include any aliens, player soldiers, downed pilots, civilians, and any other forces.
 * The player's forces don't have to be any of the teams.  This is useful if a special
 * battle should be simulated for the plot, or if more than one alien threat is in the
 * game (not in the primary campaign plot though, but good for a MOD or whatever).
 * ALSO: A team does not have to attack (isHostile toward) another team that attacks it.
 * Teams that are isHostile toward no one will wander around like sheep, doing nothing else. */
typedef struct autoMissionBattle_s {
	qboolean teamActive[MAX_ACTIVETEAM];					/**< Which teams exist in a battle, supports hardcoded MAX of 8 teams */
	int teamID[MAX_ACTIVETEAM];								/**< An ID for each team, to keep track, useful if needed.  Note: The same ID may be repeated, but two teams of same ID can't be hostile */
	qboolean isHostile[MAX_ACTIVETEAM][MAX_ACTIVETEAM];		/**< Friendly or hostile?  Params are [Each team] [Each other team] */
	short nUnits[MAX_ACTIVETEAM];							/**< Number of units (soldiers, aliens, UGVs, whatever) on each team, hardcoded MAX of 64 per team */
	autoMissionTeamType_t teamType[MAX_ACTIVETEAM];		/**< What type of team is this?  Human player?  Alien? Something else?  */

	double scoreTeamEquipment[MAX_ACTIVETEAM];			/**< Number from 0.f to 1.f, represents how good a team's equipment is (higher is better) */
	double scoreTeamSkill[MAX_ACTIVETEAM];				/**< Number from 0.f to 1.f, represents how good a team's abilities are (higher is better) */
	double scoreTeamDifficulty[MAX_ACTIVETEAM];		/**< Number from 0.f to 1.f, represents a team's global fighting ability, difficulty, or misc. adjustments (higher is better) */

	int unitHealth[MAX_ACTIVETEAM][MAX_SOLDIERS_AUTOMISSION];		/**< Health score of each unit for each team */
	int unitHealthMax[MAX_ACTIVETEAM][MAX_SOLDIERS_AUTOMISSION];	/**< Max health of each unit on each team */
	int teamAccomplishment[MAX_ACTIVETEAM];							/**< Used for calculating experience gain, and for friendly fire (including hit civilians) */
	int unitKills[MAX_ACTIVETEAM][MAX_SOLDIERS_AUTOMISSION];		/**< Number of individual kills each unit accomplishes (for experience award purposes) */

	int winningTeam;								/**< Which team is victorious */
	qboolean hasBeenFought;							/**< Did this battle run already?  Auto Battles can be fought only once, please. */
	qboolean isRescueMission;						/**< Is this a rescue or special mission? (Such as recovering a downed aircraft pilot) */
	int teamToRescue;								/**< If a rescue mission, which team needs rescue? */
	int teamNeedingRescue;							/**< If a rescue mission, which team is attempting the rescue? */
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

/**
 * @brief Clears, initializes, or resets a single auto mission, sets default values.
 * @param[in,out] battle The battle that should be initialized to defaults
 */
static void AM_ClearBattle (autoMissionBattle_t *battle)
{
	int team;

	assert(battle != NULL);

	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		int otherTeam;
		int soldier;

		battle->teamID[team] = -1;
		battle->teamType[team] = AUTOMISSION_TEAM_TYPE_MAX;
		battle->teamActive[team] = qfalse;
		battle->nUnits[team] = 0;
		battle->scoreTeamDifficulty[team] = 0.5;
		battle->scoreTeamEquipment[team] = 0.5;
		battle->scoreTeamSkill[team] = 0.5;
		battle->teamAccomplishment[team] = 0;

		for (otherTeam = 0; otherTeam < MAX_ACTIVETEAM; otherTeam++) {
			/* If you forget to set this and run a battle, everyone will just kill each other by default */
			battle->isHostile[team][otherTeam] = qtrue;
		}

		for (soldier = 0; soldier < MAX_SOLDIERS_AUTOMISSION; soldier++) {
			battle->unitHealth[team][soldier] = 0;
			battle->unitHealthMax[team][soldier] = 0;
			battle->unitKills[team][soldier] = 0;
		}
	}

	battle->winningTeam = -1;
	battle->isRescueMission = qfalse;
	battle->teamToRescue = -1;
	battle->teamNeedingRescue = -1;
	battle->hasBeenFought = qfalse;
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
static void AM_FillTeamFromAircraft (autoMissionBattle_t *battle, const int teamNum, const aircraft_t *aircraft, const campaign_t *campaign)
{
	employee_t *employee;
	int teamSize;
	int unitsAlive;

	assert(teamNum >= 0 && teamNum < MAX_ACTIVETEAM);
	assert(battle != NULL);
	assert(aircraft != NULL);

	teamSize = 0;
	unitsAlive = 0;
	LIST_Foreach(aircraft->acTeam, employee_t, employee) {
		const character_t *chr = &employee->chr;

		battle->unitHealthMax[teamNum][teamSize] = chr->maxHP;
		battle->unitHealth[teamNum][teamSize] = chr->HP;
		teamSize++;
		if (chr->HP > 0)
			unitsAlive++;

		if (teamSize >= MAX_SOLDIERS_AUTOMISSION)
			break;
	}
	battle->nUnits[teamNum] = teamSize;

	if (teamSize == 0) {
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add soldiers to an auto-mission from an aircraft with no soldiers onboard.\n");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: Aliens might win this mission by default because they are un-challenged, with no resistance!\n");
	}
	if (unitsAlive == 0) {
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add team to auto battle where all the units on the team are DEAD!\n");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: This team will LOSE the battle by default.\n");
	}

	if (teamSize > 0)
		battle->teamActive[teamNum] = qtrue;

	/* Set a few defaults.  These can be overridden later with new values if needed. */
	battle->teamType[teamNum] = AUTOMISSION_TEAM_TYPE_PLAYER;

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

	/* Zero is default ID for human player team ID, at least for auto missions. */
	battle->teamID[teamNum] = 0;
}

/**
 * @brief Creates team data for alien and civilian teams based on the mission parameters data.
 * @param[in,out] battle The auto mission battle to add team data to
 * @param[in] missionParam Mission parameters data to use
 */
static void AM_FillTeamFromBattleParams (autoMissionBattle_t *battle, const battleParam_t *missionParams)
{
	int numAliensTm;
	int numAlienDronesTm;
	int numCivsTm;
	int unit;

	/* These are used to calculate possible generated health scores of non-player units. */
	/* Adjust these to change game balance. */
	/* TODO:  Should this be scripted instead, from a .ufo file? */
	const float autoGenHealthAliens = 200.f;
	const float autoGenHealthAlienDrones = 300.f;
	const float autoGenHealthCivilians = 40.f;

	assert(battle);
	assert(missionParams);

	numAliensTm = missionParams->aliens;
	numAlienDronesTm = (int) (frand() * numAliensTm);
	numCivsTm = missionParams->civilians;

	/* Alines will go on team 2, alien drones on 3, civs on 4 (player soldiers are 0 and UGVs are 1). */
	battle->nUnits[AUTOMISSION_TEAM_TYPE_ALIEN] = numAliensTm;
	battle->nUnits[AUTOMISSION_TEAM_TYPE_CIVILIAN] = numCivsTm;

	/* Populate the teams */

	/* Aliens */
	if (numAliensTm > 0) {
		for (unit = 0; unit < numAliensTm; unit++) {
			/* Quick, ugly way of deciding alien health scores.  Eventually we'll need something better. */
			const int healthMaxm = (int) (frand() * autoGenHealthAliens) + 10.f;
			const int health = (int) (frand() * (healthMaxm - 5)) + 5;
			battle->unitHealthMax[AUTOMISSION_TEAM_TYPE_ALIEN][unit] = healthMaxm;
			battle->unitHealth[AUTOMISSION_TEAM_TYPE_ALIEN][unit] = health;
		}
		battle->teamActive[AUTOMISSION_TEAM_TYPE_ALIEN] = qtrue;
		battle->teamType[AUTOMISSION_TEAM_TYPE_ALIEN] = AUTOMISSION_TEAM_TYPE_ALIEN;
		battle->scoreTeamSkill[AUTOMISSION_TEAM_TYPE_ALIEN] = (frand() * 0.6f) + 0.2f;
	}

	if (numAlienDronesTm > 0) {
		for (unit = numAliensTm; unit < numAliensTm + numAlienDronesTm; unit++) {
			/* Quick, ugly way of deciding alien drone health scores.  Eventually we'll need something better. */
			const int healthMaxm = (int) (frand() * autoGenHealthAlienDrones) + 10.f;
			const int health = (int) (frand() * (healthMaxm - 5)) + 5;
			battle->unitHealthMax[AUTOMISSION_TEAM_TYPE_ALIEN][unit] = healthMaxm;
			battle->unitHealth[AUTOMISSION_TEAM_TYPE_ALIEN][unit] = health;
		}
	}

	/* Civilians (if any) */
	if (numCivsTm > 0) {
		for (unit = 0; unit < numCivsTm; unit++) {
			/* Quick, ugly way of deciding civilian health scores.  Eventually we'll need something better. */
			const int healthMaxm = (int) (frand() * autoGenHealthCivilians) + 10.f;
			const int health = (int) (frand() * (healthMaxm - 5)) + 5;
			battle->unitHealthMax[AUTOMISSION_TEAM_TYPE_CIVILIAN][unit] = healthMaxm;
			battle->unitHealth[AUTOMISSION_TEAM_TYPE_CIVILIAN][unit] = health;
		}
		battle->teamActive[AUTOMISSION_TEAM_TYPE_CIVILIAN] = qtrue;
		battle->teamType[AUTOMISSION_TEAM_TYPE_CIVILIAN] = AUTOMISSION_TEAM_TYPE_CIVILIAN;
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
	int team;
	int otherTeam;
	qboolean civsInverted = !civsInfected;

	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		if (!battle->teamActive[team])
			continue;

		for (otherTeam = 0; otherTeam < MAX_ACTIVETEAM; otherTeam++) {
			const autoMissionTeamType_t teamType = battle->teamType[team];
			const autoMissionTeamType_t otherTeamType = battle->teamType[otherTeam];

			if (!battle->teamActive[otherTeam])
				continue;

			if (AM_IsPlayer(teamType)) {
				if (AM_IsAlien(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, qtrue);
				else if (AM_IsPlayer(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, qfalse);
				else if (AM_IsCivilian(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, qfalse);
			} else if (AM_IsAlien(teamType)) {
				if (AM_IsAlien(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, qfalse);
				else if (AM_IsPlayer(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, qtrue);
				else if (AM_IsCivilian(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, civsInverted);
			} else if (AM_IsCivilian(teamType)) {
				if (AM_IsAlien(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, qfalse);
				else if (AM_IsPlayer(otherTeamType))
					AM_SetHostile(battle, team, otherTeam, civsInfected);
				else if (AM_IsCivilian(otherTeamType))
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
	double teamPooledHealth[MAX_ACTIVETEAM];
	double teamPooledHealthMax[MAX_ACTIVETEAM];
	double teamPooledUnitsHealthy[MAX_ACTIVETEAM];
	double teamPooledUnitsTotal[MAX_ACTIVETEAM];
	/* Ratios */
	double teamRatioHealthyUnits[MAX_ACTIVETEAM];
	double teamRatioHealthTotal[MAX_ACTIVETEAM];
	int currentUnit;

	if (battle->hasBeenFought)
		Com_Error(ERR_DROP, "Error: Auto-Battle has already been fought!");

	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		unitTotal += battle->nUnits[team];

		if (battle->teamActive[team]) {
			lastActiveTeam = team;
			totalActiveTeams++;
		}
		for (isHostileCount = 0; isHostileCount < MAX_ACTIVETEAM; isHostileCount++) {
			if (battle->nUnits[isHostileCount] <= 0)
				continue;

			if (battle->isHostile[team][isHostileCount] && battle->teamActive[team])
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
		battle->hasBeenFought = qtrue;
		return;
	}

	/* Set up teams */
	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		teamPooledHealth[team] = 0.0;
		teamPooledHealthMax[team] = 0.0;
		teamPooledUnitsHealthy[team] = 0.0;
		teamPooledUnitsTotal[team] = 0.0;

		if (battle->teamActive[team]) {
			double skillAdjCalc;
			double skillAdjCalcAbs;

			for (currentUnit = 0; currentUnit < battle->nUnits[team]; currentUnit++) {
				if (battle->unitHealth[team][currentUnit] <= 0)
					continue;

				teamPooledHealth[team] += battle->unitHealth[team][currentUnit];
				teamPooledHealthMax[team] += battle->unitHealthMax[team][currentUnit];
				teamPooledUnitsTotal[team] += 1.0;
				if (battle->unitHealth[team][currentUnit] == battle->unitHealthMax[team][currentUnit])
					teamPooledUnitsHealthy[team] += 1.0;
			}
			/* We shouldn't be dividing by zero here. */
			assert(teamPooledHealthMax[team] > 0.0);
			assert(teamPooledUnitsTotal[team] > 0.0);

			teamRatioHealthTotal[team] = teamPooledHealth[team] / teamPooledHealthMax[team];
			teamRatioHealthyUnits[team] = teamPooledUnitsHealthy[team] / teamPooledUnitsTotal[team];

			/* In DEBUG mode, these should help with telling where things are at what time, for bug-hunting purposes. */
			/* Note (Destructavator):  Is there a better way to implement this?  Is there a set protocol for this type of thing? */
			Com_DPrintf(DEBUG_CLIENT, "Team %i has calculated ratio of healthy units of %lf.\n",
					team, teamRatioHealthyUnits[team]);
			Com_DPrintf(DEBUG_CLIENT, "Team %i has calculated ratio of health values of %lf.\n",
					team, teamRatioHealthTotal[team]);

			/** @todo speaking names please */
			skillAdjCalc = teamRatioHealthyUnits[team] + teamRatioHealthTotal[team];
			skillAdjCalc *= 0.50;
			skillAdjCalc = FpCurve1D_u_in(skillAdjCalc, 0.50, 0.50);
			skillAdjCalc -= 0.50;
			skillAdjCalcAbs = fabs(skillAdjCalc);
			if (skillAdjCalc > 0.0)
				battle->scoreTeamSkill[team] = FpCurveUp (battle->scoreTeamSkill[team], skillAdjCalcAbs);
			else if (skillAdjCalc < 0.0)
				battle->scoreTeamSkill[team] = FpCurveDn (battle->scoreTeamSkill[team], skillAdjCalcAbs);
			/* if (skillAdjCalc == exact 0.0), no change to team's skill. */

			Com_DPrintf(DEBUG_CLIENT, "Team %i has adjusted skill rating of %lf.\n",
					team, battle->scoreTeamSkill[team]);
		}
	}
}

/**
 * @brief Check and do attack on a frendly team (by a low chance)
 * @param[in, out] battle The battle we fight
 * @param[in] eTeam Team idx to attack
 * @param[in] currTeam Team idx that attacks
 * @param[in] currUnit Soldier idx who attacks
 * @param[in] effective Effectiveness of the attack
 */
static void AM_CheckFriendlyFire (autoMissionBattle_t *battle, int eTeam, const int currTeam, const int currUnit, const double effective)
{
	int eUnit;

	/* Check for "friendly" fire */
	for (eUnit = 0; eUnit < battle->nUnits[eTeam]; eUnit++) {
		if (battle->unitHealth[eTeam][eUnit] > 0) {
			const double calcRand = frand();

			if (calcRand < (0.050 - (effective * 0.050))) {
				const int strikeDamage = (int) (100.0 * (1.0 - battle->scoreTeamDifficulty[currTeam]) * calcRand);

				battle->unitHealth[eTeam][eUnit] = max(0, battle->unitHealth[eTeam][eUnit] - strikeDamage);

				Com_DPrintf(DEBUG_CLIENT, "Unit %i on team %i hits unit %i on team %i for %i damage via friendly fire!\n",
						currUnit, currTeam, eUnit, eTeam, strikeDamage);

				if (battle->unitHealth[eTeam][eUnit] == 0)
					Com_DPrintf(DEBUG_CLIENT, "Friendly Unit %i on team %i is killed in action!\n",
							eUnit, eTeam);

				battle->teamAccomplishment[currTeam] -= strikeDamage;
			}
		}
	}
}

/**
 * @brief Check and do attack on enemy team
 * @param[in, out] battle The battle we fight
 * @param[in] eTeam Team idx to attack
 * @param[in] currTeam Team idx that attacks
 * @param[in] currUnit Soldier idx who attacks
 * @param[in] effective Effectiveness of the attack
 */
static void AM_CheckFire (autoMissionBattle_t *battle, int eTeam, const int currTeam, const int currUnit, const double effective)
{
	int eUnit;

	for (eUnit = 0; eUnit < battle->nUnits[eTeam]; eUnit++) {
		if (battle->unitHealth[eTeam][eUnit] > 0) {
			const double calcRand = frand();

			if (calcRand <= effective) {
				const int strikeDamage = (int) (100.0 * battle->scoreTeamDifficulty[currTeam] * (effective - calcRand) / effective);

				battle->unitHealth[eTeam][eUnit] = max(0, battle->unitHealth[eTeam][eUnit] - strikeDamage);

				Com_DPrintf(DEBUG_CLIENT, "(Debug/value track) Unit %i on team %i strikes unit %i on team %i for %i damage!\n",
						currUnit, currTeam, eUnit, eTeam, strikeDamage);

				if (battle->unitHealth[eTeam][eUnit] == 0) {
					Com_DPrintf(DEBUG_CLIENT, "Unit %i on team %i is killed in action!\n", eUnit, eTeam);
					battle->unitKills[eTeam][eUnit] += 1;
				}

				battle->teamAccomplishment[currTeam] += strikeDamage;
			}
		}
	}
}

/**
 * @brief Make Unit attack his enemies (or friends)
 * @param[in, out] battle The battle we fight
 * @param[in] currTeam Team idx that attacks
 * @param[in] currUnit Soldier idx who attacks
 * @param[in] effective Effectiveness of the attack
 */
static qboolean AM_UnitAttackEnemies (autoMissionBattle_t *battle, const int currTeam, const int currUnit, const double effective)
{
	int eTeam;
	int count = 0;

	Com_DPrintf(DEBUG_CLIENT, "Unit %i on team %i attacks!\n", currUnit, currTeam);

	for (eTeam = 0; eTeam < MAX_ACTIVETEAM; eTeam++) {
		if (!battle->teamActive[eTeam])
			continue;

		if (battle->isHostile[currTeam][eTeam]) {
			count++;
			AM_CheckFire(battle, eTeam, currTeam, currUnit, effective);
		} else {
			AM_CheckFriendlyFire(battle, eTeam, currTeam, currUnit, effective);
		}
	}

	/* If there's no one left to fight, the battle's OVER. */
	if (count == 0)
		return qfalse;

	return qtrue;
}

/**
 * @brief Main Battle loop function
 * @param[in, out] battle The battle we fight
 */
static void AM_DoFight (autoMissionBattle_t *battle)
{
	qboolean combatActive = qtrue;

	while (combatActive) {
		int team;
		for (team = 0; team < MAX_ACTIVETEAM; team++) {
			int aliveUnits;
			int currentUnit;

			if (!battle->teamActive[team])
				continue;

			aliveUnits = 0;
			/* Is this unit still alive (has any health left?) */
			for (currentUnit = 0; currentUnit < battle->nUnits[team]; currentUnit++) {
				/* Wounded units don't fight quite as well */
				const double hpLeftRatio = battle->unitHealth[team][currentUnit] / battle->unitHealthMax[team][currentUnit];
				const double effective = FpCurveDn(battle->scoreTeamSkill[team], hpLeftRatio * 0.50);

				if (battle->unitHealth[team][currentUnit] <= 0)
					continue;

				Com_DPrintf(DEBUG_CLIENT, "Unit %i on team %i has adjusted attack rating of %lf.\n",
						currentUnit, team, battle->scoreTeamSkill[team]);

				aliveUnits++;
				combatActive = AM_UnitAttackEnemies(battle, team, currentUnit, effective);
			}

			if (aliveUnits == 0) {
				battle->teamActive[team] = qfalse;
				Com_DPrintf(DEBUG_CLIENT, "Team %i has been DEFEATED and is OUT OF ACTION.\n", team);
			}
		}
	}
	battle->hasBeenFought = qtrue;
}

/**
 * @brief Check mission results and select winner
 * @param[in, out] battle The battle we fight
 */
static void AM_DecideResults (autoMissionBattle_t *battle)
{
	int team;
	int teamPlayer = -1;
	int teamEnemy = -1;

	if (!battle->hasBeenFought)
		Com_Error(ERR_DROP, "Error:  Attempt to determine winning team from an auto mission that wasn't fought!");

	/* Figure out who's who (determine which team is the player and which one is aliens.) */
	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		switch (battle->teamType[team]) {
		case AUTOMISSION_TEAM_TYPE_PLAYER:
			teamPlayer = team;
			break;
		case AUTOMISSION_TEAM_TYPE_ALIEN:
			teamEnemy = team;
			break;
		default:
			break;
		}
	}

	assert(teamPlayer >= 0);
	assert(teamEnemy >= 0);

	/* Well, if the player team is all dead, we know the player totally lost, and can stop right here. */
	if (!battle->teamActive[teamPlayer]) {
		battle->results->won = qfalse;
		battle->winningTeam = teamEnemy;
	} else {
		battle->winningTeam = teamPlayer;
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
 * @brief Collect alien bodies for auto missions
 * @param[out] aircraft mission aircraft that will bring bodies home
 * @param[in] battleParameters Parameters structure that knows how much aliens fought
 * @note collect all aliens as dead ones
 */
static void AM_AlienCollect (aircraft_t *aircraft, const battleParam_t *battleParameters)
{
	int i;
	int aliens = battleParameters->aliens;

	if (!aliens)
		return;

	MS_AddNewMessage(_("Notice"), _("Collected dead alien bodies"), qfalse, MSG_STANDARD, NULL);

	while (aliens > 0) {
		const alienTeamGroup_t *group = battleParameters->alienTeamGroup;
		for (i = 0; i < group->numAlienTeams; i++) {
			const teamDef_t *teamDef = group->alienTeams[i];
			const int addDeadAlienAmount = aliens > 1 ? rand() % aliens : aliens;
			if (!addDeadAlienAmount)
				continue;
			assert(i < MAX_CARGO);
			assert(teamDef);
			AL_AddAlienTypeToAircraftCargo(aircraft, teamDef, addDeadAlienAmount, qtrue);
			aliens -= addDeadAlienAmount;
			if (!aliens)
				break;
		}
	}
}

/**
 * @brief Move equipment carried by the soldier to the aircraft's itemcargo bay
 * @param[in, out] aircraft The craft with the team (and thus equipment) onboard.
 * @param[in, out] soldier The soldier whose inventory should be moved
 */
static void AM_MoveEmployeeInventoryIntoItemCargo (aircraft_t *aircraft, employee_t *soldier)
{
	containerIndex_t container;

	assert(aircraft != NULL);
	assert(soldier != NULL);

	if (!AIR_IsInAircraftTeam(aircraft, soldier)) {
		Com_DPrintf(DEBUG_CLIENT, "AM_MoveEmployeeInventoryIntoItemCargo: Soldier is not on the aircraft.\n");
		return;
	}

	/* add items to itemcargo */
	for (container = 0; container < csi.numIDs; container++) {
		const character_t *chr = &soldier->chr;
		const invList_t *ic = CONTAINER(chr, container);

		while (ic) {
			/** @todo isn't a pointer enough here, do we really have to do a copy of the item_t struct */
			const item_t item = ic->item;
			const invList_t *next = ic->next;

			if (item.t) {
				AII_CollectItem(aircraft, item.t, 1);

				if (item.a && item.m)
					AII_CollectItem(aircraft, item.m, 1);
			}
			ic = next;
		}
	}
	/* remove items from base storage */
	E_RemoveInventoryFromStorage(soldier);
}

/**
 * @brief This looks at a finished auto battle, and uses values from it to kill or lower health of surviving soldiers on a
 * mission drop ship as appropriate.  It also hands out some experience to soldiers that survive.
 * @param[in] battle The battle we fought
 * @param[in, out] aircraft Dropship soldiers are on
 */
static void AM_UpdateSurivorsAfterBattle (const autoMissionBattle_t *battle, struct aircraft_s *aircraft)
{
	employee_t *soldier;
	int unit = 0;
	int battleExperience = max(0, battle->teamAccomplishment[AUTOMISSION_TEAM_TYPE_PLAYER]);

	LIST_Foreach(aircraft->acTeam, employee_t, soldier) {
		character_t *chr = &soldier->chr;
		chrScoreGlobal_t *score = &chr->score;
		int expCount;

		if (unit >= MAX_SOLDIERS_AUTOMISSION)
			break;

		chr->HP = battle->unitHealth[AUTOMISSION_TEAM_TYPE_PLAYER][unit];
		score->kills[KILLED_ENEMIES] += battle->unitKills[AUTOMISSION_TEAM_TYPE_PLAYER][unit];
		unit++;

		/* dead soldiers are removed in CP_MissionEnd, just move their inventory to itemCargo */
		if (chr->HP <= 0)
			AM_MoveEmployeeInventoryIntoItemCargo(aircraft, soldier);

		for (expCount = 0; expCount < ABILITY_NUM_TYPES; expCount++)
			score->experience[expCount] += (int) (battleExperience * ABILITY_AWARD_SCALE * frand());

		for (expCount = ABILITY_NUM_TYPES; expCount < SKILL_NUM_TYPES; expCount++)
			score->experience[expCount] += (int) (battleExperience * SKILL_AWARD_SCALE * frand());

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

	AM_ClearBattle(&autoBattle);
	autoBattle.results = results;
	AM_FillTeamFromAircraft(&autoBattle, 0, aircraft, campaign);
	AM_FillTeamFromBattleParams(&autoBattle, battleParameters);
	AM_SetDefaultHostilities(&autoBattle, qfalse);
	AM_CalculateTeamScores(&autoBattle);
	AM_DoFight(&autoBattle);
	AM_DecideResults(&autoBattle);

	AM_UpdateSurivorsAfterBattle(&autoBattle, aircraft);
	AM_AlienCollect(aircraft, battleParameters);

	/* This block is old code, but it will be left in for now, until exact numbers and stats are extracted from the auto mission results. */
	results->aliensKilled = battleParameters->aliens;
	results->aliensStunned = 0;
	results->aliensSurvived = 0;
	results->civiliansKilled = 0;
	results->civiliansKilledFriendlyFire = 0;
	results->civiliansSurvived = battleParameters->civilians;
	results->ownKilled = 0;
	results->ownKilledFriendlyFire = 0;
	results->ownStunned = 0;
	results->ownSurvived = AIR_GetTeamSize(aircraft);
	CP_InitMissionResults(results->won, results);

	AM_DisplayResults(&autoBattle);
}

/**
 * @brief Init actions for automission-subsystem
 */
void AM_InitStartup (void)
{
	AM_InitCallbacks();
}

/**
 * @brief Closing actions for automission-subsystem
 */
void AM_Shutdown (void)
{
	AM_ShutdownCallbacks();
}
