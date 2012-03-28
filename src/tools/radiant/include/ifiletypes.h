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

#if !defined(INCLUDED_IFILETYPES_H)
#define INCLUDED_IFILETYPES_H

#include "generic/constant.h"

class filetype_t
{
	public:
		filetype_t () :
			name(""), pattern("")
		{
		}
		filetype_t (const std::string& _name, const std::string& _pattern) :
			name(_name), pattern(_pattern)
		{
		}
		const std::string name;
		const std::string pattern;
};

class IFileTypeList
{
	public:
		virtual ~IFileTypeList ()
		{
		}
		virtual void addType (const std::string& moduleName, filetype_t type) = 0;
};

/**
 * Interface for the FileType registry module. This module retains a list of
 * FileTypePattern objects along with their associated module names.
 */
class IFileTypeRegistry
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "filetypes");

		virtual ~IFileTypeRegistry ()
		{
		}
		/**
		 * Add a type to the registry.
		 *
		 * @param moduleType
		 * The type of the module, e.g. "map".
		 *
		 * @param moduleName
		 * Name of the module.
		 *
		 * @param type
		 * The FileTypePattern to associate with this module name.
		 */
		virtual void addType (const std::string& moduleType, const std::string& moduleName, filetype_t type) = 0;
		virtual void getTypeList (const std::string& moduleType, IFileTypeList* typelist) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<IFileTypeRegistry> GlobalFiletypesModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IFileTypeRegistry> GlobalFiletypesModuleRef;

inline IFileTypeRegistry& GlobalFiletypes ()
{
	return GlobalFiletypesModule::getTable();
}

#endif
