/**
 * @file
 * @brief Shared parsing functions.
 */

/*
All original material Copyright (C) 2002-2015 UFO: Alien Invasion.

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
static bool isUnparsedToken;
static Com_TokenType_t type;

/**
 * @brief Put back the last token into the parser
 * The next call of Com_Parse will return the same token again
 * @note Only allow to use it one time
 * @note With it, we can't read to file at the same time
 */
void Com_UnParseLastToken (void)
{
	isUnparsedToken = true;
}

/**
 * @brief Get the current token value
 * @return The current token value
 */
const char* Com_GetToken (const char** data_p)
{
	return com_token;
}

/**
 * @brief Get the current token type
 * @return The current token type
 */
Com_TokenType_t Com_GetType (const char** data_p)
{
	return type;
}

/**
 * @brief Compute the next token
 * @return Type of the next token
 */
Com_TokenType_t Com_NextToken (const char** data_p)
{
	Com_Parse(data_p);
	return type;
}

/**
 * @brief Counts the tokens in the given buffer that the Com_Parse function would extract
 * @param[in] buffer The buffer to count the tokens in
 * @return The amount of tokens in the given buffer
 */
int Com_CountTokensInBuffer (const char* buffer)
{
	const char* text = buffer;
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
 * @param target A place to put the parsed data. If 0, an internal buffer of 4096 is used.
 * @param size The length of target buffer, if any
 * @param replaceWhitespaces Replace "\\t" and "\\n" to "\t" and "\n"
 * @pre @c data_p is expected to be null-terminated
 * @return The string result of parsing in a string.
 * @note if used to parse a quoted string that needs further parsing
 * the returned string must be copied first, as Com_Parse returns a
 * pointer to a static buffer that it holds. this will be re-used on
 * the next call.
 * @sa Com_EParse
 */
const char* Com_Parse (const char* data_p[], char* target, size_t size, bool replaceWhitespaces)
{
	char c;
	size_t len;
	const char* data;

	if (!target) {
		target = com_token;
		size = sizeof(com_token);
	}

	if (isUnparsedToken) {
		isUnparsedToken = false;
		return target;
	}

	data = *data_p;
	len = 0;
	target[0] = '\0';

	if (!data) {
		*data_p = nullptr;
		type = TT_EOF;
		return "";
	}

	/* skip whitespace */
skipwhite:
	while ((c = *data) <= ' ') {
		if (c == 0) {
			*data_p = nullptr;
			type = TT_EOF;
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
		data++;
		for (;;) {
			c = *data++;
			if (c == '\\' && data[0] == 'n') {
				if (replaceWhitespaces) {
					c = '\n';
					data++;
				}
			} else if (c == '\\' && data[0] == 't') {
				if (replaceWhitespaces) {
					c = '\t';
					data++;
				}
			/* nested quotation */
			} else if (c == '\\' && data[0] == '\"') {
				c = '\"';
				data++;
			} else if (c == '\"' || !c) {
				target[len] = '\0';
				*data_p = data;
				type = TT_QUOTED_WORD;
				return target;
			} else if (c == '\0') {
				// TODO here the token is wrongly formed
				break;
			}

			if (len < size) {
				if (c != '\r') {
					target[len] = c;
					len++;
				}
			} else {
				/* exceeded com_token size */
				break;
			}
		}

		if (len == size) {
			len = 0;
		}

		// TODO here the token is wrongly formed
		target[len] = '\0';
		*data_p = data;
		type = TT_QUOTED_WORD;
		return target;
	}

	if (c == '{' || c == '}' || c == '(' || c == ')' || c == ',') {
		data++;
		target[len] = c;
		target[len + 1] = '\0';
		// Com_TokenType_t contains expected values for this set of characters
		type = (Com_TokenType_t) c;
		len++;
		*data_p = data;
		return target;
	}

	/* parse a regular word */
	do {
		if (len < size) {
			target[len] = c;
			len++;
		} else {
			/* exceeded com_token size */
			break;
		}
		data++;
		c = *data;
		if (c == '{' || c == '}' || c == '(' || c == ')' || c == ',')
			break;
	} while (c > ' ');

	if (len == size) {
		len = 0;
	}
	target[len] = '\0';

	*data_p = data;
	type = TT_WORD;
	return target;
}

/**
 * @brief Skips a block of {} in our script files
 * @param[in,out] text The text pointer to skip the block in. This pointer is set to the end of the block
 * @note Even multiple depth levels are supported. Skipping a block with nested blocks works, too.
 */
void Com_SkipBlock (const char** text)
{
	int depth = 1;

	do {
		const char* token = Com_Parse(text);
		if (*token == '{')
			depth++;
		else if (*token == '}')
			depth--;
	} while (depth && *text);
}

/**
 * @brief Get the start and end point of a block in the given text
 * @param[in,out] text The text pointer to get the start and end for. The pointer is set to the end of the block in this function.
 * @param[out] start The pointer to the beginning of the block
 * @return The amount of characters in the block, or -1 if the text does not start with the script block character '{'
 */
int Com_GetBlock (const char** text, const char** start)
{
	const char* token = Com_Parse(text);
	if (*token != '{')
		return -1;
	*start = *text;
	Com_SkipBlock(text);
	const char* end = *text - 1;	/* The pointer to the end of the block */
	return end - *start;
}
