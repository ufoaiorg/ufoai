/**
 * @file infostring.c
 * @brief Info string handling
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

#include "infostring.h"
#include "../common/common.h"

/**
 * @brief Searches the string for the given key and returns the associated value, or an empty string.
 * @param[in] s The string you want to extract the keyvalue from
 * @param[in] key The key you want to extract the value for
 * @sa Info_SetValueForKey
 * @return The value or empty string - never NULL
 * @todo Not thread safe
 */
const char *Info_ValueForKey (const char *s, const char *key)
{
	char pkey[512];
	/* use two buffers so compares */
	static char value[2][512] = {"", ""};

	/* work without stomping on each other */
	static int valueindex = 0;
	char *o;

	valueindex ^= 1;
	if (*s == '\\' && *s != '\n')
		s++;
	while (1) {
		o = pkey;
		while (*s != '\\' && *s != '\n') {
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = '\0';
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s != '\n' && *s)
			*o++ = *s++;
		*o = '\0';

		if (!Q_strcasecmp(key, pkey))
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

const char *Info_BoolForKey (const char *s, const char *key)
{
	const char *boolStr = Info_ValueForKey(s, key);
	if (boolStr[0] == '0' || boolStr[0] == '\0' || Q_streq(boolStr, "No"))
		return "No";
	return "Yes";
}

int Info_IntegerForKey (const char *s, const char *key)
{
	return atoi(Info_ValueForKey(s, key));
}

/**
 * @brief Searches through s for key and remove is
 * @param[in] s String to search key in
 * @param[in] key String to search for in s
 * @sa Info_SetValueForKey
 */
void Info_RemoveKey (char *s, const char *key)
{
	char *start;
	char pkey[512];
	char value[512];
	char *o;

	if (strstr(key, "\\")) {
/*		Com_Printf("Can't use a key with a \\\n"); */
		return;
	}

	while (1) {
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\') {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s) {
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strncmp(key, pkey, sizeof(pkey))) {
			const size_t size = strlen(s);
			memmove(start, s, size);
			start[size] = 0;
			return;
		}

		if (!*s)
			return;
	}

}


/**
 * @brief Some characters are illegal in info strings because they can mess up the server's parsing
 */
qboolean Info_Validate (const char *s)
{
	if (strstr(s, "\""))
		return qfalse;
	if (strstr(s, ";"))
		return qfalse;
	return qtrue;
}

/**
 * @sa Info_SetValueForKey
 */
void Info_SetValueForKeyAsInteger (char *s, const size_t size, const char *key, const int value)
{
	Info_SetValueForKey(s, size, key, va("%i", value));
}

/**
 * @brief Adds a new entry into string with given value.
 * @note Removed any old version of the key
 * @param[in,out] s The target info string
 * @param[in] size The size of @c s
 * @param[in] key The key to set
 * @param[in] value The value to set for the given key
 * @sa Info_RemoveKey
 * @sa Info_SetValueForKeyAsInteger
 */
void Info_SetValueForKey (char *s, const size_t size, const char *key, const char *value)
{
	char newi[MAX_INFO_STRING];

	if (strstr(key, "\\") || strstr(value, "\\")) {
		Com_Printf("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, ";")) {
		Com_Printf("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr(key, "\"") || strstr(value, "\"")) {
		Com_Printf("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY - 1) {
		Com_Printf("Keys must be < "STRINGIFY(MAX_INFO_KEY)" characters.\n");
		return;
	}

	if (strlen(key) > MAX_INFO_VALUE - 1) {
		Com_Printf("Values must be < "STRINGIFY(MAX_INFO_VALUE)" characters.\n");
		return;
	}

	Info_RemoveKey(s, key);
	if (Q_strnull(value))
		return;

	Com_sprintf(newi, sizeof(newi), "\\%s\\%s%s", key, value, s);
	Q_strncpyz(s, newi, size);
}

/**
 * @brief Prints info strings (like userinfo or serverinfo - CVAR_USERINFO, CVAR_SERVERINFO)
 */
void Info_Print (const char *s)
{
	if (*s == '\\')
		s++;
	while (*s) {
		char const* key;
		int         key_len   = 0;
		char const* value;
		int         value_len = 0;

		key = s;
		while (*s && *s != '\\') {
			++s;
			++key_len;
		}

		if (!*s) {
			Com_Printf("%-20.*sMISSING VALUE\n", key_len, key);
			return;
		}

		value = ++s;
		while (*s && *s != '\\') {
			++s;
			++value_len;
		}

		if (*s)
			s++;

		Com_Printf("%-20.*s%.*s\n", key_len, key, value_len, value);
	}
}
