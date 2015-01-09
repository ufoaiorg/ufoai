/**
 * @file
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "g_local.h"
#include "g_actor.h"
#include "g_ai.h"
#include "g_client.h"
#include "g_edicts.h"
#include "g_health.h"
#include "g_match.h"
#include "g_reaction.h"
#include "g_utils.h"
#include "g_vis.h"

/**
 * @brief Check whether a forced turn end should be executed
 */
void G_CheckForceEndRound (void)
{
	/* check for roundlimits in multiplayer only */
	if (!sv_roundtimelimit->integer || G_IsSinglePlayer())
		return;

	if (!G_MatchIsRunning())
		return;

	if (level.time != ceil(level.time))
		return;

	const int diff = level.roundstartTime + sv_roundtimelimit->integer - level.time;
	switch (diff) {
	case 240:
		gi.BroadcastPrintf(PRINT_HUD, _("4 minutes left until forced turn end."));
		return;
	case 180:
		gi.BroadcastPrintf(PRINT_HUD, _("3 minutes left until forced turn end."));
		return;
	case 120:
		gi.BroadcastPrintf(PRINT_HUD, _("2 minutes left until forced turn end."));
		return;
	case 60:
		gi.BroadcastPrintf(PRINT_HUD, _("1 minute left until forced turn end."));
		return;
	case 30:
		gi.BroadcastPrintf(PRINT_HUD, _("30 seconds left until forced turn end."));
		return;
	case 15:
		gi.BroadcastPrintf(PRINT_HUD, _("15 seconds left until forced turn end."));
		return;
	}

	/* active team still has time left */
	if (level.time < level.roundstartTime + sv_roundtimelimit->integer)
		return;

	gi.BroadcastPrintf(PRINT_HUD, _("Current active team hit the max round time."));

	/* store this in a local variable, as the global variable is changed in G_ClientEndRound */
	const int activeTeam = level.activeTeam;
	/* set all team members to ready (only human players) */
	Player* p = nullptr;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (p->getTeam() == activeTeam) {
			G_ClientEndRound(*p);
			level.nextEndRound = level.framenum;
		}
	}

	level.roundstartTime = level.time;
}

/**
 * @brief Counts the still living actors for a player
 */
static int G_PlayerSoldiersCount (const Player& player)
{
	int cnt = 0;
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextLivingActor(actor))) {
		if (actor->getPlayerNum() == player.getNum())
			cnt++;
	}

	return cnt;
}

/**
 * @brief Regenerate the "STUN" value of each (partly) stunned team member.
 * @note The values are @b not sent via network. This is done in
 * @c G_GiveTimeUnits. It @b has to be called afterwards.
 * @param[in] team The index of the team to update the values for.
 * @sa G_GiveTimeUnits
 * @todo Make this depend on the character-attributes.
 */
static void G_UpdateStunState (int team)
{
	const int regen = 1;

	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, team))) {
		const int stun = actor->getStun();
		if (stun > 0) {
			if (regen > stun)
				actor->setStun(0);
			else
				actor->addStun(-regen);

			G_ActorCheckRevitalise(actor);
		}
	}
}

/**
 * @brief Updates the weight carried for each team member.
 * @param[in] team The index of the team to update the values for.
 */
static void G_UpdateCarriedWeight (int team)
{
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, team))) {
		if (actor->chr.scoreMission) {
			actor->chr.scoreMission->carriedWeight += actor->chr.inv.getWeight();
		}
	}
}

static void G_RoundTouchTriggers (int team)
{
	Actor* actor = nullptr;

	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, team))) {
		G_TouchTriggers(actor);
	}
}

/**
 * @brief Get the next active team
 */
static void G_GetNextActiveTeam (void)
{
	const int lastTeam = G_GetActiveTeam();

	Com_DPrintf(DEBUG_GAME, "round end from team %i\n", lastTeam);
	level.activeTeam = TEAM_NO_ACTIVE;

	/* search next team */
	for (int i = 1; i < MAX_TEAMS; i++) {
		const int team = (lastTeam + i) % MAX_TEAMS;
		if (level.num_alive[team]) {
			/* found next player */
			level.activeTeam = team;
			Com_DPrintf(DEBUG_GAME, "round start for team %i\n", team);
			break;
		}
	}
}

/**
 * @sa G_PlayerSoldiersCount
 */
void G_ClientEndRound (Player& player)
{
	const int lastTeamIndex = (G_GetActiveTeam() + level.teamOfs) % MAX_TEAMS;

	if (!G_IsAIPlayer(&player)) {
		/* inactive players can't end their inactive turn :) */
		if (level.activeTeam != player.getTeam())
			return;

		/* check for "team oszillation" */
		if (level.framenum < level.nextEndRound)
			return;

		level.nextEndRound = level.framenum + 20;
	}

	/* only use this for teamplay matches like coopX or fight2on2 and above
	 * also skip this for ai players, this is only called when all ai actors
	 * have finished their 'thinking' */
	if (!G_IsAIPlayer(&player) && sv_teamplay->integer) {
		/* check if all team members are ready */
		if (!player.roundDone) {
			player.roundDone = true;
			G_EventEndRoundAnnounce(player);
			G_EventEnd();
		}
		Player* p = nullptr;
		while ((p = G_PlayerGetNextActiveHuman(p)))
			if (p->getTeam() == level.activeTeam && !p->roundDone && G_PlayerSoldiersCount(*p) > 0)
				return;
		p = nullptr;
		while ((p = G_PlayerGetNextActiveAI(p)))
			if (p->getTeam() == level.activeTeam && !p->roundDone && G_PlayerSoldiersCount(*p) > 0)
				return;
	} else {
		player.roundDone = true;
	}

	/* clear any remaining reaction fire */
	G_ReactionFireOnEndTurn();

	if (!G_IsAIPlayer(&player)) {
		if (g_lastseen->integer > 0) {
			Actor* actor = nullptr;
			while ((actor = G_EdictsGetNextActor(actor))) {
				if (G_IsAI(actor) && G_IsVisibleForTeam(actor, level.activeTeam)) {
					player.lastSeen = level.actualRound;
					break;
				}
			}
			if (level.actualRound - player.lastSeen > g_lastseen->integer) {
				Com_Printf("round end triggered by g_lastseen (player %i (team %i) last seen in round %i of %i rounds)\n",
						player.getNum(), level.activeTeam, player.lastSeen, level.actualRound);
				G_MatchEndTrigger(-1, 0);
			}
		}
	}

	/* let all the invisible players perish now */
	G_CheckVisTeamAll(level.activeTeam, VIS_APPEAR, nullptr);

	G_GetNextActiveTeam();

	AI_CheckRespawn(TEAM_ALIEN);

	/* no other team left? */
	if (!G_MatchIsRunning())
		return;

	if (lastTeamIndex > (level.activeTeam + level.teamOfs) % MAX_TEAMS)
		level.actualRound++;

	/* communicate next player in row to clients */
	G_EventEndRound();

	/* store the round start time to be able to abort the round after a give time */
	level.roundstartTime = level.time;

	/* Wounded team members bleed */
	G_BleedWounds(level.activeTeam);

	G_RoundTouchTriggers(level.activeTeam);

	/* Update the state of stuned team-members. The actual statistics are sent below! */
	G_UpdateStunState(level.activeTeam);

	/* Give the actors of the now active team their TUs. */
	G_GiveTimeUnits(level.activeTeam);

	/* apply morale behaviour, reset reaction fire */
	G_ReactionFireReset(level.activeTeam);
	if (mor_panic->integer)
		G_MoraleBehaviour(level.activeTeam);

	G_UpdateCarriedWeight(level.activeTeam);

	/* start ai - there is only one player for ai teams, and the last pointer must only
	 * be updated for ai players */
	Player* p = G_GetPlayerForTeam(level.activeTeam);
	if (p == nullptr)
		gi.Error("Could not find player for team %i", level.activeTeam);

	/* finish off events */
	G_EventEnd();

	/* reset ready flag for every player on the current team (even ai players) */
	p = nullptr;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (p->getTeam() == level.activeTeam) {
			p->roundDone = false;
		}
	}

	p = nullptr;
	while ((p = G_PlayerGetNextActiveAI(p))) {
		if (p->getTeam() == level.activeTeam) {
			p->roundDone = false;
		}
	}
}
