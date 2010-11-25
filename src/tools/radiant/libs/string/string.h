/**
 * @file string.h
 * @brief C-style null-terminated-character-array string library.
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_STRING_STRING_H)
#define INCLUDED_STRING_STRING_H

#include <string>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <cstdarg>
#include <stdio.h>
#include <glib.h>
#include "WildcardMatcher.h"

#include "memory/allocator.h"
#include "generic/arrayrange.h"

/// \brief Returns true if \p string length is zero.
/// O(1)
inline bool string_empty (const std::string& string)
{
	return string.empty();
}

/// \brief Returns <0 if \p string is lexicographically less than \p other.
/// Returns >0 if \p string is lexicographically greater than \p other.
/// Returns 0 if \p string is lexicographically equal to \p other.
/// O(n)
inline int string_compare (const std::string& string, const std::string& other)
{
	return std::strcmp(string.c_str(), other.c_str());
}

/// \brief Returns true if \p string is lexicographically equal to \p other.
/// O(n)
inline bool string_equal (const std::string& string, const std::string& other)
{
	return string == other;
}

/// \brief Returns true if [\p string, \p string + \p n) is lexicographically equal to [\p other, \p other + \p n).
/// O(n)
inline bool string_equal_n (const std::string& string, const std::string& other, std::size_t n)
{
	return std::strncmp(string.c_str(), other.c_str(), n) == 0;
}

/// \brief Returns <0 if \p string is lexicographically less than \p other after converting both to lower-case.
/// Returns >0 if \p string is lexicographically greater than \p other after converting both to lower-case.
/// Returns 0 if \p string is lexicographically equal to \p other after converting both to lower-case.
/// O(n)
inline int string_compare_nocase (const std::string& string, const std::string& other)
{
	return g_ascii_strcasecmp(string.c_str(), other.c_str());
}

/// \brief Returns <0 if [\p string, \p string + \p n) is lexicographically less than [\p other, \p other + \p n).
/// Returns >0 if [\p string, \p string + \p n) is lexicographically greater than [\p other, \p other + \p n).
/// Returns 0 if [\p string, \p string + \p n) is lexicographically equal to [\p other, \p other + \p n).
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline int string_compare_nocase_n (const std::string& string, const std::string& other, std::size_t n)
{
	return g_ascii_strncasecmp(string.c_str(), other.c_str(), n);
}

/// \brief Returns true if \p string is lexicographically equal to \p other.
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline bool string_equal_nocase (const std::string& string, const std::string& other)
{
	return string_compare_nocase(string, other) == 0;
}

/// \brief Returns true if [\p string, \p string + \p n) is lexicographically equal to [\p other, \p other + \p n).
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline bool string_equal_nocase_n (const std::string& string, const std::string& other, std::size_t n)
{
	return string_compare_nocase_n(string, other, n) == 0;
}

/// \brief Returns true if \p string is lexicographically less than \p other.
/// Treats all ascii characters as lower-case during comparisons.
/// O(n)
inline bool string_less_nocase (const std::string& string, const std::string& other)
{
	return string_compare_nocase(string, other) < 0;
}

/// \brief Returns the number of non-null characters in \p string.
/// O(n)
inline std::size_t string_length (const std::string& string)
{
	return string.length();
}

struct RawStringEqual
{
		bool operator() (const std::string& x, const std::string& y) const
		{
			return x.compare(y) == 0;
		}
};

struct RawStringLess
{
		bool operator() (const std::string& x, const std::string& y) const
		{
			return x.compare(y) < 0;
		}
};

struct RawStringLessNoCase
{
		bool operator() (const std::string& x, const std::string& y) const
		{
			return string_less_nocase(x, y);
		}
};

#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

namespace string {

template<class T>
inline std::string toString (const T& t)
{
	std::stringstream ss;
	ss << t;
	return ss.str();
}

inline bool startsWith (const std::string& source, const std::string& contains)
{
	return !source.compare(0, contains.size(), contains);
}

inline bool contains (const std::string& source, const std::string& contains)
{
	return source.rfind(contains) != std::string::npos;
}

inline int toInt (const std::string& str, int defaultValue = 0)
{
	if (str.empty())
		return defaultValue;
	return atoi(str.c_str());
}

inline float toFloat (const std::string& str, float defaultValue = 0.0f)
{
	if (str.empty())
		return defaultValue;
	return atof(str.c_str());
}

inline std::string toLower (const std::string& str)
{
	std::string convert = str;
	std::transform(convert.begin(), convert.end(), convert.begin(), (int(*) (int)) std::tolower);
	return convert;
}

inline std::string toUpper (const std::string& str)
{
	std::string convert = str;
	std::transform(convert.begin(), convert.end(), convert.begin(), (int(*) (int)) std::toupper);
	return convert;
}

inline void splitBy (const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ")
{
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos || std::string::npos != lastPos) {
		// Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
}

/**
 * Replace all occurrences of @c searchStr with @c replaceStr
 * @param str source where all occurrences should be replaced
 * @param searchStr search for this string
 * @param replaceStr replace with that string
 * @return string with all occurrences replaced
 */
inline std::string replaceAll (const std::string& str, const std::string& searchStr, const std::string& replaceStr)
{
	if (str.empty())
		return str;
	std::string sNew = str;
	std::string::size_type loc;
	const std::string::size_type replaceLength = replaceStr.length();
	const std::string::size_type searchLength = searchStr.length();
	std::string::size_type lastPosition = 0;
	while (std::string::npos != (loc = sNew.find(searchStr, lastPosition))) {
		sNew.replace(loc, searchLength, replaceStr);
		lastPosition = loc + replaceLength;
	}
	return sNew;
}

inline bool matchesWildcard (const std::string& str, const std::string& wildcard)
{
	return WildcardMatcher::matches(str, wildcard);
}

inline bool endsWith (const std::string& str, const std::string& end)
{
	const std::size_t strLength = str.length();
	const std::size_t endLength = end.length();
	if (strLength >= endLength) {
		const std::size_t index = strLength - endLength;
		return str.compare(index, endLength, end) == 0;
	} else {
		return false;
	}
}

/**
 * Returns a new string that has all the spaces from the given string removed.
 */
inline std::string eraseAllSpaces (const std::string& str)
{
	std::string tmp(str);
	tmp.erase(std::remove(tmp.begin(), tmp.end(), ' '), tmp.end());
	return tmp;
}

inline std::string cutAfterFirstMatch (const std::string& str, const std::string& pattern)
{
	std::string::size_type pos = str.find_first_of(pattern, 0);
	return str.substr(0, pos);
}

inline std::string format (const std::string &msg, ...)
{
	va_list ap;
	const std::size_t size = 1024;
	char text[size];

	va_start(ap, msg);
	vsnprintf(text, size, msg.c_str(), ap);
	va_end(ap);

	return std::string(text);
}

// inline newline (\r and \n) removal
// true if newlines where replaced, false if no newlines where in the source string
inline bool removeNewlines (std::string& string)
{
	int count = 0;

	// Baal: check string for newline characters \n \r and count them
	for (std::string::const_iterator i = string.begin(); i != string.end(); i++)
		if (*i == '\n' || *i == '\r')
			count++;

	// Baal: copy the string and remove newlines (only if one of these was found before)
	if (count > 0) {
		std::string temp(string.length() - count, 0);
		std::string::const_iterator stringIt;
		std::string::iterator tempIt;

		for ((stringIt = string.begin(), tempIt = temp.begin()); stringIt != string.end(); stringIt++) {
			if (*stringIt == '\n' || *stringIt == '\r')
				continue;

			*tempIt++ = *stringIt;
		}
		string = temp;
		return true;
	}
	return false;
}

}

#endif
