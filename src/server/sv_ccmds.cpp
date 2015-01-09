/**
 * @file
 * @brief Console-only server commands.
 *
 * These commands can only be entered from stdin or by a remote operator datagram.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

Original file from Quake 2 v3.21: quake2-2.31/server/sv_ccmds.c
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
#include "../common/http.h"
#include "../shared/scopedmutex.h"

void SV_Heartbeat_f (void)
{
	/* heartbeats will always be sent to the ufo master */
	svs.lastHeartbeat = -9999999;	/* send immediately */
}

/**
 * @brief Add the server to the master server list so that others can see the server in the server list
 * @sa SV_InitGame
 */
void SV_SetMaster_f (void)
{
	if (sv_maxclients->integer == 1)
		return;

	/* make sure the server is listed public */
	Cvar_Set("public", "1");

	Com_Printf("Master server at [%s] - sending a ping\n", masterserver_url->string);
	HTTP_GetURL(va("%s/ufo/masterserver.php?ping&port=%s", masterserver_url->string, port->string), nullptr);

	if (!sv_dedicated->integer)
		return;

	/* only dedicated servers are sending heartbeats */
	SV_Heartbeat_f();
}

/**
 * @brief searches a player by id or name
 * @param[in] s Either the numeric id of the player, or the player name
 * @return the client structure
 */
static client_t* SV_GetPlayerClientStructure (const char* s)
{
	/* numeric values are just slot numbers */
	if (s[0] >= '0' && s[0] <= '9') {
		int idnum = atoi(Cmd_Argv(1));
		client_t* cl = nullptr;
		/* check for a name match */
		while ((cl = SV_GetNextClient(cl)) != nullptr && idnum > 0)
			idnum--;
		if (cl->state == cs_free) {
			Com_Printf("Client %i is not active\n", idnum);
			return nullptr;
		}
		return cl;
	} else {
		client_t* cl = nullptr;
		/* check for a name match */
		while ((cl = SV_GetNextClient(cl)) != nullptr) {
			if (cl->state == cs_free)
				continue;
			if (Q_streq(cl->name, s)) {
				return cl;
			}
		}
	}

	Com_Printf("Userid %s is not on the server\n", s);
	return nullptr;
}

/**
 * @brief Checks whether a map exists
 */
bool SV_CheckMap (const char* map, const char* assembly)
{
	char expanded[MAX_QPATH];

	/* base attacks starts with . and random maps with + */
	if (map[0] == '+') {
		Com_sprintf(expanded, sizeof(expanded), "maps/%s.ump", map + 1);

		/* check for ump file */
		if (FS_CheckFile("%s", expanded) < 0) {
			Com_Printf("Can't find %s\n", expanded);
			return false;
		}
	} else if (!assembly) {
		Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

		/* check for bsp file */
		if (FS_CheckFile("%s", expanded) < 0) {
			Com_Printf("Can't find %s\n", expanded);
			return false;
		}
	}
	return true;
}

/**
 * @brief Goes directly to a given map
 * @sa SV_InitGame
 * @sa SV_SpawnServer
 */
static void SV_Map_f (void)
{
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <day|night> <mapname> [<assembly>]\n", Cmd_Argv(0));
		Com_Printf("Use 'maplist' to get a list of all installed maps\n");
		return;
	}

	if (Q_streq(Cmd_Argv(0), "devmap")) {
		Com_Printf("deactivate ai - make sure to reset sv_ai after maptesting\n");
		Cvar_SetValue("sv_ai", 0);
		Cvar_SetValue("sv_cheats", 1);
		Cvar_SetValue("sv_send_edicts", 1);
		Cvar_SetValue("g_notu", 1);
		Cvar_SetValue("g_nospawn", 1);
	} else {
		Cvar_SetValue("sv_ai", 1);
		Cvar_SetValue("sv_send_edicts", 0);
		Cvar_SetValue("g_notu", 0);
		Cvar_SetValue("g_nospawn", 0);
	}

	bool day;
	if (Q_streq(Cmd_Argv(1), "day")) {
		day = true;
	} else if (Q_streq(Cmd_Argv(1), "night")) {
		day = false;
	} else {
		Com_Printf("Invalid lightmap parameter - use day or night\n");
		return;
	}

	/* we copy them to buffers because the command pointers might be invalid soon */
	/* the buffers must be as big as the CS_TILES/CS_POSITONS config strings, because the base assembly
	 * gives the full resolved rma assembly string */
	char bufMap[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
	const char* assembly = nullptr;
	char bufAssembly[MAX_TOKEN_CHARS * MAX_TILESTRINGS] = "";
	Q_strncpyz(bufMap, Cmd_Argv(2), sizeof(bufMap));
	/* assembled maps uses position strings */
	if (Cmd_Argc() == 4) {
		Q_strncpyz(bufAssembly, Cmd_Argv(3), sizeof(bufAssembly));
		assembly = bufAssembly;
	}

	/* check to make sure the level exists */
	if (!SV_CheckMap(bufMap, assembly))
		return;

	/* start up the next map */
	SV_Map(day, bufMap, assembly);
}

/**
 * @brief Kick a user off of the server
 */
static void SV_Kick_f (void)
{
	client_t* cl;

	if (!svs.initialized) {
		Com_Printf("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	cl = SV_GetPlayerClientStructure(Cmd_Argv(1));
	if (cl == nullptr)
		return;

	SV_BroadcastPrintf(PRINT_CONSOLE, "%s was kicked\n", cl->name);
	/* print directly, because the dropped client won't get the
	 * SV_BroadcastPrintf message */
	SV_DropClient(cl, "You were kicked from the game\n");
}

/**
 * @brief Forces a game start even if not all players are ready yet
 * @sa SV_CheckGameStart
 */
static void SV_StartGame_f (void)
{
	client_t* cl = nullptr;
	int cnt = 0;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		if (cl->state != cs_free) {
			cl->player->setReady(true);
			cnt++;
		}
	}
	Cvar_ForceSet("sv_maxclients", va("%i", cnt));
}

/**
 * @brief Prints some server info to the game console - like connected players
 * and current running map
 */
static void SV_Status_f (void)
{
	int i;
	client_t* cl;
	const char* s;
	char buf[256];

	if (!svs.clients) {
		Com_Printf("No server running.\n");
		return;
	}
	Com_Printf("map              : %s (%s)\n", sv->name, (SV_GetConfigStringInteger(CS_LIGHTMAP) ? "day" : "night"));
	Com_Printf("active team      : %i\n", svs.ge->ClientGetActiveTeam());

	Com_Printf("num status  name            timeout        ready  address              \n");
	Com_Printf("--- ------- --------------- -------------- ----- ---------------------\n");

	cl = nullptr;
	i = 0;
	while ((cl = SV_GetNextClient(cl)) != nullptr) {
		char        state_buf[16];
		char const* state;

		i++;

		if (cl->state == cs_free)
			continue;

		switch (cl->state) {
		case cs_connected:
			state = "CONNECT"; break;
		case cs_spawning:
			state = "SPAWNIN"; break;
		case cs_began:
			state = "BEGAN  "; break;
		case cs_spawned:
			state = "SPAWNED"; break;

		default:
			sprintf(state_buf, "%7i", cl->state);
			state = state_buf;
			break;
		}

		s = NET_StreamPeerToName(cl->stream, buf, sizeof(buf), false);
		Com_Printf("%3i %s %-15s %14i %-5s %-21s\n", i, state, cl->name, cl->lastmessage,
				cl->player->isReady() ? "true" : "false", s);
	}
	Com_Printf("\n");
}

#ifdef DEDICATED_ONLY
/**
 * @sa SV_BroadcastPrintf
 */
static void SV_ConSay_f (void)
{
	const char* p;
	char text[1024];

	if (Cmd_Argc() < 2)
		return;

	if (!Com_ServerState()) {
		Com_Printf("no server is running\n");
		return;
	}

	Q_strncpyz(text, "serverconsole: ", sizeof(text));
	p = Cmd_Args();

	if (*p == '"')
		p++;

	Q_strcat(text, sizeof(text), "%s", p);
	if (text[strlen(text)] == '"')
		text[strlen(text)] = 0;
	SV_BroadcastPrintf(PRINT_CHAT, "%s\n", text);
}
#endif

/**
 * @brief Examine or change the serverinfo string
 */
static void SV_Serverinfo_f (void)
{
	Com_Printf("Server info settings:\n");
	char info[MAX_INFO_STRING];
	Info_Print(Cvar_Serverinfo(info, sizeof(info)));
}


/**
 * @brief Examine all a users info strings
 * @sa CL_UserInfo_f
 */
static void SV_UserInfo_f (void)
{
	client_t* cl;

	if (!svs.initialized) {
		Com_Printf("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <userid>\n", Cmd_Argv(0));
		return;
	}

	cl = SV_GetPlayerClientStructure(Cmd_Argv(1));
	if (cl == nullptr)
		return;

	Com_Printf("userinfo\n");
	Com_Printf("--------\n");
	Info_Print(cl->userinfo);
}

/**
 * @brief Kick everyone off, possibly in preparation for a new game
 */
static void SV_KillServer_f (void)
{
	if (!svs.initialized)
		return;
	SV_Shutdown("Server was killed.", false);
}

/**
 * @brief Let the game dll handle a command
 */
static void SV_ServerCommand_f (void)
{
	if (!svs.ge) {
		Com_Printf("No game loaded.\n");
		return;
	}

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <command> <parameter>\n", Cmd_Argv(0));
		return;
	}

	Com_DPrintf(DEBUG_SERVER, "Execute game command '%s'\n", Cmd_Args());

	const ScopedMutex scopedMutex(svs.serverMutex);
	svs.ge->ServerCommand();
}

/*=========================================================== */

/**
 * @brief Autocomplete function for the map command
 * @param[in] partial The list of params entered, starting at the first non-blank char.
 * @param[out] match The found entry of the list we are searching, in case of more than one entry their common suffix is returned.
 * @sa Cmd_AddParamCompleteFunction
 */
static int SV_CompleteMapCommand (const char* partial, const char** match)
{
	const char* dayNightStr = nullptr;
	static char dayNightMatch[7];

	if (partial[0])
		dayNightStr = strstr(partial, " ");
	if (!dayNightStr) {
		if (partial[0] == 'd') {
			Q_strncpyz(dayNightMatch, "day ", sizeof(dayNightMatch));
			*match = dayNightMatch;
			return 1;
		} else if (partial[0] == 'n') {
			Q_strncpyz(dayNightMatch, "night ", sizeof(dayNightMatch));
			*match = dayNightMatch;
			return 1;
		}
		/* neither day or night, delete previous content and display options */
		Com_Printf("day\nnight\n");
		dayNightMatch[0] = '\0';
		*match = dayNightMatch;
		return 2;
	} else {
		if (!Q_strncasecmp(partial, "day ", 4) || !Q_strncasecmp(partial, "night ", 6)) {
			/* dayNightStr is correct, use it */
			partial = dayNightStr + 1;
		} else {
			/* neither day or night, delete previous content and display options */
			Com_Printf("day\nnight\n");
			dayNightMatch[0] = '\0';
			*match = dayNightMatch;
			return 2;
		}
	}

	FS_GetMaps(false);

	int n = 0;
	const char* const* endOfMaps = fs_maps + fs_numInstalledMaps + 1;	/* bug: fs_numInstalledMaps is off by one */
	for (const char* const* mapName = fs_maps; mapName < endOfMaps; mapName++) {
		if (Cmd_GenericCompleteFunction(*mapName, partial, match)) {
			Com_Printf("%s\n", *mapName);
			++n;
		}
	}
	return n;
}

/**
 * @brief List all valid maps
 * @sa FS_GetMaps
 */
static void SV_ListMaps_f (void)
{
	FS_GetMaps(true);

	for (int i = 0; i <= fs_numInstalledMaps; i++)
		Com_Printf("%s\n", fs_maps[i]);
	Com_Printf("-----\n %i installed maps\n+name means random map assembly\n", fs_numInstalledMaps + 1);
}

/**
 * @brief List for SV_CompleteServerCommand
 * @sa ServerCommand
 */
struct serverCommand_t {
	char const* name;
	char const* description;
};

static serverCommand_t const serverCommandList[] = {
	{ "startgame",          "Force the gamestart - useful for multiplayer games" },
	{ "addip",              "The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with 'addip 192.246.40'" },
	{ "removeip",           "Removeip will only remove an address specified exactly the same way. You cannot addip a subnet, then removeip a single host" },
	{ "listip",             "Prints the current list of filters" },
	{ "writeip",            "Dumps ips to listip.cfg so it can be executed at a later date" },
	{ "ai_add",             "Used to add ai opponents to a game - but no civilians" },
	{ "win",                "Call the end game function with the given team" },
#ifdef DEBUG
	{ "debug_showall",      "Debug function: Reveal all items to all sides" },
	{ "debug_actorinvlist", "Debug function to show the whole inventory of all connected clients on the server" },
#endif
	{ 0, 0 }
};

/**
 * @brief Autocomplete function for server commands
 * @sa ServerCommand
 */
static int SV_CompleteServerCommand (const char* partial, const char** match)
{
	int n = 0;
	for (serverCommand_t const* i = serverCommandList; i->name; ++i) {
		if (Cmd_GenericCompleteFunction(i->name, partial, match)) {
			Com_Printf("[cmd] %s\n", i->name);
			if (*i->description)
				Com_Printf(S_COLOR_GREEN "      %s\n", i->description);
			++n;
		}
	}
	return n;
}

/**
 * @sa CL_ShowConfigstrings_f
 */
static void SV_PrintConfigStrings_f (void)
{
	for (int i = 0; i < MAX_CONFIGSTRINGS; i++) {
		const char* configString;
		/* CS_TILES and CS_POSITIONS can stretch over multiple configstrings,
		 * so don't send the middle parts again. */
		if (i > CS_TILES && i < CS_POSITIONS)
			continue;
		if (i > CS_POSITIONS && i < CS_MODELS)
			continue;
		configString = SV_GetConfigString(i);
		if (configString[0] == '\0')
			continue;
		Com_Printf("configstring[%3i]: %s\n", i, configString);
	}
}

#ifdef DEBUG
/** @todo this does not belong here */
#include "../common/routing.h"

/**
 * @brief  Dumps contents of the entire server map to console for inspection.
 * @sa CL_InitLocal
 */
static void Grid_DumpWholeServerMap_f (void)
{
	RT_DumpWholeMap(&sv->mapTiles, sv->mapData.routing);
}

/**
 * @brief  Dumps contents of the entire server routing table to CSV file.
 * @sa CL_InitLocal
 */
static void Grid_DumpServerRoutes_f (void)
{
	ipos3_t wpMins, wpMaxs;
	VecToPos(sv->mapData.mapBox.mins, wpMins);
	VecToPos(sv->mapData.mapBox.maxs, wpMaxs);
	RT_WriteCSVFiles(sv->mapData.routing, "ufoaiserver", GridBox(wpMins, wpMaxs));
}
#endif

/**
 * @sa SV_Init
 */
void SV_InitOperatorCommands (void)
{
	Cmd_AddCommand("heartbeat", SV_Heartbeat_f, "Sends a heartbeat to the masterserver");
	Cmd_AddCommand("kick", SV_Kick_f, "Kick a user from the server");
	Cmd_AddCommand("startgame", SV_StartGame_f, "Forces a game start even if not all players are ready yet");
	Cmd_AddCommand("status", SV_Status_f, "Prints status of server and connected clients");
	Cmd_AddCommand("serverinfo", SV_Serverinfo_f, "Prints the serverinfo that is visible in the server browsers");
	Cmd_AddCommand("info", SV_UserInfo_f, "Prints the userinfo for a given userid");

	Cmd_AddCommand("map", SV_Map_f, "Quit client and load the new map");
	Cmd_AddParamCompleteFunction("map", SV_CompleteMapCommand);
	Cmd_AddCommand("devmap", SV_Map_f, "Quit client and load the new map - deactivate the ai");
	Cmd_AddParamCompleteFunction("devmap", SV_CompleteMapCommand);
	Cmd_AddCommand("maplist", SV_ListMaps_f, "List of all available maps");

	Cmd_AddCommand("setmaster", SV_SetMaster_f, "Send ping command to masterserver (see cvar masterserver_url)");

#ifdef DEDICATED_ONLY
	Cmd_AddCommand("say", SV_ConSay_f, "Broadcasts server messages to all connected players");
#endif

#ifdef DEBUG
	Cmd_AddCommand("debug_sgrid", Grid_DumpWholeServerMap_f, "Shows the whole server side pathfinding grid of the current loaded map");
	Cmd_AddCommand("debug_sroute", Grid_DumpServerRoutes_f, "Shows the whole server side pathfinding grid of the current loaded map");
#endif

	Cmd_AddCommand("killserver", SV_KillServer_f, "Shuts the server down - and disconnect all clients");
	Cmd_AddCommand("sv_configstrings", SV_PrintConfigStrings_f, "Prints the server config strings to game console");

	Cmd_AddCommand("sv", SV_ServerCommand_f, "Server command");
	Cmd_AddParamCompleteFunction("sv", SV_CompleteServerCommand);
}
