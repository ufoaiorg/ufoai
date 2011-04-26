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
	int x = 0;
	int y = 0;
	for (x = 0; x < 8; x++) {
		battle->teamID[x] = -1;
		battle->teamActive[x] = qfalse;
		battle->nUnits[x] = 0;
		battle->scoreTeamDifficulty[x] = 0.5f;
		battle->scoreTeamEquipment[x] = 0.5f;
		battle->scoreTeamSkill[x] = 0.5f;
		battle->scoreTeamSpecialAttributeA[x] = 0.5f;
		battle->scoreTeamSpecialAttributeB[x] = 0.5f;
		battle->scoreTeamSpecialAttributeC[x] = 0.5f;
		battle->scoreTeamSpecialAttributeD[x] = 0.5f;

		for (y = 0; y < 8; y++) {
			battle->isHostile[x][y] = qtrue; /* If you forget to set this and run a battle, then f*** it, everyone will just kill each other by default! */
		}

		for (y = 0; y < 64; y++) {
			battle->unitHealth[x][y] = 0;
			battle->unitHealthMax[x][y] = 0;
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
 * find out which of all the employees are on the aircraft (in the mission) */
void CPAutoBattleFillTeamFromAircraft (autoMissionBattle_t *battle, const short teamNum, const aircraft_t *aircraft) {
		assert (battle);
		assert (aircraft);

		int x = AIR_GetTeamSize (aircraft);
		if (x == 0) {
			Com_DPrintf (DEBUG_CLIENT, "Warning: Attempt to add soldiers to an auto-mission from an aircraft with no soldiers onboard.");
			Com_DPrintf (DEBUG_CLIENT, "--- Note: Aliens might win this mission by default because they are un-challenged, with no resistance!");
			return;
		}
		battle->nUnits[teamNum] = x;

		/* Fill Health scores in battle with scores from soldiers on the aircraft */
		employee_t *employee;
		int y = 0;
		for (y = 0; y < x; y++) {
			employee = ccs.employees[aircraft->acTeam[y]];
			const character_t *chr = &employee->chr;
			battle->unitHealthMax[teamNum][y] = chr->maxHP;
			battle->unitHealth[teamNum][y] = chr->HP;
		}
}
