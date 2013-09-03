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

#if !defined(INCLUDED_STREAM_TEXTFILESTREAM_H)
#define INCLUDED_STREAM_TEXTFILESTREAM_H

#include "itextstream.h"
#include <stdio.h>
#include <string>

/// \brief A wrapper around a file input stream opened for reading in text mode. Similar to std::ifstream.
class TextFileInputStream: public TextInputStream
{
		FILE* m_file;
	public:
		/**
		 * @param name The full path of the file to read from
		 */
		TextFileInputStream (const std::string& name)
		{
			m_file = name.empty() ? 0 : fopen(name.c_str(), "rt");
		}
		~TextFileInputStream ()
		{
			if (!failed())
				fclose(m_file);
		}

		std::size_t size ()
		{
			std::size_t pos;
			std::size_t end;

			if (failed())
				return 0;

			pos = ftell(m_file);
			fseek(m_file, 0, SEEK_END);
			end = ftell(m_file);
			fseek(m_file, pos, SEEK_SET);

			return end;
		}

		bool failed () const
		{
			return m_file == 0;
		}

		std::string getString ()
		{
			const std::size_t buffer_size = 1024;
			char buffer[buffer_size];

			for (;;) {
				std::size_t size = read(buffer, buffer_size);
				if (size == 0)
					break;
			}

			return buffer;
		}

		std::size_t read (char* buffer, std::size_t length)
		{
			return fread(buffer, 1, length, m_file);
		}
};

/// \brief A wrapper around a file input stream opened for writing in text mode. Similar to std::ofstream.
class TextFileOutputStream: public TextOutputStream
{
		FILE* m_file;
	public:
		/**
		 * @param name The full path to the file to write into
		 */
		TextFileOutputStream (const std::string& name)
		{
			m_file = name.empty() ? 0 : fopen(name.c_str(), "wt");
		}
		~TextFileOutputStream ()
		{
			if (!failed())
				fclose(m_file);
		}

		bool failed () const
		{
			return m_file == 0;
		}

		std::size_t write (const std::string& buffer)
		{
			return fwrite(buffer.c_str(), 1, buffer.length(), m_file);
		}

		std::size_t write (const char* buffer, std::size_t length)
		{
			return fwrite(buffer, 1, length, m_file);
		}
};

template<typename T>
inline TextFileOutputStream& operator<< (TextFileOutputStream& ostream, const T& t)
{
	return ostream_write(ostream, t);
}

#endif
