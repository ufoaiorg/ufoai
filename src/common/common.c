/**
 * @file common.c
 * @brief Misc functions used in client and server
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

#include "common.h"
#include "../server/server.h"
#include "../shared/parse.h"
#include <setjmp.h>

#define	MAXPRINTMSG	4096
#define MAX_NUM_ARGVS	50

csi_t csi;

static int com_argc;
static const char *com_argv[MAX_NUM_ARGVS + 1];

static jmp_buf abortframe; /* an ERR_DROP occured, exit the entire frame */

cvar_t *s_language;
cvar_t *developer;
cvar_t *http_proxy;
cvar_t *http_timeout;
static cvar_t *logfile_active; /* 1 = buffer log, 2 = flush after each print */
cvar_t *sv_dedicated;
cvar_t *cl_maxfps;
cvar_t *sv_gametype;
cvar_t *masterserver_url;
cvar_t *port;
cvar_t* sys_priority;
cvar_t* sys_affinity;
cvar_t* sys_os;

static FILE *logfile;

struct memPool_s *com_aliasSysPool;
struct memPool_s *com_cmdSysPool;
struct memPool_s *com_cmodelSysPool;
struct memPool_s *com_cvarSysPool;
struct memPool_s *com_fileSysPool;
struct memPool_s *com_genericPool;
struct memPool_s *com_networkPool;

struct event {
	int when;
	event_func *func;
	event_check_func *check;
	event_clean_func *clean;
	void *data;
	struct event *next;
};

static struct event *event_queue = NULL;

#define TIMER_CHECK_INTERVAL 100
#define TIMER_CHECK_LAG 3
#define TIMER_LATENESS_HIGH 200
#define TIMER_LATENESS_LOW 50
#define TIMER_LATENESS_HISTORY 32

struct timer {
	cvar_t *min_freq;
	int interval;
	int recent_lateness[TIMER_LATENESS_HISTORY];
	int next_lateness;
	int total_lateness;
	int next_check;
	int checks_high;
	int checks_low;

	event_func *func;
	void *data;
};

/*
============================================================================
CLIENT / SERVER interactions
============================================================================
*/

static char *rd_buffer;
static unsigned int rd_buffersize;
static struct net_stream *rd_stream;

/**
 * @brief Redirect packets/output from server to client
 * @sa Com_EndRedirect
 *
 * This is used to redirect printf outputs for rcon commands
 */
void Com_BeginRedirect (struct net_stream *stream, char *buffer, int buffersize)
{
	if (!buffer || !buffersize)
		return;

	rd_stream = stream;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_buffer[0] = '\0';
}

/**
 * @brief End the redirection of packets/output
 * @sa Com_BeginRedirect
 */
void Com_EndRedirect (void)
{
	NET_OOB_Printf(rd_stream, "print\n%s", rd_buffer);

	rd_stream = NULL;
	rd_buffer = NULL;
	rd_buffersize = 0;
}

/**
 * @note Both client and server can use this, and it will output
 * to the appropriate place.
 */
void Com_vPrintf (const char *fmt, va_list ap)
{
	char msg[MAXPRINTMSG];

	Q_vsnprintf(msg, sizeof(msg), fmt, ap);

	/* redirect the output? */
	if (rd_buffer) {
		if ((strlen(msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			NET_OOB_Printf(rd_stream, "print\n%s", rd_buffer);
			rd_buffer[0] = '\0';
		}
		Q_strcat(rd_buffer, msg, sizeof(char) * rd_buffersize);
		return;
	}

#ifndef DEDICATED_ONLY
	Con_Print(msg);
#endif

	/* also echo to debugging console */
	Sys_ConsoleOutput(msg);

	/* logfile */
	if (logfile_active && logfile_active->integer) {
		char name[MAX_OSPATH];

		if (!logfile) {
			Com_sprintf(name, sizeof(name), "%s/ufoconsole.log", FS_Gamedir());
			if (logfile_active->integer > 2)
				logfile = fopen(name, "a");
			else
				logfile = fopen(name, "w");
		}
		if (logfile) {
			/* strip color codes */
			const char *output = msg;
			if (output[0] < 32)
				output++;
			fprintf(logfile, "%s", output);
		}
		if (logfile_active->integer > 1)
			fflush(logfile);	/* force it to save every time */
	}
}

void Com_Printf (const char* const fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Com_vPrintf(fmt, ap);
	va_end(ap);
}

/**
 * @brief A Com_Printf that only shows up if the "developer" cvar is set
 */
void Com_DPrintf (int level, const char *fmt, ...)
{
	level |= DEBUG_ALL;

	/* don't confuse non-developers with techie stuff... */
	if (!developer || !(developer->integer & level))
		return;
	else {
		va_list ap;

		va_start(ap, fmt);
		Com_vPrintf(fmt, ap);
		va_end(ap);
	}
}

/**
 * @note Both client and server can use this, and it will
 * do the appropriate things.
 */
void Com_Error (int code, const char *fmt, ...)
{
	va_list argptr;
	static char msg[MAXPRINTMSG];
	static qboolean recursive = qfalse;

	if (recursive)
		Sys_Error("recursive error after: %s", msg);
	recursive = qtrue;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	switch (code) {
	case ERR_DISCONNECT:
		Com_Printf("%s\n", msg);
		Cvar_Set("mn_afterdrop", "popup");
		CL_Drop();
		recursive = qfalse;
		Com_Drop();
	case ERR_DROP:
		Com_Printf("********************\nERROR: %s\n********************\n", msg);
		SV_Shutdown("Server crashed.", qfalse);
		CL_Drop();
		recursive = qfalse;
		Sys_Backtrace();
		Com_Drop();
	default:
		Com_Printf("%s\n", msg);
		SV_Shutdown("Server fatal crashed", qfalse);

		/* send an receive net messages a last time */
		NET_Wait(0);

		if (logfile) {
			fclose(logfile);
			logfile = NULL;
		}

		CL_Shutdown();
		Qcommon_Shutdown();
		Sys_Error("Shutdown");
	}
}

void Com_Drop (void)
{
	longjmp(abortframe, -1);
}

/**
 * Both client and server can use this, and it will
 * do the appropriate things.
 */
void Com_Quit (void)
{
#ifdef DEDICATED_ONLY
	Com_WriteConfigToFile("dedconfig.cfg");
#else
	Com_WriteConfigToFile("config.cfg");
#endif

	SV_Shutdown("Server quit.", qfalse);
	SV_Clear();
	CL_Shutdown();

	/* send an receive net messages a last time */
	NET_Wait(0);
	if (logfile) {
		fclose(logfile);
		logfile = NULL;
	}

	Sys_Quit();
}


/**
 * @brief Check whether we are the server or have a singleplayer tactical mission
 * @sa Com_SetServerState
 */
int Com_ServerState (void)
{
	return sv.state;
}

/**
 * @sa SV_SpawnServer
 * @sa Com_ServerState
 */
void Com_SetServerState (int state)
{
	Com_DPrintf(DEBUG_ENGINE, "Set server state to %i\n", state);
	if (state == ss_dead)
		SV_Shutdown("Server shutdown", qtrue);
	sv.state = state;
}

/**
 * @brief returns hash key for a string
 */
unsigned int Com_HashKey (const char *name, int hashsize)
{
	int i;
	unsigned int v;

	v = 0;
	for (i = 0; name[i]; i++) {
		const unsigned int c = name[i];
		v = (v + i) * 37 + tolower(c);	/* case insensitivity */
	}

	return v % hashsize;
}

/**
 * @brief Returns the script commandline argument count
 */
int Com_Argc (void)
{
	return com_argc;
}

/**
 * @brief Returns an argument of script commandline
 */
const char *Com_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

/**
 * @brief Reset com_argv entry to empty string
 * @param[in] arg Which argument in com_argv
 * @sa Com_InitArgv
 */
void Com_ClearArgv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}


void Com_InitArgv (int argc, const char **argv)
{
	int i;

	if (argc > MAX_NUM_ARGVS)
		Com_Error(ERR_FATAL, "argc > MAX_NUM_ARGVS");
	com_argc = argc;
	for (i = 0; i < argc; i++) {
		if (!argv[i] || strlen(argv[i]) >= MAX_TOKEN_CHARS)
			com_argv[i] = "";
		else
			com_argv[i] = argv[i];
	}
}

#define MACRO_CVAR_ID_LENGTH 6
/**
 * @brief Expands strings with cvar values that are dereferenced by a '*cvar'
 * @note There is an overflow check for cvars that also contain a '*cvar'
 * @sa Cmd_TokenizeString
 * @sa MN_GetReferenceString
 */
const char *Com_MacroExpandString (const char *text)
{
	int i, j, count, len;
	qboolean inquote;
	const char *scan;
	static char expanded[MAX_STRING_CHARS];
	const char *token, *start, *cvarvalue;
	char *pos;

	inquote = qfalse;
	scan = text;
	if (!text || !*text)
		return NULL;

	len = strlen(scan);
	if (len >= MAX_STRING_CHARS) {
		Com_Printf("Line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
		return NULL;
	}

	count = 0;
	memset(expanded, 0, sizeof(expanded));
	pos = expanded;

	/* also the \0 */
	assert(scan[len] == '\0');
	for (i = 0; i <= len; i++) {
		if (scan[i] == '"')
			inquote ^= 1;
		/* don't expand inside quotes */
		if (inquote || strncmp(&scan[i], "*cvar:", MACRO_CVAR_ID_LENGTH)) {
			*pos++ = scan[i];
			continue;
		}

		/* scan out the complete macro and only parse the cvar name */
		start = &scan[i + MACRO_CVAR_ID_LENGTH];
		token = Com_Parse(&start);
		if (!start)
			continue;

		/* skip the macro and the cvar name in the next loop */
		i += MACRO_CVAR_ID_LENGTH;
		i += strlen(token);
		i--;

		/* get the cvar value */
		cvarvalue = Cvar_GetString(token);
		if (!cvarvalue) {
			Com_Printf("Could not get cvar value for cvar: %s\n", token);
			return NULL;
		}

		j = strlen(cvarvalue);
		if (strlen(pos) + j >= MAX_STRING_CHARS) {
			Com_Printf("Expanded line exceeded %i chars, discarded.\n", MAX_STRING_CHARS);
			return NULL;
		}

		/* copy the cvar value into the target buffer */
		/* check for overflow is already done - so MAX_STRING_CHARS won't hurt here */
		Q_strncpyz(pos, cvarvalue, j + 1);
		pos += j;

		if (++count == 100) {
			Com_Printf("Macro expansion loop, discarded.\n");
			return NULL;
		}
	}

	if (inquote) {
		Com_Printf("Line has unmatched quote, discarded.\n");
		return NULL;
	}

	if (count)
		return expanded;
	else
		return NULL;
}

/**
 * @brief Console completion for command and variables
 * @sa Key_CompleteCommand
 * @sa Cmd_CompleteCommand
 * @sa Cvar_CompleteVariable
 * @param[in] s The string to complete
 * @param[out] target The target buffer of the completed command/cvar
 * @param[in] bufSize the target buffer size - might not be bigger than MAXCMDLINE
 * @param[out] pos The position in the buffer after command completion
 * @param[in] offset The input buffer position to put the completed command to
 */
qboolean Com_ConsoleCompleteCommand (const char *s, char *target, size_t bufSize, int *pos, int offset)
{
	const char *cmd = NULL, *cvar = NULL, *use = NULL;
	int cntCmd = 0, cntCvar = 0, cntParams = 0;
	char cmdLine[MAXCMDLINE] = "";
	char cmdBase[MAXCMDLINE] = "";
	qboolean append = qtrue;
	char *tmp;

	if (!s[0] || s[0] == ' ')
		return qfalse;

	else if (s[0] == '\\' || s[0] == '/') {
		/* maybe we are using the same buffers - and we want to keep the slashes */
		if (s == target)
			offset++;
		s++;
	}

	assert(bufSize <= MAXCMDLINE);
	assert(pos);

	/* don't try to search a command or cvar if we are already in the
	 * parameter stage */
	if (strstr(s, " ")) {
		Q_strncpyz(cmdLine, s, sizeof(cmdLine));
		/* remove the last whitespace */
		cmdLine[strlen(cmdLine) - 1] = '\0';

		tmp = cmdBase;
		while (*s != ' ')
			*tmp++ = *s++;
		/* get rid of the whitespace */
		s++;
		/* terminate the string at whitespace position to separate the cmd */
		*tmp = '\0';

		/* now strip away that part that is not yet completed */
		tmp = strrchr(cmdLine, ' ');
		if (tmp)
			*tmp = '\0';

		cntParams = Cmd_CompleteCommandParameters(cmdBase, s, &cmd);
		if (cntParams > 1)
			Com_Printf("\n");
		if (cmd) {
			/* append the found parameter */
			Q_strcat(cmdLine, " ", sizeof(cmdLine));
			Q_strcat(cmdLine, cmd, sizeof(cmdLine));
			append = qfalse;
			use = cmdLine;
		} else
			return qfalse;
	} else {
		/* Cmd_GenericCompleteFunction uses one static buffer for output, so backup one completion here if available */
		static char cmdBackup[MAX_QPATH] ;
		cntCmd = Cmd_CompleteCommand(s, &cmd);
		if (cmd)
			Q_strncpyz(cmdBackup, cmd, sizeof(cmdBackup));
		cntCvar = Cvar_CompleteVariable(s, &cvar);

		/* complete as much as possible, append only if one single match is found */
		if (cntCmd > 0 && !cntCvar) {
			use = cmd;
			if (cntCmd != 1)
				append = qfalse;
		} else if (!cntCmd && cntCvar > 0) {
			use = cvar;
			if (cntCvar != 1)
				append = qfalse;
		} else if (cmd && cvar) {
			int maxLength = min(strlen(cmdBackup),strlen(cvar));
			int idx = 0;
			/* try to find similar content of cvar and cmd match */
			Q_strncpyz(cmdLine,cmdBackup,sizeof(cmdLine));
			for (; idx < maxLength; idx++) {
				if (cmdBackup[idx] != cvar[idx]) {
					cmdLine[idx] = '\0';
					break;
				}
			}
			if (idx == maxLength)
				cmdLine[idx] = '\0';
			use = cmdLine;
			append = qfalse;
		}
	}

	if (use) {
		Q_strncpyz(&target[offset], use, bufSize - offset);
		*pos = strlen(target);
		if (append)
			target[(*pos)++] = ' ';
		target[*pos] = '\0';

		return qtrue;
	}

	return qfalse;
}

void Com_SetGameType (void)
{
	int i;

	for (i = 0; i < numGTs; i++) {
		const gametype_t *gt = &gts[i];
		if (!strncmp(gt->id, sv_gametype->string, MAX_VAR)) {
			int j;
			const cvarlist_t *list;
			if (sv_dedicated->integer)
				Com_Printf("set gametype to: %s\n", gt->id);
			for (j = 0, list = gt->cvars; j < gt->num_cvars; j++, list++) {
				Cvar_Set(list->name, list->value);
				if (sv_dedicated->integer)
					Com_Printf("  %s = %s\n", list->name, list->value);
			}
			/*Com_Printf("Make sure to restart the map if you switched during a game\n");*/
			break;
		}
	}

	if (i == numGTs)
		Com_Printf("Can't set the gametype - unknown value for cvar gametype: '%s'\n", sv_gametype->string);
}

static void Com_GameTypeList_f (void)
{
	int i;

	Com_Printf("Available gametypes:\n");
	for (i = 0; i < numGTs; i++) {
		int j;
		const gametype_t *gt = &gts[i];
		const cvarlist_t *list;

		Com_Printf("%s\n", gt->id);

		for (j = 0, list = gt->cvars; j < gt->num_cvars; j++, list++)
			Com_Printf("  %s = %s\n", list->name, list->value);
	}
}

#ifdef DEBUG
/**
 * @brief Prints some debugging help to the game console
 */
static void Com_DebugHelp_f (void)
{
	Cvar_PrintDebugCvars();

	Cmd_PrintDebugCommands();
}

/**
 * @brief Just throw a fatal error to test error shutdown procedures
 */
static void Com_DebugError_f (void)
{
	if (Cmd_Argc() == 3) {
		const char *errorType = Cmd_Argv(1);
		if (!strcmp(errorType, "ERR_DROP"))
			Com_Error(ERR_DROP, "%s", Cmd_Argv(2));
		else if (!strcmp(errorType, "ERR_FATAL"))
			Com_Error(ERR_FATAL, "%s", Cmd_Argv(2));
		else if (!strcmp(errorType, "ERR_DISCONNECT"))
			Com_Error(ERR_DISCONNECT, "%s", Cmd_Argv(2));
	}
	Com_Printf("Usage: %s <ERR_FATAL|ERR_DROP|ERR_DISCONNECT> <msg>\n", Cmd_Argv(0));
}
#endif


typedef struct debugLevel_s {
	const char *str;
	int debugLevel;
} debugLevel_t;

static const debugLevel_t debugLevels[] = {
	{"DEBUG_ALL", DEBUG_ALL},
	{"DEBUG_ENGINE", DEBUG_ENGINE},
	{"DEBUG_SHARED", DEBUG_SHARED},
	{"DEBUG_SYSTEM", DEBUG_SYSTEM},
	{"DEBUG_COMMANDS", DEBUG_COMMANDS},
	{"DEBUG_CLIENT", DEBUG_CLIENT},
	{"DEBUG_EVENTSYS", DEBUG_EVENTSYS},
	{"DEBUG_PATHING", DEBUG_PATHING},
	{"DEBUG_SERVER", DEBUG_SERVER},
	{"DEBUG_GAME", DEBUG_GAME},
	{"DEBUG_RENDERER", DEBUG_RENDERER},
	{"DEBUG_SOUND", DEBUG_SOUND},

	{NULL, 0}
};

static void Com_DeveloperSet_f (void)
{
	int oldValue = Cvar_GetInteger("developer");
	int newValue = oldValue;
	int i = 0;

	if (Cmd_Argc() == 2) {
		const char *debugLevel = Cmd_Argv(1);
		while (debugLevels[i].str) {
			if (!strcmp(debugLevel, debugLevels[i].str)) {
				if (oldValue & debugLevels[i].debugLevel)
					newValue &= ~debugLevels[i].debugLevel;
				else
					newValue |= debugLevels[i].debugLevel;
				break;
			}
			i++;
		}
		if (!debugLevels[i].str) {
			Com_Printf("No valid debug mode parameter\n");
			return;
		}
		Cvar_SetValue("developer", newValue);
		Com_Printf("Currently selected debug print levels\n");
		i = 0;
		while (debugLevels[i].str) {
			if (newValue & debugLevels[i].debugLevel)
				Com_Printf("* %s\n", debugLevels[i].str);
			i++;
		}
	} else {
		Com_Printf("Usage: %s <debug_level>\n", Cmd_Argv(0));
		Com_Printf("  valid debug_levels are:\n");
		while (debugLevels[i].str) {
			Com_Printf("  * %s\n", debugLevels[i].str);
			i++;
		}
	}
}

#ifndef DEDICATED_ONLY
/**
 * @brief Watches that the cvar cl_maxfps is never getting lower than 10
 */
static qboolean Com_CvarCheckMaxFPS (cvar_t *cvar)
{
	/* don't allow setting maxfps too low (or game could stop responding) */
	return Cvar_AssertValue(cvar, 10, 1000, qtrue);
}
#endif

/**
 * @sa Key_WriteBindings
 */
void Com_WriteConfigToFile (const char *filename)
{
	qFILE f;

	FS_OpenFile(filename, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("Couldn't write %s.\n", filename);
		return;
	}

	FS_Printf(&f, "// generated by ufo, do not modify\n");
	Cvar_WriteVariables(&f);
	FS_CloseFile(&f);
	Com_Printf("Wrote %s.\n", filename);
}


/**
 * @brief Write the config file to a specific name
 */
static void Com_WriteConfig_f (void)
{
	char filename[MAX_QPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz(filename, Cmd_Argv(1), sizeof(filename));
	Com_DefaultExtension(filename, sizeof(filename), ".cfg");
	Com_WriteConfigToFile(filename);
}

static void Cbuf_Execute_timer (int now, void *data)
{
	Cbuf_Execute();
}

/**
 * @brief Init function
 * @sa Com_ParseScripts
 * @sa Qcommon_Shutdown
 * @sa Sys_Init
 * @sa CL_Init
 */
void Qcommon_Init (int argc, const char **argv)
{
	char *s;

	Sys_InitSignals();

	/* random seed */
	srand(time(NULL));

	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");
	com_networkPool = Mem_CreatePool("Network");

	if (setjmp(abortframe))
		Sys_Error("Error during initialization");

	memset(&csi, 0, sizeof(csi));

	/* prepare enough of the subsystems to handle
	 * cvar and command buffer management */
	Com_InitArgv(argc, argv);

	Swap_Init();
	Cbuf_Init();

	Cmd_Init();
	Cvar_Init();

	Key_Init();

	/* we need to add the early commands twice, because
	 * a basedir needs to be set before executing
	 * config files, but we want other parms to override
	 * the settings of the config files */
	Cbuf_AddEarlyCommands(qfalse);
	Cbuf_Execute();

	FS_InitFilesystem(qtrue);

	Cbuf_AddText("exec default.cfg\n");
#ifdef DEDICATED_ONLY
	Cbuf_AddText("exec dedconfig.cfg\n");
#else
	Cbuf_AddText("exec config.cfg\n");
#endif

	Cbuf_AddEarlyCommands(qtrue);
	Cbuf_Execute();

	/* init commands and vars */
	Cmd_AddCommand("saveconfig", Com_WriteConfig_f, "Write the configuration to file");
	Cmd_AddCommand("gametypelist", Com_GameTypeList_f, "List all available multiplayer game types");
#ifdef DEBUG
	Cmd_AddCommand("debug_help", Com_DebugHelp_f, "Show some debugging help");
	Cmd_AddCommand("debug_error", Com_DebugError_f, "Just throw a fatal error to test error shutdown procedures");
#endif
	Cmd_AddCommand("setdeveloper", Com_DeveloperSet_f, "Set the developer cvar to only get the debug output you want");

	developer = Cvar_Get("developer", "0", 0, "Activate developer output to logfile and gameconsole");
	logfile_active = Cvar_Get("logfile", "1", 0, "0 = deactivate logfile, 1 = write normal logfile, 2 = flush on every new line");
	sv_gametype = Cvar_Get("sv_gametype", "1on1", CVAR_ARCHIVE | CVAR_SERVERINFO, "Sets the multiplayer gametype - see gametypelist command for a list of all gametypes");
	http_proxy = Cvar_Get("http_proxy", "", CVAR_ARCHIVE, "Use this proxy for http transfers");
	http_timeout = Cvar_Get("http_timeout", "3", CVAR_ARCHIVE, "Http connection timeout");
	port = Cvar_Get("port", DOUBLEQUOTE(PORT_SERVER), CVAR_NOSET, NULL);
	masterserver_url = Cvar_Get("masterserver_url", MASTER_SERVER, CVAR_ARCHIVE, "URL of UFO:AI masterserver");
#ifdef DEDICATED_ONLY
	sv_dedicated = Cvar_Get("sv_dedicated", "1", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");
#else
	sv_dedicated = Cvar_Get("sv_dedicated", "0", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");

	/* set this to false for client - otherwise Qcommon_Frame would set the initial values to multiplayer */
	sv_gametype->modified = qfalse;

	s_language = Cvar_Get("s_language", "", CVAR_ARCHIVE, "Game language - full language string e.g. en_EN.UTF-8");
	s_language->modified = qfalse;
	cl_maxfps = Cvar_Get("cl_maxfps", "50", CVAR_ARCHIVE, NULL);
	Cvar_SetCheckFunction("cl_maxfps", Com_CvarCheckMaxFPS);
#endif

	s = va("UFO: Alien Invasion %s %s %s %s", UFO_VERSION, CPUSTRING, __DATE__, BUILDSTRING);
	Cvar_Get("version", s, CVAR_NOSET, "Full version string");
	Cvar_Get("ver", UFO_VERSION, CVAR_SERVERINFO | CVAR_NOSET, "Version number");

	if (sv_dedicated->integer)
		Cmd_AddCommand("quit", Com_Quit, "Quits the game");

	Mem_Init();
	Sys_Init();

	NET_Init();

	curl_global_init(CURL_GLOBAL_NOTHING);
	Com_Printf("%s initialized.\n", curl_version());

	SV_Init();

	/* e.g. init the client hunk that is used in script parsing */
	CL_Init();

	Com_ParseScripts(sv_dedicated->integer);
#ifndef DEDICATED_ONLY
	Cbuf_AddText("exec keys.cfg\n");
#endif

	if (!sv_dedicated->integer)
		Cbuf_AddText("init\n");
	else
		Cbuf_AddText("dedicated_start\n");
	Cbuf_Execute();

	FS_ExecAutoexec();

	/* add + commands from command line
	 * if the user didn't give any commands, run default action */
	if (Cbuf_AddLateCommands()) {
		/* the user asked for something explicit
		 * so drop the loading plaque */
		SCR_EndLoadingPlaque();
	}

#ifndef DEDICATED_ONLY
	CL_InitAfter();
#endif

	/* Check memory integrity */
	Mem_CheckGlobalIntegrity();

	/* Touch memory */
	Mem_TouchGlobal();

#ifndef DEDICATED_ONLY
	if (!sv_dedicated->integer) {
		Schedule_Timer(cl_maxfps, &CL_Frame, NULL);
		Schedule_Timer(Cvar_Get("cl_slowfreq", "10", 0, NULL), &CL_SlowFrame, NULL);
	}

	/* now hide the console */
	Sys_ShowConsole(qfalse);
#endif

	Schedule_Timer(Cvar_Get("sv_freq", "10", CVAR_NOSET, NULL), &SV_Frame, NULL);

	/** @todo This line wants to be removed */
	Schedule_Timer(Cvar_Get("cbuf_freq", "10", 0, NULL), &Cbuf_Execute_timer, NULL);

	Com_Printf("====== UFO Initialized ======\n\n");
}

static void tick_timer (int now, void *data)
{
	struct timer *timer = data;
	int old_interval = timer->interval;

	/* Compute and store the lateness, updating the total */
	const int lateness = Sys_Milliseconds() - now;
	timer->total_lateness -= timer->recent_lateness[timer->next_lateness];
	timer->recent_lateness[timer->next_lateness] = lateness;
	timer->total_lateness += lateness;
	timer->next_lateness++;
	timer->next_lateness %= TIMER_LATENESS_HISTORY;

	/* Is it time to check the mean yet? */
	timer->next_check--;
	if (timer->next_check <= 0) {
		const int mean = timer->total_lateness / TIMER_LATENESS_HISTORY;

		/* We use a saturating counter to damp the adjustment */

		/* When we stay above the high water mark, increase the interval */
		if (mean > TIMER_LATENESS_HIGH)
			timer->checks_high = min(TIMER_CHECK_LAG, timer->checks_high + 1);
		else
			timer->checks_high = max(0, timer->checks_high - 1);

		if (timer->checks_high > TIMER_CHECK_LAG)
			timer->interval += 2;

		/* When we stay below the low water mark, decrease the interval */
		if (mean < TIMER_LATENESS_LOW)
			timer->checks_low = min(TIMER_CHECK_LAG, timer->checks_high + 1);
		else
			timer->checks_low = max(0, timer->checks_low - 1);

		if (timer->checks_low > TIMER_CHECK_LAG)
			timer->interval -= 1;

		/* Note that we slow the timer more quickly than we speed it up,
		 * so it should tend to settle down in the vicinity of the low
		 * water mark */

		timer->next_check = TIMER_CHECK_INTERVAL;
	}

	timer->interval = max(timer->interval, 1000 / timer->min_freq->integer);

	if (timer->interval != old_interval)
		Com_DPrintf(DEBUG_ENGINE, "Adjusted timer on %s to interval %d\n", timer->min_freq->name, timer->interval);

	if (setjmp(abortframe) == 0)
		timer->func(now, timer->data);

	/* We correct for the lateness of this frame. We do not correct for
	 * the time consumed by this frame - that's billed to the lateness
	 * of future frames (so that the automagic slowdown can work) */
	Schedule_Event(now + lateness + timer->interval, &tick_timer, NULL, NULL, timer);
}

void Schedule_Timer (cvar_t *freq, event_func *func, void *data)
{
	struct timer *timer = Mem_PoolAlloc(sizeof(*timer), com_genericPool, 0);
	int i;
	timer->min_freq = freq;
	timer->interval = 1000 / freq->integer;
	timer->next_lateness = 0;
	timer->total_lateness = 0;
	timer->next_check = TIMER_CHECK_INTERVAL;
	timer->checks_high = 0;
	timer->checks_low = 0;
	timer->func = func;
	timer->data = data;
	for (i = 0; i < TIMER_LATENESS_HISTORY; i++)
		timer->recent_lateness[i] = 0;

	Schedule_Event(Sys_Milliseconds() + timer->interval, &tick_timer, NULL, NULL, timer);
}

/**
 * @brief Schedules an event to run on or after the given time, and when its check function returns true.
 * @param when The earliest time the event can run
 * @param func The function to call when running the event
 * @param check A function that should return true when the event is safe to run.
 *  It should have no side-effects, as it might be called many times.
 * @param clean A function that should cleanup any memory allocated
 *  for the event in the case that it is not executed.  Either this
 *  function or func will be called, but never both.
 * @param data Arbitrary data to be passed to the check and event functions.
 */
void Schedule_Event (int when, event_func *func, event_check_func *check, event_clean_func *clean, void *data)
{
	struct event *event = Mem_PoolAlloc(sizeof(*event), com_genericPool, 0);
	event->when = when;
	event->func = func;
	event->check = check;
	event->clean = clean;
	event->data = data;

	if (!event_queue || event_queue->when > when) {
		event->next = event_queue;
		event_queue = event;
	} else {
		struct event *e = event_queue;
		while (e->next && e->next->when <= when)
			e = e->next;
		event->next = e->next;
		e->next = event;
	}

#ifdef DEBUG
	for (event = event_queue; event && event->next; event = event->next)
		if (event->when > event->next->when)
			abort();
#endif
}

/**
 * @brief Finds and returns the first event in the event_queue that is due.
 *  If the event has a check function, we check to see if the event can be run now, and skip it if not (even if it is due).
 * @return Returns a pointer to the event, NULL if none found.
 */
static struct event* Dequeue_Event (int now)
{
	struct event *event = event_queue;
	struct event *prev = NULL;

	while (event && event->when <= now) {
		if (event->check == NULL || event->check(now, event->data)) {
			if (prev) {
				prev->next = event->next;
			} else {
				event_queue = event->next;
			}
			return event;
		}
		prev = event;
		event = event->next;
	}
	return NULL;
}

/**
 * @brief Filters every event in the queue using the given function.
 *  Keeps all events for which the function returns true.
 * @param filter Pointer to the filter function.
 *  When called with event info, it should return true if the event is to be kept.
 */
void CL_FilterEventQueue (event_filter *filter)
{
	struct event *event = event_queue;
	struct event *prev = NULL;

	assert(filter);

	while (event) {
		qboolean keep = filter(event->when, event->func, event->check, event->data);
		struct event *freeme = event;

		if (keep) {
			prev = event;
			event = event->next;
			continue;
		}

		/* keep == qfalse */
		if (prev) {
			event = prev->next = event->next;
		} else {
			event = event_queue = event->next;
		}
		if (freeme->clean != NULL)
			freeme->clean(freeme->data);
		Mem_Free(freeme);
	}
}

/**
 * @brief This is the function that is called directly from main()
 * @sa main
 * @sa Qcommon_Init
 * @sa Qcommon_Shutdown
 * @sa SV_Frame
 * @sa CL_Frame
 */
void Qcommon_Frame (void)
{
	int time_to_next;
	struct event *event;

	/* an ERR_DROP was thrown */
	if (setjmp(abortframe))
		return;

	/* If the next event is due... */
	event = Dequeue_Event(Sys_Milliseconds());
	if (event) {
		if (setjmp(abortframe)) {
			Mem_Free(event);
			return;
		}

		/* Dispatch the event */
		event->func(event->when, event->data);

		if (setjmp(abortframe))
			return;

		Mem_Free(event);
	}

	/* Now we spend time_to_next milliseconds working on whatever
	 * IO is ready (but always try at least once, to make sure IO
	 * doesn't stall) */
	do {
		const char *s;

		/** @todo This shouldn't exist */
		do {
			s = Sys_ConsoleInput();
			if (s)
				Cbuf_AddText(va("%s\n", s));
		} while (s);

		time_to_next = event_queue ? (event_queue->when - Sys_Milliseconds()) : 1000;
		if (time_to_next < 0)
			time_to_next = 0;

		NET_Wait(time_to_next);
	} while (time_to_next > 0);
}

/**
 * @brief
 * @sa Qcommon_Init
 * @sa Sys_Quit
 * @note Don't call anything that depends on cvars, command system, or any other
 * subsystem that is allocated in the mem pools and maybe already freed
 */
void Qcommon_Shutdown (void)
{
	HTTP_Cleanup();

	Cmd_Shutdown();
	Cvar_Shutdown();
	Mem_Shutdown();
}

/*
============================================================
LINKED LIST
============================================================
*/

/**
 * @brief Adds an entry to a new or to an already existing linked list
 * @sa LIST_AddString
 * @sa LIST_Remove
 * @return Returns a pointer to the data that has been added, wrapped in a linkedList_t
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
linkedList_t* LIST_Add (linkedList_t** listDest, const byte* data, size_t length)
{
	linkedList_t *newEntry;
	linkedList_t *list;

	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest) {
		*listDest = (linkedList_t*)Mem_PoolAlloc(sizeof(linkedList_t), com_genericPool, 0);
		(*listDest)->data = (byte*)Mem_PoolAlloc(length, com_genericPool, 0);
		memcpy(((*listDest)->data), data, length);
		(*listDest)->next = NULL; /* not really needed - but for better readability */
		return *listDest;
	} else
		list = *listDest;

	while (list->next)
		list = list->next;

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(linkedList_t), com_genericPool, 0);
	list->next = newEntry;
	newEntry->data = (byte*)Mem_PoolAlloc(length, com_genericPool, 0);
	memcpy(newEntry->data, data, length);
	newEntry->next = NULL; /* not really needed - but for better readability */

	return newEntry;
}

/**
 * @brief Searches for the first occurrence of a given string
 * @return true if the string is found, otherwise false
 * @note if string is @c NULL, the function returns false
 * @sa LIST_AddString
 */
const linkedList_t* LIST_ContainsString (const linkedList_t* list, const char* string)
{
	assert(list);

	while ((string != NULL) && (list != NULL)) {
		if (!strcmp((const char*)list->data, string))
			return list;
		list = list->next;
	}

	return NULL;
}

/**
 * @brief Adds an string to a new or to an already existing linked list
 * @sa LIST_Add
 * @sa LIST_Remove
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_AddString (linkedList_t** listDest, const char* data)
{
	linkedList_t *newEntry;
	linkedList_t *list;

	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest) {
		*listDest = (linkedList_t*)Mem_PoolAlloc(sizeof(**listDest), com_genericPool, 0);
		(*listDest)->data = (byte*)Mem_PoolStrDup(data, com_genericPool, 0);
		(*listDest)->next = NULL; /* not really needed - but for better readability */
		return;
	} else
		list = *listDest;

	while (list->next)
		list = list->next;

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
	list->next = newEntry;
	newEntry->data = (byte*)Mem_StrDup(data);
	newEntry->next = NULL; /* not really needed - but for better readability */
}

/**
 * @brief Adds just a pointer to a new or to an already existing linked list
 * @sa LIST_Add
 * @sa LIST_Remove
 * @todo Optimize this to not allocate memory for every entry - but use a hunk
 */
void LIST_AddPointer (linkedList_t** listDest, void* data)
{
	linkedList_t *newEntry;
	linkedList_t *list;

	assert(listDest);
	assert(data);

	/* create the list */
	if (!*listDest) {
		*listDest = (linkedList_t*)Mem_PoolAlloc(sizeof(**listDest), com_genericPool, 0);
		(*listDest)->data = data;
		(*listDest)->ptr = qtrue;
		(*listDest)->next = NULL; /* not really needed - but for better readability */
		return;
	} else
		list = *listDest;

	while (list->next)
		list = list->next;

	newEntry = (linkedList_t*)Mem_PoolAlloc(sizeof(*newEntry), com_genericPool, 0);
	list->next = newEntry;
	newEntry->data = data;
	newEntry->ptr = qtrue;
	newEntry->next = NULL; /* not really needed - but for better readability */
}

/**
 * @brief Removes one entry from the linked list
 * @sa LIST_AddString
 * @sa LIST_Add
 * @sa LIST_AddPointer
 * @sa LIST_Delete
 */
void LIST_Remove (linkedList_t **list, linkedList_t *entry)
{
	linkedList_t* prev, *listPos;

	assert(list);
	assert(entry);

	prev = *list;
	listPos = *list;

	/* first entry */
	if (*list == entry) {
		if (!(*list)->ptr)
			Mem_Free((*list)->data);
		listPos = (*list)->next;
		Mem_Free(*list);
		*list = listPos;
	} else {
		while (listPos) {
			if (listPos == entry) {
				prev->next = listPos->next;
				if (!listPos->ptr)
					Mem_Free(listPos->data);
				Mem_Free(listPos);
				break;
			}
			prev = listPos;
			listPos = listPos->next;
		}
	}
}

/**
 * @sa LIST_Add
 * @sa LIST_Remove
 */
void LIST_Delete (linkedList_t **list)
{
	linkedList_t *next;
	linkedList_t *l = *list;

	while (l) {
		next = l->next;
		if (!l->ptr)
			Mem_Free(l->data);
		Mem_Free(l);
		l = next;
	}
	*list = NULL;
}

/**
 * @sa LIST_Add
 * @sa LIST_Remove
 */
int LIST_Count (const linkedList_t *list)
{
	const linkedList_t *l = list;
	int count = 0;

	while (l) {
		count++;
		l = l->next;
	}
	return count;
}

/**
 * @brief Get an entry of a linked list by its index in the list.
 * @param[in] list Linked list to get the entry from.
 * @param[in] index The index of the entry in the linked list.
 * @return A void pointer of the content in the list-entry.
 */
void *LIST_GetByIdx (linkedList_t *list, int index)
{
	int i;
	const int count = LIST_Count(list);

	if (!count || !list)
		return NULL;

	if (index >= count || index < 0)
		return NULL;

	i = 0;
	while (list) {
		if (i == index)
			return (void *)list->data;
		i++;
		list = list->next;
	}

	return NULL;
}
