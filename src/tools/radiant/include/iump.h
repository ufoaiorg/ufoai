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

#include "generic/constant.h"

#include <string>
#include <set>

class IUMPSystem
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "ump");

		virtual ~IUMPSystem ()
		{
		}

		virtual void editUMPDefinition () = 0;

		/**
		 * @return The ump filename for the given map
		 */
		virtual std::string getUMPFilename (const std::string& map) = 0;

		virtual void init () = 0;

		/**
		 * @return A vector with ump filesnames
		 */
		virtual const std::set<std::string> getFiles () const = 0;

};
#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IUMPSystem> GlobalUMPSystemModule;

// A reference to the call above.
template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IUMPSystem> GlobalUMPSystemModuleRef;

// This is the accessor for the event manager
inline IUMPSystem& GlobalUMPSystem() {
	return GlobalUMPSystemModule::getTable();
}

#endif  /* IUMP_H */
