/**
 * @file q_shared.c
 * @brief Common functions.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/game/q_shared.c
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

#include "q_shared.h"

/** @brief Player action format strings for netchannel transfer */
const char *pa_format[] =
{
	"",					/**< PA_NULL */
	"b",				/**< PA_TURN */
	"g",				/**< PA_MOVE */
	"s",				/**< PA_STATE - don't use a bitmask here - only one value
						 * @sa G_ClientStateChange */
	"gbbl",				/**< PA_SHOOT */
	"s",					/**< PA_USE_DOOR */
	"bbbbbb",			/**< PA_INVMOVE */
	"sss",				/**< PA_REACT_SELECT */
	"sss"				/**< PA_RESERVE_STATE */
};

/*===========================================================================*/

/**
 * @brief Compare two strings
 * @param[in] string1 The first string
 * @param[in] string2 The second string
 * @return An integer less than, equal to, or greater than zero if string1 is
 * found, respectively, to be less than,  to match, or be greater than string2
 * @note sort function pointer for qsort
 */
int Q_StringSort (const void *string1, const void *string2)
{
	const char *s1, *s2;
	s1 = string1;
	s2 = string2;
	if (*s1 < *s2)
		return -1;
	else if (*s1 == *s2) {
		while (*s1) {
			s1++;
			s2++;
			if (*s1 < *s2)
				return -1;
			else if (*s1 == *s2) {
				;
			} else
				return 1;
		}
		return 0;
	} else
		return 1;
}

/*============================================================================ */

/**
 * @return PSIDE_FRONT, PSIDE_BACK, or PSIDE_BOTH
 */
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cBspPlane_s *p)
{
	float dist1, dist2;
	int sides;

	/* fast axial cases */
	if (p->type < 3) {
		if (p->dist <= emins[p->type])
			return PSIDE_FRONT;
		if (p->dist >= emaxs[p->type])
			return PSIDE_BACK;
		return PSIDE_BOTH;
	}

	/* general case */
	switch (p->signbits) {
	case 0:
		dist1 = DotProduct(p->normal, emaxs);
		dist2 = DotProduct(p->normal, emins);
		break;
	case 1:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;
	case 2:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;
	case 3:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;
	case 4:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;
	case 7:
		dist1 = DotProduct(p->normal, emins);
		dist2 = DotProduct(p->normal, emaxs);
		break;
	default:
		dist1 = dist2 = 0;		/* shut up compiler */
		assert(0);
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = PSIDE_FRONT;
	if (dist2 < p->dist)
		sides |= PSIDE_BACK;

	assert(sides != 0);

	return sides;
}

#define VA_BUFSIZE 4096
/**
 * @brief does a varargs printf into a temp buffer, so I don't need to have
 * varargs versions of all text functions.
 */
char *va (const char *format, ...)
{
	va_list argptr;
	/* in case va is called by nested functions */
	static char string[2][VA_BUFSIZE];
	static int index = 0;
	char *buf;

	buf = string[index & 1];
	index++;

	va_start(argptr, format);
	Q_vsnprintf(buf, VA_BUFSIZE, format, argptr);
	va_end(argptr);

	buf[VA_BUFSIZE-1] = 0;

	return buf;
}

/**
 * @sa Com_Parse
 */
const char *COM_EParse (const char **text, const char *errhead, const char *errinfo)
{
	const char *token;

	token = COM_Parse(text);
	if (!*text) {
		if (errinfo)
			Com_Printf("%s \"%s\")\n", errhead, errinfo);
		else
			Com_Printf("%s\n", errhead);

		return NULL;
	}

	return token;
}

/*
============================================================================
LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

char *Q_strlwr (char *str)
{
	char* origs = str;
	while (*str) {
		*str = tolower(*str);
		str++;
	}
	return origs;
}

int Q_putenv (const char *var, const char *value)
{
#ifdef __APPLE__
	return setenv(var, value, 1);
#else
	char str[32];

	Com_sprintf(str, sizeof(str), "%s=%s", var, value);

	return putenv((char *) str);
#endif /* __APPLE__ */
}

/**
 * @brief compute the matching length of two zero terminated strings. This only counts right with 8 bit characters.
 */
int Q_strmatch (const char *s1, const char * s2)
{
	int score = 0;
	if (!s1 || !s2)
		return 0;

	if (!Q_strcmp(s1, s2))
		return strlen(s1);

	while (s1[score] && s2[score] && s1[score] == s2[score]) {
		score++;
	}

	return score;
}

/**
 * @sa Q_strncmp
 */
int Q_strcmp (const char *s1, const char *s2)
{
	return strncmp(s1, s2, 99999);
}

/**
 * @sa Q_strcmp
 */
int Q_strncmp (const char *s1, const char *s2, size_t n)
{
	return strncmp(s1, s2, n);
}

int Q_stricmp (const char *s1, const char *s2)
{
#ifdef _WIN32
	return _stricmp(s1, s2);
#else
	return stricmp(s1, s2);
#endif
}

#ifndef HAVE_STRNCASECMP
int Q_strncasecmp (const char *s1, const char *s2, size_t n)
{
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;			/* strings are equal until end point */

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		/* strings not equal */
		}
	} while (c1);

	return 0;					/* strings are equal */
}
#endif /* HAVE_STRNCASECMP */

/**
 * @brief Safe strncpy that ensures a trailing zero
 * @param dest Destination pointer
 * @param src Source pointer
 * @param destsize Size of destination buffer (this should be a sizeof size due to portability)
 */
#ifdef DEBUG
void Q_strncpyzDebug (char *dest, const char *src, size_t destsize, const char *file, int line)
#else
void Q_strncpyz (char *dest, const char *src, size_t destsize)
#endif
{
#ifdef DEBUG
	if (!dest)
		Sys_Error("Q_strncpyz: NULL dest (%s, %i)", file, line);
	if (!src)
		Sys_Error("Q_strncpyz: NULL src (%s, %i)", file, line);
	if (destsize < 1)
		Sys_Error("Q_strncpyz: destsize < 1 (%s, %i)", file, line);
#endif

	/* space for \0 terminating */
	while (*src && destsize - 1) {
		*dest++ = *src++;
		destsize--;
	}
#ifdef PARANOID
	if (*src)
		Com_Printf("Buffer too small: %s: %i (%s)\n", file, line, src);
	/* the rest is filled with null */
	memset(dest, 0, destsize);
#else
	/* terminate the string */
	*dest = '\0';
#endif
}

/**
 * @brief Safely (without overflowing the destination buffer) concatenates two strings.
 * @param[in] dest the destination string.
 * @param[in] src the source string.
 * @param[in] destsize the total size of the destination buffer.
 * @return pointer destination string.
 * never goes past bounds or leaves without a terminating 0
 */
void Q_strcat (char *dest, const char *src, size_t destsize)
{
	size_t dest_length;
	dest_length = strlen(dest);
	if (dest_length >= destsize)
		Sys_Error("Q_strcat: already overflowed");
	Q_strncpyz(dest + dest_length, src, destsize - dest_length);
}

int Q_strcasecmp (const char *s1, const char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

/**
 * @return false if overflowed - true otherwise
 */
qboolean Com_sprintf (char *dest, size_t size, const char *fmt, ...)
{
	size_t len;
	va_list argptr;
	static char bigbuffer[0x10000];

	if (!fmt)
		return qfalse;

	va_start(argptr, fmt);
	len = Q_vsnprintf(bigbuffer, sizeof(bigbuffer), fmt, argptr);
	va_end(argptr);

	bigbuffer[sizeof(bigbuffer)-1] = 0;

	Q_strncpyz(dest, bigbuffer, size);

	if (len >= size) {
#ifdef PARANOID
		Com_Printf("Com_sprintf: overflow of "UFO_SIZE_T" in "UFO_SIZE_T"\n", len, size);
#endif
		return qfalse;
	}
	return qtrue;
}

/**
 * @brief Safe (null terminating) vsnprintf implementation
 */
int Q_vsnprintf (char *str, size_t size, const char *format, va_list ap)
{
	int len;

#if defined(_WIN32)
	len = _vsnprintf(str, size, format, ap);
	str[size-1] = '\0';
#ifdef DEBUG
	if (len == -1)
		Com_Printf("Q_vsnprintf: string was truncated - target buffer too small\n");
#endif
#else
	len = vsnprintf(str, size, format, ap);
#ifdef DEBUG
	if ((size_t)len >= size)
		Com_Printf("Q_vsnprintf: string was truncated - target buffer too small\n");
#endif
#endif

	return len;
}
