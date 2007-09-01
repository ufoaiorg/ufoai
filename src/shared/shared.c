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
 * @param
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
 * @param
 * @sa COM_SkipPath
 */
void COM_StripExtension (const char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/**
 * @brief
 * @param
 * @sa
 */
void COM_FileBase (const char *in, char *out)
{
	const char *s, *s2;

	s = in + strlen(in) - 1;

	while (s != in && *s != '.')
		s--;

	for (s2 = s; s2 != in && *s2 != '/'; s2--);

	if (s - s2 < 2)
		out[0] = 0;
	else {
		s--;
		strncpy(out, s2 + 1, s - s2);
		out[s - s2] = 0;
	}
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
