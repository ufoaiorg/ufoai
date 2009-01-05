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
#include "cl_tutorials.h"
#include "cl_global.h"
#include "cl_tip.h"
#include "cl_team.h"
#include "cl_team_multiplayer.h"
#include "cl_rank.h"
#include "cl_language.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_ugv.h"
#include "campaign/cl_basesummary.h"
#include "campaign/cl_installation.h"
#include "campaign/cl_hospital.h"
#include "campaign/cl_map.h"
#include "campaign/cl_mapfightequip.h"
#include "campaign/cl_ufo.h"
#include "campaign/cl_alienbase.h"
#include "cl_sequence.h"
#include "cl_view.h"
#include "cl_joystick.h"
#include "../shared/infostring.h"
#include "renderer/r_main.h"
#include "renderer/r_particle.h"
#include "menu/m_popup.h"
#include "menu/m_main.h"
#include "menu/m_font.h"
#include "menu/m_parse.h"
#include "menu/node/m_node_selectbox.h"
#include "campaign/cp_parse.h"

cvar_t *cl_isometric;

static cvar_t *rcon_client_password;

cvar_t *cl_fps;
cvar_t *cl_particleweather;
cvar_t *cl_leshowinvis;
cvar_t *cl_logevents;
cvar_t *cl_centerview;
cvar_t *cl_worldlevel;
cvar_t *cl_selected;
cvar_t *cl_3dmap;				/**< 3D geoscape or flat geoscape */
cvar_t *cl_numnames;
/** Player preference: should the server make guys stand for long walks, to save TU. */
cvar_t *cl_autostand;

cvar_t *mn_main;
cvar_t *mn_sequence;
cvar_t *mn_active;
cvar_t *mn_afterdrop;
cvar_t *mn_main_afterdrop;
cvar_t *mn_hud;
cvar_t *mn_serverday;
cvar_t *mn_inputlength;

cvar_t *cl_lastsave;
cvar_t *cl_start_employees;
cvar_t *cl_initial_equipment;
cvar_t *cl_start_buildings;
cvar_t* cl_showCoords;
cvar_t* cl_mapDebug;

static cvar_t *cl_connecttimeout; /* multiplayer connection timeout value (ms) */

/** @brief Confirm actions in tactical mode - valid values are 0, 1 and 2 */
cvar_t *confirm_actions;

static cvar_t *cl_shownet;
static cvar_t *cl_precache;
static cvar_t *cl_introshown;
static cvar_t *cl_serverlist;

/* userinfo */
static cvar_t *info_password;
static cvar_t *cl_name;
static cvar_t *cl_msg;
cvar_t *cl_teamnum;
cvar_t *cl_team;

client_static_t cls;
client_state_t cl;

/** @brief Are the soldiers for this game already spawned? */
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

static char serverText[1024];

static int precache_check;

static void CL_SpawnSoldiers_f(void);

#define MAX_BOOKMARKS 16

struct memPool_s *cl_localPool;		/**< reset on every game restart */
struct memPool_s *cl_genericPool;	/**< permanent client data - menu, fonts */
struct memPool_s *cl_ircSysPool;	/**< irc pool */
struct memPool_s *cl_soundSysPool;
struct memPool_s *cl_menuSysPool;
struct memPool_s *vid_genericPool;	/**< also holds all the static models */
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

	if (cls.state <= ca_connected || cmd[0] == '-' || cmd[0] == '+') {
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
	NET_WriteMsg(cls.netStream, msg);
}

/**
 * @brief Set or print some environment variables via console command
 * @sa Sys_Setenv
 */
static void CL_Env_f (void)
{
	const int argc = Cmd_Argc();

	if (argc == 3) {
		Sys_Setenv(Cmd_Argv(1), Cmd_Argv(2));
	} else if (argc == 2) {
		const char *env = SDL_getenv(Cmd_Argv(1));
		if (env)
			Com_Printf("%s=%s\n", Cmd_Argv(1), env);
		else
			Com_Printf("%s undefined\n", Cmd_Argv(1));
	}
}


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
		NET_WriteMsg(cls.netStream, msg);
	}
}

static void CL_Quit_f (void)
{
	CL_Disconnect();
	Com_Quit();
}

/**
 * @brief Called when skirmish or campaign game starts
 * @param[in] load qtrue if we are loading game, qfalse otherwise
 * @sa CL_CampaignInit
 */
void CL_GameInit (qboolean load)
{
	Com_AddObjectLinks();	/**< Add tech links + ammo<->weapon links to items.*/
	RS_InitTree(load);		/**< Initialise all data in the research tree. */

	/* now check the parsed values for errors that are not catched at parsing stage */
	if (!load)
		CL_ScriptSanityCheck();
}

/**
 * @brief Ensures the right menu cvars are set after error drop or mapchange
 * @note E.g. called after an ERR_DROP was thrown
 * @sa CL_Disconnect
 * @sa SV_Map
 */
void CL_Drop (void)
{
	/* drop loading plaque */
	SCR_EndLoadingPlaque();

	/** @todo check that, there are already no menu to pop */
	if (mn.menuStackPos) {
		MN_PopMenu(qtrue);
	}

	/* make sure that we are in the correct menus in singleplayer after
	 * dropping the game due to a failure */
	if (GAME_IsSingleplayer() && curCampaign) {
		Cvar_Set("mn_main", "singleplayerInGame");
		Cvar_Set("mn_active", "map");
		MN_PushMenu("map", NULL);
	} else {
		Cvar_Set("mn_main", "main");
		Cvar_Set("mn_active", "");
		MN_PushMenu("main", NULL);
		/* the main menu may have a init node - execute it */
		Cbuf_Execute();
	}

	if (*mn_afterdrop->string) {
		MN_PushMenu(mn_afterdrop->string, NULL);
		Cvar_Set("mn_afterdrop", "");
	}
	if (*mn_main_afterdrop->string) {
		Cvar_Set("mn_main", mn_main_afterdrop->string);
		Cvar_Set("mn_main_afterdrop", "");
	}

	if (cls.state == ca_uninitialized || cls.state == ca_disconnected)
		return;

	CL_Disconnect();
}

/**
 * @note Only call CL_Connect if there is no connection yet (cls.netStream is NULL)
 * @sa CL_Disconnect
 * @sa CL_SendChangedUserinfos
 */
static void CL_Connect (void)
{
	userinfo_modified = qfalse;

	NET_DatagramSocketClose(cls.netDatagramSocket);
	cls.netDatagramSocket = NULL;

	assert(!cls.netStream);

	if (cls.servername[0]) {
		assert(cls.serverport[0]);
		cls.netStream = NET_Connect(cls.servername, cls.serverport);
	} else
		cls.netStream = NET_ConnectToLoopBack();
	if (cls.netStream) {
		NET_OOB_Printf(cls.netStream, "connect %i \"%s\"\n", PROTOCOL_VERSION, Cvar_Userinfo());
		cls.connectTime = cls.realtime;
	} else {
		if (cls.servername[0]) {
			assert(cls.serverport[0]);
			Com_Printf("Could not connect to %s %s\n", cls.servername, cls.serverport);
		} else {
			Com_Printf("Could not connect to localhost\n");
		}
	}
}

static void CL_Connect_f (void)
{
	const char *server;
	const char *serverport;
	aircraft_t *aircraft;

	if (!cls.selectedServer && Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <server>\n", Cmd_Argv(0));
		return;
	}

	if (!GAME_IsMultiplayer()) {
		Com_Printf("Start multiplayer first\n");
		return;
	}

	aircraft = AIR_AircraftGetFromIdx(0);
	assert(aircraft);
	if (!B_GetNumOnTeam(aircraft)) {
		MN_Popup(_("Error"), _("Assemble a team first"));
		return;
	}

	if (Cvar_VariableInteger("mn_server_need_password")) {
		MN_PushMenu("serverpassword", NULL);
		return;
	}

	/* if running a local server, kill it and reissue */
	if (Com_ServerState())
		SV_Shutdown("Server quit.", qfalse);

	CL_Disconnect();

	if (Cmd_Argc() == 2) {
		server = Cmd_Argv(1);
		serverport = va("%d", PORT_SERVER);
	} else {
		assert(cls.selectedServer);
		server = cls.selectedServer->node;
		serverport = cls.selectedServer->service;
	}
	Q_strncpyz(cls.servername, server, sizeof(cls.servername));
	Q_strncpyz(cls.serverport, serverport, sizeof(cls.serverport));

	CL_SetClientState(ca_connecting);

	/* everything should be reasearched for multiplayer matches */
/*	RS_MarkResearchedAll(); */

	Cvar_Set("mn_main", "multiplayerInGame");
}

/**
 * Send the rest of the command line over as
 * an unconnected command.
 */
static void CL_Rcon_f (void)
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

	/** @todo Implement rcon_address to be able to access servers we are not
	 * connected to */
	if (cls.state < ca_connected) {
		Com_Printf("You are not connected to any server\n");
		return;
	}

	Com_sprintf(message, sizeof(message), "rcon %s %s",
		rcon_client_password->string, Cmd_Args());

	NET_OOB_Printf(cls.netStream, "%s", message);
}

/**
 * @sa CL_ParseServerData
 * @sa CL_Disconnect
 * @sa R_ClearScene
 */
void CL_ClearState (void)
{
	/* wipe the entire cl structure */
	memset(&cl, 0, sizeof(cl));
	cl.cam.zoom = 1.0;
	V_CalcFovX();

	numLEs = 0;
	numLMs = 0;
	numMPs = 0;
	/* wipe the particles with every new map */
	r_numParticles = 0;
}

/**
 * @brief Sets the cls.state to ca_disconnected and informs the server
 * @sa CL_Disconnect_f
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

	/* send a disconnect message to the server */
	if (!Com_ServerState()) {
		msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, "disconnect");
		NET_WriteMsg(cls.netStream, msg);
		/* make sure, that this is send */
		NET_Wait(0);
	}

	NET_StreamFinished(cls.netStream);
	cls.netStream = NULL;

	CL_ClearState();

	S_StopAllSounds();

	CL_SetClientState(ca_disconnected);
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
	int i, l;
	const char *in;
	char *out;
	struct net_stream *s;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <destination> <port> <contents>\n", Cmd_Argv(0));
		return;
	}

	s = NET_Connect(Cmd_Argv(1), Cmd_Argv(2));
	if (!s) {
		Com_Printf("Could not connect to %s at port %s\n", Cmd_Argv(1), Cmd_Argv(2));
		return;
	}

	in = Cmd_Argv(3);

	l = strlen(in);
	for (i = 0; i < l; i++) {
		if (in[i] == '\\' && in[i + 1] == 'n') {
			*out++ = '\n';
			i++;
		} else
			*out++ = in[i];
	}
	*out = 0;

	NET_OOB_Printf(s, va("%s %i", out, PROTOCOL_VERSION));
}
#endif

/**
 * @brief The server is changing levels
 */
static void CL_Reconnect_f (void)
{
	if (Com_ServerState())
		return;

	if (!GAME_IsMultiplayer()) {
		Com_Printf("Start multiplayer first\n");
		return;
	}

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
 * @sa CL_PingServerCallback
 * @sa SVC_Info
 */
static qboolean CL_ProcessPingReply (serverList_t *server, const char *msg)
{
	if (PROTOCOL_VERSION != atoi(Info_ValueForKey(msg, "sv_protocol"))) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ProcessPingReply: Protocol mismatch\n");
		return qfalse;
	}
	if (Q_strcmp(UFO_VERSION, Info_ValueForKey(msg, "sv_version"))) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ProcessPingReply: Version mismatch\n");
	}

	if (server->pinged)
		return qfalse;

	server->pinged = qtrue;
	Q_strncpyz(server->sv_hostname, Info_ValueForKey(msg, "sv_hostname"),
		sizeof(server->sv_hostname));
	Q_strncpyz(server->version, Info_ValueForKey(msg, "sv_version"),
		sizeof(server->version));
	Q_strncpyz(server->mapname, Info_ValueForKey(msg, "sv_mapname"),
		sizeof(server->mapname));
	Q_strncpyz(server->gametype, Info_ValueForKey(msg, "sv_gametype"),
		sizeof(server->gametype));
	server->clients = atoi(Info_ValueForKey(msg, "clients"));
	server->sv_dedicated = atoi(Info_ValueForKey(msg, "sv_dedicated"));
	server->sv_maxclients = atoi(Info_ValueForKey(msg, "sv_maxclients"));
	return qtrue;
}

typedef enum {
	SERVERLIST_SHOWALL,
	SERVERLIST_HIDEFULL,
	SERVERLIST_HIDEEMPTY
} serverListStatus_t;

/**
 * @brief CL_PingServer
 */
static void CL_PingServerCallback (struct net_stream *s)
{
	struct dbuffer *buf = NET_ReadMsg(s);
	serverList_t *server = NET_StreamGetData(s);
	const int cmd = NET_ReadByte(buf);
	const char *str = NET_ReadStringLine(buf);

	if (cmd == clc_oob && Q_strncmp(str, "info", 4) == 0) {
		str = NET_ReadString(buf);
		if (!str)
			return;
		if (!CL_ProcessPingReply(server, str))
			return;
	} else
		return;

	if (cl_serverlist->integer == SERVERLIST_SHOWALL
	|| (cl_serverlist->integer == SERVERLIST_HIDEFULL && server->clients < server->sv_maxclients)
	|| (cl_serverlist->integer == SERVERLIST_HIDEEMPTY && server->clients)) {
		char string[MAX_INFO_STRING];
		Com_sprintf(string, sizeof(string), "%s\t\t\t%s\t\t\t%s\t\t%i/%i\n",
			server->sv_hostname,
			server->mapname,
			server->gametype,
			server->clients,
			server->sv_maxclients);
		server->serverListPos = cls.serverListPos;
		cls.serverListPos++;
		Q_strcat(serverText, string, sizeof(serverText));
	}
	NET_StreamFree(s);
}

/**
 * @brief Pings all servers in serverList
 * @sa CL_AddServerToList
 * @sa CL_ParseServerInfoMessage
 */
static void CL_PingServer (serverList_t *server)
{
	struct net_stream *s = NET_Connect(server->node, server->service);

	if (s) {
		Com_DPrintf(DEBUG_CLIENT, "pinging [%s]:%s...\n", server->node, server->service);
		NET_OOB_Printf(s, "info %i", PROTOCOL_VERSION);
		NET_StreamSetData(s, server);
		NET_StreamSetCallback(s, &CL_PingServerCallback);
	} else {
		Com_Printf("pinging failed [%s]:%s...\n", server->node, server->service);
	}
}

/**
 * @brief Prints all the servers on the list to game console
 */
static void CL_PrintServerList_f (void)
{
	int i;

	Com_Printf("%i servers on the list\n", cls.serverListLength);

	for (i = 0; i < cls.serverListLength; i++) {
		Com_Printf("%02i: [%s]:%s (pinged: %i)\n", i, cls.serverList[i].node, cls.serverList[i].service, cls.serverList[i].pinged);
	}
}

/**
 * @brief Adds a server to the serverlist cache
 * @return false if it is no valid address or server already exists
 * @sa CL_ParseStatusMessage
 */
static void CL_AddServerToList (const char *node, const char *service)
{
	int i;

	if (cls.serverListLength >= MAX_SERVERLIST)
		return;

	for (i = 0; i < cls.serverListLength; i++)
		if (strcmp(cls.serverList[i].node, node) == 0 && strcmp(cls.serverList[i].service, service) == 0)
			return;

	memset(&(cls.serverList[cls.serverListLength]), 0, sizeof(serverList_t));
	cls.serverList[cls.serverListLength].node = Mem_PoolStrDup(node, cl_localPool, CL_TAG_NONE);
	cls.serverList[cls.serverListLength].service = Mem_PoolStrDup(service, cl_localPool, CL_TAG_NONE);
	CL_PingServer(&cls.serverList[cls.serverListLength]);
	cls.serverListLength++;
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
 * @brief Team selection text
 *
 * This function fills the multiplayer_selectteam menu with content
 * @sa NET_OOB_Printf
 * @sa SV_TeamInfoString
 * @sa CL_SelectTeam_Init_f
 */
static void CL_ParseTeamInfoMessage (struct dbuffer *msg)
{
	char *s = NET_ReadString(msg);
	char *var;
	char *value;
	int cnt = 0, n;

	if (!s) {
		MN_MenuTextReset(TEXT_LIST);
		Com_DPrintf(DEBUG_CLIENT, "CL_ParseTeamInfoMessage: No teaminfo string\n");
		return;
	}

	memset(&teamData, 0, sizeof(teamData));

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
	}

	/* no players are connected atm */
	if (!cnt)
		Q_strcat(teamData.teamInfoText, _("No player connected\n"), sizeof(teamData.teamInfoText));

	Cvar_SetValue("mn_maxteams", teamData.maxteams);
	Cvar_SetValue("mn_maxplayersperteam", teamData.maxplayersperteam);

	mn.menuText[TEXT_LIST] = teamData.teamInfoText;
	teamData.parsed = qtrue;

	/* spawn multi-player death match soldiers here */
	if (GAME_IsMultiplayer() && baseCurrent && !teamData.teamplay)
		CL_SpawnSoldiers_f();
}

static char serverInfoText[1024];
static char userInfoText[256];
/**
 * @brief Serverbrowser text
 * @sa CL_PingServer
 * @sa CL_PingServers_f
 * @note This function fills the network browser server information with text
 * @sa NET_OOB_Printf
 * @sa CL_ServerInfoCallback
 */
static void CL_ParseServerInfoMessage (struct net_stream *stream, const char *s)
{
	const char *value;
	int team;
	const char *token;
	char buf[256];

	if (!s)
		return;

	/* check for server status response message */
	value = Info_ValueForKey(s, "sv_dedicated");
	if (*value) {
		/* server info cvars and users are seperated via newline */
		const char *users = strstr(s, "\n");
		if (!users) {
			Com_Printf("%c%s\n", COLORED_GREEN, s);
			return;
		}
		Com_DPrintf(DEBUG_CLIENT, "%s\n", s); /* status string */

		Cvar_Set("mn_mappic", "maps/shots/default.jpg");
		if (*Info_ValueForKey(s, "sv_needpass") == '1')
			Cvar_Set("mn_server_need_password", "1");
		else
			Cvar_Set("mn_server_need_password", "0");

		Com_sprintf(serverInfoText, sizeof(serverInfoText), _("IP\t%s\n\n"), NET_StreamPeerToName(stream, buf, sizeof(buf), qtrue));
		Cvar_Set("mn_server_ip", buf);
		value = Info_ValueForKey(s, "sv_mapname");
		assert(value);
		Cvar_Set("mn_svmapname", value);
		Q_strncpyz(buf, value, sizeof(buf));
		token = buf;
		/* skip random map char */
		if (token[0] == '+')
			token++;

		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Map:\t%s\n"), value);
		if (FS_CheckFile(va("pics/maps/shots/%s.jpg", token)) != -1) {
			/* store it relative to pics/ dir - not relative to game dir */
			Cvar_Set("mn_mappic", va("maps/shots/%s", token));
		}
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Servername:\t%s\n"), Info_ValueForKey(s, "sv_hostname"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Moralestates:\t%s\n"), _(Info_BoolForKey(s, "sv_enablemorale")));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Gametype:\t%s\n"), Info_ValueForKey(s, "sv_gametype"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Gameversion:\t%s\n"), Info_ValueForKey(s, "ver"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Dedicated server:\t%s\n"), _(Info_BoolForKey(s, "sv_dedicated")));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Operating system:\t%s\n"), Info_ValueForKey(s, "sys_os"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Network protocol:\t%s\n"), Info_ValueForKey(s, "sv_protocol"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Roundtime:\t%s\n"), Info_ValueForKey(s, "sv_roundtimelimit"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Teamplay:\t%s\n"), _(Info_BoolForKey(s, "sv_teamplay")));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. players per team:\t%s\n"), Info_ValueForKey(s, "sv_maxplayersperteam"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. teams allowed in this map:\t%s\n"), Info_ValueForKey(s, "sv_maxteams"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. clients:\t%s\n"), Info_ValueForKey(s, "sv_maxclients"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. soldiers per player:\t%s\n"), Info_ValueForKey(s, "sv_maxsoldiersperplayer"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Max. soldiers per team:\t%s\n"), Info_ValueForKey(s, "sv_maxsoldiersperteam"));
		Com_sprintf(serverInfoText + strlen(serverInfoText), sizeof(serverInfoText) - strlen(serverInfoText), _("Password protected:\t%s\n"), _(Info_BoolForKey(s, "sv_needpass")));
		mn.menuText[TEXT_STANDARD] = serverInfoText;
		userInfoText[0] = '\0';
		do {
			token = COM_Parse(&users);
			if (!users)
				break;
			team = atoi(token);
			token = COM_Parse(&users);
			if (!users)
				break;
			Com_sprintf(userInfoText + strlen(userInfoText), sizeof(userInfoText) - strlen(userInfoText), "%s\t%i\n", token, team);
		} while (1);
		mn.menuText[TEXT_LIST] = userInfoText;
		MN_PushMenu("serverinfo", NULL);
	} else
		Com_Printf("%c%s", COLORED_GREEN, s);
}

/**
 * @sa CL_ServerInfo_f
 * @sa CL_ParseServerInfoMessage
 */
static void CL_ServerInfoCallback (struct net_stream *s)
{
	struct dbuffer *buf = NET_ReadMsg(s);

	if (!buf)
		return;

	{
		const int cmd = NET_ReadByte(buf);
		const char *str = NET_ReadStringLine(buf);

		if (cmd == clc_oob && Q_strncmp(str, "print", 5) == 0) {
			str = NET_ReadString(buf);
			if (str)
				CL_ParseServerInfoMessage(s, str);
		}
	}
	NET_StreamFree(s);
}

/**
 * @sa CL_PingServers_f
 * @todo Not yet thread-safe
 */
static int CL_QueryMasterServer (void *data)
{
	char *responseBuf;
	const char *serverList;
	const char *token;
	char node[MAX_VAR], service[MAX_VAR];
	int i, num;

	responseBuf = HTTP_GetURL(va("%s/ufo/masterserver.php?query", masterserver_url->string));
	if (!responseBuf) {
		Com_Printf("Could not query masterserver\n");
		return 1;
	}

	serverList = responseBuf;

	Com_DPrintf(DEBUG_CLIENT, "masterserver response: %s\n", serverList);
	token = COM_Parse(&serverList);

	num = atoi(token);
	if (num >= MAX_SERVERLIST) {
		Com_DPrintf(DEBUG_CLIENT, "Too many servers: %i\n", num);
		num = MAX_SERVERLIST;
	}
	for (i = 0; i < num; i++) {
		/* host */
		token = COM_Parse(&serverList);
		if (!*token || !serverList) {
			Com_Printf("Could not finish the masterserver response parsing\n");
			break;
		}
		Q_strncpyz(node, token, sizeof(node));
		/* port */
		token = COM_Parse(&serverList);
		if (!*token || !serverList) {
			Com_Printf("Could not finish the masterserver response parsing\n");
			break;
		}
		Q_strncpyz(service, token, sizeof(service));
		CL_AddServerToList(node, service);
	}

	Mem_Free(responseBuf);

	return 0;
}

/**
 * @sa SV_DiscoveryCallback
 */
static void CL_ServerListDiscoveryCallback (struct datagram_socket *s, const char *buf, int len, struct sockaddr *from)
{
	const char match[] = "discovered";
	if (len == sizeof(match) && memcmp(buf, match, len) == 0) {
		char node[MAX_VAR];
		char service[MAX_VAR];
		NET_SockaddrToStrings(s, from, node, sizeof(node), service, sizeof(service));
		CL_AddServerToList(node, service);
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
	const char *newBookmark;

	if (Cmd_Argc() < 2) {
		newBookmark = Cvar_VariableString("mn_server_ip");
		if (!newBookmark) {
			Com_Printf("Usage: %s <ip>\n", Cmd_Argv(0));
			return;
		}
	} else
		newBookmark = Cmd_Argv(1);

	for (i = 0; i < MAX_BOOKMARKS; i++) {
		const char *bookmark = Cvar_VariableString(va("adr%i", i));
		if (bookmark[0] == '\0') {
			Cvar_Set(va("adr%i", i), newBookmark);
			return;
		}
	}
	/* bookmarks are full - overwrite the first entry */
	MN_Popup(_("Notice"), _("All bookmark slots are used - please removed unused entries and repeat this step"));
}

/**
 * @sa CL_ServerInfoCallback
 */
static void CL_ServerInfo_f (void)
{
	struct net_stream *s;
	const char *host;
	const char *port;

	switch (Cmd_Argc()) {
	case 2:
		host = Cmd_Argv(1);
		port = va("%d", PORT_SERVER);
		break;
	case 3:
		host = Cmd_Argv(1);
		port = Cmd_Argv(2);
		break;
	default:
		if (cls.selectedServer) {
			host = cls.selectedServer->node;
			port = cls.selectedServer->service;
		} else {
			host = Cvar_VariableString("mn_server_ip");
			port = va("%d", PORT_SERVER);
		}
		break;
	}
	s = NET_Connect(host, port);
	if (s) {
		NET_OOB_Printf(s, "status %i", PROTOCOL_VERSION);
		NET_StreamSetCallback(s, &CL_ServerInfoCallback);
	} else
		Com_Printf("Could not connect to %s %s\n", host, port);
}

/**
 * @brief Callback for bookmark nodes in multiplayer menu (mp_bookmarks)
 * @sa CL_ParseServerInfoMessage
 */
static void CL_ServerListClick_f (void)
{
	int num, i;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}
	num = atoi(Cmd_Argv(1));

	mn.menuText[TEXT_STANDARD] = serverInfoText;
	if (num >= 0 && num < cls.serverListLength)
		for (i = 0; i < cls.serverListLength; i++)
			if (cls.serverList[i].pinged && cls.serverList[i].serverListPos == num) {
				/* found the server - grab the infos for this server */
				cls.selectedServer = &cls.serverList[i];
				Cbuf_AddText(va("server_info %s %s;", cls.serverList[i].node, cls.serverList[i].service));
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
	MN_MenuTextReset(TEXT_STANDARD);

	NET_OOB_Printf(cls.netStream, "teaminfo %i", PROTOCOL_VERSION);
	mn.menuText[TEXT_STANDARD] = _("Select a free team or your coop team");
}

/** this is true if pingservers was already executed */
static qboolean serversAlreadyQueried = qfalse;
static int lastServerQuery = 0;
/** ms until the server query timed out */
#define SERVERQUERYTIMEOUT 40000

/**
 * @brief The first function called when entering the multiplayer menu, then CL_Frame takes over
 * @sa CL_ParseServerInfoMessage
 * @note Use a parameter for pingservers to update the current serverlist
 */
static void CL_PingServers_f (void)
{
	int i;
	char name[6];
	const char *adrstring;

	cls.selectedServer = NULL;

	/* refresh the list */
	if (Cmd_Argc() == 2) {
		/* reset current list */
		serverText[0] = 0;
		serversAlreadyQueried = qfalse;
		for (i = 0; i < cls.serverListLength; i++) {
			Mem_Free(cls.serverList[i].node);
			Mem_Free(cls.serverList[i].service);
		}
		cls.serverListPos = 0;
		cls.serverListLength = 0;
		memset(cls.serverList, 0, sizeof(cls.serverList));
	} else {
		mn.menuText[TEXT_LIST] = serverText;
		return;
	}

	if (!cls.netDatagramSocket)
		cls.netDatagramSocket = NET_DatagramSocketNew(NULL, va("%d", PORT_CLIENT), &CL_ServerListDiscoveryCallback);

	/* broadcast search for all the servers int the local network */
	if (cls.netDatagramSocket) {
		const char buf[] = "discover";
		NET_DatagramBroadcast(cls.netDatagramSocket, buf, sizeof(buf), PORT_SERVER);
	}

	for (i = 0; i < MAX_BOOKMARKS; i++) {
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

	mn.menuText[TEXT_LIST] = serverText;

	/* don't query the masterservers with every call */
	if (serversAlreadyQueried) {
		if (lastServerQuery + SERVERQUERYTIMEOUT > cls.realtime)
			return;
	} else
		serversAlreadyQueried = qtrue;

	lastServerQuery = cls.realtime;

	/* query master server? */
	if (Cmd_Argc() == 2 && Q_strcmp(Cmd_Argv(1), "local")) {
		Com_DPrintf(DEBUG_CLIENT, "Query masterserver\n");
		/*SDL_CreateThread(CL_QueryMasterServer, NULL);*/
		CL_QueryMasterServer(NULL);
	}
}


/**
 * @brief Responses to broadcasts, etc
 * @sa CL_ReadPackets
 * @sa CL_Frame
 * @sa SVC_DirectConnect
 */
static void CL_ConnectionlessPacket (struct dbuffer *msg)
{
	const char *s;
	const char *c;
	int i;

	s = NET_ReadStringLine(msg);

	Cmd_TokenizeString(s, qfalse);

	c = Cmd_Argv(0);

	Com_DPrintf(DEBUG_CLIENT, "server OOB: %s\n", Cmd_Args());

	/* server connection */
	if (!Q_strncmp(c, "client_connect", 13)) {
		for (i = 1; i < Cmd_Argc(); i++) {
			const char *p = Cmd_Argv(i);
			if (!Q_strncmp(p, "dlserver=", 9)) {
				p += 9;
				Com_sprintf(cls.downloadReferer, sizeof(cls.downloadReferer), "ufo://%s", cls.servername);
				CL_SetHTTPServer(p);
				if (cls.downloadServer[0])
					Com_Printf("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
			}
		}
		if (cls.state == ca_connected) {
			Com_Printf("Dup connect received. Ignored.\n");
			return;
		}
		msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, "new");
		NET_WriteMsg(cls.netStream, msg);
		CL_SetClientState(ca_connected);
		return;
	}

	/* remote command from gui front end */
	if (!Q_strncmp(c, "cmd", 3)) {
		if (!NET_StreamIsLoopback(cls.netStream)) {
			Com_Printf("Command packet from remote host. Ignored.\n");
			return;
		}
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
		NET_OOB_Printf(cls.netStream, "ack");
		return;
	}

	/* echo request from server */
	if (!Q_strncmp(c, "echo", 4)) {
		NET_OOB_Printf(cls.netStream, "%s", Cmd_Argv(1));
		return;
	}

	/* print */
	if (!Q_strncmp(c, "print", 5)) {
		s = NET_ReadString(msg);
		/* special reject messages needs proper handling */
		if (strstr(s, REJ_PASSWORD_REQUIRED_OR_INCORRECT))
			MN_PushMenu("serverpassword", NULL);
		MN_Popup(_("Notice"), _(s));
		return;
	}

	Com_Printf("Unknown command '%s'.\n", c);
}

/**
 * @sa CL_ConnectionlessPacket
 * @sa CL_Frame
 * @sa CL_ParseServerMessage
 * @sa NET_ReadMsg
 * @sa SV_ReadPacket
 */
static void CL_ReadPackets (void)
{
	struct dbuffer *msg;
	while ((msg = NET_ReadMsg(cls.netStream))) {
		const int cmd = NET_ReadByte(msg);
		if (cmd == clc_oob)
			CL_ConnectionlessPacket(msg);
		else
			CL_ParseServerMessage(cmd, msg);
		free_dbuffer(msg);
	}
}

/**
 * @brief Prints the current userinfo string to the game console
 * @sa SV_UserInfo_f
 */
static void CL_UserInfo_f (void)
{
	Com_Printf("User info settings:\n");
	Info_Print(Cvar_Userinfo());
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
	const int maxteamnum = Cvar_VariableInteger("mn_maxteams");

	if (maxteamnum > 0)
		max = maxteamnum;

	cl_teamnum->modified = qfalse;

	if (i <= TEAM_CIVILIAN || i > teamData.maxteams) {
		Cvar_SetValue("cl_teamnum", DEFAULT_TEAMNUM);
		i = DEFAULT_TEAMNUM;
	}

	if (Q_strncmp(Cmd_Argv(0), "teamnum_dec", 11)) {
		for (i--; i > TEAM_CIVILIAN; i--) {
			if (teamData.maxplayersperteam > teamData.teamCount[i]) {
				Cvar_SetValue("cl_teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				mn.menuText[TEXT_STANDARD] = buf;
				break;
			} else {
				mn.menuText[TEXT_STANDARD] = _("Team is already in use");
#ifdef DEBUG
				Com_DPrintf(DEBUG_CLIENT, "team %i: %i (max: %i)\n", i, teamData.teamCount[i], teamData.maxplayersperteam);
#endif
			}
		}
	} else {
		for (i++; i <= teamData.maxteams; i++) {
			if (teamData.maxplayersperteam > teamData.teamCount[i]) {
				Cvar_SetValue("cl_teamnum", i);
				Com_sprintf(buf, sizeof(buf), _("Current team: %i"), i);
				mn.menuText[TEXT_STANDARD] = buf;
				break;
			} else {
				mn.menuText[TEXT_STANDARD] = _("Team is already in use");
#ifdef DEBUG
				Com_DPrintf(DEBUG_CLIENT, "team %i: %i (max: %i)\n", i, teamData.teamCount[i], teamData.maxplayersperteam);
#endif
			}
		}
	}

#if 0
	if (!teamnum->modified)
		mn.menuText[TEXT_STANDARD] = _("Invalid or full team");
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
	const int n = cl_teamnum->integer;
	base_t *base;
	aircraft_t *aircraft = cls.missionaircraft;
	chrList_t chrListTemp;
	int i;

	if (!cls.missionaircraft) {
		Com_Printf("CL_SpawnSoldiers_f: No mission aircraft\n");
		return;
	}

	base = CP_GetMissionBase();

	if (GAME_IsMultiplayer() && base && !teamData.parsed) {
		Com_Printf("CL_SpawnSoldiers_f: teaminfo unparsed\n");
		return;
	}

	if (soldiersSpawned) {
		Com_Printf("CL_SpawnSoldiers_f: Soldiers are already spawned\n");
		return;
	}

	if (GAME_IsMultiplayer() && base) {
		/* we are already connected and in this list */
		if (n <= TEAM_CIVILIAN || teamData.maxplayersperteam < teamData.teamCount[n]) {
			mn.menuText[TEXT_STANDARD] = _("Invalid or full team");
			Com_Printf("CL_SpawnSoldiers_f: Invalid or full team %i\n"
				"  maxplayers per team: %i - players on team: %i",
				n, teamData.maxplayersperteam, teamData.teamCount[n]);
			return;
		}
	}

	/* convert aircraft team to chr_list */
	for (i = 0, chrListTemp.num = 0; i < aircraft->maxTeamSize; i++) {
		if (aircraft->acTeam[i]) {
			chrListTemp.chr[chrListTemp.num] = &aircraft->acTeam[i]->chr;
			chrListTemp.num++;
		}
	}

	if (chrListTemp.num <= 0) {
		Com_DPrintf(DEBUG_CLIENT, "CL_SpawnSoldiers_f: Error - team number <= zero - %i\n", chrListTemp.num);
	} else {
		/* send team info */
		struct dbuffer *msg = new_dbuffer();
		CL_SendCurTeamInfo(msg, &chrListTemp, base);
		NET_WriteMsg(cls.netStream, msg);
	}

	{
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, clc_stringcmd);
		NET_WriteString(msg, va("spawn %i\n", spawnCountFromServer));
		NET_WriteMsg(cls.netStream, msg);
	}

	soldiersSpawned = qtrue;

	/* spawn immediately if in single-player, otherwise wait for other players to join */
	if (GAME_IsSingleplayer()) {
		/* activate hud */
		MN_PushMenu(mn_hud->string, NULL);
		Cvar_Set("mn_active", mn_hud->string);
	} else {
		MN_PushMenu("multiplayer_wait", NULL);
	}
}

/**
 * @brief
 * @note Called after precache was sent from the server
 * @sa SV_Configstrings_f
 * @sa CL_Precache_f
 */
void CL_RequestNextDownload (void)
{
	unsigned map_checksum = 0;
	unsigned ufoScript_checksum = 0;
	const char *buf;

	if (cls.state != ca_connected) {
		Com_Printf("CL_RequestNextDownload: Not connected (%i)\n", cls.state);
		return;
	}

	/* for singleplayer game this is already loaded in our local server
	 * and if we are the server we don't have to reload the map here, too */
	if (!Com_ServerState()) {
		qboolean day = atoi(cl.configstrings[CS_LIGHTMAP]);

		/* activate the map loading screen for multiplayer, too */
		SCR_BeginLoadingPlaque();

		/* check download */
		if (precache_check == CS_MODELS) { /* confirm map */
			if (cl.configstrings[CS_TILES][0] != '+') {
				if (!CL_CheckOrDownloadFile(va("maps/%s.bsp", cl.configstrings[CS_TILES])))
					return; /* started a download */
			}
			precache_check = CS_MODELS + 1;
		}

		/* map might still be downloading? */
		if (CL_PendingHTTPDownloads())
			return;

		while ((buf = FS_GetFileData("ufos/*.ufo")) != NULL)
			ufoScript_checksum += LittleLong(Com_BlockChecksum(buf, strlen(buf)));
		FS_GetFileData(NULL);

		CM_LoadMap(cl.configstrings[CS_TILES], day, cl.configstrings[CS_POSITIONS], &map_checksum);
		if (!*cl.configstrings[CS_VERSION] || !*cl.configstrings[CS_MAPCHECKSUM]
		 || !*cl.configstrings[CS_UFOCHECKSUM] || !*cl.configstrings[CS_OBJECTAMOUNT]) {
			Com_sprintf(popupText, sizeof(popupText), _("Local game version (%s) differs from the servers"), UFO_VERSION);
			MN_Popup(_("Error"), popupText);
			Com_Error(ERR_DISCONNECT, "Local game version (%s) differs from the servers", UFO_VERSION);
			return;
		/* checksum doesn't match with the one the server gave us via configstring */
		} else if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM])) {
			MN_Popup(_("Error"), _("Local map version differs from server"));
			Com_Error(ERR_DISCONNECT, "Local map version differs from server: %u != '%s'",
				map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		/* amount of objects from script files doensn't match */
		} else if (csi.numODs != atoi(cl.configstrings[CS_OBJECTAMOUNT])) {
			MN_Popup(_("Error"), _("Script files are not the same"));
			Com_Error(ERR_DISCONNECT, "Script files are not the same");
			return;
		/* checksum doesn't match with the one the server gave us via configstring */
		} else if (atoi(cl.configstrings[CS_UFOCHECKSUM]) && ufoScript_checksum != atoi(cl.configstrings[CS_UFOCHECKSUM])) {
			Com_Printf("You are using modified ufo script files - might produce problems\n");
		} else if (Q_strncmp(UFO_VERSION, cl.configstrings[CS_VERSION], sizeof(UFO_VERSION))) {
			Com_sprintf(popupText, sizeof(popupText), _("Local game version (%s) differs from the servers (%s)"), UFO_VERSION, cl.configstrings[CS_VERSION]);
			MN_Popup(_("Error"), popupText);
			Com_Error(ERR_DISCONNECT, "Local game version (%s) differs from the servers (%s)", UFO_VERSION, cl.configstrings[CS_VERSION]);
			return;
		}
	}

	CL_LoadMedia();

	soldiersSpawned = qfalse;
	spawnCountFromServer = atoi(Cmd_Argv(1));

	{
		struct dbuffer *msg = new_dbuffer();
		/* send begin */
		/* this will activate the render process (see client state ca_active) */
		NET_WriteByte(msg, clc_stringcmd);
		/* see CL_StartGame */
		NET_WriteString(msg, va("begin %i\n", spawnCountFromServer));
		NET_WriteMsg(cls.netStream, msg);
	}

	/* for singleplayer the soldiers get spawned here */
	if (GAME_IsSingleplayer() && cls.missionaircraft)
		CL_SpawnSoldiers_f();

	cls.waitingForStart = cls.realtime;
}


/**
 * @brief The server will send this command right before allowing the client into the server
 * @sa CL_StartGame
 * @todo recheck the checksum server side
 * @sa SV_Configstrings_f
 */
static void CL_Precache_f (void)
{
	precache_check = CS_MODELS;

	CL_RequestNextDownload();
}

/**
 * @brief Precaches all models at game startup - for faster access
 * @todo In case of vid restart due to changed settings the vid_genericPool is
 * wiped away, too. So the models has to be reloaded with every map change
 */
static void CL_PrecacheModels (void)
{
	int i;
	float percent = 40.0f;

	if (cl_precache->integer)
		Com_PrecacheCharacterModels(); /* 55% */
	else
		percent = 95.0f;

	for (i = 0; i < csi.numODs; i++) {
		if (csi.ods[i].type[0] == '\0' || csi.ods[i].isDummy)
			continue;

		if (csi.ods[i].model[0] != '\0') {
			cls.model_weapons[i] = R_RegisterModelShort(csi.ods[i].model);
			if (cls.model_weapons[i])
				Com_DPrintf(DEBUG_CLIENT, "CL_PrecacheModels: Registered object model: '%s' (%i)\n", csi.ods[i].model, i);
		}
		cls.loadingPercent += percent / csi.numODs;
		SCR_DrawPrecacheScreen(qtrue);
	}
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

	/* start the music track already while precaching data */
	S_Frame();

	cls.loadingPercent = 2.0f;

	/* precache loading screen */
	SCR_DrawPrecacheScreen(qtrue);

	/* init irc commands and cvars */
	Irc_Init();

	/* this will init some more employee stuff */
	E_Init();

	/* init some production menu nodes */
	PR_Init();

	cls.loadingPercent = 5.0f;
	SCR_DrawPrecacheScreen(qtrue);

	/** @todo show this on the screen */
	S_RegisterSounds();

	/* preload all models for faster access */
	CL_PrecacheModels(); /* 95% */

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
	for (i = 0; i < VID_GetModeNums(); i++) {
		selectBoxOptions_t *selectBoxOption = MN_NodeAddOption(vidModesOptions);
		if (!selectBoxOption)
			return;
		Com_sprintf(selectBoxOption->label, sizeof(selectBoxOption->label), "%i:%i", vid_modes[i].width, vid_modes[i].height);
		Com_sprintf(selectBoxOption->value, sizeof(selectBoxOption->value), "%i", vid_modes[i].mode);
	}

	IN_JoystickInitMenu();

	CL_LanguageInit();

	/* now make sure that all the precached models are stored until we quit the game
	 * otherwise they would be freed with every map change */
	R_SwitchModelMemPoolTag();

	if (!cl_introshown->integer) {
		Cbuf_AddText("cinematic intro;");
		Cvar_Set("cl_introshown", "1");
	}
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
	if (!Q_strncmp(type, "font", 4))
		MN_ParseFont(name, text);
	else if (!Q_strncmp(type, "tutorial", 8))
		TUT_ParseTutorials(name, text);
	else if (!Q_strncmp(type, "menu_model", 10))
		MN_ParseMenuModel(name, text);
	else if (!Q_strncmp(type, "menu", 4))
		MN_ParseMenu(name, text);
	else if (!Q_strncmp(type, "icon", 4))
		MN_ParseIcon(name, text);
	else if (!Q_strncmp(type, "particle", 8))
		CL_ParseParticle(name, text);
	else if (!Q_strncmp(type, "sequence", 8))
		CL_ParseSequence(name, text);
	else if (!Q_strncmp(type, "aircraft", 8))
		AIR_ParseAircraft(name, text, qfalse);
	else if (!Q_strncmp(type, "campaign", 8))
		CL_ParseCampaign(name, text);
	else if (!Q_strncmp(type, "music", 5))
		CL_ParseMusic(name, text);
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
	if (!Q_strncmp(type, "up_chapters", 11))
		UP_ParseChapters(name, text);
	else if (!Q_strncmp(type, "building", 8))
		B_ParseBuildings(name, text, qfalse);
	else if (!Q_strncmp(type, "installation", 13))
		INS_ParseInstallations(name, text);
	else if (!Q_strncmp(type, "researched", 10))
		CL_ParseResearchedCampaignItems(name, text);
	else if (!Q_strncmp(type, "researchable", 12))
		CL_ParseResearchableCampaignStates(name, text, qtrue);
	else if (!Q_strncmp(type, "notresearchable", 15))
		CL_ParseResearchableCampaignStates(name, text, qfalse);
	else if (!Q_strncmp(type, "tech", 4))
		RS_ParseTechnologies(name, text);
	else if (!Q_strncmp(type, "basenames", 9))
		B_ParseBaseNames(name, text);
	else if (!Q_strncmp(type, "installationnames", 17))
		INS_ParseInstallationNames(name, text);
	else if (!Q_strncmp(type, "basetemplate", 10))
		B_ParseBaseTemplate(name, text);
	else if (!Q_strncmp(type, "nation", 6))
		CL_ParseNations(name, text);
	else if (!Q_strncmp(type, "city", 4))
		CL_ParseCities(name, text);
	else if (!Q_strncmp(type, "rank", 4))
		CL_ParseRanks(name, text);
	else if (!Q_strncmp(type, "mail", 4))
		CL_ParseEventMails(name, text);
	else if (!Q_strncmp(type, "components", 10))
		INV_ParseComponents(name, text);
	else if (!Q_strncmp(type, "alienteam", 9))
		CL_ParseAlienTeam(name, text);
	else if (!Q_strncmp(type, "msgoptions", 10))
		MSO_ParseSettings(name, text);
	else if (!Q_strncmp(type, "msgcategory", 11))
		MSO_ParseCategories(name, text);
#if 0
	else if (!Q_strncmp(type, "medal", 5))
		Com_ParseMedalsAndRanks(name, &text, qfalse);
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
	if (!Q_strncmp(type, "building", 8))
		B_ParseBuildings(name, text, qtrue);
	else if (!Q_strncmp(type, "aircraft", 8))
		AIR_ParseAircraft(name, text, qtrue);
	else if (!Q_strncmp(type, "ugv", 3))
		CL_ParseUGVs(name, text);
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
	{MN_ScriptSanityCheck, "menu"},
#ifdef DEBUG
	{INV_ItemsSanityCheck, "items"},
	{INV_EquipmentDefSanityCheck, "items"},
#endif

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

/** @brief Data for sanity check of parsed script data
 * @note Only datas that are specific to campaign (not skirmish).
 */
static const sanity_functions_t sanity_functions_campaign[] = {
	{NAT_ScriptSanityCheck, "nations"},

	{NULL, NULL}
};

/**
 * @brief Check the parsed script values for errors after parsing every script file
 * @note Only datas that are specific to campaign (not skirmish).
 * @sa CL_ReadSinglePlayerData
 */
void CL_ScriptSanityCheckCampaign (void)
{
	qboolean status;
	const sanity_functions_t *s;

	s = sanity_functions_campaign;
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
	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL)
		CL_ParseScriptFirst(type, name, &text);

	/* fill in IDXs for required research techs */
	RS_RequiredLinksAssign();

	/* stage two parsing */
	FS_NextScriptHeader(NULL, NULL, NULL);
	text = NULL;

	Com_DPrintf(DEBUG_CLIENT, "Second stage parsing started...\n");
	while ((type = FS_NextScriptHeader("ufos/*.ufo", &name, &text)) != NULL)
		CL_ParseScriptSecond(type, name, &text);

	Com_Printf("Global data loaded - size "UFO_SIZE_T" bytes\n", sizeof(gd));
	Com_Printf("...techs: %i\n", gd.numTechnologies);
	Com_Printf("...buildings: %i\n", gd.numBuildingTemplates);
	Com_Printf("...ranks: %i\n", gd.numRanks);
	Com_Printf("...nations: %i\n", gd.numNations);
	Com_Printf("...cities: %i\n", gd.numCities);
	Com_Printf("\n");
}

/** @brief Cvars for initial check (popup at first start) */
static cvarList_t checkcvar[] = {
	{"cl_name", NULL, NULL},
	{"s_language", NULL, NULL},

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
		if (checkcvar[i].var->string[0] == '\0') {
			Com_Printf("%s has no value\n", checkcvar[i].var->name);
			Cbuf_AddText("mn_push checkcvars;");
			break;
		}
		i++;
	}
}

/**
 * @brief Print the configstrings to game console
 * @sa SV_PrintConfigStrings_f
 */
static void CL_ShowConfigstrings_f (void)
{
	int i;

	for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
		if (cl.configstrings[i][0] == '\0')
			continue;
		Com_Printf("cl.configstrings[%3i]: %s\n", i, cl.configstrings[i]);
	}
}

#ifdef DEBUG
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
		sizeof(gd.buildingTemplates),
		sizeof(gd.employees),
		sizeof(gd.eventMails),
		sizeof(gd.upChapters),
		sizeof(gd.technologies)
	);

	Com_Printf(
		"bases:\n"
		"alienscont         %10Zu bytes\n"
		"capacities         %10Zu bytes\n"
		"bEquipment         %10Zu bytes\n"
		"aircraft           %10Zu bytes\n"
		"aircraft (single)  %10Zu bytes\n"
		"radar              %10Zu bytes\n"
		,
		sizeof(gd.bases[0].alienscont),
		sizeof(gd.bases[0].capacities),
		sizeof(gd.bases[0].bEquipment),
		sizeof(gd.bases[0].aircraft),
		sizeof(aircraft_t),
		sizeof(gd.bases[0].radar)
	);
}

/**
 * @brief Dumps the globalData_t structure to a file
 */
static void CL_DumpGlobalDataToFile_f (void)
{
	qFILE f;

	memset(&f, 0, sizeof(f));
	FS_OpenFileWrite(va("%s/gd.dump", FS_Gamedir()), &f);
	if (!f.f) {
		Com_Printf("CL_DumpGlobalDataToFile_f: Error opening dump file for writing");
		return;
	}

	FS_Write(&gd, sizeof(gd), &f);

	FS_CloseFile(&f);
}
#endif

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
			const char *adrStr = Cvar_VariableString(va("adr%i", i));
			if (adrStr[0] != '\0')
				Com_Printf("%s\n", adrStr);
		}
		return 0;
	}

	localMatch[matches] = NULL;

	/* search all matches and fill the localMatch array */
	for (i = 0; i < MAX_BOOKMARKS; i++) {
		const char *adrStr = Cvar_VariableString(va("adr%i", i));
		if (adrStr[0] != '\0' && !Q_strncmp(partial, adrStr, len)) {
			Com_Printf("%s\n", adrStr);
			localMatch[matches++] = adrStr;
			if (matches >= MAX_COMPLETE)
				break;
		}
	}

	return Cmd_GenericCompleteFunction(len, match, matches, localMatch);
}

/**
 * @brief Calls all reset functions for all subsystems like production and research
 * also inits the cvars and commands
 * @sa CL_Init
 */
static void CL_InitLocal (void)
{
	int i;

	CL_SetClientState(ca_disconnected);
	cls.realtime = Sys_Milliseconds();

	INVSH_InitInventory(invList);
	IN_Init();
	SAV_Init();

	/* init menu, UFOpaedia, basemanagement and other subsystems */
	MN_InitStartup();
	UP_InitStartup();
	B_InitStartup();
	INV_InitStartup();
	INS_InitStartup();
	RS_InitStartup();
	PR_InitStartup();
	E_InitStartup();
	HOS_InitStartup();
	AC_InitStartup();
	MAP_InitStartup();
	UFO_InitStartup();
	TR_InitStartup();
	AB_InitStartup();
	AIRFIGHT_InitStartup();
	BSUM_InitStartup();
	TUT_InitStartup();
	PTL_InitStartup();
	CP_InitStartup();
	GAME_InitStartup();
	UR_InitStartup();
	NAT_InitStartup();
	BS_InitStartup();
	SEQ_InitStartup();
	TEAM_InitStartup();
	TEAM_MP_InitStartup();
	TOTD_InitStartup();
	AIM_InitStartup();

	/* register our variables */
	for (i = 0; i < MAX_BOOKMARKS; i++)
		Cvar_Get(va("adr%i", i), "", CVAR_ARCHIVE, "Bookmark for network ip");
	cl_isometric = Cvar_Get("r_isometric", "0", CVAR_ARCHIVE, "Draw the world in isometric mode");
	cl_showCoords = Cvar_Get("cl_showcoords", "0", 0, "Show geoscape coords on console (for mission placement) and shows menu coord besides the cursor");
	cl_precache = Cvar_Get("cl_precache", "1", CVAR_ARCHIVE, "Precache character models at startup - more memory usage but smaller loading times in the game");
	cl_introshown = Cvar_Get("cl_introshown", "0", CVAR_ARCHIVE, "Only show the intro once at the initial start");
	cl_particleweather = Cvar_Get("cl_particleweather", "0", CVAR_ARCHIVE | CVAR_LATCH, "Switch the weather particles on or off");
	cl_leshowinvis = Cvar_Get("cl_leshowinvis", "0", CVAR_ARCHIVE, "Show invisible local entites as null models");
	cl_fps = Cvar_Get("cl_fps", "0", CVAR_ARCHIVE, "Show frames per second");
	cl_shownet = Cvar_Get("cl_shownet", "0", CVAR_ARCHIVE, NULL);
	rcon_client_password = Cvar_Get("rcon_password", "", 0, "Remote console password");
	cl_logevents = Cvar_Get("cl_logevents", "0", 0, "Log all events to events.log");
	cl_worldlevel = Cvar_Get("cl_worldlevel", "0", 0, "Current worldlevel in tactical mode");
	cl_worldlevel->modified = qfalse;
	cl_selected = Cvar_Get("cl_selected", "0", CVAR_NOSET, "Current selected soldier");
	cl_3dmap = Cvar_Get("cl_3dmap", "1", CVAR_ARCHIVE, "3D geoscape or flat geoscape");
	/* only 19 soldiers in soldier selection list */
	cl_numnames = Cvar_Get("cl_numnames", "15", CVAR_NOSET, NULL);
	cl_autostand = Cvar_Get("cl_autostand","1", CVAR_USERINFO | CVAR_ARCHIVE, "Save accidental TU waste by allowing server to autostand before long walks");
	cl_start_employees = Cvar_Get("cl_start_employees", "1", CVAR_ARCHIVE, "Start with hired employees");
	cl_initial_equipment = Cvar_Get("cl_initial_equipment", "human_phalanx_initial", CVAR_ARCHIVE, "Start with assigned equipment - see cl_start_employees");
	cl_start_buildings = Cvar_Get("cl_start_buildings", "1", CVAR_ARCHIVE, "Start with initial buildings in your first base");
	cl_connecttimeout = Cvar_Get("cl_connecttimeout", "8000", CVAR_ARCHIVE, "Connection timeout for multiplayer connects");
	confirm_actions = Cvar_Get("confirm_actions", "0", CVAR_ARCHIVE, "Confirm all actions in tactical mode");
	cl_lastsave = Cvar_Get("cl_lastsave", "", CVAR_ARCHIVE, "Last saved slot - use for the continue-campaign function");
	cl_serverlist = Cvar_Get("cl_serverlist", "0", CVAR_ARCHIVE, "0=show all, 1=hide full - servers on the serverlist");
	/* userinfo */
	info_password = Cvar_Get("password", "", CVAR_USERINFO, NULL);
	cl_name = Cvar_Get("cl_name", Sys_GetCurrentUser(), CVAR_USERINFO | CVAR_ARCHIVE, "Playername");
	cl_team = Cvar_Get("cl_team", "human", CVAR_USERINFO | CVAR_ARCHIVE, NULL);
	cl_teamnum = Cvar_Get("cl_teamnum", "1", CVAR_USERINFO | CVAR_ARCHIVE, "Teamnum for multiplayer teamplay games");
	cl_msg = Cvar_Get("cl_msg", "2", CVAR_USERINFO | CVAR_ARCHIVE, "Sets the message level for server messages the client receives");
	sv_maxclients = Cvar_Get("sv_maxclients", "1", CVAR_SERVERINFO, "If sv_maxclients is 1 we are in singleplayer - otherwise we are mutliplayer mode (see sv_teamplay)");

	masterserver_url = Cvar_Get("masterserver_url", MASTER_SERVER, CVAR_ARCHIVE, "URL of UFO:AI masterserver");

	cl_http_filelists = Cvar_Get("cl_http_filelists", "1", 0, NULL);
	cl_http_downloads = Cvar_Get("cl_http_downloads", "1", 0, "Try to download files via http");
	cl_http_max_connections = Cvar_Get("cl_http_max_connections", "1", 0, NULL);

	cl_mapDebug = Cvar_Get("debug_map", "0", 0, "Activate realtime map debugging options - bitmask. Valid values are 0, 1, 3 and 7");


	/* register our commands */
	Cmd_AddCommand("check_cvars", CL_CheckCvars_f, "Check cvars like playername and so on");
	Cmd_AddCommand("targetalign", CL_ActorTargetAlign_f, _("Target your shot to the ground"));
	Cmd_AddCommand("invopen", CL_ActorInventoryOpen_f, _("Open the actors inventory while we are in tactical mission"));

	Cmd_AddCommand("bookmark_add", CL_BookmarkAdd_f, "Add a new bookmark - see adrX cvars");
	Cmd_AddCommand("server_info", CL_ServerInfo_f, NULL);
	Cmd_AddCommand("serverlist", CL_PrintServerList_f, NULL);
	/* text id is servers in menu_multiplayer.ufo */
	Cmd_AddCommand("servers_click", CL_ServerListClick_f, NULL);
	Cmd_AddCommand("cmd", CL_ForwardToServer_f, "Forward to server");
	Cmd_AddCommand("pingservers", CL_PingServers_f, "Ping all servers in local network to get the serverlist");
	Cmd_AddCommand("disconnect", CL_Disconnect_f, "Disconnect from the current server");
	Cmd_AddCommand("connect", CL_Connect_f, "Connect to given ip");
	Cmd_AddParamCompleteFunction("connect", CL_CompleteNetworkAddress);
	Cmd_AddCommand("reconnect", CL_Reconnect_f, "Reconnect to last server");
	Cmd_AddCommand("rcon", CL_Rcon_f, "Execute a rcon command - see rcon_password");
	Cmd_AddParamCompleteFunction("rcon", CL_CompleteNetworkAddress);
	Cmd_AddCommand("cl_userinfo", CL_UserInfo_f, "Prints your userinfo string");
#ifdef ACTIVATE_PACKET_COMMAND
	/* this is dangerous to leave in */
	Cmd_AddCommand("packet", CL_Packet_f, "Dangerous debug function for network testing");
#endif
	Cmd_AddCommand("quit", CL_Quit_f, "Quits the game");
	Cmd_AddCommand("env", CL_Env_f, NULL);

	Cmd_AddCommand("precache", CL_Precache_f, "Function that is called at mapload to precache map data");
	Cmd_AddCommand("mp_selectteam_init", CL_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team");
	Cmd_AddCommand("mp_wait_init", CL_WaitInit_f, "Function that inits some nodes");
	Cmd_AddCommand("spawnsoldiers", CL_SpawnSoldiers_f, "Spawns the soldiers for the selected teamnum");
	Cmd_AddCommand("teamnum_dec", CL_TeamNum_f, "Decrease the prefered teamnum");
	Cmd_AddCommand("teamnum_inc", CL_TeamNum_f, "Increase the prefered teamnum");
	Cmd_AddCommand("cl_configstrings", CL_ShowConfigstrings_f, "Print client configstrings to game console");

	/* forward to server commands
	 * the only thing this does is allow command completion
	 * to work -- all unknown commands are automatically
	 * forwarded to the server */
	Cmd_AddCommand("say", NULL, "Chat command");
	Cmd_AddCommand("say_team", NULL, "Team chat command");
	Cmd_AddCommand("players", NULL, "List of team and player name");
#ifdef DEBUG
	Cmd_AddCommand("debug_aircraftsamplelist", AIR_ListAircraftSamples_f, "Show aircraft parameter on game console");
	Cmd_AddCommand("debug_gddump", CL_DumpGlobalDataToFile_f, "Dumps gd to a file");
	Cmd_AddCommand("debug_gdstats", CL_GlobalDataSizes_f, "Show globalData_t sizes");
	Cmd_AddCommand("debug_cgrid", Grid_DumpWholeClientMap_f, "Shows the whole client side pathfinding grid of the current loaded map");
	Cmd_AddCommand("debug_sgrid", Grid_DumpWholeServerMap_f, "Shows the whole server side pathfinding grid of the current loaded map");
	Cmd_AddCommand("debug_croute", Grid_DumpClientRoutes_f, "Shows the whole client side pathfinding grid of the current loaded map");
	Cmd_AddCommand("debug_sroute", Grid_DumpServerRoutes_f, "Shows the whole server side pathfinding grid of the current loaded map");
	Cmd_AddCommand("debug_tus", CL_DumpTUs_f, "Shows a table of the TUs that would be used by the current actor to move relative to its current location");
	Cmd_AddCommand("debug_movemark", CL_DumpMoveMark_f, "Triggers Grid_MoveMark in every direction at the current truePos.");
	Cmd_AddCommand("debug_actorinvlist", NULL, "Shows the inventory list of all actors");
	Cmd_AddCommand("debug_listle", LE_List_f, "Shows a list of current know local entities with type and status");
	Cmd_AddCommand("debug_listlm", LM_List_f, "Shows a list of current know local models");
	/* forward commands again */
	Cmd_AddCommand("debug_edictuse", NULL, "Call the 'use' function of a given edict");
	Cmd_AddCommand("debug_edicttouch", NULL, "Call the 'touch' function of a given edict");
	Cmd_AddCommand("debug_killteam", NULL, "Kills a given team");
	Cmd_AddCommand("debug_stunteam", NULL, "Stuns a given team");
	Cmd_AddCommand("debug_listscore", NULL, "Shows mission-score entries of all team members");
#endif

	memset(&teamData, 0, sizeof(teamData));
}

/**
 * @brief Send the userinfo to the server (and to all other clients)
 * when they changed (CVAR_USERINFO)
 * @sa CL_Connect
 */
static void CL_SendChangedUserinfos (void)
{
	/* send a userinfo update if needed */
	if (cls.state >= ca_connected) {
		if (userinfo_modified) {
			struct dbuffer *msg = new_dbuffer();
			NET_WriteByte(msg, clc_userinfo);
			NET_WriteString(msg, Cvar_Userinfo());
			NET_WriteMsg(cls.netStream, msg);
			userinfo_modified = qfalse;
		}
	}
}

/**
 * @brief Check whether we are in a tactical mission as server or as client
 * @note handles multiplayer and singleplayer
 *
 * @return true when we are in battlefield
 * @todo Check cvar mn_main for value
 */
qboolean CL_OnBattlescape (void)
{
	/* server_state is set to zero (ss_dead) on every battlefield shutdown */
	if (Com_ServerState())
		return qtrue; /* server */

	/* client */
	if (cls.state >= ca_connected)
		return qtrue;

	return qfalse;
}

/**
 * @sa CL_Frame
 */
static void CL_SendCommand (void)
{
	/* get new key events */
	IN_SendKeyEvents();

	/* process console commands */
	Cbuf_Execute();

	/* send intentions now */
	CL_SendChangedUserinfos();

	/* fix any cheating cvars */
	Cvar_FixCheatVars();

	/* if the local server is running and we aren't connected then connect */
	switch (cls.state) {
	case ca_disconnected:
		if (Com_ServerState()) {
			cls.servername[0] = '\0';
			cls.serverport[0] = '\0';
			CL_SetClientState(ca_connecting);
			userinfo_modified = qfalse;
			return;
		}
		break;
	case ca_connecting:
		if (cls.realtime - cls.connectTime > cl_connecttimeout->integer) {
			Com_Error(ERR_DROP, "Server is not reachable");
		}
		break;
	case ca_connected:
		if (cls.waitingForStart) {
			if (cls.realtime - cls.waitingForStart > cl_connecttimeout->integer) {
				Com_Error(ERR_DROP, "Server is not reachable");
			} else {
				Com_sprintf(cls.loadingMessages, sizeof(cls.loadingMessages),
					"%s (%i)", _("Awaiting game start"), (cls.realtime - cls.waitingForStart) / 1000);
				SCR_UpdateScreen();
			}
		}
		break;
	default:
		break;
	}
}

static void CL_CvarCheck (void)
{
	int v;

	/* worldlevel */
	if (cl_worldlevel->modified) {
		int i;
		if (cl_worldlevel->integer < 0) {
			CL_Drop();
			Com_DPrintf(DEBUG_CLIENT, "CL_CvarCheck: Called drop - something went wrong\n");
			return;
		}

		if (cl_worldlevel->integer >= cl.map_maxlevel - 1)
			Cvar_SetValue("cl_worldlevel", cl.map_maxlevel - 1);
		else if (cl_worldlevel->integer < 0)
			Cvar_SetValue("cl_worldlevel", 0);
		for (i = 0; i < cl.map_maxlevel; i++)
			Cbuf_AddText(va("deselfloor%i\n", i));
		for (; i < PATHFINDING_HEIGHT; i++)
			Cbuf_AddText(va("disfloor%i\n", i));
		Cbuf_AddText(va("selfloor%i\n", cl_worldlevel->integer));
		cl_worldlevel->modified = qfalse;
	}

	/* language */
	if (s_language->modified)
		CL_LanguageTryToSet(s_language->string);

	if (mn_inputlength->modified) {
		if (mn_inputlength->integer >= MAX_CVAR_EDITING_LENGTH)
			Cvar_SetValue("mn_inputlength", MAX_CVAR_EDITING_LENGTH);
	}

	/* r_mode and fullscreen */
	v = Cvar_VariableInteger("mn_vidmode");
	if (v < -1 || v >= VID_GetModeNums()) {
		Com_Printf("Max vid_mode value is %i (%i)\n", VID_GetModeNums(), v);
		v = Cvar_VariableInteger("vid_mode");
		Cvar_SetValue("mn_vidmode", v);
	}
	if (v >= 0)
		Cvar_Set("mn_vidmodestr", va("%i*%i", vid_modes[v].width, vid_modes[v].height));
	else
		Cvar_Set("mn_vidmodestr", _("Custom"));
}

/**
 * @brief Sets the client state
 */
void CL_SetClientState (int state)
{
	Com_DPrintf(DEBUG_CLIENT, "CL_SetClientState: Set new state to %i (old was: %i)\n", state, cls.state);
	cls.state = state;

	switch (cls.state) {
	case ca_uninitialized:
		Sys_Error("CL_SetClientState: Don't set state ca_uninitialized\n");
		break;
	case ca_sequence:
		refdef.ready = qtrue;
		break;
	case ca_active:
		cls.waitingForStart = 0;
		break;
	case ca_connecting:
		if (cls.servername[0]) {
			assert(cls.serverport[0]);
			Com_Printf("Connecting to %s %s...\n", cls.servername, cls.serverport);
		} else {
			Com_Printf("Connecting to localhost...\n");
		}
		CL_Connect();
		break;
	case ca_disconnected:
		cls.waitingForStart = 0;
	case ca_connected:
		break;
	default:
		break;
	}
}

/**
 * @sa Qcommon_Frame
 */
void CL_Frame (int now, void *data)
{
	static int last_frame = 0;
	int delta;

	if (sys_priority->modified || sys_affinity->modified)
		Sys_SetAffinityAndPriority();

	/* decide the simulation time */
	delta = now - last_frame;
	if (last_frame)
		cls.frametime = delta / 1000.0;
	else
		cls.frametime = 0;
	cls.realtime = Sys_Milliseconds();
	cl.time = now;
	last_frame = now;
	if (!blockEvents)
		cl.eventTime += delta;

	/* frame rate calculation */
	if (delta)
		cls.framerate = 1000.0 / delta;

	if (cls.state == ca_connected) {
		/* we run full speed when connecting */
		CL_RunHTTPDownloads();
	}

	/* fetch results from server */
	CL_ReadPackets();

	CL_SendCommand();

	Irc_Logic_Frame();

	IN_Frame();

	/* update camera position */
	CL_CameraMove();

	/* end the rounds when no soldier is alive */
	CL_RunMapParticles();
	CL_ParticleRun();

	/* update the screen */
	SCR_UpdateScreen();

	/* advance local effects for next frame */
	SCR_RunConsole();

	/* LE updates */
	LE_Think();

	/* sound frame */
	S_Frame();

	/* send a new command message to the server */
	CL_SendCommand();
}

/**
 * @sa CL_Frame
 */
void CL_SlowFrame (int now, void *data)
{
	CL_CvarCheck();

	CL_ActorUpdateCvars();
}

/**
 * @sa CL_Shutdown
 * @sa CL_InitAfter
 */
void CL_Init (void)
{
	/* i18n through gettext */
	char languagePath[MAX_OSPATH];
	cvar_t *fs_i18ndir;

	if (sv_dedicated->integer)
		return;					/* nothing running on the client */

	memset(&cls, 0, sizeof(cls));

	fs_i18ndir = Cvar_Get("fs_i18ndir", "", 0, "System path to language files");
	/* i18n through gettext */
	setlocale(LC_ALL, "C");
	setlocale(LC_MESSAGES, "");
	/* use system locale dir if we can't find in gamedir */
	if (fs_i18ndir->string[0] != '\0')
		Q_strncpyz(languagePath, fs_i18ndir->string, sizeof(languagePath));
	else
		Com_sprintf(languagePath, sizeof(languagePath), "%s/"BASEDIRNAME"/i18n/", FS_GetCwd());
	Com_DPrintf(DEBUG_CLIENT, "...using mo files from %s\n", languagePath);
	bindtextdomain(TEXT_DOMAIN, languagePath);
	bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
	/* load language file */
	textdomain(TEXT_DOMAIN);

	cl_localPool = Mem_CreatePool("Client: Local (per game)");
	cl_genericPool = Mem_CreatePool("Client: Generic");
	cl_menuSysPool = Mem_CreatePool("Client: Menu");
	cl_soundSysPool = Mem_CreatePool("Client: Sound system");
	cl_ircSysPool = Mem_CreatePool("Client: IRC system");

	/* all archived variables will now be loaded */
	Con_Init();

	CIN_Init();

	VID_Init();
	S_Init();

	SCR_Init();

	SCR_DrawPrecacheScreen(qfalse);

	CL_InitLocal();

	CL_InitParticles();

	CL_ClearState();

	/* Touch memory */
	Mem_TouchGlobal();
}


/**
 * @brief Saves configuration file and shuts the client systems down
 * @todo this is a callback from @c Sys_Quit and @c Com_Error. It would be better
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

	CL_HTTP_Cleanup();
	Irc_Shutdown();
	Con_SaveConsoleHistory(FS_Gamedir());
	Key_WriteBindings("keys.cfg");
	S_Shutdown();
	R_Shutdown();
	MN_Shutdown();
	CIN_Shutdown();
	FS_Shutdown();
}
