/**
 * @file mp_callbacks.c
 * @brief Serverlist menu callbacks for multiplayer
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "../menu/m_main.h"
#include "../menu/m_popup.h"
#include "mp_callbacks.h"
#include "mp_serverlist.h"
#include "mp_team.h"

teamData_t teamData;
static cvar_t *rcon_client_password;
static cvar_t *info_password;

void CL_Connect_f (void)
{
	const char *server;
	const char *serverport;

	if (!selectedServer && Cmd_Argc() != 2 && Cmd_Argc() != 3) {
		Com_Printf("Usage: %s <server> [<port>]\n", Cmd_Argv(0));
		return;
	}

	if (!chrDisplayList.num && !MP_LoadDefaultTeamMultiplayer()) {
		MN_Popup(_("Error"), _("Assemble a team first"));
		return;
	}

	if (Cvar_GetInteger("mn_server_need_password")) {
		MN_PushMenu("serverpassword", NULL);
		return;
	}

	/* if running a local server, kill it and reissue */
	SV_Shutdown("Server quit.", qfalse);
	CL_Disconnect();

	if (Cmd_Argc() == 2) {
		server = Cmd_Argv(1);
		serverport = DOUBLEQUOTE(PORT_SERVER);
	} else if (Cmd_Argc() == 3) {
		server = Cmd_Argv(1);
		serverport = Cmd_Argv(2);
	} else {
		assert(selectedServer);
		server = selectedServer->node;
		serverport = selectedServer->service;
	}
	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	Q_strncpyz(cls.serverport, serverport, sizeof(cls.serverport));

	CL_SetClientState(ca_connecting);

	MN_InitStack(NULL, "multiplayerInGame", qfalse, qfalse);
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

	if (cls.state < ca_connected) {
		Com_Printf("You are not connected to any server\n");
		return;
	}

	Com_sprintf(message, sizeof(message), "rcon %s %s",
		rcon_client_password->string, Cmd_Args());

	NET_OOB_Printf(cls.netStream, "%s", message);
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

		cls.connectTime = cls.realtime - 1500;

		CL_SetClientState(ca_connecting);
		Com_Printf("reconnecting...\n");
	} else
		Com_Printf("No server to reconnect to\n");
}


/**
 * @brief Multiplayer wait menu init function
 */
static void CL_WaitInit_f (void)
{
	static qboolean reconnect = qfalse;
	char buf[32];

	/* the server knows this already */
	if (!Com_ServerState()) {
		Cvar_SetValue("sv_maxteams", atoi(cl.configstrings[CS_MAXTEAMS]));
		Cvar_Set("mp_wait_init_show_force", "0");
	} else {
		Cvar_Set("mp_wait_init_show_force", "1");
	}
	Com_sprintf(buf, sizeof(buf), "%s/%s", cl.configstrings[CS_PLAYERCOUNT], cl.configstrings[CS_MAXCLIENTS]);
	Cvar_Set("mp_wait_init_players", buf);
	if (cl.configstrings[CS_NAME][0] == '\0') {
		if (!reconnect) {
			reconnect = qtrue;
			CL_Reconnect_f();
			MN_PopMenu(qfalse);
		} else {
			CL_Disconnect_f();
			MN_PopMenu(qfalse);
			MN_Popup(_("Error"), _("Server needs restarting - something went wrong"));
		}
	} else
		reconnect = qfalse;
}

/**
 * @brief Send the teaminfo string to server
 * @sa CL_ParseTeamInfoMessage
 * @sa SV_ConnectionlessPacket
 */
static void CL_SelectTeam_Init_f (void)
{
	/* reset menu text */
	MN_ResetData(TEXT_STANDARD);

	NET_OOB_Printf(cls.netStream, "teaminfo %i", PROTOCOL_VERSION);
	MN_RegisterText(TEXT_STANDARD, _("Select a free team or your coop team"));
}

/**
 * @brief Increase or decrease the teamnum
 * @sa CL_SelectTeam_Init_f
 */
static void CL_TeamNum_f (void)
{
	int max = 4;
	int i = cl_teamnum->integer;
	static char buf[MAX_STRING_CHARS];
	const int maxteamnum = Cvar_GetInteger("mn_maxteams");

	if (maxteamnum > 0)
		max = maxteamnum;

	cl_teamnum->modified = qfalse;

	if (i <= TEAM_CIVILIAN || i > teamData.maxteams) {
		Cvar_SetValue("cl_teamnum", TEAM_DEFAULT);
		i = TEAM_DEFAULT;
	}

	if (strncmp(Cmd_Argv(0), "teamnum_dec", 11)) {
		for (i--; i > TEAM_CIVILIAN; i--) {
			if (teamData.maxplayersperteam > teamData.teamCount[i]) {
				Cvar_SetValue("cl_teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				MN_RegisterText(TEXT_STANDARD, buf);
				break;
			} else {
				MN_RegisterText(TEXT_STANDARD, _("Team is already in use"));
				Com_DPrintf(DEBUG_CLIENT, "team %i is already in use: %i (max: %i)\n",
					i, teamData.teamCount[i], teamData.maxplayersperteam);
			}
		}
	} else {
		for (i++; i <= teamData.maxteams; i++) {
			if (teamData.maxplayersperteam > teamData.teamCount[i]) {
				Cvar_SetValue("cl_teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				MN_RegisterText(TEXT_STANDARD, buf);
				break;
			} else {
				MN_RegisterText(TEXT_STANDARD, _("Team is already in use"));
				Com_DPrintf(DEBUG_CLIENT, "team %i is already in use: %i (max: %i)\n",
					i, teamData.teamCount[i], teamData.maxplayersperteam);
			}
		}
	}

#if 0
	if (!teamnum->modified)
		MN_RegisterText(TEXT_STANDARD, _("Invalid or full team"));
#endif
	CL_SelectTeam_Init_f();
}

void MP_CallbacksInit (void)
{
	rcon_client_password = Cvar_Get("rcon_password", "", 0, "Remote console password");
	info_password = Cvar_Get("password", "", CVAR_USERINFO, NULL);
	Cmd_AddCommand("mp_selectteam_init", CL_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team");
	Cmd_AddCommand("mp_wait_init", CL_WaitInit_f, "Function that inits some nodes");
	Cmd_AddCommand("teamnum_dec", CL_TeamNum_f, "Decrease the prefered teamnum");
	Cmd_AddCommand("teamnum_inc", CL_TeamNum_f, "Increase the prefered teamnum");
	Cmd_AddCommand("saveteam", MP_SaveTeamMultiplayer_f, "Save a multiplayer team slot");
	Cmd_AddCommand("loadteam", MP_LoadTeamMultiplayer_f, "Load a multiplayer team slot");
	Cmd_AddCommand("team_comments", MP_MultiplayerTeamSlotComments_f, "Fills the multiplayer team selection menu with the team names");
	Cmd_AddCommand("mp_team_update", MP_UpdateMenuParameters_f, "");
	Cmd_AddCommand("mp_team_select", MP_TeamSelect_f, "");
}

void MP_CallbacksShutdown (void)
{
	Cmd_RemoveCommand("mp_selectteam_init");
	Cmd_RemoveCommand("mp_wait_init");
	Cmd_RemoveCommand("teamnum_dec");
	Cmd_RemoveCommand("teamnum_inc");
	Cmd_RemoveCommand("saveteam");
	Cmd_RemoveCommand("loadteam");
	Cmd_RemoveCommand("team_comments");
	Cmd_RemoveCommand("mp_team_update");
	Cmd_RemoveCommand("mp_team_select");
}
