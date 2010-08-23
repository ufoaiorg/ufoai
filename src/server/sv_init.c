/**
 * @file sv_init.c
 * @brief Server initialisation stuff.
 */

/*
All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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

serverInstanceStatic_t svs;			/* persistant server info */
serverInstanceGame_t sv;					/* local server */

/**
 * @brief Get the map title for a given map
 * @note the title string must be translated client side
 * @return Never NULL - mapname or maptitle (if defined in assembly)
 */
static const char* SV_GetMapTitle (const mapInfo_t *map, const char* const mapname)
{
	assert(mapname);

	if (mapname[0] == '+') {
		const mAssembly_t *mAsm = &map->mAssembly[map->mAsm];
		if (mAsm && mAsm->title[0]) {
			/* return the assembly title itself - must be translated client side */
			if (mAsm->title[0] == '_')
				return mAsm->title + 1;
			else {
				Com_Printf("The assembly title '%s' is not marked as translatable\n", mAsm->title);
				return mAsm->title;
			}
		}
	}
	return mapname;
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
	char * map = SV_GetConfigString(CS_TILES);
	char * pos = SV_GetConfigString(CS_POSITIONS);
	mapInfo_t *randomMap = NULL;

	assert(server[0] != '\0');

	Com_DPrintf(DEBUG_SERVER, "SpawnServer: %s\n", server);

	/* save name for levels that don't set message */
	SV_SetConfigString(CS_NAME, server);
	SV_SetConfigStringInteger(CS_LIGHTMAP, day);

	Q_strncpyz(sv.name, server, sizeof(sv.name));

	/* set serverinfo variable */
	sv_mapname = Cvar_FullSet("sv_mapname", sv.name, CVAR_SERVERINFO | CVAR_NOSET);

	/* notify the client in case of a listening server */
	SCR_BeginLoadingPlaque();

	if (param)
		Q_strncpyz(sv.assembly, param, sizeof(sv.assembly));
	else
		sv.assembly[0] = '\0';

	/* leave slots at start for clients only */
	for (i = 0; i < sv_maxclients->integer; i++) {
		/* needs to reconnect */
		if (svs.clients[i].state >= cs_spawning)
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
			SV_SetConfigString(CS_POSITIONS, "");
	}

	CM_LoadMap(map, day, pos, &sv.mapData, &sv.mapTiles);

	Com_Printf("checksum for the map '%s': %u\n", server, sv.mapData.mapChecksum);
	SV_SetConfigStringInteger(CS_MAPCHECKSUM, sv.mapData.mapChecksum);

	checksum = 0;
	while ((buf = FS_GetFileData("ufos/*.ufo")) != NULL)
		checksum += LittleLong(Com_BlockChecksum(buf, strlen(buf)));
	FS_GetFileData(NULL);
	Com_Printf("ufo script checksum %u\n", checksum);
	SV_SetConfigStringInteger(CS_UFOCHECKSUM, checksum);
	SV_SetConfigStringInteger(CS_OBJECTAMOUNT, csi.numODs);
	SV_SetConfigString(CS_VERSION, UFO_VERSION);
	SV_SetConfigString(CS_MAPTITLE, SV_GetMapTitle(randomMap, server));
	if (!strncmp(SV_GetConfigString(CS_MAPTITLE), "b/", 2)) {
		/* For base attack, cl.configstrings[CS_MAPTITLE] contains too many chars */
		SV_SetConfigString(CS_MAPTITLE, "Base attack");
		SV_SetConfigString(CS_NAME, ".baseattack");
	}

	/* clear random-map assembly data */
	Mem_Free(randomMap);
	randomMap = NULL;

	/* clear physics interaction links */
	SV_ClearWorld();

	/* fix this! */
	for (i = 1; i <= sv.mapData.numInline; i++)
		sv.models[i] = CM_InlineModel(&sv.mapTiles, va("*%i", i));

	/* precache and static commands can be issued during map initialization */
	Com_SetServerState(ss_loading);

	SDL_mutexP(svs.serverMutex);
	/* load and spawn all other entities */
	svs.ge->SpawnEntities(sv.name, SV_GetConfigStringInteger(CS_LIGHTMAP), sv.mapData.mapEntityString);
	SDL_mutexV(svs.serverMutex);

	/* all precaches are complete */
	Com_SetServerState(ss_game);

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
	/* allow next change after map change or restart */
	sv_maxclients->flags |= CVAR_LATCH;

	/* get any latched variable changes (sv_maxclients, etc) */
	Cvar_UpdateLatchedVars();

	svs.clients = Mem_PoolAlloc(sizeof(client_t) * sv_maxclients->integer, sv_genericPool, 0);
	svs.serverMutex = SDL_CreateMutex();

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
	/* any partially connected client will be restarted */
	Com_SetServerState(ss_restart);

	/* the game is just starting */
	SV_InitGame();

	if (!svs.initialized) {
		Com_Printf("Could not spawn the map\n");
		return;
	}

	SV_SpawnServer(day, levelstring, assembly);
	Cbuf_CopyToDefer();
}
