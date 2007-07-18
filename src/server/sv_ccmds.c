/**
 * @file sv_ccmds.c
 * @brief Console-only server commands.
 *
 * These commands can only be entered from stdin or by a remote operator datagram.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

/**
 * @brief Specify a list of master servers
 * @sa SV_InitGame
 */
void SV_SetMaster_f (void)
{
	struct net_stream *s;
	if (sv_maxclients->integer == 1)
		return;

	/* make sure the server is listed public */
	Cvar_Set("public", "1");

	Com_Printf("Master server at [%s]:%s - sending a ping\n", masterserver_ip->string, masterserver_port->string);

	s = connect_to_host(masterserver_ip->string, masterserver_port->string);
	NET_OOB_Printf(s, "ping\n");
	stream_finished(s);

	if (!dedicated->integer)
		return;

	svs.last_heartbeat = -9999999;
}



/**
 * @brief Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
 */
static qboolean SV_SetPlayer (void)
{
	client_t *cl;
	int i;
	int idnum;
	const char *s;

	if (Cmd_Argc() < 2)
		return qfalse;

	s = Cmd_Argv(1);

	/* numeric values are just slot numbers */
	if (s[0] >= '0' && s[0] <= '9') {
		idnum = atoi(Cmd_Argv(1));
		if (idnum < 0 || idnum >= sv_maxclients->integer) {
			Com_Printf("Bad client slot: %i\n", idnum);
			return qfalse;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->player;
		if (!sv_client->state) {
			Com_Printf("Client %i is not active\n", idnum);
			return qfalse;
		}
		return qtrue;
	}

	/* check for a name match */
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (!cl->state)
			continue;
		if (!strcmp(cl->name, s)) {
			sv_client = cl;
			sv_player = sv_client->player;
			return qtrue;
		}
	}

	Com_Printf("Userid %s is not on the server\n", s);
	return qfalse;
}

/**
 * @brief Puts the server in demo mode on a specific map/cinematic
 */
static void SV_Demo_f (void)
{
	SV_Map(qtrue, va("%s.dm", Cmd_Argv(1)), NULL);
}

/**
 * @brief Checks wether a map exists
 */
static qboolean SV_CheckMap (const char *map, const char *assembly)
{
	char	expanded[MAX_QPATH];

	/* base attacks starts with . and random maps with + */
	/* also check the . to determine whether a pcx or demo should be loaded */
	if (!strstr(map, ".")) {
		if (map[0] == '+') {
			Com_sprintf(expanded, sizeof(expanded), "maps/%s.ump", map+1);

			/* check for ump file */
			if (FS_CheckFile(expanded) < 0) {
				Com_Printf("Can't find %s\n", expanded);
				return qfalse;
			}
		} else if (!assembly) {
			Com_sprintf(expanded, sizeof(expanded), "maps/%s.bsp", map);

			/* check for bsp file */
			if (FS_CheckFile(expanded) < 0) {
				Com_Printf("Can't find %s\n", expanded);
				return qfalse;
			}
		}
	}
	return qtrue;
}

/**
 * @brief Goes directly to a given map
 * @sa SV_InitGame
 * @sa SV_SpawnServer
 */
static void SV_Map_f (void)
{
	const char *map, *assembly = NULL;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <mapname>\n", Cmd_Argv(0));
		Com_Printf("Use 'maplist' to get a list of all installed maps\n");
		return;
	}

	if (!Q_strncmp(Cmd_Argv(0), "devmap", 6)) {
		Com_Printf("deactivate ai - make sure to reset sv_ai after maptesting\n");
		Cvar_SetValue("sv_ai", 0);
	}

	/* if not a pcx, demo, or cinematic, check to make sure the level exists */
	map = Cmd_Argv(1);
	/* random maps uses position strings */
	if (Cmd_Argc() == 3) {
		assembly = Cmd_Argv(2);
		Com_DPrintf("SV_Map_f: assembly: '%s'\n", assembly);
	}

	if (!SV_CheckMap(map, assembly))
		return;

	sv.state = ss_dead;		/* don't save current level when changing */
	SV_Map(qfalse, map, assembly);

	/* archive server state */
	Q_strncpyz(svs.mapcmd, map, sizeof(svs.mapcmd));
}

/**
 * @brief Kick a user off of the server
 */
static void SV_Kick_f (void)
{
	if (!svs.initialized) {
		Com_Printf("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: kick <userid>\n");
		return;
	}

	if (!SV_SetPlayer())
		return;

	SV_BroadcastPrintf(PRINT_HIGH, "%s was kicked\n", sv_client->name);
	/* print directly, because the dropped client won't get the */
	/* SV_BroadcastPrintf message */
	SV_ClientPrintf(sv_client, PRINT_HIGH, "You were kicked from the game\n");
	SV_DropClient(sv_client);
}


/**
 * @brief Prints some server info to the game console - like connected players
 * and current running map
 */
static void SV_Status_f (void)
{
	int i, j, l;
	client_t *cl;
	const char *s;
	int ping;
	char buf[256];

	if (!svs.clients) {
		Com_Printf("No server running.\n");
		return;
	}
	Com_Printf("map              : %s\n", sv.name);

	Com_Printf("num ping name            address              \n");
	Com_Printf("--- ---- --------------- ---------------------\n");
	for (i = 0, cl = svs.clients; i < sv_maxclients->integer; i++, cl++) {
		if (!cl->state)
			continue;
		Com_Printf("%3i ", i);

		if (cl->state == cs_connected || cl->state == cs_spawning)
			Com_Printf("CNCT ");
		else if (cl->state == cs_zombie)
			Com_Printf("ZMBI ");
		else {
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf("%4i ", ping);
		}

		Com_Printf("%s", cl->name);
		l = 16 - strlen(cl->name);
		for (j = 0; j < l; j++)
			Com_Printf(" ");

		s = stream_peer_name(cl->stream, buf, sizeof(buf));
		Com_Printf("%s", s);
		l = 22 - strlen(s);
		for (j = 0; j < l; j++)
			Com_Printf(" ");

		Com_Printf("\n");
	}
	Com_Printf("\n");
}

#ifdef DEDICATED_ONLY
/**
 * @brief
 * @sa SV_BroadcastPrintf
 */
static void SV_ConSay_f (void)
{
	const char *p;
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

	Q_strcat(text, p, sizeof(text));
	if (text[strlen(text)] == '"')
		text[strlen(text)] = 0;
	SV_BroadcastPrintf(PRINT_CHAT, "%s\n", text);
}
#endif

/**
 * @brief
 */
static void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999999;
}


/**
 * @brief Examine or change the serverinfo string
 */
static void SV_Serverinfo_f (void)
{
	Com_Printf("Server info settings:\n");
	Info_Print(Cvar_Serverinfo());
}


/**
 * @brief Examine all a users info strings
 */
static void SV_DumpUser_f (void)
{
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: info <userid>\n");
		return;
	}

	if (!SV_SetPlayer())
		return;

	Com_Printf("userinfo\n");
	Com_Printf("--------\n");
	Info_Print(sv_client->userinfo);
}


/**
 * @brief Begins server demo recording.  Every entity and every message will be
 * recorded, but no playerinfo will be stored. Primarily for demo merging.
 * @sa SV_ServerRecordStop_f
 */
static void SV_ServerRecordRecord_f (void)
{
	char name[MAX_OSPATH];
	byte buf_data[32768];
	sizebuf_t buf;
	int len;
	int i;

	if (Cmd_Argc() != 2) {
		Com_Printf("serverrecord <demoname>\n");
		return;
	}

	if (svs.demofile.f) {
		Com_Printf("Already recording. Use serverstop to stop the record.\n");
		return;
	}

	if (sv.state != ss_game) {
		Com_Printf("You must be in a level to record.\n");
		return;
	}

	/* open the demo file */
	Com_sprintf(name, sizeof(name), "%s/demos/%s.dm", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf("recording to %s.\n", name);
	FS_CreatePath(name);
	svs.demofile.f = fopen(name, "wb");
	if (!svs.demofile.f) {
		Com_Printf("ERROR: couldn't open.\n");
		return;
	}

	/* write a single giant fake message with all the startup info */
	SZ_Init(&buf, buf_data, sizeof(buf_data));

	/* serverdata needs to go over for all types of servers */
	/* to make sure the protocol is right, and to set the gamedir */

	/* send the serverdata */
	MSG_WriteByte(&buf, svc_serverdata);
	MSG_WriteLong(&buf, PROTOCOL_VERSION);
	MSG_WriteLong(&buf, svs.spawncount);
	/* 2 means server demo */
	MSG_WriteByte(&buf, 2);		/* demos are always attract loops */
	MSG_WriteString(&buf, Cvar_VariableString("fs_gamedir"));
	MSG_WriteShort(&buf, -1);
	/* send full levelname */
	MSG_WriteString(&buf, sv.configstrings[CS_NAME]);

	for (i = 0; i < MAX_CONFIGSTRINGS; i++)
		if (sv.configstrings[i][0]) {
			MSG_WriteByte(&buf, svc_configstring);
			MSG_WriteShort(&buf, i);
			MSG_WriteString(&buf, sv.configstrings[i]);
		}

	/* write it to the demo file */
	Com_DPrintf("signon message length: %i\n", buf.cursize);
	len = LittleLong(buf.cursize);
	if (fwrite(&len, 4, 1, svs.demofile.f) != 1)
		Com_Printf("Error writing demofile\n");
	if (fwrite(buf.data, buf.cursize, 1, svs.demofile.f) != 1)
		Com_Printf("Error writing demofile\n");

	/* the rest of the demo file will be individual frames */
}


/**
 * @brief Ends server demo recording
 * @sa SV_ServerRecordRecord_f
 */
static void SV_ServerRecordStop_f (void)
{
	if (!svs.demofile.f) {
		Com_Printf("Not doing a serverrecord. Use serverrecord to start one.\n");
		return;
	}
	fclose(svs.demofile.f);
	svs.demofile.f = NULL;
	Com_Printf("Recording completed.\n");
}


/**
 * @brief Kick everyone off, possibly in preparation for a new game
 */
static void SV_KillServer_f (void)
{
	if (!svs.initialized)
		return;
	SV_Shutdown("Server was killed.\n", qfalse);
	stop_server();
}

/**
 * @brief Let the game dll handle a command
 */
static void SV_ServerCommand_f (void)
{
	if (!ge) {
		Com_Printf("No game loaded.\n");
		return;
	}

	ge->ServerCommand();
}

/*=========================================================== */

/**
 * @brief Autocomplete function for the map command
 * @sa Cmd_AddParamCompleteFunction
 */
static int SV_CompleteMapCommand (const char *partial, const char **match)
{
	int i, j, matches = 0;
	const char *localMatch[128];
	size_t len, lenResult = 0, tmp;
	static char matchString[MAX_QPATH];

	FS_GetMaps(qfalse);

	len = strlen(partial);
	if (!len) {
		for (i = 0; i <= fs_numInstalledMaps; i++)
			Com_Printf("%s\n", fs_maps[i]);
		return fs_numInstalledMaps;
	}

	localMatch[matches] = NULL;

	/* search all matches and fill the localMatch array */
	for (i = 0; i <= fs_numInstalledMaps; i++)
		if (!Q_strncmp(partial, fs_maps[i], len)) {
			Com_Printf("%s\n", fs_maps[i]);
			localMatch[matches++] = fs_maps[i];
		}

	/* no matches or exactly one match */
	if (matches <= 1) {
		if (matches == 1)
			*match = localMatch[0];
	} else {
		/* get the shortest string of the results */
		lenResult = strlen(localMatch[0]);
		for (i = 0; i < matches; i++) {
			tmp = strlen(localMatch[i]);
			if (tmp < lenResult)
				lenResult = tmp;
		}
		/* len is >= 1 here */
		if (len != lenResult) {
			if (lenResult >= MAX_QPATH)
				lenResult = MAX_QPATH - 1;
			/* compare up to lenResult chars */
			for (i = len + 1; i < lenResult; i++) {
				/* just take the first string */
				Q_strncpyz(matchString, localMatch[0], i + 1);
				for (j = 1; j < matches; j++) {
					if (Q_strncmp(matchString, localMatch[j], i)) {
						break;
					}
				}
				if (j < matches)
					break;

				/* j must be bigger than 0 here */
				Q_strncpyz(matchString, localMatch[0], i + 1);
				*match = matchString;
				matches = 1;
			}
		}
	}

	return matches;
}

/**
 * @brief List all valid maps
 * @sa FS_GetMaps
 */
static void SV_ListMaps_f (void)
{
	int i;

	FS_GetMaps(qtrue);

	for (i = 0; i <= fs_numInstalledMaps; i++)
		Com_Printf("%s\n", fs_maps[i]);
	Com_Printf("-----\n %i installed maps\n", fs_numInstalledMaps + 1);
}

/**
 * @brief
 */
static void SV_Trayicon_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage tray [on|off]\n");
		return;
	}

	if (!Q_stricmp(Cmd_Argv(1), "on"))
		Sys_EnableTray();
	else
		Sys_DisableTray();
}

/**
 * @brief
 */
static void SV_Minimize_f (void)
{
	Sys_Minimize();
}

/**
 * @brief
 */
static void SV_MapcycleList_f (void)
{
	int i;
	mapcycle_t* mapcycle;

	mapcycle = mapcycleList;
	Com_Printf("current mapcycle has %i entries\n", mapcycleCount);
	for (i = 0; i < mapcycleCount; i++) {
		Com_Printf(" %s (%s)\n", mapcycle->map, mapcycle->type);
		mapcycle = mapcycle->next;
	}
}

/**
 * @brief
 */
static void SV_MapcycleAdd_f (void)
{
	const char *map;
	const char *gametype;

	if (Cmd_Argc() == 3) {
		map = Cmd_Argv(1);
		gametype = Cmd_Argv(2);
		if (!SV_CheckMap(map, NULL)) {
			Com_Printf("map '%s' isn't a valid map\n", map);
			return;
		}
		Com_Printf("adding map '%s' with gametype '%s' to mapcycle (to add this permanently edit your mapcycle.txt)\n", map, gametype);
		SV_MapcycleAdd(map, gametype);
	} else {
		Com_Printf("usage: %s <mapname> <gametype>\n", Cmd_Argv(0));
		Com_Printf(" ...to get a list of valid maps type 'maplist'\n"
			" ...to get a list of valid gametypes 'gametypelist'\n");
	}
}

/**
 * @brief
 */
static void SV_MapcycleNext_f (void)
{
	if (mapcycleCount > 0)
		SV_NextMapcycle();
	else
		Com_Printf("no mapcycle.txt\n");
}

/**
 * @brief List for SV_CompleteServerCommand
 * @sa ServerCommand
 */
static const char *serverCommandList[] = {
	"startgame", "Force the gamestart - useful for multiplayer games",
	"addip", "The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with 'addip 192.246.40'",
	"removeip", "Removeip will only remove an address specified exactly the same way. You cannot addip a subnet, then removeip a single host",
	"listip", "Prints the current list of filters",
	"writeip", "Dumps ips to listip.cfg so it can be executed at a later date",
	"ai_add", "Used to add ai opponents to a game - but no civilians",
	"win", "Call the end game function with the given team",
#ifdef DEBUG
	"showall", "Debug function: Reveal all items to all sides",
	"actorinvlist", "Debug function to show the hole inventory of all connected clients on the server",
#endif
	NULL
};


/**
 * @brief Autocomplete function for server commands
 * @sa ServerCommand
 */
static int SV_CompleteServerCommand (const char *partial, const char **match)
{
	int i, j, matches = 0;
	const char *localMatch[128]; /* i don't think that there will ever
								* be more than 128 server command ;-) */
	size_t len, lenResult = 0, tmp;
	static char matchString[MAX_QPATH];
	int numServerCommands;

	len = strlen(partial);
	if (!len) {
		for (i = 0; ; i += 2) {
			if (!serverCommandList[i])
				break;
			Com_Printf("[cmd] %s\n", serverCommandList[i]);
			if (*serverCommandList[i + 1])
			Com_Printf("%c      %s\n", 1, serverCommandList[i + 1]);
		}
		return i - 1;
	}

	for (i = 0, numServerCommands = 0; ; numServerCommands++, i += 2) {
		if (!serverCommandList[i])
			break;
	}

	localMatch[matches] = NULL;

	/* search all matches and fill the localMatch array */
	for (i = 0; i < numServerCommands; i++)
		if (!Q_strncmp(partial, serverCommandList[i * 2], len)) {
			Com_Printf("[cmd] %s\n", serverCommandList[i * 2]);
			if (*serverCommandList[i * 2 + 1])
				Com_Printf("%c      %s\n", 1, serverCommandList[i * 2 + 1]);
			localMatch[matches++] = serverCommandList[i * 2];
		}

	/* no matches or exactly one match */
	if (matches <= 1) {
		if (matches == 1)
			*match = localMatch[0];
	} else {
		/* get the shortest string of the results */
		lenResult = strlen(localMatch[0]);
		for (i = 0; i < matches; i++) {
			tmp = strlen(localMatch[i]);
			if (tmp < lenResult)
				lenResult = tmp;
		}
		/* len is >= 1 here */
		if (len != lenResult) {
			if (lenResult >= MAX_QPATH)
				lenResult = MAX_QPATH - 1;
			/* compare up to lenResult chars */
			for (i = len + 1; i < lenResult; i++) {
				/* just take the first string */
				Q_strncpyz(matchString, localMatch[0], i + 1);
				for (j = 1; j < matches; j++) {
					if (Q_strncmp(matchString, localMatch[j], i)) {
						break;
					}
				}
				if (j < matches)
					break;

				/* j must be bigger than 0 here */
				Q_strncpyz(matchString, localMatch[0], i + 1);
				*match = matchString;
				matches = 1;
			}
		}
	}

	return matches;
}

/**
 * @brief
 */
void SV_InitOperatorCommands (void)
{
	Cmd_AddCommand("heartbeat", SV_Heartbeat_f, "Sends a heartbeat to the masterserver");
	Cmd_AddCommand("kick", SV_Kick_f, "Kick a user from the server");
	Cmd_AddCommand("status", SV_Status_f, "Prints status of server and connected clients");
	Cmd_AddCommand("serverinfo", SV_Serverinfo_f, "Prints the serverinfo that is visible in the server browsers");
	Cmd_AddCommand("dumpuser", SV_DumpUser_f, "Prints the userinfo for a given userid");

	Cmd_AddCommand("map", SV_Map_f, "Quit client and load the new map");
	Cmd_AddParamCompleteFunction("map", SV_CompleteMapCommand);
	Cmd_AddCommand("devmap", SV_Map_f, "Quit client and load the new map - deactivate the ai");
	Cmd_AddParamCompleteFunction("devmap", SV_CompleteMapCommand);
	Cmd_AddCommand("demo", SV_Demo_f, "Start a demo server");
	Cmd_AddCommand("maplist", SV_ListMaps_f, "List of all available maps");

	Cmd_AddCommand("setmaster", SV_SetMaster_f, "Send ping command to masterserver (see cvars: masterserver_ip and masterserver_port)");
	Cmd_AddCommand("tray", SV_Trayicon_f, "Enable or disable minimize to notifcation area");
	Cmd_AddCommand("minimize", SV_Minimize_f, "Minimize");
	Cmd_AddCommand("mapcyclelist", SV_MapcycleList_f, "Print the current mapcycle");
	Cmd_AddCommand("mapcyclenext", SV_MapcycleNext_f, "Start the next map from the cycle");
	Cmd_AddCommand("mapcycleclear", SV_MapcycleClear, "Delete the current mapcycle");
	Cmd_AddCommand("mapcycleadd", SV_MapcycleAdd_f, "Add new maps to the mapcycle");
	Cmd_AddParamCompleteFunction("mapcycleadd", SV_CompleteMapCommand);

#ifdef DEDICATED_ONLY
	Cmd_AddCommand("say", SV_ConSay_f, "Broadcasts server messages to all connected players");
#endif

	Cmd_AddCommand("serverrecord", SV_ServerRecordRecord_f, "Starts a server record session");
	Cmd_AddCommand("serverstop", SV_ServerRecordStop_f, "Stops the server record session");

	Cmd_AddCommand("killserver", SV_KillServer_f, "Shuts the server down - and disconnect all clients");

	Cmd_AddCommand("sv", SV_ServerCommand_f, "Server command");
	Cmd_AddParamCompleteFunction("sv", SV_CompleteServerCommand);
}
