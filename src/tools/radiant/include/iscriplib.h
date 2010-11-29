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

#if !defined(INCLUDED_ISCRIPLIB_H)
#define INCLUDED_ISCRIPLIB_H

/// \file iscriplib.h
/// \brief Token input/output stream module.

#include <cstddef>
#include <string>
#include "generic/constant.h"

class Tokeniser
{
	public:
		virtual ~Tokeniser ()
		{
		}
		virtual const std::string getToken () = 0;
		virtual void ungetToken () = 0;
		virtual std::size_t getLine () const = 0;
		virtual std::size_t getColumn () const = 0;
};

class TextInputStream;

class TokenWriter
{
	public:
		virtual ~TokenWriter ()
		{
		}
		virtual void nextLine () = 0;
		virtual void writeToken (const std::string& token) = 0;
		virtual void writeString (const std::string& string) = 0;
		virtual void writeInteger (int i) = 0;
		virtual void writeUnsigned (std::size_t i) = 0;
		virtual void writeFloat (double f) = 0;
};

class TextOutputStream;

class IScriptLibrary
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "scriptlib");

		virtual ~IScriptLibrary() {}

		virtual Tokeniser* createScriptTokeniser (TextInputStream& istream) = 0;
		virtual Tokeniser* createSimpleTokeniser (TextInputStream& istream) = 0;
		virtual TokenWriter* createSimpleTokenWriter (TextOutputStream& ostream) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IScriptLibrary> GlobalScripLibModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IScriptLibrary> GlobalScripLibModuleRef;

inline IScriptLibrary& GlobalScriptLibrary ()
{
	return GlobalScripLibModule::getTable();
}

#endif
