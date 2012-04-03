/**
 * @file
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

#include <Cocoa/Cocoa.h>
#include <SDL_main.h>
#include <fcntl.h>

#include "../../common/common.h"


/**
 * @brief Sets the current working directory to the top of the
 * application bundle hierarchy.
 */
static void SetWorkingDirectory ()
{
	if (NSBundle* const mainBundle = [NSBundle mainBundle]) {
		char const* const newPath = [[mainBundle bundlePath] UTF8String];
		Com_DPrintf(DEBUG_SYSTEM, "%s = new path\n", newPath);
		chdir(newPath);
	} else {
		Com_Printf("Main bundle appears to be NULL!\n");
	}
}

/**
 * @brief The entry point for OSX server and client.
 *
 * Inits the the program and calls Qcommon in an infinite loop.
 * FIXME: While this works, infinite loops are bad; one should not rely on exit() call; the program should be designed to fall through the bottom.
 */
int main (int argc, char **argv)
{
	SetWorkingDirectory();

	Sys_ConsoleInit();
	Qcommon_Init(argc, argv);
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	while (1)
		Qcommon_Frame();

	return 0;
}
