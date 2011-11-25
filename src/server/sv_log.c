/**
 * @file sv_log.c
 * @brief game lib logging handling
 * @note we need this because the game lib logic can run in a separate thread
 * and could cause some systems to hang if we would use Com_Printf indirectly
 */

/*
 All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "server.h"
#include "sv_log.h"
#include "../shared/mutex.h"
#include "../shared/stringhunk.h"

static threads_mutex_t *svLogMutex;
static stringHunk_t *svLogHunk;

static void SV_LogPrintOutput (const char *string)
{
	Com_Printf("%s", string);
}

/**
 * @brief Handle the log output from the main thread by reading the strings
 * from the dbuffer the game lib thread has written into.
 */
void SV_LogHandleOutput (void)
{
	TH_MutexLock(svLogMutex);
	STRHUNK_Visit(svLogHunk, SV_LogPrintOutput);
	STRHUNK_Reset(svLogHunk);
	TH_MutexUnlock(svLogMutex);
}

/**
 * @brief Async version to add a log entry for the game lib.
 * @note This is needed because using @c Com_Printf from within
 * the game lib thread might freeze some systems as the console
 * print functions are not thread safe.
 * @param format The format of the message
 * @param ap The variadic function argument list to fill the format strings
 */
void SV_LogAdd (const char *format, va_list ap)
{
	char msg[1024];

	Q_vsnprintf(msg, sizeof(msg), format, ap);

	TH_MutexLock(svLogMutex);
	STRHUNK_Add(svLogHunk, msg);
	TH_MutexUnlock(svLogMutex);
}

void SV_LogInit (void)
{
	const size_t svHunkSize = 32768;
	svLogMutex = TH_MutexCreate("sv_log");
	svLogHunk = STRHUNK_Create(svHunkSize);
}

void SV_LogShutdown (void)
{
	TH_MutexDestroy(svLogMutex);
	svLogMutex = NULL;
	STRHUNK_Delete(&svLogHunk);
}
