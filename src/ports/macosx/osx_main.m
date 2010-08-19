/**
 * @file osx_main.m
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

#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <dlfcn.h>

#include "../../common/common.h"
#include "../system.h"
#include "osx_main.h"

@interface NSApplication(SDL_Missing_Methods)
- (void)setAppleMenu:(NSMenu *)menu;
@end


typedef struct CPSProcessSerNum
{
	UInt32		lo;
	UInt32		hi;
} CPSProcessSerNum;

extern OSErr CPSGetCurrentProcess(CPSProcessSerNum *psn);
extern OSErr CPSEnableForegroundOperation(CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr CPSSetFrontProcess(CPSProcessSerNum *psn);

/* ======================================================================= */
/* General routines */
/* ======================================================================= */

void Sys_Init (void)
{
	sys_os = Cvar_Get("sys_os", "macosx", CVAR_SERVERINFO, NULL);
	sys_affinity = Cvar_Get("sys_affinity", "0", CVAR_ARCHIVE, NULL);
	sys_priority = Cvar_Get("sys_priority", "0", CVAR_ARCHIVE, "Process nice level");
}

static void InitCocoa (void)
{
	void (*nsappload)(void);
	void* cocoa_lib;
	/* @todo: Don't hardcode the path - let configure decide */
	cocoa_lib = dlopen("/System/Library/Frameworks/Cocoa.framework/Cocoa", RTLD_LAZY);
	if (!cocoa_lib)
		Sys_Error("InitCocoa: Could not load cocoa framework\n");
	nsappload = (void(*)()) dlsym(cocoa_lib, "NSApplicationLoad");
	nsappload();
}

/**
 * @brief Sets the current working directory to the top of the
 * application bundle hierarchy.
 */
static void SetWorkingDirectory (const char **argv)
{
	char newPath[MAX_OSPATH];
	NSBundle *mainBundle;
	int strLength = strlen(*argv);

	Com_DPrintf(DEBUG_SYSTEM, "Path Length : %d \n", strLength);
	mainBundle = [NSBundle mainBundle];

	if (strLength >= sizeof(newPath)) {
		Com_Printf("Path is too long. Please copy Bundle to a shorter path location \n");
	} else {
		if (mainBundle != NULL) {
			strncpy(newPath, [[mainBundle bundlePath] UTF8String], sizeof(newPath));
			Com_DPrintf(DEBUG_SYSTEM, "%s = new path\n", newPath);
			chdir(newPath);
		} else {
			Com_Printf("Main bundle appears to be NULL!\n");
		}
	}
}
@implementation osx_main
@end
@implementation SDLApplication : NSApplication
@end

/**
 * @brief The entry point for OSX server and client.
 *
 * Inits the the program and calls Qcommon in an infinite loop.
 * FIXME: While this works, infinite loops are bad; one should not rely on exit() call; the program should be designed to fall through the bottom.
 */
#ifdef main
#undef main
#endif
int main (int argc, const char **argv)
{
	/* create Autorelease Pool, to avoid Error Messages under MacOSX */
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	InitCocoa();

	[SDLApplication sharedApplication];
	{
		CPSProcessSerNum PSN;
		if (!CPSGetCurrentProcess(&PSN))
			if (!CPSEnableForegroundOperation(&PSN, 0x03, 0x3C, 0x2C, 0x1103))
				if (!CPSSetFrontProcess(&PSN))
					[SDLApplication sharedApplication];
	}

	SetWorkingDirectory(argv);

	Sys_ConsoleInit();
	Qcommon_Init(argc, argv);
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | FNDELAY);

	while (1)
		Qcommon_Frame();

	/* Free the Release Pool resources */
	[pool release];
	return 0;
}
