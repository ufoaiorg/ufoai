/**
 * @file g_svcmds.c
 * @brief Server commands.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

26/06/06, Eddy Cullen (ScreamingWithNoSound):
	Reformatted to agreed style.
	Added doxygen file comment.
	Updated copyright notice.

Original file from Quake 2 v3.21: quake2-2.31/game/g_svcmds.c
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
static void SVCmd_Test_f (void)
{
	gi.cprintf(NULL, PRINT_HIGH, "Svcmd_Test_f()\n");
}

/**
 * @brief PACKET FILTERING
 *
 * You can add or remove addresses from the filter list with:
 *
 * addip <ip>
 * removeip <ip>
 *
 * The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".
 *
 * Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.
 *
 * listip
 * Prints the current list of filters.
 *
 * writeip
 * Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.
 *
 * filterban <0 or 1>
 *
 * If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.
 *
 * If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.
 */

typedef struct {
	unsigned mask;
	unsigned compare;
} ipfilter_t;

#define	MAX_IPFILTERS	1024

static ipfilter_t ipfilters[MAX_IPFILTERS];
static int numipfilters;

/**
 * @brief
 */
static qboolean StringToFilter (char *s, ipfilter_t * f)
{
	char num[128];
	int i, j;
	byte b[4];
	byte m[4];

	for (i = 0; i < 4; i++) {
		b[i] = 0;
		m[i] = 0;
	}

	for (i = 0; i < 4; i++) {
		if (*s < '0' || *s > '9') {
			gi.cprintf(NULL, PRINT_HIGH, "Bad filter address: %s\n", s);
			return qfalse;
		}

		j = 0;
		while (*s >= '0' && *s <= '9') {
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *) m;
	f->compare = *(unsigned *) b;

	return qtrue;
}

/**
 * @brief
 */
extern qboolean SV_FilterPacket (char *from)
{
	int i;
	unsigned in;
	byte m[4];
	char *p;

	i = 0;
	p = from;
	while (*p && i < 4) {
		m[i] = 0;
		while (*p >= '0' && *p <= '9') {
			m[i] = m[i] * 10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}

	in = *(unsigned *) m;

	for (i = 0; i < numipfilters; i++)
		if ((in & ipfilters[i].mask) == ipfilters[i].compare)
			return filterban->integer;

	return !filterban->integer;
}


/**
 * @brief
 */
static void SVCmd_AddIP_f (void)
{
	int i;

	if (gi.argc() < 3) {
		gi.cprintf(NULL, PRINT_HIGH, "Usage: addip <ip-mask>\n");
		return;
	}

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break;				/* free spot */
	if (i == numipfilters) {
		if (numipfilters == MAX_IPFILTERS) {
			gi.cprintf(NULL, PRINT_HIGH, "IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter(gi.argv(2), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}

/**
 * @brief
 */
static void SVCmd_RemoveIP_f (void)
{
	ipfilter_t f;
	int i, j;

	if (gi.argc() < 3) {
		gi.cprintf(NULL, PRINT_HIGH, "Usage:  sv removeip <ip-mask>\n");
		return;
	}

	if (!StringToFilter(gi.argv(2), &f))
		return;

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare) {
			for (j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			gi.cprintf(NULL, PRINT_HIGH, "Removed.\n");
			return;
		}
	gi.cprintf(NULL, PRINT_HIGH, "Didn't find %s.\n", gi.argv(2));
}

/**
 * @brief
 */
static void SVCmd_ListIP_f (void)
{
	int i;
	byte b[4];

	gi.cprintf(NULL, PRINT_HIGH, "Filter list:\n");
	for (i = 0; i < numipfilters; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		gi.cprintf(NULL, PRINT_HIGH, "%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

/**
 * @brief
 */
static void SVCmd_WriteIP_f (void)
{
	FILE *f;
	char name[MAX_OSPATH];
	byte b[4];
	int i;
	cvar_t *game;

	game = gi.cvar("fs_gamedir", "", 0, NULL);

	if (!*game->string)
		Com_sprintf(name, MAX_OSPATH, "%s/listip.cfg", GAMEVERSION);
	else
		Com_sprintf(name, MAX_OSPATH, "%s/listip.cfg", game->string);

	gi.cprintf(NULL, PRINT_HIGH, "Writing %s.\n", name);

	f = fopen(name, "wb");
	if (!f) {
		gi.cprintf(NULL, PRINT_HIGH, "Couldn't open %s\n", name);
		return;
	}

	fprintf(f, "set filterban %d\n", filterban->integer);

	for (i = 0; i < numipfilters; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		fprintf(f, "sv addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose(f);
}

/**
 * @brief
 */
static void SVCmd_AI_Add_f (void)
{
	int team;

	if (gi.argc() < 3) {
		Com_Printf("Usage: ai_add <teamnum>\n");
		return;
	}
	team = atoi(gi.argv(2));
	if (team > 0 && team < MAX_TEAMS) {
		if (!AI_CreatePlayer(team))
			Com_Printf("Couldn't create AI player.\n");
	} else
		Com_Printf("Bad team number.\n");
}


/**
 * @brief
 */
static void SVCmd_Win_f (void)
{
	int team;

	if (gi.argc() < 3) {
		Com_Printf("Usage: win <teamnum>\n");
		return;
	}
	team = atoi(gi.argv(2));
	if (team > 0 && team < MAX_TEAMS)
		G_EndGame(team);
	else
		Com_Printf("Bad team number.\n");
}

/**
 * @brief
 */
static void SVCmd_ShowAll_f (void)
{
	edict_t *ent;
	int i;

	/* Make everything visible to anyone who can't already see it */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if (ent->inuse) {
			G_AppearPerishEvent(~G_VisToPM(ent->visflags), 1, ent);
			ent->visflags |= ~ent->visflags;
		}
	Com_Printf("All items and creatures revealed to all sides\n");
}

/**
 * @brief Start the game even if not all players are connected
 * @sa G_ClientTeamAssign
 */
static void SVCmd_StartGame_f (void)
{
	int i, j, teamCount = 0;
	int playerCount = 0;
	int knownTeams[MAX_TEAMS];
	player_t *p;
	char buffer[MAX_VAR] = "";

	/* return with no action if activeTeam already assigned or if in single-player mode */
	if (level.activeTeam != -1 || sv_maxclients->integer == 1)
		return;

	/* count number of currently connected unique teams and players (only human controlled players) */
	for (i = 0, p = game.players; i < game.maxplayers; i++, p++) {
		if (p->inuse && !p->pers.spectator && p->pers.team > 0) {
			playerCount++;
			for (j = 0; j < teamCount; j++) {
				if (p->pers.team == knownTeams[j])
					break;
			}
			if (j == teamCount)
				knownTeams[teamCount++] = p->pers.team;
		}
	}
	Com_DPrintf("SVCmd_StartGame_f: Players in game: %i, Unique teams in game: %i\n", playerCount, teamCount);

	level.activeTeam = knownTeams[(int)(frand() * (teamCount - 1) + 0.5)];
	turnTeam = level.activeTeam;
	/* spawn the player (only human controlled players) */
	for (i = 0, p = game.players; i < game.maxplayers; i++, p++) {
		if (!p->inuse || p->pers.spectator)
			continue;
		G_ClientSpawn(p);
		if (p->pers.team == level.activeTeam) {
			Q_strcat(buffer, p->pers.netname, MAX_VAR);
			Q_strcat(buffer, " ", MAX_VAR);
		}
	}
	gi.bprintf(PRINT_HIGH, _("Team %i (%s) will get the first turn.\n"), turnTeam, buffer);
}

/**
 * @brief ServerCommand will be called when an "sv" command is issued.
 * The game can issue gi.argc() / gi.argv() commands to get the rest
 * of the parameters
 */
extern void ServerCommand (void)
{
	const char *cmd;

	cmd = gi.argv(1);
	if (Q_stricmp(cmd, "test") == 0)
		SVCmd_Test_f();
	else if (Q_stricmp(cmd, "startgame") == 0)
		SVCmd_StartGame_f();
	else if (Q_stricmp(cmd, "addip") == 0)
		SVCmd_AddIP_f();
	else if (Q_stricmp(cmd, "removeip") == 0)
		SVCmd_RemoveIP_f();
	else if (Q_stricmp(cmd, "listip") == 0)
		SVCmd_ListIP_f();
	else if (Q_stricmp(cmd, "writeip") == 0)
		SVCmd_WriteIP_f();
	else if (Q_stricmp(cmd, "ai_add") == 0)
		SVCmd_AI_Add_f();
	else if (Q_stricmp(cmd, "win") == 0)
		SVCmd_Win_f();
#ifdef DEBUG
	else if (Q_stricmp(cmd, "showall") == 0)
		SVCmd_ShowAll_f();
#endif
	else
		gi.cprintf(NULL, PRINT_HIGH, "Unknown server command \"%s\"\n", cmd);
}
