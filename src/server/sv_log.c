/**
 * @file sv_log.c
 * @brief game lib logging handling
 * @note we need this because the game lib logic can run in a separate thread
 * and could cause some systems to hang if we would use Com_Printf indirectly
 */

/*
 All original material Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "../common/dbuffer.h"

static struct dbuffer *logBuffer = NULL;

void SV_LogHandleOutput (void)
{
	char buf[1024];
	int length;

	while ((length = NET_ReadString(logBuffer, buf, sizeof(buf))) > 0)
		Com_Printf("%s", buf);
}

void SV_LogAdd (const char *format, va_list ap)
{
	char msg[1024];
	Q_vsnprintf(msg, sizeof(msg), format, ap);

	if (logBuffer == NULL)
		logBuffer = new_dbuffer();

	dbuffer_add(logBuffer, msg, strlen(msg) + 1);
}
