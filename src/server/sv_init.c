/**
 * @file sv_init.c
 * @brief Server initialisation stuff.
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_init.c

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
#include "sv_rma.h"
#include "../shared/parse.h"

server_static_t svs;			/* persistant server info */
server_t sv;					/* local server */

static int SV_FindIndex (const char *name, int start, int max, qboolean create)
{
	int i;

	if (!name || !name[0])
		return 0;

	for (i = 1; i < max && sv.configstrings[start + i][0]; i++)
		if (!strcmp(sv.configstrings[start + i], name))
			return i;

	if (!create)
		return 0;

	if (i == max)
		Com_Error(ERR_DROP, "*Index: overflow '%s' start: %i, max: %i", name, start, max);

	Q_strncpyz(sv.configstrings[start + i], name, sizeof(sv.configstrings[i]));

	if (Com_ServerState() != ss_loading) {	/* send the update to everyone */
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_configstring);
		NET_WriteShort(msg, start + i);
		NET_WriteString(msg, name);
		SV_Multicast(~0, msg);
	}

	return i;
}

int SV_ModelIndex (const char *name)
{
	return SV_FindIndex(name, CS_MODELS, MAX_MODELS, qtrue);
}

/**
 * @brief Change the server to a new map, taking all connected clients along with it.
 * @sa SV_AssembleMap
 * @sa CM_LoadMap
 * @sa Com_SetServerState
 */
static void SV_SpawnServer (qboolean day, const char *server, const char *param)
{
	int i;
	unsigned checksum = 0;
	const char *buf = NULL;
	char * map = sv.configstrings[CS_TILES];
	char * pos = sv.configstrings[CS_POSITIONS];
	mapInfo_t *randomMap = NULL;

	assert(server);
	assert(server[0] != '\0');

	Com_DPrintf(DEBUG_SERVER, "SpawnServer: %s\n", server);

	/* save name for levels that don't set message */
	Q_strncpyz(sv.configstrings[CS_NAME], server, MAX_TOKEN_CHARS);
	Com_sprintf(sv.configstrings[CS_LIGHTMAP], MAX_TOKEN_CHARS, "%i", day);
	sv.day = day;

	Q_strncpyz(sv.name, server, sizeof(sv.name));
	if (param)
		Q_strncpyz(sv.assembly, param, sizeof(sv.assembly));
	else
		*sv.assembly = '\0';

	/* leave slots at start for clients only */
	for (i = 0; i < sv_maxclients->integer; i++) {
		/* needs to reconnect */
		if (svs.clients[i].state > cs_connected)
			SV_SetClientState(&svs.clients[i], cs_connected);
	}

	/* assemble and load the map */
	if (server[0] == '+') {
		randomMap = SV_AssembleMap(server + 1, param, map, pos);
		if (!randomMap) {
			Com_Printf("Could not load assembly for map '%s'\n", server);
			return;
		}
	} else {
		Q_strncpyz(sv.configstrings[CS_TILES], server, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		if (param)
			Q_strncpyz(sv.configstrings[CS_POSITIONS], param, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		else
			sv.configstrings[CS_POSITIONS][0] = 0;
	}

	CM_LoadMap(map, day, pos, &checksum);

	Com_Printf("checksum for the map '%s': %u\n", server, checksum);
	Com_sprintf(sv.configstrings[CS_MAPCHECKSUM], sizeof(sv.configstrings[CS_MAPCHECKSUM]), "%i", checksum);

	checksum = 0;
	while ((buf = FS_GetFileData("ufos/*.ufo")) != NULL)
		checksum += LittleLong(Com_BlockChecksum(buf, strlen(buf)));
	FS_GetFileData(NULL);
	Com_Printf("ufo script checksum %u\n", checksum);
	Com_sprintf(sv.configstrings[CS_UFOCHECKSUM], sizeof(sv.configstrings[CS_UFOCHECKSUM]), "%i", checksum);
	Com_sprintf(sv.configstrings[CS_OBJECTAMOUNT], sizeof(sv.configstrings[CS_OBJECTAMOUNT]), "%i", csi.numODs);

	Com_sprintf(sv.configstrings[CS_VERSION], sizeof(sv.configstrings[CS_VERSION]), "%s", UFO_VERSION);

	Com_sprintf(sv.configstrings[CS_MAPTITLE], sizeof(sv.configstrings[CS_MAPTITLE]), "%s", SV_GetMapTitle(randomMap, server));
	if (!strncmp(sv.configstrings[CS_MAPTITLE], "b/", 2)) {
		/* For base attack, cl.configstrings[CS_MAPTITLE] contains too many chars */
		Com_sprintf(sv.configstrings[CS_MAPTITLE], sizeof(sv.configstrings[CS_MAPTITLE]), "Base attack");
	}

	/* clear random-map assembly data */
	Mem_Free(randomMap);
	randomMap = NULL;

	/* clear physics interaction links */
	SV_ClearWorld();

	/* fix this! */
	for (i = 1; i <= CM_NumInlineModels(); i++)
		sv.models[i] = CM_InlineModel(va("*%i", i));

	/* precache and static commands can be issued during map initialization */
	Com_SetServerState(ss_loading);

	/* load and spawn all other entities */
	ge->SpawnEntities(sv.name, CM_EntityString());

	/* all precaches are complete */
	Com_SetServerState(ss_game);

	/* set serverinfo variable */
	sv_mapname = Cvar_FullSet("sv_mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	Com_Printf("-------------------------------------\n");
}

/**
 * @sa CL_ServerListDiscoveryCallback
 */
static void SV_DiscoveryCallback (struct datagram_socket *s, const char *buf, int len, struct sockaddr *from)
{
	const char match[] = "discover";
	if (len == sizeof(match) && memcmp(buf, match, len) == 0) {
		const char msg[] = "discovered";
		NET_DatagramSend(s, msg, sizeof(msg), from);
	}
}

/**
 * @brief A brand new game has been started
 */
static void SV_InitGame (void)
{
	if (svs.initialized)
		/* cause any connected clients to reconnect */
		SV_Shutdown("Server restarted.", qtrue);

	/* allow next change after map change or restart */
	sv_maxclients->flags |= CVAR_LATCH;

	/* get any latched variable changes (sv_maxclients, etc) */
	Cvar_UpdateLatchedVars();

	svs.spawncount = rand();
	svs.clients = Mem_PoolAlloc(sizeof(client_t) * sv_maxclients->integer, sv_genericPool, 0);

	/* init network stuff */
	if (sv_maxclients->integer > 1) {
		svs.initialized = SV_Start(NULL, port->string, &SV_ReadPacket);
		svs.netDatagramSocket = NET_DatagramSocketNew(NULL, Cvar_Get("port", DOUBLEQUOTE(PORT_SERVER), CVAR_NOSET, NULL)->string, &SV_DiscoveryCallback);
	} else
		svs.initialized = SV_Start(NULL, NULL, &SV_ReadPacket);

	SV_Heartbeat_f();

	/* init game */
	SV_InitGameProgs();

	if (sv_maxclients->integer != 1 && (sv_dedicated->integer || sv_public->integer))
		SV_SetMaster_f();
}


/**
 * @brief Loads the map
 * @note the full syntax is:
 * @code map [day|night] [+]<map> [<assembly>] @endcode
 * @sa SV_SpawnServer
 * @sa SV_Map_f
 * @param[in] day Use the day version of the map (only lightmap)
 */
void SV_Map (qboolean day, const char *levelstring, const char *assembly)
{
	assert(levelstring[0] != '\0');

	/* any partially connected client will be restarted */
	Com_SetServerState(ss_dead);

	/* the game is just starting */
	SV_InitGame();

	if (!svs.initialized) {
		Com_Printf("Could not spawn the map\n");
		return;
	}

	SCR_BeginLoadingPlaque();
	SV_SpawnServer(day, levelstring, assembly);
	Cbuf_CopyToDefer();
}
