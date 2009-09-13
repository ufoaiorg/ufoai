/**
 * @file material.h
 * @brief Material generation headers
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

#ifndef IMATERIAL_H
#define IMATERIAL_H

#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "generic/constant.h"
#include <string>
#include "ifilesystem.h"

class MaterialSystem
{
	private:
		/**
		 * @return The current material filename for the current loaded map
		 */
		const std::string getMaterialFilename () const;

	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "material");

		virtual ~MaterialSystem ()
		{
		}

		/**
		 * Constructor
		 */
		MaterialSystem ();

		/**
		 * Shows the existing material definition and append new content to it.
		 * @param append The material definition string to append to the existing one
		 */
		void showMaterialDefinition (const std::string& append);

		/**
		 * Generates material for the current selected textures
		 */
		void generateMaterialFromTexture ();
};

class MaterialSystemDependencies: public GlobalFileSystemModuleRef
{
};

// This is needed to be registered as a Radiant dependency
template<typename Type>
class GlobalModule;
typedef GlobalModule<MaterialSystem> GlobalMaterialSystemModule;

// A reference to the call above.
template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<MaterialSystem> GlobalMaterialSystemModuleRef;

// Accessor method
inline MaterialSystem * GlobalMaterialSystem ()
{
	Module * materialSystem = globalModuleServer().findModule(MaterialSystem::Name_CONSTANT_::evaluate(),
			MaterialSystem::Version_CONSTANT_::evaluate(), "*");
	ASSERT_MESSAGE(materialSystem,
			"Couldn't retrieve GlobalMaterialSystem, is not registered and/or initialized.");
	return (MaterialSystem *) materialSystem->getTable(); // findModule returns the pointer to the valid value, DO NOT DELETE!
}

#endif  /* IMATERIAL_H */
