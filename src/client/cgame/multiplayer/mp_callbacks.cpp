/**
 * @file
 * @brief Serverlist menu callbacks for multiplayer
 */

/*
Copyright (C) 2002-2015 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../ui/ui_data.h"
#include "mp_callbacks.h"
#include "mp_serverlist.h"
#include "../cl_game.h"

static const cgame_import_t* cgi;

teamData_t teamData;
cvar_t* rcon_client_password;
cvar_t* cl_maxsoldiersperteam;
cvar_t* cl_maxsoldiersperplayer;
cvar_t* cl_roundtimelimit;
static cvar_t* rcon_address;
static cvar_t* info_password;

static void GAME_MP_Connect_f (void)
{
	if (!selectedServer && cgi->Cmd_Argc() != 2 && cgi->Cmd_Argc() != 3) {
		cgi->Com_Printf("Usage: %s <server> [<port>]\n", cgi->Cmd_Argv(0));
		return;
	}

	char server[MAX_VAR];
	char serverport[16];
	if (cgi->Cmd_Argc() == 2) {
		Q_strncpyz(server, cgi->Cmd_Argv(1), sizeof(server));
		Q_strncpyz(serverport, DOUBLEQUOTE(PORT_SERVER), sizeof(serverport));
	} else if (cgi->Cmd_Argc() == 3) {
		Q_strncpyz(server, cgi->Cmd_Argv(1), sizeof(server));
		Q_strncpyz(serverport, cgi->Cmd_Argv(2), sizeof(serverport));
	} else {
		assert(selectedServer);
		Q_strncpyz(server, selectedServer->node, sizeof(server));
		Q_strncpyz(serverport, selectedServer->service, sizeof(serverport));
	}

	if (cgi->GAME_IsTeamEmpty() && !cgi->GAME_LoadDefaultTeam(true)) {
		cgi->UI_Popup(_("Error"), "%s", _("Assemble a team first"));
		return;
	}

	if (cgi->Cvar_GetInteger("mn_server_need_password")) {
		cgi->UI_PushWindow("serverpassword");
		return;
	}

	/* if running a local server, kill it and reissue */
	cgi->SV_Shutdown("Server quit.", false);
	cgi->CL_Disconnect();

	cgi->GAME_SetServerInfo(server, serverport);

	cgi->CL_SetClientState(ca_connecting);

	cgi->HUD_InitUI("missionoptions");
}

static void GAME_MP_RconCallback (struct net_stream* s)
{
	AutoPtr<dbuffer> buf(cgi->NET_ReadMsg(s));
	if (!buf) {
		cgi->NET_StreamFree(s);
		return;
	}
	const int cmd = cgi->NET_ReadByte(buf);
	if (cmd != svc_oob) {
		cgi->NET_StreamFree(s);
		return;
	}
	char commandBuf[8];
	cgi->NET_ReadStringLine(buf, commandBuf, sizeof(commandBuf));

	if (Q_streq(commandBuf, "print")) {
		char paramBuf[2048];
		cgi->NET_ReadString(buf, paramBuf, sizeof(paramBuf));
		cgi->Com_Printf("%s\n", paramBuf);
	}
	cgi->NET_StreamFree(s);
}

/**
 * @brief Sends an rcon command to the gameserver that the user is currently connected to,
 * or if this is not the case, the gameserver that is specified in the cvar rcon_address.
 * @return @c true if the command was send, @c false otherwise.
 */
bool GAME_MP_Rcon (const char* password, const char* command)
{
	if (Q_strnull(password)) {
		cgi->Com_Printf("You must set 'rcon_password' before issuing a rcon command.\n");
		return false;
	}

	if (cgi->CL_GetClientState() >= ca_connected) {
		cgi->NET_OOB_Printf2(SV_CMD_RCON " %s %s", password, command);
		return true;
	} else if (rcon_address->string) {
		const char* port;

		if (strstr(rcon_address->string, ":"))
			port = strstr(rcon_address->string, ":") + 1;
		else
			port = DOUBLEQUOTE(PORT_SERVER);

		struct net_stream* s = cgi->NET_Connect(rcon_address->string, port, nullptr);
		if (s) {
			cgi->NET_OOB_Printf(s, SV_CMD_RCON " %s %s", password, command);
			cgi->NET_StreamSetCallback(s, &GAME_MP_RconCallback);
			return true;
		}
	}

	cgi->Com_Printf("You are not connected to any server\n");
	return false;
}

/**
 * Send the rest of the command line over as
 * an unconnected command.
 */
static void GAME_MP_Rcon_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <command>\n", cgi->Cmd_Argv(0));
		return;
	}

	if (!rcon_client_password->string) {
		cgi->Com_Printf("You must set 'rcon_password' before issuing a rcon command.\n");
		return;
	}

	if (!GAME_MP_Rcon(rcon_client_password->string, cgi->Cmd_Args()))
		Com_Printf("Could not send the rcon command\n");
}

static void GAME_MP_StartGame_f (void)
{
	if (cgi->Com_ServerState())
		cgi->Cmd_ExecuteString("startgame");
	else
		cgi->Cmd_ExecuteString("rcon startgame");
}

/**
 * @brief Binding for disconnect console command
 */
static void GAME_MP_Disconnect_f (void)
{
	cgi->SV_ShutdownWhenEmpty();
	cgi->CL_Drop();
}

/**
 * @brief The server is changing levels
 */
static void GAME_MP_Reconnect_f (void)
{
	if (cgi->Com_ServerState())
		return;

	if (cgi->CL_GetClientState() >= ca_connecting) {
		cgi->Com_Printf("disconnecting...\n");
		cgi->CL_Disconnect();
	}

	cgi->CL_SetClientState(ca_connecting);
	cgi->Com_Printf("reconnecting...\n");
}

/**
 * @brief Send the SV_CMD_TEAMINFO command to server
 * @sa CL_ParseTeamInfoMessage
 * @sa SV_ConnectionlessPacket
 */
static void GAME_MP_SelectTeam_Init_f (void)
{
	/* reset menu text */
	cgi->UI_ResetData(TEXT_STANDARD);

	if (cgi->Com_ServerState())
		cgi->Cvar_Set("cl_admin", "1");
	else
		cgi->Cvar_Set("cl_admin", "0");

	cgi->NET_OOB_Printf2(SV_CMD_TEAMINFO " %i", PROTOCOL_VERSION);
	cgi->UI_RegisterText(TEXT_STANDARD, _("Select a free team or your coop team"));
}

static bool GAME_MP_SetTeamNum (int teamnum)
{
	if (teamData.maxPlayersPerTeam > teamData.teamCount[teamnum]) {
		static char buf[MAX_VAR];
		cgi->Cvar_SetValue("cl_teamnum", teamnum);
		Com_sprintf(buf, sizeof(buf), _("Current team: %i"), teamnum);
		cgi->UI_RegisterText(TEXT_STANDARD, buf);
		return true;
	}

	cgi->UI_RegisterText(TEXT_STANDARD, _("Team is already in use"));
	cgi->Com_DPrintf(DEBUG_CLIENT, "team %i is already in use: %i (max: %i)\n",
		teamnum, teamData.teamCount[teamnum], teamData.maxPlayersPerTeam);
	return false;
}

/**
 * @brief Increase or decrease the teamnum
 * @sa GAME_MP_SelectTeam_Init_f
 */
static void GAME_MP_TeamNum_f (void)
{
	int i = cgi->Cvar_GetInteger("cl_teamnum");

	if (i <= TEAM_CIVILIAN || i > teamData.maxteams) {
		cgi->Cvar_SetValue("cl_teamnum", TEAM_DEFAULT);
		i = TEAM_DEFAULT;
	}

	if (Q_streq(cgi->Cmd_Argv(0), "teamnum_dec")) {
		for (i--; i > TEAM_CIVILIAN; i--) {
			if (GAME_MP_SetTeamNum(i))
				break;
		}
	} else {
		for (i++; i <= teamData.maxteams; i++) {
			if (GAME_MP_SetTeamNum(i))
				break;
		}
	}

	GAME_MP_SelectTeam_Init_f();
}

/**
 * @brief Autocomplete function for some network functions
 * @sa Cmd_AddParamCompleteFunction
 * @todo Extend this for all the servers on the server browser list
 */
static int GAME_MP_CompleteNetworkAddress (const char* partial, const char** match)
{
	int n = 0;
	for (int i = 0; i != MAX_BOOKMARKS; ++i) {
		char const* const adrStr = cgi->Cvar_GetString(va("adr%i", i));
		if (adrStr[0] != '\0' && cgi->Cmd_GenericCompleteFunction(adrStr, partial, match)) {
			cgi->Com_Printf("%s\n", adrStr);
			++n;
		}
	}
	return n;
}

static void GAME_MP_InitUI_f (void)
{
	uiNode_t* gameTypes = nullptr;
	for (int i = 0; i < cgi->csi->numGTs; i++) {
		const gametype_t* gt = &cgi->csi->gts[i];
		cgi->UI_AddOption(&gameTypes, gt->id, _(gt->name), gt->id);
	}
	cgi->UI_RegisterOption(TEXT_LIST, gameTypes);
}

static const cmdList_t mpCallbacks[] = {
	{"mp_selectteam_init", GAME_MP_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team"},
	{"mp_init_ui", GAME_MP_InitUI_f, nullptr},
	{"teamnum_dec", GAME_MP_TeamNum_f, "Decrease the preferred teamnum"},
	{"teamnum_inc", GAME_MP_TeamNum_f, "Increase the preferred teamnum"},
	{"pingservers", GAME_MP_PingServers_f, "Ping all servers in local network to get the serverlist"},
	{"disconnect", GAME_MP_Disconnect_f, "Disconnect from the current server"},
	{"connect", GAME_MP_Connect_f, "Connect to given ip"},
	{"reconnect", GAME_MP_Reconnect_f, "Reconnect to last server"},
	{"rcon", GAME_MP_Rcon_f, "Execute a rcon command - see rcon_password"},
	{"cl_startgame", GAME_MP_StartGame_f, "Forces a gamestart if you are the admin"},
	{nullptr, nullptr, nullptr}
};
void GAME_MP_CallbacksInit (const cgame_import_t* import)
{
	cgi = import;
	rcon_client_password = cgi->Cvar_Get("rcon_password", "", 0, "Remote console password");
	rcon_address = cgi->Cvar_Get("rcon_address", "", 0, "Address of the host you would like to control via rcon");
	info_password = cgi->Cvar_Get("password", "", CVAR_USERINFO, nullptr);
	cl_maxsoldiersperteam = cgi->Cvar_Get("sv_maxsoldiersperteam", "4", CVAR_ARCHIVE | CVAR_SERVERINFO, "How many soldiers may one team have");
	cl_maxsoldiersperplayer = cgi->Cvar_Get("sv_maxsoldiersperplayer", DOUBLEQUOTE(MAX_ACTIVETEAM), CVAR_ARCHIVE | CVAR_SERVERINFO, "How many soldiers one player is able to control in a given team");
	cl_roundtimelimit = cgi->Cvar_Get("sv_roundtimelimit", "90", CVAR_ARCHIVE | CVAR_SERVERINFO, "Timelimit in seconds for multiplayer rounds");

	cgi->Cmd_TableAddList(mpCallbacks);
	cgi->Cmd_AddParamCompleteFunction("connect", GAME_MP_CompleteNetworkAddress);
	cgi->Cmd_AddParamCompleteFunction("rcon", GAME_MP_CompleteNetworkAddress);

	cl_maxsoldiersperteam->modified = false;
	cl_maxsoldiersperplayer->modified = false;
}

void GAME_MP_CallbacksShutdown (void)
{
	cgi->Cmd_TableRemoveList(mpCallbacks);

	cgi->Cvar_Delete("rcon_address");
}
