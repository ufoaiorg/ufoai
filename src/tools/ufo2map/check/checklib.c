/**
 * @file checklib.c
 * @brief functions for check.c
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

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


#include "../common/cmdlib.h"
#include "../common/shared.h"
#include "../ufo2map.h"
#include "checklib.h"

/**
 * @brief decides wether to proceed with output based on verbosity and ufo2map's mode: check/fix/compile
 * @param change true if there will an automatic change on -fix
 * @param brushnum the brush that the report is about. send NUM_NONE, if the report only regards an entity
 * @param entnum the entity the brush is from.send NUM_NONE if the report is a summary, not regarding a specific entity or brush
 * @note for brushnum and entnum send NUM_SAME in multi-call messages to indicate that the message still regards the same brush or entity
 * @note the start of a report on a particular item (eg brush) is specially prefixed. The function notes newlines in the previous
 * call, and prefixes this message with a string depending on wether the message indicates a change and the entity and brush number.
 * @note if entnum is set to NUM_SAME, then msgVerbLevel is carried over from the previous call.
 * @sa Com_Printf, Verb_Printf, DisplayContentFlags
 * @callergraph
 */
void Check_Printf (verbosityLevel_t msgVerbLevel, qboolean change,
							int entnum, int brushnum, const char *format, ...)
{
	static int skippingCheckLine = 0;
	static int lastMsgVerbLevel = VERB_NORMAL;
	static qboolean firstSuccessfulPrint = qtrue;
	static qboolean startOfLine = qtrue;
	const qboolean containsNewline = strchr(format, '\n') != NULL;

	/* some checking/fix functions are called when ufo2map is compiling
	 * then the check/fix functions should be quiet */
	if (!(config.performMapCheck || config.fixMap))
		return;

	if (entnum == NUM_SAME)
		msgVerbLevel = lastMsgVerbLevel;

	lastMsgVerbLevel = msgVerbLevel;

	if (AbortPrint(msgVerbLevel))
		return;

	/* output prefixed with "  " is only a warning, should not be
	 * be displayed in fix mode. may be sent here in several function calls.
	 * skip everything from start of line "  " to \n */
	if (config.fixMap) {
		/* skip warning output sent in single call */
		if (!skippingCheckLine && startOfLine && !change && containsNewline)
			return;

		/* enter multi-call skip mode */
		if (!skippingCheckLine && startOfLine && !change) {
			skippingCheckLine = 1;
			return;
		}

		/* leave multi-call skip mode */
		if (skippingCheckLine && containsNewline) {
			skippingCheckLine = 0;
			return;
		}

		/* middle of multi-call skip mode */
		if (skippingCheckLine)
			return;
	}

	if (firstSuccessfulPrint && config.verbosity == VERB_MAPNAME) {
		PrintMapName();
		firstSuccessfulPrint = qfalse;
	}

	{
		char out_buffer1[4096];
		{
			va_list argptr;

			va_start(argptr, format);
			Q_vsnprintf(out_buffer1, sizeof(out_buffer1), format, argptr);
			va_end(argptr);
		}

		if (startOfLine) {
			const char *prefix;
			prefix = change ? "* " : "  ";
			prefix = (brushnum == NUM_NONE && entnum == NUM_NONE) ? "//" : prefix;

			printf("%sent:%i brush:%i - %s", prefix, entnum, brushnum, out_buffer1);
		} else {
			printf("%s", out_buffer1);
		}
	}

	/* ensure next call gets brushnum and entnum printed if this is the end of the previous*/
	startOfLine = containsNewline ? qtrue : qfalse;
}
