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

#include "iscriplib.h"

#include "scripttokeniser.h"
#include "scripttokenwriter.h"

class ScriptLibraryAPI: public ScriptSystem
{
	public:
		typedef ScriptSystem Type;
		STRING_CONSTANT(Name, "*");

		Tokeniser* createScriptTokeniser (TextInputStream& istream)
		{
			return NewScriptTokeniser(istream);
		}
		Tokeniser* createSimpleTokeniser (TextInputStream& istream)
		{
			return NewSimpleTokeniser(istream);
		}
		TokenWriter* createSimpleTokenWriter (TextOutputStream& ostream)
		{
			return NewSimpleTokenWriter(ostream);
		}

		ScriptSystem* getTable ()
		{
			return this;
		}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<ScriptLibraryAPI> ScriptLibraryModule;
typedef Static<ScriptLibraryModule> StaticScriptLibraryModule;
StaticRegisterModule staticRegisterScriptLibrary(StaticScriptLibraryModule::instance());
