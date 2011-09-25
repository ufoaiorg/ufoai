/**
 * @file g_svcmds.c
 * @brief Server commands.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
 * @brief PACKET FILTERING
 * @note You can add or remove addresses from the filter list with:
 *
 * addip IP
 * removeip IP
 * The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".
 * Removeip will only remove an address specified exactly the same way. You cannot addip a subnet, then removeip a single host.
 *
 * listip
 * Prints the current list of filters.
 *
 * writeip
 * Dumps "addip IP" commands to listip.cfg so it can be executed at a later date.  The filter lists are not saved and restored by default, because I beleive it would cause too much confusion.
 *
 * sv_filterban <0 or 1>
 * If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.
 * If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.
 */

typedef struct {
	unsigned mask;
	unsigned compare;
} ipfilter_t;

#define	MAX_IPFILTERS	1024

static ipfilter_t ipfilters[MAX_IPFILTERS];
static int numipfilters;

static qboolean StringToFilter (const char *s, ipfilter_t * f)
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
			G_ClientPrintf(NULL, PRINT_CONSOLE, "Bad filter address: %s\n", s);
			return qfalse;
		}

		j = 0;
		while (isdigit(*s)) {
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 0xFF;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *) m;
	f->compare = *(unsigned *) b;

	return qtrue;
}

qboolean SV_FilterPacket (const char *from)
{
	int i;
	unsigned in;
	byte m[4];
	const char *p;

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
			return sv_filterban->integer;

	return !sv_filterban->integer;
}


/**
 * @sa SVCmd_RemoveIP_f
 */
static void SVCmd_AddIP_f (void)
{
	int i;

	if (gi.Cmd_Argc() < 3) {
		gi.DPrintf("Usage: %s <ip-mask>\n", gi.Cmd_Argv(1));
		return;
	}

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].compare == ~0x0)
			break;				/* free spot */
	if (i == numipfilters) {
		if (numipfilters == MAX_IPFILTERS) {
			gi.DPrintf("IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter(gi.Cmd_Argv(2), &ipfilters[i]))
		ipfilters[i].compare = ~0x0;
}

/**
 * @sa SVCmd_AddIP_f
 */
static void SVCmd_RemoveIP_f (void)
{
	ipfilter_t f;
	int i, j;

	if (gi.Cmd_Argc() < 3) {
		gi.DPrintf("Usage: %s <ip-mask>\n", gi.Cmd_Argv(1));
		return;
	}

	if (!StringToFilter(gi.Cmd_Argv(2), &f))
		return;

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare) {
			for (j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			gi.DPrintf("Removed.\n");
			return;
		}
	gi.DPrintf("Didn't find %s.\n", gi.Cmd_Argv(2));
}

/**
 * @brief Shows the current ip in the filter list
 */
static void SVCmd_ListIP_f (void)
{
	int i;
	byte b[4];

	gi.DPrintf("Filter list:\n");
	for (i = 0; i < numipfilters; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		gi.DPrintf("%3i.%3i.%3i.%3i\n", b[0], b[1], b[2], b[3]);
	}
}

/**
 * @brief Store all ips in the current filter list in
 */
static void SVCmd_WriteIP_f (void)
{
	FILE *f;
	char name[MAX_OSPATH];
	byte b[4];
	int i;

	Com_sprintf(name, sizeof(name), "%s/listip.cfg", gi.FS_Gamedir());

	gi.DPrintf("Writing %s.\n", name);

	f = fopen(name, "wb");
	if (!f) {
		gi.DPrintf("Couldn't open %s\n", name);
		return;
	}

	fprintf(f, "set sv_filterban %d\n", sv_filterban->integer);

	for (i = 0; i < numipfilters; i++) {
		*(unsigned *) b = ipfilters[i].compare;
		fprintf(f, "sv addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose(f);
}

/**
 * @brief Used to add ai opponents to a game
 * @note civilians can not be added with this function
 * @sa AI_CreatePlayer
 */
static void SVCmd_AI_Add_f (void)
{
	int team;

	if (gi.Cmd_Argc() < 3) {
		gi.DPrintf("Usage: %s <teamnum>\n", gi.Cmd_Argv(1));
		return;
	}
	team = atoi(gi.Cmd_Argv(2));
	if (team > TEAM_CIVILIAN && team < MAX_TEAMS) {
		if (!AI_CreatePlayer(team))
			gi.DPrintf("Couldn't create AI player.\n");
	} else
		gi.DPrintf("Bad team number.\n");
}


/**
 * @brief Call the end game function with the given team
 * used to e.g. abort singleplayer games and let the aliens win
 * @sa G_MatchEndTrigger
 */
static void SVCmd_Win_f (void)
{
	int team;

	if (gi.Cmd_Argc() < 3) {
		gi.DPrintf("Usage: %s <teamnum>\n", gi.Cmd_Argv(1));
		return;
	}
	team = atoi(gi.Cmd_Argv(2));
	if (team > TEAM_CIVILIAN && team < MAX_TEAMS)
		G_MatchEndTrigger(team, 0);
	else
		gi.DPrintf("Bad team number.\n");
}

#ifdef DEBUG
static void SVCmd_ShowAll_f (void)
{
	edict_t *ent = NULL;

	/* Make everything visible to anyone who can't already see it */
	while ((ent = G_EdictsGetNextInUse(ent))) {
		G_AppearPerishEvent(~G_VisToPM(ent->visflags), 1, ent, NULL);
		G_VisFlagsAdd(ent, ~ent->visflags);
	}
	gi.DPrintf("All items and creatures revealed to all sides\n");
}

static void SVCmd_AddItem_f (void)
{
	const int team = TEAM_DEFAULT;
	edict_t *ent = G_EdictsGetNextLivingActorOfTeam(NULL, team);

	if (gi.Cmd_Argc() < 3) {
		gi.DPrintf("Usage: %s <item-id>\n", gi.Cmd_Argv(1));
		return;
	}

	if (!ent) {
		gi.DPrintf("Could not add item, no living members of team %i left\n", team);
		return;
	}

	G_AddItemToFloor(ent->pos, gi.Cmd_Argv(2));
}

/**
 * @brief Debug function to show the hole inventory of all connected clients on the server
 */
static void SVCmd_ActorInvList_f (void)
{
	player_t* player;
	int i;

	/* show inventory off all players around - include even the ai players */
	for (i = 0, player = game.players; i < game.sv_maxplayersperteam * 2; i++, player++) {
		if (!player->inuse)
			continue;
		G_InvList_f(player);
	}
}

static void SVCmd_ListEdicts_f (void)
{
	edict_t *ent = NULL;
	int i = 0;
	Com_Printf("number | entnum | mapnum | type | inuse | pnum | team | size |  HP | state | classname      | model/ptl             | pos\n");
	while ((ent = G_EdictsGetNext(ent))) {
		char buf[128];
		const char *model;
		if (ent->type == ET_PARTICLE)
			model = ent->particle;
		else if (ent->model)
			model = ent->model;
		else
			model = "no mdl";
		Com_sprintf(buf, sizeof(buf), "#%5i | #%5i | #%5i | %4i | %5i | %4i | %4i | %4i | %3i | %5i | %14s | %21s | %i:%i:%i",
				i, ent->number, ent->mapNum, ent->type, ent->inuse, ent->pnum, ent->team, ent->fieldSize,
				ent->HP, ent->state, ent->classname, model, ent->pos[0], ent->pos[1], ent->pos[2]);
		Com_Printf("%s\n", buf);
		i++;
	}
}
#endif

/**
 * @brief ServerCommand will be called when an "sv" command is issued.
 * The game can issue gi.Cmd_Argc() / gi.Cmd_Argv() commands to get the rest
 * of the parameters
 * @sa serverCommandList
 */
void G_ServerCommand (void)
{
	const char *cmd;

	cmd = gi.Cmd_Argv(1);
	if (Q_strcasecmp(cmd, "addip") == 0)
		SVCmd_AddIP_f();
	else if (Q_strcasecmp(cmd, "removeip") == 0)
		SVCmd_RemoveIP_f();
	else if (Q_strcasecmp(cmd, "listip") == 0)
		SVCmd_ListIP_f();
	else if (Q_strcasecmp(cmd, "writeip") == 0)
		SVCmd_WriteIP_f();
	else if (Q_strcasecmp(cmd, "ai_add") == 0)
		SVCmd_AI_Add_f();
	else if (Q_strcasecmp(cmd, "win") == 0)
		SVCmd_Win_f();
#ifdef DEBUG
	else if (Q_strcasecmp(cmd, "debug_showall") == 0)
		SVCmd_ShowAll_f();
	else if (Q_strcasecmp(cmd, "debug_additem") == 0)
		SVCmd_AddItem_f();
	else if (Q_strcasecmp(cmd, "debug_actorinvlist") == 0)
		SVCmd_ActorInvList_f();
	else if (Q_strcasecmp(cmd, "debug_listedicts") == 0)
		SVCmd_ListEdicts_f();
#endif
	else
		gi.DPrintf("Unknown server command \"%s\"\n", cmd);
}
