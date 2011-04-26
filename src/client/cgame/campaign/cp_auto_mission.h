/**
 * @file cp_auto_mission.h
 * @brief Header file for single player automatic (quick, simulated) missions,
 * without going to the battlescape.
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

#ifndef CP_AUTO_MISSION_H
#define CP_AUTO_MISSION_H

#include "../../client.h"
#include "cp_campaign.h"
#include "../../src/shared/mathlib_extra.h"

/** @brief Possilbe results from an auto mission to display on-screen after a simulated battle.
 * @note This list of potential results is NOT meant to replace missionResults_t, but is mainly used for displaying the proper message on the screen afterwards. */
typedef enum autoMission_results_s {
	AUTOMISSION_RESULT_SUCCESS,					/**< Victory! Aliens dead or otherwise neutralized */
	AUTOMISSION_RESULT_COSTLY_SUCCESS,			/**< Aliens dead, but so are most or all civilians. */
	AUTOMISSION_RESULT_FAILED_SOME_SURVIVORS,	/**< Defeat, but some soldiers managed to retreat and escape */
	AUTOMISSION_RESULT_FAILED_NO_SURVIVORS,		/**< Defeat, all soldiers/units dead, total loss */
	AUTOMISSION_RESULT_RESCUE_SUCCESSFUL,		/**< Special case for downed pilot rescue missions */
	AUTOMISSION_RESULT_RESCUE_FAILED,			/**< Soldiers survived and killed enemies, but pilot to be rescued did not survive */
	AUTOMISSION_RESULT_PLAYER_BASE_LOST,		/**< Base defence mission where the player's forces lost the battle */
	AUTOMISSION_RESULT_ALIEN_BASE_CAPTURED,		/**< An assault on an alien base was successful */

	AUTOMISSION_RESULT_MAX
} autoMission_results_t;

/* FIX ME: The hard-coded max teams of 8 and units on a team (64) should be changed to #define(ed) values set elsewhere! */
/** @brief Data structure for a simulated or auto mission.
 * @note Supports a hardcoded max of 8 teams that can be simulated in a battle, to
 * include any aliens, player soldiers, downed pilots, civilians, and any other forces.
 * The player's forces don't have to be any of the teams.  This is useful if a special
 * battle should be simulated for the plot, or if more than one alien threat is in the
 * game (not in the primary campaign plot though, but good for a MOD or whatever).
 * "Special Attribute" modifiers can be used for psi-abilities, cyber-implant stuff,
 * magic (only in a mod, thank you), or any other un-implemented future abilities of
 * a team.  If a team doesn't have such abilites, set it to 0.5f.
 * ALSO: A team does not have to attack (isHostile toward) another team that attacks it.
 * Teams that are isHostile toward no one will wander around like sheep, doing nothing else. */
typedef struct autoMission_battle_s {
	bool teamActive[8];						/**< Which teams exist in a battle, supports hardcoded MAX of 8 teams */
	int teamID[8];								/**< An ID for each team, to keep track, useful if needed.  Note: The same ID may be repeated, but two teams of same ID can't be hostile */
	bool isHostile[8][8];						/**< Friendly or hostile?  Params are [Each team] [Each other team] */
	short nUnits[8];							/**< Number of units (soldiers, aliens, UGVs, whatever) on each team, hardcoded MAX of 64 per team */

	float amScore_team_equipment[8];			/**< Number from 0.f to 1.f, represents how good a team's equipment is (higher is better) */
	float amScore_team_skill[8];				/**< Number from 0.f to 1.f, represents how good a team's abilities are (higher is better) */
	float amScore_team_difficulty[8];			/**< Number from 0.f to 1.f, represents a team's global fighting ability, difficulty, or misc. adjustments (higher is better) */
	float amScore_team_special_attribute_A[8];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */
	float amScore_team_special_attribute_B[8];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */
	float amScore_team_special_attribute_C[8];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */
	float amScore_team_special_attribute_D[8];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */

	bool am_use_special_attribute_factor[4];	/**< These decide if special team attributes should be used in calculations (TURE) or ignored and not even factored (FALSE) */

	int unit_health[8][64];						/**< Health score of each unit for each team */
	int unit_health_max[8][64];					/**< Max health of each unit on each team */

	short winning_team;						/**< Which team is victorious */
	bool has_been_fought;						/**< Did this battle run already?  Auto Battles can be fought only once, please. */
	bool is_rescue_mission;						/**< Is this a rescue or special mission? (Such as recovering a downed aircraft pilot) */
	short team_to_rescue;						/**< If a rescue mission, which team needs rescue? */
	short team_needing_rescue;					/**< If a rescue mission, which team is attempting the rescue? */
} autoMission_battle_t;

/* Functions for auto missions */
void CP_AutoBattle_Clear_Battle(autoMission_battle_t *battle);
void CP_AutoBattle_Run_Battle(autoMission_battle_t *battle);
void CP_AutoBattle_Fill_Team_From_Aircraft(autoMission_battle_t *battle, const short teamNum, const aircraft_t *aircraft);
void CP_AutoBattle_Fill_Team_From_AlienGroup(autoMission_battle_t *battle, const short teamNum, const alienTeamGroup_t *alienGroup);
void CP_AutoBattle_Create_Team_From_Scratch(autoMission_battle_t *battle, const short teamNum);
short CP_AutoBattle_Get_Winning_Team(const autoMission_battle_t *battle);

#endif
