/**
 * @file shared.c
 * @brief Shared functions
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

#include "../common/common.h"

/**
 * @brief Returns just the filename from a given path
 * @sa COM_StripExtension
 */
const char *COM_SkipPath (char *pathname)
{
	const char *last;

	last = pathname;
	while (*pathname) {
		if (*pathname == '/')
			last = pathname + 1;
		pathname++;
	}
	return last;
}

/**
 * @brief Removed the file extension from a filename
 * @sa COM_SkipPath
 * @param[in] size The size of the output buffer
 */
void COM_StripExtension (const char *in, char *out, size_t size)
{
	char *out_ext = NULL;
	int i = 1;

	while (*in && i < size) {
		*out++ = *in++;
		i++;

		if (*in == '.')
			out_ext = out;
	}

	if (out_ext)
		*out_ext = 0;
	else
		*out = 0;
}

/**
  * @brief Returns the path up to, but not including the last /
  */
void COM_FilePath (const char *in, char *out)
{
	const char *s;

	s = in + strlen(in) - 1;

	while (s != in && *s != '/')
		s--;

	/* FIXME: Q_strncpyz */
	strncpy(out, in, s - in);
	out[s - in] = 0;
}

/**
 * @brief Parse a token out of a string
 * @param data_p Pointer to a string which is to be parsed
 * @pre @c data_p is expected to be null-terminated
 * @return The string result of parsing in a string.
 * @sa COM_EParse
 */
const char *COM_Parse (const char *data_p[])
{
	static char com_token[4096];
	int c;
	size_t len;
	const char *data;

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

	if (c == '/' && data[1] == '*') {
		int clen = 0;
		data += 2;
		while (!((data[clen] && data[clen] == '*') && (data[clen+1] && data[clen+1] == '/'))) {
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
		while (1) {
			c = *data++;
			if (c == '\\' && data[0] == 'n') {
				c = '\n';
				data++;
			} else if (c == '\"' || !c) {
				com_token[len] = 0;
				*data_p = data;
				return com_token;
			}
			if (len < sizeof(com_token)) {
				com_token[len] = c;
				len++;
			} else {
				Com_Printf("Com_Parse len exceeded: "UFO_SIZE_T"/MAX_TOKEN_CHARS\n", len);
			}
		}
	}

	/* parse a regular word */
	do {
		if (c == '\\' && data[1] == 'n') {
			c = '\n';
			data++;
		}
		if (len < sizeof(com_token)) {
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > 32);

	if (len == sizeof(com_token)) {
		Com_Printf("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = data;
	return com_token;
}
