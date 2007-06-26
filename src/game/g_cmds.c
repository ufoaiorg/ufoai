/**
 * @file g_cmds.c
 * @brief Player commands.
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

/**
 * @brief
 */
static void Cmd_Players_f (player_t * player)
{
	int i;
	int count = 0;
	char small[64];
	char large[1280];

	/* print information */
	large[0] = 0;

	for (i = 0; i < game.maxplayers; i++) {
		if (!game.players[i].pers.team)
			continue;

		Com_sprintf(small, sizeof(small), "Team %i %s\n", game.players[i].pers.team, game.players[i].pers.netname);

		/* can't print all of them in one packet */
		if (strlen(small) + strlen(large) > sizeof(large) - 100) {
			Q_strcat(large, "...\n", sizeof(large));
			break;
		}
		Q_strcat(large, small, sizeof(large));
		count++;
	}

	gi.cprintf(player, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/**
 * @brief Check whether the user can talk
 */
static qboolean G_CheckFlood (player_t *player)
{
	int		i;

	if (flood_msgs->integer) {
		if (level.time < player->pers.flood_locktill) {
			gi.cprintf(player, PRINT_HIGH, "You can't talk for %d more seconds\n",
					   (int)(player->pers.flood_locktill - level.time));
			return qtrue;
		}
		i = player->pers.flood_whenhead - flood_msgs->value + 1;
		if (i < 0)
			i = (sizeof(player->pers.flood_when)/sizeof(player->pers.flood_when[0])) + i;
		if (player->pers.flood_when[i] && level.time - player->pers.flood_when[i] < flood_persecond->value) {
			player->pers.flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(player, PRINT_CHAT, "Flood protection: You can't talk for %d seconds.\n",
					   flood_waitdelay->integer);
			return qtrue;
		}
		player->pers.flood_whenhead = (player->pers.flood_whenhead + 1) %
				(sizeof(player->pers.flood_when)/sizeof(player->pers.flood_when[0]));
		player->pers.flood_when[player->pers.flood_whenhead] = level.time;
	}
	return qfalse;
}

/**
 * @brief
 */
static void Cmd_Say_f (player_t *player, qboolean arg0, qboolean team)
{
	int j;
	char *p;
	char text[2048];

	if (gi.argc() < 2 && !arg0)
		return;

	if (!team)
		Com_sprintf(text, sizeof(text), "%s: ", player->pers.netname);
	else
		Com_sprintf(text, sizeof(text), "^B%s (team): ", player->pers.netname);

	if (arg0) {
		Q_strcat(text, gi.argv(0), sizeof(text));
		Q_strcat(text, " ", sizeof(text));
		Q_strcat(text, gi.args(), sizeof(text));
	} else {
		p = gi.args();

		if (*p == '"') {
			p++;
			p[strlen(p) - 1] = 0;
		}
		Q_strcat(text, p, sizeof(text));
	}

	/* don't let text be too long for malicious reasons */
	if (strlen(text) > 150)
		text[150] = 0;

	Q_strcat(text, "\n", sizeof(text));

	if (G_CheckFlood(player))
		return;

	if (dedicated->integer)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 0; j < game.maxplayers; j++) {
		if (!game.players[j].inuse)
			continue;
		if (team && game.players[j].pers.team != player->pers.team)
			continue;
		gi.cprintf(&game.players[j], PRINT_CHAT, "%s", text);
	}
}

/**
 * @brief
 */
static void Cmd_PlayerList_f (player_t * player)
{
	int i;
	char st[80];
	char text[1400];
	player_t *e2;

	/* team, ping, name */
	*text = 0;
	/* list all human controlled players here */
	for (i = 0, e2 = game.players; i < game.maxplayers; i++, e2++) {
		if (!e2->inuse)
			continue;

		Com_sprintf(st, sizeof(st), "(%i) Team %i %s status: %s\n", e2->num,
				e2->pers.team, e2->pers.netname, (e2->ready ? "waiting" : "playing"));
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			Q_strcat(text, "And more...\n", sizeof(text));
			gi.cprintf(player, PRINT_HIGH, "%s", text);
			return;
		}
		Q_strcat(text, st, sizeof(text));
	}
	gi.cprintf(player, PRINT_HIGH, "%s", text);
}

#ifdef DEBUG
void G_KillTeam(void);
void G_StunTeam(void);

/**
 * @brief Debug function to print a player's inventory
 */
extern void Cmd_InvList (player_t *player)
{
	edict_t *ent;
	int i;

	Com_Printf("Print inventory for '%s'\n", player->pers.netname);
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && (ent->type == ET_ACTOR || ent->type == ET_UGV) && ent->team == player->pers.team) {
			Com_Printf("actor: '%s'\n", ent->chr.name);
			INV_PrintToConsole(&ent->i);
		}
}
#endif

/**
 * @brief
 */
void G_ClientCommand (player_t * player)
{
	char *cmd;

	if (!player->inuse)
		return;					/* not fully in game yet */

	cmd = gi.argv(0);

	if (Q_stricmp(cmd, "players") == 0)
		Cmd_Players_f(player);
	else if (Q_stricmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(player);
	else if (Q_stricmp(cmd, "say") == 0)
		Cmd_Say_f(player, qfalse, qfalse);
	else if (Q_stricmp(cmd, "say_team") == 0)
		Cmd_Say_f(player, qfalse, qtrue);
#ifdef DEBUG
	else if (Q_stricmp(cmd, "actorinvlist") == 0)
		Cmd_InvList(player);
	else if (Q_stricmp(cmd, "killteam") == 0)
		G_KillTeam();
	else if (Q_stricmp(cmd, "stunteam") == 0)
		G_StunTeam();
#endif
	else
		/* anything that doesn't match a command will be a chat */
		Cmd_Say_f(player, qtrue, qfalse);
}
