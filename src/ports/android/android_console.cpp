/**
 * @file android_console.c
 * @brief console functions for andriod ports
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
#include <android/log.h>


void Sys_ShowConsole (qboolean show)
{
}

void Sys_ConsoleShutdown (void)
{
}

/**
 * @brief Initialize the console input (tty mode if possible)
 */
void Sys_ConsoleInit (void)
{
}

const char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_ConsoleOutput (const char *string)
{
	__android_log_print(ANDROID_LOG_INFO, "UFOAI", "%s", string);
}
