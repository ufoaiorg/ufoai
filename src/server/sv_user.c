/**
 * @file sv_user.c
 * @brief Server code for moving users.
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
============================================================
*/

/**
 * @brief Sends the first message from the server to a connected client.
 * This will be sent on the initial connection and upon each server load.
 * Client reads via CL_ParseServerData in cl_parse.c
 * @sa CL_Reconnect_f
 * @sa CL_ConnectionlessPacket
 */
static void SV_New_f (client_t *cl)
{
	int playernum;

	Com_DPrintf(DEBUG_SERVER, "New() from %s\n", cl->name);

	if (cl->state != cs_connected) {
		if (cl->state == cs_spawning) {
			/* client typed 'reconnect/new' while connecting. */
			Com_Printf("SV_New_f: client typed 'reconnect/new' while connecting\n");
			SV_ClientCommand(cl, "\ndisconnect\nreconnect\n");
			SV_DropClient(cl, "");
		} else
			Com_DPrintf(DEBUG_SERVER, "WARNING: Illegal 'new' from %s, client state %d. This shouldn't happen...\n", cl->name, cl->state);
		return;
	}

	/* client state to prevent multiple new from causing high cpu / overflows. */
	SV_SetClientState(cl, cs_spawning);

	/* serverdata needs to go over for all types of servers
	 * to make sure the protocol is right, and to set the gamedir */
	playernum = cl - svs.clients;

	/* send the serverdata */
	{
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_serverdata);
		NET_WriteLong(msg, PROTOCOL_VERSION);

		NET_WriteShort(msg, playernum);

		/* send full levelname */
		NET_WriteString(msg, sv.configstrings[CS_NAME]);

		NET_WriteMsg(cl->stream, msg);
	}

	/* game server */
	if (Com_ServerState() == ss_game) {
		int i;
		for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
			if (sv.configstrings[i][0] != '\0') {
				struct dbuffer *msg = new_dbuffer();
				Com_DPrintf(DEBUG_SERVER, "sending configstring %d: %s\n", i, sv.configstrings[i]);
				NET_WriteByte(msg, svc_configstring);
				NET_WriteShort(msg, i);
				NET_WriteString(msg, sv.configstrings[i]);
				/* enqueue and free msg */
				NET_WriteMsg(cl->stream, msg);
			}
		}
	}

	SV_ClientCommand(cl, "precache\n");
}

/**
 * @sa SV_Spawn_f
 */
static void SV_Begin_f (client_t *cl)
{
	qboolean began;

	Com_DPrintf(DEBUG_SERVER, "Begin() from %s\n", cl->name);

	/* could be abused to respawn or cause spam/other mod-specific problems */
	if (cl->state != cs_spawning) {
		Com_Printf("EXPLOIT: Illegal 'begin' from %s (already spawned), client dropped.\n", cl->name);
		SV_DropClient(cl, "Illegal begin\n");
		return;
	}

	/* call the game begin function */
	SDL_mutexP(svs.serverMutex);
	began = ge->ClientBegin(cl->player);
	SDL_mutexV(svs.serverMutex);

	if (!began) {
		SV_DropClient(cl, "'begin' failed\n");
		return;
	}
	SV_SetClientState(cl, cs_began);

	Cbuf_InsertFromDefer();
}

/**
 * @sa SV_Begin_f
 */
static void SV_Spawn_f (client_t *cl)
{
	Com_DPrintf(DEBUG_SERVER, "Spawn() from %s\n", cl->name);

	if (cl->state != cs_began) {
		SV_DropClient(cl, "Invalid state\n");
		return;
	}

	SDL_mutexP(svs.serverMutex);
	ge->ClientSpawn(cl->player);
	SDL_mutexV(svs.serverMutex);
	SV_SetClientState(cl, cs_spawned);

	Cbuf_InsertFromDefer();
}

/*============================================================================ */

/**
 * @brief The client is going to disconnect, so remove the connection immediately
 */
static void SV_Disconnect_f (client_t *cl)
{
	SV_DropClient(cl, "Disconnect\n");
}


/**
 * @brief Dumps the serverinfo info string
 */
static void SV_ShowServerinfo_f (client_t *cl)
{
	Info_Print(Cvar_Serverinfo());
}


typedef struct {
	const char *name;
	void (*func) (client_t *client);
} ucmd_t;

static const ucmd_t ucmds[] = {
	/* auto issued */
	{"new", SV_New_f},
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
static void SV_ExecuteUserCommand (client_t * cl, const char *s)
{
	const ucmd_t *u;

	Cmd_TokenizeString(s, qfalse);

	for (u = ucmds; u->name; u++)
		if (!strcmp(Cmd_Argv(0), u->name)) {
			Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: %s\n", s);
			u->func(cl);
			return;
		}

	if (Com_ServerState() == ss_game) {
		Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: client command: %s\n", s);
		SDL_mutexP(svs.serverMutex);
		ge->ClientCommand(cl->player);
		SDL_mutexV(svs.serverMutex);
	}
}

/**
 * @brief The current net_message is parsed for the given client
 */
void SV_ExecuteClientMessage (client_t * cl, int cmd, struct dbuffer *msg)
{
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
		NET_ReadString(msg, cl->userinfo, sizeof(cl->userinfo));
		Com_DPrintf(DEBUG_SERVER, "userinfo from client: %s\n", cl->userinfo);
		SV_UserinfoChanged(cl);
		break;

	case clc_stringcmd:
	{
		char str[1024];
		NET_ReadString(msg, str, sizeof(str));

		Com_DPrintf(DEBUG_SERVER, "stringcmd from client: %s\n", str);
		SV_ExecuteUserCommand(cl, str);

		if (cl->state == cs_free)
			return;			/* disconnect command */
		break;
	}

	case clc_action:
		/* client actions are handled by the game module */
		sv_msg = msg;
		SDL_mutexP(svs.serverMutex);
		ge->ClientAction(cl->player);
		SDL_mutexV(svs.serverMutex);
		sv_msg = NULL;
		break;

	case clc_endround:
		/* player wants to end round */
		sv_msg = msg;
		SDL_mutexP(svs.serverMutex);
		ge->ClientEndRound(cl->player);
		SDL_mutexV(svs.serverMutex);
		sv_msg = NULL;
		break;

	case clc_teaminfo:
		/* player sends team info */
		/* actors spawn accordingly */
		sv_msg = msg;
		SDL_mutexP(svs.serverMutex);
		ge->ClientTeamInfo(cl->player);
		SDL_mutexV(svs.serverMutex);
		sv_msg = NULL;
		break;

	case clc_initactorstates:
		/* player sends team info */
		/* actors spawn accordingly */
		sv_msg = msg;
		SDL_mutexP(svs.serverMutex);
		ge->ClientInitActorStates(cl->player);
		SDL_mutexV(svs.serverMutex);
		sv_msg = NULL;
		break;
	}
}
