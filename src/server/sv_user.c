/**
 * @file sv_user.c
 * @brief Server code for moving users.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_users.c
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

#include "server.h"

player_t *sv_player;

/**
 * @brief Set the client state
 * @sa client_state_t
 */
void SV_SetClientState (client_t* client, int state)
{
	assert(client);
	Com_DPrintf(DEBUG_SERVER, "Set state for client '%s' to %i\n", client->name, state);
	client->state = state;
}

/*
============================================================
USER STRINGCMD EXECUTION

sv_client and sv_player will be valid.
============================================================
*/

/**
 * @brief Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 * Client reads via CL_ParseServerData in cl_parse.c
 * @sa CL_Reconnect_f
 * @sa CL_ConnectionlessPacket
 */
static void SV_New_f (void)
{
	int playernum;

	Com_DPrintf(DEBUG_SERVER, "New() from %s\n", sv_client->name);

	if (sv_client->state != cs_connected) {
		if (sv_client->state == cs_spawning) {
			/* client typed 'reconnect/new' while connecting. */
			Com_Printf("SV_New_f: client typed 'reconnect/new' while connecting\n");
			SV_ClientCommand(sv_client, "\ndisconnect\nreconnect\n");
			SV_DropClient(sv_client, "");
		} else
			Com_DPrintf(DEBUG_SERVER, "WARNING: Illegal 'new' from %s, client state %d. This shouldn't happen...\n", sv_client->name, sv_client->state);
		return;
	}

	/* client state to prevent multiple new from causing high cpu / overflows. */
	SV_SetClientState(sv_client, cs_spawning);

	/* serverdata needs to go over for all types of servers
	 * to make sure the protocol is right, and to set the gamedir */
	playernum = sv_client - svs.clients;

	/* send the serverdata */
	{
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_serverdata);
		NET_WriteLong(msg, PROTOCOL_VERSION);
		NET_WriteLong(msg, svs.spawncount);

		NET_WriteShort(msg, playernum);

		/* send full levelname */
		NET_WriteString(msg, sv.configstrings[CS_NAME]);

		NET_WriteMsg(sv_client->stream, msg);
	}

	/* game server */
	if (Com_ServerState() == ss_game) {
		/* begin fetching configstrings */
		SV_ClientCommand(sv_client, "cmd configstrings %i 0\n", svs.spawncount);
	}
}

/**
 * @brief Send the configstrings to the client
 */
static void SV_Configstrings_f (void)
{
	int i;
	int start;

	Com_DPrintf(DEBUG_SERVER, "Configstrings() from %s\n", sv_client->name);

	if (sv_client->state != cs_spawning) {
		Com_Printf("configstrings not valid -- already spawning\n");
		return;
	}

	/* handle the case of a level changing while a client was connecting */
	if (atoi(Cmd_Argv(1)) != svs.spawncount) {
		Com_Printf("SV_Configstrings_f from different level\n");
		SV_New_f();
		return;
	}

	start = atoi(Cmd_Argv(2));
	if (start < 0)
		start = 0; /* catch negative offsets */

	for (i = start; i < MAX_CONFIGSTRINGS; i++) {
		if (sv.configstrings[i][0]) {
			struct dbuffer *msg = new_dbuffer();
			Com_DPrintf(DEBUG_SERVER, "sending configstring %d: %s\n", i, sv.configstrings[i]);
			NET_WriteByte(msg, svc_configstring);
			NET_WriteShort(msg, i);
			NET_WriteString(msg, sv.configstrings[i]);
			/* enqueue and free msg */
			NET_WriteMsg(sv_client->stream, msg);
		}
	}

	SV_ClientCommand(sv_client, "precache %i\n", svs.spawncount);
}

/**
 * @brief Returns the number of spawned players
 * @sa SV_ShutdownWhenEmpty
 */
int SV_CountPlayers (void)
{
	int i;
	int count = 0;
	client_t *cl;

	if (!svs.initialized)
		return 0;

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state != cs_spawned)
			continue;

		count++;
	}

	return count;
}

/**
 * @sa SV_Spawn_f
 */
static void SV_Begin_f (void)
{
	Com_DPrintf(DEBUG_SERVER, "Begin() from %s\n", sv_client->name);

	/* could be abused to respawn or cause spam/other mod-specific problems */
	if (sv_client->state != cs_spawning) {
		Com_Printf("EXPLOIT: Illegal 'begin' from %s (already spawned), client dropped.\n", sv_client->name);
		SV_DropClient(sv_client, "Illegal begin\n");
		return;
	}

	/* handle the case of a level changing while a client was connecting */
	if (atoi(Cmd_Argv(1)) != svs.spawncount) {
		Com_Printf("SV_Begin_f from different level (%i)\n", atoi(Cmd_Argv(1)));
		SV_New_f();
		return;
	}

	/* call the game begin function */
	ge->ClientBegin(sv_player);

	Cbuf_InsertFromDefer();
}

/**
 * @brief Spawns all connected clients if they are not already spawned
 * @sa CL_SpawnSoldiers_f
 */
static void SV_SpawnAllPending (void)
{
	int i;
	client_t *cl;

	if (!svs.initialized)
		return;

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl && cl->state == cs_spawning) {
			if (ge->ClientSpawn(cl->player))
				SV_SetClientState(cl, cs_spawned);
			else
				/* false means, that there are not all players - so cycling to
				 * the end here is not needed */
				return;
		}
	}
}

/**
 * @sa SV_Begin_f
 */
static void SV_Spawn_f (void)
{
	Com_DPrintf(DEBUG_SERVER, "Spawn() from %s\n", sv_client->name);

	/* handle the case of a level changing while a client was connecting */
	if (atoi(Cmd_Argv(1)) != svs.spawncount) {
		Com_Printf("SV_Spawn_f from different level\n");
		SV_New_f();
		return;
	}

	/* try and spawn any connected but unspawned players */
	SV_SpawnAllPending();

	Cbuf_InsertFromDefer();
}

/*============================================================================ */

/**
 * @brief The client is going to disconnect, so remove the connection immediately
 */
static void SV_Disconnect_f (void)
{
	SV_DropClient(sv_client, "Disconnect\n");
}


/**
 * @brief Dumps the serverinfo info string
 */
static void SV_ShowServerinfo_f (void)
{
	Info_Print(Cvar_Serverinfo());
}


typedef struct {
	const char *name;
	void (*func) (void);
} ucmd_t;

static const ucmd_t ucmds[] = {
	/* auto issued */
	{"new", SV_New_f},
	{"configstrings", SV_Configstrings_f},
	{"begin", SV_Begin_f},
	{"spawn", SV_Spawn_f},

	{"disconnect", SV_Disconnect_f},

	/* issued by hand at client consoles */
	{"info", SV_ShowServerinfo_f},

	{NULL, NULL}
};

/**
 * @sa SV_ExecuteClientMessage
 */
static void SV_ExecuteUserCommand (const char *s)
{
	const ucmd_t *u;

	Cmd_TokenizeString(s, qfalse);
	sv_player = sv_client->player;

	for (u = ucmds; u->name; u++)
		if (!strcmp(Cmd_Argv(0), u->name)) {
			Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: %s\n", s);
			u->func();
			return;
		}

	if (Com_ServerState() == ss_game) {
		Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: client command: %s\n", s);
		ge->ClientCommand(sv_player);
	}
}

/**
 * @brief The current net_message is parsed for the given client
 */
void SV_ExecuteClientMessage (client_t * cl, int cmd, struct dbuffer *msg)
{
	const char *s;

	sv_client = cl;
	sv_player = sv_client->player;

	if (cmd == -1)
		return;

	switch (cmd) {
	default:
		Com_Printf("SV_ExecuteClientMessage: unknown command char '%d'\n", cmd);
		SV_DropClient(cl, "Unknown command\n");
		return;

	case clc_nop:
		break;

	case clc_userinfo:
		Q_strncpyz(cl->userinfo, NET_ReadString(msg), sizeof(cl->userinfo));
		Com_DPrintf(DEBUG_SERVER, "userinfo from client: %s\n", cl->userinfo);
		SV_UserinfoChanged(cl);
		break;

	case clc_stringcmd:
		s = NET_ReadString(msg);

		Com_DPrintf(DEBUG_SERVER, "stringcmd from client: %s\n", s);
		SV_ExecuteUserCommand(s);

		if (cl->state == cs_free)
			return;			/* disconnect command */
		break;

	case clc_action:
		/* client actions are handled by the game module */
		sv_msg = msg;
		ge->ClientAction(sv_player);
		sv_msg = NULL;
		break;

	case clc_endround:
		/* player wants to end round */
		sv_msg = msg;
		ge->ClientEndRound(sv_player, NOISY);
		sv_msg = NULL;
		break;

	case clc_teaminfo:
		/* player sends team info */
		/* actors spawn accordingly */
		sv_msg = msg;
		ge->ClientTeamInfo(sv_player);
		sv_msg = NULL;
		break;
	}
}
