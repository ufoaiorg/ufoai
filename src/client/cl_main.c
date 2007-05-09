/**
 * @file cl_main.c
 * @brief Primary functions for the client. NB: The main() is system-secific and can currently be found in ports/.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_main.c
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

#include "client.h"
#include "cl_global.h"

cvar_t *masterserver_ip;
cvar_t *masterserver_port;

cvar_t *noudp;
cvar_t *noipx;

cvar_t *cl_isometric;
cvar_t *cl_stereo_separation;
cvar_t *cl_stereo;

cvar_t *rcon_client_password;
cvar_t *rcon_address;

cvar_t *cl_timeout;

cvar_t *cl_markactors;

cvar_t *cl_fps;
cvar_t *cl_shownet;
cvar_t *cl_show_tooltips;
cvar_t *cl_show_cursor_tooltips;

cvar_t *cl_paused;
cvar_t *cl_timedemo;

cvar_t *cl_aviForceDemo;
cvar_t *cl_aviFrameRate;
cvar_t *cl_aviMotionJpeg;

cvar_t *sensitivity;

cvar_t *cl_logevents;

cvar_t *cl_centerview;

cvar_t *cl_worldlevel;
cvar_t *cl_selected;

cvar_t *cl_3dmap;
cvar_t *gl_3dmapradius;

cvar_t *cl_numnames;

cvar_t *mn_serverlist;

cvar_t *mn_main;
cvar_t *mn_sequence;
cvar_t *mn_active;
cvar_t *mn_hud;
cvar_t *mn_lastsave;

cvar_t *difficulty;
cvar_t *cl_start_employees;
cvar_t *cl_initial_equipment;
cvar_t *cl_start_buildings;

cvar_t *confirm_actions;
cvar_t *confirm_movement;

cvar_t *cl_precachemenus;

/* userinfo */
cvar_t *info_password;
cvar_t *info_spectator;
cvar_t *name;
cvar_t *team;
cvar_t *equip;
cvar_t *teamnum;
cvar_t *campaign;
cvar_t *rate;
cvar_t *msg;

client_static_t cls;
client_state_t cl;

centity_t cl_entities[MAX_EDICTS];

static qboolean soldiersSpawned = qfalse;

typedef struct teamData_s {
	int teamCount[MAX_TEAMS];	/**< team counter - parsed from server data 'teaminfo' */
	qboolean teamplay;
	int maxteams;
	int maxplayersperteam;		/**< max players per team */
	char teamInfoText[MAX_MESSAGE_TEXT];
	qboolean parsed;
} teamData_t;

static teamData_t teamData;

static int precache_check;

static void CL_SpawnSoldiers_f(void);

/*====================================================================== */

/**
 * @brief adds the current command line as a clc_stringcmd to the client message.
 * things like action, turn, etc, are commands directed to the server,
 * so when they are typed in at the console, they will need to be forwarded.
 */
void Cmd_ForwardToServer (void)
{
	char *cmd;

	cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+') {
		Com_Printf("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	SZ_Print(&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1) {
		SZ_Print(&cls.netchan.message, " ");
		SZ_Print(&cls.netchan.message, Cmd_Args());
	}
}

/**
 * @brief Set or print some environment variables via console command
 * @sa Q_putenv
 */
static void CL_Env_f (void)
{
	int argc = Cmd_Argc();

	if (argc == 3) {
		Q_putenv(Cmd_Argv(1), Cmd_Argv(2));
	} else if (argc == 2) {
		char *env = getenv(Cmd_Argv(1));

		if (env)
			Com_Printf("%s=%s\n", Cmd_Argv(1), env);
		else
			Com_Printf("%s undefined\n", Cmd_Argv(1));
	}
}


/**
 * @brief
 */
static void CL_ForwardToServer_f (void)
{
	if (cls.state != ca_connected && cls.state != ca_active) {
		Com_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	/* don't forward the first argument */
	if (Cmd_Argc() > 1) {
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		SZ_Print(&cls.netchan.message, Cmd_Args());
	}
}


/**
 * @brief Allow server (singleplayer and multiplayer) to pause the game
 */
static void CL_Pause_f (void)
{
	/* never pause in multiplayer (as client - server is allowed) */
	if (!ccs.singleplayer || !Com_ServerState()) {
		Cvar_SetValue("paused", 0);
		return;
	}

	Cvar_SetValue("paused", !cl_paused->value);
}

/**
 * @brief
 */
static void CL_Quit_f (void)
{
	CL_Disconnect();
	Com_Quit();
}

/**
 * @brief Disconnects a multiplayer game if singleplayer is true and set ccs.singleplayer to true
 */
extern void CL_StartSingleplayer (qboolean singleplayer)
{
	if (singleplayer) {
		ccs.singleplayer = qtrue;
		if (Qcommon_ServerActive()) {
			/* shutdown server */
			SV_Shutdown("Server was killed.\n", qfalse);
		}
		if (cls.state >= ca_connecting) {
			Com_Printf("Disconnect from current server\n");
			CL_Disconnect();
		}
		Cvar_ForceSet("maxclients", "1");

		/* reset maxsoldiersperplayer and maxsoldiers(perteam) to default values */
		/* FIXME: these should probably default to something bigger */
		if (Cvar_VariableInteger("maxsoldiers") != 4)
			Cvar_SetValue("maxsoldiers", 4);
		if (Cvar_VariableInteger("maxsoldiersperplayer") != 8)
			Cvar_SetValue("maxsoldiersperplayer", 8);

		/* this is needed to let 'soldier_select 0' do
		   the right thing while we are on geoscape */
		sv_maxclients->modified = qtrue;
	} else {
		char *max_s, *max_spp;
		max_s = Cvar_VariableStringOld("maxsoldiers");
		max_spp = Cvar_VariableStringOld("maxsoldiersperplayer");

		/* restore old maxsoldiersperplayer and maxsoldiers cvars if values were previously set */
		if (strlen(max_s))
			Cvar_Set("maxsoldiers", max_s);
		if (strlen(max_spp))
			Cvar_Set("maxsoldiersperplayer", max_spp);

		ccs.singleplayer = qfalse;
	}
}

/**
 * @brief
 * @note Called after an ERR_DROP was thrown
 */
void CL_Drop (void)
{
	/* @todo: I hope this will work for all CL_Drop calls.
	   at least it is working when a map isn't found */
#if 1
	/* drop loading plaque unless this is the initial game start */
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque();	/* get rid of loading plaque */
#endif

	if (cls.state == ca_uninitialized)
		return;
	if (cls.state == ca_disconnected)
		return;

#if 0
	/* drop loading plaque unless this is the initial game start */
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque();	/* get rid of loading plaque */
#endif
	CL_Disconnect();
}


/**
 * @brief We have gotten a challenge from the server, so try and connect.
 */
void CL_SendConnectPacket (void)
{
	netadr_t adr;
	int port;

	if (!NET_StringToAdr(cls.servername, &adr)) {
		Com_Printf("Bad server address: %s\n", cls.servername);
		cls.connect_time = 0;
		return;
	}
	if (adr.port == 0)
		adr.port = (unsigned)BigShort(PORT_SERVER);

	port = Cvar_VariableInteger("qport");
	userinfo_modified = qfalse;

	cls.ufoPort = port;

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo());
}

/**
 * @brief Resend a connect message if the last one has timed out
 */
void CL_CheckForResend (void)
{
	netadr_t adr;

	/* if the local server is running and we aren't */
	/* then connect */
	if (cls.state == ca_disconnected && Com_ServerState()) {
		cls.state = ca_connecting;
		Q_strncpyz(cls.servername, "localhost", sizeof(cls.servername));
		/* we don't need a challenge on the localhost */
		CL_SendConnectPacket();
		return;
/*		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately */
	}

	/* resend if we haven't gotten a reply yet */
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr(cls.servername, &adr)) {
		Com_Printf("Bad server address: %s\n", cls.servername);
		cls.state = ca_disconnected;
		return;
	}
	if (adr.port == 0)
		adr.port = (unsigned)BigShort(PORT_SERVER);

	cls.connect_time = cls.realtime;	/* for retransmit requests */

	Com_Printf("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "getchallenge\n");
}

/**
 * @brief
 *
 * FIXME: Spectator needs no team
 */
static void CL_Connect_f (void)
{
	char *server;

	if (Cmd_Argc() != 2) {
		Com_Printf("usage: connect <server>\n");
		return;
	}

	if (!B_GetNumOnTeam()) {
		MN_Popup(_("Error"), _("Assemble a team first"));
		return;
	}

	/* if running a local server, kill it and reissue */
	if (Com_ServerState())
		SV_Shutdown("Server quit\n", qfalse);
	else
		CL_Disconnect();

	server = Cmd_Argv(1);

	/* allow remote */
	NET_Config(qtrue);

	/* FIXME: why a second time? */
	CL_Disconnect();

	cls.state = ca_connecting;

	/* everything should be reasearched for multiplayer matches */
/*	RS_MarkResearchedAll(); */

	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	/* CL_CheckForResend() will fire immediately */
	Cvar_Set("mn_main", "multiplayerInGame");
	cls.connect_time = -99999;
}


/**
 * @brief
 *
 * Send the rest of the command line over as
 * an unconnected command.
 */
static void CL_Rcon_f (void)
{
	char message[1024];
	int i;
	netadr_t to;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: %s <command>\n", Cmd_Argv(0));
		return;
	}

	if (!rcon_client_password->string) {
		Com_Printf("You must set 'rcon_password' before issuing an rcon command.\n");
		return;
	}

	message[0] = (char) 255; /* \xFF - for searching only */
	message[1] = (char) 255;
	message[2] = (char) 255;
	message[3] = (char) 255;
	message[4] = 0;

	/* allow remote */
	NET_Config(qtrue);

	Q_strcat(message, "rcon ", sizeof(message));

	Q_strcat(message, rcon_client_password->string, sizeof(message));
	Q_strcat(message, " ", sizeof(message));

	for (i = 1; i < Cmd_Argc(); i++) {
		Q_strcat(message, Cmd_Argv(i), sizeof(message));
		Q_strcat(message, " ", sizeof(message));
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else {
		if (!strlen(rcon_address->string)) {
			Com_Printf("You must either be connected, or set the 'rcon_address' cvar to issue rcon commands\n");
			return;
		}
		if (!NET_StringToAdr(rcon_address->string, &to)) {
			Com_Printf("Bad address: %s\n", rcon_address->string);
			return;
		}
		if (to.port == 0)
			to.port = (unsigned)BigShort(PORT_SERVER);
	}

	NET_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
}


/**
 * @brief
 * @sa CL_ParseServerData
 * @sa CL_Disconnect
 */
extern void CL_ClearState (void)
{
	S_StopAllSounds();
	CL_ClearEffects();
	CL_InitEvents();

	/* wipe the entire cl structure */
	memset(&cl, 0, sizeof(cl));
	memset(&cl_entities, 0, sizeof(cl_entities));
	cl.cam.zoom = 1.0;
	CalcFovX();

	numLEs = 0;
	numLMs = 0;
	numMPs = 0;
	numPtls = 0;

	SZ_Clear(&cls.netchan.message);
}

/**
 * @brief Sets the cls.state to ca_disconnected and informs the server
 * @sa CL_Disconnect_f
 * @sa CL_CloseAVI
 * @note Goes from a connected state to full screen console state
 * Sends a disconnect message to the server
 * This is also called on Com_Error, so it shouldn't cause any errors
 */
void CL_Disconnect (void)
{
	byte final[32];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->value) {
		int time;

		time = Sys_Milliseconds() - cl.timedemo_start;
		if (time > 0)
			Com_Printf("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames, time / 1000.0, cl.timedemo_frames * 1000.0 / time);
	}

	VectorClear(cl.refdef.blend);

	cls.connect_time = 0;

	/* send a disconnect message to the server */
	final[0] = clc_stringcmd;
	Q_strncpyz((char *) final + 1, "disconnect", sizeof(final) - 1);
	Netchan_Transmit(&cls.netchan, strlen((char *) final), final);
	/* FIXME: why 3 times? */
	Netchan_Transmit(&cls.netchan, strlen((char *) final), final);
	Netchan_Transmit(&cls.netchan, strlen((char *) final), final);

	CL_ClearState();

	/* Stop recording any video */
	if (CL_VideoRecording())
		CL_CloseAVI();

	cls.state = ca_disconnected;
}

/**
 * @brief Binding for disconnect console command
 * @sa CL_Disconnect
 * @sa CL_Drop
 * @sa SV_ShutdownWhenEmpty
 */
static void CL_Disconnect_f (void)
{
	SV_ShutdownWhenEmpty();
	CL_Drop();
}

/* it's dangerous to activate this */
/*#define ACTIVATE_PACKET_COMMAND*/
#ifdef ACTIVATE_PACKET_COMMAND
/**
 * @brief This function allows you to send network commands from commandline
 * @note This function is only for debugging and testing purposes
 * It is dangerous to leave this activated in final releases
 * packet [destination] [contents]
 * Contents allows \n escape character
 */
static void CL_Packet_f (void)
{
	char send[2048];

	int i, l;
	char *in, *out;
	netadr_t adr;

	if (Cmd_Argc() != 3) {
		Com_Printf("Usage: packet <destination> <contents>\n");
		return;
	}

	/* allow remote */
	NET_Config(qtrue);

	if (!NET_StringToAdr(Cmd_Argv(1), &adr)) {
		Com_Printf("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = (unsigned)BigShort(PORT_SERVER);

	in = Cmd_Argv(2);
	out = send + 4;
	/* FIXME: Should be unsigned, don't it */
	send[0] = send[1] = send[2] = send[3] = (char) 0xff;

	l = strlen(in);
	for (i = 0; i < l; i++) {
		if (in[i] == '\\' && in[i + 1] == 'n') {
			*out++ = '\n';
			i++;
		} else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket(NS_CLIENT, out - send, send, adr);
}
#endif

/**
 * @brief Just sent as a hint to the client that they should drop to full console
 */
static void CL_Changing_f (void)
{
	SCR_BeginLoadingPlaque();
	cls.state = ca_connected;	/* not active anymore, but not disconnected */
	Com_Printf("\nChanging map...\n");
}


/**
 * @brief
 *
 * The server is changing levels
 */
static void CL_Reconnect_f (void)
{
	S_StopAllSounds();
	if (cls.state == ca_connected) {
		Com_Printf("reconnecting...\n");
		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");
		return;
	}

	if (*cls.servername) {
		if (cls.state >= ca_connected) {
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1500;
		} else
			cls.connect_time = -99999;	/* fire immediately */

		cls.state = ca_connecting;
		Com_Printf("reconnecting...\n");
	}
}

typedef struct serverList_s
{
	netadr_t adr;
	qboolean pinged;
	char hostname[16];
	char mapname[16];
	char version[8];
	char gametype[8];
	qboolean dedicated;
	int maxclients;
	int clients;
	int serverListPos;
} serverList_t;

#define MAX_SERVERLIST 128
static char serverText[1024];
static int serverListLength = 0;
static int serverListPos = 0;
static serverList_t serverList[MAX_SERVERLIST];

/**
 * @brief Pings all servers in serverList
 * @sa CL_AddServerToList
 */
static void CL_PingServer (netadr_t* adr)
{
	Com_DPrintf ("pinging %s...\n", NET_AdrToString(*adr));
	Netchan_OutOfBandPrint (NS_CLIENT, *adr, va("info %i", PROTOCOL_VERSION));
}

/**
 * @brief Prints all the servers on the list to game console
 */
static void CL_PrintServerList_f (void)
{
	int i;

	Com_Printf("%i servers on the list\n", serverListLength);

	for (i = 0; i < serverListLength; i++) {
		Com_Printf("%02i: %s (pinged: %i)\n", i, NET_AdrToString(serverList[i].adr), serverList[i].pinged);
	}
}

typedef enum {
	SERVERLIST_SHOWALL,
	SERVERLIST_HIDEFULL,
	SERVERLIST_HIDEEMPTY
} serverListStatus_t;

/**
 * @brief Adds a server to the serverlist cache
 * @return false if it is no valid address or server already exists
 * @sa CL_ParseStatusMessage
 */
static int CL_AddServerToList (netadr_t* adr, char *msg)
{
	int i;

	/* check whether the port was set */
	if (!adr || !adr->port)
		return -1;

	/* check some server data */
	if (msg) {
		if (PROTOCOL_VERSION != atoi(Info_ValueForKey(msg, "protocol"))) {
			Com_DPrintf("CL_AddServerToList: Protocol mismatch\n");
			return -1;
		}
		if (Q_strcmp(UFO_VERSION, Info_ValueForKey(msg, "version"))) {
			Com_DPrintf("CL_AddServerToList: Version mismatch\n");
			/*return -1;*/
		}
		/* hide full servers */
		switch (mn_serverlist->integer) {
		case SERVERLIST_SHOWALL:
			break;
		case SERVERLIST_HIDEFULL:
			if (atoi(Info_ValueForKey(msg, "maxclients")) <= atoi(Info_ValueForKey(msg, "clients"))) {
				Com_DPrintf("CL_AddServerToList: Server is full - hide from list\n");
				return -1;
			}
			break;
		case SERVERLIST_HIDEEMPTY:
			if (!atoi(Info_ValueForKey(msg, "clients"))) {
				Com_DPrintf("CL_AddServerToList: Server is empty - hide from list\n");
				return -1;
			}
			break;
		}
	}

	for (i = 0; i < serverListLength; i++) {
		if (NET_CompareAdr(*adr, serverList[i].adr)) {
			/* mark this server as pinged */
			if (msg) {
				if (!serverList[i].pinged) {
					serverList[i].pinged = qtrue;
					Q_strncpyz(serverList[i].hostname,
						Info_ValueForKey(msg, "hostname"),
						sizeof(serverList[i].hostname));
					Q_strncpyz(serverList[i].version,
						Info_ValueForKey(msg, "version"),
						sizeof(serverList[i].version));
					Q_strncpyz(serverList[i].mapname,
						Info_ValueForKey(msg, "mapname"),
						sizeof(serverList[i].mapname));
					Q_strncpyz(serverList[i].gametype,
						Info_ValueForKey(msg, "gametype"),
						sizeof(serverList[i].gametype));
					serverList[i].dedicated = atoi(Info_ValueForKey(msg, "dedicated"));
					serverList[i].clients = atoi(Info_ValueForKey(msg, "clients"));
					serverList[i].maxclients = atoi(Info_ValueForKey(msg, "maxclients"));
					/* first time response - add it to the list */
					return i;
				}
				/* we don't want to ping again - because msg is already the response */
				return -1;
			} else
				/* already on the list */
				return -1;
		}
	}

	if (msg)
		Com_DPrintf("Warning: a response for a server that was not on the list before (normal for broadcast scans)\n");

	memset(&(serverList[serverListLength]), 0, sizeof(serverList_t));
	serverList[serverListLength].adr = *adr;
	/* increase the number of servers in the list now */
	CL_PingServer(adr);
	serverListLength++;

	return -1;
}

/**
 * @brief Handle the short reply from a ping
 * @sa CL_PingServers_f
 * @sa SVC_Info
 */
static void CL_ParseStatusMessage (void)
{
	int serverID = -1;
	char *s = MSG_ReadString(&net_message);
	char string[MAX_INFO_STRING];

	Com_DPrintf("CL_ParseStatusMessage: %s\n", s);

	/* do this even if the list is empty
	 * or there are too many servers on the list */
	menuText[TEXT_LIST] = serverText;

	if (serverListLength >= MAX_SERVERLIST)
		return;

	/* update the server string */
	serverID = CL_AddServerToList(&net_from, s);
	if (serverID != -1) {
		Com_sprintf(string, sizeof(string), "%s\t%s %s (%s)\t%i/%i\n",
			serverList[serverID].hostname,
			serverList[serverID].mapname,
			serverList[serverID].gametype,
			serverList[serverID].version,
			serverList[serverID].clients,
			serverList[serverID].maxclients);
		serverList[serverID].serverListPos = serverListPos;
		serverListPos++;
		Q_strcat(serverText, string, sizeof(serverText));
	}
}

/**
 * @brief Multiplayer wait menu init function
 */
static void CL_Wait_Init_f (void)
{
	/* the server knows this already */
	if (!Com_ServerState()) {
		Cvar_SetValue("sv_maxteams", atoi(cl.configstrings[CS_MAXTEAMS]));
	}
}

/**
 * @brief Team selection text
 *
 * This function fills the multiplayer_selectteam menu with content
 * @sa Netchan_OutOfBandPrint
 * @sa SV_TeamInfoString
 * @sa CL_SelectTeam_Init_f
 */
static void CL_ParseTeamInfoMessage (void)
{
	char *s = MSG_ReadString(&net_message);
	char *var = NULL;
	char *value = NULL;
	int cnt = 0, n;

	if (!s) {
		menuText[TEXT_LIST] = NULL;
		Com_DPrintf("CL_ParseTeamInfoMessage: No teaminfo string\n");
		return;
	}

	memset(&teamData, 0, sizeof(teamData_t));

#if 0
	Com_Printf("CL_ParseTeamInfoMessage: %s\n", s);
#endif

	value = s;
	var = strstr(value, "\n");
	*var++ = '\0';

	teamData.teamplay = atoi(value);

	value = var;
	var = strstr(var, "\n");
	*var++ = '\0';
	teamData.maxteams = atoi(value);

	value = var;
	var = strstr(var, "\n");
	*var++ = '\0';
	teamData.maxplayersperteam = atoi(value);

	s = var;
	if (s)
		Q_strncpyz(teamData.teamInfoText, s, sizeof(teamData.teamInfoText));

	while (s != NULL) {
		value = strstr(s, "\n");
		if (value)
			*value++ = '\0';
		else
			break;
		/* get teamnum */
		var = strstr(s, "\t");
		if (var)
			*var++ = '\0';

		n = atoi(s);
		if (n > 0 && n < MAX_TEAMS)
			teamData.teamCount[n]++;
		s = value;
		cnt++;
	};

	/* no players are connected atm */
	if (!cnt)
		Q_strcat(teamData.teamInfoText, _("No player connected\n"), sizeof(teamData.teamInfoText));

	Cvar_SetValue("mn_maxteams", teamData.maxteams);
	Cvar_SetValue("mn_maxplayersperteam", teamData.maxplayersperteam);

	menuText[TEXT_LIST] = teamData.teamInfoText;
	teamData.parsed = qtrue;

	/* spawn multi-player death match soldiers here */
	if (!ccs.singleplayer && baseCurrent && !teamData.teamplay)
		CL_SpawnSoldiers_f();
}

static char serverInfoText[MAX_MESSAGE_TEXT];
/**
 * @brief Serverbrowser text
 *
 * This function fills the network browser server information with text
 * @sa Netchan_OutOfBandPrint
 */
static void CL_ParseServerInfoMessage (void)
{
	char *s = MSG_ReadString(&net_message);
	char *var = NULL;
	char *value = NULL;

	if (!s)
		return;

	var = strstr(s, "\n");
	if (!var) {
		Com_Printf("CL_ParseServerInfoMessage: Invalid status response '%s'\n", s);
		return;
	}
	*var = '\0';
	Com_DPrintf("%s\n", s);
	Cvar_Set("mn_mappic", "maps/shots/na.jpg");
	Cvar_Set("mn_server_need_password", "0"); /* string */

	Com_sprintf(serverInfoText, sizeof(serverInfoText), _("IP\t%s\n\n"), NET_AdrToString(net_from));

	/* first char is slash */
	s++;
	do {
		/* var */
		var = s;
		s = strstr(s, "\\");
		if (!s)
			break;
		*s++ = '\0';

		/* value */
		value = s;
		s = strstr(s, "\\");
		/* last? */
		if (s)
			*s++ = '\0';

		if (!Q_strncmp(var, "mapname", 7)) {
			Cvar_ForceSet("mapname", value);
			Q_strcat(serverInfoText, va(_("Map:\t%s\n"), value), sizeof(serverInfoText));

			if (FS_CheckFile(va("pics/maps/shots/%s.jpg", value)) != -1)
				Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", value));
			else {
				value[strlen(value)-1] = '\0';
				if (FS_CheckFile(va("pics/maps/shots/%s.jpg", value)) != -1)
					Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", value));
			}

		} else if (!Q_strncmp(var, "version", 7))
			Q_strcat(serverInfoText, va(_("Version:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "hostname", 8))
			Q_strcat(serverInfoText, va(_("Servername:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "sv_enablemorale", 15))
			Q_strcat(serverInfoText, va(_("Moralestates:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "gametype", 8))
			Q_strcat(serverInfoText, va(_("Gametype:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "sv_roundtimelimit", 17))
			Q_strcat(serverInfoText, va(_("Roundtime:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "sv_teamplay", 11))
			Q_strcat(serverInfoText, va(_("Teamplay:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "maxplayers", 10))
			Q_strcat(serverInfoText, va(_("Max. players per team:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "sv_maxteams", 11))
			Q_strcat(serverInfoText, va(_("Max. teams allowed in this map:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "maxclients", 10))
			Q_strcat(serverInfoText, va(_("Max. clients:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "maxsoldiersperplayer", 20))
			Q_strcat(serverInfoText, va(_("Max. soldiers per player:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "maxsoldiers", 11))
			Q_strcat(serverInfoText, va(_("Max. soldiers per team:\t%s\n"), value), sizeof(serverInfoText));
		else if (!Q_strncmp(var, "needpass", 8)) {
			if (*value == '1') {
				Q_strcat(serverInfoText, va(_("Password protected:\t%s\n"), _("yes")), sizeof(serverInfoText));
				Cvar_Set("mn_server_need_password", "1"); /* string */
			} else {
				Q_strcat(serverInfoText, va(_("Password protected:\t%s\n"), _("no")), sizeof(serverInfoText));
			}
		}
#ifdef DEBUG
		else
			Q_strcat(serverInfoText, va("%s\t%s\n", var, value), sizeof(serverInfoText));
#endif
	} while (s != NULL);
	menuText[TEXT_STANDARD] = serverInfoText;
	MN_PushMenu("serverinfo");
}

/**
 * @brief Masterserver server list
 * @sa Netchan_OutOfBandPrint
 * @sa CL_ConnectionlessPacket
 */
static void CL_ParseMasterServerResponse (void)
{
	byte* buffptr;
	byte* buffend;
	byte ip[4];
	unsigned short port;
	netadr_t adr;
	char adrstring[MAX_VAR];

	buffptr = net_message.data + 12;
	buffend = buffptr + net_message.cursize;

	while (buffptr+1 < buffend) {
		/* parse the ip */
		ip[0] = *buffptr++;
		ip[1] = *buffptr++;
		ip[2] = *buffptr++;
		ip[3] = *buffptr++;

		/* parse out port */
		port = (*buffptr++)<<8;
		port += *buffptr++;
		Com_sprintf(adrstring, sizeof(adrstring), "%d.%d.%d.%d:%d", ip[0], ip[1], ip[2], ip[3], port);
		if (!NET_StringToAdr(adrstring, &adr)) {
			Com_Printf("Invalid masterserver response '%s'\n", adrstring);
			break;
		}
		if (!adr.port)
			break;
		Com_DPrintf("server: %s\n", adrstring);
		CL_AddServerToList(&adr, NULL);
	}
	/* end of stream */
	net_message.readcount = net_message.cursize;
}

/**
 * @brief
 *
 * called via server_connect
 * FIXME: Spectator needs no team
 */
static void CL_ServerConnect_f (void)
{
	char *ip = Cvar_VariableString("mn_server_ip");

	/* @todo: if we are in multiplayer auto generate a team */
	if (!B_GetNumOnTeam()) {
		MN_Popup(_("Error"), _("Assemble a team first"));
		return;
	}

	if (Cvar_VariableInteger("mn_server_need_password") && !*info_password->string) {
		MN_PushMenu("serverpassword");
		return;
	}

	if (ip) {
		Com_DPrintf("CL_ServerConnect_f: connect to %s\n", ip);
		Cbuf_AddText(va("connect %s\n", ip));
	}
}

/**
 * @brief Add a new bookmark
 *
 * bookmarks are saved in cvar adr[0-15]
 */
static void CL_BookmarkAdd_f (void)
{
	int i;
	char *bookmark = NULL;
	char *newBookmark = NULL;
	netadr_t adr;

	if (Cmd_Argc() < 2) {
		newBookmark = Cvar_VariableString("mn_server_ip");
		if (!newBookmark) {
			Com_Printf("usage: bookmark_add <ip>\n");
			return;
		}
	} else
		newBookmark = Cmd_Argv(1);

	if (!NET_StringToAdr(newBookmark, &adr)) {
		Com_Printf("CL_BookmarkAdd_f: Invalid address %s\n", newBookmark);
		return;
	}

	for (i = 0; i < 16; i++) {
		bookmark = Cvar_VariableString(va("adr%i", i));
		if (!bookmark || !NET_StringToAdr(bookmark, &adr)) {
			Cvar_Set(va("adr%i", i), newBookmark);
			return;
		}
	}
	/* bookmarks are full - overwrite the first entry */
	MN_Popup(_("Notice"), _("All bookmark slots are used - please removed unused entries and repeat this step"));
}

/**
 * @brief
 * @sa CL_ParseServerInfoMessage
 */
void CL_BookmarkListClick_f (void)
{
	int num;
	char *bookmark = NULL;
	netadr_t adr;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: bookmarks_click <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));
	bookmark = Cvar_VariableString(va("adr%i", num));

	if (bookmark) {
		if (!NET_StringToAdr(bookmark, &adr)) {
			Com_Printf("Bad address: %s\n", bookmark);
			return;
		}
		if (adr.port == 0)
			adr.port = (unsigned)BigShort(PORT_SERVER);

		Cvar_Set("mn_server_ip", bookmark);
		Netchan_OutOfBandPrint(NS_CLIENT, adr, "status %i", PROTOCOL_VERSION);
	}
}

/**
 * @brief
 */
static void CL_ServerInfo_f (void)
{
	char ip[MAX_VAR];
	netadr_t adr;

	if (Cmd_Argc() < 2) {
		Com_DPrintf("usage: server_info <ip>\n");
		Q_strncpyz(ip, Cvar_VariableString("mn_server_ip"), sizeof(ip));
		Com_Printf("ip: %s\n", ip);
	} else {
		Q_strncpyz(ip, Cmd_Argv(1), sizeof(ip));
		Cvar_Set("mn_server_ip", ip);
	}
	if (!NET_StringToAdr(ip, &adr)) {
		Com_Printf("Bad address: %s\n", ip);
		return;
	}

	if (adr.port == 0)
		adr.port = (unsigned)BigShort(PORT_SERVER);

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "status %i", PROTOCOL_VERSION);
}

/**
 * @brief Callback for bookmark nodes in multiplayer menu (mp_bookmarks)
 * @sa CL_ParseServerInfoMessage
 */
static void CL_ServerListClick_f (void)
{
	int num, i;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: servers_click <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));

	menuText[TEXT_STANDARD] = serverInfoText;
	if (num >= 0 && num < serverListLength)
		for (i = 0; i < serverListLength; i++)
			if (serverList[i].pinged && serverList[i].serverListPos == num) {
				/* found the server - grab the infos for this server */
				Netchan_OutOfBandPrint(NS_CLIENT, serverList[i].adr, "status %i", PROTOCOL_VERSION);
				Cvar_Set("mn_server_ip", NET_AdrToString(serverList[i].adr));
				return;
			}
}

/**
 * @brief Send the teaminfo string to server
 * @sa CL_ParseTeamInfoMessage
 */
static void CL_SelectTeam_Init_f (void)
{
	netadr_t adr;

	/* reset menu text */
	menuText[TEXT_STANDARD] = NULL;

	if (!NET_StringToAdr(cls.servername, &adr)) {
		Com_Printf("CL_SelectTeam_Init_f: Invalid servername '%s'\n", cls.servername);
		return;
	}
	if (!adr.port)
		adr.port = (unsigned)BigShort(PORT_SERVER);

	Netchan_OutOfBandPrint(NS_CLIENT, adr, "teaminfo %i", PROTOCOL_VERSION);
	menuText[TEXT_STANDARD] = _("Select a free team or your coop team");
}

/**< this is true if pingservers was already executed */
static qboolean serversAlreadyQueried = qfalse;

static int lastServerQuery = 0;

/* ms until the server query timed out */
#define SERVERQUERYTIMEOUT 40000

/**
 * @brief The first function called when entering the multiplayer menu, then CL_Frame takes over
 * @sa CL_ParseStatusMessage
 * @note Use a parameter for pingservers to update the current serverlist
 */
static void CL_PingServers_f (void)
{
	netadr_t adr;
	int i;
	char name[6];
	char *adrstring;

	/* refresh the list */
	if (Cmd_Argc() == 2) {
		/* reset current list */
		serverText[0] = 0;
		serverListPos = 0;
		serverListLength = 0;
		serversAlreadyQueried = qfalse;
		memset(serverList, 0, sizeof(serverList_t) * MAX_SERVERLIST);
	}

/*	menuText[TEXT_STANDARD] = NULL;*/
	menuText[TEXT_LIST] = serverText;

	NET_Config(qtrue);			/* allow remote */

	/* send a broadcast packet */
	Com_DPrintf("pinging broadcast...\n");

	if (!noudp->value) {
		adr.type = NA_BROADCAST;
		adr.port = (unsigned)BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint(NS_CLIENT, adr, "info %i", PROTOCOL_VERSION);
	}

	if (!noipx->value) {
		adr.type = NA_BROADCAST_IPX;
		adr.port = (unsigned)BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint(NS_CLIENT, adr, "info %i", PROTOCOL_VERSION);
	}

	for (i = 0; i < 16; i++) {
		Com_sprintf(name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString(name);
		if (!adrstring || !adrstring[0])
			continue;

		if (!NET_StringToAdr(adrstring, &adr)) {
			Com_Printf("Bad address: %s\n", adrstring);
			continue;
		}

		if (!adr.port)
			adr.port = (unsigned)BigShort(PORT_SERVER);
		CL_AddServerToList(&adr, NULL);
	}

	/* don't query the masterservers with every call */
	if (serversAlreadyQueried) {
		menuText[TEXT_LIST] = serverText;
		if (lastServerQuery + SERVERQUERYTIMEOUT > Sys_Milliseconds())
			return;
	} else
		serversAlreadyQueried = qtrue;

	lastServerQuery = Sys_Milliseconds();

	/* query master server? */
	/* @todo: Cache this to save bandwidth */
	if (!noudp->value && (Cmd_Argc() == 2 || Q_strcmp(Cmd_Argv(1), "local"))) {
		adr.port = (unsigned)BigShort(masterserver_port->integer);
		if (NET_StringToAdr(masterserver_ip->string, &adr)) {
			if (!adr.port)
				adr.port = (unsigned)BigShort(masterserver_port->integer);
			adr.type = NA_IP;
			Com_DPrintf("Send master server query request to '%s:%s'\n", masterserver_ip->string, masterserver_port->string);
			Netchan_OutOfBandPrint (NS_CLIENT, adr, "getservers");
		}
	}
}


/**
 * @brief Responses to broadcasts, etc
 * @sa CL_ReadPackets
 * @sa CL_Frame
 */
static void CL_ConnectionlessPacket (void)
{
	char *s;
	char *c;
	int i;

	MSG_BeginReading(&net_message);
	MSG_ReadLong(&net_message);	/* skip the -1 */

	s = MSG_ReadStringLine(&net_message);

	Cmd_TokenizeString(s, qfalse);

	c = Cmd_Argv(0);

	Com_DPrintf("%s: %s\n", NET_AdrToString(net_from), c);

	/* server connection */
	if (!Q_strncmp(c, "client_connect", 13)) {
		char *p;
		for (i = 1; i < Cmd_Argc(); i++) {
			p = Cmd_Argv(i);
			if (!Q_strncmp(p, "dlserver=", 9)) {
#ifdef USE_CURL
				p += 9;
				Com_sprintf(cls.downloadReferer, sizeof(cls.downloadReferer), "ufo://%s", buff);
				CL_SetHTTPServer(p);
				if (cls.downloadServer[0])
					Com_Printf("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
#else
				Com_Printf("HTTP downloading supported by server but this client was built without USE_CURL, bad luck.\n");
#endif
			}
		}
		if (cls.state == ca_connected) {
			Com_Printf("Dup connect received. Ignored.\n");
			return;
		}
		Netchan_Setup(NS_CLIENT, &cls.netchan, net_from, cls.ufoPort);
		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");
		cls.state = ca_connected;
		return;
	}

	/* server responding to a status broadcast */
	if (!Q_strncmp(c, "info", 4)) {
		CL_ParseStatusMessage();
		return;
	}

	/* remote command from gui front end */
	if (!Q_strncmp(c, "cmd", 3)) {
		if (!NET_IsLocalAddress(net_from)) {
			Com_Printf("Command packet from remote host. Ignored.\n");
			return;
		}
		Sys_AppActivate();
		s = MSG_ReadString(&net_message);
		Cbuf_AddText(s);
		Cbuf_AddText("\n");
		return;
	}

	/* print command from somewhere */
	if (!Q_strncmp(c, "print", 5)) {
		CL_ParseServerInfoMessage();
		return;
	}

	/* teaminfo command */
	if (!Q_strncmp(c, "teaminfo", 8)) {
		CL_ParseTeamInfoMessage();
		return;
	}

	/* ping from somewhere */
	if (!Q_strncmp(c, "ping", 4)) {
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "ack");
		return;
	}

	/* serverlist from masterserver */
	if (!Q_strncmp(c, "servers", 7)) {
		CL_ParseMasterServerResponse();
		return;
	}

	/* challenge from the server we are connecting to */
	if (!Q_strncmp(c, "challenge", 9)) {
		if (cls.state != ca_connecting) {
			Com_Printf("Dup challenge received.  Ignored.\n");
			return;
		}
		cls.challenge = atoi(Cmd_Argv(1));
		CL_SendConnectPacket();
		return;
	}

	/* echo request from server */
	if (!Q_strncmp(c, "echo", 4)) {
		Netchan_OutOfBandPrint(NS_CLIENT, net_from, "%s", Cmd_Argv(1));
		return;
	}

	Com_Printf("Unknown command.\n");
}

#if 0
/**
 * @brief A vain attempt to help bad TCP stacks that cause problems when they overflow
 */
void CL_DumpPackets (void)
{
	while (NET_GetPacket(NS_CLIENT, &net_from, &net_message))
		Com_Printf("dumnping a packet\n");
}
#endif

/**
 * @brief
 * @sa CL_ConnectionlessPacket
 * @sa CL_Frame
 * @sa CL_ParseServerMessage
 */
static void CL_ReadPackets (void)
{
	int j;
	while ((j = NET_GetPacket(NS_CLIENT, &net_from, &net_message)) != 0) {
		/* icmp ignore cvar */
		if (j == -2)
			continue;
		/* remote command packet */
		if (*(int *) net_message.data == -1) {
			CL_ConnectionlessPacket();
			continue;
		}

		/* dump it if not connected */
		if (cls.state == ca_disconnected || cls.state == ca_connecting)
			continue;

		if (net_message.cursize < 8) {
			Com_Printf("%s: Runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		/* packet from server */
		if (!NET_CompareAdr(net_from, cls.netchan.remote_address)) {
			Com_DPrintf("%s:sequenced packet without connection\n", NET_AdrToString(net_from));
			continue;
		}
		/* wasn't accepted for some reason */
		if (!Netchan_Process(&cls.netchan, &net_message))
			continue;
		CL_ParseServerMessage();
	}

	/* check timeout */
	if (cls.state >= ca_connected && cls.realtime - cls.netchan.last_received > cl_timeout->value * 1000) {
		/* timeoutcount saves debugger */
		if (++cl.timeoutcount > 5) {
			MN_Popup(_("Network error"), _("Connection timed out - server is no longer reachable"));
			Com_Printf("\nServer connection timed out.\n");
			CL_Disconnect();
			return;
		}
	} else
		cl.timeoutcount = 0;
}


/**
 * @brief
 */
static void CL_Userinfo_f (void)
{
	Com_Printf("User info settings:\n");
	Info_Print(Cvar_Userinfo());
}

/**
 * @brief Restart the sound subsystem so it can pick up new parameters and flush all sounds
 * @sa S_Shutdown
 * @sa S_Init
 * @sa CL_RegisterSounds
 */
extern void CL_Snd_Restart_f (void)
{
	S_Shutdown();
	S_Init();
	CL_RegisterSounds();
}

/**
 * @brief Increase or decrease the teamnum
 * @sa CL_SelectTeam_Init_f
 * @todo: If no team is free - change to spectator
 */
static void CL_TeamNum_f (void)
{
	int max = 4;
	int maxteamnum = 0;
	int i = teamnum->integer;
	static char buf[MAX_STRING_CHARS];

	maxteamnum = Cvar_VariableInteger("mn_maxteams");

	if (maxteamnum > 0)
		max = maxteamnum;

	teamnum->modified = qfalse;

	if (i <= TEAM_CIVILIAN || i > teamData.maxteams) {
		Cvar_SetValue("teamnum", DEFAULT_TEAMNUM);
		i = DEFAULT_TEAMNUM;
	}

	if (Q_strncmp(Cmd_Argv(0), "teamnum_dec", 11)) {
		for (i--; i > TEAM_CIVILIAN; i--) {
			if (teamData.maxplayersperteam > teamData.teamCount[i]) {
				Cvar_SetValue("teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				menuText[TEXT_STANDARD] = buf;
				break;
			} else {
				menuText[TEXT_STANDARD] = _("Team is already in use");
#if DEBUG
				Com_DPrintf("team %i: %i (max: %i)\n", i, teamData.teamCount[i], teamData.maxplayersperteam);
#endif
			}
		}
	} else {
		for (i++; i <= teamData.maxteams; i++) {
			if (teamData.maxplayersperteam > teamData.teamCount[i]) {
				Cvar_SetValue("teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				menuText[TEXT_STANDARD] = buf;
				break;
			} else {
				menuText[TEXT_STANDARD] = _("Team is already in use");
#if DEBUG
				Com_DPrintf("team %i: %i (max: %i)\n", i, teamData.teamCount[i], teamData.maxplayersperteam);
#endif
			}
		}
	}

#if 0
	if (!teamnum->modified)
		menuText[TEXT_STANDARD] = _("Invalid or full team");
#endif
	CL_SelectTeam_Init_f();
}

static int spawnCountFromServer = -1;
/**
 * @brief Send the clc_teaminfo command to server
 * @sa CL_SendCurTeamInfo
 */
static void CL_SpawnSoldiers_f (void)
{
	int n = teamnum->integer;
	int amount = 0;

	if (!ccs.singleplayer && baseCurrent && !teamData.parsed) {
		Com_Printf("CL_SpawnSoldiers_f: teaminfo unparsed\n");
		return;
	}

	if (soldiersSpawned) {
		Com_Printf("CL_SpawnSoldiers_f: Soldiers are already spawned\n");
		return;
	}

	if (!ccs.singleplayer && baseCurrent) {
		/* we are already connected and in this list */
		if (n <= TEAM_CIVILIAN || teamData.maxplayersperteam < teamData.teamCount[n]) {
			menuText[TEXT_STANDARD] = _("Invalid or full team");
			Com_Printf("CL_SpawnSoldiers_f: Invalid or full team %i\n"
				"  maxplayers per team: %i - players on team: %i",
				n, teamData.maxplayersperteam, teamData.teamCount[n]);
			return;
		}
	}

	/* maybe we start the map directly from commandline for testing */
	if (baseCurrent) {
		amount = B_GetNumOnTeam();
		if (amount <= 0) {
			Com_DPrintf("CL_SpawnSoldiers_f: Error - B_GetNumOnTeam returned value smaller than zero - %i\n", amount);
		} else {
			/* send team info */
			CL_SendCurTeamInfo(&cls.netchan.message, baseCurrent->curTeam, amount);
		}
	}

	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	MSG_WriteString(&cls.netchan.message, va("spawn %i\n", spawnCountFromServer));

	soldiersSpawned = qtrue;

	/* spawn immediately if in single-player, otherwise wait for other players to join */
	if (ccs.singleplayer) {
		/* activate hud */
		MN_PushMenu(mn_hud->string);
		Cvar_Set("mn_active", mn_hud->string);
	} else {
		MN_PushMenu("multiplayer_wait");
	}
}

/**
 * @brief
 */
void CL_RequestNextDownload (void)
{
	unsigned map_checksum = 0;
	unsigned ufoScript_checksum = 0;

	if (cls.state != ca_connected)
		return;

	if (precache_check == CS_MODELS) { /* confirm map */
		if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
			return; /* started a download */
		precache_check = CS_MODELS + 1;
	}

#ifdef USE_CURL
	/* map might still be downloading? */
	if (CL_PendingHTTPDownloads())
		return;
#endif

	/* for singleplayer game this is already loaded in our local server */
	/* and if we are the server we don't have to reload the map here, too */
	if (!Com_ServerState()) {
		/* activate the map loading screen for multiplayer, too */
		SCR_BeginLoadingPlaque();

		CM_LoadMap(cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS], &map_checksum);
		if (!*cl.configstrings[CS_VERSION] || !*cl.configstrings[CS_MAPCHECKSUM] || !*cl.configstrings[CS_UFOCHECKSUM]) {
			Com_sprintf(popupText, sizeof(popupText), _("Local game version (%s) differs from the servers"), UFO_VERSION);
			MN_Popup(_("Error"), popupText);
			Com_Error(ERR_DROP, "Local game version (%s) differs from the servers", UFO_VERSION);
			return;
		/* checksum doesn't match with the one the server gave us via configstring */
		} else if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM])) {
			MN_Popup(_("Error"), _("Local map version differs from server"));
			Com_Error(ERR_DROP, "Local map version differs from server: %u != '%s'\n",
				map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		/* checksum doesn't match with the one the server gave us via configstring */
		} else if (atoi(cl.configstrings[CS_UFOCHECKSUM]) && ufoScript_checksum != atoi(cl.configstrings[CS_UFOCHECKSUM])) {
			MN_Popup(_("Error"), _("You are using modified ufo script files - you won't be able to connect to the server"));
			Com_Error(ERR_DROP, "You are using modified ufo script files - you won't be able to connect to the server: %u != '%s'\n",
				ufoScript_checksum, cl.configstrings[CS_UFOCHECKSUM]);
			return;
		} else if (Q_strncmp(UFO_VERSION, cl.configstrings[CS_VERSION], sizeof(UFO_VERSION))) {
			Com_sprintf(popupText, sizeof(popupText), _("Local game version (%s) differs from the servers (%s)"), UFO_VERSION, cl.configstrings[CS_VERSION]);
			MN_Popup(_("Error"), popupText);
			Com_Error(ERR_DROP, "Local game version (%s) differs from the servers (%s)", UFO_VERSION, cl.configstrings[CS_VERSION]);
			return;
		}
	}

	CL_RegisterSounds();
	CL_PrepRefresh();

	soldiersSpawned = qfalse;
	spawnCountFromServer = atoi(Cmd_Argv(1));

	/* send begin */
	/* this will activate the render process (see client state ca_active) */
	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	/* see CL_StartGame */
	MSG_WriteString(&cls.netchan.message, va("begin %i\n", spawnCountFromServer));

	/* for singleplayer the soldiers get spawned here */
	if (ccs.singleplayer || !baseCurrent)
		CL_SpawnSoldiers_f();
}


/**
 * @brief The server will send this command right before allowing the client into the server
 * @sa CL_StartGame
 * @todo recheck the checksum server side
 */
static void CL_Precache_f (void)
{
	precache_check = CS_MODELS;

	/* stop sound */
	S_StopAllSounds();
	CL_RequestNextDownload();
}

/**
 * @brief Init function for clients - called after menu was inited and ufo-scripts were parsed
 * @sa Qcommon_Init
 */
extern void CL_InitAfter (void)
{
	/* this will init some more employee stuff */
	E_Init();

	/* init some production menu nodes */
	PR_Init();

	/* link for faster access */
	MN_LinkMenuModels();
}

/**
 * @brief Called at client startup
 * @note not called for dedicated servers
 * parses all *.ufos that are needed for single- and multiplayer
 * @sa Com_ParseScripts
 * @sa CL_ParseScriptSecond
 * @sa CL_ParseScriptFirst
 * @note Nothing here should depends on items, equipments, actors and all other
 * entities that are parsed in Com_ParseScripts (because maybe items are not parsed
 * but e.g. techs would need those parsed items - thus we have to parse e.g. techs
 * at a later stage)
 */
extern void CL_ParseClientData (const char *type, const char *name, char **text)
{
	if (!Q_strncmp(type, "shader", 6))
		CL_ParseShaders(name, text);
	else if (!Q_strncmp(type, "font", 4))
		CL_ParseFont(name, text);
	else if (!Q_strncmp(type, "tutorial", 8))
		MN_ParseTutorials(name, text);
	else if (!Q_strncmp(type, "menu_model", 10))
		MN_ParseMenuModel(name, text);
	else if (!Q_strncmp(type, "menu", 4))
		MN_ParseMenu(name, text);
	else if (!Q_strncmp(type, "particle", 8))
		CL_ParseParticle(name, text);
	else if (!Q_strncmp(type, "sequence", 8))
		CL_ParseSequence(name, text);
	else if (!Q_strncmp(type, "aircraft", 8))
		AIR_ParseAircraft(name, text);
	else if (!Q_strncmp(type, "craftitem", 8))
		AII_ParseAircraftItem(name, text);
	else if (!Q_strncmp(type, "campaign", 8))
		CL_ParseCampaign(name, text);
	else if (!Q_strncmp(type, "ugv", 3))
		CL_ParseUGVs(name, text);
}

/**
 * @brief
 *
 * parsed if we are no dedicated server
 * first stage parses all the main data into their struct
 * see CL_ParseScriptSecond for more details about parsing stages
 * @sa Com_ParseScripts
 * @sa CL_ParseScriptSecond
 * @note make sure that the client hunk was cleared - otherwise it may overflow
 */
static void CL_ParseScriptFirst (const char *type, char *name, char **text)
{
	/* check for client interpretable scripts */
	if (!Q_strncmp(type, "mission", 7))
		CL_ParseMission(name, text);
	else if (!Q_strncmp(type, "up_chapters", 11))
		UP_ParseUpChapters(name, text);
	else if (!Q_strncmp(type, "building", 8))
		B_ParseBuildings(name, text, qfalse);
	else if (!Q_strncmp(type, "researched", 10))
		CL_ParseResearchedCampaignItems(name, text);
	else if (!Q_strncmp(type, "researchable", 12))
		CL_ParseResearchableCampaignStates(name, text, qtrue);
	else if (!Q_strncmp(type, "notresearchable", 15))
		CL_ParseResearchableCampaignStates(name, text, qfalse);
	else if (!Q_strncmp(type, "tech", 4))
		RS_ParseTechnologies(name, text);
	else if (!Q_strncmp(type, "base", 4))
		B_ParseBases(name, text);
	else if (!Q_strncmp(type, "nation", 6))
		CL_ParseNations(name, text);
	else if (!Q_strncmp(type, "rank", 4))
		CL_ParseMedalsAndRanks(name, text, qtrue);
	else if (!Q_strncmp(type, "mail", 4))
		CL_ParseEventMails(name, text);
#if 0
	else if (!Q_strncmp(type, "medal", 5))
		Com_ParseMedalsAndRanks( name, &text, qfalse );
#endif
}

/**
 * @brief
 *
 * parsed if we are no dedicated server
 * second stage links all the parsed data from first stage
 * example: we need a techpointer in a building - in the second stage the buildings and the
 * techs are already parsed - so now we can link them
 * @sa Com_ParseScripts
 * @sa CL_ParseScriptFirst
 * @note make sure that the client hunk was cleared - otherwise it may overflow
 */
static void CL_ParseScriptSecond (const char *type, char *name, char **text)
{
	/* check for client interpretable scripts */
	if (!Q_strncmp(type, "stage", 5))
		CL_ParseStage(name, text);
	else if (!Q_strncmp(type, "building", 8))
		B_ParseBuildings(name, text, qtrue);
}

/**
 * @brief Read the data into gd for singleplayer campaigns
 * @sa SAV_GameLoad
 * @sa CL_GameNew
 */
extern void CL_ReadSinglePlayerData (void)
{
	char *type, *name, *text;

	/* pre-stage parsing */
	FS_BuildFileList("ufos/*.ufo");
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	CL_ResetSinglePlayerData();
	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0)
		CL_ParseScriptFirst(type, name, &text);

	/* fill in IDXs for required research techs */
	RS_RequiredIdxAssign();

	/* stage two parsing */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	Com_DPrintf("Second stage parsing started...\n");
	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0)
		CL_ParseScriptSecond(type, name, &text);

	Com_Printf("Global data loaded - size %Zu bytes\n", sizeof(gd));
	Com_Printf("...techs: %i\n", gd.numTechnologies);
	Com_Printf("...buildings: %i\n", gd.numBuildingTypes);
	Com_Printf("...ranks: %i\n", gd.numRanks);
	Com_Printf("...nations: %i\n", gd.numNations);
	Com_Printf("\n");
}

/**
 * @brief Prints current ip to game console
 * @sa Sys_ShowIP
 */
static void CL_ShowIP_f (void)
{
	Sys_ShowIP();
}

/**
 * @brief Writes key bindings and archived cvars to config.cfg
 */
static void CL_WriteConfiguration (void)
{
	char path[MAX_OSPATH];

	if (cls.state == ca_uninitialized)
		return;

	if (strlen(FS_Gamedir()) >= MAX_OSPATH) {
		Com_Printf("Error: Can't save. Write path exceeded MAX_OSPATH\n");
		return;
	}
	Com_sprintf(path, sizeof(path), "%s/config.cfg", FS_Gamedir());
	Com_Printf("Save user settings to %s\n", path);
	Cvar_WriteVariables(path);
}

/**
 * @brief Calls all reset functions for all subsystems like production and research
 * also inits the cvars and commands
 * @sa CL_Init
 */
static void CL_InitLocal (void)
{
	int i;

	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds();

	Com_InitInventory(invList);
	Con_CheckResize();
	CL_InitInput();
	CL_InitMessageSystem();

	SAV_Init();
	MN_ResetMenus();
	CL_ResetParticles();
	CL_ResetCampaign();
	BS_ResetMarket();
	CL_ResetSequences();
	CL_ResetTeams();

	for (i = 0; i < 16; i++)
		Cvar_Get(va("adr%i", i), "", CVAR_ARCHIVE, "Bookmark for network ip");

	map_dropship = Cvar_Get("map_dropship", "craft_dropship", 0, "The dropship that is to be used in the map");

	/* register our variables */
	cl_stereo_separation = Cvar_Get("cl_stereo_separation", "0.4", CVAR_ARCHIVE, NULL);
	cl_stereo = Cvar_Get("cl_stereo", "0", 0, NULL);
	cl_isometric = Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");

	cl_show_tooltips = Cvar_Get("cl_show_tooltips", "1", CVAR_ARCHIVE, "Show tooltips in menus and hud");
	cl_show_cursor_tooltips = Cvar_Get("cl_show_cursor_tooltips", "1", CVAR_ARCHIVE, "Show cursor tooltips in tactical game mode");

	cl_camrotspeed = Cvar_Get("cl_camrotspeed", "250", CVAR_ARCHIVE, NULL);
	cl_camrotaccel = Cvar_Get("cl_camrotaccel", "400", CVAR_ARCHIVE, NULL);
	cl_cammovespeed = Cvar_Get("cl_cammovespeed", "750", CVAR_ARCHIVE, NULL);
	cl_cammoveaccel = Cvar_Get("cl_cammoveaccel", "1250", CVAR_ARCHIVE, NULL);
	cl_camyawspeed = Cvar_Get("cl_camyawspeed", "160", CVAR_ARCHIVE, NULL);
	cl_campitchmax = Cvar_Get("cl_campitchmax", "90", 0, "Max camera pitch - over 90 presents apparent mouse inversion");
	cl_campitchmin = Cvar_Get("cl_campitchmin", "35", 0, "Min camera pitch - under 35 presents difficulty positioning cursor");
	cl_campitchspeed = Cvar_Get("cl_campitchspeed", "0.5", CVAR_ARCHIVE, NULL);
	cl_camzoomquant = Cvar_Get("cl_camzoomquant", "0.16", CVAR_ARCHIVE, NULL);
	cl_camzoommin = Cvar_Get("cl_camzoommin", "0.7", 0, "Minimum zoom value for tactical missions");
	cl_camzoommax = Cvar_Get("cl_camzoommax", "3.4", 0, "Maximum zoom value for tactical missions");
	cl_centerview = Cvar_Get("cl_centerview", "1", CVAR_ARCHIVE, "Center the view when selecting a new soldier");

	cl_mapzoommax = Cvar_Get("cl_mapzoommax", "6.0", CVAR_ARCHIVE, "Maximum geoscape zooming value");
	cl_mapzoommin = Cvar_Get("cl_mapzoommin", "1.0", CVAR_ARCHIVE, "Minimum geoscape zooming value");

	sensitivity = Cvar_Get("sensitivity", "2", CVAR_ARCHIVE, NULL);
	cl_markactors = Cvar_Get("cl_markactors", "1", CVAR_ARCHIVE, NULL);

	cl_precachemenus = Cvar_Get("cl_precachemenus", "0", CVAR_ARCHIVE, "Precache all menus at startup");

	cl_aviForceDemo = Cvar_Get("cl_aviForceDemo", "1", CVAR_ARCHIVE, "AVI recording - record even if no game is loaded");
	cl_aviFrameRate = Cvar_Get("cl_aviFrameRate", "25", CVAR_ARCHIVE, "AVI recording - see video command");
	cl_aviMotionJpeg = Cvar_Get("cl_aviMotionJpeg", "1", CVAR_ARCHIVE, "AVI recording - see video command");

	cl_fps = Cvar_Get("cl_fps", "0", CVAR_ARCHIVE, "Show frames per second");
	cl_shownet = Cvar_Get("cl_shownet", "0", CVAR_ARCHIVE, NULL);
	cl_timeout = Cvar_Get("cl_timeout", "120", 0, NULL);
	cl_paused = Cvar_Get("paused", "0", 0, NULL);
	cl_timedemo = Cvar_Get("timedemo", "0", 0, NULL);

	rcon_client_password = Cvar_Get("rcon_password", "", 0, "Remote console password");
	rcon_address = Cvar_Get("rcon_address", "", 0, "Remote console address - set this if you aren't connected to a server");

	cl_logevents = Cvar_Get("cl_logevents", "0", 0, "Log all events to events.log");

	cl_worldlevel = Cvar_Get("cl_worldlevel", "0", 0, "Current worldlevel in tactical mode");
	cl_worldlevel->modified = qfalse;
	cl_selected = Cvar_Get("cl_selected", "0", CVAR_NOSET, "Current selected soldier");

	cl_3dmap = Cvar_Get("cl_3dmap", "0", CVAR_ARCHIVE, "3D geoscape or float geoscape");
	gl_3dmapradius = Cvar_Get("gl_3dmapradius", "8192.0", CVAR_NOSET, "3D geoscape radius");

	/* only 19 soldiers in soldier selection list */
	cl_numnames = Cvar_Get("cl_numnames", "19", CVAR_NOSET, NULL);

	difficulty = Cvar_Get("difficulty", "0", CVAR_NOSET, "Difficulty level");
	cl_start_employees = Cvar_Get("cl_start_employees", "1", CVAR_ARCHIVE, "Start with hired employees");
	cl_initial_equipment = Cvar_Get("cl_initial_equipment", "human_phalanx_initial", CVAR_ARCHIVE, "Start with assigned equipment - see cl_start_employees");
	cl_start_buildings = Cvar_Get("cl_start_buildings", "1", CVAR_ARCHIVE, "Start with initial buildings in your first base");

	confirm_actions = Cvar_Get("confirm_actions", "0", CVAR_ARCHIVE, "Confirm all actions in tactical mode");
	confirm_movement = Cvar_Get("confirm_movement", "0", CVAR_ARCHIVE, "Confirm all movements in tactical mode");

	Cvar_Set("music", "");

	mn_main = Cvar_Get("mn_main", "main", 0, "Which is the main menu id to return to");
	mn_sequence = Cvar_Get("mn_sequence", "sequence", 0, "Which is the sequence menu node to render the sequence in");
	mn_active = Cvar_Get("mn_active", "", 0, NULL);
	mn_hud = Cvar_Get("mn_hud", "hud", CVAR_ARCHIVE, "Which is the current selected hud");
	mn_lastsave = Cvar_Get("mn_lastsave", "", CVAR_ARCHIVE, NULL);

	mn_serverlist = Cvar_Get("mn_serverlist", "0", CVAR_ARCHIVE, "0=show all, 1=hide full - servers on the serverlist");

	mn_inputlength = Cvar_Get("mn_inputlength", "32", 0, "Limit the input length for messagemenu input");
	mn_inputlength->modified = qfalse;

	/* userinfo */
	info_password = Cvar_Get("password", "", CVAR_USERINFO, NULL);
	info_spectator = Cvar_Get("spectator", "0", CVAR_USERINFO, NULL);
	name = Cvar_Get("name", _("Unnamed"), CVAR_USERINFO | CVAR_ARCHIVE, "Playername");
	snd_ref = Cvar_Get("snd_ref", "sdl", CVAR_ARCHIVE, "Sound renderer");
	team = Cvar_Get("team", "human", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	equip = Cvar_Get("equip", "multiplayer_initial", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	teamnum = Cvar_Get("teamnum", "1", CVAR_USERINFO | CVAR_ARCHIVE, "Teamnum for multiplayer teamplay games");
	campaign = Cvar_Get("campaign", "main", 0, "Which is the current selected campaign id");
	rate = Cvar_Get("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE, NULL);	/* FIXME */
	msg = Cvar_Get("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE, "Sets the message level for server messages the client receives");
	sv_maxclients = Cvar_Get("maxclients", "1", CVAR_SERVERINFO, "If maxclients is 1 we are in singleplayer - otherwise we are mutliplayer mode (see sv_teamplay)");

	noudp = Cvar_Get("noudp", "0", CVAR_NOSET, "Don't use UDP as network protocol'");
	noipx = Cvar_Get("noipx", "0", CVAR_NOSET, "Don't use IPX as network protocol");
	masterserver_ip = Cvar_Get("masterserver_ip", "195.136.48.62", CVAR_ARCHIVE, "IP address of UFO:AI masterserver (Sponsored by NineX)");
	masterserver_port = Cvar_Get("masterserver_port", "27900", CVAR_ARCHIVE, "Port of UFO:AI masterserver");

#ifdef HAVE_CURL
	cl_http_proxy = Cvar_Get("cl_http_proxy", "", 0, NULL);
	cl_http_filelists = Cvar_Get("cl_http_filelists", "1", 0, NULL);
	cl_http_downloads = Cvar_Get("cl_http_downloads", "1", 0, "Try to download files via http");
	cl_http_max_connections = Cvar_Get("cl_http_max_connections", "1", 0, NULL);
#endif /* HAVE_CURL */

	/* register our commands */
	Cmd_AddCommand("cmd", CL_ForwardToServer_f, "Forward to server");
	Cmd_AddCommand("pause", CL_Pause_f, _("Pause the current server (singleplayer and multiplayer when you are server)"));
	Cmd_AddCommand("pingservers", CL_PingServers_f, "Ping all servers in local network to get the serverlist");

	Cmd_AddCommand("saveconfig", CL_WriteConfiguration, "Save the configuration");

	Cmd_AddCommand("showip", CL_ShowIP_f, "Command to show your ip");

	/* text id is servers in menu_multiplayer.ufo */
	Cmd_AddCommand("server_info", CL_ServerInfo_f, NULL);
	Cmd_AddCommand("serverlist", CL_PrintServerList_f, NULL);
	Cmd_AddCommand("servers_click", CL_ServerListClick_f, NULL);
	Cmd_AddCommand("server_connect", CL_ServerConnect_f, NULL);
	Cmd_AddCommand("bookmarks_click", CL_BookmarkListClick_f, NULL);
	Cmd_AddCommand("bookmark_add", CL_BookmarkAdd_f, "Add a new bookmark - see adrX cvars");

	Cmd_AddCommand("userinfo", CL_Userinfo_f, NULL);
	Cmd_AddCommand("snd_restart", CL_Snd_Restart_f, "Restart the sound renderer");

	Cmd_AddCommand("changing", CL_Changing_f, NULL);
	Cmd_AddCommand("disconnect", CL_Disconnect_f, NULL);

	Cmd_AddCommand("quit", CL_Quit_f, "Quits the game");

	Cmd_AddCommand("connect", CL_Connect_f, "Connect to given ip");
	Cmd_AddCommand("reconnect", CL_Reconnect_f, "Reconnect to last server");

	Cmd_AddCommand("rcon", CL_Rcon_f, "Execute a rcon command - see rcon_password");

#ifdef ACTIVATE_PACKET_COMMAND
	/* this is dangerous to leave in */
	Cmd_AddCommand ("packet", CL_Packet_f, "Dangerous debug function for network testing");
#endif

	Cmd_AddCommand("env", CL_Env_f, NULL);

	Cmd_AddCommand("precache", CL_Precache_f, "Function that is called at mapload to precache map data");
	Cmd_AddCommand("mp_selectteam_init", CL_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team");
	Cmd_AddCommand("mp_wait_init", CL_Wait_Init_f, "Function that inits some nodes");
	Cmd_AddCommand("spawnsoldiers", CL_SpawnSoldiers_f, "Spawns the soldiers for the selected teamnum");
	Cmd_AddCommand("teamnum_dec", CL_TeamNum_f, "Decrease the prefered teamnum");
	Cmd_AddCommand("teamnum_inc", CL_TeamNum_f, "Increase the prefered teamnum");

	Cmd_AddCommand("seq_click", CL_SequenceClick_f, NULL);
	Cmd_AddCommand("seq_start", CL_SequenceStart_f, NULL);
	Cmd_AddCommand("seq_end", CL_SequenceEnd_f, NULL);

	Cmd_AddCommand("video", CL_Video_f, "Enable AVI recording - see stopvideo");
	Cmd_AddCommand("stopvideo", CL_StopVideo_f, "Disable AVI recording - see video");

	/* allow to change the sound renderer even if no sound was initialized */
	Cmd_AddCommand("snd_modifyref", S_ModifySndRef_f, "Modify sound renderer");

	/* forward to server commands */
	/* the only thing this does is allow command completion */
	/* to work -- all unknown commands are automatically */
	/* forwarded to the server */
	Cmd_AddCommand("say", NULL, NULL);
	Cmd_AddCommand("say_team", NULL, NULL);
	Cmd_AddCommand("info", NULL, NULL);
	Cmd_AddCommand("playerlist", NULL, NULL);
	Cmd_AddCommand("players", NULL, NULL);
#ifdef DEBUG
	Cmd_AddCommand("actorinvlist", NULL, "Shows the inventory list of all actors");
	Cmd_AddCommand("killteam", NULL, NULL);
	Cmd_AddCommand("stunteam", NULL, NULL);
#endif
}

typedef struct {
	const char *name;
	const char *value;
	cvar_t *var;
} cheatvar_t;

static cheatvar_t cheatvars[] = {
	{"timedemo", "0", NULL},
	{"r_drawworld", "1", NULL},
	{"r_fullbright", "0", NULL},
	{"paused", "0", NULL},
	{"fixedtime", "0", NULL},
	{"gl_lightmap", "0", NULL},
	{"gl_wire", "0", NULL},
	{"gl_saturatelighting", "0", NULL},
	{NULL, NULL, NULL}
};

static int numcheatvars = 0;

/**
 * @brief
 * @sa CL_SendCommand
 */
static void CL_FixCvarCheats (void)
{
	int i;
	cheatvar_t *var;

	if (!Q_strncmp(cl.configstrings[CS_MAXCLIENTS], "1", MAX_TOKEN_CHARS)
		|| !cl.configstrings[CS_MAXCLIENTS][0])
		return;					/* single player can cheat */

	/* find all the cvars if we haven't done it yet */
	if (!numcheatvars) {
		while (cheatvars[numcheatvars].name) {
			cheatvars[numcheatvars].var = Cvar_Get(cheatvars[numcheatvars].name, cheatvars[numcheatvars].value, 0, NULL);
			numcheatvars++;
		}
	}

	/* make sure they are all set to the proper values */
	for (i = 0, var = cheatvars; i < numcheatvars; i++, var++) {
		if (Q_strcmp(var->var->string, var->value)) {
			Com_DPrintf("...cheatcvar '%s': value from '%s' to '%s'\n", var->name, var->var->string, var->value);
			Cvar_Set(var->name, var->value);
		}
	}
}

/**
 * @brief
 */
static void CL_SendCmd (void)
{
	/* send a userinfo update if needed */
	if (cls.state >= ca_connected) {
		if (userinfo_modified) {
			userinfo_modified = qfalse;
			MSG_WriteByte(&cls.netchan.message, clc_userinfo);
			MSG_WriteString(&cls.netchan.message, Cvar_Userinfo());
		}

		if (cls.netchan.message.cursize || curtime - cls.netchan.last_sent > 1000)
			Netchan_Transmit(&cls.netchan, 0, NULL);
	}
}


/**
 * @brief
 * @sa CL_Frame
 */
static void CL_SendCommand (void)
{
	/* get new key events */
	Sys_SendKeyEvents();

	/* allow mice or other external controllers to add commands */
	IN_Commands();

	/* process console commands */
	Cbuf_Execute();

	/* send intentions now */
	CL_SendCmd();

	/* fix any cheating cvars */
	CL_FixCvarCheats();

	/* resend a connection request if necessary */
	CL_CheckForResend();
}


/**
 * @brief
 */
extern void CL_AddMapParticle (char *ptl, vec3_t origin, vec2_t wait, char *info, int levelflags)
{
	mp_t *mp;

	mp = &MPs[numMPs++];

	if (numMPs >= MAX_MAPPARTICLES) {
		Sys_Error("Too many map particles\n");
		return;
	}

	Q_strncpyz(mp->ptl, ptl, MAX_QPATH);
	VectorCopy(origin, mp->origin);
	mp->info = info;
	mp->levelflags = levelflags;
	mp->wait[0] = wait[0] * 1000;
	mp->wait[1] = wait[1] * 1000;
	mp->nextTime = cl.time + wait[0] + wait[1] * frand() + 1;

	Com_DPrintf("Adding map particle %s (%i) with levelflags %i\n", ptl, numMPs, levelflags);
}

/**
 * @brief Translate the difficulty int to a translated string
 * @param difficulty the difficulty integer value
 */
extern char* CL_ToDifficultyName (int difficulty)
{
	switch (difficulty) {
	case -4:
		return _("Chicken-hearted");
	case -3:
		return _("Very Easy");
	case -2:
	case -1:
		return _("Easy");
	case 0:
		return _("Normal");
	case 1:
	case 2:
		return _("Hard");
	case 3:
		return _("Very Hard");
	case 4:
		return _("Insane");
	default:
		Sys_Error("Unknown difficulty id %i\n", difficulty);
		break;
	}
	return NULL;
}

/**
 * @brief
 */
static void CL_CvarCheck (void)
{
	int v;

	/* worldlevel */
	if (cl_worldlevel->modified) {
		int i;
		if (cl_worldlevel->integer < 0) {
			CL_Drop();
			Com_DPrintf("CL_CvarCheck: Called drop - something went wrong\n");
			return;
		}

		if (cl_worldlevel->integer >= map_maxlevel - 1)
			Cvar_SetValue("cl_worldlevel", map_maxlevel - 1);
		else if (cl_worldlevel->integer < 0)
			Cvar_SetValue("cl_worldlevel", 0);
		for (i = 0; i < map_maxlevel; i++)
			Cbuf_AddText(va("deselfloor%i\n", i));
		for (; i < 8; i++)
			Cbuf_AddText(va("disfloor%i\n", i));
		Cbuf_AddText(va("selfloor%i\n", cl_worldlevel->integer));
		cl_worldlevel->modified = qfalse;
	}

#ifdef HAVE_GETTEXT
	/* language */
	if (s_language->modified)
		Qcommon_LocaleInit();
#endif

	if (mn_inputlength->modified) {
		if (mn_inputlength->integer >= MAX_CVAR_EDITING_LENGTH)
			Cvar_SetValue("mn_inputlength", MAX_CVAR_EDITING_LENGTH);
	}

	/* gl_mode and fullscreen */
	v = Cvar_VariableInteger("mn_glmode");
	if (v < 0 || v >= maxVidModes) {
		Com_Printf("Max gl_mode mode is %i (%i)\n", maxVidModes, v);
		v = Cvar_VariableInteger("gl_mode");
		Cvar_SetValue("mn_glmode", v);
	}
	Cvar_Set("mn_glmodestr", va("%i*%i", vid_modes[v].width, vid_modes[v].height));
}


#define NUM_DELTA_FRAMES	20
/**
 * @brief
 * @sa Qcommon_Frame
 */
extern void CL_Frame (int msec)
{
	static int extratime = 0;
	static int lasttimecalled = 0;
	char *type, *name, *text;

	if (sv_maxclients->modified) {
		if (sv_maxclients->integer > 1 && ccs.singleplayer) {
			CL_StartSingleplayer(qfalse);
			curCampaign = NULL;
			selMis = NULL;
			baseCurrent = &gd.bases[0];
			B_ClearBase(&gd.bases[0]);
			RS_ResetHash();
			gd.numBases = 1;
			gd.numAircraft = 0;

			/* now add a dropship where we can place our soldiers in */
			AIR_NewAircraft(baseCurrent, "craft_dropship");

			Com_Printf("Changing to Multiplayer\n");
			/* no campaign equipment but for multiplayer */
			Cvar_Set("map_dropship", "craft_dropship");
			/* disconnect already running session */
			if (cls.state >= ca_connecting)
				CL_Disconnect();

			/* pre-stage parsing */
			FS_BuildFileList("ufos/*.ufo");
			FS_NextScriptHeader(NULL, NULL, NULL);
			text = NULL;

			while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != 0)
				if (!Q_strncmp(type, "tech", 4))
					RS_ParseTechnologies(name, &text);

			/* fill in IDXs for required research techs */
			RS_RequiredIdxAssign();
			Com_AddObjectLinks();	/* Add tech links + ammo<->weapon links to items.*/
		} else if (sv_maxclients->integer == 1) {
			CL_StartSingleplayer(qtrue);
			Com_Printf("Changing to Singleplayer\n");
		}
		sv_maxclients->modified = qfalse;
	}

	extratime += msec;

	/* don't allow setting maxfps too low (or game could stop responding) */
	if (cl_maxfps->integer < 10)
		Cvar_SetValue("cl_maxfps", 10);

	if (!cl_timedemo->value) {
		/* don't flood packets out while connecting */
		if (cls.state == ca_connected && extratime < 100)
			return;
		/* framerate is too high */
		if (extratime < ceil(1000.0 / cl_maxfps->value))
			return;
	}

	/* decide the simulation time */
	cls.frametime = extratime / 1000.0;
	cls.realtime = curtime;
	cl.time += extratime;
	if (!blockEvents)
		cl.eventTime += extratime;

	/* let the mouse activate or deactivate */
	IN_Frame();

	/* if recording an avi, lock to a fixed fps */
	if (CL_VideoRecording() && cl_aviFrameRate->value && msec) {
		/* save the current screen */
		if (cls.state == ca_active || cl_aviForceDemo->value) {
			CL_TakeVideoFrame();

			/* fixed time for next frame' */
			msec = (int) ceil((1000.0f / cl_aviFrameRate->value));
			if (msec == 0)
				msec = 1;
		}
	}

	/* frame rate calculation */
	if (!(cls.framecount % NUM_DELTA_FRAMES)) {
		cls.framerate = NUM_DELTA_FRAMES * 1000.0 / (cls.framedelta + extratime);
		cls.framedelta = 0;
	} else
		cls.framedelta += extratime;

	extratime = 0;

	/* don't extrapolate too far ahead */
	if (cls.frametime > (1.0 / 5))
		cls.frametime = (1.0 / 5);

	/* if in the debugger last frame, don't timeout */
	if (msec > 5000)
		cls.netchan.last_received = Sys_Milliseconds();

	if (cls.state == ca_connected) {
#ifdef HAVE_CURL
		/* we run full speed when connecting */
		CL_RunHTTPDownloads();
#endif /* HAVE_CURL */
	}

	/* fetch results from server */
	CL_ReadPackets();

	/* event and LE updates */
	CL_CvarCheck();
	CL_Events();
	LE_Think();
	/* end the rounds when no soldier is alive */
	CL_RunMapParticles();
	CL_ParticleRun();

	/* send a new command message to the server */
	CL_SendCommand();

	/* update camera position */
	CL_CameraMove();
	CL_ParseInput();
	CL_ActorUpdateCVars();

	/* allow rendering DLL change */
	VID_CheckChanges();
	if (!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh();

	/* update the screen */
	if (host_speeds->value)
		time_before_ref = Sys_Milliseconds();
	SCR_UpdateScreen();
	if (host_speeds->value)
		time_after_ref = Sys_Milliseconds();

	/* update audio */
	S_Update(cl.refdef.vieworg, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);

	CDAudio_Update();

	/* advance local effects for next frame */
	CL_RunLightStyles();
	SCR_RunConsole();

	cls.framecount++;

	if (log_stats->value) {
		if (cls.state == ca_active) {
			if (!lasttimecalled) {
				lasttimecalled = Sys_Milliseconds();
				if (log_stats_file)
					fprintf(log_stats_file, "0\n");
			} else {
				int now = Sys_Milliseconds();

				if (log_stats_file)
					fprintf(log_stats_file, "%d\n", now - lasttimecalled);
				lasttimecalled = now;
			}
		}
	}
}

/**
 * @brief
 * @sa CL_Shutdown
 * @sa CL_InitAfter
 */
extern void CL_Init (void)
{
	if (dedicated->value)
		return;					/* nothing running on the client */

	CL_ClientHunkInit();

	/* all archived variables will now be loaded */
	Con_Init();
#ifdef _WIN32
	/* sound must be initialized after window is created */
	VID_Init();
	S_Init();
#else
	S_Init();
	VID_Init();
#endif

	V_Init();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	SCR_Init();

	CDAudio_Init();
	CL_InitLocal();
	IN_Init();

	/* FIXME: Maybe we should activate this again when all savegames issues are solved */
/*	Cbuf_AddText("loadteam current\n"); */
	FS_ExecAutoexec();
	Cbuf_Execute();

	memset(&teamData, 0, sizeof(teamData_t));
	/* Default to single-player mode */
	ccs.singleplayer = qtrue;

#ifdef HAVE_CURL
	CL_InitHTTPDownloads();
#endif /* HAVE_CURL */

	CL_InitParticles();
}


/**
 * @brief Saves configuration file and shuts the client systems down
 * FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
 * to run quit through here before the final handoff to the sys code.
 * @sa Sys_Quit
 * @sa CL_Init
 */
extern void CL_Shutdown (void)
{
	static qboolean isdown = qfalse;

	if (isdown) {
		printf("recursive shutdown\n");
		return;
	}
	isdown = qtrue;

#ifdef HAVE_CURL
	CL_HTTP_Cleanup(qtrue);
#endif /* HAVE_CURL */
	Irc_Shutdown();
	CL_WriteConfiguration();
	Con_SaveConsoleHistory(FS_Gamedir());
	if (CL_VideoRecording())
		CL_CloseAVI();
	CDAudio_Shutdown();
	S_Shutdown();
	IN_Shutdown();
	VID_Shutdown();
	MN_Shutdown();
	FS_Shutdown();
	CL_ClientHunkShutdown();
}
