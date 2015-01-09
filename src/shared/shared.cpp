/**
 * @file
 * @brief Shared functions
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

#include "shared.h"
#include "../common/filesys.h"
#include "../ports/system.h"
#include "utf8.h"

/**
 * @brief Returns just the filename from a given path
 * @sa Com_StripExtension
 */
const char* Com_SkipPath (const char* pathname)
{
	char const* const last = strrchr(pathname, '/');
	return last ? last + 1 : pathname;
}

/**
 * @brief Removed trailing whitespaces
 * @param s The string that is modified
 * @return The start of the input string
 */
char* Com_Chop (char* s)
{
	char* right;

	right = s + strlen(s) - 1;

	while (isspace(*right))
		*right-- = 0;

	return s;
}

/**
 * @brief Removed leading and trailing whitespaces
 * @param s The string that is modified
 * @return The first none whitespace character of the given input string
 */
char* Com_Trim (char* s)
{
	char* left;

	left = s;

	while (isspace(*left))
		left++;

	return Com_Chop(left);
}

/**
 * @brief Remove high character values and only keep ascii.
 * This can be used to print utf-8 characters to the console.
 * It will replace every high character value with a simple dot.
 */
char* Com_ConvertToASCII7 (char* s)
{
	unsigned int l;
	const size_t length = strlen(s);

	l = 0;
	do {
		int c = s[l];
		if (c == '\0')
			break;
		/* don't allow higher ascii values */
		if (c >= 127)
			s[l] = '.';
		l++;
	} while (l < length);

	s[l] = '\0';

	return s;
}

/**
 * @brief Like Com_Filter, but match PATTERN against any final segment of TEXT.
 */
static int Com_FilterAfterStar (const char* pattern, const char* text)
{
	register const char* p = pattern, *t = text;
	register char c, c1;

	while ((c = *p++) == '?' || c == '*')
		if (c == '?' && *t++ == '\0')
			return 0;

	if (c == '\0')
		return 1;

	if (c == '\\')
		c1 = *p;
	else
		c1 = c;

	while (1) {
		if ((c == '[' || *t == c1) && Com_Filter(p - 1, t))
			return 1;
		if (*t++ == '\0')
			return 0;
	}
}

/**
 * @brief Match the pattern PATTERN against the string TEXT;
 * @return 1 if it matches, 0 otherwise.
 * @note A match means the entire string TEXT is used up in matching.
 * @note In the pattern string, `*' matches any sequence of characters,
 * `?' matches any character, [SET] matches any character in the specified set,
 * [!SET] matches any character not in the specified set.
 * @note A set is composed of characters or ranges; a range looks like
 * character hyphen character (as in 0-9 or A-Z).
 * [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
 * Any other character in the pattern must be matched exactly.
 * @note To suppress the special syntactic significance of any of `[]*?!-\',
 * and match the character exactly, precede it with a `\'.
 */
int Com_Filter (const char* pattern, const char* text)
{
	register const char* p = pattern, *t = text;
	register char c;

	while ((c = *p++) != '\0')
		switch (c) {
		case '?':
			if (*t == '\0')
				return 0;
			else
				++t;
			break;

		case '\\':
			if (*p++ != *t++)
				return 0;
			break;

		case '*':
			return Com_FilterAfterStar(p, t);

		case '[':
			{
				register char c1 = *t++;
				int invert;

				if (!c1)
					return (0);

				invert = ((*p == '!') || (*p == '^'));
				if (invert)
					p++;

				c = *p++;
				while (1) {
					register char cstart = c, cend = c;

					if (c == '\\') {
						cstart = *p++;
						cend = cstart;
					}
					if (c == '\0')
						return 0;

					c = *p++;
					if (c == '-' && *p != ']') {
						cend = *p++;
						if (cend == '\\')
							cend = *p++;
						if (cend == '\0')
							return 0;
						c = *p++;
					}
					if (c1 >= cstart && c1 <= cend)
						goto match;
					if (c == ']')
						break;
				}
				if (!invert)
					return 0;
				break;

			  match:
				/* Skip the rest of the [...] construct that already matched. */
				while (c != ']') {
					if (c == '\0')
						return 0;
					c = *p++;
					if (c == '\0')
						return 0;
					else if (c == '\\')
						++p;
				}
				if (invert)
					return 0;
				break;
			}

		default:
			if (c != *t++)
				return 0;
		}

	return *t == '\0';
}

/**
 * @brief Replaces the filename from one path with another one
 * @param[in] inputPath The full path to a filename
 * @param[in] expectedFileName The filename to insert into the given full path
 * @param[out] outputPath The target buffer
 * @param[in] size The size of the target buffer
 */
void Com_ReplaceFilename (const char* inputPath, const char* expectedFileName, char* outputPath, size_t size)
{
	char* slash, *end;

	Q_strncpyz(outputPath, inputPath, size);

	end = outputPath;
	while ((slash = strchr(end, '/')) != 0)
		end = slash + 1;

	strcpy(end, expectedFileName);
}

/**
 * @brief Removes the file extension from a filename
 * @sa Com_SkipPath
 * @param[in] in The incoming filename
 * @param[in] out The stripped filename
 * @param[in] size The size of the output buffer
 */
void Com_StripExtension (const char* in, char* out, const size_t size)
{
	char* out_ext = nullptr;
	int i = 1;

	while (*in && i < size) {
		*out++ = *in++;
		i++;

		if (*in == '.')
			out_ext = out;
	}

	if (out_ext)
		*out_ext = '\0';
	else
		*out = '\0';
}

/**
 * @param path The path resp. filename to extract the extension from
 * @return @c nullptr if the given path name does not contain an extension
 */
const char* Com_GetExtension (const char* path)
{
	const char* src = path + strlen(path) - 1;
	while (*src != '/' && src != path) {
		if (*src == '.')
			return src + 1;
		src--;
	}

	return nullptr;
}

/**
 * @brief Sets a default extension if there is none
 */
void Com_DefaultExtension (char* path, size_t len, const char* extension)
{
	char oldPath[MAX_OSPATH];
	const char* src;

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
void Com_FilePath (const char* in, char* out, size_t size)
{
	const char* s = in + strlen(in) - 1;

	while (s != in && *s != '/')
		s--;

	const size_t pathLength = s - in + 1;
	if (pathLength <= size)
		Q_strncpyz(out, in, pathLength);
	else if (size >= 1)
		out[0] = '\0';
}

/**
 * @brief returns hash key for a string
 */
unsigned int Com_HashKey (const char* name, int hashsize)
{
	unsigned int v = 0;
	for (int i = 0; name[i]; i++) {
		const unsigned int c = name[i];
		v = (v + i) * 37 + tolower(c);	/* case insensitivity */
	}

	return v % hashsize;
}

/**
 * @brief Creates a timestamp with date and time at the specified location
 * @param ts ptr to the resulting string
 * @param tslen length of target buffer
 */
void Com_MakeTimestamp (char* ts, const size_t tslen)
{
	struct tm* t;
	time_t aclock;

	time(&aclock);
	t = localtime(&aclock);

	Com_sprintf(ts, tslen, "%4i/%02i/%02i %02i:%02i:%02i", t->tm_year + 1900,
			t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

/**
 * @brief Compare two floats
 * @param[in] float1 The first float
 * @param[in] float2 The second float
 * @return An integer less than, equal to, or greater than zero if float1 is
 * found, respectively, to be less than,  to match, or be greater than float2
 * @note sort function pointer for qsort
 */
int Q_FloatSort (const void* float1, const void* float2)
{
	return (*(const float*)float1 - *(const float*)float2);
}

/**
 * @brief Compare two strings
 * @param[in] string1 The first string
 * @param[in] string2 The second string
 * @return An integer less than, equal to, or greater than zero if string1 is
 * found, respectively, to be less than,  to match, or be greater than string2
 * @note sort function pointer for qsort
 */
int Q_StringSort (const void* string1, const void* string2)
{
	const char* s1 = (const char*)string1;
	const char* s2 = (const char*)string2;
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
const char* va (const char* format, ...)
{
	va_list argptr;
	/* in case va is called by nested functions */
	static char string[16][VA_BUFSIZE];
	static unsigned int index = 0;
	char* buf;

	buf = string[index & 0x0F];
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

/**
 * @brief Converts a string to lowercase
 * @param[in,out] str The string that is converted to lowercase
 */
char* Q_strlwr (char* str)
{
	char* origs = str;
	while (*str) {
		*str = tolower(*str);
		str++;
	}
	return origs;
}

/**
 * @brief Safe strncpy that ensures a trailing zero
 * @param dest Destination pointer
 * @param src Source pointer
 * @param destsize Size of destination buffer (this should be a sizeof size due to portability)
 */
#ifdef DEBUG
void Q_strncpyzDebug (char* dest, const char* src, size_t destsize, const char* file, int line)
#else
void Q_strncpyz (char* dest, const char* src, size_t destsize)
#endif
{
#ifdef DEBUG
	if (!dest)
		Sys_Error("Q_strncpyz: nullptr dest (%s, %i)", file, line);
	if (!src)
		Sys_Error("Q_strncpyz: nullptr src (%s, %i)", file, line);
	if (destsize < 1)
		Sys_Error("Q_strncpyz: destsize < 1 (%s, %i)", file, line);
#endif
	UTF8_strncpyz(dest, src, destsize);
}

/**
 * @brief Safely (without overflowing the destination buffer) concatenates two strings.
 * @param[in] dest the destination string.
 * @param[in] destsize the total size of the destination buffer.
 * @param[in] format the format or simple source string.
 * @return pointer destination string.
 * never goes past bounds or leaves without a terminating 0
 */
void Q_strcat (char* dest, size_t destsize, const char* format, ...)
{
	const size_t dest_length = strlen(dest);
	if (dest_length >= destsize)
		Sys_Error("Q_strcat: already overflowed");

	va_list argptr;
	va_start(argptr, format);
	Q_vsnprintf(dest + dest_length, destsize - dest_length, format, argptr);
	va_end(argptr);
}

/**
 * @brief copies formatted string with buffer-size checking
 * @param[out] dest Destination buffer
 * @param[in] size Size of the destination buffer
 * @param[in] fmt Stringformat (like printf)
 * @return false if overflowed - true otherwise
 */
bool Com_sprintf (char* dest, size_t size, const char* fmt, ...)
{
	va_list ap;
	int len;

	if (!fmt)
		return false;

	va_start(ap, fmt);
	len = Q_vsnprintf(dest, size, fmt, ap);
	va_end(ap);

	if (len <= size - 1)
		return true;

	/* number of character */
	len = size - 1;

	/* check for UTF8 multibyte sequences */
	if (len > 0 && (unsigned char) dest[len - 1] >= 0x80) {
		int i = len - 1;
		while (i > 0 && UTF8_CONTINUATION_BYTE((unsigned char) dest[i]))
			i--;
		if (UTF8_char_len(dest[i]) + i > len) {
			dest[i] = '\0';
		}
#ifdef DEBUG
		else {
			/* the '\0' is already at the right place */
			len = i + UTF8_char_len(dest[i]);
			assert(dest[len] == '\0');
		}
#endif
	}

	return false;
}

/**
 * @brief Safe (null terminating) vsnprintf implementation
 */
int Q_vsnprintf (char* str, size_t size, const char* format, va_list ap)
{
	int len;

#if defined(_WIN32)
	len = _vsnprintf(str, size, format, ap);
	str[size - 1] = '\0';
#ifdef DEBUG
	if (len == -1)
		Com_Printf("Q_vsnprintf: string (%.32s...) was truncated (%i) - target buffer too small (" UFO_SIZE_T ")\n", str, len, size);
#endif
#else
	len = vsnprintf(str, size, format, ap);
#ifdef DEBUG
	if ((size_t)len >= size)
		Com_Printf("Q_vsnprintf: string (%.32s...) was truncated (%i) - target buffer too small (" UFO_SIZE_T ")\n", str, len, size);
#endif
#endif

	return len;
}

/**
 * @brief Checks in case insensitive manner whether @c str contains @c substr
 * @param[in] str The source string
 * @param[in] substr The string to search in the source string
 * @return The start pointer where substr begins, or @c null if @c substr is no part of @c str
 */
const char* Q_stristr (const char* str, const char* substr)
{
	const size_t sublen = strlen(substr);
	while (*str) {
		if (!Q_strncasecmp(str, substr, sublen))
			break;
		str++;
	}
	if (!(*str))
		str = nullptr;
	return str;
}

char const* Q_strstart (char const* str, char const* start)
{
	for (; *start != '\0'; ++str, ++start) {
		if (*str != *start)
			return 0;
	}
	return str;
}

bool Q_strreplace (const char* source, const char* pattern, const char* replace, char* dest, size_t destsize)
{
	if (char const* const hit = strstr(source, pattern)) {
		int const len = snprintf(dest, destsize, "%.*s%s%s", (int)(hit - source), source, replace, hit + strlen(pattern));
		return 0 < len && (size_t)len < destsize;
	} else {
		return false;
	}
}

/**
 * @brief Checks whether the given input string is allowed to be used as a user-given name string
 * for aircraft, soldiers, bases and so on
 * @param[in] input The input string to check
 * @return @c true if the string is valid and can be used as a name, @c false otherwise
 */
bool Com_IsValidName (const char* input)
{
	/* empty strings are not allowed */
	if (!Q_strvalid(input))
		return false;
	/* names with only _ are not allowed - they would get translated with as empty msgid for gettext */
	if (Q_streq(input, "_"))
		return false;
	/* there may be no quotes in the names - as they are given very often as parameter in the scripts */
	if (strchr(input, '"') != nullptr)
		return false;
	return true;
}

#ifndef NDEBUG
void UFO_assert (bool condition, const char* fmt, ...)
{
	if (condition)
		return;
	char msg[1024];
	va_list argptr;

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Sys_Error("%s", msg);
}
#endif
