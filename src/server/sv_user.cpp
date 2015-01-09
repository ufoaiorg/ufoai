/**
 * @file
 * @brief Server code for moving users.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../shared/scopedmutex.h"

/**
 * @brief Set the client state
 * @sa client_state_t
 */
void SV_SetClientState (client_t* client, client_state_t state)
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
static void SV_New_f (client_t* cl)
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
		dbuffer msg;
		NET_WriteByte(&msg, svc_serverdata);
		NET_WriteLong(&msg, PROTOCOL_VERSION);

		NET_WriteShort(&msg, playernum);

		/* send full levelname */
		NET_WriteString(&msg, SV_GetConfigString(CS_NAME));

		NET_WriteMsg(cl->stream, msg);
	}

	/* game server */
	if (Com_ServerState() == ss_game) {
		for (int i = 0; i < MAX_CONFIGSTRINGS; i++) {
			/* CS_TILES and CS_POSITIONS can stretch over multiple configstrings,
			 * so don't send the middle parts again. */
			if (i > CS_TILES && i < CS_POSITIONS)
				continue;
			if (i > CS_POSITIONS && i < CS_MODELS)
				continue;

			const char* configString = SV_GetConfigString(i);
			if (!Q_strvalid(configString))
				continue;

			Com_DPrintf(DEBUG_SERVER, "sending configstring %d: %s\n", i, configString);

			dbuffer msg;
			NET_WriteByte(&msg, svc_configstring);
			NET_WriteShort(&msg, i);
			NET_WriteString(&msg, configString);
			/* enqueue and free msg */
			NET_WriteMsg(cl->stream, msg);
		}
	}

	SV_ClientCommand(cl, CL_PRECACHE "\n");
}

/**
 * @sa SV_Spawn_f
 */
static void SV_Begin_f (client_t* cl)
{
	bool began;

	Com_DPrintf(DEBUG_SERVER, "Begin() from %s\n", cl->name);

	/* could be abused to respawn or cause spam/other mod-specific problems */
	if (cl->state != cs_spawning) {
		Com_Printf("EXPLOIT: Illegal 'begin' from %s (already spawned), client dropped.\n", cl->name);
		SV_DropClient(cl, "Illegal begin\n");
		return;
	}

	/* call the game begin function */
	{
		ScopedMutex scopedMutex(svs.serverMutex);
		began = svs.ge->ClientBegin(*cl->player);
	}

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
static void SV_StartMatch_f (client_t* cl)
{
	Com_DPrintf(DEBUG_SERVER, "StartMatch() from %s\n", cl->name);

	if (cl->state != cs_spawned) {
		SV_DropClient(cl, "Invalid state\n");
		return;
	}

	{
		ScopedMutex scopedMutex(svs.serverMutex);
		svs.ge->ClientStartMatch(*cl->player);
	}

	Cbuf_InsertFromDefer();
}

/*============================================================================ */

/**
 * @brief The client is going to disconnect, so remove the connection immediately
 */
static void SV_Disconnect_f (client_t* cl)
{
	SV_DropClient(cl, "Disconnect\n");
}


/**
 * @brief Dumps the serverinfo info string
 */
static void SV_ShowServerinfo_f (client_t* cl)
{
	char info[MAX_INFO_STRING];
	Info_Print(Cvar_Serverinfo(info, sizeof(info)));
}


typedef struct {
	const char* name;
	void (*func) (client_t* client);
} ucmd_t;

static const ucmd_t ucmds[] = {
	/* auto issued */
	{NET_STATE_NEW, SV_New_f},
	{NET_STATE_BEGIN, SV_Begin_f},
	{NET_STATE_STARTMATCH, SV_StartMatch_f},

	{NET_STATE_DISCONNECT, SV_Disconnect_f},

	/* issued by hand at client consoles */
	{NET_STATE_INFO, SV_ShowServerinfo_f},

	{nullptr, nullptr}
};

/**
 * @sa SV_ExecuteClientMessage
 */
static void SV_ExecuteUserCommand (client_t* cl, const char* s)
{
	Cmd_TokenizeString(s, false, false);

	for (const ucmd_t* u = ucmds; u->name; u++)
		if (Q_streq(Cmd_Argv(0), u->name)) {
			Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: %s\n", s);
			u->func(cl);
			return;
		}

	if (Com_ServerState() == ss_game) {
		Com_DPrintf(DEBUG_SERVER, "SV_ExecuteUserCommand: client command: %s\n", s);
		ScopedMutex scopedMutex(svs.serverMutex);
		svs.ge->ClientCommand(*cl->player);
	}
}

/**
 * @brief The current net_message is parsed for the given client
 */
void SV_ExecuteClientMessage (client_t* cl, int cmd, dbuffer* msg)
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

	case clc_stringcmd: {
		char str[MAX_CLC_STRINGCMD];
		NET_ReadString(msg, str, sizeof(str));

		Com_DPrintf(DEBUG_SERVER, "stringcmd from client: %s\n", str);
		SV_ExecuteUserCommand(cl, str);

		if (cl->state == cs_free)
			return;			/* disconnect command */
		break;
	}

	case clc_action: {
		/* client actions are handled by the game module */
		ScopedMutex scopedMutex(svs.serverMutex);
		sv->messageBuffer = msg;
		svs.ge->ClientAction(*cl->player);
		sv->messageBuffer = nullptr;
		break;
	}

	case clc_endround: {
		/* player wants to end round */
		ScopedMutex scopedMutex(svs.serverMutex);
		sv->messageBuffer = msg;
		svs.ge->ClientEndRound(*cl->player);
		sv->messageBuffer = nullptr;
		break;
	}

	case clc_teaminfo: {
		/* player sends team info */
		/* actors spawn accordingly */
		ScopedMutex scopedMutex(svs.serverMutex);
		sv->messageBuffer = msg;
		svs.ge->ClientTeamInfo(*cl->player);
		sv->messageBuffer = nullptr;
		SV_SetClientState(cl, cs_spawned);
		break;
	}

	case clc_initactorstates: {
		/* player sends team info */
		/* actors spawn accordingly */
		ScopedMutex scopedMutex(svs.serverMutex);
		sv->messageBuffer = msg;
		svs.ge->ClientInitActorStates(*cl->player);
		sv->messageBuffer = nullptr;
		break;
	}
	}
}

server_state_t SV_GetServerState(void)
{
	return sv->state;
}

void SV_SetServerState(server_state_t state)
{
	sv->state = state;
}
