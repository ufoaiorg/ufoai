/**
 * @file material.cpp
 * @brief Material generation code
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

#include "material.h"
#include "radiant_i18n.h"

#include "iradiant.h"
#include "iump.h"
#include "gtkmisc.h"
#include "iselection.h"
#include "brush/brush.h"
#include "commands.h"
#include "map/map.h"
#include "gtkutil/dialog.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "ifilesystem.h"
#include "AutoPtr.h"
#include "iarchive.h"
#include "modulesystem.h"
#include "modulesystem/moduleregistry.h"
#include "modulesystem/singletonmodule.h"
#include "ui/materialeditor/MaterialEditor.h"
#include "itextures.h"

MaterialShader::MaterialShader (const std::string& fileName, const std::string& content) :
	_refcount(0), _fileName(fileName)
{
	_texture = 0;
	_notfound = 0;

	StringInputStream inputStream(content);
	AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewSimpleTokeniser(inputStream));
	parseMaterial(*tokeniser);

	realise();
}

MaterialShader::~MaterialShader ()
{
	unrealise();
}

BlendFactor MaterialShader::parseBlendMode (const std::string token)
{
	if (token == "GL_ONE")
		return BLEND_ONE;
	if (token == "GL_ZERO")
		return BLEND_ZERO;
	if (token == "GL_SRC_ALPHA")
		return BLEND_SRC_ALPHA;
	if (token == "GL_ONE_MINUS_SRC_ALPHA")
		return BLEND_ONE_MINUS_SRC_ALPHA;
	if (token == "GL_SRC_COLOR")
		return BLEND_SRC_COLOUR;
	if (token == "GL_DST_COLOR")
		return BLEND_DST_COLOUR;
	if (token == "GL_ONE_MINUS_SRC_COLOR")
		return BLEND_ONE_MINUS_SRC_COLOUR;
	if (token == "GL_ONE_MINUS_DST_COLOR")
		return BLEND_ONE_MINUS_DST_COLOUR;

	return BLEND_ZERO;
}

void MaterialShader::parseMaterial (Tokeniser& tokeniser)
{
	int depth = 0;
	Vector3 color(1, 1, 1);
	double alphaTest = 0.0f;
	bool terrain = false;
	qtexture_t *layerTexture = 0;
	BlendFactor src = BLEND_SRC_ALPHA;
	BlendFactor dest = BLEND_ONE_MINUS_SRC_ALPHA;
	float ceilVal, floorVal;

	std::string token = tokeniser.getToken();
	while (token.length()) {
		if (token == "{") {
			depth++;
		}
		else if (token == "}") {
			--depth;
			if (depth == 0) {
				MapLayer layer(layerTexture, BlendFunc(src, dest), ShaderLayer::BLEND, color, alphaTest);
				if (terrain) {
					layer.setTerrain(floorVal, ceilVal);
					terrain = false;
				}
				color.set(1, 1, 1);
				addLayer(layer);
				break;
			}
		}
		else if (depth == 2) {
			if (token == "texture") {
				layerTexture = GlobalTexturesCache().capture(GlobalTexturePrefix_get() + tokeniser.getToken());
				if (layerTexture->texture_number == 0) {
					GlobalTexturesCache().release(layerTexture);
					layerTexture = GlobalTexturesCache().capture(GlobalTexturePrefix_get() + "tex_common/nodraw");
				}
			} else if (token == "blend") {
				src = parseBlendMode(tokeniser.getToken());
				dest = parseBlendMode(tokeniser.getToken());
			} else if (token == "lightmap") {
			} else if (token == "envmap") {
				token = tokeniser.getToken();
			} else if (token == "pulse") {
				token = tokeniser.getToken();
			} else if (token == "rotate") {
				token = tokeniser.getToken();
			} else if (token == "color") {
				const float red = string::toFloat(tokeniser.getToken());
				const float green = string::toFloat(tokeniser.getToken());
				const float blue = string::toFloat(tokeniser.getToken());
				color.set(red, green, blue);
			} else if (token == "flare") {
				token = tokeniser.getToken();
			} else if (token == "glowscale") {
				token = tokeniser.getToken();
			} else if (token == "anim" || token == "anima") {
				token = tokeniser.getToken();
				token = tokeniser.getToken();
			} else if (token == "terrain") {
				floorVal = string::toFloat(tokeniser.getToken());
				ceilVal = string::toFloat(tokeniser.getToken());
				terrain = true;
			}
		} else if (depth == 1) {
			if (token == "bump") {
				token = tokeniser.getToken();
			} else if (token == "parallax") {
				token = tokeniser.getToken();
			} else if (token == "hardness") {
				token = tokeniser.getToken();
			} else if (token == "specular") {
				token = tokeniser.getToken();
			} else if (token == "material") {
				token = tokeniser.getToken();
				// must be the same as _fileName
			}
		}
		token = tokeniser.getToken();
	}
}

// IShaders implementation -----------------
void MaterialShader::IncRef ()
{
	++_refcount;
}
void MaterialShader::DecRef ()
{
	if (--_refcount == 0) {
		delete this;
	}
}

std::size_t MaterialShader::refcount ()
{
	return _refcount;
}

// get/set the qtexture_t* Radiant uses to represent this shader object
qtexture_t* MaterialShader::getTexture () const
{
	return _texture;
}

// get shader name
const char* MaterialShader::getName () const
{
	return _fileName.c_str();
}

bool MaterialShader::IsInUse () const
{
	return _inUse;
}

void MaterialShader::SetInUse (bool inUse)
{
	_inUse = inUse;
}

bool MaterialShader::IsValid () const {
	return _isValid;
}

void MaterialShader::SetIsValid (bool bIsValid) {
	_isValid = bIsValid;
}

// get the shader flags
int MaterialShader::getFlags () const
{
	return 0;
}

// get the transparency value
float MaterialShader::getTrans () const
{
	return 1.0f;
}

// test if it's a true shader, or a default shader created to wrap around a texture
bool MaterialShader::IsDefault () const
{
	return _fileName.empty();
}

// get the alphaFunc
void MaterialShader::getAlphaFunc (MaterialShader::EAlphaFunc *func, float *ref)
{
	*func = eAlways;
}

BlendFunc MaterialShader::getBlendFunc () const
{
	return BlendFunc(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);
}

// get the cull type
MaterialShader::ECull MaterialShader::getCull ()
{
	return eCullNone;
}

float MaterialShader::getPolygonOffset() const
{
	return 1.0f;
}

void MaterialShader::forEachLayer(const ShaderLayerCallback& callback) const
{
	for (MapLayers::const_iterator i = m_layers.begin(); i
			!= m_layers.end(); ++i) {
		callback(*i);
	}
}

void MaterialShader::realise ()
{
	_texture = GlobalTexturesCache().capture(_fileName);

	if (_texture->texture_number == 0) {
		_notfound = _texture;
		_texture = GlobalTexturesCache().capture(GlobalTexturePrefix_get() + "tex_common/nodraw");
	}
}

bool MaterialShader::isLayerValid (const MapLayer& layer) const {
	if (layer.getType() == ShaderLayer::BLEND) {
		if (layer.getTexture() == 0)
			return false;
	}
	return true;
}

void MaterialShader::addLayer(MapLayer &layer) {
	if (isLayerValid(layer))
		m_layers.push_back(layer);
}

void MaterialShader::unrealise ()
{
	GlobalTexturesCache().release(_texture);

	for (MapLayers::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
		MapLayer& layer = *i;
		if (layer.getTexture() != 0) {
			GlobalTexturesCache().release(layer.getTexture());
		}
	}
	m_layers.clear();

	if (_notfound != 0) {
		GlobalTexturesCache().release(_notfound);
	}
}

MaterialSystem::MaterialSystem ()
{
}

void MaterialSystem::showMaterialDefinition ()
{
	ui::MaterialEditor editor("");
	editor.show();
}

void MaterialSystem::showMaterialDefinitionAndAppend (const std::string& append)
{
	ui::MaterialEditor editor(append);
	editor.show();
}

void MaterialSystem::generateMaterialForFace (int contentFlags, int surfaceFlags, std::string& textureName,
		std::stringstream& os)
{
	if (textureName.find("dirt") != std::string::npos || textureName.find("rock") != std::string::npos
			|| textureName.find("grass") != std::string::npos) {
		os << "\t{" << std::endl;
		os << "\t\ttexture <fillme>" << std::endl;
		os << "\t\tterrain 0 64" << std::endl;
		os << "\t\tlightmap" << std::endl;
		os << "\t}" << std::endl;
	}

	if (contentFlags & CONTENTS_WATER || textureName.find("glass") != std::string::npos || textureName.find("window")
			!= std::string::npos) {
		os << "\t{" << std::endl;
		os << "\t\tspecular 2.0" << std::endl;
		os << "\t\tenvmap 0" << std::endl;
		os << "\t}" << std::endl;
	}

	if (textureName.find("wood") != std::string::npos || textureName.find("desert") != std::string::npos) {
		os << "\t{" << std::endl;
		os << "\t\tspecular 0.2" << std::endl;
		os << "\t\thardness 0.0" << std::endl;
		os << "\t}" << std::endl;
	}

	if (textureName.find("metal") != std::string::npos) {
		os << "\t{" << std::endl;
		os << "\t\tspecular 0.8" << std::endl;
		os << "\t\thardness 0.5" << std::endl;
		os << "\t}" << std::endl;
	}

	if (textureName.find("wall") != std::string::npos) {
		os << "\t{" << std::endl;
		os << "\t\tspecular 0.6" << std::endl;
		os << "\t\tbump 2.0" << std::endl;
		os << "\t}" << std::endl;
	}
}

bool MaterialSystem::isDefined(const std::string& texture, const std::string& content)
{
	const std::string textureDir = GlobalTexturePrefix_get();
	const std::string& mapname = GlobalRadiant().getMapName();
	if (texture == textureDir || mapname.empty() || Map_Unnamed(g_map))
		return false;

	std::string skippedTextureDirectory = texture.substr(textureDir.length());
	std::string materialDefinition = "material " + skippedTextureDirectory;
	/* check whether there is already an entry for the selected texture */
	if (content.find(materialDefinition) != std::string::npos)
		return true;

	return false;
}

void MaterialSystem::generateMaterialFromTexture ()
{
	const std::string textureDir = GlobalTexturePrefix_get();
	const std::string& mapname = GlobalRadiant().getMapName();
	if (mapname.empty() || Map_Unnamed(g_map)) {
		// save the map first
		gtkutil::errorDialog(_("You have to save your map before material generation can work"));
		return;
	}

	loadMaterials();
	if (!_materialLoaded)
		return;

	std::string append = "";
	if (GlobalSelectionSystem().areFacesSelected()) {
		for (FaceInstancesList::iterator i = g_SelectedFaceInstances.m_faceInstances.begin(); i
				!= g_SelectedFaceInstances.m_faceInstances.end(); ++i) {
			const FaceInstance& faceInstance = *(*i);
			const Face &face = faceInstance.getFace();
			const std::string& texture = face.GetShader();
			// don't generate materials for common textures
			if (texture.find("tex_common") != std::string::npos)
				continue;
			std::string skippedTextureDirectory = texture.substr(textureDir.length());
			std::string materialDefinition = "material " + skippedTextureDirectory;
			/* check whether there is already an entry for the selected texture */
			if (_material.find(materialDefinition) == std::string::npos) {
				std::stringstream os;
				ContentsFlagsValue flags;

				face.GetFlags(flags);

				os << "{" << std::endl;
				os << "\t" << materialDefinition << std::endl;
				os << "\t{" << std::endl;
				generateMaterialForFace(flags.m_contentFlags, flags.m_surfaceFlags, skippedTextureDirectory, os);
				os << "\t}" << std::endl;
				os << "}" << std::endl;

				append += os.str();
			}
		}
	}

	showMaterialDefinitionAndAppend(append);
}

void MaterialSystem::importMaterialFile (const std::string& name)
{
	// Find the material
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(name));
	if (!file)
		return;

	showMaterialDefinitionAndAppend(file->getString());
}

const std::string MaterialSystem::getMaterialFilename () const
{
	const std::string& mapname = GlobalRadiant().getMapName();
	const std::string umpname = GlobalUMPSystem()->getUMPFilename(mapname);
	std::string materialFilename;
	if (umpname.empty())
		materialFilename = os::getFilenameFromPath(mapname);
	else
		materialFilename = os::getFilenameFromPath(umpname);
	std::string relativePath = "materials/" + os::stripExtension(materialFilename) + ".mat";
	return relativePath;
}

std::string MaterialSystem::getBlock (const std::string& texture)
{
	const std::string textureDir = GlobalTexturePrefix_get();
	std::string skippedTextureDirectory = texture.substr(textureDir.length());

	MaterialBlockMap::iterator i = _blocks.find(skippedTextureDirectory);
	if (i != _blocks.end())
		return i->second;

	if (!_materialLoaded)
		loadMaterials();

	if (!_materialLoaded)
		return "";

	StringOutputStream outputStream;

	StringInputStream inputStream(_material);
	AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewSimpleTokeniser(inputStream));
	int depth = 0;
	bool found = false;
	std::string token = tokeniser->getToken();
	while (token.length()) {
		if (token == "{") {
			depth++;
		} else if (token == "}") {
			depth--;
		}
		if (depth >= 1) {
			if (depth == 1 && token == "material") {
				token = tokeniser->getToken();
				if (token == skippedTextureDirectory) {
					found = true;
					outputStream << "{ material ";
				}
			}
			if (found)
				outputStream << token << " ";
		} else if (found) {
			outputStream << "}";
			break;
		}
		token = tokeniser->getToken();
	}
	return outputStream.toString();
}

void MaterialSystem::freeMaterials ()
{
	for (MaterialShaders::iterator i = _activeMaterialShaders.begin(); i != _activeMaterialShaders.end(); ++i) {
		ASSERT_MESSAGE(i->second->refcount() == 1, "orphan material still referenced");
	}
	_activeMaterialShaders.clear();
	_blocks.clear();
	_materialLoaded = false;
}

void MaterialSystem::loadMaterials ()
{
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(getMaterialFilename()));
	if (file) {
		_material = file->getString();
		_materialLoaded = true;
	}
}

IShader* MaterialSystem::getMaterialForName (const std::string& name)
{
	MaterialShaders::iterator i = _activeMaterialShaders.find(name);
	if (i != _activeMaterialShaders.end()) {
		return (*i).second;
	}

	std::string block = getBlock(name);
	if (block.empty())
		return (IShader*)0;

	MaterialPointer pShader(new MaterialShader(name, block));
	_activeMaterialShaders.insert(MaterialShaders::value_type(name, pShader));
	return pShader;
}

void MaterialSystem::foreachMaterialName (const ShaderNameCallback& callback)
{
	for (MaterialShaders::const_iterator i = _activeMaterialShaders.begin(); i != _activeMaterialShaders.end(); ++i) {
		const std::string& str = (*i).first;
		callback(str);
	}
}

void MaterialSystem::foreachMaterialName (const ShaderSystem::Visitor& visitor)
{
	for (MaterialShaders::const_iterator i = _activeMaterialShaders.begin(); i != _activeMaterialShaders.end(); ++i) {
		const std::string& str = (*i).first;
		visitor.visit(str);
	}
}

void MaterialSystem::beginActiveMaterialsIterator ()
{
	_activeMaterialsIterator = _activeMaterialShaders.begin();
}

bool MaterialSystem::endActiveMaterialsIterator ()
{
	return _activeMaterialsIterator == _activeMaterialShaders.end();
}

IShader* MaterialSystem::dereferenceActiveMaterialsIterator ()
{
	return _activeMaterialsIterator->second;
}

void MaterialSystem::incrementActiveMaterialsIterator ()
{
	++_activeMaterialsIterator;
}

void MaterialSystem::construct () {
	GlobalRadiant().commandInsert("GenerateMaterialFromTexture", MemberCaller<MaterialSystem,
			&MaterialSystem::generateMaterialFromTexture> (*this), Accelerator('M'));
	command_connect_accelerator("GenerateMaterialFromTexture");

	GlobalRadiant().commandInsert("ShowMaterialDefinition", MemberCaller<MaterialSystem,
			&MaterialSystem::showMaterialDefinition> (*this), accelerator_null());
}


class MaterialSystemAPI
{
		MaterialSystem * _materialSystem;
	public:
		typedef MaterialSystem Type;
		STRING_CONSTANT(Name, "*");

		MaterialSystemAPI () :
			_materialSystem(0)
		{
			_materialSystem = new MaterialSystem();
		}
		~MaterialSystemAPI ()
		{
			delete _materialSystem;
		}

		MaterialSystem* getTable ()
		{
			return _materialSystem;
		}
};

typedef SingletonModule<MaterialSystemAPI> MaterialSystemModule;
typedef Static<MaterialSystemModule> StaticMaterialSystemModule;
StaticRegisterModule staticRegisterMaterial(StaticMaterialSystemModule::instance());

void Material_Construct ()
{
	GlobalMaterialSystem()->construct();
}

void Material_Destroy ()
{
}
