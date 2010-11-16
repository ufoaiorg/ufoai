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
#include "generic/referencecounted.h"
#include "generic/constant.h"
#include <string>
#include <map>
#include <vector>
#include "ifilesystem.h"
#include "ishader.h"
#include "iscriplib.h"
#include "shaderlib.h"

class MapLayer: public ShaderLayer {
	private:
		qtexture_t* m_texture;
		BlendFunc m_blendFunc;
		bool m_clampToBorder;
		double m_alphaTest;
		ShaderLayer::Type m_type;
		Vector3 m_color;
		float _height;
		float _ceilVal;
		float _floorVal;
		bool _terrain;
	public:
		MapLayer(qtexture_t* texture, BlendFunc blendFunc, ShaderLayer::Type type, Vector3& color,
				double alphaTest) :
			m_texture(texture), m_blendFunc(blendFunc), m_alphaTest(alphaTest), m_type(type),
					m_color(color), _terrain(false) {
		}

		Type getType() const {
			return m_type;
		}

		Vector3 getColour() const {
			return m_color;
		}

		qtexture_t* getTexture() const {
			return m_texture;
		}

		BlendFunc getBlendFunc() const {
			return m_blendFunc;
		}

		double getAlphaTest() const {
			return m_alphaTest;
		}

		void setTerrain(float floorVal, float ceilVal) {
			_floorVal = floorVal;
			_ceilVal = ceilVal;
			_height = _ceilVal - _floorVal;
			_terrain = true;
		}

		bool isTerrain() {
			return _terrain;
		}

		float getTerrainAlpha(float z) const {
			if (z < _floorVal)
				return 0.0;
			else if (z > _ceilVal)
				return 1.0;
			else
				return (z - _floorVal) / _height;
		}
};

class MaterialShader: public IShader {
	private:

		std::size_t _refcount;

		std::string _fileName;

		bool _inUse;

		bool _isValid;

		qtexture_t* _texture;

		qtexture_t* _notfound;

	public:

		MaterialShader(const std::string& fileName, const std::string& content);

		virtual ~MaterialShader();

		// IShaders implementation -----------------
		void IncRef();
		void DecRef();

		std::size_t refcount();

		BlendFactor parseBlendMode(const std::string& token);
		void parseMaterial(Tokeniser& tokenizer);

		// get/set the qtexture_t* Radiant uses to represent this shader object
		qtexture_t* getTexture() const;

		// get shader name
		const char* getName() const;

		bool IsInUse() const;

		void SetInUse(bool inUse);

		bool IsValid() const;

		void SetIsValid(bool bIsValid);

		// get the shader flags
		int getFlags() const;

		// get the transparency value
		float getTrans() const;

		// test if it's a true shader, or a default shader created to wrap around a texture
		bool IsDefault() const;

		// get the alphaFunc
		void getAlphaFunc(EAlphaFunc *func, float *ref);

		BlendFunc getBlendFunc() const;

		// get the cull type
		ECull getCull();

		void realise();

		void unrealise();

		typedef std::vector<MapLayer> MapLayers;
		MapLayers m_layers;

		void addLayer(MapLayer &layer);

		bool isLayerValid(const MapLayer& layer) const;

		void forEachLayer(const ShaderLayerCallback& callback) const;

		float getPolygonOffset() const;
};

class MaterialSystem {
	private:

		void generateMaterialForFace(int contentFlags, int surfaceFlags, std::string& textureName,
				std::stringstream& stream);

		typedef SmartPointer<MaterialShader> MaterialPointer;
		typedef std::map<std::string, MaterialPointer, shader_less_t> MaterialShaders;

		typedef std::map<std::string, std::string> MaterialBlockMap;
		MaterialBlockMap _blocks;

		std::string _material;
		bool _materialLoaded;

		MaterialShaders _activeMaterialShaders;

		MaterialShaders::iterator _activeMaterialsIterator;

	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "material");

		virtual ~MaterialSystem() {
		}

		/**
		 * Constructor
		 */
		MaterialSystem();

		/**
		 * Shows the existing material definition and append new content to it.
		 * @param append The material definition string to append to the existing one
		 */
		void showMaterialDefinitionAndAppend(const std::string& append = "");
		void showMaterialDefinition();

		/**
		 * @return The current material filename for the current loaded map
		 */
		const std::string getMaterialFilename() const;

		/**
		 * Import a material file to the already existing material definition
		 * @param name The material file to import
		 */
		void importMaterialFile(const std::string& name);

		/**
		 * Generates material for the current selected textures
		 */
		void generateMaterialFromTexture();

		/**
		 * Checks whether the material for the texture is already defined
		 * @param texture The texture name including textures/
		 * @return @c true if found, @c false if not
		 */
		bool isDefined(const std::string& texture, const std::string& content);

		std::string getBlock(const std::string& texture);

		/**
		 * activate the material for a given name and return it
		 * will return the default shader if name is not found
		 */
		IShader* getMaterialForName(const std::string& name);

		void foreachMaterialName(const ShaderNameCallback& callback);

		void foreachMaterialName(const ShaderSystem::Visitor& visitor);

		void loadMaterials();

		void freeMaterials();

		void beginActiveMaterialsIterator();

		bool endActiveMaterialsIterator();

		IShader* dereferenceActiveMaterialsIterator();

		void incrementActiveMaterialsIterator();

		void construct();
};

class MaterialSystemDependencies: public GlobalFileSystemModuleRef {
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
inline MaterialSystem * GlobalMaterialSystem() {
	Module * materialSystem = globalModuleServer().findModule(
			MaterialSystem::Name_CONSTANT_::evaluate(),
			MaterialSystem::Version_CONSTANT_::evaluate(), "*");
	ASSERT_MESSAGE(materialSystem,
			"Couldn't retrieve GlobalMaterialSystem, is not registered and/or initialized.");
	return (MaterialSystem *) materialSystem->getTable(); // findModule returns the pointer to the valid value, DO NOT DELETE!
}

#endif  /* IMATERIAL_H */
