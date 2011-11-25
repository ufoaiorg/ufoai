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

#include "imaterial.h"
#include "iradiant.h"
#include "ieventmanager.h"
#include "iump.h"
#include "iselection.h"
#include "ifilesystem.h"
#include "iarchive.h"
#include "itextures.h"
#include "iscriplib.h"

#include "radiant_i18n.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "AutoPtr.h"
#include "shared.h"
#include "gtkutil/dialog.h"

#include "../map/map.h"
#include "../ui/materialeditor/MaterialEditor.h"
#include "../brush/FaceInstance.h"
#include "../brush/ContentsFlagsValue.h"

MaterialSystem::MaterialSystem () : _materialLoaded(false)
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

	if ((contentFlags & CONTENTS_WATER) || textureName.find("glass") != std::string::npos || textureName.find("window")
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
	if (texture == textureDir || GlobalMap().isUnnamed())
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
	if (GlobalMap().isUnnamed()) {
		// save the map first
		gtkutil::errorDialog(_("You have to save your map before material generation can work"));
		return;
	}

	loadMaterials();
	if (!_materialLoaded)
		return;

	const std::string textureDir = GlobalTexturePrefix_get();

	std::string append = "";
	if (GlobalSelectionSystem().areFacesSelected()) {
		for (FaceInstancesList::iterator i = g_SelectedFaceInstances.m_faceInstances.begin(); i
				!= g_SelectedFaceInstances.m_faceInstances.end(); ++i) {
			const FaceInstance& faceInstance = *(*i);
			const Face *face = faceInstance.getFacePtr();
			const std::string& texture = face->GetShader();
			// don't generate materials for common textures
			if (texture.find("tex_common") != std::string::npos)
				continue;
			std::string skippedTextureDirectory = texture.substr(textureDir.length());
			std::string materialDefinition = "material " + skippedTextureDirectory;
			/* check whether there is already an entry for the selected texture */
			if (_material.find(materialDefinition) == std::string::npos) {
				std::stringstream os;
				const ContentsFlagsValue& faceFlags = face->GetFlags();

				os << "{" << std::endl;
				os << "\t" << materialDefinition << std::endl;
				os << "\t{" << std::endl;
				generateMaterialForFace(faceFlags.getContentFlags(), faceFlags.getSurfaceFlags(), skippedTextureDirectory, os);
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
	const std::string& mapname = GlobalMap().getName();
	const std::string umpname = GlobalUMPSystem().getUMPFilename(mapname);
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
	if (texture.empty())
		return "";

	const std::string textureDir = GlobalTexturePrefix_get();
	std::string skippedTextureDirectory = texture.substr(textureDir.length());

	if (skippedTextureDirectory.empty())
		return "";

	MaterialBlockMap::iterator i = _blocks.find(skippedTextureDirectory);
	if (i != _blocks.end())
		return i->second;

	if (!_materialLoaded)
		loadMaterials();

	if (!_materialLoaded)
		return "";

	StringOutputStream outputStream;

	StringInputStream inputStream(_material);
	AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().createSimpleTokeniser(inputStream));
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
	_blocks.clear();
	_materialLoaded = false;
}

void MaterialSystem::loadMaterials ()
{
	if (GlobalMap().isUnnamed())
		return;

	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(getMaterialFilename()));
	if (file) {
		_material = file->getString();
		_materialLoaded = true;
	}
}

void MaterialSystem::construct () {
	GlobalEventManager().addCommand("GenerateMaterialFromTexture", MemberCaller<MaterialSystem,
			&MaterialSystem::generateMaterialFromTexture> (*this));
	GlobalEventManager().addCommand("ShowMaterialDefinition", MemberCaller<MaterialSystem,
			&MaterialSystem::showMaterialDefinition> (*this));
}
