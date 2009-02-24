/**
 * @file linux_main.c
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

#include <fcntl.h>
#include <signal.h>

#include "../../common/common.h"
#include "../unix/unix_curses.h"

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#define MAX_BACKTRACE_SYMBOLS 50
#endif

cvar_t* sys_priority;
cvar_t* sys_affinity;
cvar_t* sys_os;


/* ======================================================================= */
/* General routines */
/* ======================================================================= */

/**
 * @brief On platforms supporting it, print a backtrace.
 */
static void Sys_Backtrace (void)
{
#ifdef HAVE_EXECINFO_H
	void *symbols[MAX_BACKTRACE_SYMBOLS];
	const int i = backtrace(symbols, MAX_BACKTRACE_SYMBOLS);
	backtrace_symbols_fd(symbols, i, STDERR_FILENO);
#endif
}

void Sys_Init (void)
{
	sys_os = Cvar_Get("sys_os", "linux", CVAR_SERVERINFO, NULL);
	sys_affinity = Cvar_Get("sys_affinity", "0", CVAR_ARCHIVE, NULL);
	sys_priority = Cvar_Get("sys_priority", "0", CVAR_ARCHIVE, "Process nice level");
}


/**
 * @brief Catch kernel interrupts and dispatch the appropriate exit routine.
 */
static void Sys_Signal (int s)
{
	switch (s) {
	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		Com_Printf("Received signal %d, quitting..\n", s);
		Sys_Quit();
		break;
#ifdef HAVE_CURSES
	case SIGWINCH:
		Curses_Resize();
		break;
#endif
	default:
		Sys_Backtrace();
		Sys_Error("Received signal %d.\n", s);
		break;
	}
}

/**
 * @brief The entry point for linux server and client.
 * Inits the the program and calls @c Qcommon_Frame in an infinite loop.
 */
int main (int argc, const char **argv)
{
	Sys_ConsoleInit();
	Qcommon_Init(argc, argv);

	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

#ifdef HAVE_CURSES
	signal(SIGWINCH, Sys_Signal);
#endif
	signal(SIGHUP, Sys_Signal);
	signal(SIGINT, Sys_Signal);
	signal(SIGQUIT, Sys_Signal);
	signal(SIGILL, Sys_Signal);
	signal(SIGABRT, Sys_Signal);
	signal(SIGFPE, Sys_Signal);
	signal(SIGSEGV, Sys_Signal);
	signal(SIGTERM, Sys_Signal);

	while (1)
		Qcommon_Frame();
}
