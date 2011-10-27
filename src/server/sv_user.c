/**
 * @file sv_user.c
 * @brief Server code for moving users.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

	/* send the serverdata */
	{
		const int playernum = cl - SV_GetClient(0);
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_serverdata);
		NET_WriteLong(msg, PROTOCOL_VERSION);

		NET_WriteShort(msg, playernum);

		/* send full levelname */
		NET_WriteString(msg, SV_GetConfigString(CS_NAME));

		NET_WriteMsg(cl->stream, msg);
	}

	/* game server */
	if (Com_ServerState() == ss_game) {
		int i;
		for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
			const char *configString;
			/* CS_TILES and CS_POSITIONS can stretch over multiple configstrings,
			 * so don't send the middle parts again. */
			if (i > CS_TILES && i < CS_POSITIONS)
				continue;
			if (i > CS_POSITIONS && i < CS_MODELS)
				continue;

			configString = SV_GetConfigString(i);
			if (configString[0] != '\0') {
				struct dbuffer *msg = new_dbuffer();
				Com_DPrintf(DEBUG_SERVER, "sending configstring %d: %s\n", i, configString);
				NET_WriteByte(msg, svc_configstring);
				NET_WriteShort(msg, i);
				NET_WriteString(msg, configString);
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
	TH_MutexLock(svs.serverMutex);
	began = svs.ge->ClientBegin(cl->player);
	TH_MutexUnlock(svs.serverMutex);

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
static void SV_StartMatch_f (client_t *cl)
{
	Com_DPrintf(DEBUG_SERVER, "StartMatch() from %s\n", cl->name);

	if (cl->state != cs_spawned) {
		SV_DropClient(cl, "Invalid state\n");
		return;
	}

	TH_MutexLock(svs.serverMutex);
	svs.ge->ClientStartMatch(cl->player);
	TH_MutexUnlock(svs.serverMutex);

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
	{"startmatch", SV_StartMatch_f},

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
		if (Q_streq(Cmd_Argv(0), u->name)) {
			Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: %s\n", s);
			u->func(cl);
			return;
		}

	if (Com_ServerState() == ss_game) {
		Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: client command: %s\n", s);
		TH_MutexLock(svs.serverMutex);
		svs.ge->ClientCommand(cl->player);
		TH_MutexUnlock(svs.serverMutex);
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

	case clc_ack:
		cl->lastmessage = svs.realtime;
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
		sv->messageBuffer = msg;
		TH_MutexLock(svs.serverMutex);
		svs.ge->ClientAction(cl->player);
		TH_MutexUnlock(svs.serverMutex);
		sv->messageBuffer = NULL;
		break;

	case clc_endround:
		/* player wants to end round */
		sv->messageBuffer = msg;
		TH_MutexLock(svs.serverMutex);
		svs.ge->ClientEndRound(cl->player);
		TH_MutexUnlock(svs.serverMutex);
		sv->messageBuffer = NULL;
		break;

	case clc_teaminfo:
		/* player sends team info */
		/* actors spawn accordingly */
		sv->messageBuffer = msg;
		TH_MutexLock(svs.serverMutex);
		svs.ge->ClientTeamInfo(cl->player);
		TH_MutexUnlock(svs.serverMutex);
		sv->messageBuffer = NULL;
		SV_SetClientState(cl, cs_spawned);
		break;

	case clc_initactorstates:
		/* player sends team info */
		/* actors spawn accordingly */
		sv->messageBuffer = msg;
		TH_MutexLock(svs.serverMutex);
		svs.ge->ClientInitActorStates(cl->player);
		TH_MutexUnlock(svs.serverMutex);
		sv->messageBuffer = NULL;
		break;
	}
}
