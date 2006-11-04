/**
 * @file scripts.c
 * @brief Parses the ufo scripts as needed for campaign editor
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "scripts.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

static char headerType[32];
static char headerName[32];
static char com_token[1024];

/**
 * @brief Skips all {} blocks even with a deep greater then 1
 */
void skip_block(char **text)
{
	char *token;
	int depth = 1;

	do {
		token = parse_token(text);
		if (*token == '{')
			depth++;
		else if (*token == '}')
			depth--;
	} while (depth && *text);
}

/**
 * @brief Returns the next token from input string
 * @param[in] string The input string to get the next token from
 */
char *parse_token (char **data_p)
{
	static char com_token[4096];
	int c;
	size_t len;
	char *data;

	data = *data_p;
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

	/* skip // comments */
	if (c == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	/* handle quoted strings specially */
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c == '\"' || !c) {
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < sizeof(com_token)) {
				com_token[len] = c;
				len++;
			} else {
				printf("parse_token: len exceeded: %i/MAX_TOKEN_CHARS\n", len);
			}
		}
	}

	/* parse a regular word */
	do {
		if (len < sizeof(com_token)) {
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > 32);

	if (len == sizeof(com_token)) {
		printf("Token exceeded %i chars, discarded.\n", sizeof(com_token));
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

/**
 * @brief Loads the first token of a scriptfile
 * @return the first token from scriptfile or NULL on error
 */
char* loadscript (char **name, char **text)
{
	char *token;

	if (!text)
		return NULL;

	if (*text) {
		token = parse_token(text);
		if (*token == '{') {
			skip_block(text);
			token = parse_token(text);
		}

		strncpy(headerType, token, sizeof(headerType));
		if (*text) {
			token = parse_token(text);
			strncpy(headerName, token, sizeof(headerName));
			*name = headerName;
			return headerType;
		}
	}
	return NULL;
}
