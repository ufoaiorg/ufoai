/**
 * @file unix_console.c
 * @brief console functions for *nix ports
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

#include "../../common/common.h"
#include "../system.h"
#include <unistd.h>

#ifdef HAVE_CURSES
#include "unix_curses.h"
#endif /* HAVE_CURSES */

void Sys_ShowConsole (qboolean show)
{
}

/**
 * @brief Shutdown the console
 */
void Sys_ConsoleShutdown (void)
{
#ifdef HAVE_CURSES
	Curses_Shutdown();
#endif
}

/**
 * @brief initialise the console
 */
void Sys_ConsoleInit (void)
{
#ifdef HAVE_CURSES
	Curses_Init();
#endif /* HAVE_CURSES */
}

const char *Sys_ConsoleInput (void)
{
#ifdef HAVE_CURSES
	return Curses_Input();
#else
	static qboolean stdinActive = qtrue;
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!stdinActive)
		return NULL;

	FD_ZERO(&fdset);
	FD_SET(STDIN_FILENO, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select(1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
		return NULL;

	len = read(STDIN_FILENO, text, sizeof(text));
	if (len == 0) { /* eof! */
		stdinActive = qfalse;
		return NULL;
	}

	if (len < 1)
		return NULL;
	text[len - 1] = '\0'; /* rip off the \n and terminate */

	return text;
#endif /* HAVE_CURSES */
}

void Sys_ConsoleOutput (const char *string)
{
#ifdef HAVE_CURSES
	Curses_Output(string);
#else
	/* skip color char */
	if (!strncmp(string, COLORED_GREEN, strlen(COLORED_GREEN)))
		string++;

	fputs(string, stdout);
#endif /* HAVE_CURSES */
}
