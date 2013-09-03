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

#if !defined(INCLUDED_UNIQUENAMES_H)
#define INCLUDED_UNIQUENAMES_H

#include "debugging/debugging.h"
#include <map>
#include "string/string.h"
#include "generic/static.h"

class Postfix
{
		unsigned int m_value;
	public:
		Postfix (const std::string& postfix) :
			m_value(string::toInt(postfix))
		{
		}
		unsigned int number () const
		{
			return m_value;
		}
		void write (char* buffer)
		{
			sprintf(buffer, "%u", m_value);
		}
		Postfix& operator++ ()
		{
			++m_value;
			return *this;
		}
		bool operator< (const Postfix& other) const
		{
			return m_value < other.m_value;
		}
		bool operator== (const Postfix& other) const
		{
			return m_value == other.m_value;
		}
		bool operator!= (const Postfix& other) const
		{
			return !operator==(other);
		}
};

typedef std::pair<std::string, Postfix> name_t;

inline void name_write (char* buffer, name_t name)
{
	strcpy(buffer, name.first.c_str());
	name.second.write(buffer + name.first.length());
}

inline name_t name_read (const std::string& name)
{
	const std::string pattern = "1234567890";
	std::string cut = string::cutAfterFirstMatch(name, pattern);
	const std::size_t len = cut.length();
	std::string post = name.substr(len, name.length() - len);
	return name_t(cut, Postfix(post));
}

class PostFixes
{
		typedef std::map<Postfix, unsigned int> postfixes_t;
		postfixes_t m_postfixes;

		Postfix find_first_empty () const
		{
			Postfix postfix("1");
			for (postfixes_t::const_iterator i = m_postfixes.find(postfix); i != m_postfixes.end(); ++i, ++postfix) {
				if ((*i).first != postfix) {
					break;
				}
			}
			return postfix;
		}

	public:
		Postfix make_unique (Postfix postfix) const
		{
			postfixes_t::const_iterator i = m_postfixes.find(postfix);
			if (i == m_postfixes.end()) {
				return postfix;
			} else {
				return find_first_empty();
			}
		}

		void insert (Postfix postfix)
		{
			postfixes_t::iterator i = m_postfixes.find(postfix);
			if (i == m_postfixes.end()) {
				m_postfixes.insert(postfixes_t::value_type(postfix, 1));
			} else {
				++(*i).second;
			}
		}

		void erase (Postfix postfix)
		{
			postfixes_t::iterator i = m_postfixes.find(postfix);
			if (i == m_postfixes.end()) {
				// error
			} else {
				if (--(*i).second == 0)
					m_postfixes.erase(i);
			}
		}

		bool empty () const
		{
			return m_postfixes.empty();
		}
};

class UniqueNames
{
		typedef std::map<std::string, PostFixes> names_t;
		names_t m_names;
	public:
		name_t make_unique (const name_t& name) const
		{
			names_t::const_iterator i = m_names.find(name.first);
			if (i == m_names.end()) {
				return name;
			} else {
				return name_t(name.first, (*i).second.make_unique(name.second));
			}
		}

		void insert (const name_t& name)
		{
			m_names[name.first].insert(name.second);
		}

		void erase (const name_t& name)
		{
			names_t::iterator i = m_names.find(name.first);
			if (i == m_names.end()) {
				ASSERT_MESSAGE(true, "erase: name not found");
			} else {
				(*i).second.erase(name.second);
				if ((*i).second.empty())
					m_names.erase(i);
			}
		}

		bool empty () const
		{
			return m_names.empty();
		}
};

#endif
