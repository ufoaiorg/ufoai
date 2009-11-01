/**
 * @file iump.h
 * @brief Global UMP interface
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

#ifndef IUMP_H
#define IUMP_H

#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "generic/constant.h"
#include <string>
#include <set>
#include "ifilesystem.h"

class UMPSystem
{
	private:

		std::set<std::string> _umpFiles;
		typedef std::set<std::string>::iterator UMPFilesIterator;

	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "ump");

		virtual ~UMPSystem ()
		{
		}

		/**
		 * Constructor
		 */
		UMPSystem ();

		void editUMPDefinition ();

		/**
		 * @return The ump filename for the given map
		 */
		const std::string getUMPFilename (const std::string& map);

		void init ();

		/**
		 * @return A vector with ump filesnames
		 */
		const std::set<std::string> getFiles () const
		{
			return _umpFiles;
		}

};

class UMPSystemDependencies: public GlobalFileSystemModuleRef
{
};

// This is needed to be registered as a Radiant dependency
template<typename Type>
class GlobalModule;
typedef GlobalModule<UMPSystem> GlobalUMPSystemModule;

// A reference to the call above.
template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<UMPSystem> GlobalUMPSystemModuleRef;

// Accessor method
inline UMPSystem * GlobalUMPSystem ()
{
	Module * umpSystem = globalModuleServer().findModule(UMPSystem::Name_CONSTANT_::evaluate(),
			UMPSystem::Version_CONSTANT_::evaluate(), "*");
	ASSERT_MESSAGE(umpSystem,
			"Couldn't retrieve GlobalUMPSystem, is not registered and/or initialized.");
	return (UMPSystem *) umpSystem->getTable(); // findModule returns the pointer to the valid value, DO NOT DELETE!
}

#endif  /* IUMP_H */
