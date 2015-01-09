/**
 * @file
 * @brief Main server code?
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "sv_log.h"
#include "../ports/system.h"
#include "../shared/scopedmutex.h"
#include "../shared/thread.h"

/** password for remote server commands */
static cvar_t* rcon_password;
static cvar_t* sv_http_downloadserver;
static cvar_t* sv_enablemorale;
static cvar_t* sv_maxsoldiersperteam;
static cvar_t* sv_maxsoldiersperplayer;
static cvar_t* sv_hostname;
/** minimum seconds between connect messages */
static cvar_t* sv_reconnect_limit;
static cvar_t* sv_timeout;			/* seconds without any message */

cvar_t* sv_maxclients = nullptr;
cvar_t* sv_dumpmapassembly;
cvar_t* sv_threads;
cvar_t* sv_rma;
cvar_t* sv_rmadisplaythemap;
/** should heartbeats be sent */
cvar_t* sv_public;
cvar_t* sv_mapname;

memPool_t* sv_genericPool;

typedef struct leakyBucket_s {
	char node[64];

	int lastTime;
	signed char burst;

	long hash;

	struct leakyBucket_s* prev;
	struct leakyBucket_s* next;
} leakyBucket_t;

/* This is deliberately quite large to make it more of an effort to DoS */
#define MAX_BUCKETS			16384
#define MAX_HASHES			1024

static leakyBucket_t buckets[MAX_BUCKETS];
static leakyBucket_t* bucketHashes[MAX_HASHES];
static leakyBucket_t outboundLeakyBucket;

char* SV_GetConfigString (int index)
{
	if (!Com_CheckConfigStringIndex(index))
		Com_Error(ERR_FATAL, "Invalid config string index given: %i", index);

	return sv->configstrings[index];
}

int SV_GetConfigStringLength (int index)
{
	if (!Com_CheckConfigStringIndex(index))
		Com_Error(ERR_FATAL, "Invalid config string index given: %i", index);

	switch (index) {
	case CS_ENTITYSTRING:
		return MAX_TOKEN_CHARS * MAX_ENTITYSTRINGS;
	case CS_TILES:
		return MAX_TOKEN_CHARS * MAX_TILESTRINGS;
	case CS_POSITIONS:
		return MAX_TOKEN_CHARS * MAX_TILESTRINGS;
	case CS_MODELS:
		return MAX_TOKEN_CHARS * MAX_MODELS;
	case CS_PLAYERNAMES:
		return MAX_TOKEN_CHARS * MAX_CLIENTS;
	case CS_GENERAL:
		return MAX_TOKEN_CHARS * MAX_GENERAL;
	default:
		return MAX_TOKEN_CHARS;
	}
}

int SV_GetConfigStringInteger (int index)
{
	return atoi(SV_GetConfigString(index));
}

char* SV_SetConfigString (int index, ...)
{
	va_list ap;
	const char* value;

	if (!Com_CheckConfigStringIndex(index))
		Com_Error(ERR_FATAL, "Invalid config string index given: %i", index);

	va_start(ap, index);

	switch (index) {
	case CS_LIGHTMAP:
	case CS_MAPCHECKSUM:
	case CS_UFOCHECKSUM:
	case CS_OBJECTAMOUNT:
		value = va("%i", va_arg(ap, int));
		break;
	default:
		value = va_arg(ap, char*);
		break;
	}

	/* change the string in sv
	 * there may be overflows in i==CS_TILES - but thats ok
	 * see definition of configstrings and MAX_TILESTRINGS */
	if (index == CS_TILES || index == CS_POSITIONS)
		Q_strncpyz(sv->configstrings[index], value, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
	else
		Q_strncpyz(sv->configstrings[index], value, sizeof(sv->configstrings[index]));

	va_end(ap);

	return sv->configstrings[index];
}

/**
 * @brief Iterates through clients
 * @param[in] lastClient Pointer of the client to iterate from. call with nullptr to get the first one.
 */
client_t* SV_GetNextClient (client_t* lastClient)
{
	client_t* endOfClients = &svs.clients[sv_maxclients->integer];
	client_t* client;

	if (!sv_maxclients->integer)
		return nullptr;

	if (!lastClient)
		return svs.clients;
	assert(lastClient >= svs.clients);
	assert(lastClient < endOfClients);

	client = lastClient;

	client++;
	if (client >= endOfClients)
		return nullptr;
	else
		return client;
}

client_t* SV_GetClient (int index)
{
	return &svs.clients[index];
}

/**
 * @brief Called when the player is totally leaving the server, either willingly
 * or unwillingly. This is NOT called if the entire server is quitting
 * or crashing.
 */
void SV_DropClient (client_t* drop, const char* message)
{
	/* add the disconnect */
	dbuffer msg(2 + strlen(message));
	NET_WriteByte(&msg, svc_disconnect);
	NET_WriteString(&msg, message);
	NET_WriteMsg(drop->stream, msg);
	SV_BroadcastPrintf(PRINT_CHAT, "%s was dropped from the server - reason: %s\n", drop->name, message);

	if (drop->state == cs_spawned || drop->state == cs_spawning) {
		/* call the prog function for removing a client */
		/* this will remove the body, among other things */
		const ScopedMutex scopedMutex(svs.serverMutex);
		svs.ge->ClientDisconnect(*drop->player);
	}

	NET_StreamFinished(drop->stream);
	drop->stream = nullptr;

	drop->player->setInUse(false);
	SV_SetClientState(drop, cs_free);
	drop->name[0] = 0;

	if (svs.abandon) {
		int count = 0;
		client_t* cl = nullptr;
		while ((cl = SV_GetNextClient(cl)) != nullptr)
			if (cl->state >= cs_connected)
				count++;
		if (count == 0)
			svs.killserver = true;
	}
}

/*
==============================================================================
CONNECTIONLESS COMMANDS
==============================================================================
*/

/**
 * @brief Find or allocate a bucket for an address
 */
static leakyBucket_t* SVC_BucketForAddress (struct net_stream& address, int burst, int period)
{
	char node[64];
	NET_StreamPeerToName(&address, node, sizeof(node), false);

	const long hash = Com_HashKey(node, MAX_HASHES);
	const int now = Sys_Milliseconds();

	for (leakyBucket_t* bucket = bucketHashes[hash]; bucket; bucket = bucket->next) {
		if (!Q_streq(bucket->node, node))
			continue;

		return bucket;
	}

	for (int i = 0; i < MAX_BUCKETS; i++) {
		leakyBucket_t* bucket = &buckets[i];
		const int interval = now - bucket->lastTime;

		/* Reclaim expired buckets */
		if (bucket->lastTime > 0 && (interval > (burst * period) || interval < 0)) {
			if (bucket->prev != nullptr) {
				bucket->prev->next = bucket->next;
			} else {
				bucketHashes[bucket->hash] = bucket->next;
			}

			if (bucket->next != nullptr) {
				bucket->next->prev = bucket->prev;
			}

			OBJZERO(*bucket);
		}

		if (Q_strnull(bucket->node)) {
			Q_strncpyz(bucket->node, node, sizeof(bucket->node));
			bucket->lastTime = now;
			bucket->burst = 0;
			bucket->hash = hash;

			/* Add to the head of the relevant hash chain */
			bucket->next = bucketHashes[hash];
			if (bucketHashes[hash] != nullptr) {
				bucketHashes[hash]->prev = bucket;
			}

			bucket->prev = nullptr;
			bucketHashes[hash] = bucket;

			return bucket;
		}
	}

	/* Couldn't allocate a bucket for this address */
	return nullptr;
}

static bool SVC_RateLimit (leakyBucket_t* bucket, int burst = 10, int period = 100)
{
	if (bucket == nullptr)
		return true;

	const int now = Sys_Milliseconds();
	const int interval = now - bucket->lastTime;
	const int expired = interval / period;
	const int expiredRemainder = interval % period;

	if (expired > bucket->burst) {
		bucket->burst = 0;
		bucket->lastTime = now;
	} else {
		bucket->burst -= expired;
		bucket->lastTime = now - expiredRemainder;
	}

	if (bucket->burst < burst) {
		bucket->burst++;
		return false;
	}

	return true;
}

/**
 * @brief Rate limit for a particular address
 */
static bool SVC_RateLimitAddress (struct net_stream& from, int burst = 10, int period = 1000)
{
	leakyBucket_t* bucket = SVC_BucketForAddress(from, burst, period);
	return SVC_RateLimit(bucket, burst, period);
}

/**
 * @brief Responds with teaminfo such as free team num
 * @sa CL_ParseTeamInfoMessage
 */
static void SVC_TeamInfo (struct net_stream* s)
{
	if (SVC_RateLimitAddress(*s)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_TeamInfo: rate limit from %s exceeded, dropping request\n", NET_StreamToString(s));
		return;
	}

	/* Allow getinfo to be DoSed relatively easily, but prevent excess outbound bandwidth usage when being flooded inbound */
	if (SVC_RateLimit(&outboundLeakyBucket)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_TeamInfo: rate limit exceeded, dropping request\n");
		return;
	}

	char infoGlobal[MAX_INFO_STRING] = "";
	Info_SetValueForKey(infoGlobal, sizeof(infoGlobal), "sv_teamplay", Cvar_GetString("sv_teamplay"));
	Info_SetValueForKey(infoGlobal, sizeof(infoGlobal), "sv_maxteams", Cvar_GetString("sv_maxteams"));
	Info_SetValueForKey(infoGlobal, sizeof(infoGlobal), "sv_maxplayersperteam", Cvar_GetString("sv_maxplayersperteam"));

	dbuffer msg;
	NET_WriteByte(&msg, svc_oob);
	NET_WriteRawString(&msg, "teaminfo\n");
	NET_WriteString(&msg, infoGlobal);

	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (cl->state < cs_connected)
			continue;
		char infoPlayer[MAX_INFO_STRING] = "";
		/* show players that already have a team with their teamnum */
		int teamId = svs.ge->ClientGetTeamNum(*cl->player);
		if (!teamId)
			teamId = TEAM_NO_ACTIVE;
		Info_SetValueForKeyAsInteger(infoPlayer, sizeof(infoPlayer), "cl_team", teamId);
		Info_SetValueForKeyAsInteger(infoPlayer, sizeof(infoPlayer), "cl_ready", svs.ge->ClientIsReady(cl->player));
		Info_SetValueForKey(infoPlayer, sizeof(infoPlayer), "cl_name", cl->name);
		NET_WriteString(&msg, infoPlayer);
	}

	NET_WriteByte(&msg, 0);

	NET_WriteMsg(s, msg);
}

/**
 * @brief Responds with all the info that the server browser can see
 * @sa SV_StatusString
 */
static void SVC_Status (struct net_stream* s)
{
	if (SVC_RateLimitAddress(*s)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_Status: rate limit from %s exceeded, dropping request\n", NET_StreamToString(s));
		return;
	}

	/* Allow getstatus to be DoSed relatively easily, but prevent excess outbound bandwidth usage when being flooded inbound */
	if (SVC_RateLimit(&outboundLeakyBucket, 10, 100)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_Status: rate limit exceeded, dropping request\n");
		return;
	}

	dbuffer msg;
	NET_WriteByte(&msg, svc_oob);
	NET_WriteRawString(&msg, SV_CMD_PRINT "\n");
	char info[MAX_INFO_STRING];
	NET_WriteRawString(&msg, Cvar_Serverinfo(info, sizeof(info)));
	NET_WriteRawString(&msg, "\n");

	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (cl->state <= cs_free)
			continue;

		char player[1024];
		Com_sprintf(player, sizeof(player), "%i \"%s\"\n", svs.ge->ClientGetTeamNum(*cl->player), cl->name);
		NET_WriteRawString(&msg, player);
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
static void SVC_Info (struct net_stream* s)
{
	if (SVC_RateLimitAddress(*s)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_Info: rate limit from %s exceeded, dropping request\n", NET_StreamToString(s));
		return;
	}

	/* Allow getinfo to be DoSed relatively easily, but prevent excess outbound bandwidth usage when being flooded inbound */
	if (SVC_RateLimit(&outboundLeakyBucket)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_Info: rate limit exceeded, dropping request\n");
		return;
	}

	if (sv_maxclients->integer == 1) {
		Com_DPrintf(DEBUG_SERVER, "Ignore info string in singleplayer mode\n");
		return;	/* ignore in single player */
	}

	const int version = atoi(Cmd_Argv(1));
	if (version != PROTOCOL_VERSION) {
		char string[MAX_VAR];
		Com_sprintf(string, sizeof(string), "%s: wrong version (client: %i, host: %i)\n", sv_hostname->string, version, PROTOCOL_VERSION);
		NET_OOB_Printf(s, SV_CMD_PRINT "\n%s", string);
		return;
	}

	int count = 0;

	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr)
		if (cl->state >= cs_spawning)
			count++;

	char infostring[MAX_INFO_STRING];
	infostring[0] = '\0';
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_protocol", DOUBLEQUOTE(PROTOCOL_VERSION));
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_hostname", sv_hostname->string);
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_dedicated", sv_dedicated->string);
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_gametype", sv_gametype->string);
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_mapname", sv->name);
	Info_SetValueForKeyAsInteger(infostring, sizeof(infostring), "clients", count);
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_maxclients", sv_maxclients->string);
	Info_SetValueForKey(infostring, sizeof(infostring), "sv_version", UFO_VERSION);
	NET_OOB_Printf(s, SV_CMD_INFO "\n%s", infostring);
}


/**
 * @brief A connection request that did not come from the master
 * @sa CL_ConnectionlessPacket
 */
static void SVC_DirectConnect (struct net_stream* stream)
{
	Com_DPrintf(DEBUG_SERVER, "SVC_DirectConnect()\n");

	if (sv->started || sv->spawned) {
		Com_Printf("rejected connect because match is already running\n");
		NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_GAME_ALREADY_STARTED "\n");
		return;
	}

	char buf[256];
	const char* peername = NET_StreamPeerToName(stream, buf, sizeof(buf), false);

	const int version = atoi(Cmd_Argv(1));
	if (version != PROTOCOL_VERSION) {
		Com_Printf("rejected connect from version %i - %s\n", version, peername);
		NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_SERVER_VERSION_MISMATCH "\n");
		return;
	}

	char userinfo[MAX_INFO_STRING];
	Q_strncpyz(userinfo, Cmd_Argv(2), sizeof(userinfo));

	if (Q_strnull(userinfo)) {  /* catch empty userinfo */
		Com_Printf("Empty userinfo from %s\n", peername);
		NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_CONNECTION_REFUSED "\n");
		return;
	}

	if (strchr(userinfo, '\xFF')) {  /* catch end of message in string exploit */
		Com_Printf("Illegal userinfo contained xFF from %s\n", peername);
		NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_CONNECTION_REFUSED "\n");
		return;
	}

	if (strlen(Info_ValueForKey(userinfo, "ip"))) {  /* catch spoofed ips  */
		Com_Printf("Illegal userinfo contained ip from %s\n", peername);
		NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_CONNECTION_REFUSED "\n");
		return;
	}

	/* force the IP key/value pair so the game can filter based on ip */
	Info_SetValueForKey(userinfo, sizeof(userinfo), "ip", peername);

	/* find a client slot */
	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr)
		if (cl->state == cs_free)
			break;
	if (cl == nullptr) {
		NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_SERVER_FULL "\n");
		Com_Printf("Rejected a connection - server is full.\n");
		return;
	}

	/* build a new connection - accept the new client
	 * this is the only place a client_t is ever initialized */
	OBJZERO(*cl);
	const int playernum = cl - SV_GetClient(0);
	SrvPlayer* player = PLAYER_NUM(playernum);
	cl->player = player;
	cl->player->setNum(playernum);

	bool connected;
	{
		const ScopedMutex scopedMutex(svs.serverMutex);
		connected = svs.ge->ClientConnect(player, userinfo, sizeof(userinfo));
	}

	/* get the game a chance to reject this connection or modify the userinfo */
	if (!connected) {
		const char* rejmsg = Info_ValueForKey(userinfo, "rejmsg");
		if (rejmsg[0] != '\0') {
			NET_OOB_Printf(stream, SV_CMD_PRINT "\n%s\n" REJ_CONNECTION_REFUSED "\n", rejmsg);
			Com_Printf("Game rejected a connection from %s. Reason: %s\n", peername, rejmsg);
		} else {
			NET_OOB_Printf(stream, SV_CMD_PRINT "\n" REJ_CONNECTION_REFUSED "\n");
			Com_Printf("Game rejected a connection from %s.\n", peername);
		}
		return;
	}

	/* new player */
	cl->player->setInUse(true);
	cl->lastmessage = svs.realtime;

	/* parse some info from the info strings */
	Q_strncpyz(cl->userinfo, userinfo, sizeof(cl->userinfo));
	SV_UserinfoChanged(cl);

	/* send the connect packet to the client */
	if (sv_http_downloadserver->string[0])
		NET_OOB_Printf(stream, CL_CMD_CLIENT_CONNECT " dlserver=%s", sv_http_downloadserver->string);
	else
		NET_OOB_Printf(stream, CL_CMD_CLIENT_CONNECT);

	SV_SetClientState(cl, cs_connected);

	Q_strncpyz(cl->peername, peername, sizeof(cl->peername));
	cl->stream = stream;
	NET_StreamSetData(stream, cl);
}

/**
 * @brief Checks whether the remote connection is allowed (rcon_password must be
 * set on the server) - and verify the user given password with the cvar value.
 */
static inline bool Rcon_Validate (const char* password)
{
	/* no rcon access */
	if (Q_strnull(rcon_password->string))
		return false;

	/* password not valid */
	if (!Q_streq(password, rcon_password->string))
		return false;

	return true;
}

/**
 * @brief A client issued an rcon command. Shift down the remaining args. Redirect all printfs
 */
static void SVC_RemoteCommand (struct net_stream* stream)
{
	char buf[64];
	const char* peername = NET_StreamPeerToName(stream, buf, sizeof(buf), false);

	/* Prevent using rcon as an amplifier and make dictionary attacks impractical */
	if (SVC_RateLimitAddress(*stream)) {
		Com_DPrintf(DEBUG_SERVER, "SVC_RemoteCommand: rate limit from %s exceeded, dropping request\n", peername);
		return;
	}

	const bool valid = Rcon_Validate(Cmd_Argv(1));
	if (!valid) {
		static leakyBucket_t bucket;
		/* Make DoS via rcon impractical */
		if (SVC_RateLimit(&bucket, 10, 1000)) {
			Com_DPrintf(DEBUG_SERVER, "SVC_RemoteCommand: rate limit exceeded, dropping request\n");
			return;
		}

		Com_Printf("Bad rcon from %s with password: '%s'\n", peername, Cmd_Argv(1));
	} else {
		Com_Printf("Rcon from %s\n", peername);
	}

	static char sv_outputbuf[1024];
	Com_BeginRedirect(stream, sv_outputbuf, sizeof(sv_outputbuf));

	if (!valid) {
		/* inform the client */
		Com_Printf(BAD_RCON_PASSWORD);
	} else {
		char remaining[1024] = "";
		int i;

		/* execute the rcon commands */
		for (i = 2; i < Cmd_Argc(); i++) {
			Q_strcat(remaining, sizeof(remaining), "%s ", Cmd_Argv(i));
		}

		/* execute the string */
		Cmd_ExecuteString("%s", remaining);
	}

	Com_EndRedirect();
}

/**
 * @brief Handles a connectionless message from a client
 * @sa NET_OOB_Printf
 * @param[out] stream The stream to write to
 * @param msg The message buffer to read the connectionless data from
 */
static void SV_ConnectionlessPacket (struct net_stream* stream, dbuffer* msg)
{
	char s[512];

	NET_ReadStringLine(msg, s, sizeof(s));
	Cmd_TokenizeString(s, false, false);

	const char* c = Cmd_Argv(0);
	Com_DPrintf(DEBUG_SERVER, "Packet : %s\n", c);

	if (Q_streq(c, SV_CMD_TEAMINFO)) {
		SVC_TeamInfo(stream);
	} else if (Q_streq(c, SV_CMD_INFO)) {
		SVC_Info(stream);
	} else if (Q_streq(c, SV_CMD_STATUS)) {
		SVC_Status(stream);
	} else if (Q_streq(c, SV_CMD_CONNECT)) {
		SVC_DirectConnect(stream);
	} else if (Q_streq(c, SV_CMD_RCON)) {
		SVC_RemoteCommand(stream);
	} else {
		char buf[256];
		Com_Printf("Bad connectionless packet from %s:\n%s\n", NET_StreamPeerToName(stream, buf, sizeof(buf), true), s);
	}
}

/**
 * @sa CL_ReadPacket
 * @sa NET_ReadMsg
 * @sa SV_Start
 */
void SV_ReadPacket (struct net_stream* s)
{
	client_t* cl = static_cast<client_t* >(NET_StreamGetData(s));
	dbuffer* msg;

	while ((msg = NET_ReadMsg(s))) {
		const int cmd = NET_ReadByte(msg);

		if (cmd == clc_oob)
			SV_ConnectionlessPacket(s, msg);
		else if (cl)
			SV_ExecuteClientMessage(cl, cmd, msg);
		else {
			NET_StreamFree(s);
			s = nullptr;
		}

		delete msg;
	}
}

#define	HEARTBEAT_SECONDS	30

static SDL_Thread* masterServerHeartBeatThread;

/**
 * @brief Send a message to the master every few minutes to
 * let it know we are alive, and log information
 */
static int Master_HeartbeatThread (void*  data)
{
	char url[512];
	Com_sprintf(url, sizeof(url), "%s/ufo/masterserver.php?heartbeat&port=%s", masterserver_url->string, port->string);

	/* send to master */
	Com_Printf("sending heartbeat\n");
	HTTP_GetURL(url, nullptr);

	masterServerHeartBeatThread = nullptr;
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

	if (masterServerHeartBeatThread != nullptr)
		SDL_WaitThread(masterServerHeartBeatThread, nullptr);

	masterServerHeartBeatThread = Com_CreateThread(Master_HeartbeatThread, "HeartbeatThread");
}

/**
 * @brief If all connected clients have set their ready flag the server will spawn the clients
 * and that change the client state.
 * @sa SV_Spawn_f
 */
static void SV_CheckSpawnSoldiers (void)
{
	/* already started? */
	if (sv->spawned)
		return;

	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		/* all players must be connected and all of them must have set
		 * the ready flag */
		if (cl->state != cs_began || !cl->player->isReady())
			return;
	}

	sv->spawned = true;

	cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr)
		if (cl->state != cs_free)
			SV_ClientCommand(cl, CL_SPAWNSOLDIERS "\n");
}

static void SV_CheckStartMatch (void)
{
	client_t* cl;

	if (!sv->spawned || sv->started)
		return;

	if (sv_maxclients->integer > 1) {
		cl = nullptr;
		while ((cl = SV_GetNextClient(cl)) != nullptr) {
			/* all players must have their actors spawned */
			if (cl->state != cs_spawned)
				return;
		}
	} else if (SV_GetClient(0)->state != cs_spawned) {
		/* in single player mode we must have received the 'spawnsoldiers' */
		return;
	}

	sv->started = true;

	cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr)
		if (cl->state != cs_free)
			SV_ClientCommand(cl, CL_STARTMATCH "\n");
}

#define	PING_SECONDS	5

static void SV_PingPlayers (void)
{
	/* check for time wraparound */
	if (svs.lastPing > svs.realtime)
		svs.lastPing = svs.realtime;

	if (svs.realtime - svs.lastPing < PING_SECONDS * 1000)
		return;					/* not time to send yet */

	svs.lastPing = svs.realtime;
	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (cl->state == cs_free)
			continue;
		dbuffer msg(1);
		NET_WriteByte(&msg, svc_ping);
		NET_WriteMsg(cl->stream, msg);
	}
}

static void SV_CheckTimeouts (void)
{
	const int droppoint = svs.realtime - 1000 * sv_timeout->integer;

	if (sv_maxclients->integer == 1)
		return;

	client_t* cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (cl->state == cs_free)
			continue;

		/* might be invalid across a mapchange */
		if (cl->lastmessage > svs.realtime)
			cl->lastmessage = svs.realtime;

		if (cl->lastmessage > 0 && cl->lastmessage < droppoint)
			SV_DropClient(cl, "timed out");
	}
}

/**
 * @sa Qcommon_Frame
 */
void SV_Frame (int now, void* data)
{
	Com_ReadFromPipe();

	/* change the gametype even if no server is running (e.g. the first time) */
	if (sv_dedicated->integer && sv_gametype->modified) {
		Com_SetGameType();
		sv_gametype->modified = false;
	}

	if (sv_dedicated->integer) {
		const char* s;
		do {
			s = Sys_ConsoleInput();
			if (s)
				Cbuf_AddText("%s\n", s);
		} while (s);
	}

	/* if server is not active, do nothing */
	if (!svs.initialized) {
#ifdef DEDICATED_ONLY
		Com_Printf("Starting next map from the mapcycle\n");
		SV_NextMapcycle();
#endif
		return;
	}

	svs.realtime = now;

	/* if time is about to hit the 32nd bit, kick all clients
	 * and clear sv.time, rather
	 * than checking for negative time wraparound everywhere.
	 * 2giga-milliseconds = 23 days, so it won't be too often */
	if (svs.realtime > 0x70000000) {
		SV_Map(true, sv->name, sv->assembly);
		return;
	}

	/* keep the random time dependent */
	rand();

	SV_CheckSpawnSoldiers();
	SV_CheckStartMatch();
	SV_CheckTimeouts();

	if (!sv_threads->integer)
		SV_RunGameFrame();
	else
		/* signal the game frame thread to wake up */
		SDL_CondSignal(svs.gameFrameCond);
	SV_LogHandleOutput();

	/* next map in the cycle */
	if (sv->endgame && sv_maxclients->integer > 1)
		SV_NextMapcycle();

	/* send a heartbeat to the master if needed */
	Master_Heartbeat();
	SV_PingPlayers();

	/* server is empty - so shutdown */
	if (svs.abandon && svs.killserver)
		SV_Shutdown("Server disconnected.", false);
}

/**
 * @brief Informs all masters that this server is going down
 */
static void Master_Shutdown (void)
{
	if (!sv_dedicated || !sv_dedicated->integer)
		return;					/* only dedicated servers send heartbeats */

	if (!sv_public || !sv_public->integer)
		return;					/* a private dedicated game */

	/* send to master */
	HTTP_GetURL(va("%s/ufo/masterserver.php?shutdown&port=%s", masterserver_url->string, port->string), nullptr);
}

/**
 * @brief Pull specific info from a newly changed userinfo string into a more C friendly form.
 */
void SV_UserinfoChanged (client_t* cl)
{
	/* call prog code to allow overrides */
	{
		const ScopedMutex scopedMutex(svs.serverMutex);
		svs.ge->ClientUserinfoChanged(*cl->player, cl->userinfo);
	}

	/* name of the player */
	Q_strncpyz(cl->name, Info_ValueForKey(cl->userinfo, "cl_name"), sizeof(cl->name));
	/* mask off high bit */
	for (int i = 0; i < sizeof(cl->name); i++)
		cl->name[i] &= 127;

	/* msg command */
	cl->messagelevel = Info_IntegerForKey(cl->userinfo, "cl_msg");

	Com_DPrintf(DEBUG_SERVER, "SV_UserinfoChanged: Changed userinfo for player %s\n", cl->name);
}

static bool SV_CheckMaxSoldiersPerPlayer (cvar_t* cvar)
{
	const int max = MAX_ACTIVETEAM;
	return Cvar_AssertValue(cvar, 1, max, true);
}

mapData_t* SV_GetMapData (void)
{
	return &sv->mapData;
}

mapTiles_t* SV_GetMapTiles (void)
{
	return &sv->mapTiles;
}

/**
 * @brief Only called once at startup, not for each game
 */
void SV_Init (void)
{
	Com_Printf("\n------ server initialization -------\n");

	sv_genericPool = Mem_CreatePool("Server: Generic");

	OBJZERO(svs);

	sv = Mem_PoolAllocType(serverInstanceGame_t, sv_genericPool);

	SV_InitOperatorCommands();

	rcon_password = Cvar_Get("rcon_password", "", CVAR_ARCHIVE);
	Cvar_Get("sv_cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);
	Cvar_Get("sv_protocol", DOUBLEQUOTE(PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_NOSET);
	/* this cvar will become a latched cvar when you start the server */
	sv_maxclients = Cvar_Get("sv_maxclients", "1", CVAR_SERVERINFO, "Max. connected clients");
	sv_hostname = Cvar_Get("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE, "The name of the server that is displayed in the serverlist");
	sv_http_downloadserver = Cvar_Get("sv_http_downloadserver", "", CVAR_ARCHIVE, "URL to a location where clients can download game content over HTTP");
	sv_enablemorale = Cvar_Get("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "Enable morale changes in multiplayer");
	sv_maxsoldiersperteam = Cvar_Get("sv_maxsoldiersperteam", "4", CVAR_ARCHIVE | CVAR_SERVERINFO, "Max. amount of soldiers per team (see sv_maxsoldiersperplayer and sv_teamplay)");
	sv_maxsoldiersperplayer = Cvar_Get("sv_maxsoldiersperplayer", DOUBLEQUOTE(MAX_ACTIVETEAM), CVAR_ARCHIVE | CVAR_SERVERINFO, "Max. amount of soldiers each player can control (see maxsoldiers and sv_teamplay)");
	Cvar_SetCheckFunction("sv_maxsoldiersperplayer", SV_CheckMaxSoldiersPerPlayer);

	sv_dumpmapassembly = Cvar_Get("sv_dumpmapassembly", "0", CVAR_ARCHIVE, "Dump map assembly information to game console");

	sv_threads = Cvar_Get("sv_threads", "1", CVAR_LATCH | CVAR_ARCHIVE, "Run the server threaded");
	sv_rma = Cvar_Get("sv_rma", "2", 0, "1 = old algorithm, 2 = new algorithm");
	sv_rmadisplaythemap = Cvar_Get("sv_rmadisplaythemap", "0", 0, "Activate rma problem output");
	sv_public = Cvar_Get("sv_public", "1", 0, "Should heartbeats be send to the masterserver");
	sv_reconnect_limit = Cvar_Get("sv_reconnect_limit", "3", CVAR_ARCHIVE, "Minimum seconds between connect messages");
	sv_timeout = Cvar_Get("sv_timeout", "200", CVAR_ARCHIVE, "Seconds until a client times out");

	SV_MapcycleInit();
	SV_LogInit();
}

/**
 * @brief Used by SV_Shutdown to send a final message to all
 * connected clients before the server goes down.
 * @sa SV_Shutdown
 */
static void SV_FinalMessage (const char* message, bool reconnect)
{
	client_t* cl;
	dbuffer msg(2 + strlen(message));

	if (reconnect)
		NET_WriteByte(&msg, svc_reconnect);
	else
		NET_WriteByte(&msg, svc_disconnect);
	NET_WriteString(&msg, message);

	cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr)
		if (cl->state >= cs_connected) {
			NET_WriteConstMsg(cl->stream, msg);
			NET_StreamFinished(cl->stream);
			cl->stream = nullptr;
		}

	/* make sure, that this is send */
	NET_Wait(0);
}

/**
 * @brief Cleanup when the whole game process is shutting down
 * @sa SV_Shutdown
 * @sa Com_Quit
 */
void SV_Clear (void)
{
	SV_MapcycleClear();
	SV_LogShutdown();
}

/**
 * @brief Called when each game quits, before Sys_Quit or Sys_Error
 * @param[in] finalmsg The message all clients get as server shutdown message
 * @param[in] reconnect True if this is only a restart (new map or map restart),
 * false if the server shutdown completely and you also want to disconnect all clients
 */
void SV_Shutdown (const char* finalmsg, bool reconnect)
{
	if (!svs.initialized)
		return;

	if (svs.clients)
		SV_FinalMessage(finalmsg, reconnect);

	Com_Printf("Shutdown server: %s\n", finalmsg);

	Master_Shutdown();
	SV_ShutdownGameProgs();

	NET_DatagramSocketClose(svs.netDatagramSocket);
	SV_Stop();

	for (int i = 0; i < sv->numSVModels; i++) {
		sv_model_t* model = &sv->svModels[i];
		Mem_Free(model->name);
	}

	/* free current level */
	OBJZERO(*sv);

	/* free server static data */
	Mem_Free(svs.clients);

	SDL_DestroyMutex(svs.serverMutex);

	OBJZERO(svs);

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
	svs.abandon = true;
	/* pretend server is already dead, otherwise clients may try and reconnect */
	Com_SetServerState(ss_dead);
}

/**
 * @brief Returns the number of spawned players
 * @sa SV_ShutdownWhenEmpty
 */
int SV_CountPlayers (void)
{
	int count = 0;
	client_t* cl;

	if (!svs.initialized)
		return 0;

	cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (cl->state != cs_spawned)
			continue;

		count++;
	}

	return count;
}
