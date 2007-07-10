/**
 * @file sv_main.c
 * @brief Main server code?
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

netadr_t master_adr;	/* address of group servers */

client_t *sv_client;			/* current client */

cvar_t *sv_timedemo;

cvar_t *timeout;				/* seconds without any message */
cvar_t *zombietime;				/* seconds to sink messages after disconnect */

cvar_t *rcon_password;			/* password for remote server commands */

cvar_t *sv_downloadserver;

cvar_t *sv_maxclients = NULL;
cvar_t *sv_showclamp;
cvar_t *sv_enablemorale;
cvar_t *maxsoldiers;
cvar_t *maxsoldiersperplayer;

cvar_t *hostname;
cvar_t *public_server;			/* should heartbeats be sent */

cvar_t *sv_reconnect_limit;		/* minimum seconds between connect messages */

cvar_t *masterserver_ip;
cvar_t *masterserver_port;

void Master_Shutdown(void);

static qboolean abandon = qfalse;		/* shutdown server when all clients disconnect and don't accept new connections */
static qboolean killserver = qfalse;	/* will initiate shutdown once abandon is set */

mapcycle_t *mapcycleList;
int mapcycleCount;

struct memPool_s *sv_gameSysPool;
struct memPool_s *sv_genericPool;

/*============================================================================ */


/**
 * @brief Called when the player is totally leaving the server, either willingly
 * or unwillingly.  This is NOT called if the entire server is quiting
 * or crashing.
 */
void SV_DropClient (client_t * drop)
{
	/* add the disconnect */
	MSG_WriteByte(&drop->netchan.message, svc_disconnect);

	if (drop->state == cs_spawned || drop->state == cs_spawning) {
		/* call the prog function for removing a client */
		/* this will remove the body, among other things */
		ge->ClientDisconnect(drop->player);
	}

	drop->player->inuse = qfalse;
	drop->state = cs_zombie;	/* become free in a few seconds */
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

/**
 * @brief Transfers a netadr to player name
 */
static const char *SV_FindPlayer (netadr_t from)
{
	int			i;
	client_t	*cl;

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++ ) {
		if (cl->state == cs_free)
			continue;

		if (!NET_CompareBaseAdr(from, cl->netchan.remote_address))
			continue;

		return cl->name;
	}

	return "";
}

/*
==============================================================================
CONNECTIONLESS COMMANDS
==============================================================================
*/

/**
 * @brief Builds the string that is sent as heartbeats and status replies
 * @sa Cvar_Serverinfo
 */
static char *SV_StatusString (void)
{
	char player[1024];
	static char status[MAX_MSGLEN - 16];
	int i;
	client_t *cl;
	unsigned int statusLength;
	unsigned int playerLength;

	Q_strncpyz(status, Cvar_Serverinfo(), MAX_MSGLEN - 16);
	Q_strcat(status, "\n", sizeof(status));
	statusLength = strlen(status);

	for (i = 0; i < sv_maxclients->integer; i++) {
		cl = &svs.clients[i];
		if (cl->state == cs_connected || cl->state == cs_spawning || cl->state == cs_spawned) {
			Com_sprintf(player, sizeof(player), "%i %i \"%s\"\n", ge->ClientGetTeamNum(cl->player), cl->ping, cl->name);
			playerLength = strlen(player);
			if (statusLength + playerLength >= sizeof(status))
				break;			/* can't hold any more */
			Q_strcat(status, player, sizeof(status));
			statusLength += playerLength;
		}
#if 0
		else
			Com_Printf("%2i client->state: %i\n", i, cl->state);
#endif
	}

	return status;
}

/**
 * @brief Builds the string that is send as teaminfo reply
 * @sa SVC_Teaminfo
 * @sa CL_ParseTeamInfoMessage
 */
static char* SV_TeamInfoString (void)
{
	char player[1024];
	static char teaminfo[MAX_MSGLEN - 16];
	int i;
	client_t *cl;

	Q_strncpyz(teaminfo, Cvar_VariableString("sv_teamplay"), sizeof(teaminfo));
	Q_strcat(teaminfo, "\n", sizeof(teaminfo));
	Q_strcat(teaminfo, Cvar_VariableString("sv_maxteams"), sizeof(teaminfo));
	Q_strcat(teaminfo, "\n", sizeof(teaminfo));
	Q_strcat(teaminfo, Cvar_VariableString("maxplayers"), sizeof(teaminfo));
	Q_strcat(teaminfo, "\n", sizeof(teaminfo));
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state == cs_connected || cl->state == cs_spawned || cl->state == cs_spawning) {
			Com_DPrintf("SV_TeamInfoString: connected client: %i %s\n", i, cl->name);
			/* show players that already have a team with their teamnum */
			if (ge->ClientGetTeamNum(cl->player))
				Com_sprintf(player, sizeof(player), "%i\t\"%s\"\n", ge->ClientGetTeamNum(cl->player), cl->name);
			else if (ge->ClientGetTeamNumPref(cl->player))
				Com_sprintf(player, sizeof(player), "%i\t\"%s\"\n", ge->ClientGetTeamNumPref(cl->player), cl->name);
			else
				Com_sprintf(player, sizeof(player), "-\t\"%s\"\n", cl->name);
			Q_strcat(teaminfo, player, sizeof(teaminfo));
		} else {
			Com_DPrintf("SV_TeamInfoString: unconnected client: %i %s\n", i, cl->name);
		}
	}

	return teaminfo;
}

/**
 * @brief Responds with teaminfo such as free team num
 */
static void SVC_TeamInfo (void)
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "teaminfo\n%s", SV_TeamInfoString());
}

/**
 * @brief Responds with all the info that qplug or qspy can see
 * @sa SV_StatusString
 */
static void SVC_Status (void)
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "print\n%s", SV_StatusString());
#if 0
	Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);
	Com_Printf(SV_StatusString());
	Com_EndRedirect();
#endif
}

/**
 * @brief Sends an acknowledge
 */
static void SVC_Ack (void)
{
	Com_Printf("Ping acknowledge from %s\n", NET_AdrToString(net_from));
}

/**
 * @brief Responds with short info for broadcast scans
 * @note The second parameter should be the current protocol version number.
 * @note Only a short server description - the user can determine whether he is
 * interested in a full status
 * @sa CL_ParseStatusMessage
 */
static void SVC_Info (void)
{
	char string[64];
	int i, count;
	int version;
	char infostring[MAX_INFO_STRING];

	if (sv_maxclients->integer == 1) {
		Com_DPrintf("Ignore info string in singleplayer mode\n");
		return;	/* ignore in single player */
	}

	version = atoi(Cmd_Argv(1));

	if (version != PROTOCOL_VERSION)
		Com_sprintf(string, sizeof(string), "%s: wrong version (client: %i, host: %i)\n", hostname->string, version, PROTOCOL_VERSION);
	else {
		count = 0;
		for (i = 0; i < sv_maxclients->integer; i++)
			if (svs.clients[i].state >= cs_spawning)
				count++;

		infostring[0] = '\0';

		Info_SetValueForKey(infostring, "protocol", va("%i", PROTOCOL_VERSION));
		Info_SetValueForKey(infostring, "hostname", hostname->string);
		Info_SetValueForKey(infostring, "dedicated", dedicated->string);
		Info_SetValueForKey(infostring, "gametype", gametype->string);
		Info_SetValueForKey(infostring, "mapname", sv.name);
		Info_SetValueForKey(infostring, "clients", va("%i", count));
		Info_SetValueForKey(infostring, "maxclients", sv_maxclients->string);
		Info_SetValueForKey(infostring, "version", UFO_VERSION);
	}

	Netchan_OutOfBandPrint(NS_SERVER, net_from, "info\n%s", infostring);
}

/**
 * @brief Just responds with an acknowledgement
 */
static void SVC_Ping (void)
{
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "ack");
}


/**
 * @brief Returns a challenge number that can be used
 * in a subsequent client_connect command.
 * We do this to prevent denial of service attacks that
 * flood the server with invalid connection IPs.  With a
 * challenge, they must give a valid IP address.
 */
static void SVC_GetChallenge (void)
{
	int i;
	int oldest;
	int oldestTime;

	oldest = 0;
	oldestTime = 0x7fffffff;

	/* see if we already have a challenge for this ip */
	for (i = 0; i < MAX_CHALLENGES; i++) {
		if (NET_CompareBaseAdr(net_from, svs.challenges[i].adr))
			break;
		if (svs.challenges[i].time < oldestTime) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if (i == MAX_CHALLENGES) {
		/* overwrite the oldest */
		svs.challenges[oldest].challenge = rand() & 0x7fff;
		svs.challenges[oldest].adr = net_from;
		svs.challenges[oldest].time = curtime;
		i = oldest;
	}

	/* send it back */
	Netchan_OutOfBandPrint(NS_SERVER, net_from, "challenge %i", svs.challenges[i].challenge);
}

/**
 * @brief A connection request that did not come from the master
 */
static void SVC_DirectConnect (void)
{
	char userinfo[MAX_INFO_STRING];
	netadr_t adr;
	int i;
	client_t *cl, *newcl;
	client_t temp;
	player_t *player;
	int playernum;
	int version;
	int qport;
	int challenge;

	adr = net_from;

	Com_DPrintf("SVC_DirectConnect ()\n");

	version = atoi(Cmd_Argv(1));
	if (version != PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nServer is version %s.\n", UFO_VERSION);
		Com_DPrintf("    rejected connect from version %i\n", version);
		return;
	}

	qport = atoi(Cmd_Argv(2));

	challenge = atoi(Cmd_Argv(3));

	strncpy(userinfo, Cmd_Argv(4), sizeof(userinfo) - 1);
	userinfo[sizeof(userinfo) - 1] = 0;

	if (!strlen(userinfo)) {  /* catch empty userinfo */
		Com_Printf("Empty userinfo from %s\n", NET_AdrToString(adr));
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");
		return;
	}

	if (strchr(userinfo, '\xFF')) {  /* catch end of message in string exploit */
		Com_Printf("Illegal userinfo contained xFF from %s\n", NET_AdrToString(adr));
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");
		return;
	}

	if (strlen(Info_ValueForKey(userinfo, "ip"))) {  /* catch spoofed ips  */
		Com_Printf("Illegal userinfo contained ip from %s\n", NET_AdrToString(adr));
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");
		return;
	}

	/* force the IP key/value pair so the game can filter based on ip */
	Info_SetValueForKey(userinfo, "ip", NET_AdrToString(net_from));

	/* attractloop servers are ONLY for local clients */
	if (sv.attractloop) {
		if (!NET_IsLocalAddress(adr)) {
			Com_Printf("Remote connect in attract loop.  Ignored.\n");
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");
			return;
		}
	}

	/* see if the challenge is valid */
	if (!NET_IsLocalAddress(adr)) {
		for (i = 0; i < MAX_CHALLENGES; i++) {
			if (NET_CompareBaseAdr(net_from, svs.challenges[i].adr)) {
				if (challenge == svs.challenges[i].challenge) {
					svs.challenges[i].challenge = 0;
					break;		/* good */
				}
				Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nBad challenge.\n");
				return;
			}
		}
		if (i == MAX_CHALLENGES) {
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nNo challenge for address.\n");
			return;
		}
	}

	newcl = &temp;
	memset(newcl, 0, sizeof(client_t));

	/* if there is already a slot for this ip, reuse it */
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state == cs_free)
			continue;
		if (NET_CompareBaseAdr(adr, cl->netchan.remote_address)
			&& (cl->netchan.qport == qport || adr.port == cl->netchan.remote_address.port)) {
			if (!NET_IsLocalAddress(adr) && (svs.realtime - cl->lastconnect) < (sv_reconnect_limit->integer * 1000)) {
				Com_DPrintf("%s:reconnect rejected : too soon\n", NET_AdrToString(adr));
				return;
			}
			/* FIXME: Dont let the fighters respawn - reuse the already spawned fighters */
			Com_Printf("%s:reconnect\n", NET_AdrToString(adr));
			newcl = cl;
			goto gotnewcl;
		}
	}

	/* find a client slot */
	newcl = NULL;
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (cl->state == cs_free) {
			newcl = cl;
			break;
		}
	}
	if (!newcl) {
		Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nServer is full.\n");
		Com_DPrintf("Rejected a connection.\n");
		return;
	}

	/* @todo: Check if the teamnum preference has already reached maxsoldiers */
	/*       and reject connection if so */

	gotnewcl:
	/* build a new connection */
	/* accept the new client */
	/* this is the only place a client_t is ever initialized */
	*newcl = temp;
	sv_client = newcl;
	playernum = newcl - svs.clients;
	player = PLAYER_NUM(playernum);
	newcl->player = player;
	newcl->player->num = playernum;
	newcl->challenge = challenge;	/* save challenge for checksumming */

	/* get the game a chance to reject this connection or modify the userinfo */
	if (!(ge->ClientConnect(player, userinfo))) {
		if (*Info_ValueForKey(userinfo, "rejmsg"))
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\n%s\nConnection refused.\n", Info_ValueForKey(userinfo, "rejmsg"));
		else
			Netchan_OutOfBandPrint(NS_SERVER, adr, "print\nConnection refused.\n");
		Com_Printf("Game rejected a connection.\n");
		return;
	}

	/* new player */
	newcl->player->inuse = qtrue;

	/* parse some info from the info strings */
	strncpy(newcl->userinfo, userinfo, sizeof(newcl->userinfo) - 1);
	SV_UserinfoChanged(newcl);

	/* send the connect packet to the client */
	if (sv_downloadserver->string[0])
		Netchan_OutOfBandPrint(NS_SERVER, adr, "client_connect dlserver=%s", sv_downloadserver->string);
	else
		Netchan_OutOfBandPrint(NS_SERVER, adr, "client_connect");

	Netchan_Setup(NS_SERVER, &newcl->netchan, adr, qport);

	for (i = 0; i < RELIABLEBUFFERS; i++)
		SZ_Init(&newcl->reliable[i], newcl->reliable_buf + i * MAX_MSGLEN, MAX_MSGLEN);

	newcl->state = cs_connected;

	SZ_Init(&newcl->datagram, newcl->datagram_buf, sizeof(newcl->datagram_buf));
	newcl->datagram.allowoverflow = qtrue;
	newcl->lastmessage = svs.realtime;	/* don't timeout */
	newcl->lastconnect = svs.realtime;
}

/**
 * @brief Checks whether the remote connection is allowed (rcon_password must be
 * set on the server) - and verify the user given password with the cvar value.
 */
static qboolean Rcon_Validate (void)
{
	if (!strlen(rcon_password->string))
		return qfalse;

	if (strcmp(Cmd_Argv(1), rcon_password->string))
		return qfalse;

	return qtrue;
}

/**
 * @brief A client issued an rcon command. Shift down the remaining args. Redirect all printfs
 */
static void SVC_RemoteCommand (void)
{
	int i;
	qboolean valid;
	char remaining[1024];

	valid = Rcon_Validate();

	if (!valid)
		Com_Printf("Bad rcon from %s (%s):\n%s\n", SV_FindPlayer(net_from), NET_AdrToString(net_from), net_message.data + 4);
	else
		Com_Printf("Rcon from %s (%s):\n%s\n", SV_FindPlayer(net_from), NET_AdrToString(net_from), net_message.data + 4);

	Com_BeginRedirect(RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect);

	if (!valid)
		/* inform the client */
		Com_Printf("Bad rcon_password.\n");
	else {
		/* execute the rcon commands */
		remaining[0] = 0;

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
 * @brief A connectionless packet has four leading 0xff
 * characters to distinguish it from a game channel.
 * Clients that are in the game can still send
 * connectionless packets.
 */
static void SV_ConnectionlessPacket (void)
{
	const char *s, *c;

	MSG_BeginReading(&net_message);
	MSG_ReadLong(&net_message);	/* skip the -1 marker */

	s = MSG_ReadStringLine(&net_message);

	Cmd_TokenizeString(s, qfalse);

	c = Cmd_Argv(0);
	Com_DPrintf("Packet %s : %s\n", NET_AdrToString(net_from), c);

	if (!strcmp(c, "ping"))
		SVC_Ping();
	else if (!strcmp(c, "ack"))
		SVC_Ack();
	else if (!strcmp(c, "status"))
		SVC_Status();
	else if (!strcmp(c, "teaminfo"))
		SVC_TeamInfo();
	else if (!strcmp(c, "info"))
		SVC_Info();
	else if (!strcmp(c, "getchallenge"))
		SVC_GetChallenge();
	else if (!strcmp(c, "connect"))
		SVC_DirectConnect();
	else if (!strcmp(c, "rcon"))
		SVC_RemoteCommand();
	else
		Com_Printf("bad connectionless packet from %s:\n%s\n", NET_AdrToString(net_from), s);
}


/*============================================================================ */

/**
 * @brief Updates the cl->ping variables
 */
static void SV_CalcPings (void)
{
	int i, j;
	client_t *cl;
	int total, count;

	for (i = 0; i < sv_maxclients->integer; i++) {
		cl = &svs.clients[i];
		if (cl->state != cs_spawned)
			continue;

#if 0
		if (cl->lastframe > 0)
			cl->frame_latency[sv.framenum & (LATENCY_COUNTS - 1)] = sv.framenum - cl->lastframe + 1;
		else
			cl->frame_latency[sv.framenum & (LATENCY_COUNTS - 1)] = 0;
#endif

		total = 0;
		count = 0;
		for (j = 0; j < LATENCY_COUNTS; j++) {
			if (cl->frame_latency[j] > 0) {
				count++;
				total += cl->frame_latency[j];
			}
		}
		if (!count)
			cl->ping = 0;
		else
#if 0
			cl->ping = total * 100 / count - 100; /* div by zero! */
#else
			cl->ping = total / count;
#endif

		/* let the game dll know about the ping */
		cl->player->ping = cl->ping;
	}
}


/**
 * @brief Every few frames, gives all clients an allotment of milliseconds
 * for their command moves.  If they exceed it, assume cheating.
 */
static void SV_GiveMsec (void)
{
	int i;
	client_t *cl;

	if (sv.framenum & 15)
		return;

	for (i = 0; i < sv_maxclients->integer; i++) {
		cl = &svs.clients[i];
		if (cl->state == cs_free)
			continue;

		cl->commandMsec = 1800;	/* 1600 + some slop */
	}
}


/**
 * @brief
 */
static void SV_ReadPackets (void)
{
	int i, j;
	client_t *cl;
	int qport;

	while ((j = NET_GetPacket(NS_SERVER, &net_from, &net_message)) != 0) {
		/* icmp ignore cvar */
		if (j == -2)
			continue;

		/* check for connectionless packet (0xffffffff) first */
		if (*(int *) net_message.data == -1) {
			SV_ConnectionlessPacket();
			continue;
		}

		/* read the qport out of the message so we can fix up */
		/* stupid address translating routers */
		MSG_BeginReading(&net_message);
		MSG_ReadLong(&net_message);	/* sequence number */
		MSG_ReadLong(&net_message);	/* sequence number */
		qport = MSG_ReadShort(&net_message) & USHRT_MAX;

		/* check for packets from connected clients */
		for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
			if (cl->state <= cs_zombie)
				continue;
			/* r1: workaround for broken linux kernel */
			if (net_from.port == 0) {
				if (!NET_CompareBaseAdr(net_from, cl->netchan.remote_address))
					continue;
			} else if (!NET_CompareBaseAdr(net_from, cl->netchan.remote_address))
				continue;
#if 0
			if (cl->state == cs_spawned) {
				if (cl->lastmessage > svs.realtime - 1500)
					continue;
			} else {
				/* r1: longer delay if they are still loading */
				if (cl->lastmessage > svs.realtime - 10000)
					continue;
			}
#endif
			if (cl->netchan.qport != qport)
				continue;
			if (cl->netchan.remote_address.port != net_from.port) {
				Com_Printf("SV_ReadPackets: fixing up a translated port\n");
				cl->netchan.remote_address.port = net_from.port;
			}

			if (Netchan_Process(&cl->netchan, &net_message)) {	/* this is a valid, sequenced packet, so process it */
				if (cl->state != cs_zombie) {
					cl->lastmessage = svs.realtime;	/* don't timeout */
					SV_ExecuteClientMessage(cl);
				}
			}
			break;
		}

		if (i != sv_maxclients->integer)
			continue;
	}
}

/**
 * @brief If a packet has not been received from a client for timeout->value
 * seconds, drop the conneciton.  Server frames are used instead of
 * realtime to avoid dropping the local client while debugging.
 *
 * When a client is normally dropped, the client_t goes into a zombie state
 * for a few seconds to make sure any final reliable message gets resent
 * if necessary
 */
static void SV_CheckTimeouts (void)
{
	int i;
	client_t *cl;
	int droppoint;
	int zombiepoint;

	droppoint = svs.realtime - 1000 * timeout->integer;
	zombiepoint = svs.realtime - 1000 * zombietime->integer;

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		/* message times may be wrong across a changelevel */
		if (cl->lastmessage > svs.realtime)
			cl->lastmessage = svs.realtime;

		if (cl->state == cs_zombie && cl->lastmessage < zombiepoint) {
			cl->state = cs_free;	/* can now be reused */
			continue;
		}
		if ((cl->state == cs_connected || cl->state == cs_spawning || cl->state == cs_spawned)
			&& cl->lastmessage < droppoint) {
			SV_BroadcastPrintf(PRINT_HIGH, "%s timed out\n", cl->name);
			SV_DropClient(cl);
			cl->state = cs_free;	/* don't bother with zombie state */
		}
	}
}

/**
 * @brief Start the next map in the cycle
 */
void SV_NextMapcycle (void)
{
	int i;
	const char *map = NULL, *gameType = NULL;
	char *base;
	char assembly[MAX_QPATH];
	char expanded[MAX_QPATH];
	char cmd[MAX_QPATH];
	mapcycle_t *mapcycle;

	mapcycle = mapcycleList;
	if (*sv.name) {
		Com_Printf("current map: %s\n", sv.name);
		for (i = 0; i < mapcycleCount; i++) {
			/* random maps may have a theme - but that's not stored in sv.name
			* but in sv.assembly */
			if (*mapcycle->map == '+') {
				Q_strncpyz(expanded, mapcycle->map, sizeof(expanded));
				base = strstr(expanded, " ");
				if (base) {
					*base = '\0'; /* split the strings */
					Q_strncpyz(assembly, base+1, sizeof(assembly));
					/* get current position */
					if (!Q_strcmp(sv.name, expanded) && !Q_strcmp(sv.assembly, assembly)) {
						/* next map in cycle */
						if (mapcycle->next) {
							map = mapcycle->next->map;
							gameType = mapcycle->next->type;
							Com_DPrintf("SV_NextMapcycle: next one: '%s' (gametype: %s)\n", map, gameType);
						/* switch back to first list on cycle - if there is one */
						} else {
							map = mapcycleList->map;
							gameType = mapcycleList->type;
							Com_DPrintf("SV_NextMapcycle: first one: '%s' (gametype: %s)\n", map, gameType);
						}
						break;
					}
				} else {
					Com_Printf("ignore mapcycle entry for random map (%s) with"
						" no assembly given\n", mapcycle->map);
				}
			} else {
				/* get current position */
				if (!Q_strcmp(sv.name, mapcycle->map)) {
					/* next map in cycle */
					if (mapcycle->next) {
						map = mapcycle->next->map;
						gameType = mapcycle->next->type;
						Com_DPrintf("SV_NextMapcycle: next one: '%s' (gametype: %s)\n", map, gameType);
					/* switch back to first list on cycle - if there is one */
					} else {
						map = mapcycleList->map;
						gameType = mapcycleList->type;
						Com_DPrintf("SV_NextMapcycle: first one: '%s' (gametype: %s)\n", map, gameType);
					}
					Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

					/* check for bsp file */
					if (FS_CheckFile(expanded) < 0) {
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
			gameType = mapcycleList->type;
			if (*map != '+') {
				Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

				/* check for bsp file */
				if (FS_CheckFile(expanded) < 0) {
					Com_Printf("SV_NextMapcycle: Can't find '%s' - mapcycle error\n"
						"Use the 'maplist' command to get a list of valid maps\n", expanded);
					return;
				}
			}
		} else if (*sv.name) {
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
	if (gameType) {
		Com_sprintf(cmd, sizeof(cmd), "gametype %s;", gameType);
		Cbuf_ExecuteText(EXEC_NOW, cmd);
	}
	Com_sprintf(cmd, sizeof(cmd), "map %s", map);
	Cbuf_AddText(cmd);
	Cbuf_Execute();
}

/**
 * @brief Empty the mapcycle list
 * @sa SV_MapcycleAdd
 */
void SV_MapcycleClear (void)
{
	int i;
	mapcycle_t *mapcycle, *oldMapcycle;
	mapcycle = mapcycleList;
	for (i = 0; i < mapcycleCount; i++) {
		oldMapcycle = mapcycle;
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
void SV_MapcycleAdd (const char* mapName, const char* gameType)
{
	mapcycle_t *mapcycle;

	if (!mapcycleList) {
		mapcycleList = Mem_PoolAlloc(sizeof(mapcycle_t), sv_genericPool, 0);
		mapcycle = mapcycleList; /* first one */
	} else {
		/* go the the last entry */
		mapcycle = mapcycleList;
		while (mapcycle->next)
			mapcycle = mapcycle->next;
		mapcycle->next = Mem_PoolAlloc(sizeof(mapcycle_t), sv_genericPool, 0);
		mapcycle = mapcycle->next;
	}
	mapcycle->map = Mem_PoolStrDup(mapName, sv_genericPool, 0);
	mapcycle->type = Mem_PoolStrDup(gameType, sv_genericPool, 0);
	Com_DPrintf("mapcycle add: '%s' type '%s'\n", mapcycle->map, mapcycle->type);
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
	char *buffer = NULL;
	const char *token;
	const char *buf;
	char map[MAX_VAR], gameType[MAX_VAR];

	mapcycleCount = 0;
	mapcycleList = NULL;

	length = FS_LoadFile("mapcycle.txt", (void **) &buffer);
	if (length == -1)
		return;

	if (length != -1) {
		buf = buffer;
		do {
			token = COM_Parse(&buf);
			if (!*buf)
				break;
			Q_strncpyz(map, token, sizeof(map));
			token = COM_Parse(&buf);
			if (!*buf)
				break;
			Q_strncpyz(gameType, token, sizeof(gameType));
			SV_MapcycleAdd(map, gameType);
		} while (buf);

		Com_Printf("..added %i maps to the mapcycle\n", mapcycleCount);
	}
	FS_FreeFile(buffer);
}

/**
 * @brief Calls the G_RunFrame function from game api
 * @sa G_RunFrame
 * @sa SV_Frame
 */
static qboolean SV_RunGameFrame (void)
{
	qboolean gameEnd = qfalse;
	if (host_speeds->integer)
		time_before_game = Sys_Milliseconds();

	/* we always need to bump framenum, even if we */
	/* don't run the world, otherwise the delta */
	/* compression can get confused when a client */
	/* has the "current" frame */
	sv.framenum++;
	sv.time = sv.framenum * 100;

	gameEnd = ge->RunFrame();

	if (host_speeds->integer)
		time_after_game = Sys_Milliseconds();

	/* next map in the cycle */
	if (gameEnd && sv_maxclients->integer > 1)
		SV_NextMapcycle();

	return gameEnd;
}

/**
 * @brief
 * @sa Qcommon_Frame
 */
void SV_Frame (int msec)
{
	time_before_game = time_after_game = 0;

	/* if server is not active, do nothing */
	if (!svs.initialized)
		return;

	svs.realtime += msec;

	/* keep the random time dependent */
	rand();

	/* check timeouts */
	SV_CheckTimeouts();

	/* get packets from clients */
	SV_ReadPackets();

	/* move autonomous things around if enough time has passed */
	if (!sv_timedemo->integer && svs.realtime < sv.time) {
		/* never let the time get too far off */
		if (sv.time - svs.realtime > 100) {
			if (sv_showclamp->integer)
				Com_Printf("sv lowclamp\n");
			svs.realtime = sv.time - 100;
		}
		NET_Sleep(sv.time - svs.realtime);
		return;
	}

	/* update ping based on the last known frame from all clients */
	SV_CalcPings();

	/* give the clients some timeslices */
	SV_GiveMsec();

	/* let everything in the world think and move */
	SV_RunGameFrame();

	/* send messages back to the clients that had packets read this frame */
	SV_SendClientMessages();

	/* send a heartbeat to the master if needed */
	Master_Heartbeat();

	if (abandon && killserver) {
		abandon = qfalse;
		killserver = qfalse;
		SV_Shutdown("Server disconnected\n", qfalse);
	}
}

/*============================================================================ */

/**
 * @brief Send a message to the master every few minutes to
 * let it know we are alive, and log information
 */
#define	HEARTBEAT_SECONDS	300
void Master_Heartbeat (void)
{
	char *string;

	if (!dedicated || !dedicated->integer)
		return;		/* only dedicated servers send heartbeats */

	if (!public_server || !public_server->integer)
		return;		/* a private dedicated game */

	/* check for time wraparound */
	if (svs.last_heartbeat > svs.realtime)
		svs.last_heartbeat = svs.realtime;

	if (svs.realtime - svs.last_heartbeat < HEARTBEAT_SECONDS * 1000)
		return;					/* not time to send yet */

	svs.last_heartbeat = svs.realtime;

	/* send the same string that we would give for a status OOB command */
	string = SV_StatusString();

	/* send to master */
	if (master_adr.port) {
		Com_Printf("Sending heartbeat to %s (%s)\n", NET_AdrToString(master_adr), string);
		Netchan_OutOfBandPrint(NS_SERVER, master_adr, "heartbeat\n%s", string);
	}
}

/**
 * @brief Informs all masters that this server is going down
 */
void Master_Shutdown (void)
{
	/* pgm post3.19 change, cvar pointer not validated before dereferencing */
	if (!dedicated || !dedicated->integer)
		return;					/* only dedicated servers send heartbeats */

	/* pgm post3.19 change, cvar pointer not validated before dereferencing */
	if (!public_server || !public_server->integer)
		return;					/* a private dedicated game */

	/* send to master */
	if (master_adr.port) {
		Com_Printf("Sending shutdown to %s\n", NET_AdrToString(master_adr));
		Netchan_OutOfBandPrint(NS_SERVER, master_adr, "shutdown");
	}
}

/*============================================================================ */

/**
 * @brief Pull specific info from a newly changed userinfo string into a more C freindly form.
 */
void SV_UserinfoChanged (client_t * cl)
{
	const char *val;
	unsigned int i;

	/* call prog code to allow overrides */
	ge->ClientUserinfoChanged(cl->player, cl->userinfo);

	/* name for C code */
	strncpy(cl->name, Info_ValueForKey(cl->userinfo, "name"), sizeof(cl->name) - 1);
	/* mask off high bit */
	for (i = 0; i < sizeof(cl->name); i++)
		cl->name[i] &= 127;

	/* msg command */
	val = Info_ValueForKey(cl->userinfo, "msg");
	if (strlen(val))
		cl->messagelevel = atoi(val);
}

/**
 * @brief Only called at ufo.exe startup, not for each game
 */
void SV_Init (void)
{
	sv_gameSysPool = Mem_CreatePool("Server: Game system");
	sv_genericPool = Mem_CreatePool("Server: Generic");

	memset(&svs, 0, sizeof(svs));

	SV_InitOperatorCommands();

	rcon_password = Cvar_Get("rcon_password", "", 0, NULL);
/*	Cvar_Get("skill", "1", 0); */
	Cvar_Get("deathmatch", "0", CVAR_LATCH, NULL);
	Cvar_Get("timelimit", "0", CVAR_SERVERINFO, NULL);
	Cvar_Get("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH, NULL);
	Cvar_Get("protocol", va("%i", PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_NOSET, NULL);
	masterserver_ip = Cvar_Get("masterserver_ip", "195.136.48.62", CVAR_ARCHIVE, "IP address of UFO:AI masterserver (Sponsored by NineX)");
	masterserver_port = Cvar_Get("masterserver_port", "27900", CVAR_ARCHIVE, "Port of UFO:AI masterserver");
	/* this cvar will become a latched cvar when you start the server */
	sv_maxclients = Cvar_Get("maxclients", "1", CVAR_SERVERINFO, "Max. connected clients");
	hostname = Cvar_Get("hostname", _("noname"), CVAR_SERVERINFO | CVAR_ARCHIVE, "The name of the server that is displayed in the serverlist");
	timeout = Cvar_Get("timeout", "125", 0, "Timeout in seconds");
	zombietime = Cvar_Get("zombietime", "2", 0, "Timeout for zombies (in sec)");
	sv_showclamp = Cvar_Get("showclamp", "0", 0, NULL);
	sv_downloadserver = Cvar_Get("sv_downloadserver", "", CVAR_ARCHIVE, "URL to a location where clients can download game content over HTTP");
	sv_timedemo = Cvar_Get("timedemo", "0", 0, NULL);
	sv_enablemorale = Cvar_Get("sv_enablemorale", "1", CVAR_ARCHIVE | CVAR_SERVERINFO | CVAR_LATCH, "Enable morale changes in multiplayer");
	maxsoldiers = Cvar_Get("maxsoldiers", "4", CVAR_ARCHIVE | CVAR_SERVERINFO, "Max. amount of soldiers per team (see maxsoldiersperplayer and sv_teamplay)");
	maxsoldiersperplayer = Cvar_Get("maxsoldiersperplayer", "8", CVAR_ARCHIVE | CVAR_SERVERINFO, "Max. amount of soldiers each player can controll (see maxsoldiers and sv_teamplay)");

	public_server = Cvar_Get("public", "1", 0, "Should heartbeats be send to the masterserver");
	sv_reconnect_limit = Cvar_Get("sv_reconnect_limit", "3", CVAR_ARCHIVE, "Minimum seconds between connect messages");

	if (dedicated->integer)
		Cvar_Set("maxclients", "8");

	sv_maxclients->modified = qfalse;

	SZ_Init(&net_message, net_message_buffer, sizeof(net_message_buffer));

	SV_ParseMapcycle();
}

/**
 * @brief Used by SV_Shutdown to send a final message to all
 * connected clients before the server goes down.  The messages are sent immediately,
 * not just stuck on the outgoing message list, because the server is going
 * to totally exit after returning from this function.
 * @sa SV_Shutdown
*/
static void SV_FinalMessage (const char *message, qboolean reconnect)
{
	int i;
	client_t *cl;

	SZ_Clear(&net_message);
	MSG_WriteByte(&net_message, svc_print);
	MSG_WriteByte(&net_message, PRINT_HIGH);
	MSG_WriteString(&net_message, message);

	if (reconnect)
		MSG_WriteByte(&net_message, svc_reconnect);
	else
		MSG_WriteByte(&net_message, svc_disconnect);

	/* send it twice */
	/* stagger the packets to crutch operating system limited buffers */

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
		if (cl->state >= cs_connected)
			Netchan_Transmit(&cl->netchan, net_message.cursize, net_message.data);

	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++)
		if (cl->state >= cs_connected)
			Netchan_Transmit(&cl->netchan, net_message.cursize, net_message.data);
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
 */
void SV_Shutdown (const char *finalmsg, qboolean reconnect)
{
	if (svs.clients)
		SV_FinalMessage(finalmsg, reconnect);

	Master_Shutdown();
	SV_ShutdownGameProgs();

	/* free current level */
	if (sv.demofile.f)
		fclose(sv.demofile.f);
	memset(&sv, 0, sizeof(sv));
	Com_SetServerState(sv.state);

	/* free server static data */
	if (svs.clients)
		Mem_Free(svs.clients);
	if (svs.demofile.f)
		fclose(svs.demofile.f);
	memset(&svs, 0, sizeof(svs));

	/* maybe we shut down before we init - e.g. in case of an error */
	if (sv_maxclients)
		sv_maxclients->flags &= ~CVAR_LATCH;
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
 * @brief
 */
qboolean SV_RenderTrace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	trace_t	trace;

	if (sv.state == ss_dead || sv.state == ss_loading)
		return qfalse;

	trace = SV_Trace(start, mins, maxs, end, NULL, MASK_VISIBILILITY);

	if (trace.fraction != 1)
		return qfalse;

	return qtrue;
}
