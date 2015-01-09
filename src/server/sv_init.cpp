/**
 * @file
 * @brief Server initialisation stuff.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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
#include "../shared/scopedmutex.h"

serverInstanceStatic_t svs;			/* persistant server info */
serverInstanceGame_t* sv;			/* local server */

/**
 * @brief Get the map title for a given map
 * @note the title string must be translated client side
 * @return Never nullptr - mapname or maptitle (if defined in assembly)
 */
static const char* SV_GetMapTitle (const char* asmTitle, const char* const mapname)
{
	assert(mapname);

	if (mapname[0] == '+') {
		if (asmTitle && asmTitle[0]) {
			/* return the assembly title itself - must be translated client side */
			if (asmTitle[0] == '_')
				return asmTitle + 1;
			else {
				Com_Printf("The assembly title '%s' is not marked as translatable\n", asmTitle);
				return asmTitle;
			}
		}
	}
	return mapname;
}

/**
 * @sa CL_ServerListDiscoveryCallback
 */
static void SV_DiscoveryCallback (struct datagram_socket* s, const char* buf, int len, struct sockaddr* from)
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

	if (svs.serverMutex)
		Sys_Error("There is still a server running");

	svs.clients     = Mem_PoolAllocTypeN(client_t, sv_maxclients->integer, sv_genericPool);
	svs.serverMutex = SDL_CreateMutex();

	/* init network stuff */
	if (sv_maxclients->integer > 1) {
		svs.initialized = SV_Start(nullptr, port->string, &SV_ReadPacket);
		svs.netDatagramSocket = NET_DatagramSocketNew(nullptr, port->string, &SV_DiscoveryCallback);
	} else {
		svs.initialized = SV_Start(nullptr, nullptr, &SV_ReadPacket);
	}

	SV_Heartbeat_f();

	/* init game */
	SV_InitGameProgs();

	if (sv_maxclients->integer > 1 && (sv_dedicated->integer || sv_public->integer))
		SV_SetMaster_f();
}

/**
 * @brief Change the server to a new map, taking all connected clients along with it.
 * @note the full syntax is: @code map [day|night] [+]<map> [<assembly>] @endcode
 * @sa CM_LoadMap
 * @sa Com_SetServerState
 */
void SV_Map (bool day, const char* levelstring, const char* assembly, bool verbose)
{
	int i;
	unsigned checksum = 0;
	char* tileString = SV_GetConfigString(CS_TILES);
	char* posString = SV_GetConfigString(CS_POSITIONS);
	char* entityString = SV_GetConfigString(CS_ENTITYSTRING);
	client_t* cl;

	/* any partially connected client will be restarted */
	Com_SetServerState(ss_restart);

	/* the game is just starting */
	SV_InitGame();

	if (!svs.initialized) {
		Com_Printf("Could not spawn the server\n");
		return;
	}

	assert(Q_strvalid(levelstring));

	Com_DPrintf(DEBUG_SERVER, "SpawnServer: %s\n", levelstring);

	/* save name for levels that don't set message */
	SV_SetConfigString(CS_NAME, levelstring);
	SV_SetConfigString(CS_LIGHTMAP, day);
	SV_SetConfigString(CS_MAPZONE, Cvar_GetString("sv_mapzone"));

	Q_strncpyz(sv->name, levelstring, sizeof(sv->name));

	/* set serverinfo variable */
	sv_mapname = Cvar_FullSet("sv_mapname", sv->name, CVAR_SERVERINFO | CVAR_NOSET);

	/* notify the client in case of a listening server */
	SCR_BeginLoadingPlaque();

	if (assembly)
		Q_strncpyz(sv->assembly, assembly, sizeof(sv->assembly));
	else
		sv->assembly[0] = '\0';

	/* leave slots at start for clients only */
	cl = nullptr;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		/* needs to reconnect */
		if (cl->state >= cs_spawning)
			SV_SetClientState(cl, cs_connected);
	}

	/* assemble and load the map */
	char asmTitle[MAX_VAR];
	int numPlaced = 0;
	if (levelstring[0] == '+') {
		numPlaced = SV_AssembleMapAndTitle(levelstring + 1, assembly, tileString, posString, entityString, 0, verbose, asmTitle);
		if (!numPlaced) {
			Com_Printf("Could not load assembly for map '%s'\n", levelstring);
			return;
		}
	} else {
		SV_SetConfigString(CS_TILES, levelstring);
		SV_SetConfigString(CS_POSITIONS, assembly ? assembly : "");
		SV_SetConfigString(CS_ENTITYSTRING, "");
	}

	CM_LoadMap(tileString, day, posString, entityString, &sv->mapData, &sv->mapTiles);

	Com_Printf("checksum for the map '%s': %u\n", levelstring, sv->mapData.mapChecksum);
	SV_SetConfigString(CS_MAPCHECKSUM, sv->mapData.mapChecksum);

	checksum = Com_GetScriptChecksum();

	Com_Printf("ufo script checksum %u\n", checksum);
	SV_SetConfigString(CS_UFOCHECKSUM, checksum);
	SV_SetConfigString(CS_OBJECTAMOUNT, csi.numODs);
	SV_SetConfigString(CS_VERSION, UFO_VERSION);
	SV_SetConfigString(CS_MAPTITLE, SV_GetMapTitle(asmTitle, levelstring));
	if (Q_strstart(SV_GetConfigString(CS_MAPTITLE), "b/")) {
		/* For base attack, CS_MAPTITLE contains too many chars */
		SV_SetConfigString(CS_MAPTITLE, "Base attack");
		SV_SetConfigString(CS_NAME, ".baseattack");
	}

	/* clear physics interaction links */
	SV_ClearWorld();

	/* fix this! */
	for (i = 1; i <= sv->mapData.numInline; i++)
		sv->models[i] = CM_InlineModel(&sv->mapTiles, va("*%i", i));

	/* precache and static commands can be issued during map initialization */
	Com_SetServerState(ss_loading);

	/* load and spawn all other entities */
	{
		const ScopedMutex scopedMutex(svs.serverMutex);
		svs.ge->SpawnEntities(sv->name, SV_GetConfigStringInteger(CS_LIGHTMAP), sv->mapData.mapEntityString);
	}

	/* all precaches are complete */
	Com_SetServerState(ss_game);

	Com_Printf("-------------------------------------\n");

	Cbuf_CopyToDefer();
}
