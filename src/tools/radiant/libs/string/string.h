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

/// \brief Copies \p other into \p string and returns \p string.
/// Assumes that the space allocated for \p string is at least string_length(other) + 1.
/// O(n)
inline char* string_copy (char* string, const char* other)
{
	return std::strcpy(string, other);
}

/// \brief Allocates a string buffer large enough to hold \p length characters, using \p allocator.
/// The returned buffer must be released with \c string_release using a matching \p allocator.
template<typename Allocator>
inline char* string_new (std::size_t length, Allocator& allocator)
{
	return allocator.allocate(length + 1);
}

/// \brief Deallocates the \p buffer large enough to hold \p length characters, using \p allocator.
template<typename Allocator>
inline void string_release (char* buffer, std::size_t length, Allocator& allocator)
{
	allocator.deallocate(buffer, length + 1);
}

/// \brief Allocates a string buffer large enough to hold \p length characters.
/// The returned buffer must be released with \c string_release.
inline char* string_new (std::size_t length)
{
	DefaultAllocator<char> allocator;
	return string_new(length, allocator);
}

/// \brief Deallocates the \p buffer large enough to hold \p length characters.
inline void string_release (char* string, std::size_t length)
{
	DefaultAllocator<char> allocator;
	string_release(string, length, allocator);
}

typedef char* char_pointer;
/// \brief Swaps the values of \p string and \p other.
inline void string_swap (char_pointer& string, char_pointer& other)
{
	std::swap(string, other);
}

/// \brief Converts each character of \p string to lower-case and returns \p string.
/// O(n)
inline char* string_to_lowercase (char* string)
{
	for (char* p = string; *p != '\0'; ++p) {
		*p = (char) std::tolower(*p);
	}
	return string;
}

/// \brief case-insensitive strstr
inline char* string_contains_nocase (char* haystack, char* needle)
{
	return std::strstr(string_to_lowercase(haystack), string_to_lowercase(needle));
}

/// \brief A re-entrant string tokeniser similar to strchr.
class StringTokeniser
{
		bool istoken (char c) const
		{
			if (strchr(m_delimiters, c) != 0) {
				return false;
			}
			return true;
		}
		const char* advance ()
		{
			const char* token = m_pos;
			bool intoken = true;
			while (!string_empty(m_pos)) {
				if (!istoken(*m_pos)) {
					*m_pos = '\0';
					intoken = false;
				} else if (!intoken) {
					return token;
				}
				++m_pos;
			}
			return token;
		}
		std::size_t m_length;
		char* m_string;
		char* m_pos;
		const char* m_delimiters;
	public:
		StringTokeniser (const char* string, const char* delimiters = " \n\r\t\v") :
			m_length(string_length(string)), m_string(string_copy(string_new(m_length), string)), m_pos(m_string),
					m_delimiters(delimiters)
		{
			while (!string_empty(m_pos) && !istoken(*m_pos)) {
				++m_pos;
			}
		}
		~StringTokeniser ()
		{
			string_release(m_string, m_length);
		}
		/// \brief Returns the next token or "" if there are no more tokens available.
		const char* getToken ()
		{
			return advance();
		}
};

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
			return string_less_nocase(x.c_str(), y.c_str());
		}
};

#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>
#include <vector>

namespace string
{
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
}

#endif
