/**
 * @file g_cmds.c
 * @brief Player commands.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "../shared/parse.h"

static void G_Players_f (const player_t *player)
{
	int count = 0;
	char smallBuf[64];
	char largeBuf[1280];
	player_t *p;

	/* print information */
	largeBuf[0] = 0;

	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		Com_sprintf(smallBuf, sizeof(smallBuf), "(%i) Team %i %s status: %s\n", p->num,
				p->pers.team, p->pers.netname, (p->roundDone ? "waiting" : "playing"));

		/* can't print all of them in one packet */
		if (strlen(smallBuf) + strlen(largeBuf) > sizeof(largeBuf) - 100) {
			Q_strcat(largeBuf, "...\n", sizeof(largeBuf));
			break;
		}
		Q_strcat(largeBuf, smallBuf, sizeof(largeBuf));
		count++;
	}

	G_ClientPrintf(player, PRINT_CONSOLE, "%s\n%i players\n", largeBuf, count);
}

/**
 * @brief Check whether the user can talk
 */
static qboolean G_CheckFlood (player_t *player)
{
	int i;

	if (flood_msgs->integer) {
		if (level.time < player->pers.flood_locktill) {
			G_ClientPrintf(player, PRINT_CHAT, _("You can't talk for %d more seconds\n"), (int)(player->pers.flood_locktill - level.time));
			return qtrue;
		}
		i = player->pers.flood_whenhead - flood_msgs->value + 1;
		if (i < 0)
			i = (sizeof(player->pers.flood_when)/sizeof(player->pers.flood_when[0])) + i;
		if (player->pers.flood_when[i] && level.time - player->pers.flood_when[i] < flood_persecond->value) {
			player->pers.flood_locktill = level.time + flood_waitdelay->value;
			G_ClientPrintf(player, PRINT_CHAT, _("Flood protection: You can't talk for %d seconds.\n"), flood_waitdelay->integer);
			return qtrue;
		}
		player->pers.flood_whenhead = (player->pers.flood_whenhead + 1) %
				(sizeof(player->pers.flood_when)/sizeof(player->pers.flood_when[0]));
		player->pers.flood_when[player->pers.flood_whenhead] = level.time;
	}
	return qfalse;
}

static void G_Say_f (player_t *player, qboolean arg0, qboolean team)
{
	char text[256];
	player_t *p;

	if (gi.Cmd_Argc() < 2 && !arg0)
		return;

	if (G_CheckFlood(player))
		return;

	if (!team)
		Com_sprintf(text, sizeof(text), "%s: ", player->pers.netname);
	else
		Com_sprintf(text, sizeof(text), "^B%s (team): ", player->pers.netname);

	if (arg0) {
		Q_strcat(text, gi.Cmd_Argv(0), sizeof(text));
		Q_strcat(text, " ", sizeof(text));
		Q_strcat(text, gi.Cmd_Args(), sizeof(text));
	} else {
		const char *p = gi.Cmd_Args();
		const char *token = Com_Parse(&p);

		Q_strcat(text, token, sizeof(text));
	}

	Q_strcat(text, "\n", sizeof(text));

	if (sv_dedicated->integer)
		gi.DPrintf("%s", text);

	p = NULL;
	while ((p = G_PlayerGetNextActiveHuman(p))) {
		if (team && p->pers.team != player->pers.team)
			continue;
		G_ClientPrintf(p, PRINT_CHAT, "%s", text);
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
	edict_t *ent = NULL;
	int amount = -1;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() >= 2) {
		teamToKill = atoi(gi.Cmd_Argv(1));
		if (gi.Cmd_Argc() == 3)
			amount = atoi(gi.Cmd_Argv(2));
	}

	Com_DPrintf(DEBUG_GAME, "G_KillTeam: kill team %i\n", teamToKill);

	if (teamToKill >= 0) {
		while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, teamToKill))) {
			if (amount == 0)
				break;
			/* die */
			ent->HP = 0;
			G_ActorDieOrStun(ent, NULL);
			amount--;
		}
	}

	/* check for win conditions */
	G_MatchEndCheck();
}

/**
 * @brief Stun all members of a giben team.
 */
static void G_StunTeam_f (void)
{
	/* default is to kill all teams */
	int teamToKill = -1;
	edict_t *ent = NULL;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() == 2)
		teamToKill = atoi(gi.Cmd_Argv(1));

	if (teamToKill >= 0) {
		while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, teamToKill))) {
			/* stun */
			G_ActorDieOrStun(ent, NULL);

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
	edict_t *ent = NULL;
	int i, j;

	/* With a parameter we will be able to get the info for a specific team */
	if (gi.Cmd_Argc() == 2) {
		team = atoi(gi.Cmd_Argv(1));
	} else {
		gi.DPrintf("Usage: %s <teamnumber>\n", gi.Cmd_Argv(0));
		return;
	}

	while ((ent = G_EdictsGetNextLivingActor(ent))) {
		if (team >= 0 && ent->team != team)
			continue;

		assert(ent->chr.scoreMission);

		gi.DPrintf("Soldier: %s\n", ent->chr.name);

		/* ===================== */
		gi.DPrintf("  Move: Normal=%i Crouched=%i\n", ent->chr.scoreMission->movedNormal, ent->chr.scoreMission->movedCrouched);

		gi.DPrintf("  Kills:");
		for (i = 0; i < KILLED_NUM_TYPES; i++) {
			gi.DPrintf(" %i", ent->chr.scoreMission->kills[i]);
		}
		gi.DPrintf("\n");

		gi.DPrintf("  Stuns:");
		for (i = 0; i < KILLED_NUM_TYPES; i++) {
			gi.DPrintf(" %i", ent->chr.scoreMission->stuns[i]);
		}
		gi.DPrintf("\n");

		/* ===================== */
		gi.DPrintf("  Fired:");
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf(" %i", ent->chr.scoreMission->fired[i]);
		}
		gi.DPrintf("\n");

		gi.DPrintf("  Hits:\n");
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf("    Skill%i: ",i);
			for (j = 0; j < KILLED_NUM_TYPES; j++) {
				gi.DPrintf(" %i", ent->chr.scoreMission->hits[i][j]);
			}
			gi.DPrintf("\n");
		}

		/* ===================== */
		gi.DPrintf("  Fired Splash:");
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf(" %i", ent->chr.scoreMission->firedSplash[i]);
		}
		gi.DPrintf("\n");

		gi.DPrintf("  Hits Splash:\n");
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf("    Skill%i: ",i);
			for (j = 0; j < KILLED_NUM_TYPES; j++) {
				gi.DPrintf(" %i", ent->chr.scoreMission->hitsSplash[i][j]);
			}
			gi.DPrintf("\n");
		}

		gi.DPrintf("  Splash Damage:\n");
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf("    Skill%i: ",i);
			for (j = 0; j < KILLED_NUM_TYPES; j++) {
				gi.DPrintf(" %i", ent->chr.scoreMission->hitsSplashDamage[i][j]);
			}
			gi.DPrintf("\n");
		}

		/* ===================== */
		gi.DPrintf("  Kills per skill:");
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			gi.DPrintf(" %i", ent->chr.scoreMission->skillKills[i]);
		}
		gi.DPrintf("\n");

		/* ===================== */
		gi.DPrintf("  Heal (received): %i\n", ent->chr.scoreMission->heal);
	}
}

/**
 * @brief Debug function to print a player's inventory
 */
void G_InvList_f (const player_t *player)
{
	edict_t *ent = NULL;

	gi.DPrintf("Print inventory for '%s'\n", player->pers.netname);
	while ((ent = G_EdictsGetNextLivingActorOfTeam(ent, player->pers.team))) {
		containerIndex_t container;
		gi.DPrintf("actor: '%s'\n", ent->chr.name);

		for (container = 0; container < gi.csi->numIDs; container++) {
			const invList_t *ic = CONTAINER(ent, container);
			Com_Printf("Container: %i\n", container);
			while (ic) {
				Com_Printf(".. item.t: %i, item.m: %i, item.a: %i, x: %i, y: %i\n",
						(ic->item.t ? ic->item.t->idx : NONE), (ic->item.m ? ic->item.m->idx : NONE),
						ic->item.a, ic->x, ic->y);
				if (ic->item.t)
					Com_Printf(".... weapon: %s\n", ic->item.t->id);
				if (ic->item.m)
					Com_Printf(".... ammo:   %s (%i)\n", ic->item.m->id, ic->item.a);
				ic = ic->next;
			}
		}
	}
}

static void G_TouchEdict_f (void)
{
	edict_t *e, *ent;
	int i;

	if (gi.Cmd_Argc() < 2) {
		gi.DPrintf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (!G_EdictsIsValidNum(i))
		return;

	e = G_EdictsGetByNum(i);
	if (!e->touch) {
		gi.DPrintf("No touch function for entity %s\n", e->classname);
		return;
	}

	ent = G_EdictsGetNextLivingActor(NULL);
	if (!ent)
		return;	/* didn't find any */

	gi.DPrintf("Call touch function for %s\n", e->classname);
	e->touch(e, ent);
}

static void G_UseEdict_f (void)
{
	edict_t *e;
	int i;

	if (gi.Cmd_Argc() < 2) {
		gi.DPrintf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (!G_EdictsIsValidNum(i)) {
		gi.DPrintf("No entity with number %i\n", i);
		return;
	}

	e = G_EdictsGetByNum(i);
	if (!e->use) {
		gi.DPrintf("No use function for entity %s\n", e->classname);
		return;
	}

	gi.DPrintf("Call use function for %s\n", e->classname);
	e->use(e, NULL);
}

static void G_DestroyEdict_f (void)
{
	edict_t *e;
	int i;

	if (gi.Cmd_Argc() < 2) {
		gi.DPrintf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (!G_EdictsIsValidNum(i))
		return;

	e = G_EdictsGetByNum(i);
	if (!e->destroy) {
		gi.DPrintf("No destroy function for entity %s\n", e->classname);
		return;
	}

	gi.DPrintf("Call destroy function for %s\n", e->classname);
	e->destroy(e);
}

#endif

void G_ClientCommand (player_t * player)
{
	const char *cmd;

	if (!player->inuse)
		return;					/* not fully in game yet */

	cmd = gi.Cmd_Argv(0);

	if (Q_strcasecmp(cmd, "players") == 0)
		G_Players_f(player);
	else if (Q_strcasecmp(cmd, "say") == 0)
		G_Say_f(player, qfalse, qfalse);
	else if (Q_strcasecmp(cmd, "say_team") == 0)
		G_Say_f(player, qfalse, qtrue);
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
#endif
	else
		/* anything that doesn't match a command will be a chat */
		G_Say_f(player, qtrue, qfalse);
}
