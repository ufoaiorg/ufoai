/**
 * @file sv_game.c
 * @brief Interface to the game library.
 */

/*
 All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include "sv_log.h"
#include "../common/grid.h"
#include "../ports/system.h"
#include <SDL.h>

/**
 * @brief Debug print to server console
 */
static void SV_dprintf (const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	SV_LogAdd(fmt, ap);
	va_end(ap);
}

/**
 * @brief Print to a single client
 * @sa SV_BroadcastPrintf
 */
static void SV_PlayerPrintf (const player_t * player, int level, const char *fmt, va_list ap)
{
	if (level == PRINT_NONE)
		return;

	if (player) {
		char msg[1024];
		Q_vsnprintf(msg, sizeof(msg), fmt, ap);
		client_t *cl = SV_GetClient(player->num);
		SV_ClientPrintf(cl, level, "%s", msg);
	} else
		SV_LogAdd(fmt, ap);
}

/**
 * @brief Glue function to get the visibility from a given position
 */
static float SV_GetVisibility (const pos3_t position)
{
	return CM_GetVisibility(&sv->mapTiles, position);
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

	/* unlock the game thread loop */
	TH_MutexUnlock(svs.serverMutex);

	Com_Error(ERR_DROP, "Game Error: %s", msg);
}

/**
 * @brief Search the index in the config strings relative to a given start
 * @param name The value of the config string to search the index for
 * @param start The relative start point for the search
 * @param max The max. searched entries in the config string before giving up
 * @param create if @c true the value will get written into the config strings (appended)
 * @return @c 0 if not found
 */
static unsigned int SV_FindIndex (const char *name, int start, int max, qboolean create)
{
	int i;

	if (!name || !name[0])
		return 0;

	for (i = 1; i < max && SV_GetConfigString(start + i)[0] != '\0'; i++) {
		const char *configString = SV_GetConfigString(start + i);
		if (Q_streq(configString, name))
			return i;
	}

	if (!create)
		return 0;

	if (i == max)
		SV_error("*Index: overflow '%s' start: %i, max: %i", name, start, max);

	SV_SetConfigString(start + i, name);

	if (Com_ServerState() != ss_loading) {	/* send the update to everyone */
		struct dbuffer *msg = new_dbuffer();
		NET_WriteByte(msg, svc_configstring);
		NET_WriteShort(msg, start + i);
		NET_WriteString(msg, name);
		SV_Multicast(~0, msg);
	}

	return i;
}

static unsigned int SV_ModelIndex (const char *name)
{
	return SV_FindIndex(name, CS_MODELS, MAX_MODELS, qtrue);
}

/**
 * @note Also sets mins and maxs for inline bmodels
 * @sa CM_InlineModel
 */
static void SV_SetModel (edict_t * ent, const char *name)
{
	if (!name)
		SV_error("SV_SetModel: NULL");

	ent->modelindex = SV_ModelIndex(name);

	/* if it is an inline model, get the size information for it */
	if (name[0] == '*') {
		const cBspModel_t *mod = CM_InlineModel(&sv->mapTiles, name);
		/* Copy model mins and maxs to entity */
		VectorCopy(mod->mins, ent->mins);
		VectorCopy(mod->maxs, ent->maxs);
		/* This is to help the entity collision code out */
		/* Copy entity origin and angles to model*/
		CM_SetInlineModelOrientation(&sv->mapTiles, name, ent->origin, ent->angles);
	}
}

/**
 * @sa CL_ParseConfigString
 */
static void SV_Configstring (int index, const char *fmt, ...)
{
	char val[MAX_TOKEN_CHARS * MAX_TILESTRINGS];
	va_list argptr;

	if (!Com_CheckConfigStringIndex(index))
		SV_error("configstring: bad index %i", index);

	va_start(argptr, fmt);
	Q_vsnprintf(val, sizeof(val), fmt, argptr);
	va_end(argptr);

	SV_SetConfigString(index, val);

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
	NET_WriteChar(sv->pendingEvent.buf, c);
}

static void SV_WriteByte (byte c)
{
	NET_WriteByte(sv->pendingEvent.buf, c);
}

/**
 * @brief Use this if the value might change and you need the position in the buffer
 * @note If someone aborts or adds an event, this pointer is of course no longer valid
 */
static byte* SV_WriteDummyByte (byte c)
{
	pending_event_t *p = &sv->pendingEvent;
	byte *pos = (byte*) p->buf->end;
	NET_WriteByte(p->buf, c);
	assert(pos != NULL);
	return pos;
}

static void SV_WriteShort (int c)
{
	NET_WriteShort(sv->pendingEvent.buf, c);
}

static void SV_WriteLong (int c)
{
	NET_WriteLong(sv->pendingEvent.buf, c);
}

static void SV_WriteString (const char *s)
{
	NET_WriteString(sv->pendingEvent.buf, s);
}

static void SV_WritePos (const vec3_t pos)
{
	NET_WritePos(sv->pendingEvent.buf, pos);
}

static void SV_WriteGPos (const pos3_t pos)
{
	NET_WriteGPos(sv->pendingEvent.buf, pos);
}

static void SV_WriteDir (const vec3_t dir)
{
	NET_WriteDir(sv->pendingEvent.buf, dir);
}

static void SV_WriteAngle (float f)
{
	NET_WriteAngle(sv->pendingEvent.buf, f);
}

static void SV_WriteFormat (const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	NET_vWriteFormat(sv->pendingEvent.buf, format, ap);
	va_end(ap);
}

static int SV_ReadChar (void)
{
	return NET_ReadChar(sv->messageBuffer);
}

static int SV_ReadByte (void)
{
	return NET_ReadByte(sv->messageBuffer);
}

static int SV_ReadShort (void)
{
	return NET_ReadShort(sv->messageBuffer);
}

static int SV_ReadLong (void)
{
	return NET_ReadLong(sv->messageBuffer);
}

static int SV_ReadString (char *str, size_t length)
{
	return NET_ReadString(sv->messageBuffer, str, length);
}

static void SV_ReadPos (vec3_t pos)
{
	NET_ReadPos(sv->messageBuffer, pos);
}

static void SV_ReadGPos (pos3_t pos)
{
	NET_ReadGPos(sv->messageBuffer, pos);
}

static void SV_ReadDir (vec3_t vector)
{
	NET_ReadDir(sv->messageBuffer, vector);
}

static float SV_ReadAngle (void)
{
	return NET_ReadAngle(sv->messageBuffer);
}

static void SV_ReadData (void *buffer, int size)
{
	NET_ReadData(sv->messageBuffer, buffer, size);
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
	NET_vReadFormat(sv->messageBuffer, format, ap);
	va_end(ap);
}

/**
 * @sa gi.AbortEvents
 */
static void SV_AbortEvents (void)
{
	pending_event_t *p = &sv->pendingEvent;

	if (!p->pending)
		return;

	p->pending = qfalse;
	free_dbuffer(p->buf);
	p->buf = NULL;
}

/**
 * @sa gi.EndEvents
 */
static void SV_EndEvents (void)
{
	pending_event_t *p = &sv->pendingEvent;

	if (!p->pending)
		return;

	NET_WriteByte(p->buf, EV_NULL);
	SV_Multicast(p->playerMask, p->buf);
	p->pending = qfalse;
	/* freed in SV_Multicast */
	p->buf = NULL;
}

typedef struct {
	const char *name;
	event_t eventNum;
} eventNames_t;

#define M(x) {#x, x}
const eventNames_t eventNames[] = {
	M(EV_NULL),
	M(EV_RESET),
	M(EV_START),
	M(EV_ENDROUND),
	M(EV_ENDROUNDANNOUNCE),

	M(EV_RESULTS),
	M(EV_CENTERVIEW),

	M(EV_ENT_APPEAR),
	M(EV_ENT_PERISH),
	M(EV_ENT_DESTROY),
	M(EV_ADD_BRUSH_MODEL),
	M(EV_ADD_EDICT),

	M(EV_ACTOR_APPEAR),
	M(EV_ACTOR_ADD),
	M(EV_ACTOR_TURN),
	M(EV_ACTOR_MOVE),
	M(EV_ACTOR_REACTIONFIRECHANGE),

	M(EV_ACTOR_START_SHOOT),
	M(EV_ACTOR_SHOOT),
	M(EV_ACTOR_SHOOT_HIDDEN),
	M(EV_ACTOR_THROW),

	M(EV_ACTOR_DIE),
	M(EV_ACTOR_REVITALISED),
	M(EV_ACTOR_STATS),
	M(EV_ACTOR_STATECHANGE),
	M(EV_ACTOR_RESERVATIONCHANGE),

	M(EV_INV_ADD),
	M(EV_INV_DEL),
	M(EV_INV_AMMO),
	M(EV_INV_RELOAD),
	M(EV_INV_TRANSFER),

	M(EV_MODEL_EXPLODE),
	M(EV_MODEL_EXPLODE_TRIGGERED),

	M(EV_PARTICLE_APPEAR),
	M(EV_PARTICLE_SPAWN),

	M(EV_DOOR_OPEN),
	M(EV_DOOR_CLOSE),
	M(EV_CLIENT_ACTION),
	M(EV_RESET_CLIENT_ACTION)
};
#undef M

/**
 * @sa gi.AddEvent
 * @param[in] mask The player bitmask to send the events to. Use @c PM_ALL to send to every connected player.
 */
static void SV_AddEvent (unsigned int mask, int eType)
{
	pending_event_t *p = &sv->pendingEvent;
	const int rawType = eType &~ EVENT_INSTANTLY;
	const char *eventName = eType >= 0 ? eventNames[rawType].name : "-";
	Com_DPrintf(DEBUG_EVENTSYS, "Event type: %s (%i - %i) (mask %s)\n", eventName,
			rawType, eType, Com_UnsignedIntToBinary(mask));

	/* finish the last event */
	if (p->pending)
		SV_EndEvents();

	/* start the new event */
	p->pending = qtrue;
	p->playerMask = mask;
	p->type = eType;
	p->buf = new_dbuffer();

	/* write header */
	NET_WriteByte(p->buf, svc_event);
	NET_WriteByte(p->buf, eType);
}

/**
 * @return The current active event or -1 if no event is active at the moment
 */
static int SV_GetEvent (void)
{
	const pending_event_t *p = &sv->pendingEvent;
	if (!p->pending)
		return -1;

	return p->type;
}

/**
 * @brief Makes sure the game DLL does not use client, or signed tags
 */
static void *SV_TagAlloc (int size, int tagNum, const char *file, int line)
{
	if (tagNum < 0)
		tagNum *= -1;

	return _Mem_Alloc(size, qtrue, sv->gameSysPool, tagNum, file, line);
}

static void SV_MemFree (void *ptr, const char *file, int line)
{
	_Mem_Free(ptr, file, line);
}

/**
 * @brief Makes sure the game DLL does not use client, or signed tags
 */
static void SV_FreeTags (int tagNum, const char *file, int line)
{
	if (tagNum < 0)
		tagNum *= -1;

	_Mem_FreeTag(sv->gameSysPool, tagNum, file, line);
}

static qboolean SV_TestLine (const vec3_t start, const vec3_t stop, const int levelmask)
{
	return TR_TestLine(&sv->mapTiles, start, stop, levelmask);
}

static qboolean SV_TestLineWithEnt (const vec3_t start, const vec3_t stop, const int levelmask, const char **entlist)
{
	/* do the line test */
	const qboolean hit = CM_EntTestLine(&sv->mapTiles, start, stop, levelmask, entlist);
	return hit;
}

static void SV_RecalcRouting (routing_t *map, const char *name, const char **list)
{
	Grid_RecalcRouting(&sv->mapTiles, map, name, list);
}

static void SV_SetInlineModelOrientation (const char *name, const vec3_t origin, const vec3_t angles)
{
	CM_SetInlineModelOrientation(&sv->mapTiles, name, origin, angles);
}

static void SV_UnloadGame (void)
{
#ifndef HARD_LINKED_GAME
	if (svs.gameLibrary) {
		Com_Printf("Unload the game library\n");
		SDL_UnloadObject(svs.gameLibrary);
	}
#endif
}

#ifndef HARD_LINKED_GAME
static qboolean SV_LoadGame (const char *path)
{
	char name[MAX_OSPATH];

	Com_sprintf(name, sizeof(name), "%s/game_"CPUSTRING".%s", path, SO_EXT);
	svs.gameLibrary = SDL_LoadObject(name);
	if (!svs.gameLibrary) {
		Com_sprintf(name, sizeof(name), "%s/game.%s", path, SO_EXT);
		svs.gameLibrary = SDL_LoadObject(name);
	}

	if (svs.gameLibrary) {
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
	typedef game_export_t *(*game_api_t) (game_import_t *);
	game_api_t GetGameAPI;
	const char *path;

	if (svs.gameLibrary)
		Com_Error(ERR_FATAL, "SV_GetGameAPI without SV_UnloadGame");

	Com_Printf("------- Loading game.%s -------\n", SO_EXT);

#ifdef PKGLIBDIR
	SV_LoadGame(PKGLIBDIR);
#endif

	/* now run through the search paths */
	path = NULL;
	while (!svs.gameLibrary) {
		path = FS_NextPath(path);
		if (!path)
			/* couldn't find one anywhere */
			return NULL;
		else if (SV_LoadGame(path))
			break;
	}

	GetGameAPI = (game_api_t)(uintptr_t)SDL_LoadFunction(svs.gameLibrary, "GetGameAPI");
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

	if (!svs.ge)
		return;

	Com_SetServerState(ss_game_shutdown);

	if (svs.gameThread) {
		SDL_CondSignal(svs.gameFrameCond);
		SDL_WaitThread(svs.gameThread, NULL);
		SDL_DestroyCond(svs.gameFrameCond);
		svs.gameFrameCond = NULL;
		svs.gameThread = NULL;
	}

	svs.ge->Shutdown();

	size = Mem_PoolSize(sv->gameSysPool);
	if (size > 0) {
		Com_Printf("WARNING: Game memory leak (%u bytes)\n", size);
		Cmd_ExecuteString(va("mem_stats %s", sv->gameSysPool->name));
	}

	Mem_DeletePool(sv->gameSysPool);
	sv->gameSysPool = NULL;

	SV_UnloadGame();

	svs.ge = NULL;
}

/**
 * @brief Thread for the game frame function
 * @sa SV_RunGameFrame
 * @sa SV_Frame
 */
int SV_RunGameFrameThread (void *data)
{
	do {
		TH_MutexLock(svs.serverMutex);
		TH_MutexCondWait(svs.serverMutex, svs.gameFrameCond);
		SV_RunGameFrame();
		TH_MutexUnlock(svs.serverMutex);
	} while (!sv->endgame);

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
	sv->endgame = svs.ge->RunFrame();
	if (sv->state == ss_game_shutdown)
		sv->endgame = qtrue;
}

/**
 * @brief Init the game subsystem for a new map
 * @sa SV_ShutdownGameProgs
 */
void SV_InitGameProgs (void)
{
	game_import_t import;

	/* unload anything we have now */
	/*SV_ShutdownGameProgs();*/

	/* load a new game dll */
	import.BroadcastPrintf = SV_BroadcastPrintf;
	import.DPrintf = SV_dprintf;
	import.PlayerPrintf = SV_PlayerPrintf;
	import.Error = SV_error;

	import.Trace = SV_Trace;
	import.LinkEdict = SV_LinkEdict;
	import.UnlinkEdict = SV_UnlinkEdict;
	import.BoxEdicts = SV_AreaEdicts;
	import.TouchEdicts = SV_TouchEdicts;

	import.TestLine = SV_TestLine;
	import.TestLineWithEnt = SV_TestLineWithEnt;
	import.GrenadeTarget = Com_GrenadeTarget;

	import.MoveCalc = Grid_MoveCalc;
	import.MoveStore = Grid_MoveStore;
	import.MoveLength = Grid_MoveLength;
	import.MoveNext = Grid_MoveNext;
	import.GridFloor = Grid_Floor;
	import.GetTUsForDirection = Grid_GetTUsForDirection;
	import.GridFall = Grid_Fall;
	import.GridPosToVec = Grid_PosToVec;
	import.GridRecalcRouting = SV_RecalcRouting;

	import.GetVisibility = SV_GetVisibility;

	import.ModelIndex = SV_ModelIndex;

	import.SetInlineModelOrientation = SV_SetInlineModelOrientation;

	import.SetModel = SV_SetModel;

	import.ConfigString = SV_Configstring;

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

	Com_Printf("setting game random seed to %i\n", import.seed);

	/* import the server routing table */
	import.routingMap = sv->mapData.map;

	svs.ge = SV_GetGameAPI(&import);

	if (!svs.ge)
		Com_Error(ERR_DROP, "failed to load game library");
	if (svs.ge->apiversion != GAME_API_VERSION)
		Com_Error(ERR_DROP, "game is version %i, not %i", svs.ge->apiversion, GAME_API_VERSION);

	sv->gameSysPool = Mem_CreatePool("Server: Game system");

	svs.ge->Init();

	if (sv_threads->integer) {
		svs.gameFrameCond = SDL_CreateCond();
		svs.gameThread = SDL_CreateThread(SV_RunGameFrameThread, NULL);
	}
}
