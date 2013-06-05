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
	char	filename[MAX_OSPATH];
	char	*buffer;
	char	*script_p;
	char	*end_p;
	int     line;
} script_t;

static script_t script;

char parsedToken[MAX_TOKEN_CHARS];

int GetScriptLine (void)
{
	return script.line;
}

/**
 * @sa LoadScriptFile
 */
static void AddScriptToStack (const char *filename)
{
	int size;

	strncpy(script.filename, filename, sizeof(script.filename));

	size = FS_LoadFile(script.filename, (byte **)&script.buffer);
	if (size == -1)
		Sys_Error("file '%s' doesn't exist", script.filename);

	script.line = 1;

	script.script_p = script.buffer;
	script.end_p = script.buffer + size;
}

void LoadScriptFile (const char *filename)
{
	AddScriptToStack(filename);
}

/**
 * @brief Parses e.g. the entity string that is already stored in memory
 */
void ParseFromMemory (char *buffer, int size)
{
	Q_strncpyz(script.filename, "memory buffer", sizeof(script.filename));

	script.buffer = buffer;
	script.line = 1;
	script.script_p = script.buffer;
	script.end_p = script.buffer + size;
}

static bool EndOfScript ()
{
	/* not if the current script is a memory buffer */
	if (!Q_streq(script.filename, "memory buffer"))
		Mem_Free(script.buffer);
	return false;
}

/**
 * @brief Parses the next token from the current script on the stack
 * and store the result in parsedToken
 * @param[in] crossline The next token may not be seperated by
 * comment or newline if this is true - everything must be on the same line
 */
bool GetToken ()
{
	char *token_p;

	if (script.script_p >= script.end_p)
		return EndOfScript();

	/* skip space */
skipspace:
	while (*script.script_p <= ' ') {
		if (script.script_p >= script.end_p)
			return EndOfScript();
		if (*script.script_p++ == '\n') {
			script.line++;
		}
	}

	if (script.script_p >= script.end_p)
		return EndOfScript();

	/* // comments */
	if (script.script_p[0] == '/' && script.script_p[1] == '/') {
		while (*script.script_p++ != '\n')
			if (script.script_p >= script.end_p)
				return EndOfScript();
		script.line++;
		goto skipspace;
	}

	/* copy token */
	token_p = parsedToken;

	if (*script.script_p == '"') {
		/* quoted token */
		script.script_p++;
		while (*script.script_p != '"') {
			*token_p++ = *script.script_p++;
			if (script.script_p == script.end_p)
				break;
			if (token_p == &parsedToken[MAX_TOKEN_CHARS])
				Sys_Error("Token too large on line %i", script.line);
		}
		script.script_p++;
	} else	/* regular token */
		while (*script.script_p > ' ') {
			*token_p++ = *script.script_p++;
			if (script.script_p == script.end_p)
				break;
			if (token_p == &parsedToken[MAX_TOKEN_CHARS])
				Sys_Error("Token too large on line %i", script.line);
		}

	*token_p = 0;

	return true;
}

/**
 * @brief Returns true if there is another token on the line
 */
bool TokenAvailable (void)
{
	const char *search_p = script.script_p;

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
