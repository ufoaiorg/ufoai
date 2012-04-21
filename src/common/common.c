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
#include "http.h"
#include "../server/server.h"
#include "../shared/parse.h"
#include "../ports/system.h"
#include <setjmp.h>

#define	MAXPRINTMSG	4096
#define MAX_NUM_ARGVS	50

csi_t csi;

static int com_argc;
static const char *com_argv[MAX_NUM_ARGVS + 1];

static jmp_buf abortframe; /* an ERR_DROP occurred, exit the entire frame */

static vPrintfPtr_t vPrintfPtr = Com_vPrintf;

cvar_t *developer;
cvar_t *http_proxy;
cvar_t *http_timeout;
static const char *consoleLogName = "ufoconsole.log";
static cvar_t *logfile_active; /* 1 = buffer log, 2 = flush after each print */
static cvar_t *com_pipefile; /* name of the pipe to send commands to */
cvar_t *sv_dedicated;
#ifndef DEDICATED_ONLY
static cvar_t *cl_maxfps;
cvar_t *s_language;
#endif
cvar_t *sv_gametype;
cvar_t *masterserver_url;
cvar_t *port;
cvar_t* sys_priority;
cvar_t* sys_affinity;
cvar_t* sys_os;

static qFILE logfile;
static qFILE pipefile;

struct memPool_s *com_aliasSysPool;
struct memPool_s *com_cmdSysPool;
struct memPool_s *com_cmodelSysPool;
struct memPool_s *com_cvarSysPool;
struct memPool_s *com_fileSysPool;
struct memPool_s *com_genericPool;
struct memPool_s *com_networkPool;

static scheduleEvent_t *event_queue = NULL;

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

static void Schedule_Timer(cvar_t *freq, event_func *func, event_check_func *check, void *data);

/*
==============================================================================
TARGETING FUNCTIONS
==============================================================================
*/

/**
 * @note Grenade Aiming Maths
 * --------------------------------------------------------------
 *
 * There are two possibilities when aiming: either we can reach the target at
 * maximum speed or we can't. If we can reach it we would like to reach it with
 * as flat a trajectory as possible. To do this we calculate the angle to hit the
 * target with the projectile traveling at the maximum allowed velocity.
 *
 * However, if we can't reach it then we'd like the aiming curve to use the smallest
 * possible velocity that would have reached the target.
 *
 * let d  = horizontal distance to target
 *     h  = vertical distance to target
 *     g  = gravity
 *     v  = launch velocity
 *     vx = horizontal component of launch velocity
 *     vy = vertical component of launch velocity
 *     alpha = launch angle
 *     t  = time
 *
 * Then using the laws of linear motion and some trig
 *
 * d = vx * t
 * h = vy * t - 1/2 * g * t^2
 * vx = v * cos(alpha)
 * vy = v * sin(alpha)
 *
 * and some trig identities
 *
 * 2*cos^2(x) = 1 + cos(2*x)
 * 2*sin(x)*cos(x) = sin(2*x)
 * a*cos(x) + b*sin(x) = sqrt(a^2 + b^2) * cos(x - atan2(b,a))
 *
 * it is possible to show that:
 *
 * alpha = 0.5 * (atan2(d, -h) - theta)
 *
 * where
 *               /    2       2  \
 *              |    v h + g d    |
 *              |  -------------- |
 * theta = acos |        ________ |
 *              |   2   / 2    2  |
 *               \ v  \/ h  + d  /
 *
 *
 * Thus we can calculate the desired aiming angle for any given velocity.
 *
 * With some more rearrangement we can show:
 *
 *               ________________________
 *              /           2
 *             /         g d
 *  v =       / ------------------------
 *           /   ________
 *          /   / 2    2
 *        \/  \/ h  + d   cos(theta) - h
 *
 * Which we can also write as:
 *                _________________
 *               /        a
 * f(theta) =   / ----------------
 *            \/  b cos(theta) - c
 *
 * where
 *  a = g*d^2
 *  b = sqrt(h*h+d*d)
 *  c = h
 *
 * We can imagine a graph of launch velocity versus launch angle. When the angle is near 0 degrees (i.e. totally flat)
 * more and more velocity is needed. Similarly as the angle gets closer and closer to 90 degrees.
 *
 * Somewhere in the middle is the minimum velocity that we could possibly hit the target with and the 'optimum' angle
 * to fire at. Note that when h = 0 the optimum angle is 45 degrees. We want to find the minimum velocity so we need
 * to take the derivative of f (which I suggest doing with an algebra system!).
 *
 * f'(theta) =  a * b * sin(theta) / junk
 *
 * the `junk` is unimportant because we're just going to set f'(theta) = 0 and multiply it out anyway.
 *
 * 0 = a * b * sin(theta)
 *
 * Neither a nor b can be 0 as long as d does not equal 0 (which is a degenerate case). Thus if we solve for theta
 * we get 'sin(theta) = 0', thus 'theta = 0'. If we recall that:
 *
 *  alpha = 0.5 * (atan2(d, -h) - theta)
 *
 * then we get our 'optimum' firing angle alpha as
 *
 * alpha = 1/2 * atan2(d, -h)
 *
 * and if we substitute back into our equation for v and we get
 *
 *               _______________
 *              /        2
 *             /      g d
 *  vmin =    / ---------------
 *           /   ________
 *          /   / 2    2
 *        \/  \/ h  + d   - h
 *
 * as the minimum launch velocity for that angle.
 *
 * @brief Calculates parabola-type shot.
 * @param[in] from Starting position for calculations.
 * @param[in] at Ending position for calculations.
 * @param[in] speed Launch velocity.
 * @param[in] launched Set to true for grenade launchers.
 * @param[in] rolled Set to true for "roll" type shoot.
 * @param[in,out] v0 The velocity vector
 * @todo refactor and move me
 * @todo Com_GrenadeTarget() is called from CL_TargetingGrenade() with speed
 * param as (fireDef_s) fd->range (gi.GrenadeTarget, too), while it is being used here for speed
 * calculations - a bug or just misleading documentation?
 * @sa CL_TargetingGrenade
 */
float Com_GrenadeTarget (const vec3_t from, const vec3_t at, float speed, qboolean launched, qboolean rolled, vec3_t v0)
{
	const float rollAngle = 3.0; /* angle to throw at for rolling, in degrees. */
	vec3_t delta;
	float d, h, g, v, alpha, vx, vy;
	float k, gd2, len;

	/* calculate target distance and height */
	h = at[2] - from[2];
	VectorSubtract(at, from, delta);
	delta[2] = 0;
	d = VectorLength(delta);

	/* check that it's not degenerate */
	if (d == 0) {
		return 0;
	}

	/* precalculate some useful values */
	g = GRAVITY;
	gd2 = g * d * d;
	len = sqrt(h * h + d * d);

	/* are we rolling? */
	if (rolled) {
		float theta;
		alpha = rollAngle * torad;
		theta = atan2(d, -h) - 2 * alpha;
		k = gd2 / (len * cos(theta) - h);
		if (k <= 0)	/* impossible shot at any velocity */
			return 0;
		v = sqrt(k);
	} else {
		/* firstly try with the maximum speed possible */
		v = speed;
		k = (v * v * h + gd2) / (v * v * len);

		/* check whether the shot's possible */
		if (launched && k >= -1 && k <= 1) {
			/* it is possible, so calculate the angle */
			alpha = 0.5 * (atan2(d, -h) - acos(k));
		} else {
			/* calculate the minimum possible velocity that would make it possible */
			alpha = 0.5 * atan2(d, -h);
			v = sqrt(gd2 / (len - h));
		}
	}

	/* calculate velocities */
	vx = v * cos(alpha);
	vy = v * sin(alpha);
	VectorNormalizeFast(delta);
	VectorScale(delta, vx, v0);
	v0[2] = vy;

	/* prevent any rounding errors */
	VectorNormalizeFast(v0);
	VectorScale(v0, v - DIST_EPSILON, v0);

	/* return time */
	return d / vx;
}

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
	if (buffersize > MAXPRINTMSG)
		Com_Error(ERR_DROP, "redirect buffer may not be bigger than MAXPRINTMSG (%i)", MAXPRINTMSG);
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
 * @brief Creates a timestamp with date and time at the specified location
 * @param ts ptr to the resulting string
 * @param tslen length of target buffer
 */
void Com_MakeTimestamp (char* ts, const size_t tslen)
{
	struct tm *t;
	time_t aclock;

	time(&aclock);
	t = localtime(&aclock);

	Com_sprintf(ts, tslen, "%4i/%02i/%02i %02i:%02i:%02i", t->tm_year + 1900,
			t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
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

	Con_Print(msg);

	/* also echo to debugging console */
	Sys_ConsoleOutput(msg);

	/* logfile */
	if (logfile_active && logfile_active->integer) {
		if (!logfile.f) {
			if (logfile_active->integer > 2)
				FS_OpenFile(consoleLogName, &logfile, FILE_APPEND);
			else
				FS_OpenFile(consoleLogName, &logfile, FILE_WRITE);
		}
		if (logfile.f) {
			/* strip color codes */
			const char *output = msg;

			if (output[strlen(output) - 1] == '\n') {
				char timestamp[40];
				Com_MakeTimestamp(timestamp, sizeof(timestamp));
				FS_Write(timestamp, strlen(timestamp), &logfile);
				FS_Write(" ", 1, &logfile);
			}

			FS_Write(output, strlen(output), &logfile);

			if (logfile_active->integer > 1)
				fflush(logfile.f);	/* force it to save every time */
		}
	}
}

void Com_Printf (const char* const fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vPrintfPtr(fmt, ap);
	va_end(ap);
}

/**
 * @brief A Com_Printf that only shows up if the "developer" cvar is set
 */
void Com_DPrintf (int level, const char *fmt, ...)
{
	/* don't confuse non-developers with techie stuff... */
	if (!developer)
		return;

	if (developer->integer == 1 || (developer->integer & level)) {
		va_list ap;

		va_start(ap, fmt);
		vPrintfPtr(fmt, ap);
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
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	switch (code) {
	case ERR_DISCONNECT:
		Com_Printf("%s\n", msg);
		CL_Drop();
		recursive = qfalse;
		Com_Drop();
	case ERR_DROP:
		Com_Printf("********************\n");
		Com_Printf("ERROR: %s\n", msg);
		Com_Printf("********************\n");
		Sys_Backtrace();
		SV_Shutdown("Server crashed.", qfalse);
		CL_Drop();
		recursive = qfalse;
		Com_Drop();
	default:
		Com_Printf("%s\n", msg);
		SV_Shutdown("Server fatal crashed", qfalse);

		/* send an receive net messages a last time */
		NET_Wait(0);

		FS_CloseFile(&logfile);
		if (pipefile.f != NULL) {
			FS_CloseFile(&pipefile);
			FS_RemoveFile(va("%s/%s", FS_Gamedir(), pipefile.name));
		}

		CL_Shutdown();
		Qcommon_Shutdown();
		Sys_Error("Shutdown");
	}
}

void Com_SetExceptionCallback (exceptionCallback_t callback)
{
	static exceptionCallback_t callbackFunc;
	callbackFunc = callback;
	if (setjmp(abortframe))
		callbackFunc();
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
	FS_CloseFile(&logfile);
	if (pipefile.f != NULL) {
		FS_CloseFile(&pipefile);
		FS_RemoveFile(va("%s/%s", FS_Gamedir(), pipefile.name));
	}
	Sys_Quit();
}


/**
 * @brief Check whether we are the server or have a singleplayer tactical mission
 * @sa Com_SetServerState
 */
int Com_ServerState (void)
{
	return sv->state;
}

/**
 * @sa SV_SpawnServer
 * @sa Com_ServerState
 */
void Com_SetServerState (int state)
{
	Com_DPrintf(DEBUG_ENGINE, "Set server state to %i\n", state);
	if (state == ss_dead)
		SV_Shutdown("Server shutdown", qfalse);
	else if (state == ss_restart)
		SV_Shutdown("Server map change", qtrue);
	sv->state = state;
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
 * @sa UI_GetReferenceString
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
	OBJZERO(expanded);
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

void Com_UploadCrashDump (const char *crashDumpFile)
{
	upparam_t paramUser;
	upparam_t paramVersion;
	upparam_t paramOS;
	const char *crashDumpURL = "http://ufoai.org/CrashDump.php";

	paramUser.name = "user";
	paramUser.value = Sys_GetCurrentUser();
	paramUser.next = &paramVersion;
	paramVersion.name = "version";
	paramVersion.value = UFO_VERSION;
	paramVersion.next = &paramOS;
	paramOS.name = "os";
	paramOS.value = BUILDSTRING_OS;
	paramOS.next = NULL;

	HTTP_PutFile("crashdump", crashDumpFile, crashDumpURL, &paramUser);
	HTTP_PutFile("crashdump", va("%s/%s", FS_Gamedir(), consoleLogName), crashDumpURL, &paramUser);
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
qboolean Com_ConsoleCompleteCommand (const char *s, char *target, size_t bufSize, uint32_t *pos, uint32_t offset)
{
	const char *cmd = NULL, *cvar = NULL, *use = NULL;
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
		int cntParams;
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
		static char cmdBackup[MAX_QPATH];
		int cntCvar;
		int cntCmd = Cmd_CompleteCommand(s, &cmd);
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

	for (i = 0; i < csi.numGTs; i++) {
		const gametype_t *gt = &csi.gts[i];
		if (Q_streq(gt->id, sv_gametype->string)) {
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

	if (i == csi.numGTs)
		Com_Printf("Can't set the gametype - unknown value for cvar gametype: '%s'\n", sv_gametype->string);
}

static void Com_GameTypeList_f (void)
{
	int i;

	Com_Printf("Available gametypes:\n");
	for (i = 0; i < csi.numGTs; i++) {
		int j;
		const gametype_t *gt = &csi.gts[i];
		const cvarlist_t *list;

		Com_Printf("%s\n", gt->id);

		for (j = 0, list = gt->cvars; j < gt->num_cvars; j++, list++)
			Com_Printf("  %s = %s\n", list->name, list->value);
	}
}

/**
 * @param[in] index The index of the config string array
 * @note Some config strings are using one value over multiple config string
 * slots. These may not be accessed directly.
 */
qboolean Com_CheckConfigStringIndex (int index)
{
	if (index < 0 || index >= MAX_CONFIGSTRINGS)
		return qfalse;

	/* CS_TILES and CS_POSITIONS can stretch over multiple configstrings,
	 * so don't access the middle parts. */
	if (index > CS_TILES && index < CS_POSITIONS)
		return qfalse;
	if (index > CS_POSITIONS && index < CS_MODELS)
		return qfalse;

	return qtrue;
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
		if (Q_streq(errorType, "ERR_DROP"))
			Com_Error(ERR_DROP, "%s", Cmd_Argv(2));
		else if (Q_streq(errorType, "ERR_FATAL"))
			Com_Error(ERR_FATAL, "%s", Cmd_Argv(2));
		else if (Q_streq(errorType, "ERR_DISCONNECT"))
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
	{"DEBUG_ROUTING", DEBUG_ROUTING},
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
			if (Q_streq(debugLevel, debugLevels[i].str)) {
				if (oldValue & debugLevels[i].debugLevel)	/* if it's already set... */
					newValue &= ~debugLevels[i].debugLevel;	/* ...reset it. */
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
	FS_Printf(&f, "// variables\n");
	Cvar_WriteVariables(&f);
	FS_Printf(&f, "// aliases\n");
	Cmd_WriteAliases(&f);
	FS_CloseFile(&f);
	Com_Printf("Wrote %s.\n", filename);
}

void Com_SetRandomSeed (unsigned int seed)
{
	srand(seed);
	/*Com_Printf("setting random seed to %i\n", seed);*/
}

const char* Com_ByteToBinary (byte x)
{
	static char buf[9];
	int cnt, mask = 1 << 7;
	char *b = buf;

	for (cnt = 1; cnt <= 8; ++cnt) {
		*b++ = ((x & mask) == 0) ? '0' : '1';
		x <<= 1;
		if (cnt == 8)
			*b++ = '\0';
	}

	return buf;
}

const char* Com_UnsignedIntToBinary (uint32_t x)
{
	static char buf[37];
	int cnt, mask = 1 << 31;
	char *b = buf;

	for (cnt = 1; cnt <= 32; ++cnt) {
		*b++ = ((x & mask) == 0) ? '0' : '1';
		x <<= 1;
		if (cnt % 8 == 0 && cnt != 32)
			*b++ = ' ';
		if (cnt == 32)
			*b++ = '\0';
	}

	return buf;
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

void Qcommon_SetPrintFunction (vPrintfPtr_t func)
{
	vPrintfPtr = func;
}

vPrintfPtr_t Qcommon_GetPrintFunction (void)
{
	return vPrintfPtr;
}

static void Qcommon_InitError (void)
{
	Sys_Error("Error during initialization");
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
	Com_SetRandomSeed(time(NULL));

	com_aliasSysPool = Mem_CreatePool("Common: Alias system for commands and enums");
	com_cmdSysPool = Mem_CreatePool("Common: Command system");
	com_cmodelSysPool = Mem_CreatePool("Common: Collision model");
	com_cvarSysPool = Mem_CreatePool("Common: Cvar system");
	com_fileSysPool = Mem_CreatePool("Common: File system");
	com_genericPool = Mem_CreatePool("Generic");
	com_networkPool = Mem_CreatePool("Network");

	Com_SetExceptionCallback(Qcommon_InitError);

	OBJZERO(csi);

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

	Com_SetRenderModified(qfalse);
	Com_SetUserinfoModified(qfalse);

	/* init commands and vars */
	Cmd_AddCommand("saveconfig", Com_WriteConfig_f, "Write the configuration to file");
	Cmd_AddCommand("gametypelist", Com_GameTypeList_f, "List all available multiplayer game types");
#ifdef DEBUG
	Cmd_AddCommand("debug_help", Com_DebugHelp_f, "Show some debugging help");
	Cmd_AddCommand("debug_error", Com_DebugError_f, "Just throw a fatal error to test error shutdown procedures");
#endif
	Cmd_AddCommand("setdeveloper", Com_DeveloperSet_f, "Set the developer cvar to only get the debug output you want");

	developer = Cvar_Get("developer", "0", 0, "Activate developer output to logfile and gameconsole");
#ifdef DEBUG
	logfile_active = Cvar_Get("logfile", "2", 0, "0 = deactivate logfile, 1 = write normal logfile, 2 = flush on every new line, 3 = always append to existing file");
#else
	logfile_active = Cvar_Get("logfile", "1", 0, "0 = deactivate logfile, 1 = write normal logfile, 2 = flush on every new line, 3 = always append to existing file");
#endif
	sv_gametype = Cvar_Get("sv_gametype", "1on1", CVAR_ARCHIVE | CVAR_SERVERINFO, "Sets the multiplayer gametype - see gametypelist command for a list of all gametypes");
	http_proxy = Cvar_Get("http_proxy", "", CVAR_ARCHIVE, "Use this proxy for http transfers");
	http_timeout = Cvar_Get("http_timeout", "3", CVAR_ARCHIVE, "Http connection and read timeout");
	port = Cvar_Get("port", DOUBLEQUOTE(PORT_SERVER), CVAR_NOSET, NULL);
	masterserver_url = Cvar_Get("masterserver_url", MASTER_SERVER, CVAR_ARCHIVE, "URL of UFO:AI masterserver");
#ifdef DEDICATED_ONLY
	sv_dedicated = Cvar_Get("sv_dedicated", "1", CVAR_SERVERINFO | CVAR_NOSET, "Is this a dedicated server?");
	/* don't allow to override this from commandline of config */
	Cvar_ForceSet("sv_dedicated", "1");
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

	com_pipefile = Cvar_Get("com_pipefile", "", CVAR_ARCHIVE, "Filename of the pipe that is used to send commands to the game");
	if (com_pipefile->string[0] != '\0') {
		FS_CreateOpenPipeFile(com_pipefile->string, &pipefile);
	}

	CL_InitAfter();

	/* Check memory integrity */
	Mem_CheckGlobalIntegrity();

	/* Touch memory */
	Mem_TouchGlobal();

#ifndef DEDICATED_ONLY
	if (!sv_dedicated->integer) {
		Schedule_Timer(cl_maxfps, &CL_Frame, NULL, NULL);
		Schedule_Timer(Cvar_Get("cl_slowfreq", "10", 0, NULL), &CL_SlowFrame, NULL, NULL);

		/* now hide the console */
		Sys_ShowConsole(qfalse);
	}
#endif

	Schedule_Timer(Cvar_Get("sv_freq", "10", CVAR_NOSET, NULL), &SV_Frame, NULL, NULL);

	/** @todo This line wants to be removed */
	Schedule_Timer(Cvar_Get("cbuf_freq", "10", 0, NULL), &Cbuf_Execute_timer, NULL, NULL);

	Com_Printf("====== UFO Initialized ======\n");
	Com_Printf("=============================\n");
}

/**
 * @brief Read whatever is in com_pipefile, if anything, and execute it
 */
void Com_ReadFromPipe (void)
{
	char buffer[MAX_STRING_CHARS] = { "" };
	int read;

	if (pipefile.f == NULL)
		return;

	read = FS_Read2(buffer, sizeof(buffer), &pipefile, qfalse);
	if (read > 0)
		Cmd_ExecuteString(buffer);
}

static void tick_timer (int now, void *data)
{
	struct timer *timer = (struct timer *)data;
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

static void Schedule_Timer (cvar_t *freq, event_func *func, event_check_func *check, void *data)
{
	struct timer *timer = (struct timer *)Mem_PoolAlloc(sizeof(*timer), com_genericPool, 0);
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

	Schedule_Event(Sys_Milliseconds() + timer->interval, &tick_timer, check, NULL, timer);
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
scheduleEvent_t *Schedule_Event (int when, event_func *func, event_check_func *check, event_clean_func *clean, void *data)
{
#ifdef DEBUG
	scheduleEvent_t *i;
#endif
	scheduleEvent_t *event = (scheduleEvent_t *)Mem_PoolAlloc(sizeof(*event), com_genericPool, 0);
	event->when = when;
	event->func = func;
	event->check = check;
	event->clean = clean;
	event->data = data;

	if (!event_queue || event_queue->when > when) {
		event->next = event_queue;
		event_queue = event;
	} else {
		scheduleEvent_t *e = event_queue;
		while (e->next && e->next->when <= when)
			e = e->next;
		event->next = e->next;
		e->next = event;
	}

#ifdef DEBUG
	for (i = event_queue; i && i->next; i = i->next)
		if (i->when > i->next->when)
			abort();
#endif

	return event;
}

scheduleEvent_t* Dequeue_Event(int now);
/**
 * @brief Finds and returns the first event in the event_queue that is due.
 *  If the event has a check function, we check to see if the event can be run now, and skip it if not (even if it is due).
 * @return Returns a pointer to the event, NULL if none found.
 */
scheduleEvent_t* Dequeue_Event (int now)
{
	scheduleEvent_t *event = event_queue;
	scheduleEvent_t *prev = NULL;

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
int CL_FilterEventQueue (event_filter *filter)
{
	scheduleEvent_t *event = event_queue;
	scheduleEvent_t *prev = NULL;
	int filtered = 0;

	assert(filter);

	while (event) {
		qboolean keep = filter(event->when, event->func, event->check, event->data);
		scheduleEvent_t *freeme = event;

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
		filtered++;
	}

	return filtered;
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
	static scheduleEvent_t *event;

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

	FS_Shutdown();
	Cvar_Shutdown();
	Cmd_Shutdown();
	NET_Shutdown();
	Mem_Shutdown();
	Com_Shutdown();
}
