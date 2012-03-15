/**
 * @file android_system.c
 * @brief Some android specific functions
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <dirent.h>

#include "../../common/common.h"
#include "../system.h"

#include <android/log.h>
#include "../android/android_debugger.h"

const char *Sys_GetCurrentUser (void)
{
	char * user = getenv("USER");
	if (user)
		return user;
	return "Player";
}

/**
 * @brief Errors out of the game.
 * @note The error message should not have a newline - it's added inside of this function
 * @note This function does never return
 */
void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	Sys_Backtrace();

#ifdef COMPILE_UFO
	Sys_ConsoleShutdown();
#endif

#ifdef COMPILE_MAP
	Mem_Shutdown();
#endif

	va_start(argptr,error);
	Q_vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	__android_log_print(ANDROID_LOG_FATAL, "UFOAI", "Error: %s", string);

	exit(1);
}

/**
 * @brief
 * @sa Qcommon_Shutdown
 */
void Sys_Quit (void)
{
#ifdef COMPILE_UFO
	CL_Shutdown();
	Qcommon_Shutdown();
	Sys_ConsoleShutdown();
#elif COMPILE_MAP
	Mem_Shutdown();
#endif

	exit(0);
}

/**
 * @brief On platforms supporting it, print a backtrace.
 */
void Sys_Backtrace (void)
{
	const char *dumpFile = "crashdump.txt";
	FILE *file = fopen(dumpFile, "w");
	FILE *crash = file != NULL ? file : stderr;

	fprintf(crash, "======start======\n");

	fprintf(crash, BUILDSTRING ", cpu: " CPUSTRING ", version: " UFO_VERSION "\n\n");
	fflush(crash);

	androidDumpBacktrace(crash);

	fprintf(crash, "======end========\n");

	if (file != NULL)
		fclose(file);

#ifdef COMPILE_UFO
	Com_UploadCrashDump(dumpFile);
#endif
}
