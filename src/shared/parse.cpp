/**
 * @file src/shared/parse.c
 * @brief Shared parsing functions.
 */

/*
All original material Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "parse.h"
#include "defines.h"
#include "ufotypes.h"

static char com_token[4096];
static qboolean isUnparsedToken;
static qboolean isQuotedToken;
static qboolean functionScriptTokenEnabled;

/**
 * @brief Put back the last token into the parser
 * The next call of Com_Parse will return the same token again
 * @note Only allow to use it one time
 * @note With it, we can't read to file at the same time
 */
void Com_UnParseLastToken (void)
{
	isUnparsedToken = qtrue;
}

/**
 * @brief Check if the last read token is quoted
 * @return True if the token is quoted
 * @sa Com_Parse
 */
qboolean Com_ParsedTokenIsQuoted (void)
{
	return isQuotedToken;
}

/**
 * @brief Enable parsing of token '(', ')' and ','
 * @param enable If true, enable parsing of extra tokens
 * @sa Com_Parse
 */
void Com_EnableFunctionScriptToken (qboolean enable)
{
	functionScriptTokenEnabled = enable;
}

/**
 * @brief Counts the tokens in the given buffer that the Com_Parse function would extract
 * @param[in] buffer The buffer to count the tokens in
 * @return The amount of tokens in the given buffer
 */
int Com_CountTokensInBuffer (const char *buffer)
{
	const char *text = buffer;
	int n = 0;
	for (;;) {
		Com_Parse(&text);
		if (!text)
			break;
		n++;
	}
	return n;
}

/**
 * @brief Parse a token out of a string
 * @param data_p Pointer to a string which is to be parsed
 * @pre @c data_p is expected to be null-terminated
 * @return The string result of parsing in a string.
 * @note if used to parse a quoted string that needs further parsing
 * the returned string must be copied first, as Com_Parse returns a
 * pointer to a static buffer that it holds. this will be re-used on
 * the next call.
 * @sa Com_EParse
 */
const char *Com_Parse (const char *data_p[])
{
	char c;
	size_t len;
	const char *data;

	if (isUnparsedToken) {
		isUnparsedToken = qfalse;
		return com_token;
	}

	data = *data_p;
	isQuotedToken = qfalse;
	len = 0;
	com_token[0] = 0;

	if (!data) {
		*data_p = NULL;
		return "";
	}

	/* skip whitespace */
skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0) {
			*data_p = NULL;
			return "";
		}
		data++;
	}

	if (c == '/' && data[1] == '*') {
		int clen = 0;
		data += 2;
		while (!((data[clen] && data[clen] == '*') && (data[clen + 1] && data[clen + 1] == '/'))) {
			clen++;
		}
		data += clen + 2; /* skip end of multiline comment */
		goto skipwhite;
	}

	/* skip // comments */
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	/* handle quoted strings specially */
	if (c == '\"') {
		isQuotedToken = qtrue;
		data++;
		for (;;) {
			c = *data++;
			if (c == '\\' && data[0] == 'n') {
				c = '\n';
				data++;
			} else if (c == '\\' && data[0] == 't') {
				c = '\t';
				data++;
			/* nested quotation */
			} else if (c == '\\' && data[0] == '\"') {
				c = '\"';
				data++;
			} else if (c == '\"' || !c) {
				com_token[len] = '\0';
				*data_p = data;
				return com_token;
			} else if (c == '\0') {
				break;
			}

			if (len < sizeof(com_token)) {
				com_token[len] = c;
				len++;
			}
		}
		com_token[len] = '\0';
		*data_p = data;
		return com_token;
	}

	if ((c == '{' || c == '}') || (functionScriptTokenEnabled && (c == '(' || c == ')' || c == ','))) {
		data++;
		com_token[len] = c;
		com_token[len + 1] = '\0';
		len++;
		*data_p = data;
		return com_token;
	}

	/* parse a regular word */
	do {
		if (len < sizeof(com_token)) {
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
		if (c == '{' || c == '}')
			break;
		if (functionScriptTokenEnabled && (c == '(' || c == ')' || c == ','))
			break;
	} while (c > 32);

	if (len == sizeof(com_token)) {
		len = 0;
	}
	com_token[len] = '\0';

	*data_p = data;
	return com_token;
}
