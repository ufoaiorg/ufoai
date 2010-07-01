/**
 * @file sv_game.c
 * @brief Interface to the game library.
 */

/*
 All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

 Original file from Quake 2 v3.21: quake2-2.31/server/sv_game.c
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
#include <SDL.h>
#include <SDL_thread.h>

game_export_t *ge;
/** this is true when there was an event - and false if the event reached the end */
static qboolean pfe_pending = qfalse;
/** player mask of the current event */
static int pfe_mask, pfe_type;
static struct dbuffer *pfe_msg;
struct dbuffer *sv_msg = NULL;
static SDL_Thread *thread;
static void *gameLibrary;

/**
 * @brief Debug print to server console
 */
static void SV_dprintf (const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_vPrintf(fmt, ap);
	va_end(ap);
}

/**
 * @brief Print to a single client
 * @sa SV_BroadcastPrintf
 */
static void SV_PlayerPrintf (const player_t * player, int level, const char *fmt, va_list ap)
{
	char msg[1024];

	if (level == PRINT_NONE)
		return;

	Q_vsnprintf(msg, sizeof(msg), fmt, ap);

	if (player)
		SV_ClientPrintf(svs.clients + player->num, level, "%s", msg);
	else
		Com_Printf("%s", msg);
}

static void SV_error (const char *fmt, ...) __attribute__((noreturn));
/**
 * @brief Abort the server with a game error
 * @note The error message should not have a newline - it's added inside of this function
 */
static void SV_error (const char *fmt, ...)
{
	char msg[1024];
	va_list argptr;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Error(ERR_DROP, "Game Error: %s", msg);
}

/**
 * @note Also sets mins and maxs for inline bmodels
 * @sa CM_InlineModel
 */
static void SV_SetModel (edict_t * ent, const char *name)
{
	if (!name)
		Com_Error(ERR_DROP, "SV_SetModel: NULL");

	ent->modelindex = SV_ModelIndex(name);

	/* if it is an inline model, get the size information for it */
	if (name[0] == '*') {
		cBspModel_t *mod = CM_InlineModel(name);
		assert(mod);
		/* Copy model mins and maxs to entity */
		VectorCopy(mod->mins, ent->mins);
		VectorCopy(mod->maxs, ent->maxs);
		/* This is to help the entity collision code out */
		/* Copy entity origin and angles to model*/
		CM_SetInlineModelOrientation(name, ent->origin, ent->angles);
	}
}

/**
 * @sa CL_ParseConfigString
 */
static void SV_Configstring (int index, const char *fmt, ...)
{
	char val[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
	va_list argptr;

	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		Com_Error(ERR_DROP, "configstring: bad index %i", index);

	va_start(argptr, fmt);
	Q_vsnprintf(val, sizeof(val), fmt, argptr);
	va_end(argptr);

	/* change the string in sv
	 * there may be overflows in i==CS_TILES - but thats ok
	 * see definition of configstrings and MAX_TILESTRINGS */
	switch (index) {
	case CS_TILES:
	case CS_POSITIONS:
		Q_strncpyz(sv.configstrings[index], val, MAX_TOKEN_CHARS * MAX_TILESTRINGS);
		break;
	default:
		Q_strncpyz(sv.configstrings[index], val, MAX_TOKEN_CHARS);
		break;
	}

	if (Com_ServerState() != ss_loading) { /* send the update to everyone */
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_configstring);
		NET_WriteShort(msg, index);
		NET_WriteString(msg, val);

		/* send to all clients */
		SV_Multicast(~0, msg);
	}
}

static void SV_WriteChar (char c)
{
	NET_WriteChar(pfe_msg, c);
}

static void SV_WriteByte (byte c)
{
	NET_WriteByte(pfe_msg, c);
}

/**
 * @brief Use this if the value might change and you need the position in the buffer
 */
static byte* SV_WriteDummyByte (byte c)
{
	byte *pos = (byte*) pfe_msg->end;
	NET_WriteByte(pfe_msg, c);
	return pos;
}

static void SV_WriteShort (int c)
{
	NET_WriteShort(pfe_msg, c);
}

static void SV_WriteLong (int c)
{
	NET_WriteLong(pfe_msg, c);
}

static void SV_WriteString (const char *s)
{
	NET_WriteString(pfe_msg, s);
}

static void SV_WritePos (const vec3_t pos)
{
	NET_WritePos(pfe_msg, pos);
}

static void SV_WriteGPos (const pos3_t pos)
{
	NET_WriteGPos(pfe_msg, pos);
}

static void SV_WriteDir (const vec3_t dir)
{
	NET_WriteDir(pfe_msg, dir);
}

static void SV_WriteAngle (float f)
{
	NET_WriteAngle(pfe_msg, f);
}

static void SV_WriteFormat (const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	NET_vWriteFormat(pfe_msg, format, ap);
	va_end(ap);
}

static int SV_ReadChar (void)
{
	return NET_ReadChar(sv_msg);
}

static int SV_ReadByte (void)
{
	return NET_ReadByte(sv_msg);
}

static int SV_ReadShort (void)
{
	return NET_ReadShort(sv_msg);
}

static int SV_ReadLong (void)
{
	return NET_ReadLong(sv_msg);
}

static char *SV_ReadString (void)
{
	return NET_ReadString(sv_msg);
}

static void SV_ReadPos (vec3_t pos)
{
	NET_ReadPos(sv_msg, pos);
}

static void SV_ReadGPos (pos3_t pos)
{
	NET_ReadGPos(sv_msg, pos);
}

static void SV_ReadDir (vec3_t vector)
{
	NET_ReadDir(sv_msg, vector);
}

static float SV_ReadAngle (void)
{
	return NET_ReadAngle(sv_msg);
}

static void SV_ReadData (void *buffer, int size)
{
	NET_ReadData(sv_msg, buffer, size);
}

/**
 * @sa NET_vReadFormat
 */
static void SV_ReadFormat (const char *format, ...)
{
	va_list ap;

	assert(format);
	if (!*format) /* PA_NULL */
		return;
	va_start(ap, format);
	NET_vReadFormat(sv_msg, format, ap);
	va_end(ap);
}

/**
 * @sa gi.AbortEvents
 */
static void SV_AbortEvents (void)
{
	if (!pfe_pending)
		return;

	pfe_pending = qfalse;
	free_dbuffer(pfe_msg);
	pfe_msg = NULL;
}

/**
 * @sa gi.EndEvents
 */
static void SV_EndEvents (void)
{
	if (!pfe_pending)
		return;

	NET_WriteByte(pfe_msg, EV_NULL);
	SV_Multicast(pfe_mask, pfe_msg);
	pfe_pending = qfalse;
	/* freed in SV_Multicast */
	pfe_msg = NULL;
}

/**
 * @sa gi.AddEvent
 * @param[in] mask The player bitmask to send the events to. Use @c PM_ALL to send to every connected player.
 */
static void SV_AddEvent (unsigned int mask, int eType)
{
	/* finish the last event */
	if (pfe_pending)
		SV_EndEvents();

	/* start the new event */
	pfe_pending = qtrue;
	pfe_mask = mask;
	pfe_type = eType;
	pfe_msg = new_dbuffer();

	/* write header */
	NET_WriteByte(pfe_msg, svc_event);
	NET_WriteByte(pfe_msg, eType);
}

/**
 * @return The current active event or -1 if no event is active at the moment
 */
static int SV_GetEvent (void)
{
	if (!pfe_pending)
		return -1;

	return pfe_type;
}

/**
 * @brief Makes sure the game DLL does not use client, or signed tags
 */
static void *SV_TagAlloc (int size, int tagNum, const char *file, int line)
{
	if (tagNum < 0)
		tagNum *= -1;

	return _Mem_Alloc(size, qtrue, sv_gameSysPool, tagNum, file, line);
}

static void SV_MemFree (void *ptr, const char *file, int line)
{
	_Mem_Free(ptr, file, line);
}

/**
 * @brief Makes sure the game DLL does not use client, or signed tags
 */
static void SV_FreeTags (int tagNum)
{
	if (tagNum < 0)
		tagNum *= -1;

	_Mem_FreeTag(sv_gameSysPool, tagNum, "GAME DLL", 0);
}

static void SV_UnloadGame (void)
{
#ifndef GAME_HARD_LINKED
	if (gameLibrary)
		SDL_UnloadObject(gameLibrary);
#endif
	gameLibrary = NULL;
}

#ifndef HARD_LINKED_GAME
static qboolean SV_LoadGame (const char *path)
{
	char name[MAX_OSPATH];

	Com_sprintf(name, sizeof(name), "%s/game_"CPUSTRING".%s", path, SHARED_EXT);
	gameLibrary = SDL_LoadObject(name);
	if (!gameLibrary) {
		Com_sprintf(name, sizeof(name), "%s/game.%s", path, SHARED_EXT);
		gameLibrary = SDL_LoadObject(name);
	}

	if (gameLibrary) {
		Com_Printf("found at '%s'\n", path);
		return qtrue;
	} else {
		Com_Printf("not found at '%s'\n", path);
		Com_DPrintf(DEBUG_SYSTEM, "%s\n", SDL_GetError());
		return qfalse;
	}
}
#endif

/**
 * @brief Loads the game shared library and calls the api init function
 */
static game_export_t *SV_GetGameAPI (game_import_t *parms)
{
#ifndef HARD_LINKED_GAME
	void *(*GetGameAPI) (void *);

	const char *path;
#endif

	if (gameLibrary)
		Com_Error(ERR_FATAL, "SV_GetGameAPI without Com_UnloadGame");

#ifndef HARD_LINKED_GAME
	Com_Printf("------- Loading game.%s -------\n", SHARED_EXT);

#ifdef PKGLIBDIR
	SV_LoadGame(PKGLIBDIR);
#endif

	/* now run through the search paths */
	path = NULL;
	while (!gameLibrary) {
		path = FS_NextPath(path);
		if (!path)
			/* couldn't find one anywhere */
			return NULL;
		else if (SV_LoadGame(path))
			break;
	}

	GetGameAPI = (void *)SDL_LoadFunction(gameLibrary, "GetGameAPI");
	if (!GetGameAPI) {
		SV_UnloadGame();
		return NULL;
	}
#endif

	return GetGameAPI(parms);
}

/**
 * @brief Called when either the entire server is being killed, or it is changing to a different game directory.
 * @sa G_Shutdown
 * @sa SV_InitGameProgs
 */
void SV_ShutdownGameProgs (void)
{
	uint32_t size;

	if (!ge)
		return;

	if (thread)
		SDL_KillThread(thread);

	thread = NULL;

	ge->Shutdown();
	SV_UnloadGame();

	size = Mem_PoolSize(sv_gameSysPool);
	if (size > 0) {
		Com_Printf("WARNING: Game memory leak (%u bytes)\n", size);
	}

	ge = NULL;
}

/**
 * @brief Thread for the game frame function
 * @sa SV_RunGameFrame
 * @sa SV_Frame
 */
int SV_RunGameFrameThread (void *data)
{
	do {
		SV_RunGameFrame();
	} while (!sv.endgame);

	return 0;
}

/**
 * @brief Calls the G_RunFrame function from game api
 * let everything in the world think and move
 * @sa G_RunFrame
 * @sa SV_Frame
 */
void SV_RunGameFrame (void)
{
	sv.endgame = ge->RunFrame();
}

/**
 * @brief Init the game subsystem for a new map
 * @sa SV_ShutdownGameProgs
 */
void SV_InitGameProgs (void)
{
	game_import_t import;

	/* unload anything we have now */
	if (ge)
		SV_ShutdownGameProgs();

	/* load a new game dll */
	import.BroadcastPrintf = SV_BroadcastPrintf;
	import.dprintf = SV_dprintf;
	import.PlayerPrintf = SV_PlayerPrintf;
	import.error = SV_error;

	import.trace = SV_Trace;
	import.LinkEdict = SV_LinkEdict;
	import.UnlinkEdict = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.TouchEdicts = SV_TouchEdicts;

	import.TestLine = TR_TestLine;
	import.TestLineWithEnt = CM_TestLineWithEnt;
	import.GrenadeTarget = Com_GrenadeTarget;

	import.MoveCalc = Grid_MoveCalc;
	import.MoveStore = Grid_MoveStore;
	import.MoveLength = Grid_MoveLength;
	import.MoveNext = Grid_MoveNext;
	import.GridFloor = Grid_Floor;
	import.GetTUsForDirection = Grid_GetTUsForDirection;
	import.GridFall = Grid_Fall;
	import.GridPosToVec = Grid_PosToVec;
	import.GridRecalcRouting = Grid_RecalcRouting;
	import.GridDumpDVTable = Grid_DumpDVTable;

	import.ModelIndex = SV_ModelIndex;

	import.SetInlineModelOrientation = CM_SetInlineModelOrientation;

	import.SetModel = SV_SetModel;

	import.ConfigString = SV_Configstring;
	import.PositionedSound = SV_StartSound;

	import.PointContents = SV_PointContents;
	import.GetFootstepSound = SV_GetFootstepSound;
	import.GetBounceFraction = SV_GetBounceFraction;
	import.LoadModelMinsMaxs = SV_LoadModelMinsMaxs;

	import.FS_Gamedir = FS_Gamedir;
	import.FS_LoadFile = FS_LoadFile;
	import.FS_FreeFile = FS_FreeFile;

	import.WriteChar = SV_WriteChar;
	import.WriteByte = SV_WriteByte;
	import.WriteDummyByte = SV_WriteDummyByte;
	import.WriteShort = SV_WriteShort;
	import.WriteLong = SV_WriteLong;
	import.WriteString = SV_WriteString;
	import.WritePos = SV_WritePos;
	import.WriteGPos = SV_WriteGPos;
	import.WriteDir = SV_WriteDir;
	import.WriteAngle = SV_WriteAngle;
	import.WriteFormat = SV_WriteFormat;

	import.AbortEvents = SV_AbortEvents;
	import.EndEvents = SV_EndEvents;
	import.AddEvent = SV_AddEvent;
	import.GetEvent = SV_GetEvent;

	import.ReadChar = SV_ReadChar;
	import.ReadByte = SV_ReadByte;
	import.ReadShort = SV_ReadShort;
	import.ReadLong = SV_ReadLong;
	import.ReadString = SV_ReadString;
	import.ReadPos = SV_ReadPos;
	import.ReadGPos = SV_ReadGPos;
	import.ReadDir = SV_ReadDir;
	import.ReadAngle = SV_ReadAngle;
	import.ReadData = SV_ReadData;
	import.ReadFormat = SV_ReadFormat;

	import.GetConstInt = Com_GetConstInt;
	import.GetConstIntFromNamespace = Com_GetConstIntFromNamespace;
	import.GetConstVariable = Com_GetConstVariable;
	import.RegisterConstInt = Com_RegisterConstInt;
	import.UnregisterConstVariable = Com_UnregisterConstVariable;

	import.GetCharacterValues = Com_GetCharacterValues;

	import.TagMalloc = SV_TagAlloc;
	import.TagFree = SV_MemFree;
	import.FreeTags = SV_FreeTags;

	import.Cvar_Get = Cvar_Get;
	import.Cvar_Set = Cvar_Set;
	import.Cvar_String = Cvar_GetString;

	import.Cmd_Argc = Cmd_Argc;
	import.Cmd_Argv = Cmd_Argv;
	import.Cmd_Args = Cmd_Args;
	import.AddCommandString = Cbuf_AddText;

	import.seed = Sys_Milliseconds();
	import.csi = &csi;

	/* import the server routing table */
	import.routingMap = (void *) &svMap;
	/* import the server pathing table */
	import.pathingMap = (void *) &svPathMap;

	ge = SV_GetGameAPI(&import);

	if (!ge)
		Com_Error(ERR_DROP, "failed to load game library");
	if (ge->apiversion != GAME_API_VERSION)
		Com_Error(ERR_DROP, "game is version %i, not %i", ge->apiversion, GAME_API_VERSION);

	ge->Init();

	if (sv_threads->integer)
		thread = SDL_CreateThread(SV_RunGameFrameThread, NULL);
}
