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
#include "moduleobserver.h"
#include "generic/constant.h"
#include <string>
#include <map>
#include "ifilesystem.h"
#include "ishader.h"

class MaterialShader: public IShader
{
	private:

		std::size_t _refcount;

		std::string _fileName;

		BlendFunc _blendFunc;

		bool _inUse;

		qtexture_t* _texture;

		qtexture_t* _notfound;

	public:

		MaterialShader (const std::string& fileName);

		virtual ~MaterialShader ();

		// IShaders implementation -----------------
		void IncRef ();
		void DecRef ();

		std::size_t refcount ();

		// get/set the qtexture_t* Radiant uses to represent this shader object
		qtexture_t* getTexture () const;

		// get shader name
		const char* getName () const;

		bool IsInUse () const;

		void SetInUse (bool inUse);

		bool IsValid () const = 0;

		void SetIsValid (bool bIsValid) = 0;

		// get the shader flags
		int getFlags () const;

		// get the transparency value
		float getTrans () const;

		// test if it's a true shader, or a default shader created to wrap around a texture
		bool IsDefault () const;

		// get the alphaFunc
		void getAlphaFunc (EAlphaFunc *func, float *ref);

		BlendFunc getBlendFunc () const;

		// get the cull type
		ECull getCull ();

		void realise ();

		void unrealise ();
};


class MaterialSystem
{
	private:

		void generateMaterialForFace (int contentFlags, int surfaceFlags, std::string& textureName,
				std::stringstream& stream);

		typedef std::map<std::string, MaterialShader*> MaterialShaders;
		MaterialShaders _activeMaterialShaders;

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
		void showMaterialDefinition (const std::string& append = "");

		/**
		 * @return The current material filename for the current loaded map
		 */
		const std::string getMaterialFilename () const;

		/**
		 * Import a material file to the already existing material definition
		 * @param name The material file to import
		 */
		void importMaterialFile (const std::string& name);

		/**
		 * Generates material for the current selected textures
		 */
		void generateMaterialFromTexture ();

		/**
		 * activate the material for a given name and return it
		 * will return the default shader if name is not found
		 */
		IShader* getMaterialForName (const std::string& name);
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
