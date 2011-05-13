/**
 * @file sv_log.h
 * @brief game lib logging handling
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

#ifndef SV_LOG_H
#define SV_LOG_H

#include <stdarg.h>

void SV_LogHandleOutput(void);
void SV_LogAdd(const char *format, va_list ap);
void SV_LogInit(void);
void SV_LogShutdown(void);

#endif
