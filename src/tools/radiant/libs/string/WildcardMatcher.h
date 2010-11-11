/*
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

#ifndef WILDCARDMATCHER_H_
#define WILDCARDMATCHER_H_

#include <string>

class WildcardMatcher
{
	private:
		const std::string _text;
		const std::string _pattern;

	private:
		/* Like glob_match, but match PATTERN against any final segment of TEXT.  */
		inline bool glob_match_after_star (const char *pattern, const char *text)
		{
			register const char *p = pattern, *t = text;
			register char c, c1;

			while ((c = *p++) == '?' || c == '*')
				if (c == '?' && *t++ == '\0')
					return false;

			if (c == '\0')
				return true;

			if (c == '\\')
				c1 = *p;
			else
				c1 = c;

			while (1) {
				if ((c == '[' || *t == c1) && glob_matches(p - 1, t))
					return true;
				if (*t++ == '\0')
					return false;
			}
		}

		/* Return nonzero if PATTERN has any special globbing chars in it. */
		inline bool glob_pattern_p (const char *pattern)
		{
			register const char *p = pattern;
			register char c;
			int open = 0;

			while ((c = *p++) != '\0')
				switch (c) {
				case '?':
				case '*':
					return true;

				case '[': /* Only accept an open brace if there is a close */
					open++; /* brace to match it.  Bracket expressions must be */
					continue; /* complete, according to Posix.2 */
				case ']':
					if (open)
						return true;
					continue;

				case '\\':
					if (*p++ == '\0')
						return false;
				}

			return false;
		}

		bool glob_matches (const char *pattern, const char *text)
		{
			register const char *p = pattern, *t = text;
			register char c;

			while ((c = *p++) != '\0')
				switch (c) {
				case '?':
					if (*t == '\0')
						return false;
					else
						++t;
					break;

				case '\\':
					if (*p++ != *t++)
						return false;
					break;

				case '*':
					return glob_match_after_star(p, t);

				case '[': {
					register char c1 = *t++;
					int invert;

					if (!c1)
						return false;

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
							return false;

						c = *p++;
						if (c == '-' && *p != ']') {
							cend = *p++;
							if (cend == '\\')
								cend = *p++;
							if (cend == '\0')
								return false;
							c = *p++;
						}
						if (c1 >= cstart && c1 <= cend)
							goto match;
						if (c == ']')
							break;
					}
					if (!invert)
						return false;
					break;

					match:
					/* Skip the rest of the [...] construct that already matched.  */
					while (c != ']') {
						if (c == '\0')
							return false;
						c = *p++;
						if (c == '\0')
							return false;
						else if (c == '\\')
							++p;
					}
					if (invert)
						return false;
					break;
				}

				default:
					if (c != *t++)
						return false;
				}

			return *t == '\0';
		}

		bool matches ()
		{
			return glob_matches(_pattern.c_str(), _text.c_str());
		}

		WildcardMatcher (const std::string& text, const std::string& pattern) :
			_text(text), _pattern(pattern)
		{
		}

	public:

		/**
		 * @brief Match the pattern PATTERN against the string TEXT;
		 * return @c true if it matches, @c false otherwise.
		 *
		 * A match means the entire string TEXT is used up in matching.
		 *
		 * In the pattern string, `*' matches any sequence of characters,
		 * `?' matches any character, [SET] matches any character in the specified set,
		 * [!SET] matches any character not in the specified set.
		 *
		 * A set is composed of characters or ranges; a range looks like
		 * character hyphen character (as in 0-9 or A-Z).
		 * [0-9a-zA-Z_] is the set of characters allowed in C identifiers.
		 * Any other character in the pattern must be matched exactly.
		 *
		 * To suppress the special syntactic significance of any of `[]*?!-\',
		 * and match the character exactly, precede it with a `\'.
		 */
		static bool matches (const std::string& text, const std::string& pattern)
		{
			WildcardMatcher matcher(text, pattern);
			return matcher.matches();
		}
};

#endif
