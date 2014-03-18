/**
 * @file
 * @todo Remove this and use parse.c
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
#include "scriplib.h"

/*
=============================================================================
PARSING STUFF
=============================================================================
*/

typedef struct {
	char filename[MAX_OSPATH];
	char* buffer;
	const char* script_p;
	const char* end_p;
} script_t;

static script_t script;

char parsedToken[MAX_TOKEN_CHARS];

void LoadScriptFile (const char* filename)
{
	strncpy(script.filename, filename, sizeof(script.filename));

	const int size = FS_LoadFile(script.filename, (byte**)&script.buffer);
	if (size == -1)
		Sys_Error("file '%s' doesn't exist", script.filename);

	script.script_p = script.buffer;
	script.end_p = script.buffer + size;
}

/**
 * @brief Parses e.g. the entity string that is already stored in memory
 */
void ParseFromMemory (char* buffer, int size)
{
	Q_strncpyz(script.filename, "memory buffer", sizeof(script.filename));

	script.buffer = buffer;
	script.script_p = script.buffer;
	script.end_p = script.buffer + size;
}

/**
 * @brief Parses the next token from the current script on the stack
 * and store the result in @c parsedToken
 */
const char* GetToken ()
{
	const char* token = Com_Parse(&script.script_p, parsedToken, sizeof(parsedToken));
	if (!script.script_p) {
		/* not if the current script is a memory buffer */
		if (!Q_streq(script.filename, "memory buffer"))
			Mem_Free(script.buffer);
		assert(Q_strnull(parsedToken));
		return parsedToken;
	}

	return token;
}

/**
 * @brief Returns true if there is another token on the line
 */
bool TokenAvailable (void)
{
	const char* search_p = script.script_p;

	if (search_p >= script.end_p)
		return false;

	while (*search_p <= ' ') {
		if (*search_p == '\n')
			return false;
		search_p++;
		if (search_p == script.end_p)
			return false;
	}

	return true;
}
