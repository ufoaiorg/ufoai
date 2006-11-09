/**
 * @file qcommon.h
 * @brief definitions common between client and server, but not game lib
 */

/*
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

#ifndef QCOMMON_DEFINED
#define QCOMMON_DEFINED

#include "../game/q_shared.h"
#include "../game/game.h"

#define UFO_VERSION "2.0-RC6"

#define	BASEDIRNAME	"base"

#ifdef _WIN32

#ifdef DEBUG
#define BUILDSTRING "Win32 DEBUG"
#else							/* DEBUG */
#define BUILDSTRING "Win32 RELEASE"
#endif							/* DEBUG */

#ifdef _M_IX86
#define	CPUSTRING	"x86"
#elif defined _M_ALPHA			/* _M_IX86 */
#define	CPUSTRING	"AXP"
#else							/* _M_IX86 */
#define	CPUSTRING	"Unknown"
#endif							/* _M_IX86 */

#elif defined __linux__			/* _WIN32 */

#define BUILDSTRING "Linux"

#ifdef __i386__
#define CPUSTRING "i386"
#elif defined __alpha__			/* __i386__ */
#define CPUSTRING "axp"
#else							/* __i386__ */
#define CPUSTRING "Unknown"
#endif							/* __i386__ */

#elif defined __FreeBSD__			/* _WIN32 */

#define BUILDSTRING "FreeBSD"

#ifdef __i386__
#define CPUSTRING "i386"
#elif defined __alpha__			/* __i386__ */
#define CPUSTRING "axp"
#else							/* __i386__ */
#define CPUSTRING "Unknown"
#endif							/* __i386__ */

#elif defined __sun__			/* _WIN32 */

#define BUILDSTRING "Solaris"

#ifdef __i386__
#define CPUSTRING "i386"
#else							/* __i386__ */
#define CPUSTRING "sparc"
#endif							/* __i386__ */

#elif defined (__APPLE__) || defined (MACOSX)			/* _WIN32 */

#define BUILDSTRING "MacOSX"

#ifdef __i386__
#define CPUSTRING "i386"
#else							/* __i386__ */
#define CPUSTRING "sparc"
#endif							/* __i386__ */

#else							/* !_WIN32 */

#define BUILDSTRING "NON-WIN32"
#define	CPUSTRING	"NON-WIN32"

#endif							/* _WIN32 */

#ifndef DEFAULT_BASEDIR
# define DEFAULT_BASEDIR BASEDIRNAME
#endif							/* DEFAULT_BASEDIR */
#ifndef DEFAULT_LIBDIR
# define DEFAULT_LIBDIR DEFAULT_BASEDIR
#endif							/* DEFAULT_LIBDIR */

#include "common.h"

void Info_Print(char *s);

/*
==============================================================
PROTOCOL
==============================================================
*/

/* protocol.h -- communications protocols */

#define	PROTOCOL_VERSION	3

/*========================================= */

#define	PORT_MASTER	27900
#define	PORT_CLIENT	27901
#define	PORT_SERVER	27910

/* FIXME: this is the id master server */
#define IP_MASTER "192.246.40.37"

/*========================================= */

#define	UPDATE_BACKUP	16		/* copies of entity_state_t to keep buffered */
							/* must be power of two */
#define	UPDATE_MASK		(UPDATE_BACKUP-1)

/**
  * @brief server to client
  *
  * the svc_strings[] array in cl_parse.c should mirror this
  */
enum svc_ops_e {
	svc_bad,

	/* these ops are known to the game dll */
	svc_inventory,

	/* the rest are private to the client and server */
	svc_nop,
	svc_disconnect,
	svc_reconnect,
	svc_sound,					/* <see code> */
	svc_print,					/* [byte] id [string] null terminated string */
	svc_stufftext,				/* [string] stuffed into client's console buffer, should be \n terminated */
	svc_serverdata,				/* [long] protocol ... */
	svc_configstring,			/* [short] [string] */
	svc_spawnbaseline,
	svc_centerprint,			/* [string] to put in center of the screen */
	svc_playerinfo,				/* variable */
	svc_event					/* ... */
};

/*============================================== */

/**
  * @brief client to server
  */
enum clc_ops_e {
	clc_bad,
	clc_nop,
	clc_endround,
	clc_teaminfo,
	clc_action,
	clc_userinfo,				/* [[userinfo string] */
	clc_stringcmd				/* [string] message */
};


/*============================================== */

/* a sound without an ent or pos will be a local only sound */
#define	SND_VOLUME		(1<<0)	/* a byte */
#define	SND_ATTENUATION	(1<<1)	/* a byte */
#define	SND_POS			(1<<2)	/* three coordinates */
#define	SND_ENT			(1<<3)	/* a short 0-2: channel, 3-12: entity */
#define	SND_OFFSET		(1<<4)	/* a byte, msec offset from frame start */

#define DEFAULT_SOUND_PACKET_VOLUME	1.0
#define DEFAULT_SOUND_PACKET_ATTENUATION 1.0

/*============================================== */

#include "cmd.h"

#include "cvar.h"

#include "net_chan.h"

#include "cmodel.h"

#include "filesys.h"

#include "scripts.h"

/*
==============================================================
MISC
==============================================================
*/


#define	ERR_FATAL	0			/* exit the entire game with a popup window */
#define	ERR_DROP	1			/* print to console and disconnect from game */
#define	ERR_QUIT	2			/* not an error, just a normal exit */

#define	EXEC_NOW	0			/* don't return until completed */
#define	EXEC_INSERT	1			/* insert at current position, but don't run yet */
#define	EXEC_APPEND	2			/* add to end of the command buffer */

#define	PRINT_ALL		0
#define PRINT_DEVELOPER	1		/* only print when "developer 1" */

void Com_BeginRedirect(int target, char *buffer, int buffersize, void (*flush) (int, char *));
void Com_EndRedirect(void);
void Com_Printf(char *msg, ...) __attribute__((format(printf, 1, 2)));
void Com_DPrintf(char *msg, ...) __attribute__((format(printf, 1, 2)));
void Com_Error(int code, char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void Com_Drop(void);
void Com_Quit(void);

int Com_ServerState(void);		/* this should have just been a cvar... */
void Com_SetServerState(int state);

unsigned Com_BlockChecksum(void *buffer, int length);
byte COM_BlockSequenceCRCByte(byte * base, int length, int sequence);

extern cvar_t *developer;
extern cvar_t *dedicated;
extern cvar_t *host_speeds;
extern cvar_t *log_stats;
extern cvar_t *sv_maxclients;
extern cvar_t *sv_reaction_leftover;
extern cvar_t *sv_shot_origin;

extern FILE *log_stats_file;

/* host_speeds times */
extern int time_before_game;
extern int time_after_game;
extern int time_before_ref;
extern int time_after_ref;

extern csi_t csi;

extern char map_entitystring[MAX_MAP_ENTSTRING];


#include "mem.h"

void Qcommon_LocaleInit(void);
void Qcommon_Init(int argc, char **argv);
float Qcommon_Frame(int msec);
void Qcommon_Shutdown(void);
qboolean Qcommon_ServerActive(void);

#define NUMVERTEXNORMALS	162
extern vec3_t bytedirs[NUMVERTEXNORMALS];

/* this is in the client code, but can be used for debugging from server */
void SCR_DebugGraph(float value, int color);

/*
==============================================================
NON-PORTABLE SYSTEM SERVICES
==============================================================
*/

void Sys_Init(void);
void Sys_NormPath(char *path);
void Sys_Sleep(int milliseconds);
void Sys_AppActivate(void);

void Sys_UnloadGame(void);
game_export_t *Sys_GetGameAPI(game_import_t * parms);

/* loads the game dll and calls the api init function */

char *Sys_ConsoleInput(void);
void Sys_ConsoleOutput(char *string);
void Sys_SendKeyEvents(void);
void Sys_Error(char *error, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Sys_Quit(void);
char *Sys_GetClipboardData(void);
char *Sys_GetHomeDirectory(void);

/*
==============================================================
CLIENT / SERVER SYSTEMS
==============================================================
*/

void CL_Init(void);
void CL_Drop(void);
void CL_Shutdown(void);
void CL_Frame(int msec);
void CL_ParseClientData(char *type, char *name, char **text);
void Con_Print(char *text);
void SCR_BeginLoadingPlaque(void);
void MN_PrecacheMenus(void);

void SV_Init(void);
void SV_Shutdown(char *finalmsg, qboolean reconnect);
void SV_Frame(int msec);

#endif							/* QCOMMON_DEFINED */
