/**
 * @file mp_callbacks.c
 * @brief Serverlist menu callbacks for multiplayer
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#include "../client.h"
#include "../cl_team.h"
#include "../ui/ui_main.h"
#include "../ui/ui_popup.h"
#include "mp_callbacks.h"
#include "mp_serverlist.h"
#include "mp_team.h"

teamData_t teamData;
static cvar_t *rcon_client_password;
static cvar_t *rcon_address;
static cvar_t *info_password;

void CL_Connect_f (void)
{
	char server[MAX_VAR];
	char serverport[16];

	if (!selectedServer && Cmd_Argc() != 2 && Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <server> [<port>]\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 2) {
		Q_strncpyz(server, Cmd_Argv(1), sizeof(server));
		Q_strncpyz(serverport, DOUBLEQUOTE(PORT_SERVER), sizeof(serverport));
	} else if (Cmd_Argc() == 3) {
		Q_strncpyz(server, Cmd_Argv(1), sizeof(server));
		Q_strncpyz(serverport, Cmd_Argv(2), sizeof(serverport));
	} else {
		assert(selectedServer);
		Q_strncpyz(server, selectedServer->node, sizeof(server));
		Q_strncpyz(serverport, selectedServer->service, sizeof(serverport));
	}

	if (!chrDisplayList.num && !MP_LoadDefaultTeamMultiplayer()) {
		UI_Popup(_("Error"), _("Assemble a team first"));
		return;
	}

	if (Cvar_GetInteger("mn_server_need_password")) {
		UI_PushWindow("serverpassword", NULL);
		return;
	}

	/* if running a local server, kill it and reissue */
	SV_Shutdown("Server quit.", qfalse);
	CL_Disconnect();

	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	Q_strncpyz(cls.serverport, serverport, sizeof(cls.serverport));

	CL_SetClientState(ca_connecting);

	UI_InitStack(NULL, "multiplayerInGame", qfalse, qfalse);
}

static void CL_RconCallback (struct net_stream *s)
{
	struct dbuffer *buf = NET_ReadMsg(s);
	if (buf) {
		const int cmd = NET_ReadByte(buf);
		char str[8];
		NET_ReadStringLine(buf, str, sizeof(str));

		if (cmd == clc_oob && !strcmp(str, "print")) {
			char str[2048];
			NET_ReadString(buf, str, sizeof(str));
			Com_Printf("%s\n", str);
		}
	}
	NET_StreamFree(s);
}

/**
 * Send the rest of the command line over as
 * an unconnected command.
 */
void CL_Rcon_f (void)
{
	char message[MAX_STRING_CHARS];

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <command>\n", Cmd_Argv(0));
		return;
	}

	if (!rcon_client_password->string) {
		Com_Printf("You must set 'rcon_password' before issuing an rcon command.\n");
		return;
	}

	Com_sprintf(message, sizeof(message), "rcon %s %s",
		rcon_client_password->string, Cmd_Args());

	if (cls.state >= ca_connected) {
		NET_OOB_Printf(cls.netStream, "%s", message);
	} else if (rcon_address->string) {
		const char *port;
		struct net_stream *s;

		if (strstr(rcon_address->string, ":"))
			port = strstr(rcon_address->string, ":") + 1;
		else
			port = DOUBLEQUOTE(PORT_SERVER);

		s = NET_Connect(rcon_address->string, port);
		if (s) {
			NET_OOB_Printf(s, "%s", message);
			NET_StreamSetCallback(s, &CL_RconCallback);
		}
	} else {
		Com_Printf("You are not connected to any server\n");
	}
}

/**
 * @brief Binding for disconnect console command
 * @sa CL_Disconnect
 * @sa CL_Drop
 * @sa SV_ShutdownWhenEmpty
 */
void CL_Disconnect_f (void)
{
	SV_ShutdownWhenEmpty();
	CL_Drop();
}

/**
 * @brief The server is changing levels
 */
void CL_Reconnect_f (void)
{
	if (Com_ServerState())
		return;

	if (cls.servername[0]) {
		if (cls.state >= ca_connecting) {
			Com_Printf("disconnecting...\n");
			CL_Disconnect();
		}

		cls.connectTime = CL_Milliseconds() - 1500;

		CL_SetClientState(ca_connecting);
		Com_Printf("reconnecting...\n");
	} else
		Com_Printf("No server to reconnect to\n");
}

/**
 * @brief Send the teaminfo string to server
 * @sa CL_ParseTeamInfoMessage
 * @sa SV_ConnectionlessPacket
 */
static void CL_SelectTeam_Init_f (void)
{
	/* reset menu text */
	UI_ResetData(TEXT_STANDARD);

	if (Com_ServerState())
		Cvar_Set("cl_admin", "1");
	else
		Cvar_Set("cl_admin", "0");

	NET_OOB_Printf(cls.netStream, "teaminfo %i", PROTOCOL_VERSION);
	UI_RegisterText(TEXT_STANDARD, _("Select a free team or your coop team"));
}

/**
 * @brief Increase or decrease the teamnum
 * @sa CL_SelectTeam_Init_f
 */
static void CL_TeamNum_f (void)
{
	int i = cl_teamnum->integer;
	static char buf[MAX_STRING_CHARS];

	cl_teamnum->modified = qfalse;

	if (i <= TEAM_CIVILIAN || i > teamData.maxteams) {
		Cvar_SetValue("cl_teamnum", TEAM_DEFAULT);
		i = TEAM_DEFAULT;
	}

	if (strncmp(Cmd_Argv(0), "teamnum_dec", 11)) {
		for (i--; i > TEAM_CIVILIAN; i--) {
			if (teamData.maxPlayersPerTeam > teamData.teamCount[i]) {
				Cvar_SetValue("cl_teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				UI_RegisterText(TEXT_STANDARD, buf);
				break;
			} else {
				UI_RegisterText(TEXT_STANDARD, _("Team is already in use"));
				Com_DPrintf(DEBUG_CLIENT, "team %i is already in use: %i (max: %i)\n",
					i, teamData.teamCount[i], teamData.maxPlayersPerTeam);
			}
		}
	} else {
		for (i++; i <= teamData.maxteams; i++) {
			if (teamData.maxPlayersPerTeam > teamData.teamCount[i]) {
				Cvar_SetValue("cl_teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				UI_RegisterText(TEXT_STANDARD, buf);
				break;
			} else {
				UI_RegisterText(TEXT_STANDARD, _("Team is already in use"));
				Com_DPrintf(DEBUG_CLIENT, "team %i is already in use: %i (max: %i)\n",
					i, teamData.teamCount[i], teamData.maxPlayersPerTeam);
			}
		}
	}

#if 0
	if (!teamnum->modified)
		UI_RegisterText(TEXT_STANDARD, _("Invalid or full team"));
#endif
	CL_SelectTeam_Init_f();
}

/**
 * @brief Autocomplete function for some network functions
 * @sa Cmd_AddParamCompleteFunction
 * @todo Extend this for all the servers on the server browser list
 */
static int CL_CompleteNetworkAddress (const char *partial, const char **match)
{
	int i, matches = 0;
	const char *localMatch[MAX_COMPLETE];
	const size_t len = strlen(partial);
	if (!len) {
		/* list them all if there was no parameter given */
		for (i = 0; i < MAX_BOOKMARKS; i++) {
			const char *adrStr = Cvar_GetString(va("adr%i", i));
			if (adrStr[0] != '\0')
				Com_Printf("%s\n", adrStr);
		}
		return 0;
	}

	localMatch[matches] = NULL;

	/* search all matches and fill the localMatch array */
	for (i = 0; i < MAX_BOOKMARKS; i++) {
		const char *adrStr = Cvar_GetString(va("adr%i", i));
		if (adrStr[0] != '\0' && !strncmp(partial, adrStr, len)) {
			Com_Printf("%s\n", adrStr);
			localMatch[matches++] = adrStr;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

void MP_CallbacksInit (void)
{
	rcon_client_password = Cvar_Get("rcon_password", "", 0, "Remote console password");
	rcon_address = Cvar_Get("rcon_address", "", 0, "Address of the host you would like to control via rcon");
	info_password = Cvar_Get("password", "", CVAR_USERINFO, NULL);
	Cmd_AddCommand("mp_selectteam_init", CL_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team");
	Cmd_AddCommand("teamnum_dec", CL_TeamNum_f, "Decrease the prefered teamnum");
	Cmd_AddCommand("teamnum_inc", CL_TeamNum_f, "Increase the prefered teamnum");
	Cmd_AddCommand("saveteam", MP_SaveTeamMultiplayer_f, "Save a multiplayer team slot");
	Cmd_AddCommand("loadteam", MP_LoadTeamMultiplayer_f, "Load a multiplayer team slot");
	Cmd_AddCommand("team_comments", MP_MultiplayerTeamSlotComments_f, "Fills the multiplayer team selection menu with the team names");
	Cmd_AddCommand("mp_team_update", MP_UpdateMenuParameters_f, "");
	Cmd_AddCommand("mp_team_select", MP_TeamSelect_f, "");
	Cmd_AddCommand("pingservers", CL_PingServers_f, "Ping all servers in local network to get the serverlist");
	Cmd_AddCommand("disconnect", CL_Disconnect_f, "Disconnect from the current server");
	Cmd_AddCommand("connect", CL_Connect_f, "Connect to given ip");
	Cmd_AddParamCompleteFunction("connect", CL_CompleteNetworkAddress);
	Cmd_AddCommand("reconnect", CL_Reconnect_f, "Reconnect to last server");
	Cmd_AddCommand("rcon", CL_Rcon_f, "Execute a rcon command - see rcon_password");
	Cmd_AddParamCompleteFunction("rcon", CL_CompleteNetworkAddress);
	Cmd_AddCommand("mp_toggleactor", MP_ToggleActorForTeam_f, NULL);
	Cmd_AddCommand("mp_saveteamstate", MP_SaveTeamState_f, NULL);
	Cmd_AddCommand("mp_autoteam", MP_AutoTeam_f, "Assign initial multiplayer equipment to soldiers");
}

void MP_CallbacksShutdown (void)
{
	Cmd_RemoveCommand("mp_selectteam_init");
	Cmd_RemoveCommand("teamnum_dec");
	Cmd_RemoveCommand("teamnum_inc");
	Cmd_RemoveCommand("saveteam");
	Cmd_RemoveCommand("loadteam");
	Cmd_RemoveCommand("team_comments");
	Cmd_RemoveCommand("mp_team_update");
	Cmd_RemoveCommand("mp_team_select");
	Cmd_RemoveCommand("rcon");
	Cmd_RemoveCommand("pingservers");
	Cmd_RemoveCommand("disconnect");
	Cmd_RemoveCommand("connect");
	Cmd_RemoveCommand("reconnect");
	Cmd_RemoveCommand("mp_toggleactor");
	Cmd_RemoveCommand("mp_saveteamstate");
	Cmd_RemoveCommand("mp_autoteam");
}
