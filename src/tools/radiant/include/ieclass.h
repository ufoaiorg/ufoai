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

/// \file ieclass.h
/// \brief Entity Class definition loader API.


#if !defined(INCLUDED_IECLASS_H)
#define INCLUDED_IECLASS_H

#include "generic/constant.h"

#define	MAX_FLAGS	16

class Shader;

class EntityClass;
class ListAttributeType;

class EntityClassCollector
{
	public:
		virtual ~EntityClassCollector ()
		{
		}

		virtual void insert (EntityClass* eclass) = 0;
		virtual void insert (const std::string& name, const ListAttributeType& list)
		{
		}
};

class EntityClassScanner
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "eclass");

		virtual ~EntityClassScanner ()
		{
		}

		virtual void scanFile (EntityClassCollector& collector) = 0;
};

#include "modulesystem.h"

template<typename Type>
class ModuleRef;
typedef ModuleRef<EntityClassScanner> EClassModuleRef;

template<typename Type>
class Modules;
typedef Modules<EntityClassScanner> EClassModules;

template<typename Type>
class ModulesRef;
typedef ModulesRef<EntityClassScanner> EClassModulesRef;

class EntityClassVisitor
{
	public:
		virtual ~EntityClassVisitor ()
		{
		}
		virtual void visit (EntityClass* eclass) = 0;
};

class ModuleObserver;

/** EntityClassManager interface. The entity class manager
 * is responsible for maintaining a list of available entity
 * classes which the EntityCreator can insert into a map.
 */
class IEntityClassManager
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "eclassmanager");

		virtual ~IEntityClassManager() {}

		virtual EntityClass* findOrInsert (const std::string& name, bool has_brushes) = 0;
		virtual const ListAttributeType* findListType (const std::string& name) = 0;
		virtual void forEach (EntityClassVisitor& visitor) = 0;
		virtual void attach (ModuleObserver& observer) = 0;
		virtual void detach (ModuleObserver& observer) = 0;
		virtual void realise () = 0;
		virtual void unrealise () = 0;
};

template<typename Type>
class GlobalModule;
typedef GlobalModule<IEntityClassManager> GlobalEntityClassManagerModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<IEntityClassManager> GlobalEntityClassManagerModuleRef;

inline IEntityClassManager& GlobalEntityClassManager ()
{
	return GlobalEntityClassManagerModule::getTable();
}

#endif
