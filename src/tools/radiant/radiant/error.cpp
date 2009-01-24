/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "error.h"

#include "debugging/debugging.h"
#include "igl.h"

#include "gtkutil/messagebox.h"
#include "console.h"
#include "preferences.h"

#include <errno.h>


#ifdef WIN32
#define UNICODE
#include <windows.h>
#else
#include <unistd.h>
#endif



/**
 * @todo the prompt wether to do prefs dialog, may not even be possible
 * if the crash happens before the game is loaded
 * @brief For abnormal program terminations
 */

void Error (const char *error, ...)
{
	va_list argptr;
	char text[4096];

	va_start(argptr, error);
	vsnprintf(text, sizeof(text) - 1, error, argptr);
	va_end(argptr);

	strcat(text, "\n");

	/** @todo this can still overflow - unlikely, but possible */
	if (errno != 0) {
		strcat(text, g_strerror(errno));
		strcat(text, "\n");
	}

	ERROR_MESSAGE(text);

	// force close logging if necessary
	Sys_LogFile(false);

	_exit(1);
}
