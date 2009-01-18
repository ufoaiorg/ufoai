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

/* flags for Com_Parse_O, could be expanded to specify other options */
#define PARSE_NO_COMMENTS 0 /**< Com_Parse_O ignores all comments */
#define PARSE_C_STYLE_COMMENTS 1 /**< Com_Parse_O does not ignore c-style comments */

/**
 * @brief Returns just the filename from a given path
 * @sa COM_StripExtension
 */
const char *COM_SkipPath (const char *pathname)
{
	char const* const last = strrchr(pathname, '/');
	return last ? last + 1 : pathname;
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
 * @brief Sets a default extension if there is none
 */
void COM_DefaultExtension (char *path, size_t len, const char *extension)
{
	char oldPath[MAX_OSPATH];
	const char *src;

	/* if path doesn't have a .EXT, append extension
	 * (extension should include the .) */
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return;
		src--;
	}

	Q_strncpyz(oldPath, path, sizeof(oldPath));
	Com_sprintf(path, len, "%s%s", oldPath, extension);
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

	/** @todo Q_strncpyz */
	strncpy(out, in, s - in);
	out[s - in] = 0;
}

/**
 * @brief Parse a token out of a string
 * @param data_p Pointer to a string which is to be parsed
 * @param options wether to ignore comments or not
 * @pre @c data_p is expected to be null-terminated
 * @return The string result of parsing in a string.
 * @sa COM_EParse
 */
const char *COM_Parse_O (const char *data_p[], const int options)
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

	if (!(options & PARSE_C_STYLE_COMMENTS)
	 && c == '/' && data[1] == '*') {
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
		while (1) {
			c = *data++;
			if (c == '\\' && data[0] == 'n') {
				c = '\n';
				data++;
			/* nested quotation */
			} else if (c == '\\' && data[0] == '\"') {
				c = '\"';
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

/* @brief as Com_Parse_O, but does not parse comments */
const char *COM_Parse (const char *data_p[])
{
	return COM_Parse_O(data_p, PARSE_NO_COMMENTS);
}

/**
 * @brief Parsing function that prints an error message when there is no text in the buffer
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

/**
 * @brief Compare two floats
 * @param[in] float1 The first float
 * @param[in] float2 The second float
 * @return An integer less than, equal to, or greater than zero if float1 is
 * found, respectively, to be less than,  to match, or be greater than float2
 * @note sort function pointer for qsort
 */
int Q_FloatSort (const void *float1, const void *float2)
{
	return (*(const float *)float1 - *(const float *)float2);
}

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
			if (*s1 > *s2)
				return 1;
		}
		return 0;
	} else
		return 1;
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

	return buf;
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

/**
 * @return false if overflowed - true otherwise
 */
qboolean Com_sprintf (char *dest, size_t size, const char *fmt, ...)
{
	va_list ap;
	int     len;

	if (!fmt)
		return qfalse;

	va_start(ap, fmt);
	len = Q_vsnprintf(dest, size, fmt, ap);
	va_end(ap);

	return 0 <= len && (size_t)len < size;
}

/**
 * @brief Safe (null terminating) vsnprintf implementation
 */
int Q_vsnprintf (char *str, size_t size, const char *format, va_list ap)
{
	int len;

#if defined(_WIN32)
	len = _vsnprintf(str, size, format, ap);
	str[size - 1] = '\0';
#ifdef DEBUG
	if (len == -1)
		Com_Printf("Q_vsnprintf: string (%.32s...) was truncated (%i) - target buffer too small ("UFO_SIZE_T")\n", str, len, size);
#endif
#else
	len = vsnprintf(str, size, format, ap);
#ifdef DEBUG
	if ((size_t)len >= size)
		Com_Printf("Q_vsnprintf: string (%.32s...) was truncated (%i) - target buffer too small ("UFO_SIZE_T")\n", str, len, size);
#endif
#endif

	return len;
}

const char *Q_stristr (const char *str, const char *substr)
{
	const size_t sublen = strlen(substr);
	while (*str) {
		if (!Q_strncasecmp(str, substr, sublen))
			break;
		str++;
	}
	if (!(*str))
		str = NULL;
	return str;
}

/**
 * @brief Delete a whole (possibly multibyte) character from a string.
 * @param[in] s Start of the string
 * @param[in] pos Offset from the start
 * @return position where deleted character started
 */
int UTF8_delete_char (char *s, int pos)
{
	int start = pos;
	int next = pos;

	while (start > 0 && UTF8_CONTINUATION_BYTE(s[start]))
		start--;
	if (s[next] != 0)
		next++;
	while (s[next] != 0 && UTF8_CONTINUATION_BYTE(s[next]))
		next++;
	/* memmove is the only standard copying function that is guaranteed
	 * to work if the source and destination overlap. */
	memmove(&s[start], &s[next], strlen(&s[next]) + 1);
	return start;
}

/**
 * @brief Insert a (possibly multibyte) UTF-8 character into a string.
 * @param[in] s Start of the string
 * @param[in] n Buffer size of the string
 * @param[in] pos Offset from the start
 * @param[in] c Unicode code as 32-bit integer
 * @return Number of bytes added
 */
int UTF8_insert_char (char *s, int n, int pos, int c)
{
	const int utf8len = UTF8_encoded_len(c);
	const int tail = strlen(&s[pos]) + 1;

	if (utf8len == 0)
		return 0;

	if (pos + tail + utf8len >= n)
		return 0;

	/* Insertion: move up rest of string. Also moves string terminator. */
	memmove(&s[pos + utf8len], &s[pos], tail);

	if (c <= 0x7f) {
		s[pos] = c;
	} else if (c <= 0x7ff) { 				/* c has 11 bits */
		s[pos] = 0xc0 | (c >> 6);	  			/* high 5 bits */
		s[pos + 1] = 0x80 | (c & 0x3f); 		/* low 6 bits */
	} else if (c <= 0xffff) { 				/* c has 16 bits */
		s[pos] = 0xe0 | (c >> 12);				/* high 4 bits */
		s[pos + 1] = 0x80 | ((c >> 6) & 0x3f);	/* mid 6 bits */
		s[pos + 2] = 0x80 | (c & 0x3f);			/* low 6 bits */
	} else if (c <= 0x10ffff) {				/* c has 21 bits */
		s[pos] = 0xf0 | (c >> 18);				/* high 3 bits */
		s[pos + 1] = 0x80 | ((c >> 12) & 0x3f);	/* mid 6 bits */
		s[pos + 2] = 0x80 | ((c >> 6) & 0x3f);	/* mid 6 bits */
		s[pos + 3] = 0x80 | (c & 0x3f);			/* low 6 bits */
	}

	return utf8len;
}

/**
 * @brief length of UTF-8 character starting with this byte.
 * @return length of character encoding, or 0 if not start of a UTF-8 sequence
 * @todo Using this does not solve the truncation problem in case of
 * decomposed characters. For example a code for "a" followed by a
 * code for "put dots above previous character: the "a" will be reported
 * as a character of length 1 by this function, even though the code
 * that follows is part of its visual appearance and should not be
 * cut off separately. Fortunately decomposed characters are rarely used.
 */
int UTF8_char_len (unsigned char c)
{
	if (c < 0x80)
		return 1;
	if (c < 0xc0)
		return 0;
	if (c < 0xe0)
		return 2;
	if (c < 0xf0)
		return 3;
	if (c < 0xf8)
		return 4;
	/* UTF-8 used to define 5 and 6 byte sequences, but they are
	 * no longer valid. */
	return 0;
}

/**
 * Calculate how long a Unicode code point (such as returned by
 * SDL key events in unicode mode) would be in UTF-8 encoding.
 */
int UTF8_encoded_len(int c)
{
	if (c <= 0x7F)
		return 1;
	if (c <= 0x07FF)
		return 2;
	if (c <= 0xFFFF)
		return 3;
	if (c <= 0x10FFFF)  /* highest defined Unicode code */
		return 4;
	return 0;
}
