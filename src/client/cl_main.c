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

FILE *log_stats_file;

cvar_t *masterserver_ip;
cvar_t *masterserver_port;

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

cvar_t *cl_timedemo;

cvar_t *cl_aviForceDemo;
cvar_t *cl_aviFrameRate;
cvar_t *cl_aviMotionJpeg;

cvar_t *cl_particleWeather;

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

/** @brief Confirm actions in tactical mode - valid values are 0, 1 and 2 */
cvar_t *confirm_actions;

cvar_t *cl_precache;
cvar_t *log_stats;

/* userinfo */
cvar_t *info_password;
cvar_t *info_spectator;
cvar_t *name;
cvar_t *team;
cvar_t *equip;
cvar_t *teamnum;
cvar_t *campaign;
cvar_t *msg;

client_static_t cls;
client_state_t cl;

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

struct memPool_s *cl_localPool;		/**< reset on every game restart */
struct memPool_s *cl_genericPool;	/**< permanent client data - menu, fonts */
struct memPool_s *cl_ircSysPool;	/**< irc pool */
struct memPool_s *cl_soundSysPool;
struct memPool_s *cl_menuSysPool;
struct memPool_s *vid_genericPool;
struct memPool_s *vid_imagePool;
struct memPool_s *vid_lightPool;	/**< lightmap - wiped with every new map */
struct memPool_s *vid_modelPool;	/**< modeldata - wiped with every new map */
/*====================================================================== */

/**
 * @brief adds the current command line as a clc_stringcmd to the client message.
 * things like action, turn, etc, are commands directed to the server,
 * so when they are typed in at the console, they will need to be forwarded.
 */
void Cmd_ForwardToServer (void)
{
	const char *cmd = Cmd_Argv(0);
	struct dbuffer *msg;

	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+') {
		Com_Printf("Unknown command \"%s\"\n", cmd);
		return;
	}

	msg = new_dbuffer();
	NET_WriteByte(msg, clc_stringcmd);
	dbuffer_add(msg, cmd, strlen(cmd));
	if (Cmd_Argc() > 1) {
		dbuffer_add(msg, " ", 1);
		dbuffer_add(msg, Cmd_Args(), strlen(Cmd_Args()));
	}
	dbuffer_add(msg, "", 1);
	NET_WriteMsg(cls.stream, msg);
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
		struct dbuffer *msg;
		msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		dbuffer_add(msg, Cmd_Args(), strlen(Cmd_Args()) + 1);
		NET_WriteMsg(cls.stream, msg);
	}
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
void CL_StartSingleplayer (qboolean singleplayer)
{
	const char *type, *name, *text;
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
		const char *max_s, *max_spp;
		max_s = Cvar_VariableStringOld("maxsoldiers");
		max_spp = Cvar_VariableStringOld("maxsoldiersperplayer");

		/* restore old maxsoldiersperplayer and maxsoldiers cvars if values were previously set */
		if (strlen(max_s))
			Cvar_Set("maxsoldiers", max_s);
		if (strlen(max_spp))
			Cvar_Set("maxsoldiersperplayer", max_spp);

		ccs.singleplayer = qfalse;
		curCampaign = NULL;
		selMis = NULL;
		baseCurrent = &gd.bases[0];
		B_ClearBase(&gd.bases[0]);
		RS_ResetHash();
		gd.numBases = 1;
		gd.numAircraft = 0;

		/* now add a dropship where we can place our soldiers in */
		AIR_NewAircraft(baseCurrent, "craft_drop_firebird");

		Com_Printf("Changing to Multiplayer\n");
		/* no campaign equipment but for multiplayer */
		Cvar_Set("map_dropship", "craft_drop_firebird");
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
	}
}

/**
 * @brief
 * @note Called after an ERR_DROP was thrown
 * @sa CL_Disconnect
 */
void CL_Drop (void)
{
	/* drop loading plaque unless this is the initial game start */
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque();	/* get rid of loading plaque */

	if (cls.state == ca_uninitialized || cls.state == ca_disconnected)
		return;

	CL_Disconnect();

	/* make sure that we are in the correct menus in singleplayer after
	 * dropping the game due to a failure */
	if (ccs.singleplayer) {
		Cvar_Set("mn_main", "singleplayerInGame");
		Cvar_Set("mn_active", "map");
	} else {
		Cvar_Set("mn_main", "main");
		Cvar_Set("mn_active", "");
	}
}

/**
 * @brief
 */
static void CL_Connect (void)
{
	userinfo_modified = qfalse;

	free_stream(cls.stream);

	if (cls.servername[0]) {
		cvar_t *port = Cvar_Get("port", va("%i", PORT_SERVER), CVAR_NOSET, NULL);
		cls.stream = connect_to_host(cls.servername, port->string);
	} else
		cls.stream = connect_to_loopback();

	NET_OOB_Printf(cls.stream, "connect %i \"%s\"\n", PROTOCOL_VERSION, Cvar_Userinfo());

	cls.connect_time = cls.realtime;
}

/**
 * @brief Resend a connect message if the last one has timed out
 */
static void CL_CheckForResend (void)
{
	/* if the local server is running and we aren't */
	/* then connect */
	if (cls.state == ca_disconnected && Com_ServerState()) {
		cls.state = ca_connecting;
		cls.servername[0] = '\0';
		CL_Connect();
		userinfo_modified = qfalse;
		return;
	}

	/* resend if we haven't gotten a reply yet */
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	Com_Printf("Connecting to %s...\n", cls.servername ? cls.servername : "internal server");
	CL_Connect();
}

/**
 * @brief
 *
 * FIXME: Spectator needs no team
 */
static void CL_Connect_f (void)
{
	const char *server;

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

	/* FIXME: why a second time? */
	CL_Disconnect();

	cls.state = ca_connecting;

	/* everything should be reasearched for multiplayer matches */
/*	RS_MarkResearchedAll(); */

	Q_strncpyz(cls.servername, server, sizeof(cls.servername));

	CL_Connect();

	/* CL_CheckForResend() will fire immediately */
	Cvar_Set("mn_main", "multiplayerInGame");
	cls.connect_time = -99999;
}

#if 0
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
			to.port = htons(PORT_SERVER);
	}

	NET_SendPacket(NS_CLIENT, strlen(message) + 1, message, to);
}
#endif

/**
 * @brief
 * @sa CL_ParseServerData
 * @sa CL_Disconnect
 */
void CL_ClearState (void)
{
	const char *mapZone;

	S_StopAllSounds();
	CL_ClearEffects();

	/* wipe the entire cl structure */
	mapZone = cl.refdef.mapZone;
	memset(&cl, 0, sizeof(cl));
	cl.refdef.mapZone = mapZone;
	cl.cam.zoom = 1.0;
	CalcFovX();

	numLEs = 0;
	numLMs = 0;
	numMPs = 0;
	numPtls = 0;
}

/**
 * @brief Sets the cls.state to ca_disconnected and informs the server
 * @sa CL_Disconnect_f
 * @sa CL_CloseAVI
 * @sa CL_Drop
 * @note Goes from a connected state to full screen console state
 * Sends a disconnect message to the server
 * This is also called on Com_Error, so it shouldn't cause any errors
 */
void CL_Disconnect (void)
{
	struct dbuffer *msg;

	/* If playing a cinematic, stop it */
	CIN_StopCinematic();

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->integer) {
		int time;

		time = Sys_Milliseconds() - cl.timedemo_start;
		if (time > 0)
			Com_Printf("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames, time / 1000.0, cl.timedemo_frames * 1000.0 / time);
	}

	VectorClear(cl.refdef.blend);

	cls.connect_time = 0;

	/* send a disconnect message to the server */
	msg = new_dbuffer();
	NET_WriteByte(msg, clc_stringcmd);
	NET_WriteString(msg, "disconnect");
	NET_WriteMsg(cls.stream, msg);
	stream_finished(cls.stream);
	cls.stream = NULL;

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
		adr.port = htons(PORT_SERVER);

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
		struct dbuffer *msg;
		Com_Printf("reconnecting...\n");
		msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, "new");
		NET_WriteMsg(cls.stream, msg);
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
		CL_Connect();
	}
}

typedef struct serverList_s {
	char *node;
	char *service;
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

static void process_ping_reply (serverList_t *server, char *msg)
{
	if (PROTOCOL_VERSION != atoi(Info_ValueForKey(msg, "protocol"))) {
		Com_DPrintf("process_ping_reply: Protocol mismatch\n");
		return;
	}
	if (Q_strcmp(UFO_VERSION, Info_ValueForKey(msg, "version"))) {
		Com_DPrintf("process_ping_reply: Version mismatch\n");
	}

	if (server->pinged)
		return;

	server->pinged = qtrue;
	Q_strncpyz(server->hostname, Info_ValueForKey(msg, "hostname"),
		sizeof(server->hostname));
	Q_strncpyz(server->version, Info_ValueForKey(msg, "version"),
		sizeof(server->version));
	Q_strncpyz(server->mapname, Info_ValueForKey(msg, "mapname"),
		sizeof(server->mapname));
	Q_strncpyz(server->gametype, Info_ValueForKey(msg, "gametype"),
		sizeof(server->gametype));
	server->dedicated = atoi(Info_ValueForKey(msg, "dedicated"));
	server->clients = atoi(Info_ValueForKey(msg, "clients"));
	server->maxclients = atoi(Info_ValueForKey(msg, "maxclients"));
}

static void ping_callback (struct net_stream *s)
{
	struct dbuffer *buf = NET_ReadMsg(s);
	serverList_t *server = stream_data(s);
	int cmd = NET_ReadByte(buf);
	char *str = NET_ReadStringLine(buf);
	char string[MAX_INFO_STRING];

	if (cmd == clc_oob && Q_strncmp(str, "info", 4) == 0) {
		str = NET_ReadString(buf);
		if (str)
		process_ping_reply(server, str);
	}

	menuText[TEXT_LIST] = serverText;
	Com_sprintf(string, sizeof(string), "%s\t\t\t%s\t\t\t%s\t(%s)\t%i/%i\n",
		server->hostname,
		server->mapname,
		server->gametype,
		server->version,
		server->clients,
		server->maxclients);
	server->serverListPos = serverListPos;
	serverListPos++;
	Q_strcat(serverText, string, sizeof(serverText));
	free_stream(s);
}

/**
 * @brief Pings all servers in serverList
 * @sa CL_AddServerToList
 */
static void CL_PingServer (serverList_t *server)
{
	struct net_stream *s = connect_to_host(server->node, server->service);
	Com_DPrintf("pinging [%s]:%s...\n", server->node, server->service);
	NET_OOB_Printf(s, "info %i", PROTOCOL_VERSION);
	set_stream_data(s, server);
	stream_callback(s, &ping_callback);
}

/**
 * @brief Prints all the servers on the list to game console
 */
static void CL_PrintServerList_f (void)
{
	int i;

	Com_Printf("%i servers on the list\n", serverListLength);

	for (i = 0; i < serverListLength; i++) {
		Com_Printf("%02i: [%s]:%s (pinged: %i)\n", i, serverList[i].node, serverList[i].service, serverList[i].pinged);
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
static void CL_AddServerToList (const char *node, const char *service)
{
	if (serverListLength >= MAX_SERVERLIST)
		return;

	memset(&(serverList[serverListLength]), 0, sizeof(serverList_t));
	serverList[serverListLength].node = strdup(node);
	serverList[serverListLength].service = strdup(service);
	CL_PingServer(&serverList[serverListLength]);
	serverListLength++;
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
	if (!*cl.configstrings[CS_NAME]) {
		if (!reconnect) {
			reconnect = qtrue;
			Cbuf_ExecuteText(EXEC_NOW, "reconnect\nmn_pop");
		} else {
			Cbuf_ExecuteText(EXEC_NOW, "disconnect\nmn_pop");
			MN_Popup(_("Error"), _("Server needs restarting - something went wrong"));
		}
	} else
		reconnect = qfalse;
}

/**
 * @brief Team selection text
 *
 * This function fills the multiplayer_selectteam menu with content
 * @sa Netchan_OutOfBandPrint
 * @sa SV_TeamInfoString
 * @sa CL_SelectTeam_Init_f
 */
static void CL_ParseTeamInfoMessage (struct dbuffer *msg)
{
	char *s = NET_ReadString(msg);
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
static char userInfoText[MAX_MESSAGE_TEXT];
/**
 * @brief Serverbrowser text
 *
 * This function fills the network browser server information with text
 * @sa Netchan_OutOfBandPrint
 */
static void CL_ParseServerInfoMessage (struct net_stream *stream, const char *s)
{
	const char *value = NULL;
	const char *users;
	int team;
	const char *token;
	char buf[256];

	if (!s)
		return;

	/* check for server status response message */
	value = Info_ValueForKey(s, "dedicated");
	if (*value) {
		Com_DPrintf("%s\n", s); /* status string */
		/* server info cvars and users are seperated via newline */
		users = strstr(s, "\n");
		if (!users) {
			Com_Printf("%c%s\n", 1, s);
			return;
		}

		Cvar_Set("mn_mappic", "maps/shots/na.jpg");
		Cvar_Set("mn_server_need_password", "0"); /* string */

		Com_sprintf(serverInfoText, sizeof(serverInfoText), _("IP\t%s\n\n"), stream_peer_name(stream, buf, sizeof(buf)));
		value = Info_ValueForKey(s, "mapname");
		Cvar_Set("mapname", value);
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Map:\t%s\n"), value);
		if (FS_CheckFile(va("pics/maps/shots/%s.jpg", value)) != -1)
			Cvar_Set("mn_mappic", va("maps/shots/%s.jpg", value));
		else {
			char filename[MAX_QPATH];
			Q_strncpyz(filename, "pics/maps/shots/", sizeof(filename));
			/* strip off the day or night char */
			Q_strncpyz(&filename[strlen(filename)], value, strlen(value) - 1);
			if (FS_CheckFile(filename) != -1)
				Cvar_Set("mn_mappic", filename);
		}
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Servername:\t%s\n"), Info_ValueForKey(s, "hostname"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Moralestates:\t%s\n"), Info_ValueForKey(s, "sv_enablemorale"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Gametype:\t%s\n"), Info_ValueForKey(s, "gametype"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Gameversion:\t%s\n"), Info_ValueForKey(s, "ver"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Dedicated server:\t%s\n"), Info_ValueForKey(s, "dedicated"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Operating system:\t%s\n"), Info_ValueForKey(s, "sys_os"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Network protocol:\t%s\n"), Info_ValueForKey(s, "protocol"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Roundtime:\t%s\n"), Info_ValueForKey(s, "sv_roundtimelimit"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Teamplay:\t%s\n"), Info_ValueForKey(s, "sv_teamplay"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. players per team:\t%s\n"), Info_ValueForKey(s, "maxplayers"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. teams allowed in this map:\t%s\n"), Info_ValueForKey(s, "sv_maxteams"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. clients:\t%s\n"), Info_ValueForKey(s, "maxclients"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. soldiers per player:\t%s\n"), Info_ValueForKey(s, "maxsoldiersperplayer"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. soldiers per team:\t%s\n"), Info_ValueForKey(s, "maxsoldiers"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Password protected:\t%s\n"), Info_ValueForKey(s, "needpass"));
		menuText[TEXT_STANDARD] = serverInfoText;
		do {
			token = COM_Parse(&users);
			if (!users)
				break;
			team = atoi(token);
			/* skip null ping */
			COM_Parse(&users);
			if (!users)
				break;
			token = COM_Parse(&users);
			if (!users)
				break;
			Com_sprintf(userInfoText, sizeof(userInfoText), "%s\t%i\n", token, team);
		} while (1);
		menuText[TEXT_LIST] = userInfoText;
		Cvar_Set("mn_server_ip", stream_peer_name(stream, buf, sizeof(buf)));
		MN_PushMenu("serverinfo");
	} else
		Com_Printf("%c%s", 1, s);
}

static void status_callback (struct net_stream *s)
{
	struct dbuffer *buf = NET_ReadMsg(s);

	if (!buf)
		return;

	{
		int cmd = NET_ReadByte(buf);
		char *str = NET_ReadStringLine(buf);

		if (cmd == clc_oob && Q_strncmp(str, "print", 5) == 0) {
			str = NET_ReadString(buf);
			if (str)
				CL_ParseServerInfoMessage(s, str);
		}
	}
	free_stream(s);
}

/**
 * @brief Masterserver server list
 * @sa Netchan_OutOfBandPrint
 * @sa CL_ConnectionlessPacket
 */
static void CL_ParseMasterServerResponse (struct dbuffer *buf)
{
	byte ip[4];
	unsigned short port;
	char node[MAX_VAR];
	char service[MAX_VAR];

	while (dbuffer_len(buf) >= 6) {
		/* parse the ip */
		dbuffer_extract(buf, (char *)ip, sizeof(ip));

		/* parse out port */
		port = NET_ReadByte(buf) << 8;
		port += NET_ReadByte(buf);
		Com_sprintf(node, sizeof(node), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
		Com_sprintf(service, sizeof(service), "%d", port);
		Com_DPrintf("server: [%s]:%s\n", node, service);
		CL_AddServerToList(node, service);
	}
	/* end of stream */
}

static void masterserver_callback (struct net_stream *s)
{
	struct dbuffer *buf = NET_ReadMsg(s);

	if (buf) {
		int cmd = NET_ReadByte(buf);
		char *str = NET_ReadStringLine(buf);

		if (cmd == clc_oob && Q_strncmp(str, "servers", 7) == 0) {
			CL_ParseMasterServerResponse(buf);
		}

		free_stream(s);
	}
}

/**
 * @brief
 *
 * called via server_connect
 * FIXME: Spectator needs no team
 */
static void CL_ServerConnect_f (void)
{
	const char *ip = Cvar_VariableString("mn_server_ip");

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
	const char *bookmark = NULL;
	const char *newBookmark = NULL;

	if (Cmd_Argc() < 2) {
		newBookmark = Cvar_VariableString("mn_server_ip");
		if (!newBookmark) {
			Com_Printf("usage: bookmark_add <ip>\n");
			return;
		}
	} else
		newBookmark = Cmd_Argv(1);

	for (i = 0; i < 16; i++) {
		bookmark = Cvar_VariableString(va("adr%i", i));
		if (!*bookmark) {
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
static void CL_BookmarkListClick_f (void)
{
	int num;
	const char *bookmark = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("usage: bookmarks_click <num>\n");
		return;
	}
	num = atoi(Cmd_Argv(1));
	bookmark = Cvar_VariableString(va("adr%i", num));

	if (bookmark) {
		Cvar_Set("mn_server_ip", bookmark);
		/* XXX: I don't think this feature does anything sensible right now, fix it later */
		/* Netchan_OutOfBandPrint(NS_CLIENT, adr, "status %i", PROTOCOL_VERSION); */
	}
}

/**
 * @brief
 */
static void CL_ServerInfo_f (void)
{
	struct net_stream *s;
	if (Cmd_Argc() < 2)
		s = connect_to_host(Cvar_VariableString("mn_server_ip"), va("%d", PORT_SERVER));
	else
		s = connect_to_host(Cmd_Argv(1), va("%d", PORT_SERVER));
	NET_OOB_Printf(s, "status %i", PROTOCOL_VERSION);
	stream_callback(s, &status_callback);
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
				/* XXX: I don't think this feature does anything sensible right now, fix it later */
				/*Netchan_OutOfBandPrint(NS_CLIENT, serverList[i].adr, "status %i", PROTOCOL_VERSION);*/
				Cvar_Set("mn_server_ip", serverList[i].node);
				return;
			}
}

/**
 * @brief Send the teaminfo string to server
 * @sa CL_ParseTeamInfoMessage
 */
static void CL_SelectTeam_Init_f (void)
{
	/* reset menu text */
	menuText[TEXT_STANDARD] = NULL;

	NET_OOB_Printf(cls.stream, "teaminfo %i", PROTOCOL_VERSION);
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
	int i;
	char name[6];
	const char *adrstring;

	/* refresh the list */
	if (Cmd_Argc() == 2) {
		/* reset current list */
		serverText[0] = 0;
		serverListPos = 0;
		serverListLength = 0;
		serversAlreadyQueried = qfalse;
		for (i = 0; i < MAX_SERVERLIST; i++) {
			free(serverList[i].node);
			free(serverList[i].service);
		}
		memset(serverList, 0, sizeof(serverList_t) * MAX_SERVERLIST);
	}

/*	menuText[TEXT_STANDARD] = NULL;*/
	menuText[TEXT_LIST] = serverText;

	for (i = 0; i < 16; i++) {
		char service[256];
		const char *p;
		Com_sprintf(name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString(name);
		if (!adrstring || !adrstring[0])
			continue;

		p = strrchr(adrstring, ':');
		if (p)
			Q_strncpyz(service, p + 1, sizeof(service));
		else
			Com_sprintf(service, sizeof(service), "%d", PORT_SERVER);
		CL_AddServerToList(adrstring, service);
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
	if (Cmd_Argc() == 2 || Q_strcmp(Cmd_Argv(1), "local")) {
		struct net_stream *s = connect_to_host(masterserver_ip->string, masterserver_port->string);
		NET_OOB_Printf(s, "getservers 0\n");
		stream_callback(s, &masterserver_callback);
	}
}


/**
 * @brief Responses to broadcasts, etc
 * @sa CL_ReadPackets
 * @sa CL_Frame
 */
static void CL_ConnectionlessPacket (struct dbuffer *msg)
{
	char *s;
	const char *c;
	int i;

	s = NET_ReadStringLine(msg);

	Cmd_TokenizeString(s, qfalse);

	c = Cmd_Argv(0);

	Com_DPrintf("server OOB: %s\n", Cmd_Args());

	/* server connection */
	if (!Q_strncmp(c, "client_connect", 13)) {
		const char *p;
		for (i = 1; i < Cmd_Argc(); i++) {
			p = Cmd_Argv(i);
			if (!Q_strncmp(p, "dlserver=", 9)) {
#ifdef HAVE_CURL
				p += 9;
				Com_sprintf(cls.downloadReferer, sizeof(cls.downloadReferer), "ufo://%s", cls.servername);
				CL_SetHTTPServer(p);
				if (cls.downloadServer[0])
					Com_Printf("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
#else
				Com_Printf("HTTP downloading supported by server but this client was built without HAVE_CURL, bad luck.\n");
#endif /* HAVE_CURL */
			}
		}
		if (cls.state == ca_connected) {
			Com_Printf("Dup connect received. Ignored.\n");
			return;
		}
		msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, "new");
		NET_WriteMsg(cls.stream, msg);
		cls.state = ca_connected;
		return;
	}

	/* remote command from gui front end */
	if (!Q_strncmp(c, "cmd", 3)) {
		if (!stream_is_loopback(cls.stream)) {
			Com_Printf("Command packet from remote host. Ignored.\n");
			return;
		}
		Sys_AppActivate();
		s = NET_ReadString(msg);
		Cbuf_AddText(s);
		Cbuf_AddText("\n");
		return;
	}

	/* teaminfo command */
	if (!Q_strncmp(c, "teaminfo", 8)) {
		CL_ParseTeamInfoMessage(msg);
		return;
	}

	/* ping from server */
	if (!Q_strncmp(c, "ping", 4)) {
		NET_OOB_Printf(cls.stream, "ack");
		return;
	}

	/* echo request from server */
	if (!Q_strncmp(c, "echo", 4)) {
		NET_OOB_Printf(cls.stream, "%s", Cmd_Argv(1));
		return;
	}

	Com_Printf("Unknown command.\n");
}

/**
 * @brief
 * @sa CL_ConnectionlessPacket
 * @sa CL_Frame
 * @sa CL_ParseServerMessage
 */
static void CL_ReadPackets (void)
{
	struct dbuffer *msg;
	while ((msg = NET_ReadMsg(cls.stream))) {
		int cmd = NET_ReadByte(msg);
		if (cmd == clc_oob)
			CL_ConnectionlessPacket(msg);
		else
			CL_ParseServerMessage(cmd, msg);
		free_dbuffer(msg);
	}
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
void CL_Snd_Restart_f (void)
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
			struct dbuffer *msg = new_dbuffer();
			CL_SendCurTeamInfo(msg, baseCurrent->curTeam, amount);
			NET_WriteMsg(cls.stream, msg);
		}
	}

	{
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, va("spawn %i\n", spawnCountFromServer));
		NET_WriteMsg(cls.stream, msg);
	}

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

	/* for singleplayer game this is already loaded in our local server */
	/* and if we are the server we don't have to reload the map here, too */
	if (!Com_ServerState()) {
		/* check download */
		if (precache_check == CS_MODELS) { /* confirm map */
			if (*cl.configstrings[CS_TILES] == '+') {
			} else {
				if (!CL_CheckOrDownloadFile(va("maps/%s.bsp", cl.configstrings[CS_TILES])))
					return; /* started a download */
			}
			precache_check = CS_MODELS + 1;
		}
#ifdef HAVE_CURL
		/* map might still be downloading? */
		if (CL_PendingHTTPDownloads())
			return;
#endif /* HAVE_CURL */

		/* activate the map loading screen for multiplayer, too */
		SCR_BeginLoadingPlaque();

		CM_LoadMap(cl.configstrings[CS_TILES], cl.configstrings[CS_POSITIONS], &map_checksum);
		if (!*cl.configstrings[CS_VERSION] || !*cl.configstrings[CS_MAPCHECKSUM] || !*cl.configstrings[CS_UFOCHECKSUM]) {
			Com_sprintf(popupText, sizeof(popupText), _("Local game version (%s) differs from the servers"), UFO_VERSION);
			MN_Popup(_("Error"), popupText);
			Com_Error(ERR_DISCONNECT, "Local game version (%s) differs from the servers", UFO_VERSION);
			return;
		/* checksum doesn't match with the one the server gave us via configstring */
		} else if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM])) {
			MN_Popup(_("Error"), _("Local map version differs from server"));
			Com_Error(ERR_DISCONNECT, "Local map version differs from server: %u != '%s'\n",
				map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		/* checksum doesn't match with the one the server gave us via configstring */
		} else if (atoi(cl.configstrings[CS_UFOCHECKSUM]) && ufoScript_checksum != atoi(cl.configstrings[CS_UFOCHECKSUM])) {
			MN_Popup(_("Error"), _("You are using modified ufo script files - you won't be able to connect to the server"));
			Com_Error(ERR_DISCONNECT, "You are using modified ufo script files - you won't be able to connect to the server: %u != '%s'\n",
				ufoScript_checksum, cl.configstrings[CS_UFOCHECKSUM]);
			return;
		} else if (Q_strncmp(UFO_VERSION, cl.configstrings[CS_VERSION], sizeof(UFO_VERSION))) {
			Com_sprintf(popupText, sizeof(popupText), _("Local game version (%s) differs from the servers (%s)"), UFO_VERSION, cl.configstrings[CS_VERSION]);
			MN_Popup(_("Error"), popupText);
			Com_Error(ERR_DISCONNECT, "Local game version (%s) differs from the servers (%s)", UFO_VERSION, cl.configstrings[CS_VERSION]);
			return;
		}
	}

	CL_RegisterSounds();
	CL_PrepRefresh();

	soldiersSpawned = qfalse;
	spawnCountFromServer = atoi(Cmd_Argv(1));

	{
		struct dbuffer *msg = new_dbuffer();
		/* send begin */
		/* this will activate the render process (see client state ca_active) */
		NET_WriteByte(msg, clc_stringcmd);
		/* see CL_StartGame */
		NET_WriteString(msg, va("begin %i\n", spawnCountFromServer));
		NET_WriteMsg(cls.stream, msg);
	}

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
 * @brief Precaches all models at game startup - for faster access
 * @note only called when cl_precache was set to 1
 * @sa MN_PrecacheModels
 * @sa E_PrecacheModels
 */
static void CL_PrecacheModels (void)
{
	int i;
	float loading;

	MN_PrecacheModels(); /* 20% */
	Com_PrecacheCharacterModels(); /* 20% */

	loading = cls.loadingPercent;

	for (i = 0; i < csi.numODs; i++) {
		if (*csi.ods[i].model)
			if (!re.RegisterModel(csi.ods[i].model))
				Com_Printf("CL_PrecacheModels: Could not register object model: '%s'\n", csi.ods[i].model);
		cls.loadingPercent += 20.0f / csi.numODs;
		SCR_DrawPrecacheScreen(qtrue);
	}
	/* ensure 20% */
	cls.loadingPercent = loading + 20.0f;
	SCR_DrawPrecacheScreen(qtrue);
}

/**
 * @brief Init function for clients - called after menu was inited and ufo-scripts were parsed
 * @sa Qcommon_Init
 */
void CL_InitAfter (void)
{
	int i;
	menu_t* menu;
	menuNode_t* vidModesOptions;
	selectBoxOptions_t* selectBoxOption;

	cls.loadingPercent = 2.0f;

	/* precache loading screen */
	SCR_DrawPrecacheScreen(qtrue);

	/* init irc commands and cvars */
	Irc_Init();

	/* this will init some more employee stuff */
	E_Init();

	/* init some production menu nodes */
	PR_Init();

	FS_GetMaps(qfalse);

	cls.loadingPercent = 5.0f;
	SCR_DrawPrecacheScreen(qtrue);

	/* preload all models for faster access */
	if (cl_precache->integer) {
		CL_PrecacheModels(); /* 60% */
		/* loading percent is 65 now */
		MN_PrecacheMenus(); /* 35% */
	}

	cls.loadingPercent = 100.0f;
	SCR_DrawPrecacheScreen(qtrue);

	/* link for faster access */
	MN_LinkMenuModels();

	menu = MN_GetMenu("options_video");
	if (!menu)
		Sys_Error("Could not find menu options_video\n");
	vidModesOptions = MN_GetNode(menu, "select_res");
	if (!vidModesOptions)
		Sys_Error("Could not find node select_res in menu options_video\n");
	for (i = 0; i < maxVidModes; i++) {
		selectBoxOption = MN_AddSelectboxOption(vidModesOptions);
		if (!selectBoxOption) {
			return;
		}
		Com_sprintf(selectBoxOption->label, sizeof(selectBoxOption->label), "%i:%i", vid_modes[i].width, vid_modes[i].height);
		Com_sprintf(selectBoxOption->value, sizeof(selectBoxOption->value), "%i", vid_modes[i].mode);
	}

	CL_LanguageInit();
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
 * @note This data should not go into cl_localPool memory pool - this data is
 * persistent until you shutdown the game
 */
void CL_ParseClientData (const char *type, const char *name, const char **text)
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
		AIR_ParseAircraft(name, text, qfalse);
	else if (!Q_strncmp(type, "campaign", 8))
		CL_ParseCampaign(name, text);
	else if (!Q_strncmp(type, "ugv", 3))
		CL_ParseUGVs(name, text);
	else if (!Q_strncmp(type, "tips", 4))
		CL_ParseTipsOfTheDay(name, text);
	else if (!Q_strncmp(type, "language", 8))
		CL_ParseLanguages(name, text);
}

/**
 * @brief Parsing only for singleplayer
 *
 * parsed if we are no dedicated server
 * first stage parses all the main data into their struct
 * see CL_ParseScriptSecond for more details about parsing stages
 * @sa CL_ReadSinglePlayerData
 * @sa Com_ParseScripts
 * @sa CL_ParseScriptSecond
 * @note write into cl_localPool - free on every game restart and reparse
 */
static void CL_ParseScriptFirst (const char *type, const char *name, const char **text)
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
	else if (!Q_strncmp(type, "craftitem", 8))
		AII_ParseAircraftItem(name, text);
	else if (!Q_strncmp(type, "base", 4))
		B_ParseBases(name, text);
	else if (!Q_strncmp(type, "nation", 6))
		CL_ParseNations(name, text);
	else if (!Q_strncmp(type, "rank", 4))
		CL_ParseMedalsAndRanks(name, text, qtrue);
	else if (!Q_strncmp(type, "mail", 4))
		CL_ParseEventMails(name, text);
	else if (!Q_strncmp(type, "components", 10))
		INV_ParseComponents(name, text);
#if 0
	else if (!Q_strncmp(type, "medal", 5))
		Com_ParseMedalsAndRanks( name, &text, qfalse );
#endif
}

/**
 * @brief Parsing only for singleplayer
 *
 * parsed if we are no dedicated server
 * second stage links all the parsed data from first stage
 * example: we need a techpointer in a building - in the second stage the buildings and the
 * techs are already parsed - so now we can link them
 * @sa CL_ReadSinglePlayerData
 * @sa Com_ParseScripts
 * @sa CL_ParseScriptFirst
 * @note make sure that the client hunk was cleared - otherwise it may overflow
 * @note write into cl_localPool - free on every game restart and reparse
 */
static void CL_ParseScriptSecond (const char *type, const char *name, const char **text)
{
	/* check for client interpretable scripts */
	if (!Q_strncmp(type, "stage", 5))
		CL_ParseStage(name, text);
	else if (!Q_strncmp(type, "building", 8))
		B_ParseBuildings(name, text, qtrue);
	else if (!Q_strncmp(type, "aircraft", 8))
		AIR_ParseAircraft(name, text, qtrue);
}

/** @brief struct that holds the sanity check data */
typedef struct {
	qboolean (*check)(void);	/**< function pointer to check function */
	const char* name;			/**< name of the subsystem to check */
} sanity_functions_t;

/** @brief Data for sanity check of parsed script data */
static const sanity_functions_t sanity_functions[] = {
	{B_ScriptSanityCheck, "buildings"},
	{RS_ScriptSanityCheck, "tech"},
	{AIR_ScriptSanityCheck, "aircraft"},

	{NULL, NULL}
};

/**
 * @brief Check the parsed script values for errors after parsing every script file
 * @sa CL_ReadSinglePlayerData
 */
void CL_ScriptSanityCheck (void)
{
	qboolean status;
	const sanity_functions_t *s;

	Com_Printf("Sanity check for script data\n");
	s = sanity_functions;
	while (s->check) {
		status = s->check();
		Com_Printf("...%s %s\n", s->name, (status ? "ok" : "failed"));
		s++;
	}
}

/**
 * @brief Read the data into gd for singleplayer campaigns
 * @sa SAV_GameLoad
 * @sa CL_GameNew_f
 * @sa CL_ResetSinglePlayerData
 */
void CL_ReadSinglePlayerData (void)
{
	const char *type, *name, *text;

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

	Com_Printf("Global data loaded - size "UFO_SIZE_T" bytes\n", sizeof(gd));
	Com_Printf("...techs: %i\n", gd.numTechnologies);
	Com_Printf("...buildings: %i\n", gd.numBuildingTypes);
	Com_Printf("...ranks: %i\n", gd.numRanks);
	Com_Printf("...nations: %i\n", gd.numNations);
	Com_Printf("\n");
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

/** @brief Cvars for initial check (popup at first start) */
static cvarList_t checkcvar[] = {
	{"name", NULL, NULL},

	{NULL, NULL, NULL}
};
/**
 * @brief Check cvars for some initial values that should/must be set
 */
static void CL_CheckCvars_f (void)
{
	int i = 0;

	while (checkcvar[i].name) {
		if (!checkcvar[i].var)
			checkcvar[i].var = Cvar_Get(checkcvar[i].name, "", 0, NULL);
		if (!*(checkcvar[i].var->string))
			Cbuf_AddText("mn_push checkcvars;");
		i++;
	}
}

#ifdef DEBUG
/**
 * @brief Print the configstrings to game console
 */
static void CL_ShowConfigstrings_f (void)
{
	int i;
	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
		if (*cl.configstrings[i])
			Com_Printf("cl.configstrings[%2i]: %s\n", i, cl.configstrings[i]);
}

/**
 * @brief Shows the sizes some parts of globalData_t uses - this is only to
 * analyse where the most optimization potential is hiding
 */
static void CL_GlobalDataSizes_f (void)
{
	Com_Printf(
		"globalData_t size: %10Zu bytes\n"
		"bases              %10Zu bytes\n"
		"buildings          %10Zu bytes\n"
		"nations            %10Zu bytes\n"
		"ranks              %10Zu bytes\n"
		"ugv                %10Zu bytes\n"
		"productions        %10Zu bytes\n"
		"buildingTypes      %10Zu bytes\n"
		"employees          %10Zu bytes\n"
		"eventMails         %10Zu bytes\n"
		"upChapters         %10Zu bytes\n"
		"technologies       %10Zu bytes\n"
		,
		sizeof(globalData_t),
		sizeof(gd.bases),
		sizeof(gd.buildings),
		sizeof(gd.nations),
		sizeof(gd.ranks),
		sizeof(gd.ugvs),
		sizeof(gd.productions),
		sizeof(gd.buildingTypes),
		sizeof(gd.employees),
		sizeof(gd.eventMails),
		sizeof(gd.upChapters),
		sizeof(gd.technologies)
	);

	Com_Printf(
		"bases:\n"
		"alienscont         %10Zu bytes\n"
		"capacities         %10Zu bytes\n"
		"equipByBuyType     %10Zu bytes\n"
		"hospitalList       %10Zu bytes\n"
		"hospitalMissionLst %10Zu bytes\n"
		"aircraft           %10Zu bytes\n"
		"aircraft (single)  %10Zu bytes\n"
		"allBuildingsList   %10Zu bytes\n"
		"radar              %10Zu bytes\n"
		,
		sizeof(gd.bases[0].alienscont),
		sizeof(gd.bases[0].capacities),
		sizeof(gd.bases[0].equipByBuyType),
		sizeof(gd.bases[0].hospitalList),
		sizeof(gd.bases[0].hospitalMissionList),
		sizeof(gd.bases[0].aircraft),
		sizeof(aircraft_t),
		sizeof(gd.bases[0].allBuildingsList),
		sizeof(gd.bases[0].radar)
	);
}

/**
 * @brief Dumps the globalData_t structure to a file
 */
static void CL_DumpGlobalDataToFile_f (void)
{
	qFILE f;

	memset(&f, 0, sizeof(qFILE));
	FS_FOpenFileWrite(va("%s/gd.dump", FS_Gamedir()), &f);
	if (!f.f) {
		Com_Printf("CL_DumpGlobalDataToFile_f: Error opening dump file for writing");
		return;
	}

	FS_Write(&gd, sizeof(gd), &f);

	FS_FCloseFile(&f);
}
#endif /* DEBUG */

/**
 * @brief Calls all reset functions for all subsystems like production and research
 * also inits the cvars and commands
 * @sa CL_Init
 */
static void CL_InitLocal (void)
{
	int i;

	memset(serverList, 0, sizeof(serverList_t) * MAX_SERVERLIST);

	cls.state = ca_disconnected;
	cls.stream = NULL;
	cls.realtime = Sys_Milliseconds();

	Com_InitInventory(invList);
	Con_CheckResize();
	CL_InitInput();

	SAV_Init();
	MN_ResetMenus();
	CL_ResetParticles();
	CL_ResetCampaign();
	BS_ResetMarket();
	CL_ResetSequences();
	CL_ResetTeams();

	CL_TipOfTheDayInit();

	for (i = 0; i < 16; i++)
		Cvar_Get(va("adr%i", i), "", CVAR_ARCHIVE, "Bookmark for network ip");

	map_dropship = Cvar_Get("map_dropship", "craft_drop_firebird", 0, "The dropship that is to be used in the map");

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

	cl_precache = Cvar_Get("cl_precache", "1", CVAR_ARCHIVE, "Precache models and menus at startup");

	cl_aviForceDemo = Cvar_Get("cl_aviForceDemo", "1", CVAR_ARCHIVE, "AVI recording - record even if no game is loaded");
	cl_aviFrameRate = Cvar_Get("cl_aviFrameRate", "25", CVAR_ARCHIVE, "AVI recording - see video command");
	cl_aviMotionJpeg = Cvar_Get("cl_aviMotionJpeg", "1", CVAR_ARCHIVE, "AVI recording - see video command");

	cl_particleWeather = Cvar_Get("cl_particleweather", "0", CVAR_ARCHIVE | CVAR_LATCH, "Switch the weather particles on or off");

	cl_fps = Cvar_Get("cl_fps", "0", CVAR_ARCHIVE, "Show frames per second");
	cl_shownet = Cvar_Get("cl_shownet", "0", CVAR_ARCHIVE, NULL);
	cl_timeout = Cvar_Get("cl_timeout", "120", 0, NULL);
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

	Cvar_Set("music", "");

	mn_main = Cvar_Get("mn_main", "main", 0, "Which is the main menu id to return to");
	mn_sequence = Cvar_Get("mn_sequence", "sequence", 0, "Which is the sequence menu node to render the sequence in");
	mn_active = Cvar_Get("mn_active", "", 0, NULL);
	mn_hud = Cvar_Get("mn_hud", "hud", CVAR_ARCHIVE, "Which is the current selected hud");
	mn_lastsave = Cvar_Get("mn_lastsave", "", CVAR_ARCHIVE, NULL);

	mn_serverlist = Cvar_Get("mn_serverlist", "0", CVAR_ARCHIVE, "0=show all, 1=hide full - servers on the serverlist");

	mn_inputlength = Cvar_Get("mn_inputlength", "32", 0, "Limit the input length for messagemenu input");
	mn_inputlength->modified = qfalse;

	con_fontWidth = Cvar_Get("con_fontWidth", "16", CVAR_NOSET, NULL);
	con_fontHeight = Cvar_Get("con_fontHeight", "32", CVAR_NOSET, NULL);
	con_fontShift = Cvar_Get("con_fontShift", "4", CVAR_NOSET, NULL);

	/* userinfo */
	info_password = Cvar_Get("password", "", CVAR_USERINFO, NULL);
	info_spectator = Cvar_Get("spectator", "0", CVAR_USERINFO, NULL);
	name = Cvar_Get("name", "", CVAR_USERINFO | CVAR_ARCHIVE, "Playername");
	snd_ref = Cvar_Get("snd_ref", "sdl", CVAR_ARCHIVE, "Sound renderer");
	team = Cvar_Get("team", "human", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	equip = Cvar_Get("equip", "multiplayer_initial", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	teamnum = Cvar_Get("teamnum", "1", CVAR_USERINFO | CVAR_ARCHIVE, "Teamnum for multiplayer teamplay games");
	campaign = Cvar_Get("campaign", "main", 0, "Which is the current selected campaign id");
	msg = Cvar_Get("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE, "Sets the message level for server messages the client receives");
	sv_maxclients = Cvar_Get("maxclients", "1", CVAR_SERVERINFO, "If maxclients is 1 we are in singleplayer - otherwise we are mutliplayer mode (see sv_teamplay)");

	masterserver_ip = Cvar_Get("masterserver_ip", "195.136.48.62", CVAR_ARCHIVE, "IP address of UFO:AI masterserver (Sponsored by NineX)");
	masterserver_port = Cvar_Get("masterserver_port", "27900", CVAR_ARCHIVE, "Port of UFO:AI masterserver");

	log_stats = Cvar_Get("log_stats", "0", 0, NULL);

#ifdef HAVE_CURL
	cl_http_proxy = Cvar_Get("cl_http_proxy", "", 0, NULL);
	cl_http_filelists = Cvar_Get("cl_http_filelists", "1", 0, NULL);
	cl_http_downloads = Cvar_Get("cl_http_downloads", "1", 0, "Try to download files via http");
	cl_http_max_connections = Cvar_Get("cl_http_max_connections", "1", 0, NULL);
#endif /* HAVE_CURL */

	/* register our commands */
	Cmd_AddCommand("cmd", CL_ForwardToServer_f, "Forward to server");
	Cmd_AddCommand("pingservers", CL_PingServers_f, "Ping all servers in local network to get the serverlist");

	Cmd_AddCommand("check_cvars", CL_CheckCvars_f, "Check cvars like playername and so on");

	Cmd_AddCommand("saveconfig", CL_WriteConfiguration, "Save the configuration");

	Cmd_AddCommand("targetalign", CL_ActorTargetAlign_f, _("Target your shot to the ground"));

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

	/*Cmd_AddCommand("rcon", CL_Rcon_f, "Execute a rcon command - see rcon_password");*/

#ifdef ACTIVATE_PACKET_COMMAND
	/* this is dangerous to leave in */
	Cmd_AddCommand("packet", CL_Packet_f, "Dangerous debug function for network testing");
#endif

	Cmd_AddCommand("env", CL_Env_f, NULL);

	Cmd_AddCommand("precache", CL_Precache_f, "Function that is called at mapload to precache map data");
	Cmd_AddCommand("mp_selectteam_init", CL_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team");
	Cmd_AddCommand("mp_wait_init", CL_WaitInit_f, "Function that inits some nodes");
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
	Cmd_AddCommand("debug_aircraftsamplelist", AIR_ListAircraftSamples_f, "Show aircraft parameter on game console");
	Cmd_AddCommand("debug_configstrings", CL_ShowConfigstrings_f, "Print configstrings to game console");
	Cmd_AddCommand("debug_gddump", CL_DumpGlobalDataToFile_f, "Dumps gd to a file");
	Cmd_AddCommand("debug_gdstats", CL_GlobalDataSizes_f, "Show globalData_t sizes");
	Cmd_AddCommand("actorinvlist", NULL, "Shows the inventory list of all actors");
	Cmd_AddCommand("killteam", NULL, NULL);
	Cmd_AddCommand("stunteam", NULL, NULL);
#endif
}

/**
 * @brief
 */
static void CL_SendCmd (void)
{
	/* send a userinfo update if needed */
	if (cls.state >= ca_connected) {
		if (userinfo_modified) {
			struct dbuffer *msg = new_dbuffer();
			NET_WriteByte(msg, clc_userinfo);
			NET_WriteString(msg, Cvar_Userinfo());
			NET_WriteMsg(cls.stream, msg);
			userinfo_modified = qfalse;
		}
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
	Cvar_FixCheatVars();

	/* resend a connection request if necessary */
	CL_CheckForResend();
}

/**
 * @brief Translate the difficulty int to a translated string
 * @param difficulty the difficulty integer value
 */
const char* CL_ToDifficultyName (int difficulty)
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
		CL_LanguageTryToSet(s_language->string);
#endif

	if (mn_inputlength->modified) {
		if (mn_inputlength->integer >= MAX_CVAR_EDITING_LENGTH)
			Cvar_SetValue("mn_inputlength", MAX_CVAR_EDITING_LENGTH);
	}

	/* gl_mode and fullscreen */
	v = Cvar_VariableInteger("mn_glmode");
	if (v < -1 || v >= maxVidModes) {
		Com_Printf("Max gl_mode mode is %i (%i)\n", maxVidModes, v);
		v = Cvar_VariableInteger("gl_mode");
		Cvar_SetValue("mn_glmode", v);
	}
	if (v >= 0)
		Cvar_Set("mn_glmodestr", va("%i*%i", vid_modes[v].width, vid_modes[v].height));
	else
		Cvar_Set("mn_glmodestr", _("Custom"));
}


#define NUM_DELTA_FRAMES	20
/**
 * @brief
 * @sa Qcommon_Frame
 */
void CL_Frame (int now, void *data)
{
	static int lasttimecalled = 0;
	static int last_frame = 0;
	int delta;

	if (log_stats->modified) {
		log_stats->modified = qfalse;
		if (log_stats->integer) {
			if (log_stats_file) {
				fclose(log_stats_file);
				log_stats_file = NULL;
			}
			log_stats_file = fopen("stats.log", "w");
			if (log_stats_file)
				fprintf(log_stats_file, "entities,dlights,parts,frame time\n");
		} else {
			if (log_stats_file) {
				fclose(log_stats_file);
				log_stats_file = NULL;
			}
		}
	}

	if (sv_maxclients->modified) {
		if (sv_maxclients->integer > 1 && ccs.singleplayer) {
			CL_StartSingleplayer(qfalse);
			Com_Printf("Changing to Multiplayer\n");
		} else if (sv_maxclients->integer == 1) {
			CL_StartSingleplayer(qtrue);
			Com_Printf("Changing to Singleplayer\n");
		}
		sv_maxclients->modified = qfalse;
	}

	/* decide the simulation time */
	delta = now - last_frame;
	if (last_frame)
		cls.frametime = delta / 1000.0;
	else
		cls.frametime = 0;
	cls.realtime = curtime;
	cl.time = now;
	last_frame = now;
	if (!blockEvents)
		cl.eventTime += delta;

	/* frame rate calculation */
	cls.framedelta += delta;
	if (0 == (cls.framecount % NUM_DELTA_FRAMES)) {
		cls.framerate = NUM_DELTA_FRAMES * 1000.0 / cls.framedelta;
		cls.framedelta = 0;
	}

#if 0
	/* don't extrapolate too far ahead */
	if (cls.frametime > (1.0 / 5))
		cls.frametime = (1.0 / 5);
#endif

#ifdef HAVE_CURL
	if (cls.state == ca_connected) {
		/* we run full speed when connecting */
		CL_RunHTTPDownloads();
	}
#endif /* HAVE_CURL */

	/* fetch results from server */
	CL_ReadPackets();

	CL_SendCommand();

	CL_ParseInput();
	/* update camera position */
	CL_CameraMove();

	/* avi recording needs to move out to its own timer */
#if 0
	/* if recording an avi, lock to a fixed fps */
	if (CL_VideoRecording() && refreshDelta < 1000.0 / cl_aviFrameRate->value) {
		/* save the current screen */
		if (cls.state == ca_active || cl_aviForceDemo->integer)
			CL_TakeVideoFrame();
	}
#endif
	/* end the rounds when no soldier is alive */
	CL_RunMapParticles();
	CL_ParticleRun();

	/* update the screen */
	SCR_UpdateScreen();

	/* update audio */
	S_Update(cl.refdef.vieworg, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);

	CDAudio_Update();

	/* advance local effects for next frame */
	CL_RunLightStyles();
	SCR_RunConsole();

	/* LE updates */
	LE_Think();

	/* send a new command message to the server */
	CL_SendCommand();

	cls.framecount++;

	if (log_stats->integer) {
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
 * @sa CL_Frame
 */
void CL_SlowFrame (int now, void *data)
{
	/* let the mouse activate or deactivate */
	IN_Frame();

	CL_CvarCheck();

	/* allow rendering DLL change */
	VID_CheckChanges();
	if (!cl.refresh_prepped && cls.state == ca_active)
		CL_PrepRefresh();

	CL_ActorUpdateCVars();
}

/**
 * @brief
 * @sa CL_Shutdown
 * @sa CL_InitAfter
 */
void CL_Init (void)
{
	if (dedicated->value)
		return;					/* nothing running on the client */

	cl_localPool = Mem_CreatePool("Client: Local (per game)");
	cl_genericPool = Mem_CreatePool("Client: Generic");
	cl_menuSysPool = Mem_CreatePool("Client: Menu");
	cl_soundSysPool = Mem_CreatePool("Client: Sound system");
	cl_ircSysPool = Mem_CreatePool("Client: IRC system");

	/* all archived variables will now be loaded */
	Con_Init();

	CIN_Init();

#ifdef _WIN32
	/* sound must be initialized after window is created */
	VID_Init();
	S_Init();
#else
	S_Init();
	VID_Init();
#endif

	V_Init();

	SCR_Init();
	cls.loadingPercent = 0.0f;
	SCR_DrawPrecacheScreen(qfalse);

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

	/* Touch memory */
	Mem_TouchGlobal();
}


/**
 * @brief Saves configuration file and shuts the client systems down
 * FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
 * to run quit through here before the final handoff to the sys code.
 * @sa Sys_Quit
 * @sa CL_Init
 */
void CL_Shutdown (void)
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
	CIN_Shutdown();
	FS_Shutdown();
}
