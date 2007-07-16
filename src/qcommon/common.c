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

#include "qcommon.h"
#include "../server/server.h"
#include <setjmp.h>

#if defined DEBUG && defined _MSC_VER >= 1300
#include <intrin.h>
#endif

#define	MAXPRINTMSG	4096
#define MAX_NUM_ARGVS	50

csi_t csi;

static int com_argc;
static const char *com_argv[MAX_NUM_ARGVS + 1];

jmp_buf abortframe;				/* an ERR_DROP occured, exit the entire frame */

cvar_t *s_sleep;
cvar_t *s_language;
cvar_t *developer;
cvar_t *timescale;
cvar_t *fixedtime;
cvar_t *logfile_active;			/* 1 = buffer log, 2 = flush after each print */
cvar_t *dedicated;
cvar_t *cl_maxfps;
cvar_t *teamnum;
cvar_t *gametype;
static FILE *logfile;

static int server_state;

struct memPool_s *com_aliasSysPool;
struct memPool_s *com_cmdSysPool;
struct memPool_s *com_cmodelSysPool;
struct memPool_s *com_cvarSysPool;
struct memPool_s *com_fileSysPool;
struct memPool_s *com_genericPool;

struct event {
  int when;
  event_func *func;
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

static int rd_target; /**< redirect the output to e.g. an rcon user */
static char *rd_buffer;
static unsigned int rd_buffersize;
static void (*rd_flush) (int target, char *buffer);

/**
 * @brief Redirect packets/ouput from server to client
 * @sa Com_EndRedirect
 *
 * This is used to redirect printf outputs for rcon commands
 */
void Com_BeginRedirect (int target, char *buffer, int buffersize, void (*flush) (int, char *))
{
	if (!target || !buffer || !buffersize || !flush)
		return;
	rd_target = target;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

/**
 * @brief End the redirection of packets/output
 * @sa Com_BeginRedirect
 */
void Com_EndRedirect (void)
{
	rd_flush(rd_target, rd_buffer);

	rd_target = 0;
	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}

/**
 * @brief
 *
 * Both client and server can use this, and it will output
 * to the apropriate place.
 */
void Com_Printf (const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	msg[sizeof(msg)-1] = 0;

	/* redirect the output? */
	if (rd_target) {
		if ((strlen(msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_target, rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, msg, sizeof(char) * rd_buffersize);
		return;
	}

	Con_Print(msg);

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
		if (logfile)
			fprintf(logfile, "%s", msg);
		if (logfile_active->integer > 1)
			fflush(logfile);	/* force it to save every time */
	}
}


/**
 * @brief
 *
 * A Com_Printf that only shows up if the "developer" cvar is set
 */
void Com_DPrintf (const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	/* don't confuse non-developers with techie stuff... */
	if (!developer || !developer->integer)
		return;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, MAXPRINTMSG, fmt, argptr);
	va_end(argptr);

	msg[sizeof(msg)-1] = 0;

	Com_Printf("%s", msg);
}


/**
 * @brief
 *
 * Both client and server can use this, and it will
 * do the apropriate things.
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

	msg[sizeof(msg)-1] = 0;

	switch (code) {
	case ERR_DISCONNECT:
		CL_Drop();
		recursive = qfalse;
		longjmp(abortframe, -1);
		break;
	case ERR_DROP:
		Com_Printf("********************\nERROR: %s\n********************\n", msg);
		SV_Shutdown(va("Server crashed: %s\n", msg), qfalse);
		CL_Drop();
		recursive = qfalse;
		longjmp(abortframe, -1);
		break;
	default:
		SV_Shutdown(va("Server fatal crashed: %s\n", msg), qfalse);
		CL_Shutdown();
	}

	wait_for_net(0);

	if (logfile) {
		fclose(logfile);
		logfile = NULL;
	}

	Sys_Error("%s", msg);
}


/**
 * @brief
 */
void Com_Drop (void)
{
	SV_Shutdown("Server disconnected\n", qfalse);
	CL_Drop();
	longjmp(abortframe, -1);
}


/**
 * @brief
 *
 * Both client and server can use this, and it will
 * do the apropriate things.
 */
void Com_Quit (void)
{
	SV_Shutdown("Server quit\n", qfalse);
	SV_Clear();
#ifndef DEDICATED_ONLY
	CL_Shutdown();
#else
	Cvar_WriteVariables(va("%s/config.cfg", FS_Gamedir()));
#endif
	wait_for_net(0);
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
	return server_state;
}

/**
 * @brief
 * @sa SV_SpawnServer
 * @sa Com_ServerState
 */
void Com_SetServerState (int state)
{
	server_state = state;
}

/*=========================================================================== */

/**
 * @brief
 */
void SZ_Init (sizebuf_t * buf, byte * data, int length)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

/**
 * @brief
 */
void SZ_Clear (sizebuf_t * buf)
{
	buf->cursize = 0;
	buf->overflowed = qfalse;
}

/**
 * @brief
 */
void *SZ_GetSpace (sizebuf_t * buf, int length)
{
	void *data;

	if (buf->cursize + length > buf->maxsize) {
		if (!buf->allowoverflow)
			Com_Error(ERR_FATAL, "SZ_GetSpace: overflow without allowoverflow set");

		if (length > buf->maxsize)
			Com_Error(ERR_FATAL, "SZ_GetSpace: %i is > full buffer size", length);

		Com_Printf("SZ_GetSpace: overflow\n");
		SZ_Clear(buf);
		buf->overflowed = qtrue;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

/**
 * @brief
 */
void SZ_Write (sizebuf_t * buf, const void *data, int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

/**
 * @brief
 */
void SZ_Print (sizebuf_t * buf, const char *data)
{
	int len;

	len = strlen(data) + 1;

	if (buf->cursize) {
		if (buf->data[buf->cursize - 1])
			memcpy((byte *) SZ_GetSpace(buf, len), data, len);	/* no trailing 0 */
		else
			memcpy((byte *) SZ_GetSpace(buf, len - 1) - 1, data, len);	/* write over trailing 0 */
	} else
		memcpy((byte *) SZ_GetSpace(buf, len), data, len);
}


/*============================================================================ */

/**
 * @brief returns hash key for a string
 */
unsigned int Com_HashKey (const char *name, int hashsize)
{
	int i;
	unsigned int v;
	unsigned int c;

	v = 0;
	for (i = 0; name[i]; i++) {
		c = name[i];
		v = (v + i) * 37 + tolower( c );	/* case insensitivity */
	}

	return v % hashsize;
}

/**
 * @brief Checks whether a given commandline paramter was set
 * @param[in] parm Check for this commandline parameter
 * @return the position (1 to argc-1) in the program's argument list
 * where the given parameter apears, or 0 if not present
 * @sa COM_InitArgv
 */
int COM_CheckParm (const char *parm)
{
	int i;

	for (i = 1; i < com_argc; i++) {
		if (!com_argv[i])
			continue;               /* NEXTSTEP sometimes clears appkit vars. */
		if (!Q_strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

/**
 * @brief Returns the script commandline argument count
 */
int COM_Argc (void)
{
	return com_argc;
}

/**
 * @brief Returns an argument of script commandline
 */
const char *COM_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return "";
	return com_argv[arg];
}

/**
 * @brief Reset com_argv entry to empty string
 * @param[in] arg Which argument in com_argv
 * @sa COM_InitArgv
 * @sa COM_CheckParm
 * @sa COM_AddParm
 */
void COM_ClearArgv (int arg)
{
	if (arg < 0 || arg >= com_argc || !com_argv[arg])
		return;
	com_argv[arg] = "";
}


/**
 * @brief
 * @sa COM_CheckParm
 */
void COM_InitArgv (int argc, const char **argv)
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

/**
 * @brief Adds the given string at the end of the current argument list
 * @sa COM_InitArgv
 */
void COM_AddParm (const char *parm)
{
	if (com_argc == MAX_NUM_ARGVS)
		Com_Error(ERR_FATAL, "COM_AddParm: MAX_NUM)ARGS");
	com_argv[com_argc++] = parm;
}

/**
 * @brief
 */
void Info_Print (char *s)
{
	char key[512];
	char value[512];
	char *o;
	int l;

	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20) {
			memset(o, ' ', 20 - l);
			key[20] = 0;
		} else
			*o = 0;
		Com_Printf("%s", key);

		if (!*s) {
			Com_Printf("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		Com_Printf("%s\n", value);
	}
}

/*======================================================== */

/**
 * @brief Just throw a fatal error to test error shutdown procedures
 */
static void Com_Error_f (void)
{
	Com_Error(ERR_FATAL, "%s", Cmd_Argv(1));
}

#ifdef HAVE_GETTEXT
/**
 * @brief Gettext init function
 *
 * Initialize the locale settings for gettext
 * po files are searched in ./base/i18n
 * You can override the language-settings in setting
 * the cvar s_language to a valid language-string like
 * e.g. de_DE, en or en_US
 *
 * This function is only build in and called when
 * defining HAVE_GETTEXT
 * Under Linux see Makefile options for this
 *
 * @sa Qcommon_Init
 */
qboolean Qcommon_LocaleSet (void)
{
	char *locale;

	s_language = Cvar_Get("s_language", "", CVAR_ARCHIVE, NULL);
	s_language->modified = qfalse;

#ifdef _WIN32
	Q_putenv("LANG", s_language->string);
	Q_putenv("LANGUAGE", s_language->string);
#else /* _WIN32 */
# ifndef __sun
	unsetenv("LANGUAGE");
# endif /* __sun */
# ifdef __APPLE__
	if (*s_language->string && Q_putenv("LANGUAGE", s_language->string) == -1)
		Com_Printf("...setenv for LANGUAGE failed: %s\n", s_language->string);
	if (*s_language->string && Q_putenv("LC_ALL", s_language->string) == -1)
		Com_Printf("...setenv for LC_ALL failed: %s\n", s_language->string);
# endif /* __APPLE__ */
#endif /* _WIN32 */

	/* set to system default */
	setlocale(LC_ALL, "C");
	locale = setlocale(LC_MESSAGES, s_language->string);
	if (!locale) {
		Com_DPrintf("...could not set to language: %s\n", s_language->string);
		locale = setlocale(LC_MESSAGES, "");
		if (!locale) {
			Com_DPrintf("...could not set to system language\n");
			return qfalse;
		}
	} else {
		Com_Printf("...using language: %s\n", locale);
		Cvar_Set("s_language", locale);
		s_language->modified = qfalse;
	}
	return qtrue;
}
#endif /* HAVE_GETTEXT */

/**
 * @brief
 */
void Com_SetGameType (void)
{
	int i, j;
	gametype_t* gt;
	cvarlist_t* list;

	for (i = 0; i < numGTs; i++) {
		gt = &gts[i];
		if (!Q_strncmp(gt->id, gametype->string, MAX_VAR)) {
			if (dedicated->integer)
				Com_Printf("set gametype to: %s\n", gt->id);
			for (j = 0, list = gt->cvars; j < gt->num_cvars; j++, list++) {
				Cvar_Set(list->name, list->value);
				if (dedicated->integer)
					Com_Printf("  %s = %s\n", list->name, list->value);
			}
			/*Com_Printf("Make sure to restart the map if you switched during a game\n");*/
			break;
		}
	}

	if (i == numGTs)
		Com_Printf("Can't set the gametype - unknown value for cvar gametype: '%s'\n", gametype->string);
}

/**
 * @brief
 */
static void Com_GameTypeList_f (void)
{
	int i, j;
	gametype_t* gt;
	cvarlist_t* list;

	Com_Printf("Available gametypes:\n");
	for (i = 0; i < numGTs; i++) {
		gt = &gts[i];
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
	Com_Printf("Debugging cvars:\n"
			"------------------------------\n"
			" * developer\n"
			" * g_nodamage\n"
			" * mn_debugmenu\n"
			"------------------------------\n"
			"\n"
			"Debugging commands:\n"
			"------------------------------\n"
			" * debug_additems\n"
			" * debug_aircraftlist\n"
			" * debug_baselist\n"
			" * debug_buildinglist\n"
			" * debug_campaignstats\n"
			" * debug_configstrings\n"
			" * debug_capacities\n"
			" * debug_destroyallufos\n"
			" * debug_drawblocked\n"
			"   prints forbidden list to console\n"
			" * debug_fullcredits\n"
			"   restore to full credits\n"
			" * debug_hosp_hurt_all\n"
			" * debug_hosp_heal_all\n"
			" * debug_listufo\n"
			" * debug_menueditnode\n"
			" * debug_menuprint\n"
			" * debug_menureload\n"
			"   reload the menu if you changed it and don't want to restart\n"
			" * debug_researchall\n"
			"   mark all techs as researched\n"
			" * debug_researchableall\n"
			"   mark all techs as researchable\n"
			" * debug_showitems\n"
			" * debug_statsupdate\n"
			" * debug_techlist\n"
			" * killteam <teamid>\n"
			"   kills all living actors in the given team\n"
			" * sv showall\n"
			"   make everything visible to everyone\n"
			"------------------------------\n"
			"\n"
			"Network debugging:\n"
			"------------------------------\n"
			" * net_showdrop"
			"   console message if we have to drop a packet\n"
			" * net_showpackets"
			"   show packets (reliable and unreliable)\n"
			" * net_showpacketsreliable"
			"   only show reliable packets (net_showpackets must be 1, too)\n"
			" * net_showpacketsdata"
			"   also print the received and sent data packets to console\n"
			"------------------------------\n"
			);
}
#endif

/**
 * @brief Watches that the cvar cl_maxfps is never getting lower than 10
 */
static qboolean Com_CvarCheckMaxFPS (cvar_t *cvar)
{
	/* don't allow setting maxfps too low (or game could stop responding) */
	return Cvar_AssertValue(cvar, 10, 1000, qtrue);
}

static void
Cbuf_Execute_timer (int now, void *data) {
  Cbuf_Execute();
}

/**
 * @brief Init function
 *
 * @param[in] argc int
 * @param[in] argv char**
 * @sa Com_ParseScripts
 * @sa Sys_Init
 * @sa CL_Init
 * @sa Qcommon_LocaleSet
 *
 * To compile language support into UFO:AI you need to activate the preprocessor variable
 * HAVE_GETTEXT (for linux have a look at the makefile)
 */
void Qcommon_Init (int argc, const char **argv)
{
	char *s;

#ifndef DEDICATED_ONLY
#ifdef HAVE_GETTEXT
	/* i18n through gettext */
	char languagePath[MAX_OSPATH];
	cvar_t *fs_i18ndir;
#endif /* HAVE_GETTEXT */
#endif /* DEDICATED_ONLY */

	com_aliasSysPool = Mem_CreatePool("Common: Alias system");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");

	if (setjmp(abortframe))
		Sys_Error("Error during initialization");

	/* prepare enough of the subsystems to handle
	   cvar and command buffer management */
	COM_InitArgv(argc, argv);

	Swap_Init();
	Cbuf_Init();

	Cmd_Init();
	Cvar_Init();

	Key_Init();

	/* we need to add the early commands twice, because
	   a basedir needs to be set before execing
	   config files, but we want other parms to override
	   the settings of the config files */
	Cbuf_AddEarlyCommands(qfalse);
	Cbuf_Execute();

	FS_InitFilesystem();

	Cbuf_AddText("exec default.cfg\n");
	Cbuf_AddText("exec config.cfg\n");
#ifndef DEDICATED_ONLY
	Cbuf_AddText("exec keys.cfg\n");
#endif

	Cbuf_AddEarlyCommands(qtrue);
	Cbuf_Execute();

	/* init commands and vars */
	Cmd_AddCommand("error", Com_Error_f, "Just throw a fatal error to test error shutdown procedures");
	Cmd_AddCommand("gametypelist", Com_GameTypeList_f, "List all available multiplayer game types");
#ifdef DEBUG
	Cmd_AddCommand("debug_help", Com_DebugHelp_f, "Show some debugging help");
#endif

	s_sleep = Cvar_Get("s_sleep", "1", CVAR_ARCHIVE, "Use the sleep function to redruce cpu usage");
	developer = Cvar_Get("developer", "0", 0, "Activate developer output to logfile and gameconsole");
	timescale = Cvar_Get("timescale", "1", 0, NULL);
	fixedtime = Cvar_Get("fixedtime", "0", 0, NULL);
	logfile_active = Cvar_Get("logfile", "1", 0, "0 = deacticate logfile, 1 = write normal logfile, 2 = flush on every new line");
	gametype = Cvar_Get("gametype", "1on1", CVAR_ARCHIVE | CVAR_SERVERINFO, "Sets the multiplayer gametype - see gametypelist command for a list of all gametypes");
#ifdef DEDICATED_ONLY
	dedicated = Cvar_Get("dedicated", "1", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");
#else
	dedicated = Cvar_Get("dedicated", "0", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");

	/* set this to false for client - otherwise Qcommon_Frame would set the initial values to multiplayer */
	gametype->modified = qfalse;
#endif
	cl_maxfps = Cvar_Get("cl_maxfps", "90", 0, NULL);
	Cvar_SetCheckFunction("cl_maxfps", Com_CvarCheckMaxFPS);
	teamnum = Cvar_Get("teamnum", "1", CVAR_ARCHIVE, "Multiplayer team num");

	s = va("UFO: Alien Invasion %s %s %s %s", UFO_VERSION, CPUSTRING, __DATE__, BUILDSTRING);
	Cvar_Get("version", s, CVAR_NOSET, "Full version string");
	Cvar_Get("ver", UFO_VERSION, CVAR_SERVERINFO | CVAR_NOSET, "Version number");

	if (dedicated->integer)
		Cmd_AddCommand("quit", Com_Quit, NULL);

	Mem_Init();
	Sys_Init();

	init_net();

	SV_Init();

#ifndef DEDICATED_ONLY /* no gettext support for dedicated servers please */
#ifdef HAVE_GETTEXT
	fs_i18ndir = Cvar_Get("fs_i18ndir", "", 0, "System path to language files");
	/* i18n through gettext */
	setlocale(LC_ALL, "C");
	setlocale(LC_MESSAGES, "");
	/* use system locale dir if we can't find in gamedir */
	if (*fs_i18ndir->string)
		Q_strncpyz(languagePath, fs_i18ndir->string, sizeof(languagePath));
	else
		Com_sprintf(languagePath, sizeof(languagePath), "%s/base/i18n/", FS_GetCwd());
	Com_DPrintf("...using mo files from %s\n", languagePath);
	bindtextdomain(TEXT_DOMAIN, languagePath);
	bind_textdomain_codeset(TEXT_DOMAIN, "UTF-8");
	/* load language file */
	textdomain(TEXT_DOMAIN);

	Qcommon_LocaleSet();
#else /* HAVE_GETTEXT */
	Com_Printf("..no gettext compiled into this binary\n");
#endif /* HAVE_GETTEXT */
#endif /* DEDICATED_ONLY */

	/* e.g. init the client hunk that is used in script parsing */
	CL_Init();

	Com_ParseScripts();

	if (!dedicated->integer)
		Cbuf_AddText("init\n");
	else
		Cbuf_AddText("dedicated_start\n");
	Cbuf_Execute();

	/* add + commands from command line
	   if the user didn't give any commands, run default action */
	if (Cbuf_AddLateCommands()) {
		/* the user asked for something explicit
		 * so drop the loading plaque */
		SCR_EndLoadingPlaque();
	}

#ifndef DEDICATED_ONLY
	CL_InitAfter();
#else
	Com_AddObjectLinks();	/* Add tech links + ammo<->weapon links to items.*/
#endif

	/* Check memory integrity */
	Mem_CheckGlobalIntegrity();

	/* Touch memory */
	Mem_TouchGlobal();

#ifndef DEDICATED_ONLY
	if (!dedicated->integer) {
		Schedule_Timer(cl_maxfps, &CL_Frame, NULL);
		Schedule_Timer(Cvar_Get("cl_slowfreq", "10", 0, NULL), &CL_SlowFrame, NULL);
	}
#endif

	Schedule_Timer(Cvar_Get("sv_freq", "10", 0, NULL), &SV_Frame, NULL);

	/* XXX: These next two lines want to be removed */

#ifndef DEDICATED_ONLY
	/* Temporary hack: IRC logic shouldn't poll a timer like this */
	Schedule_Timer(Cvar_Get("irc_freq", "10", 0, NULL), &Irc_Logic_Frame, NULL);
#endif

	/* Horrible hack: cbuf system wants rethinking */
	Schedule_Timer(Cvar_Get("cbuf_freq", "10", 0, NULL), &Cbuf_Execute_timer, NULL);

	Com_Printf("====== UFO Initialized ======\n\n");
}

/**
 * @brief
 */
static void tick_timer (int now, void *data)
{
	struct timer *timer = data;
	int old_interval = timer->interval;

	/* Compute and store the lateness, updating the total */
	int lateness = Sys_Milliseconds() - now;
	timer->total_lateness -= timer->recent_lateness[timer->next_lateness];
	timer->recent_lateness[timer->next_lateness] = lateness;
	timer->total_lateness += lateness;
	timer->next_lateness++;
	timer->next_lateness %= TIMER_LATENESS_HISTORY;

	/* Is it time to check the mean yet? */
	timer->next_check--;
	if (timer->next_check <= 0) {
		int mean = timer->total_lateness / TIMER_LATENESS_HISTORY;

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
		so it should tend to settle down in the vicinity of the low
		water mark */

		timer->next_check = TIMER_CHECK_INTERVAL;
	}

	timer->interval = max(timer->interval, 1000 / timer->min_freq->integer);

	if (timer->interval != old_interval)
		Com_DPrintf("Adjusted timer on %s to interval %d\n", timer->min_freq->name, timer->interval);

	if (setjmp(abortframe) == 0)
		timer->func(now, timer->data);

	/* We correct for the lateness of this frame. We do not correct for
		the time consumed by this frame - that's billed to the lateness
		of future frames (so that the automagic slowdown can work) */
	Schedule_Event(now + lateness + timer->interval, &tick_timer, timer);
}

/**
 * @brief
 */
void Schedule_Timer (cvar_t *freq, event_func *func, void *data)
{
	struct timer *timer = malloc(sizeof(*timer));
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

	Schedule_Event(Sys_Milliseconds() + timer->interval, &tick_timer, timer);
}

/**
 * @brief
 */
void Schedule_Event (int when, event_func *func, void *data)
{
	struct event *event = malloc(sizeof(*event));
	event->when = when;
	event->func = func;
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

	/* an ERR_DROP was thrown */
	if (setjmp(abortframe))
		return;

	/* If the next event is due... */
	if (event_queue && Sys_Milliseconds() >= event_queue->when) {
		struct event *event = event_queue;

		/* Remove the event from the queue */
		event_queue = event->next;

		if (setjmp(abortframe)) {
			free(event);
			return;
		}

		/* Dispatch the event */
		event->func(event->when, event->data);

		if (setjmp(abortframe))
			return;

		free(event);
	}

	/* Now we spend time_to_next milliseconds working on whatever
	 * IO is ready (but always try at least once, to make sure IO
	 * doesn't stall) */
	do {
		char *s;

		/* XXX: This shouldn't exist */
		do {
			s = Sys_ConsoleInput();
			if (s)
				Cbuf_AddText(va("%s\n", s));
		} while (s);

		/* XXX: This should be somewhere else */
		if (gametype->modified) {
			Com_SetGameType();
			gametype->modified = qfalse;
		}

		time_to_next = event_queue ? (event_queue->when - Sys_Milliseconds()) : 1000;
		if (time_to_next < 0)
			time_to_next = 0;

		wait_for_net(time_to_next);
	} while (time_to_next > 0);
}

/**
 * @brief
 */
void Qcommon_Shutdown (void)
{
	Cbuf_ExecuteText(EXEC_NOW, "shutdown_alias");
}

/**
 * @brief
 * @note This is here to let the client know (without sv or server.h) about the server active bool
 * @sa SV_SpawnServer
 */
qboolean Qcommon_ServerActive (void)
{
	return sv.active;
}
