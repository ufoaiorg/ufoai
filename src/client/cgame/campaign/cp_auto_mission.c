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
	for (team = 0; team < 8; team++) {
		int otherTeam;
		int soldier;

		battle->teamID[team] = -1;
		battle->teamActive[team] = qfalse;
		battle->nUnits[team] = 0;
		battle->scoreTeamDifficulty[team] = 0.5f;
		battle->scoreTeamEquipment[team] = 0.5f;
		battle->scoreTeamSkill[team] = 0.5f;
		battle->scoreTeamSpecialAttributeA[team] = 0.5f;
		battle->scoreTeamSpecialAttributeB[team] = 0.5f;
		battle->scoreTeamSpecialAttributeC[team] = 0.5f;
		battle->scoreTeamSpecialAttributeD[team] = 0.5f;

		for (otherTeam = 0; otherTeam < 8; otherTeam++) {
			/* If you forget to set this and run a battle, everyone will just kill each other by default */
			battle->isHostile[team][otherTeam] = qtrue;
		}

		for (soldier = 0; soldier < 64; soldier++) {
			battle->unitHealth[team][soldier] = 0;
			battle->unitHealthMax[team][soldier] = 0;
		}
	}
	battle->useSpecialAttributeFactor[0] = qfalse;
	battle->useSpecialAttributeFactor[1] = qfalse;
	battle->useSpecialAttributeFactor[2] = qfalse;
	battle->useSpecialAttributeFactor[3] = qfalse;

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
 * @note This function actually gets the data from the campaign ccs object, using the aircraft data to
 * find out which of all the employees are on the aircraft (in the mission)
 */
void CP_AutoBattleFillTeamFromAircraft (autoMissionBattle_t *battle, const short teamNum, const aircraft_t *aircraft)
{
	employee_t *employee;
	int teamSize;

	assert(teamNum >= 0 && teamNum < MAX_ACTIVETEAM);
	assert(battle);
	assert(aircraft);

	teamSize = 0;
	LIST_Foreach (aircraft->acTeam, employee_t, employee) {
		const character_t *chr = &employee->chr;

		battle->unitHealthMax[teamNum][teamSize] = chr->maxHP;
		battle->unitHealth[teamNum][teamSize] = chr->HP;
		teamSize++;

		if (teamSize >= 64)
			break;
	}
	battle->nUnits[teamNum] = teamSize;

	if (teamSize == 0) {
		Com_DPrintf(DEBUG_CLIENT, "Warning: Attempt to add soldiers to an auto-mission from an aircraft with no soldiers onboard.");
		Com_DPrintf(DEBUG_CLIENT, "--- Note: Aliens might win this mission by default because they are un-challenged, with no resistance!");
	}
}
