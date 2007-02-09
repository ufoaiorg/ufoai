/**
 * @file sys_linux.c
 * @brief main function and system functions
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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#ifdef __linux__
# include <execinfo.h>
# include <sys/utsname.h>
# include <link.h>
# include <sys/ucontext.h>

/* for old headers */
# ifndef REG_EIP
#  ifndef EIP
#   define EIP 12 /* aiee */
#  endif
#  define REG_EIP EIP
# endif
#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__NetBSD__)
#else
#include <mntent.h>
#endif
#include <pwd.h>
#include <dirent.h>
#include <libgen.h> /* dirname */

#include <dlfcn.h>

#include "../../qcommon/qcommon.h"

#include "rw_linux.h"

cvar_t *nostdout;

unsigned	sys_frame_time;

uid_t saved_euid;
qboolean stdin_active = qtrue;

static void *game_library;

/**
 * @brief
 */
char *Sys_GetCurrentUser( void )
{
	struct passwd *p;

	if ( (p = getpwuid( getuid() )) == NULL ) {
		return "player";
	}
	return p->pw_name;
}

/**
 * @brief
 * @return NULL if getcwd failed
 */
char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	if (getcwd(cwd, sizeof(cwd) - 1) == NULL)
		return NULL;
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/**
 * @brief
 */
void Sys_NormPath(char* path)
{
}

/**
 * @brief This resolves any symlinks to the binary. It's disabled for debug
 * builds because there are situations where you are likely to want
 * to symlink to binaries and /not/ have the links resolved.
 * This way you can make a link in /usr/bin and the data-files are still
 * found in e.g. /usr/local/games/ufoai
 */
char *Sys_BinName( const char *arg0 )
{
#ifndef DEBUG
	int	n;
	char	src[MAX_OSPATH];
	char	dir[MAX_OSPATH];
	qboolean	links = qfalse;
#endif

	static char	dst[MAX_OSPATH];
	Com_sprintf(dst, MAX_OSPATH, (char*)arg0);

#ifndef DEBUG
	while((n = readlink(dst, src, MAX_OSPATH)) >= 0) {
		src[n] = '\0';
		Com_sprintf(dir, MAX_OSPATH, dirname(dst));
		Com_sprintf(dst, MAX_OSPATH, dir);
		Q_strcat(dst, "/", MAX_OSPATH);
		Q_strcat(dst, src, MAX_OSPATH);
		links = qtrue;
	}

	if (links) {
		Com_sprintf(dst, MAX_OSPATH, Sys_Cwd());
		Q_strcat(dst, "/", MAX_OSPATH);
		Q_strcat(dst, dir, MAX_OSPATH);
		Q_strcat(dst, "/", MAX_OSPATH);
		Q_strcat(dst, src, MAX_OSPATH);
	}
#endif
	return dst;
}

/**
 * @brief
 */
char *Sys_GetHomeDirectory (void)
{
	return getenv ( "HOME" );
}

#ifdef __linux__

/* gcc 2.7.2 has trouble understanding this unfortunately at compile time */
/* and 2.95.3 won't find it at link time. blah. */
#define GCC_VERSION (__GNUC__ * 10000 \
	+ __GNUC_MINOR__ * 100 \
	+ __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 30000
/**
 * @brief
 */
static int dlcallback (struct dl_phdr_info *info, size_t size, void *data)
{
	int j;
	int end;

	end = 0;

	if (!info->dlpi_name || !info->dlpi_name[0])
		return 0;

	for (j = 0; j < info->dlpi_phnum; j++) {
		end += info->dlpi_phdr[j].p_memsz;
	}

	/* this is terrible. */
#if __WORDSIZE == 64
	fprintf (stderr, "[0x%lux-0x%lux] %s\n", info->dlpi_addr, info->dlpi_addr + end, info->dlpi_name);
#else
	fprintf (stderr, "[0x%ux-0x%ux] %s\n", info->dlpi_addr, info->dlpi_addr + end, info->dlpi_name);
#endif
	return 0;
}
#endif

/**
 * @brief Obtain a backtrace and print it to stderr.
 * @note Adapted from http://www.delorie.com/gnu/docs/glibc/libc_665.html
 */
#ifdef __x86_64__
void Sys_Backtrace (int sig)
#else
void Sys_Backtrace (int sig, siginfo_t *siginfo, void *secret)
#endif
{
	void		*array[32];
	struct utsname	info;
	size_t		size;
	size_t		i;
	char		**strings;
#ifndef __x86_64__
	ucontext_t 	*uc = (ucontext_t *)secret;
#endif

	signal (SIGSEGV, SIG_DFL);

	fprintf (stderr, "=====================================================\n"
		"Segmentation Fault\n"
		"=====================================================\n"
		"A crash has occured within UFO:AI or the Game library\n"
		"that you are running.\n"
		"\n"
		"If possible, re-building UFO:AI to ensure it is not a\n"
		"compile problem. If the crash still persists,  please\n"
		"put the following debug info on the UFO:AI forum with\n"
		"details including the version, Linux distribution and\n"
		"any other pertinent information.\n"
		"\n");

	size = backtrace (array, sizeof(array)/sizeof(void*));

#ifndef __x86_64__
	array[1] = (void *) uc->uc_mcontext.gregs[REG_EIP];
#endif

	strings = backtrace_symbols (array, size);

	fprintf (stderr, "Stack dump (%zd frames):\n", size);

	for (i = 0; i < size; i++)
		fprintf (stderr, "%.2zd: %s\n", i, strings[i]);

	fprintf (stderr, "\nVersion: " UFO_VERSION " (" BUILDSTRING " " CPUSTRING ")\n");

	uname (&info);
	fprintf (stderr, "OS Info: %s %s %s %s %s\n\n", info.sysname, info.nodename, info.release, info.version, info.machine);

#if GCC_VERSION > 30000
	fprintf (stderr, "Loaded libraries:\n");
	dl_iterate_phdr(dlcallback, NULL);
#endif

	free (strings);

	raise (SIGSEGV);
}

#endif /* __linux__ */

/* ======================================================================= */
/* General routines */
/* ======================================================================= */

/**
 * @brief
 */
void Sys_ConsoleOutput (char *string)
{
	char text[2048];
	int i, j;

	if (nostdout && nostdout->value)
		return;

	i = j = 0;

	/* strip high bits */
	while (string[j]) {
		text[i] = string[j] & SCHAR_MAX;

		/* strip low bits */
		if (text[i] >= 32 || text[i] == '\n' || text[i] == '\t')
			i++;

		j++;

		if (i == sizeof(text) - 2) {
			text[i++] = '\n';
			break;
		}
	}
	text[i] = 0;

	fputs(string, stdout);
}

/**
 * @brief
 */
int Sys_FileLength (const char *path)
{
	struct stat st;

	if (stat (path, &st) || (st.st_mode & S_IFDIR))
		return -1;

	return st.st_size;
}

/**
 * @brief
 */
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];
	unsigned char		*p;

	if (nostdout && nostdout->value)
		return;

	va_start (argptr,fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);

	text[sizeof(text)-1] = 0;

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	for (p = (unsigned char *)text; *p; p++) {
		*p &= SCHAR_MAX;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
}

/**
 * @brief
 */
void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();
	fcntl (0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);
	exit(0);
}

/**
 * @brief
 */
void Sys_Init (void)
{
	Cvar_Get("sys_os", "linux", CVAR_SERVERINFO, NULL);
#if id386
/*	Sys_SetFPCW(); */
#endif
#ifdef __linux__
# ifndef __x86_64__
	struct sigaction sa;

	if (sizeof(uint32_t) != 4)
		Sys_Error("uint32 != 32 bits");
	else if (sizeof(uint64_t) != 8)
		Sys_Error("uint64 != 64 bits");
	else if (sizeof(uint16_t) != 2)
		Sys_Error("uint16 != 16 bits");

	sa.sa_handler = (void *)Sys_Backtrace;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_SIGINFO;

	sigaction(SIGSEGV, &sa, NULL);
# else
	signal(SIGSEGV, Sys_Backtrace);
# endif
#endif /* __linux__ */
}

/**
 * @brief
 * @sa Sys_DisableTray
 */
void Sys_EnableTray (void)
{
}

/**
 * @brief
 * @sa Sys_EnableTray
 */
void Sys_DisableTray (void)
{
}

/**
 * @brief
 */
void Sys_Minimize (void)
{
}

/**
 * @brief
 */
void Sys_Error (char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	/* change stdin to non blocking */
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) & ~FNDELAY);

#ifndef DEDICATED_ONLY
	CL_Shutdown ();
#endif
	Qcommon_Shutdown ();

	va_start(argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	string[sizeof(string)-1] = 0;

	fprintf(stderr, "Error: %s\n", string);
	exit(1);
}

/**
 * @brief
 * @return -1 if not present
 */
int Sys_FileTime (char *path)
{
	struct	stat	buf;

	if (stat (path,&buf) == -1)
		return -1;

	return buf.st_mtime;
}

/**
 * @brief
 */
void floating_point_exception_handler (int whatever)
{
	signal(SIGFPE, floating_point_exception_handler);
}

/**
 * @brief
 */
char *Sys_ConsoleInput (void)
{
	static char text[256];
	int     len;
	fd_set	fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
		return NULL;

	if (!stdin_active)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(0, &fdset))
		return NULL;

	len = read (0, text, sizeof(text));
	if (len == 0) { /* eof! */
		stdin_active = qfalse;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len-1] = 0;    /* rip off the /n and terminate */

	return text;
}

/**
 * @brief
 */
void Sys_UnloadGame (void)
{
	if (game_library)
		dlclose (game_library);
	game_library = NULL;
}

/**
 * @brief Loads the game dll
 */
game_export_t *Sys_GetGameAPI (game_import_t *parms)
{
	void	*(*GetGameAPI) (void *);

	char	name[MAX_OSPATH];
	char	*curpath;
	char	*path;

	setreuid(getuid(), getuid());
	setegid(getgid());

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	curpath = Sys_Cwd();

	Com_Printf("------- Loading game.so -------\n");

	/* now run through the search paths */
	path = NULL;
	while (1) {
		path = FS_NextPath (path);
		if (!path)
			return NULL;		/* couldn't find one anywhere */
		Com_sprintf(name, MAX_OSPATH, "%s/%s/game.so", curpath, path);
		game_library = dlopen (name, RTLD_LAZY );
		if (game_library) {
			Com_Printf("LoadLibrary (%s)\n", name);
			break;
		} else {
			Com_Printf("LoadLibrary failed (%s)\n", name);
			Com_Printf("%s\n", dlerror());
		}
	}

	GetGameAPI = (void *)dlsym (game_library, "GetGameAPI");
	if (!GetGameAPI) {
		Sys_UnloadGame ();
		return NULL;
	}

	return GetGameAPI (parms);
}

/**
 * @brief
 */
void Sys_AppActivate (void)
{
}

/**
 * @brief
 */
void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	if (KBD_Update_fp)
		KBD_Update_fp();
#endif

	/* grab frame time */
	sys_frame_time = Sys_Milliseconds();
}

/**
 * @brief
 */
char *Sys_GetClipboardData (void)
{
#if 0 /* this should be in the renderer lib */
	Window sowner;
	Atom type, property;
	unsigned long len, bytes_left, tmp;
	unsigned char *data;
	int format, result;
	char *ret = NULL;

	sowner = XGetSelectionOwner(dpy, XA_PRIMARY);

	if (sowner != None) {
		property = XInternAtom(dpy, "GETCLIPBOARDDATA_PROP", False);

		XConvertSelection(dpy, XA_PRIMARY, XA_STRING, property, win, myxtime);
		/* myxtime is time of last X event */

		XFlush(dpy);

		XGetWindowProperty(dpy, win, property, 0, 0, False, AnyPropertyType,
							&type, &format, &len, &bytes_left, &data);

		if (bytes_left > 0) {
			result = XGetWindowProperty(dpy, win, property, 0, bytes_left, True, AnyPropertyType,
							&type, &format, &len, &tmp, &data);

			if (result == Success)
				ret = strdup((char*)data);
			XFree(data);
		}
	}
	return ret;
#else
	return NULL;
#endif
}

/**
 * @brief The entry point for linux server and client.
 *
 * Inits the the program and calls Qcommon in an infinite loop.
 * FIXME: While this works, infinite loops are bad; one should not rely on exit() call; the program should be designed to fall through the bottom.
 * FIXME: Why is there a sleep statement?
 */
int main (int argc, char **argv)
{
	int time, oldtime, newtime;
	float timescale = 1.0;

	/* go back to real user for config loads */
	saved_euid = geteuid();
	seteuid(getuid());

	Qcommon_Init(argc, argv);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	nostdout = Cvar_Get("nostdout", "0", 0, NULL);
	if (!nostdout->value) {
		fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);
	}

	newtime = Sys_Milliseconds();
	for (;;) {
		/* find time spent rendering last frame */
		oldtime = newtime;
		newtime = Sys_Milliseconds();
		time = timescale * (newtime - oldtime);
		timescale = Qcommon_Frame(time);
	}
	return 0;
}

