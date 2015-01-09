/**
 * @file
 * @brief Serverlist management for multiplayer
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
#include "../cl_game.h"
#include "../../../shared/parse.h"
#include "../../ui/ui_data.h"
#include "../../../shared/infostring.h"
#include "mp_serverlist.h"
#include "mp_callbacks.h"

#define MAX_SERVERLIST 128

static const cgame_import_t* cgi;

static serverList_t serverList[MAX_SERVERLIST];
serverList_t* selectedServer;
static char serverText[1024];
static int serverListLength;
static int serverListPos;
static cvar_t* cl_serverlist;
static struct datagram_socket* netDatagramSocket;

/**
 * @brief Parsed the server ping response.
 * @param[out] server The server data to store the parsed information in
 * @param[in] msg The ping response with the server information to parse
 * @sa CL_PingServerCallback
 * @sa SVC_Info
 * @return @c true if the server is compatible, @c msg is not @c null and the server
 * wasn't pinged already, @c false otherwise
 */
static bool GAME_MP_ProcessPingReply (serverList_t* server, const char* msg)
{
	if (!msg)
		return false;

	if (PROTOCOL_VERSION != Info_IntegerForKey(msg, "sv_protocol")) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ProcessPingReply: Protocol mismatch\n");
		return false;
	}
	if (!Q_streq(UFO_VERSION, Info_ValueForKey(msg, "sv_version"))) {
		Com_DPrintf(DEBUG_CLIENT, "CL_ProcessPingReply: Version mismatch\n");
	}

	if (server->pinged)
		return false;

	server->pinged = true;
	Q_strncpyz(server->sv_hostname, Info_ValueForKey(msg, "sv_hostname"),
		sizeof(server->sv_hostname));
	Q_strncpyz(server->version, Info_ValueForKey(msg, "sv_version"),
		sizeof(server->version));
	Q_strncpyz(server->mapname, Info_ValueForKey(msg, "sv_mapname"),
		sizeof(server->mapname));
	Q_strncpyz(server->gametype, Info_ValueForKey(msg, "sv_gametype"),
		sizeof(server->gametype));
	server->clients = Info_IntegerForKey(msg, "clients");
	server->sv_dedicated = Info_IntegerForKey(msg, "sv_dedicated");
	server->sv_maxclients = Info_IntegerForKey(msg, "sv_maxclients");
	return true;
}

typedef enum {
	SERVERLIST_SHOWALL,
	SERVERLIST_HIDEFULL,
	SERVERLIST_HIDEEMPTY
} serverListStatus_t;

/**
 * @brief Perform the server filtering
 * @param[in] server The server data
 * @return @c true if the server should be visible for the current filter settings, @c false otherwise
 */
static inline bool GAME_MP_ShowServer (const serverList_t* server)
{
	if (cl_serverlist->integer == SERVERLIST_SHOWALL)
		return true;
	if (cl_serverlist->integer == SERVERLIST_HIDEFULL && server->clients < server->sv_maxclients)
		return true;
	if (cl_serverlist->integer == SERVERLIST_HIDEEMPTY && server->clients > 0)
		return true;

	return false;
}

static void GAME_MP_PingServerCallback (struct net_stream* s)
{
	AutoPtr<dbuffer> buf(cgi->NET_ReadMsg(s));
	if (!buf) {
		cgi->NET_StreamFree(s);
		return;
	}
	serverList_t* server = (serverList_t*)cgi->NET_StreamGetData(s);
	const int cmd = cgi->NET_ReadByte(buf);
	if (cmd != svc_oob) {
		cgi->NET_StreamFree(s);
		return;
	}

	char str[512];
	cgi->NET_ReadStringLine(buf, str, sizeof(str));

	if (strncmp(str, "info", 4) == 0) {
		cgi->NET_ReadString(buf, str, sizeof(str));
		if (GAME_MP_ProcessPingReply(server, str)) {
			if (GAME_MP_ShowServer(server)) {
				server->serverListPos = serverListPos;
				serverListPos++;
				Q_strcat(serverText, sizeof(serverText), "%s\t\t\t%s\t\t\t%s\t\t%i/%i\n",
						server->sv_hostname,
						server->mapname,
						server->gametype,
						server->clients,
						server->sv_maxclients);
			}
		}
	} else if (strncmp(str, "print", 5) == 0) {
		char paramBuf[2048];
		cgi->NET_ReadString(buf, paramBuf, sizeof(paramBuf));
		cgi->Com_DPrintf(DEBUG_CLIENT, "%s", paramBuf);
	}
	cgi->NET_StreamFree(s);
}

/**
 * @brief Pings all servers in serverList
 * @sa CL_AddServerToList
 * @sa GAME_MP_ParseServerInfoMessage
 */
static void GAME_MP_PingServer (serverList_t* server)
{
	struct net_stream* s = cgi->NET_Connect(server->node, server->service, nullptr);
	if (s == nullptr) {
		cgi->Com_Printf("pinging failed [%s]:%s...\n", server->node, server->service);
		return;
	}
	cgi->Com_DPrintf(DEBUG_CLIENT, "pinging [%s]:%s...\n", server->node, server->service);
	cgi->NET_OOB_Printf(s, SV_CMD_INFO " %i", PROTOCOL_VERSION);
	cgi->NET_StreamSetData(s, server);
	cgi->NET_StreamSetCallback(s, &GAME_MP_PingServerCallback);
}

/**
 * @brief Prints all the servers on the list to game console
 */
static void GAME_MP_PrintServerList_f (void)
{
	cgi->Com_Printf("%i servers on the list\n", serverListLength);

	for (int i = 0; i < serverListLength; i++) {
		const serverList_t* list = &serverList[i];
		cgi->Com_Printf("%02i: [%s]:%s (pinged: %i)\n", i, list->node, list->service, list->pinged);
	}
}

/**
 * @brief Adds a server to the serverlist cache
 * @return false if it is no valid address or server already exists
 * @sa CL_ParseStatusMessage
 */
static void GAME_MP_AddServerToList (const char* node, const char* service)
{
	if (serverListLength >= MAX_SERVERLIST)
		return;

	for (int i = 0; i < serverListLength; i++)
		if (Q_streq(serverList[i].node, node) && Q_streq(serverList[i].service, service))
			return;

	OBJZERO(serverList[serverListLength]);
	serverList[serverListLength].node = cgi->GAME_StrDup(node);
	serverList[serverListLength].service = cgi->GAME_StrDup(service);
	GAME_MP_PingServer(&serverList[serverListLength]);
	serverListLength++;
}

/**
 * @brief Team selection text
 *
 * This function fills the multiplayer_selectteam menu with content
 * @sa NET_OOB_Printf
 * @sa SVC_TeamInfo
 * @sa GAME_MP_SelectTeam_Init_f
 */
void GAME_MP_ParseTeamInfoMessage (dbuffer* msg)
{
	char str[4096];
	if (cgi->NET_ReadString(msg, str, sizeof(str)) == 0) {
		cgi->UI_ResetData(TEXT_MULTIPLAYER_USERLIST);
		cgi->UI_ResetData(TEXT_MULTIPLAYER_USERTEAM);
		cgi->UI_ExecuteConfunc("multiplayer_playerNumber 0");
		cgi->Com_DPrintf(DEBUG_CLIENT, "GAME_MP_ParseTeamInfoMessage: No teaminfo string\n");
		return;
	}

	OBJZERO(teamData);

	teamData.maxteams = Info_IntegerForKey(str, "sv_maxteams");
	teamData.maxPlayersPerTeam = Info_IntegerForKey(str, "sv_maxplayersperteam");

	int cnt = 0;
	linkedList_t* userList = nullptr;
	linkedList_t* userTeam = nullptr;

	/* for each lines */
	while (cgi->NET_ReadString(msg, str, sizeof(str)) > 0) {
		const int team = Info_IntegerForKey(str, "cl_team");
		const int isReady = Info_IntegerForKey(str, "cl_ready");
		const char* user = Info_ValueForKey(str, "cl_name");

		if (team > 0 && team < MAX_TEAMS)
			teamData.teamCount[team]++;

		/* store data */
		cgi->LIST_AddString(&userList, user);
		if (team != TEAM_NO_ACTIVE)
			cgi->LIST_AddString(&userTeam, va(_("Team %d"), team));
		else
			cgi->LIST_AddString(&userTeam, _("No team"));

		cgi->UI_ExecuteConfunc("multiplayer_playerIsReady %i %i", cnt, isReady);

		cnt++;
	}

	cgi->UI_RegisterLinkedListText(TEXT_MULTIPLAYER_USERLIST, userList);
	cgi->UI_RegisterLinkedListText(TEXT_MULTIPLAYER_USERTEAM, userTeam);
	cgi->UI_ExecuteConfunc("multiplayer_playerNumber %i", cnt);

	/* no players are connected ATM */
	if (!cnt) {
		/** @todo warning must not be into the player list.
		 * if we see this we are the first player that would be connected to the server */
		/* Q_strcat(teamData.teamInfoText, sizeof(teamData.teamInfoText), _("No player connected\n")); */
	}

	cgi->Cvar_SetValue("mn_maxteams", teamData.maxteams);
	cgi->Cvar_SetValue("mn_maxplayersperteam", teamData.maxPlayersPerTeam);
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
 * @sa SVC_Info
 */
static void GAME_MP_ParseServerInfoMessage (dbuffer* msg, const char* hostname)
{
	const char* value;
	char str[MAX_INFO_STRING];

	cgi->NET_ReadString(msg, str, sizeof(str));

	/* check for server status response message */
	value = Info_ValueForKey(str, "sv_dedicated");
	if (Q_strnull(value)) {
		cgi->Com_Printf(S_COLOR_GREEN "%s", str);
		return;
	}

	/* server info cvars and users are seperated via newline */
	const char* users = strstr(str, "\n");
	if (users == nullptr) {
		cgi->Com_Printf(S_COLOR_GREEN "%s\n", str);
		return;
	}
	cgi->Com_DPrintf(DEBUG_CLIENT, "%s\n", str); /* status string */

	cgi->Cvar_Set("mn_mappic", "maps/shots/default");
	if (*Info_ValueForKey(str, "sv_needpass") == '1')
		cgi->Cvar_Set("mn_server_need_password", "1");
	else
		cgi->Cvar_Set("mn_server_need_password", "0");

	Com_sprintf(serverInfoText, sizeof(serverInfoText), _("IP\t%s\n\n"), hostname);
	cgi->Cvar_Set("mn_server_ip", "%s", hostname);
	value = Info_ValueForKey(str, "sv_mapname");
	assert(value);
	cgi->Cvar_Set("mn_svmapname", "%s", value);
	char buf[256];
	Q_strncpyz(buf, value, sizeof(buf));
	const char* token = buf;
	/* skip random map char. */
	if (token[0] == '+')
		token++;

	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Map:\t%s\n"), value);
	if (!cgi->R_ImageExists("pics/maps/shots/%s", token)) {
		/* store it relative to pics/ dir - not relative to game dir */
		cgi->Cvar_Set("mn_mappic", "maps/shots/%s", token);
	}
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Servername:\t%s\n"), Info_ValueForKey(str, "sv_hostname"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Moralestates:\t%s\n"), _(Info_BoolForKey(str, "sv_enablemorale")));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Gametype:\t%s\n"), Info_ValueForKey(str, "sv_gametype"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Gameversion:\t%s\n"), Info_ValueForKey(str, "ver"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Dedicated server:\t%s\n"), _(Info_BoolForKey(str, "sv_dedicated")));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Operating system:\t%s\n"), Info_ValueForKey(str, "sys_os"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Network protocol:\t%s\n"), Info_ValueForKey(str, "sv_protocol"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Roundtime:\t%s\n"), Info_ValueForKey(str, "sv_roundtimelimit"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Teamplay:\t%s\n"), _(Info_BoolForKey(str, "sv_teamplay")));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Max. players per team:\t%s\n"), Info_ValueForKey(str, "sv_maxplayersperteam"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Max. teams allowed in this map:\t%s\n"), Info_ValueForKey(str, "sv_maxteams"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Max. clients:\t%s\n"), Info_ValueForKey(str, "sv_maxclients"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Max. soldiers per player:\t%s\n"), Info_ValueForKey(str, "sv_maxsoldiersperplayer"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Max. soldiers per team:\t%s\n"), Info_ValueForKey(str, "sv_maxsoldiersperteam"));
	Q_strcat(serverInfoText, sizeof(serverInfoText), _("Password protected:\t%s\n"), _(Info_BoolForKey(str, "sv_needpass")));
	cgi->UI_RegisterText(TEXT_STANDARD, serverInfoText);
	userInfoText[0] = '\0';
	for (;;) {
		token = Com_Parse(&users);
		if (users == nullptr)
			break;
		const int team = atoi(token);
		token = Com_Parse(&users);
		if (users == nullptr)
			break;
		Q_strcat(userInfoText, sizeof(userInfoText), "%s\t%i\n", token, team);
	}
	cgi->UI_RegisterText(TEXT_LIST, userInfoText);
	cgi->UI_PushWindow("serverinfo");
}

/**
 * @sa GAME_MP_ServerInfo_f
 * @sa GAME_MP_ParseServerInfoMessage
 */
static void GAME_MP_ServerInfoCallback (struct net_stream* s)
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
	char str[8];
	cgi->NET_ReadStringLine(buf, str, sizeof(str));
	if (Q_streq(str, "print")) {
		char hostname[256];
		cgi->NET_StreamPeerToName(s, hostname, sizeof(hostname), true);
		GAME_MP_ParseServerInfoMessage(buf, hostname);
	}
	cgi->NET_StreamFree(s);
}

static void GAME_MP_QueryMasterServerThread (const char* responseBuf, void* userdata)
{
	if (!responseBuf) {
		cgi->Com_Printf("Could not query masterserver\n");
		return;
	}

	const char* serverListBuf = responseBuf;

	Com_DPrintf(DEBUG_CLIENT, "masterserver response: %s\n", serverListBuf);
	const char* token = Com_Parse(&serverListBuf);

	int num = atoi(token);
	if (num >= MAX_SERVERLIST) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Too many servers: %i\n", num);
		num = MAX_SERVERLIST;
	}
	for (int i = 0; i < num; i++) {
		/* host */
		token = Com_Parse(&serverListBuf);
		if (!*token || !serverListBuf) {
			cgi->Com_Printf("Could not finish the masterserver response parsing\n");
			break;
		}
		char node[MAX_VAR];
		Q_strncpyz(node, token, sizeof(node));
		/* port */
		token = Com_Parse(&serverListBuf);
		if (token[0] == '\0' || !serverListBuf) {
			cgi->Com_Printf("Could not finish the masterserver response parsing\n");
			break;
		}
		char service[MAX_VAR];
		Q_strncpyz(service, token, sizeof(service));
		GAME_MP_AddServerToList(node, service);
	}
}

/**
 * @sa SV_DiscoveryCallback
 */
static void GAME_MP_ServerListDiscoveryCallback (struct datagram_socket* s, const char* buf, int len, struct sockaddr* from)
{
	const char match[] = "discovered";
	if (len == sizeof(match) && memcmp(buf, match, len) == 0) {
		char node[MAX_VAR];
		char service[MAX_VAR];
		cgi->NET_SockaddrToStrings(s, from, node, sizeof(node), service, sizeof(service));
		GAME_MP_AddServerToList(node, service);
	}
}

/**
 * @brief Add a new bookmark
 *
 * bookmarks are saved in cvar adr[0-15]
 */
static void GAME_MP_BookmarkAdd_f (void)
{
	const char* newBookmark;

	if (cgi->Cmd_Argc() < 2) {
		newBookmark = cgi->Cvar_GetString("mn_server_ip");
		if (!newBookmark) {
			cgi->Com_Printf("Usage: %s <ip>\n", cgi->Cmd_Argv(0));
			return;
		}
	} else {
		newBookmark = cgi->Cmd_Argv(1);
	}

	for (int i = 0; i < MAX_BOOKMARKS; i++) {
		const char* bookmark = cgi->Cvar_GetString(va("adr%i", i));
		if (bookmark[0] == '\0') {
			cgi->Cvar_Set(va("adr%i", i), "%s", newBookmark);
			return;
		}
	}
	/* bookmarks are full - overwrite the first entry */
	cgi->UI_Popup(_("Notice"), "%s", _("All bookmark slots are used - please removed unused entries and repeat this step"));
}

/**
 * @sa CL_ServerInfoCallback
 */
static void GAME_MP_ServerInfo_f (void)
{
	const char* host;
	const char* port;

	switch (cgi->Cmd_Argc()) {
	case 2:
		host = cgi->Cmd_Argv(1);
		port = DOUBLEQUOTE(PORT_SERVER);
		break;
	case 3:
		host = cgi->Cmd_Argv(1);
		port = cgi->Cmd_Argv(2);
		break;
	default:
		if (selectedServer) {
			host = selectedServer->node;
			port = selectedServer->service;
		} else {
			host = cgi->Cvar_GetString("mn_server_ip");
			port = DOUBLEQUOTE(PORT_SERVER);
		}
		break;
	}
	struct net_stream* s = cgi->NET_Connect(host, port, nullptr);
	if (s != nullptr) {
		cgi->NET_OOB_Printf(s, SV_CMD_STATUS " %i", PROTOCOL_VERSION);
		cgi->NET_StreamSetCallback(s, &GAME_MP_ServerInfoCallback);
	} else {
		cgi->Com_Printf("Could not connect to %s %s\n", host, port);
	}
}

/**
 * @brief Callback for bookmark nodes in multiplayer menu (mp_bookmarks)
 * @sa GAME_MP_ParseServerInfoMessage
 */
static void GAME_MP_ServerListClick_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <num>\n", cgi->Cmd_Argv(0));
		return;
	}
	const int num = atoi(cgi->Cmd_Argv(1));

	cgi->UI_RegisterText(TEXT_STANDARD, serverInfoText);
	if (num < 0 || num >= serverListLength)
		return;

	for (int i = 0; i < serverListLength; i++) {
		if (!serverList[i].pinged || serverList[i].serverListPos != num)
			continue;
		/* found the server - grab the infos for this server */
		selectedServer = &serverList[i];
		cgi->Cbuf_AddText("server_info %s %s\n", serverList[i].node, serverList[i].service);
		return;
	}
}

/** this is true if pingservers was already executed */
static bool serversAlreadyQueried = false;
static int lastServerQuery = 0;
/** ms until the server query timed out */
#define SERVERQUERYTIMEOUT 40000

/**
 * @brief The first function called when entering the multiplayer menu, then CL_Frame takes over
 * @sa GAME_MP_ParseServerInfoMessage
 * @note Use a parameter for pingservers to update the current serverlist
 */
void GAME_MP_PingServers_f (void)
{
	selectedServer = nullptr;

	/* refresh the list */
	if (cgi->Cmd_Argc() == 2) {
		/* reset current list */
		serverText[0] = 0;
		serversAlreadyQueried = false;
		for (int i = 0; i < serverListLength; i++) {
			cgi->Free(serverList[i].node);
			cgi->Free(serverList[i].service);
		}
		serverListPos = 0;
		serverListLength = 0;
		OBJZERO(serverList);
	} else {
		cgi->UI_RegisterText(TEXT_LIST, serverText);
		return;
	}

	if (!netDatagramSocket)
		netDatagramSocket = cgi->NET_DatagramSocketNew(nullptr, DOUBLEQUOTE(PORT_CLIENT), &GAME_MP_ServerListDiscoveryCallback);

	/* broadcast search for all the servers int the local network */
	if (netDatagramSocket) {
		const char buf[] = "discover";
		cgi->NET_DatagramBroadcast(netDatagramSocket, buf, sizeof(buf), PORT_SERVER);
	}
	cgi->UI_RegisterText(TEXT_LIST, serverText);

	/* don't query the masterservers with every call */
	if (serversAlreadyQueried) {
		if (lastServerQuery + SERVERQUERYTIMEOUT > cgi->CL_Milliseconds())
			return;
	} else
		serversAlreadyQueried = true;

	lastServerQuery = cgi->CL_Milliseconds();

	/* query master server? */
	if (cgi->Cmd_Argc() == 2 && !Q_streq(cgi->Cmd_Argv(1), "local")) {
		cgi->Com_DPrintf(DEBUG_CLIENT, "Query masterserver\n");
		cgi->CL_QueryMasterServer("query", GAME_MP_QueryMasterServerThread);
	}
}

static const cmdList_t serverListCmds[] = {
	{"bookmark_add", GAME_MP_BookmarkAdd_f, "Add a new bookmark - see adrX cvars"},
	{"server_info", GAME_MP_ServerInfo_f, nullptr},
	{"serverlist", GAME_MP_PrintServerList_f, nullptr},
	/* text id is servers in menu_multiplayer.ufo */
	{"servers_click", GAME_MP_ServerListClick_f, nullptr},
	{nullptr, nullptr, nullptr}
};
void GAME_MP_ServerListInit (const cgame_import_t* import)
{
	cgi = import;
	/* register our variables */
	for (int i = 0; i < MAX_BOOKMARKS; i++)
		cgi->Cvar_Get(va("adr%i", i), "", CVAR_ARCHIVE, "Bookmark for network ip");
	cl_serverlist = cgi->Cvar_Get("cl_serverlist", "0", CVAR_ARCHIVE, "0=show all, 1=hide full - servers on the serverlist");

	cgi->Cmd_TableAddList(serverListCmds);
}

void GAME_MP_ServerListShutdown (void)
{
	cgi->Cmd_TableRemoveList(serverListCmds);

	cgi->NET_DatagramSocketClose(netDatagramSocket);
	netDatagramSocket = nullptr;
}
