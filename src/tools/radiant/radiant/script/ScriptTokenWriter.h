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

#ifndef INCLUDED_SCRIPT_SCRIPTTOKENWRITER_H
#define INCLUDED_SCRIPT_SCRIPTTOKENWRITER_H

#include "iscriplib.h"

class ScriptTokenWriter: public TokenWriter
{
	private:

		void writeSeparator ();

		TextOutputStream& m_ostream;

		char m_separator;

	public:

		ScriptTokenWriter (TextOutputStream& ostream);
		~ScriptTokenWriter ();

		void nextLine ();

		void writeToken (const std::string& token);

		void writeString (const std::string& string);

		void writeInteger (int i);

		void writeUnsigned (std::size_t i);

		void writeFloat (double f);
};

#endif
