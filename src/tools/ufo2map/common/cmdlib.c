/**
 * @file cmdlib.c
 * @brief
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

#include "shared.h"
#include "cmdlib.h"

/**
 * @brief
 */
void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}

/**
 * @brief return nonzero if printing should be aborted based on the command line verbosity
 * level and the importance of the message
 * @param msgVerbLevel insignificance of the message. Larger numbers mean the message is
 * less important. The message will first be printed if the msgVerbLevel is equal to the config.verbosity.
 * @sa verbosityLevel_t
 */
qboolean AbortPrint (const verbosityLevel_t msgVerbLevel)
{
	return (msgVerbLevel > config.verbosity);
}

/**
 * @brief decides wether to proceed with output based on verbosity level
 * @sa Com_Printf, Check_Printf, AbortPrint
 */
void Verb_Printf (const verbosityLevel_t msgVerbLevel, const char *format, ...)
{
	if (AbortPrint(msgVerbLevel))
		return;

	{
		char out_buffer[4096];
		va_list argptr;

		va_start(argptr, format);
		Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
		va_end(argptr);

		printf("%s", out_buffer);
	}
}
