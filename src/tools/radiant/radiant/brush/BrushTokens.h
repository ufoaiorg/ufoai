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

#if !defined(INCLUDED_BRUSHTOKENS_H)
#define INCLUDED_BRUSHTOKENS_H

#include "stringio.h"
#include "stream/stringstream.h"
#include "imap.h"
#include "Face.h"
#include "ContentsFlagsValue.h"

class BrushTokenImporter: public MapImporter
{
		Brush& m_brush;

	public:
		BrushTokenImporter (Brush& brush);
		bool importTokens (Tokeniser& tokeniser);
};

class BrushTokenExporter: public MapExporter
{
		const Brush& m_brush;

	public:
		BrushTokenExporter (const Brush& brush);
		void exportTokens (TokenWriter& writer) const;
};

#endif
