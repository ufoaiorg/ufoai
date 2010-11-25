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

#ifndef INCLUDED_FS_PATH_H
#define INCLUDED_FS_PATH_H

#include "stream/stringstream.h"

/// \brief A unix-style path string which can be modified at runtime.
///
/// - Maintains a path ending in a path-separator.
/// - Provides a limited STL-style interface to push and pop file or directory names at the end of the path.
class UnixPath
{
		std::string m_string;

		void check_separator ()
		{
			if (!empty() && m_string[m_string.length() - 1] != '/') {
				m_string.push_back('/');
			}
		}

	public:
		/// \brief Constructs with the directory \p root.
		UnixPath (const std::string& root) :
			m_string(root)
		{
			check_separator();
		}

		bool empty () const
		{
			return m_string.empty();
		}

		operator const std::string& () const
		{
			return m_string;
		}

		/// \brief Appends the directory \p name.
		void push (const std::string& name)
		{
			m_string += name;
			check_separator();
		}

		/// \brief Appends the filename \p name.
		void push_filename (const std::string& name)
		{
			m_string += name;
		}

		/// \brief Removes the last directory or filename appended.
		void pop ()
		{
			if (m_string[m_string.length() - 1] == '/') {
				m_string.erase(m_string.length() - 1, 1);
			}
			while (!empty() && m_string[m_string.length() - 1] != '/') {
				m_string.erase(m_string.length() - 1, 1);
			}
		}
};

#endif
