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

/** @brief Clears, initializes, or resets a single auto mission, sets default values.
 * @param[in,out] battle The battle that should be init to defaults */
void CP_AutoBattle_Clear_Battle (autoMission_battle_t *battle) {
	int x = 0;
	int y = 0;
	for(x = 0; x < 8; x++) {
		battle->teamID[x] = -1;
		battle->teamActive[x] = false;
		battle->nUnits[x] = 0;
		battle->amScore_team_difficulty[x] = 0.5f;
		battle->amScore_team_equipment[x] = 0.5f;
		battle->amScore_team_skill[x] = 0.5f;
		battle->amScore_team_special_attribute_A[x] = 0.5f;
		battle->amScore_team_special_attribute_B[x] = 0.5f;
		battle->amScore_team_special_attribute_C[x] = 0.5f;
		battle->amScore_team_special_attribute_D[x] = 0.5f;

		for(y = 0; y < 8; y++) {
			battle->isHostile[x][y] = true; /* If you forget to set this and run a battle, then f*** it, everyone will just kill each other by default! */
		}

		for(y = 0; y < 64; y++) {
			battle->unit_health[x][y] = 0;
			battle->unit_health_max[x][y] = 0;
		}
	}
	battle->am_use_special_attribute_factor[0] = false;
	battle->am_use_special_attribute_factor[1] = false;
	battle->am_use_special_attribute_factor[2] = false;
	battle->am_use_special_attribute_factor[3] = false;

	battle->winning_team = -1;
	battle->is_rescue_mission = false;
	battle->team_to_rescue = -1;
	battle->team_needing_rescue = -1;
	battle->has_been_fought = false;
}
