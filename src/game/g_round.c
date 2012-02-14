/**
 * @file g_round.c
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

#include "g_local.h"

/**
 * @brief Check whether a forced turn end should be executed
 */
void G_CheckForceEndRound (void)
{
	player_t *p;
	int diff;
	int activeTeam;

	/* check for roundlimits in multiplayer only */
	if (!sv_roundtimelimit->integer || sv_maxclients->integer == 1)
		return;

	if (!G_MatchIsRunning())
		return;

	if (level.time != ceil(level.time))
		return;

	diff = level.roundstartTime + sv_roundtimelimit->integer - level.time;
	switch (diff) {
	case 240:
		gi.BroadcastPrintf(PRINT_HUD, _("4 minutes left until forced turn end.\n"));
		return;
	case 180:
		gi.BroadcastPrintf(PRINT_HUD, _("3 minutes left until forced turn end.\n"));
		return;
	case 120:
		gi.BroadcastPrintf(PRINT_HUD, _("2 minutes left until forced turn end.\n"));
		return;
	case 60:
		gi.BroadcastPrintf(PRINT_HUD, _("1 minute left until forced turn end.\n"));
		return;
	case 30:
		gi.BroadcastPrintf(PRINT_HUD, _("30 seconds left until forced turn end.\n"));
		return;
	case 15:
		gi.BroadcastPrintf(PRINT_HUD, _("15 seconds left until forced turn end.\n"));
		return;
	}

	/* active team still has time left */
	if (level.time < level.roundstartTime + sv_roundtimelimit->integer)
		return;

	gi.BroadcastPrintf(PRINT_HUD, _("Current active team hit the max round time\n"));

	/* store this in a local variable, as the global variable is changed in G_ClientEndRound */
	activeTeam = level.activeTeam;
	/* set all team members to ready (only human players) */
	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (p->pers.team == activeTeam) {
			G_ClientEndRound(p);
			level.nextEndRound = level.framenum;
		}
	}

	level.roundstartTime = level.time;
}

/**
 * @brief Counts the still living actors for a player
 */
static int G_PlayerSoldiersCount (const player_t* player)
{
	int cnt = 0;
	edict_t *ent = NULL;

	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		if (ent->pnum == player->num)
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
 * @todo Make this depend on the character-attributes. http://lists.cifrid.net/pipermail/ufoai/2008-February/000346.html
 */
static void G_UpdateStunState (int team)
{
	edict_t *ent = NULL;
	const int regen = 1;

	while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, team))) {
		if (ent->STUN > 0) {
			if (regen > ent->STUN)
				ent->STUN = 0;
			else
				ent->STUN -= regen;

			G_ActorCheckRevitalise(ent);
		}
	}
}

/**
 * @brief Get the next active team
 */
static void G_GetNextActiveTeam (void)
{
	int i;
	const int lastTeam = G_GetActiveTeam();

	level.activeTeam = TEAM_NO_ACTIVE;

	/* search next team */
	for (i = 1; i < MAX_TEAMS; i++) {
		const int team = (lastTeam + i) % MAX_TEAMS;
		if (level.num_alive[team]) {
			/* found next player */
			level.activeTeam = team;
			break;
		}
	}
}

/**
 * @sa G_PlayerSoldiersCount
 */
void G_ClientEndRound (player_t * player)
{
	player_t *p;

	if (!G_IsAIPlayer(player)) {
		/* inactive players can't end their inactive turn :) */
		if (level.activeTeam != player->pers.team)
			return;

		/* check for "team oszillation" */
		if (level.framenum < level.nextEndRound)
			return;

		level.nextEndRound = level.framenum + 20;
	}

	/* only use this for teamplay matches like coopX or 2on2 and above
	 * also skip this for ai players, this is only called when all ai actors
	 * have finished their 'thinking' */
	if (!G_IsAIPlayer(player) && sv_teamplay->integer) {
		/* check if all team members are ready */
		if (!player->roundDone) {
			player->roundDone = qtrue;
			G_EventEndRoundAnnounce(player);
			gi.EndEvents();
		}
		p = NULL;
		while ((p = G_PlayerGetNextActiveHuman(p)))
			if (p->pers.team == level.activeTeam && !p->roundDone && G_PlayerSoldiersCount(p) > 0)
				return;
		p = NULL;
		while ((p = G_PlayerGetNextActiveAI(p)))
			if (p->pers.team == level.activeTeam && !p->roundDone && G_PlayerSoldiersCount(p) > 0)
				return;
	} else {
		player->roundDone = qtrue;
	}

	/* clear any remaining reaction fire */
	G_ReactionFireOnEndTurn();

	/* let all the invisible players perish now */
	G_CheckVisTeamAll(level.activeTeam, qtrue, NULL);

	G_GetNextActiveTeam();

	AI_CheckRespawn(TEAM_ALIEN);

	/* no other team left? */
	if (!G_MatchIsRunning())
		return;

	level.actualRound++;

	/* communicate next player in row to clients */
	G_EventEndRound();

	/* store the round start time to be able to abort the round after a give time */
	level.roundstartTime = level.time;

	/* Update the state of stuned team-members. The actual statistics are sent below! */
	G_UpdateStunState(level.activeTeam);

	/* Give the actors of the now active team their TUs. */
	G_GiveTimeUnits(level.activeTeam);

	/* apply morale behaviour, reset reaction fire */
	G_ReactionFireReset(level.activeTeam);
	if (mor_panic->integer)
		G_MoraleBehaviour(level.activeTeam);

	/* start ai - there is only one player for ai teams, and the last pointer must only
	 * be updated for ai players */
	p = G_GetPlayerForTeam(level.activeTeam);
	if (p == NULL)
		gi.Error("Could not find player for team %i", level.activeTeam);

	/* finish off events */
	gi.EndEvents();

	/* reset ready flag for every player on the current team (even ai players) */
	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p)))
		if (p->pers.team == level.activeTeam)
			p->roundDone = qfalse;

	p = NULL;
	while ((p = G_PlayerGetNextActiveAI(p)))
		if (p->pers.team == level.activeTeam)
			p->roundDone = qfalse;
}
