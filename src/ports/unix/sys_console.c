/**
 * @file sys_console.c
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

#include <termios.h>
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


#include "../../qcommon/qcommon.h"

qboolean stdin_active = qtrue;
cvar_t *nostdout;

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
void Sys_Printf (const char *fmt, ...)
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
void Sys_ConsoleOutput (const char *string)
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
