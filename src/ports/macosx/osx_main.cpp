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

#include <CoreFoundation/CoreFoundation.h>
#include <SDL_main.h>
#include <fcntl.h>
#include <sys/param.h>
#include <unistd.h>

#include "../../common/common.h"

/**
 * @brief Sets the current working directory to the top of the
 * application bundle hierarchy.
 */
static void SetWorkingDirectory ()
{
	bool success = false;
	if (CFBundleRef const mainBundle = CFBundleGetMainBundle()) {
		if (CFURLRef const url = CFBundleCopyBundleURL(mainBundle)) {
			char path[MAXPATHLEN];
			if (CFURLGetFileSystemRepresentation(url, true, reinterpret_cast<UInt8*>(path), sizeof(path))) {
				Com_DPrintf(DEBUG_SYSTEM, "%s = new path\n", path);
				chdir(path);
				success = true;
			}
			CFRelease(url);
		}
	}
	if (!success)
		Com_Printf("Failed to get path of main bundle\n");
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
