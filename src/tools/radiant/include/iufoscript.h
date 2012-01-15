/**
 * @file iufoscript.h
 * @brief Global UFOScript interface
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#ifndef IUFOSCRIPT_H
#define IUFOSCRIPT_H

#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "generic/constant.h"
#include <string>
#include <set>
#include "ifilesystem.h"

class UFOScriptSystem
{
	public:

		typedef std::set<std::string> UFOScriptFiles;

	private:

		UFOScriptFiles _ufoFiles;
		mutable bool _init;

	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "ufos");
		typedef std::set<std::string>::iterator UFOScriptFilesIterator;

		virtual ~UFOScriptSystem ()
		{
		}

		/**
		 * Constructor
		 */
		UFOScriptSystem ();

		void init ();

		void editTerrainDefinition ();

		void generateTerrainDefinition ();

		void editMapDefinition ();

		const std::string getUFOScriptDir () const;

		/**
		 * @return A vector with ufo filesnames
		 */
		const UFOScriptFiles& getFiles ();
};

class UFOScriptSystemDependencies: public GlobalFileSystemModuleRef
{
};

// This is needed to be registered as a Radiant dependency
template<typename Type>
class GlobalModule;
typedef GlobalModule<UFOScriptSystem> GlobalUFOScriptSystemModule;

// A reference to the call above.
template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<UFOScriptSystem> GlobalUFOScriptSystemModuleRef;

// Accessor method
inline UFOScriptSystem * GlobalUFOScriptSystem ()
{
	Module * ufoScriptSystem = globalModuleServer().findModule(UFOScriptSystem::Name_CONSTANT_::evaluate(),
			UFOScriptSystem::Version_CONSTANT_::evaluate(), "*");
	ASSERT_MESSAGE(ufoScriptSystem,
			"Couldn't retrieve GlobalUFOScriptSystem, is not registered and/or initialized.");
	return (UFOScriptSystem *) ufoScriptSystem->getTable(); // findModule returns the pointer to the valid value, DO NOT DELETE!
}

#endif  /* IUFOSCRIPT_H */
