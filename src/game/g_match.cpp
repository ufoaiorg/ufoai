/**
 * @file
 * @brief Match related functions
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "g_match.h"
#include "g_local.h"
#include "g_actor.h"
#include "g_ai.h"
#include "g_edicts.h"
#include "g_trigger.h"
#include "g_utils.h"
#include "g_vis.h"

/**
 * @brief Determines the amount of XP earned by a given soldier for a given skill, based on the soldier's performance in the last mission.
 * @param[in] skill The skill for which to fetch the maximum amount of XP.
 * @param[in] ent Pointer to the character you want to get the earned experience for
 * @sa G_UpdateCharacterExperience
 * @sa G_GetMaxExperiencePerMission
 */
static int G_GetEarnedExperience (abilityskills_t skill, Edict* ent)
{
	character_t* chr = &ent->chr;

	int experience = 0;

	switch (skill) {
	case ABILITY_POWER: {
		const float weight = chr->scoreMission->carriedWeight / WEIGHT_FACTOR / level.actualRound;
		const float penalty = GET_ENCUMBRANCE_PENALTY(weight, chr->score.skills[ABILITY_POWER]);
		experience = 50 * (weight / chr->score.skills[ABILITY_POWER]) / penalty;
		break;
	}
	case ABILITY_ACCURACY:
		/* skip skills < ABILITY_NUM_TYPES, they are abilities not real skills */
		for (int i = ABILITY_NUM_TYPES; i < SKILL_NUM_TYPES; i++)
			if (i == SKILL_SNIPER)
				experience += 60 * (chr->scoreMission->hits[i][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[i][KILLED_ENEMIES]);
			else
				experience += 40 * (chr->scoreMission->hits[i][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[i][KILLED_ENEMIES]);
		break;
	case ABILITY_MIND:
		experience = 50 + 100 * chr->scoreMission->kills[KILLED_ENEMIES];
		break;
	case SKILL_CLOSE:
		experience = 180 * (chr->scoreMission->hits[skill][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[skill][KILLED_ENEMIES]);
		break;
#if 0
	case SKILL_HEAVY:
		experience = 180 * (chr->scoreMission->hits[skill][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[skill][KILLED_ENEMIES]);
		break;
#endif
	case SKILL_ASSAULT:
		experience = 180 * (chr->scoreMission->hits[skill][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[skill][KILLED_ENEMIES]);
		break;
	case SKILL_SNIPER:
		experience = 180 * (chr->scoreMission->hits[skill][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[skill][KILLED_ENEMIES]);
		break;
	case SKILL_EXPLOSIVE:
		experience = 180 * (chr->scoreMission->hits[skill][KILLED_ENEMIES] + chr->scoreMission->hitsSplash[skill][KILLED_ENEMIES]);
		break;
	default:
		break;
	}

	return experience;
}

/**
 * @brief Updates character experience after a mission.
 * @param[in,out] ent Pointer to the character that should get the experience updated
 * @sa CP_UpdateCharacterStats
 * @sa G_UpdateCharacterScore
 * @sa G_UpdateHitScore
 */
static void G_UpdateCharacterExperience (Edict* ent)
{
	character_t* chr = &ent->chr;

	/* Robots/UGVs do not get skill-upgrades. */
	if (chr->teamDef->robot)
		return;

	unsigned int totalGainedXP = 0;
	for (int i = 0; i < SKILL_NUM_TYPES; i++) {
		const abilityskills_t skill = static_cast<abilityskills_t>(i);
		const int gainedXP = G_GetEarnedExperience(skill, ent);

		chr->score.experience[i] += gainedXP;
		totalGainedXP += gainedXP;
	}

	/* Speed and health are handled separately */
	chr->score.experience[ABILITY_SPEED] += totalGainedXP / 5;
	/* This is health */
	chr->score.experience[SKILL_NUM_TYPES] += totalGainedXP / 5;
}

/**
 * @brief Triggers the end of the game. Will be executed in the next server (or game) frame.
 * @param[in] team The winning team
 * @param[in] timeGap Second to wait before really ending the game. Useful if you want to allow a last view on the scene
 */
void G_MatchEndTrigger (int team, int timeGap)
{
	bool foundNextMap = false;
	Edict* ent = nullptr;

	while ((ent = G_EdictsGetTriggerNextMaps(ent)) != nullptr) {
		if (ent->getTeam() == team) {
			ent->think = Think_NextMapTrigger;
			ent->nextthink = 1;
			foundNextMap = true;
		}
	}

	if (!foundNextMap) {
		const int realTimeGap = timeGap > 0 ? level.time + timeGap : 1;
		level.winningTeam = team;
		level.intermissionTime = realTimeGap;
	}
}

/**
 * @brief Sends character stats like assigned missions and kills back to client
 * @param[in] actor The edict to send the data for
 * @note first short is the ucn to allow the client to identify the character
 * @note parsed in GAME_CP_Results
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @note you also have to update the pascal string size in G_MatchSendResults if you change the buffer here
 */
static void G_SendCharacterData (const Actor* actor)
{
	assert(actor);

	/* write character number */
	gi.WriteShort(actor->chr.ucn);

	gi.WriteShort(actor->HP);
	gi.WriteByte(actor->getStun());
	gi.WriteByte(actor->morale);

	for (int k = 0; k < BODYPART_MAXTYPE; ++k)
		gi.WriteByte(actor->chr.wounds.woundLevel[k] + actor->chr.wounds.treatmentLevel[k]);

	/** Scores @sa inv_shared.h:chrScoreGlobal_t */
	for (int k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.WriteLong(actor->chr.score.experience[k]);
	for (int k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(actor->chr.score.kills[k]);
	for (int k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(actor->chr.score.stuns[k]);
	gi.WriteShort(actor->chr.score.assignedMissions);
}

/**
 * @brief Handles the end of a match
 * @param[in] team The winning team number
 * @param[in] nextmap Is there a follow-up map within the same match ?
 * @sa G_RunFrame
 * @sa CL_ParseResults
 */
static void G_MatchSendResults (int team, bool nextmap)
{
	Edict* attacker = nullptr;
	Actor* actor = nullptr;
	/* Calculate new scores/skills for the soldiers. */
	while ((actor = G_EdictsGetNextLivingActor(actor))) {
		if (!G_IsAI(actor))
			G_UpdateCharacterExperience(actor);
		else if (actor->getTeam() == team)
			attacker = actor;
	}

	/* if aliens won, make sure every soldier that is not in the rescue zone dies */
	if (team == TEAM_ALIEN) {
		actor = nullptr;
		while ((actor = G_EdictsGetNextLivingActor(actor)))
			if (actor->getTeam() != team && !actor->isInRescueZone()) {
				actor->HP = 0;
				G_ActorDieOrStun(actor, attacker);
			}
	}

	G_VisMakeEverythingVisible();

	/* send results */
	G_EventAdd(PM_ALL, EV_RESULTS, -1);
	gi.WriteByte(MAX_TEAMS);
	gi.WriteByte(team);
	gi.WriteByte(nextmap);

	for (int i = 0; i < MAX_TEAMS; i++) {
		gi.WriteByte(level.num_spawned[i]);
		gi.WriteByte(level.num_alive[i]);
	}

	for (int i = 0; i <= MAX_TEAMS; i++)
		for (int j = 0; j < MAX_TEAMS; j++)
			gi.WriteByte(level.num_kills[i][j]);

	for (int i = 0; i <= MAX_TEAMS; i++)
		for (int j = 0; j < MAX_TEAMS; j++)
			gi.WriteByte(level.num_stuns[i][j]);

	/* how many actors */
	int n  = 0;
	actor = nullptr;
	while ((actor = G_EdictsGetNextActor(actor)))
		if (!G_IsAI(actor))
			n++;

	/* number of soldiers */
	gi.WriteByte(n);

	if (n) {
		actor = nullptr;
		while ((actor = G_EdictsGetNextActor(actor))) {
			if (!G_IsAI(actor)) {
				G_SendCharacterData(actor);
			}
		}
	}

	G_EventEnd();
}

/**
 * @brief Checks whether a match is over.
 * @return @c true if this match is over, @c false otherwise
 */
bool G_MatchDoEnd (void)
{
	/* check for intermission */
	if (level.intermissionTime > 0.0 && level.time > level.intermissionTime) {
		G_PrintStats("End of game - Team %i is the winner", level.winningTeam);
		G_MatchSendResults(level.winningTeam, level.nextMapSwitch);

		/* now we cleanup the AI */
		AIL_Cleanup();

		if (level.mapEndCommand != nullptr) {
			gi.AddCommandString("%s\n", level.mapEndCommand);
		}

		level.intermissionTime = 0.0;
		level.winningTeam = 0;
		return true;
	}

	return false;
}

/**
 * @brief Checks whether there are still actors to fight with left. If none
 * are the match end will be triggered.
 * @sa G_MatchEndTrigger
 */
void G_MatchEndCheck (void)
{
	if (level.intermissionTime > 0.0) /* already decided */
		return;

	if (!level.numplayers) {
		G_MatchEndTrigger(0, 0);
		return;
	}

	int last = 0;
	int activeTeams = 0;
	/** @todo count from 0 to get the civilians for objectives */
	for (int i = 1; i < MAX_TEAMS; i++) {
		Actor* actor = nullptr;
		/* search for living but not stunned actors - there must at least be one actor
		 * that is still able to attack or defend himself */
		while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, i)) != nullptr) {
			if (!actor->isStunned()) {
				last = i;
				activeTeams++;
				break;
			}
		}
	}

	/** @todo < 2 does not work when we count civilians */
	/* prepare for sending results */
	if (activeTeams < 2) {
		const int timeGap = (level.activeTeam == TEAM_ALIEN ? 10.0 : 3.0);
		G_MatchEndTrigger(activeTeams == 1 ? last : 0, timeGap);
	}
}

/**
 * @brief Checks whether the game is running (active team and no intermission time)
 * @return @c true if there is an active team for the current turn and the end of the game
 * was not yet triggered
 * @sa G_MatchEndTrigger
 */
bool G_MatchIsRunning (void)
{
	if (level.intermissionTime > 0.0)
		return false;
	return (level.activeTeam != TEAM_NO_ACTIVE);
}
