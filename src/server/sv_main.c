/**
 * @file sv_main.c
 * @brief Main server code?
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_main.c
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
#include "../shared/parse.h"
#include "../ports/system.h"

/** password for remote server commands */
static cvar_t *rcon_password;
static cvar_t *sv_http_downloadserver;
static cvar_t *sv_enablemorale;
static cvar_t *sv_maxsoldiersperteam;
static cvar_t *sv_maxsoldiersperplayer;
static cvar_t *sv_hostname;
/** minimum seconds between connect messages */
static cvar_t *sv_reconnect_limit;

cvar_t *sv_maxclients = NULL;
cvar_t *sv_dumpmapassembly;
cvar_t *sv_threads;
/** should heartbeats be sent */
cvar_t *sv_public;
cvar_t *sv_mapname;

static qboolean abandon;		/**< shutdown server when all clients disconnect and don't accept new connections */
static qboolean killserver;		/**< will initiate shutdown once abandon is set */

mapcycle_t *mapcycleList;
int mapcycleCount;

memPool_t *sv_gameSysPool;
memPool_t *sv_genericPool;

/*============================================================================ */


/**
 * @brief Called when the player is totally leaving the server, either willingly
 * or unwillingly. This is NOT called if the entire server is quitting
 * or crashing.
 */
void SV_DropClient (client_t * drop, const char *message)
{
	/* add the disconnect */
	struct dbuffer *msg = new_dbuffer();
	NET_WriteByte(msg, svc_disconnect);
	NET_WriteString(msg, message);
	NET_WriteMsg(drop->stream, msg);
	SV_BroadcastPrintf(PRINT_CHAT, "%s was dropped from the server - reason: %s\n", drop->name, message);

	if (drop->state == cs_spawned || drop->state == cs_spawning) {
		/* call the prog function for removing a client */
		/* this will remove the body, among other things */
		SDL_mutexP(svs.serverMutex);
		svs.ge->ClientDisconnect(drop->player);
		SDL_mutexV(svs.serverMutex);
	}

	NET_StreamFinished(drop->stream);
	drop->stream = NULL;

	drop->player->inuse = qfalse;
	SV_SetClientState(drop, cs_free);
	drop->name[0] = 0;

	if (abandon) {
		int count = 0;
		int i;
		for (i = 0; i < sv_maxclients->integer; i++)
			if (svs.clients[i].state >= cs_connected)
				count++;
		if (count == 0)
			killserver = qtrue;
	}
}

/*
==============================================================================
CONNECTIONLESS COMMANDS
==============================================================================
*/

/**
 * @brief Responds with teaminfo such as free team num
 * @sa CL_ParseTeamInfoMessage
 */
static void SVC_TeamInfo (struct net_stream *s)
{
	int i;
	client_t *cl;
	struct dbuffer *msg = new_dbuffer();
	char infoGlobal[MAX_INFO_STRING];

	NET_WriteByte(msg, clc_oob);
	NET_WriteRawString(msg, "teaminfo\n");

	Info_SetValueForKey(infoGlobal, sizeof(infoGlobal), "sv_teamplay", Cvar_GetString("sv_teamplay"));
	Info_SetValueForKey(infoGlobal, sizeof(infoGlobal), "sv_maxteams", Cvar_GetString("sv_maxteams"));
	Info_SetValueForKey(infoGlobal, sizeof(infoGlobal), "sv_maxplayersperteam", Cvar_GetString("sv_maxplayersperteam"));
	NET_WriteString(msg, infoGlobal);

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state >= cs_connected) {
			char infoPlayer[MAX_INFO_STRING];
			/* show players that already have a team with their teamnum */
			int teamId = svs.ge->ClientGetTeamNum(cl->player);
			if (!teamId)
				teamId = TEAM_NO_ACTIVE;
			Com_DPrintf(DEBUG_SERVER, "SVC_TeamInfo: connected client: %i %s\n", i, cl->name);
			Info_SetValueForKeyAsInteger(infoPlayer, sizeof(infoPlayer), "cl_team", teamId);
			Info_SetValueForKeyAsInteger(infoPlayer, sizeof(infoPlayer), "cl_ready", svs.ge->ClientIsReady(cl->player));
			Info_SetValueForKey(infoPlayer, sizeof(infoPlayer), "cl_name", cl->name);
			NET_WriteString(msg, infoPlayer);
		} else {
			Com_DPrintf(DEBUG_SERVER, "SVC_TeamInfo: unconnected client: %i %s\n", i, cl->name);
		}
	}

	NET_WriteByte(msg, 0);

	NET_WriteMsg(s, msg);
}

/**
 * @brief Responds with all the info that the server browser can see
 * @sa SV_StatusString
 */
static void SVC_Status (struct net_stream *s)
{
	char player[1024];
	int i;
	struct dbuffer *msg = new_dbuffer();
	NET_WriteByte(msg, clc_oob);
	NET_WriteRawString(msg, "print\n");

	NET_WriteRawString(msg, Cvar_Serverinfo());
	NET_WriteRawString(msg, "\n");

	for (i = 0; i < sv_maxclients->integer; i++) {
		const client_t *cl = &svs.clients[i];
		if (cl->state > cs_free) {
			Com_sprintf(player, sizeof(player), "%i \"%s\"\n", svs.ge->ClientGetTeamNum(cl->player), cl->name);
			NET_WriteRawString(msg, player);
		}
	}

	NET_WriteMsg(s, msg);
}

/**
 * @brief Responds with short info for broadcast scans
 * @note The second parameter should be the current protocol version number.
 * @note Only a short server description - the user can determine whether he is
 * interested in a full status
 * @sa CL_ParseStatusMessage
 * @sa CL_ProcessPingReply
 */
static void SVC_Info (struct net_stream *s)
{
	int version;

	if (sv_maxclients->integer == 1) {
		Com_DPrintf(DEBUG_SERVER, "Ignore info string in singleplayer mode\n");
		return;	/* ignore in single player */
	}

	version = atoi(Cmd_Argv(1));

	if (version != PROTOCOL_VERSION) {
		char string[MAX_VAR];
		Com_sprintf(string, sizeof(string), "%s: wrong version (client: %i, host: %i)\n", sv_hostname->string, version, PROTOCOL_VERSION);
		NET_OOB_Printf(s, "print\n%s", string);
	} else {
		char infostring[MAX_INFO_STRING];
		int i;
		int count = 0;

		for (i = 0; i < sv_maxclients->integer; i++)
			if (svs.clients[i].state >= cs_spawning)
				count++;

		infostring[0] = '\0';

		Info_SetValueForKey(infostring, sizeof(infostring), "sv_protocol", DOUBLEQUOTE(PROTOCOL_VERSION));
		Info_SetValueForKey(infostring, sizeof(infostring), "sv_hostname", sv_hostname->string);
		Info_SetValueForKey(infostring, sizeof(infostring), "sv_dedicated", sv_dedicated->string);
		Info_SetValueForKey(infostring, sizeof(infostring), "sv_gametype", sv_gametype->string);
		Info_SetValueForKey(infostring, sizeof(infostring), "sv_mapname", sv.name);
		Info_SetValueForKeyAsInteger(infostring, sizeof(infostring), "clients", count);
		Info_SetValueForKey(infostring, sizeof(infostring), "sv_maxclients", sv_maxclients->string);
		Info_SetValueForKey(infostring, sizeof(infostring), "sv_version", UFO_VERSION);
		NET_OOB_Printf(s, "info\n%s", infostring);
	}
}


/**
 * @brief A connection request that did not come from the master
 * @sa CL_ConnectionlessPacket
 */
static void SVC_DirectConnect (struct net_stream *stream)
{
	char userinfo[MAX_INFO_STRING];
	int i;
	client_t *cl;
	player_t *player;
	int playernum;
	int version;
	qboolean connected;
	char buf[256];
	const char *peername = NET_StreamPeerToName(stream, buf, sizeof(buf), qfalse);

	Com_DPrintf(DEBUG_SERVER, "SVC_DirectConnect()\n");

	if (sv.started) {
		Com_Printf("rejected connect because match is already running\n");
		NET_OOB_Printf(stream, "print\nGame has started already.\n");
		return;
	}

	version = atoi(Cmd_Argv(1));
	if (version != PROTOCOL_VERSION) {
		Com_Printf("rejected connect from version %i - %s\n", version, peername);
		NET_OOB_Printf(stream, "print\nServer is version %s.\n", UFO_VERSION);
		return;
	}

	strncpy(userinfo, Cmd_Argv(2), sizeof(userinfo) - 1);
	userinfo[sizeof(userinfo) - 1] = 0;

	if (!strlen(userinfo)) {  /* catch empty userinfo */
		Com_Printf("Empty userinfo from %s\n", peername);
		NET_OOB_Printf(stream, "print\nConnection refused.\n");
		return;
	}

	if (strchr(userinfo, '\xFF')) {  /* catch end of message in string exploit */
		Com_Printf("Illegal userinfo contained xFF from %s\n", peername);
		NET_OOB_Printf(stream, "print\nConnection refused.\n");
		return;
	}

	if (strlen(Info_ValueForKey(userinfo, "ip"))) {  /* catch spoofed ips  */
		Com_Printf("Illegal userinfo contained ip from %s\n", peername);
		NET_OOB_Printf(stream, "print\nConnection refused.\n");
		return;
	}

	/* force the IP key/value pair so the game can filter based on ip */
	Info_SetValueForKey(userinfo, sizeof(userinfo), "ip", peername);

	/* find a client slot */
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
		if (cl->state == cs_free)
			break;
	if (i == sv_maxclients->integer) {
		NET_OOB_Printf(stream, "print\nServer is full.\n");
		Com_Printf("Rejected a connection - server is full.\n");
		return;
	}

	/* build a new connection - accept the new client
	 * this is the only place a client_t is ever initialized */
	memset(cl, 0, sizeof(*cl));
	playernum = cl - svs.clients;
	player = PLAYER_NUM(playernum);
	cl->player = player;
	cl->player->num = playernum;

	SDL_mutexP(svs.serverMutex);
	connected = svs.ge->ClientConnect(player, userinfo, sizeof(userinfo));
	SDL_mutexV(svs.serverMutex);

	/* get the game a chance to reject this connection or modify the userinfo */
	if (!connected) {
		const char *rejmsg = Info_ValueForKey(userinfo, "rejmsg");
		if (rejmsg[0] != '\0') {
			NET_OOB_Printf(stream, "print\n%s\nConnection refused.\n", rejmsg);
			Com_Printf("Game rejected a connection from %s. Reason: %s\n", peername, rejmsg);
 		} else {
			NET_OOB_Printf(stream, "print\nConnection refused.\n");
			Com_Printf("Game rejected a connection from %s.\n", peername);
 		}
		return;
	}

	/* new player */
	cl->player->inuse = qtrue;

	/* parse some info from the info strings */
	strncpy(cl->userinfo, userinfo, sizeof(cl->userinfo) - 1);
	SV_UserinfoChanged(cl);

	/* send the connect packet to the client */
	if (sv_http_downloadserver->string[0])
		NET_OOB_Printf(stream, "client_connect dlserver=%s", sv_http_downloadserver->string);
	else
		NET_OOB_Printf(stream, "client_connect");

	SV_SetClientState(cl, cs_connected);

	cl->lastconnect = svs.realtime;
	Q_strncpyz(cl->peername, peername, sizeof(cl->peername));
	cl->stream = stream;
	NET_StreamSetData(stream, cl);
}

/**
 * @brief Checks whether the remote connection is allowed (rcon_password must be
 * set on the server) - and verify the user given password with the cvar value.
 */
static inline qboolean Rcon_Validate (const char *password)
{
	/* no rcon access */
	if (!strlen(rcon_password->string))
		return qfalse;

	/* password not valid */
	if (strcmp(password, rcon_password->string))
		return qfalse;

	return qtrue;
}

#define SV_OUTPUTBUF_LENGTH 1024

static char sv_outputbuf[SV_OUTPUTBUF_LENGTH];

/**
 * @brief A client issued an rcon command. Shift down the remaining args. Redirect all printfs
 */
static void SVC_RemoteCommand (struct net_stream *stream)
{
	char buf[256];
	const char *peername = NET_StreamPeerToName(stream, buf, sizeof(buf), qfalse);
	qboolean valid = Rcon_Validate(Cmd_Argv(1));

	if (!valid)
		Com_Printf("Bad rcon from %s:\n%s\n", peername, Cmd_Argv(1));
	else
		Com_Printf("Rcon from %s:\n%s\n", peername, Cmd_Argv(1));

	Com_BeginRedirect(stream, sv_outputbuf, SV_OUTPUTBUF_LENGTH);

	if (!valid)
		/* inform the client */
		Com_Printf("Bad rcon_password.\n");
	else {
		char remaining[1024] = "";
		int i;

		/* execute the rcon commands */
		for (i = 2; i < Cmd_Argc(); i++) {
			Q_strcat(remaining, Cmd_Argv(i), sizeof(remaining));
			Q_strcat(remaining, " ", sizeof(remaining));
		}

		/* execute the string */
		Cmd_ExecuteString(remaining);
	}

	Com_EndRedirect();
}

/**
 * @brief Handles a connectionless message from a client
 * @sa NET_OOB_Printf
 * @param[out] stream The stream to write to
 * @param msg The message buffer to read the connectionless data from
 */
static void SV_ConnectionlessPacket (struct net_stream *stream, struct dbuffer *msg)
{
	const char *c;
	char s[512];
	char buf[256];

	NET_ReadStringLine(msg, s, sizeof(s));

	Cmd_TokenizeString(s, qfalse);

	c = Cmd_Argv(0);
	Com_DPrintf(DEBUG_SERVER, "Packet : %s\n", c);

	if (!strcmp(c, "teaminfo"))
		SVC_TeamInfo(stream);
	else if (!strcmp(c, "info"))
		SVC_Info(stream);
	else if (!strcmp(c, "status"))
		SVC_Status(stream);
	else if (!strcmp(c, "connect"))
		SVC_DirectConnect(stream);
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand(stream);
	else
		Com_Printf("Bad connectionless packet from %s:\n%s\n", NET_StreamPeerToName(stream, buf, sizeof(buf), qtrue), s);
}

/**
 * @sa CL_ReadPacket
 * @sa NET_ReadMsg
 * @sa SV_Start
 */
void SV_ReadPacket (struct net_stream *s)
{
	client_t *cl = NET_StreamGetData(s);
	struct dbuffer *msg;

	while ((msg = NET_ReadMsg(s))) {
		const int cmd = NET_ReadByte(msg);

		if (cmd == clc_oob)
			SV_ConnectionlessPacket(s, msg);
		else if (cl)
			SV_ExecuteClientMessage(cl, cmd, msg);
		else
			NET_StreamFree(s);

		free_dbuffer(msg);
	}
}

/**
 * @brief Start the next map in the cycle
 */
void SV_NextMapcycle (void)
{
	int i;
	const char *map = NULL, *gameType = NULL;
	qboolean day = qtrue;
	char *base;
	char assembly[MAX_QPATH];
	char expanded[MAX_QPATH];
	char cmd[MAX_QPATH];
	mapcycle_t *mapcycle;

	mapcycle = mapcycleList;
	if (sv.name[0]) {
		Com_Printf("current map: %s\n", sv.name);
		for (i = 0; i < mapcycleCount; i++) {
			/* random maps may have a theme - but that's not stored in sv.name
			 * but in sv.assembly */
			if (mapcycle->map[0] == '+') {
				Q_strncpyz(expanded, mapcycle->map, sizeof(expanded));
				base = strstr(expanded, " ");
				if (base) {
					base[0] = '\0'; /* split the strings */
					Q_strncpyz(assembly, base + 1, sizeof(assembly));
					/* get current position */
					if (!strcmp(sv.name, expanded) && !strcmp(sv.assembly, assembly)) {
						/* next map in cycle */
						if (mapcycle->next) {
							map = mapcycle->next->map;
							day = mapcycle->next->day;
							gameType = mapcycle->next->type;
							Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: next one: '%s' (gametype: %s)\n", map, gameType);
						/* switch back to first list on cycle - if there is one */
						} else {
							map = mapcycleList->map;
							day = mapcycleList->day;
							gameType = mapcycleList->type;
							Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: first one: '%s' (gametype: %s)\n", map, gameType);
						}
						break;
					}
				} else {
					Com_Printf("ignore mapcycle entry for random map (%s) with"
						" no assembly given\n", mapcycle->map);
				}
			} else {
				/* get current position */
				if (!strcmp(sv.name, mapcycle->map)) {
					/* next map in cycle */
					if (mapcycle->next) {
						map = mapcycle->next->map;
						day = mapcycle->next->day;
						gameType = mapcycle->next->type;
						Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: next one: '%s' (gametype: %s)\n", map, gameType);
					/* switch back to first list on cycle - if there is one */
					} else {
						map = mapcycleList->map;
						day = mapcycleList->day;
						gameType = mapcycleList->type;
						Com_DPrintf(DEBUG_SERVER, "SV_NextMapcycle: first one: '%s' (gametype: %s)\n", map, gameType);
					}
					Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

					/* check for bsp file */
					if (map[0] != '+' && FS_CheckFile("%s", expanded) < 0) {
						Com_Printf("SV_NextMapcycle: Can't find '%s' - mapcycle error\n"
							"Use the 'maplist' command to get a list of valid maps\n", expanded);
						map = NULL;
						gameType = NULL;
					} else
						break;
				}
			}
			mapcycle = mapcycle->next;
		}
	}

	if (!map) {
		if (mapcycleCount > 0) {
			map = mapcycleList->map;
			day = mapcycleList->day;
			gameType = mapcycleList->type;
			if (map[0] != '+') {
				Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

				/* check for bsp file */
				if (FS_CheckFile("%s", expanded) < 0) {
					Com_Printf("SV_NextMapcycle: Can't find '%s' - mapcycle error\n"
						"Use the 'maplist' command to get a list of valid maps\n", expanded);
					return;
				}
			}
		} else if (sv.name[0]) {
			Com_Printf("No mapcycle - restart the current map (%s)\n", sv.name);
			map = sv.name;
			gameType = NULL;
		} else {
			Com_Printf("No mapcycle and no running map\n");
			return;
		}
		/* still not set? */
		if (!map)
			return;
	}

	/* check whether we want to change the gametype, too */
	if (gameType && gameType[0] != '\0') {
		Cvar_Set("sv_gametype", gameType);
		Com_SetGameType();
		sv_gametype->modified = qfalse;
	}

	if (day)
		Com_sprintf(cmd, sizeof(cmd), "map day %s", map);
	else
		Com_sprintf(cmd, sizeof(cmd), "map night %s", map);
	Cbuf_AddText(cmd);
}

/**
 * @brief Empty the mapcycle list
 * @sa SV_MapcycleAdd
 */
void SV_MapcycleClear (void)
{
	int i;
	mapcycle_t *mapcycle;

	mapcycle = mapcycleList;
	for (i = 0; i < mapcycleCount; i++) {
		mapcycle_t *oldMapcycle = mapcycle;
		mapcycle = mapcycle->next;
		Mem_Free(oldMapcycle->type);
		Mem_Free(oldMapcycle->map);
		Mem_Free(oldMapcycle);
	}

	/* reset the mapcycle data */
	mapcycleList = NULL;
	mapcycleCount = 0;
}

/**
 * @brief Append a new mapname to the list of maps for the cycle
 * @todo check for maps and valid gametypes here
 * @sa SV_MapcycleClear
 */
void SV_MapcycleAdd (const char* mapName, qboolean day, const char* gameType)
{
	mapcycle_t *mapcycle;

	if (!mapcycleList) {
		mapcycleList = Mem_PoolAlloc(sizeof(*mapcycle), sv_genericPool, 0);
		mapcycle = mapcycleList; /* first one */
	} else {
		/* go to the last entry */
		mapcycle = mapcycleList;
		while (mapcycle->next)
			mapcycle = mapcycle->next;
		mapcycle->next = Mem_PoolAlloc(sizeof(*mapcycle), sv_genericPool, 0);
		mapcycle = mapcycle->next;
	}
	mapcycle->map = Mem_PoolStrDup(mapName, sv_genericPool, 0);
	mapcycle->day = day;
	mapcycle->type = Mem_PoolStrDup(gameType, sv_genericPool, 0);
	Com_DPrintf(DEBUG_SERVER, "mapcycle add: '%s' type '%s'\n", mapcycle->map, mapcycle->type);
	mapcycle->next = NULL;
	mapcycleCount++;
}

/**
 * @brief Parses the server mapcycle
 * @sa SV_MapcycleAdd
 * @sa SV_MapcycleClear
 */
static void SV_ParseMapcycle (void)
{
	int length = 0;
	byte *buffer = NULL;
	const char *token;
	const char *buf;
	char map[MAX_VAR], gameType[MAX_VAR];

	mapcycleCount = 0;
	mapcycleList = NULL;

	length = FS_LoadFile("mapcycle.txt", &buffer);
	if (length == -1 || !buffer)
		return;

	if (length != -1) {
		buf = (const char*)buffer;
		do {
			qboolean day = qfalse;
			/* parse map name */
			token = Com_Parse(&buf);
			if (!buf)
				break;
			Q_strncpyz(map, token, sizeof(map));
			/* parse day or night */
			token = Com_Parse(&buf);
			if (!buf)
				break;
			if (!strcmp(token, "day"))
				day = qtrue;
			else if (strcmp(token, "night")) {
				Com_Printf("Skip mapcycle parsing, expected day or night.");
				break;
			}
			/* parse gametype */
			token = Com_Parse(&buf);
			if (!buf)
				break;
			Q_strncpyz(gameType, token, sizeof(gameType));
			SV_MapcycleAdd(map, day, gameType);
		} while (buf);

		Com_Printf("added %i maps to the mapcycle\n", mapcycleCount);
	}
	FS_FreeFile(buffer);
}

#define	HEARTBEAT_SECONDS	300

static SDL_Thread *masterServerHeartBeatThread;

/**
 * @brief Send a message to the master every few minutes to
 * let it know we are alive, and log information
 */
static int Master_HeartbeatThread (void * data)
{
	char *responseBuf;

	/* send to master */
	Com_Printf("sending heartbeat\n");
	responseBuf = HTTP_GetURL(va("%s/ufo/masterserver.php?heartbeat&port=%s", masterserver_url->string, port->string));
	if (responseBuf) {
		Com_DPrintf(DEBUG_SERVER, "response: %s\n", responseBuf);
		Mem_Free(responseBuf);
	}

	masterServerHeartBeatThread = NULL;
	return 0;
}

/**
 * @sa CL_PingServers_f
 */
static void Master_Heartbeat (void)
{
	if (!sv_dedicated || !sv_dedicated->integer)
		return;		/* only dedicated servers send heartbeats */

	if (!sv_public || !sv_public->integer)
		return;		/* a private dedicated game */

	/* check for time wraparound */
	if (svs.lastHeartbeat > svs.realtime)
		svs.lastHeartbeat = svs.realtime;

	if (svs.realtime - svs.lastHeartbeat < HEARTBEAT_SECONDS * 1000)
		return;					/* not time to send yet */

	svs.lastHeartbeat = svs.realtime;

	if (masterServerHeartBeatThread != NULL)
		SDL_WaitThread(masterServerHeartBeatThread, NULL);

	masterServerHeartBeatThread = SDL_CreateThread(Master_HeartbeatThread, NULL);
}

/**
 * @brief If all connected clients have set their ready flag the server will spawn the clients
 * and that change the client state.
 * @sa SV_Spawn_f
 */
static void SV_CheckGameStart (void)
{
	int i;
	client_t *cl;

	/* already started? */
	if (sv.started)
		return;

	if (sv_maxclients->integer > 1) {
		for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
			/* all players must be connected and all of them must have set
			 * the ready flag */
			if (cl->state != cs_began || !cl->player->isReady)
				return;
	} else if (svs.clients[0].state != cs_began) {
		/* in single player mode we must have received the 'begin' */
		return;
	}

	sv.started = qtrue;

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
		if (cl && cl->state != cs_free)
			SV_ClientCommand(cl, "spawnsoldiers\n");
}

/**
 * @sa Qcommon_Frame
 */
void SV_Frame (int now, void *data)
{
	/* change the gametype even if no server is running (e.g. the first time) */
	if (sv_dedicated->integer && sv_gametype->modified) {
		Com_SetGameType();
		sv_gametype->modified = qfalse;
	}

	if (sv_dedicated->integer) {
		const char *s;
		do {
			s = Sys_ConsoleInput();
			if (s)
				Cbuf_AddText(va("%s\n", s));
		} while (s);
	}

	/* if server is not active, do nothing */
	if (!svs.initialized)
		return;

	svs.realtime = now;

	/* keep the random time dependent */
	rand();

	SV_CheckGameStart();

	if (!sv_threads->integer)
		SV_RunGameFrame();

	/* next map in the cycle */
	if (sv.endgame && sv_maxclients->integer > 1)
		SV_NextMapcycle();

	/* send a heartbeat to the master if needed */
	Master_Heartbeat();

	/* server is empty - so shutdown */
	if (abandon && killserver) {
		abandon = qfalse;
		killserver = qfalse;
		SV_Shutdown("Server disconnected.", qfalse);
	}
}

/**
 * @brief Informs all masters that this server is going down
 */
static void Master_Shutdown (void)
{
	char *responseBuf;

	if (!sv_dedicated || !sv_dedicated->integer)
		return;					/* only dedicated servers send heartbeats */

	if (!sv_public || !sv_public->integer)
		return;					/* a private dedicated game */

	/* send to master */
	responseBuf = HTTP_GetURL(va("%s/ufo/masterserver.php?shutdown&port=%s", masterserver_url->string, port->string));
	if (responseBuf) {
		Com_DPrintf(DEBUG_SERVER, "response: %s\n", responseBuf);
		Mem_Free(responseBuf);
	}
}

/**
 * @brief Pull specific info from a newly changed userinfo string into a more C friendly form.
 */
void SV_UserinfoChanged (client_t * cl)
{
	const char *val;
	unsigned int i;

	/* call prog code to allow overrides */
	SDL_mutexP(svs.serverMutex);
	svs.ge->ClientUserinfoChanged(cl->player, cl->userinfo);
	SDL_mutexV(svs.serverMutex);

	/* name for C code */
	strncpy(cl->name, Info_ValueForKey(cl->userinfo, "cl_name"), sizeof(cl->name) - 1);
	/* mask off high bit */
	for (i = 0; i < sizeof(cl->name); i++)
		cl->name[i] &= 127;

	/* msg command */
	val = Info_ValueForKey(cl->userinfo, "cl_msg");
	if (strlen(val))
		cl->messagelevel = atoi(val);

	Com_DPrintf(DEBUG_SERVER, "SV_UserinfoChanged: Changed userinfo for player %s\n", cl->name);
}

static qboolean SV_CheckMaxSoldiersPerPlayer (cvar_t* cvar)
{
	const int max = MAX_ACTIVETEAM;
	return Cvar_AssertValue(cvar, 1, max, qtrue);
}

const routing_t** SV_GetRoutingMap (void)
{
	return (const routing_t **) &sv.svMap;
}

const mapData_t* SV_GetMapData (void)
{
	return &sv.mapData;
}

/**
 * @brief Only called once at startup, not for each game
 */
void SV_Init (void)
{
	Com_Printf("\n------ server initialization -------\n");

	sv_gameSysPool = Mem_CreatePool("Server: Game system");
	sv_genericPool = Mem_CreatePool("Server: Generic");

	memset(&svs, 0, sizeof(svs));

	SV_InitOperatorCommands();

	rcon_password = Cvar_Get("rcon_password", "", 0, NULL);
	Cvar_Get("sv_cheats", "0", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	Cvar_Get("sv_protocol", DOUBLEQUOTE(PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_NOSET, NULL);
	/* this cvar will become a latched cvar when you start the server */
	sv_maxclients = Cvar_Get("sv_maxclients", "1", CVAR_SERVERINFO, "Max. connected clients");
	sv_hostname = Cvar_Get("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE, "The name of the server that is displayed in the serverlist");
	sv_http_downloadserver = Cvar_Get("sv_http_downloadserver", "", CVAR_ARCHIVE, "URL to a location where clients can download game content over HTTP");
	sv_enablemorale = Cvar_Get("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "Enable morale changes in multiplayer");
	sv_maxsoldiersperteam = Cvar_Get("sv_maxsoldiersperteam", "4", CVAR_ARCHIVE | CVAR_SERVERINFO, "Max. amount of soldiers per team (see sv_maxsoldiersperplayer and sv_teamplay)");
	sv_maxsoldiersperplayer = Cvar_Get("sv_maxsoldiersperplayer", "8", CVAR_ARCHIVE | CVAR_SERVERINFO, "Max. amount of soldiers each player can controll (see maxsoldiers and sv_teamplay)");
	Cvar_SetCheckFunction("sv_maxsoldiersperplayer", SV_CheckMaxSoldiersPerPlayer);

	sv_dumpmapassembly = Cvar_Get("sv_dumpmapassembly", "0", CVAR_ARCHIVE, "Dump map assembly information to game console");

	sv_threads = Cvar_Get("sv_threads", "1", CVAR_LATCH | CVAR_ARCHIVE, "Run the server threaded");
	sv_public = Cvar_Get("sv_public", "1", 0, "Should heartbeats be send to the masterserver");
	sv_reconnect_limit = Cvar_Get("sv_reconnect_limit", "3", CVAR_ARCHIVE, "Minimum seconds between connect messages");

	if (sv_dedicated->integer)
		Cvar_Set("sv_maxclients", "8");

	sv_maxclients->modified = qfalse;

	SV_ParseMapcycle();
}

/**
 * @brief Used by SV_Shutdown to send a final message to all
 * connected clients before the server goes down.
 * @sa SV_Shutdown
 */
static void SV_FinalMessage (const char *message, qboolean reconnect)
{
	int i;
	client_t *cl;
	struct dbuffer *msg = new_dbuffer();

	if (reconnect)
		NET_WriteByte(msg, svc_reconnect);
	else
		NET_WriteByte(msg, svc_disconnect);
	NET_WriteString(msg, message);

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
		if (cl->state >= cs_connected) {
			NET_WriteConstMsg(cl->stream, msg);
			NET_StreamFinished(cl->stream);
			cl->stream = NULL;
		}

	/* make sure, that this is send */
	NET_Wait(0);

	free_dbuffer(msg);
}

/**
 * @brief Cleanup on game shutdown
 * @sa SV_Shutdown
 * @sa Com_Quit
 */
void SV_Clear (void)
{
	SV_MapcycleClear();
}

/**
 * @brief Called when each game quits, before Sys_Quit or Sys_Error
 * @param[in] finalmsg The message all clients get as server shutdown message
 * @param[in] reconnect True if this is only a restart (new map or map restart),
 * false if the server shutdown completely and you also want to disconnect all clients
 */
void SV_Shutdown (const char *finalmsg, qboolean reconnect)
{
	unsigned int i;

	if (!svs.initialized)
		return;

	if (svs.clients)
		SV_FinalMessage(finalmsg, reconnect);

	Com_Printf("Shutdown server: %s\n", finalmsg);

	Master_Shutdown();
	SV_ShutdownGameProgs();

	NET_DatagramSocketClose(svs.netDatagramSocket);
	SV_Stop();

	for (i = 0; i < sv.numSVModels; i++)
		if (sv.svModels[i].name)
			Mem_Free(sv.svModels[i].name);

	/* free current level */
	memset(&sv, 0, sizeof(sv));

	/* free server static data */
	if (svs.clients)
		Mem_Free(svs.clients);

	if (svs.serverMutex != NULL)
		SDL_DestroyMutex(svs.serverMutex);

	memset(&svs, 0, sizeof(svs));

	/* maybe we shut down before we init - e.g. in case of an error */
	if (sv_maxclients)
		sv_maxclients->flags &= ~CVAR_LATCH;

	if (sv_mapname)
		sv_mapname->flags &= ~CVAR_NOSET;
}

/**
 * @brief Will eventually shutdown the server once all clients have disconnected
 * @sa SV_CountPlayers
 */
void SV_ShutdownWhenEmpty (void)
{
	abandon = qtrue;
	/* pretend server is already dead, otherwise clients may try and reconnect */
	Com_SetServerState(ss_dead);
}

/**
 * @brief Returns the number of spawned players
 * @sa SV_ShutdownWhenEmpty
 */
int SV_CountPlayers (void)
{
	int i;
	int count = 0;
	client_t *cl;

	if (!svs.initialized)
		return 0;

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state != cs_spawned)
			continue;

		count++;
	}

	return count;
}
