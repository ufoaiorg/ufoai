/**
 * @file
 * @brief Player commands.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/game/g_cmds.c
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "g_client.h"
#include "g_edicts.h"
#include "g_match.h"
#include "../shared/parse.h"

static void G_Players_f (const Player& player)
{
	int count = 0;
	char smallBuf[64];
	char largeBuf[1280];

	/* print information */
	largeBuf[0] = 0;

	Player* p = nullptr;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		Com_sprintf(smallBuf, sizeof(smallBuf), "(%i) Team %i %s status: %s\n", p->getNum(),
				p->getTeam(), p->pers.netname, (p->roundDone ? "waiting" : "playing"));

		/* can't print all of them in one packet */
		if (strlen(smallBuf) + strlen(largeBuf) > sizeof(largeBuf) - 100) {
			Q_strcat(largeBuf, sizeof(largeBuf), "...\n");
			break;
		}
		Q_strcat(largeBuf, sizeof(largeBuf), "%s", smallBuf);
		count++;
	}

	G_ClientPrintf(player, PRINT_CONSOLE, "%s\n%i players\n", largeBuf, count);
}

/**
 * @brief Check whether the user can talk
 */
static bool G_CheckFlood (Player& player)
{
	if (flood_msgs->integer) {
		if (level.time < player.pers.flood_locktill) {
			G_ClientPrintf(player, PRINT_CHAT, _("You can't talk for %d more seconds\n"), (int)(player.pers.flood_locktill - level.time));
			return true;
		}
		int i = player.pers.flood_whenhead - flood_msgs->value + 1;
		if (i < 0)
			i = (sizeof(player.pers.flood_when)/sizeof(player.pers.flood_when[0])) + i;
		if (player.pers.flood_when[i] && level.time - player.pers.flood_when[i] < flood_persecond->value) {
			player.pers.flood_locktill = level.time + flood_waitdelay->value;
			G_ClientPrintf(player, PRINT_CHAT, _("Flood protection: You can't talk for %d seconds.\n"), flood_waitdelay->integer);
			return true;
		}
		player.pers.flood_whenhead = (player.pers.flood_whenhead + 1) %
				(sizeof(player.pers.flood_when)/sizeof(player.pers.flood_when[0]));
		player.pers.flood_when[player.pers.flood_whenhead] = level.time;
	}
	return false;
}

static void G_Say_f (Player& player, bool arg0, bool team)
{
	if (gi.Cmd_Argc() < 2 && !arg0)
		return;

	if (G_CheckFlood(player))
		return;

	char text[256];
	if (arg0) {
		Com_sprintf(text, sizeof(text), "%s %s", gi.Cmd_Argv(0), gi.Cmd_Args());
	} else {
		Com_sprintf(text, sizeof(text), "%s", gi.Cmd_Args());
	}

	/* strip quotes */
	char* s = text;
	if (s[0] == '"' && s[strlen(s) - 1] == '"') {
		s[strlen(s) - 1] = '\0';
		s++;
	}

	if (sv_dedicated->integer) {
		if (!team)
			gi.DPrintf("%s: %s\n", player.pers.netname, s);
		else
			gi.DPrintf("^B%s (team): %s\n", player.pers.netname, s);
	}

	Player* p = nullptr;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (team && p->getTeam() != player.getTeam())
			continue;
		if (!team)
			G_ClientPrintf(*p, PRINT_CHAT, "%s: %s\n", player.pers.netname, s);
		else
			G_ClientPrintf(*p, PRINT_CHAT, "^B%s (team): %s\n", player.pers.netname, s);
	}
}

#ifdef DEBUG
/**
 * @brief This function does not add statistical values. Because there is no attacker.
 * The same counts for morale states - they are not affected.
 * @note: This is a debug function to let a whole team die
 */
static void G_KillTeam_f (void)
{
	/* default is to kill all teams */
	int teamToKill = -1;
	int amount = -1;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() >= 2) {
		teamToKill = atoi(gi.Cmd_Argv(1));
		if (gi.Cmd_Argc() >= 3)
			amount = atoi(gi.Cmd_Argv(2));
	}

	Com_DPrintf(DEBUG_GAME, "G_KillTeam: kill team %i\n", teamToKill);

	if (teamToKill >= 0) {
		Actor* actor = nullptr;
		while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, teamToKill))) {
			if (amount == 0)
				break;
			/* die */
			actor->HP = 0;
			G_ActorDieOrStun(actor, nullptr);

			if (teamToKill == TEAM_ALIEN)
				level.num_kills[TEAM_PHALANX][TEAM_ALIEN]++;
			else
				level.num_kills[TEAM_ALIEN][teamToKill]++;
			amount--;
		}
	}

	/* check for win conditions */
	G_MatchEndCheck();
}

/**
 * @brief Stun all members of a given team.
 */
static void G_StunTeam_f (void)
{
	/* default is to kill all teams */
	int teamToKill = -1;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() >= 2)
		teamToKill = atoi(gi.Cmd_Argv(1));

	if (teamToKill >= 0) {
		Actor* actor = nullptr;
		while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, teamToKill))) {
			/* stun */
			G_ActorDieOrStun(actor, nullptr);

			if (teamToKill == TEAM_ALIEN)
				level.num_stuns[TEAM_PHALANX][TEAM_ALIEN]++;
			else
				level.num_stuns[TEAM_ALIEN][teamToKill]++;
		}
	}

	/* check for win conditions */
	G_MatchEndCheck();
}

/**
 * @brief Prints all mission-score entries of all team members.
 * @note Console command: debug_listscore
 */
static void G_ListMissionScore_f (void)
{
	int team = -1;

	/* With a parameter we will be able to get the info for a specific team */
	if (gi.Cmd_Argc() >= 2) {
		team = atoi(gi.Cmd_Argv(1));
	} else {
		gi.DPrintf("Usage: %s <teamnumber>\n", gi.Cmd_Argv(0));
		return;
	}

	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActor(actor))) {
		if (team >= 0 && actor->getTeam() != team)
			continue;

		assert(actor->chr.scoreMission);

		gi.DPrintf("Soldier: %s\n", actor->chr.name);

		/* ===================== */
		gi.DPrintf("  Move: Normal=%i Crouched=%i\n", actor->chr.scoreMission->movedNormal, actor->chr.scoreMission->movedCrouched);

		gi.DPrintf("  Kills:");
		for (int i = 0; i < KILLED_NUM_TYPES; i++) {
			gi.DPrintf(" %i", actor->chr.scoreMission->kills[i]);
		}
		gi.DPrintf("\n");

		gi.DPrintf("  Stuns:");
		for (int i = 0; i < KILLED_NUM_TYPES; i++) {
			gi.DPrintf(" %i", actor->chr.scoreMission->stuns[i]);
		}
		gi.DPrintf("\n");

		/* ===================== */
		gi.DPrintf("  Fired:");
		for (int i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf(" %i", actor->chr.scoreMission->fired[i]);
		}
		gi.DPrintf("\n");

		gi.DPrintf("  Hits:\n");
		for (int i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf("    Skill%i: ",i);
			for (int j = 0; j < KILLED_NUM_TYPES; j++) {
				gi.DPrintf(" %i", actor->chr.scoreMission->hits[i][j]);
			}
			gi.DPrintf("\n");
		}

		/* ===================== */
		gi.DPrintf("  Fired Splash:");
		for (int i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf(" %i", actor->chr.scoreMission->firedSplash[i]);
		}
		gi.DPrintf("\n");

		gi.DPrintf("  Hits Splash:\n");
		for (int i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf("    Skill%i: ",i);
			for (int j = 0; j < KILLED_NUM_TYPES; j++) {
				gi.DPrintf(" %i", actor->chr.scoreMission->hitsSplash[i][j]);
			}
			gi.DPrintf("\n");
		}

		gi.DPrintf("  Splash Damage:\n");
		for (int i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf("    Skill%i: ",i);
			for (int j = 0; j < KILLED_NUM_TYPES; j++) {
				gi.DPrintf(" %i", actor->chr.scoreMission->hitsSplashDamage[i][j]);
			}
			gi.DPrintf("\n");
		}

		/* ===================== */
		gi.DPrintf("  Kills per skill:");
		for (int i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf(" %i", actor->chr.scoreMission->skillKills[i]);
		}
		gi.DPrintf("\n");

		/* ===================== */
		gi.DPrintf("  Heal (received): %i\n", actor->chr.scoreMission->heal);
	}
}

/**
 * @brief Debug function to print a player's inventory
 */
void G_InvList_f (const Player& player)
{
	gi.DPrintf("Print inventory for '%s'\n", player.pers.netname);

	Actor* actor = nullptr;
	while ((actor = G_EdictsGetNextLivingActorOfTeam(actor, player.getTeam()))) {
		gi.DPrintf("actor: '%s'\n", actor->chr.name);

		const Container* cont = nullptr;
		while ((cont = actor->chr.inv.getNextCont(cont, true))) {
			Com_Printf("Container: %i\n", cont->id);
			Item* item = nullptr;
			while ((item = cont->getNextItem(item))) {
				Com_Printf(".. item.def(): %i, item.ammo: %i, item.ammoLeft: %i, x: %i, y: %i\n",
						(item->def() ? item->def()->idx : NONE), (item->ammoDef() ? item->ammoDef()->idx : NONE),
						item->getAmmoLeft(), item->getX(), item->getY());
				if (item->def())
					Com_Printf(".... weapon: %s\n", item->def()->id);
				if (item->ammoDef())
					Com_Printf(".... ammo:   %s (%i)\n", item->ammoDef()->id, item->getAmmoLeft());
			}
		}
		const float invWeight = actor->chr.inv.getWeight();
		const int maxWeight = actor->chr.score.skills[ABILITY_POWER];
		const float penalty = GET_ENCUMBRANCE_PENALTY(invWeight, maxWeight);
		const int normalTU = GET_TU(actor->chr.score.skills[ABILITY_SPEED], 1.0f - WEIGHT_NORMAL_PENALTY);
		const int tus = GET_TU(actor->chr.score.skills[ABILITY_SPEED], penalty);
		const int tuPenalty = tus - normalTU;
		const char* penaltyStr = 1.0f - penalty < WEIGHT_NORMAL_PENALTY ? "'Light weight'" : (1.0f - penalty < WEIGHT_HEAVY_PENALTY ? "'Normal weight'" : "'Encumbered'");
		Com_Printf("Weight: %g/%i, Encumbrance: %s (%.0f%%), TU's: %i (normal: %i, penalty/bonus: %+i)\n", invWeight, maxWeight, penaltyStr, invWeight / maxWeight * 100.0f, tus, normalTU, tuPenalty);
	}
}

static void G_TouchEdict_f (void)
{
	if (gi.Cmd_Argc() < 2) {
		gi.DPrintf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	const int i = atoi(gi.Cmd_Argv(1));
	if (!G_EdictsIsValidNum(i))
		return;

	Edict* e = G_EdictsGetByNum(i);
	if (!e->hasTouch()) {
		gi.DPrintf("No touch function for entity %s\n", e->classname);
		return;
	}

	Actor* actor = G_EdictsGetNextLivingActor(nullptr);
	if (!actor)
		return;	/* didn't find any */

	gi.DPrintf("Call touch function for %s\n", e->classname);
	e->callTouch(actor);
}

static void G_UseEdict_f (void)
{
	if (gi.Cmd_Argc() < 2) {
		gi.DPrintf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	const int i = atoi(gi.Cmd_Argv(1));
	if (!G_EdictsIsValidNum(i)) {
		gi.DPrintf("No entity with number %i\n", i);
		return;
	}

	Edict* e = G_EdictsGetByNum(i);
	if (!e->use) {
		gi.DPrintf("No use function for entity %s\n", e->classname);
		return;
	}

	gi.DPrintf("Call use function for %s\n", e->classname);
	e->use(e, nullptr);
}

static void G_DestroyEdict_f (void)
{
	if (gi.Cmd_Argc() < 2) {
		gi.DPrintf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	const int i = atoi(gi.Cmd_Argv(1));
	if (!G_EdictsIsValidNum(i))
		return;

	Edict* e = G_EdictsGetByNum(i);
	if (!e->destroy) {
		gi.DPrintf("No destroy function for entity %s\n", e->classname);
		return;
	}

	gi.DPrintf("Call destroy function for %s\n", e->classname);
	e->destroy(e);
}

static void G_StateChange_f (void)
{
	if (gi.Cmd_Argc() < 3) {
		gi.DPrintf("Usage: %s <entnum> <state>\n States are: panic, rage, shaken", gi.Cmd_Argv(0));
		return;
	}

	const int entnum = atoi(gi.Cmd_Argv(1));
	Edict* e = G_EdictsGetByNum(entnum);
	if (e == nullptr)
		return;

	const char* state = gi.Cmd_Argv(2);
	if (Q_strcasecmp(state, "panic")) {
		e->setMorale(mor_panic->integer / 2);
	} else if (Q_strcasecmp(state, "shaken")) {
		e->setMorale(mor_shaken->integer / 2);
	} else if (Q_strcasecmp(state, "rage")) {
		e->setMorale(m_rage->integer / 2);
	} else {
		e->setMorale(0);
	}

	G_MoraleBehaviour(e->getTeam());
}
#endif

void G_ClientCommand (Player& player)
{
	if (!player.isInUse())
		return;					/* not fully in game yet */

	const char* cmd = gi.Cmd_Argv(0);

	if (Q_strcasecmp(cmd, "players") == 0)
		G_Players_f(player);
	else if (Q_strcasecmp(cmd, "say") == 0)
		G_Say_f(player, false, false);
	else if (Q_strcasecmp(cmd, "say_team") == 0)
		G_Say_f(player, false, true);
#ifdef DEBUG
	else if (Q_strcasecmp(cmd, "debug_actorinvlist") == 0)
		G_InvList_f(player);
	else if (Q_strcasecmp(cmd, "debug_killteam") == 0)
		G_KillTeam_f();
	else if (Q_strcasecmp(cmd, "debug_stunteam") == 0)
		G_StunTeam_f();
	else if (Q_strcasecmp(cmd, "debug_listscore") == 0)
		G_ListMissionScore_f();
	else if (Q_strcasecmp(cmd, "debug_edicttouch") == 0)
		G_TouchEdict_f();
	else if (Q_strcasecmp(cmd, "debug_edictuse") == 0)
		G_UseEdict_f();
	else if (Q_strcasecmp(cmd, "debug_edictdestroy") == 0)
		G_DestroyEdict_f();
	else if (Q_strcasecmp(cmd, "debug_statechange") == 0)
		G_StateChange_f();
#endif
	else
		/* anything that doesn't match a command will be a chat */
		G_Say_f(player, true, false);
}
