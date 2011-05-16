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

#include "cp_auto_mission.h"
#include "cp_campaign.h"
#include "../../../shared/mathlib_extra.h"

/**
 * @brief Clears, initializes, or resets a single auto mission, sets default values.
 * @param[in,out] battle The battle that should be init to defaults
 */
void CP_AutoBattleClearBattle (autoMissionBattle_t *battle)
{
	int team;
	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		int otherTeam;
		int soldier;

		battle->teamID[team] = -1;
		battle->teamActive[team] = qfalse;
		battle->nUnits[team] = 0;
		battle->scoreTeamDifficulty[team] = 0.5f;
		battle->scoreTeamEquipment[team] = 0.5f;
		battle->scoreTeamSkill[team] = 0.5f;

		for (otherTeam = 0; otherTeam < MAX_ACTIVETEAM; otherTeam++) {
			/* If you forget to set this and run a battle, everyone will just kill each other by default */
			battle->isHostile[team][otherTeam] = qtrue;
		}

		for (soldier = 0; soldier < MAX_SOLDIERS_AUTOMISSION; soldier++) {
			battle->unitHealth[team][soldier] = 0;
			battle->unitHealthMax[team][soldier] = 0;
		}
	}

	battle->winningTeam = -1;
	battle->isRescueMission = qfalse;
	battle->teamToRescue = -1;
	battle->teamNeedingRescue = -1;
	battle->hasBeenFought = qfalse;
}

/**
 * @brief Adds team data for a specified team in an auto-mission object, from a (player) aircraft.
 * @param[in,out] battle  The auto mission battle to add team data to
 * @param[in] teamNum The team number in the auto mission instance to update
 * @param[in] aircraft The aircraft to get data from
 * @param[in] campaign The current campaign (for retrieving difficulty level)
 * @note This function actually gets the data from the campaign ccs object, using the aircraft data to
 * find out which of all the employees are on the aircraft (in the mission)
 */
void CP_AutoBattleFillTeamFromAircraft (autoMissionBattle_t *battle, const int teamNum, const struct aircraft_s *aircraft, const struct campaign_s *cmpn)
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
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add soldiers to an auto-mission from an aircraft with no soldiers onboard.");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: Aliens might win this mission by default because they are un-challenged, with no resistance!");
	}
	if (unitsAlive == 0) {
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add team to auto battle where all the units on the team are DEAD!");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: This team will LOSE the battle by default.");
	}

	if (teamSize > 0)
		battle->teamActive[teamNum] = qtrue;

	/* Set a few defaults.  These can be overridden later with new values if needed. */
	battle->teamType[teamNum] = AUTOMISSION_TEAM_TYPE_PLAYER;

	/* NOTE:  For now these are hard-coded to values based upon general campaign difficulty. */
	/* --- In the future, it might be easier to set this according to a scripted value in a .ufo */
	/* --- file, with other campaign info.  Reminder:  Higher floating point values mean better */
	/* --- soldiers, and therefore make an easier fight for the player. */
	switch (cmpn->difficulty) {
		case 4:
			battle->scoreTeamDifficulty[teamNum] = 0.20;
			break;
		case 3:
			battle->scoreTeamDifficulty[teamNum] = 0.30;
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
			battle->scoreTeamDifficulty[teamNum] = 0.70;
			break;
		case -4:
			battle->scoreTeamDifficulty[teamNum] = 0.80;
			break;
		default:
			battle->scoreTeamDifficulty[teamNum] = 0.50;
	}

	/* Zero is default ID for human player team ID, at least for auto missions. */
	battle->teamID[teamNum] = 0;
}

/* These are "placeholders" for the functions not yet written out, to (hopefully) allow compiling the project without error (for now) */
void CP_AutoBattleFillTeamFromAircraftUGVs (autoMissionBattle_t *battle, const int teamNum, const struct aircraft_s *aircraft, const struct campaign_s *campaign)
{
	/** @todo */
}

void CP_AutoBattleFillTeamFromPlayerBase (autoMissionBattle_t *battle, const int teamNum, const int baseNum, const struct campaign_s *campaign)
{
	/** @todo */
}

void CP_AutoBattleFillTeamFromAlienGroup (autoMissionBattle_t *battle, const int teamNum, const struct alienTeamGroup_s *alienGroup)
{
	/** @todo */
}

void CP_AutoBattleCreateTeamFromScratch (autoMissionBattle_t *battle, const int teamNum)
{
	/** @todo */
}

int CP_AutoBattleGetWinningTeam (const autoMissionBattle_t *battle)
{
	/** @todo */
	if (battle->hasBeenFought == qfalse) {
		Com_Error(ERR_DROP, "Error:  Attempt to retrieve winning team from an auto mission that wasn't fought!");
		return -1;
	} else {
		if (battle->winningTeam == -1)
			Com_Error(ERR_DROP, "Error:  Auto mission has been fought, but no winning team was set!");
		return battle->winningTeam;
	}
	return 0;
}

/** @brief Run this on an auto mission battle before the battle is actually simulated with CP_AutoBattleRunBattle(), to set
 * default values for who will attack which team.  If you forget to do this before battle simulation, all teams will default
 * to "free for all" (Everyone will try to kill everyone else).
 * @param[in, out] battle The battle to set up team hostility values for.
 * @param[in] civsInfected Set to "qtrue" if civs have XVI influence, otherwise "qfalse" for a normal mission. */
void CP_AutoBattleSetDefaultHostilities(autoMissionBattle_t *battle, const qboolean civsInfected)
{
	int team;
	int otherTeam;

	qboolean civsInverted =! civsInfected;

	for (team = 0; team < MAX_ACTIVETEAM; team++) {
		if (battle->teamActive[team] == qtrue) {
			for (otherTeam = 0; otherTeam < MAX_ACTIVETEAM; otherTeam++) {
				if (battle->teamActive[otherTeam] == qtrue) {
					if (battle->teamType[team] == AUTOMISSION_TEAM_TYPE_PLAYER || battle->teamType[team] == AUTOMISSION_TEAM_TYPE_PLAYER_UGV) {
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_ALIEN || battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_ALIEN_DRONE)
							battle->isHostile[team][otherTeam] = qtrue;
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_PLAYER || battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_PLAYER_UGV)
							battle->isHostile[team][otherTeam] = qfalse;
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_CIVILIAN)
							battle->isHostile[team][otherTeam] = qfalse;
					}
					if (battle->teamType[team] == AUTOMISSION_TEAM_TYPE_ALIEN || battle->teamType[team] == AUTOMISSION_TEAM_TYPE_ALIEN_DRONE) {
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_ALIEN || battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_ALIEN_DRONE)
							battle->isHostile[team][otherTeam] = qfalse;
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_PLAYER || battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_PLAYER_UGV)
							battle->isHostile[team][otherTeam] = qtrue;
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_CIVILIAN)
							battle->isHostile[team][otherTeam] = civsInverted;
					}
					if (battle->teamType[team] == AUTOMISSION_TEAM_TYPE_CIVILIAN) {
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_ALIEN || battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_ALIEN_DRONE)
							battle->isHostile[team][otherTeam] = qfalse;
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_PLAYER || battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_PLAYER_UGV)
							battle->isHostile[team][otherTeam] = civsInfected;
						if (battle->teamType[otherTeam] == AUTOMISSION_TEAM_TYPE_CIVILIAN)
							battle->isHostile[team][otherTeam] = qfalse;
					}
				}
			}
		}
	}
}

void CP_AutoBattleRunBattle (autoMissionBattle_t *battle)
{
	int unitTotal = 0;
	int isHostileTotal = 0;
	int totalActiveTeams = 0;
	int lastActiveTeam = -1;
	int isHostileCount;
	int count;

	if (battle->hasBeenFought)
		Com_Error(ERR_DROP, "Error: Auto-Battle has already been fought!");

	for (count = 0; count < MAX_ACTIVETEAM; count++) {
		unitTotal += battle->nUnits[count];

		if (battle->teamActive[count]) {
			lastActiveTeam = count;
			totalActiveTeams++;
		}
		for (isHostileCount = 0; isHostileCount < MAX_ACTIVETEAM; isHostileCount++) {
			if (battle->nUnits[isHostileCount] > 0) {
				if (battle->isHostile[count][isHostileCount] && battle->teamActive[count])
					isHostileTotal++;
			}
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

	if (totalActiveTeams == 1)
		Com_DPrintf(DEBUG_CLIENT, "Note: Only one active team detected, this team will win the auto mission battle by default.");

	/* Quick easy victory check */
	if (totalActiveTeams == 1) {
		battle->winningTeam = lastActiveTeam;
		battle->hasBeenFought = qtrue;
		return;
	}

	/* Set up teams */
	double teamPooledHealth[MAX_ACTIVETEAM];
	double teamPooledHealthMax[MAX_ACTIVETEAM];
	double teamPooledUnitsHealthy[MAX_ACTIVETEAM];
	double teamPooledUnitsTotal[MAX_ACTIVETEAM];
	int currentUnit;

	for (count = 0; count < MAX_ACTIVETEAM; count++) {
		teamPooledHealth[count] = 0.0;
		teamPooledHealthMax[count] = 0.0;
		teamPooledUnitsHealthy[count] = 0.0;
		teamPooledUnitsTotal[count] = 0.0;

		if (battle->teamActive[count]) {
			for (currentUnit = 0; currentUnit < battle->nUnits[count]; currentUnit++) {
				if (battle->unitHealth[count][currentUnit] > 0) {
					teamPooledHealth[count] += battle->unitHealth[count][currentUnit];
					teamPooledHealthMax[count] += battle->unitHealthMax[count][currentUnit];
					teamPooledUnitsTotal[count] += 1.0;
					if (battle->unitHealth[count][currentUnit] == battle->unitHealthMax[count][currentUnit])
						teamPooledUnitsHealthy[count] += 1.0;
				}
			}
		}
	}
}
