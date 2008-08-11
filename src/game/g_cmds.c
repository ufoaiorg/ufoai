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

static void Cmd_Players_f (player_t * player)
{
	int i;
	int count = 0;
	char smallBuf[64];
	char largeBuf[1280];

	/* print information */
	largeBuf[0] = 0;

	for (i = 0; i < game.sv_maxplayersperteam; i++) {
		if (!game.players[i].pers.team)
			continue;

		Com_sprintf(smallBuf, sizeof(smallBuf), "Team %i %s\n", game.players[i].pers.team, game.players[i].pers.netname);

		/* can't print all of them in one packet */
		if (strlen(smallBuf) + strlen(largeBuf) > sizeof(largeBuf) - 100) {
			Q_strcat(largeBuf, "...\n", sizeof(largeBuf));
			break;
		}
		Q_strcat(largeBuf, smallBuf, sizeof(largeBuf));
		count++;
	}

	gi.cprintf(player, PRINT_CONSOLE, "%s\n%i players\n", largeBuf, count);
}

/**
 * @brief Check whether the user can talk
 */
static qboolean G_CheckFlood (player_t *player)
{
	int i;

	if (flood_msgs->integer) {
		if (level.time < player->pers.flood_locktill) {
			gi.cprintf(player, PRINT_CONSOLE, "You can't talk for %d more seconds\n",
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

static void G_Say (player_t *player, qboolean arg0, qboolean team)
{
	int j;
	const char *p, *token;
	char text[2048];

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
		p = gi.Cmd_Args();

		token = COM_Parse(&p);

		Q_strcat(text, token, sizeof(text));
	}

	/* don't let text be too long for malicious reasons */
	if (strlen(text) > 150)
		text[150] = 0;

	Q_strcat(text, "\n", sizeof(text));

	if (sv_dedicated->integer)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 0; j < game.sv_maxplayersperteam; j++) {
		if (!game.players[j].inuse)
			continue;
		if (team && game.players[j].pers.team != player->pers.team)
			continue;
		gi.cprintf(&game.players[j], PRINT_CHAT, "%s", text);
	}
}

static void Cmd_PlayerList_f (player_t * player)
{
	int i;
	char st[80];
	char text[1400];
	player_t *e2;

	/* team, ping, name */
	*text = 0;
	/* list all human controlled players here */
	for (i = 0, e2 = game.players; i < game.sv_maxplayersperteam; i++, e2++) {
		if (!e2->inuse)
			continue;

		Com_sprintf(st, sizeof(st), "(%i) Team %i %s status: %s\n", e2->num,
				e2->pers.team, e2->pers.netname, (e2->ready ? "waiting" : "playing"));
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			Q_strcat(text, "And more...\n", sizeof(text));
			gi.cprintf(player, PRINT_CONSOLE, "%s", text);
			return;
		}
		Q_strcat(text, st, sizeof(text));
	}
	gi.cprintf(player, PRINT_CONSOLE, "%s", text);
}

#ifdef DEBUG
/**
 * @brief Debug function to print a player's inventory
 */
void Cmd_InvList (player_t *player)
{
	edict_t *ent;
	int i;

	Com_Printf("Print inventory for '%s'\n", player->pers.netname);
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == player->pers.team) {
			Com_Printf("actor: '%s'\n", ent->chr.name);
			INVSH_PrintContainerToConsole(&ent->i);
		}
}

static void G_TouchEdict_f (void)
{
	edict_t *e;
	int i, j;

	if (gi.Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (i < 0 || i >= globals.num_edicts)
		return;

	e = &g_edicts[i];
	if (!e->touch) {
		Com_Printf("No touch function for entity %s\n", e->classname);
		return;
	}

	for (j = 0; j < globals.num_edicts; j++)
		if (G_IsLivingActor(&g_edicts[j]))
			break;
	if (j == globals.num_edicts)
		return;

	Com_Printf("Call touch function for %s\n", e->classname);
	e->touch(e, &g_edicts[j]);
}

static void G_UseEdict_f (void)
{
	edict_t *e;
	int i;

	if (gi.Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (i < 0 || i >= globals.num_edicts)
		return;

	e = &g_edicts[i];
	if (!e->use) {
		Com_Printf("No use function for entity %s\n", e->classname);
		return;
	}

	Com_Printf("Call use function for %s\n", e->classname);
	e->use(e);
}

#endif

void G_ClientCommand (player_t * player)
{
	const char *cmd;

	if (!player->inuse)
		return;					/* not fully in game yet */

	cmd = gi.Cmd_Argv(0);

	if (Q_strcasecmp(cmd, "players") == 0)
		Cmd_Players_f(player);
	else if (Q_strcasecmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(player);
	else if (Q_strcasecmp(cmd, "say") == 0)
		G_Say(player, qfalse, qfalse);
	else if (Q_strcasecmp(cmd, "say_team") == 0)
		G_Say(player, qfalse, qtrue);
#ifdef DEBUG
	else if (Q_strcasecmp(cmd, "actorinvlist") == 0)
		Cmd_InvList(player);
	else if (Q_strcasecmp(cmd, "killteam") == 0)
		G_KillTeam();
	else if (Q_strcasecmp(cmd, "stunteam") == 0)
		G_StunTeam();
	else if (Q_strcasecmp(cmd, "debug_listscore") == 0)
		G_ListMissionScore_f();
	else if (Q_strcasecmp(cmd, "debug_edicttouch") == 0)
		G_TouchEdict_f();
	else if (Q_strcasecmp(cmd, "debug_edictuse") == 0)
		G_UseEdict_f();
#endif
	else
		/* anything that doesn't match a command will be a chat */
		G_Say(player, qtrue, qfalse);
}
