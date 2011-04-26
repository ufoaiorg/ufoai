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

#include "../../cl_shared.h"

struct aircraft_s;
struct alienTeamGroup_s;

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
	qboolean teamActive[MAX_ACTIVETEAM];					/**< Which teams exist in a battle, supports hardcoded MAX of 8 teams */
	int teamID[MAX_ACTIVETEAM];								/**< An ID for each team, to keep track, useful if needed.  Note: The same ID may be repeated, but two teams of same ID can't be hostile */
	qboolean isHostile[MAX_ACTIVETEAM][MAX_ACTIVETEAM];		/**< Friendly or hostile?  Params are [Each team] [Each other team] */
	short nUnits[MAX_ACTIVETEAM];							/**< Number of units (soldiers, aliens, UGVs, whatever) on each team, hardcoded MAX of 64 per team */

	float scoreTeamEquipment[MAX_ACTIVETEAM];			/**< Number from 0.f to 1.f, represents how good a team's equipment is (higher is better) */
	float scoreTeamSkill[MAX_ACTIVETEAM];				/**< Number from 0.f to 1.f, represents how good a team's abilities are (higher is better) */
	float scoreTeamDifficulty[MAX_ACTIVETEAM];			/**< Number from 0.f to 1.f, represents a team's global fighting ability, difficulty, or misc. adjustments (higher is better) */
	float scoreTeamSpecialAttributeA[MAX_ACTIVETEAM];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */
	float scoreTeamSpecialAttributeB[MAX_ACTIVETEAM];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */
	float scoreTeamSpecialAttributeC[MAX_ACTIVETEAM];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */
	float scoreTeamSpecialAttributeD[MAX_ACTIVETEAM];	/**< UNUSED Special combat modifier, RESERVED FOR FUTURE USE */

	qboolean useSpecialAttributeFactor[4];	/**< These decide if special team attributes should be used in calculations (TURE) or ignored and not even factored (FALSE) */

	int unitHealth[MAX_ACTIVETEAM][64];		/**< Health score of each unit for each team */
	int unitHealthMax[MAX_ACTIVETEAM][64];	/**< Max health of each unit on each team */

	int winningTeam;							/**< Which team is victorious */
	qboolean hasBeenFought;						/**< Did this battle run already?  Auto Battles can be fought only once, please. */
	qboolean isRescueMission;						/**< Is this a rescue or special mission? (Such as recovering a downed aircraft pilot) */
	int teamToRescue;						/**< If a rescue mission, which team needs rescue? */
	int teamNeedingRescue;					/**< If a rescue mission, which team is attempting the rescue? */
} autoMissionBattle_t;

/* Functions for auto missions */
void CP_AutoBattleClearBattle(autoMissionBattle_t *battle);
void CP_AutoBattleRunBattle(autoMissionBattle_t *battle);
void CP_AutoBattleFillTeamFromAircraft(autoMissionBattle_t *battle, const int teamNum, const struct aircraft_s *aircraft);
void CP_AutoBattleFillTeamFromAircraftUGVs(autoMissionBattle_t *battle, const int teamNum, const struct aircraft_s *aircraft);
void CP_AutoBattleFillTeamFromPlayerBase(autoMissionBattle_t *battle, const int teamNum, const int baseNum);
void CP_AutoBattleFillTeamFromAlienGroup(autoMissionBattle_t *battle, const int teamNum, const struct alienTeamGroup_s *alienGroup);
void CP_AutoBattleCreateTeamFromScratch(autoMissionBattle_t *battle, const int teamNum);
int CP_AutoBattleGetWinningTeam(const autoMissionBattle_t *battle);

#endif
