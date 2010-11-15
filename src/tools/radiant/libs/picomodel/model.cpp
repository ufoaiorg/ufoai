/**
 * @file model.cpp
 */

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

#include <string>

#include "model.h"

#include "picomodel.h"
#include "RenderablePicoModel.h"

#include "iarchive.h"
#include "idatastream.h"
#include "imodel.h"

#include "cullable.h"
#include "renderable.h"
#include "selectable.h"

#include "math/frustum.h"
#include "string/string.h"
#include "generic/static.h"
#include "entitylib.h"
#include "shaderlib.h"
#include "scenelib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "render.h"
#include "os/path.h"
#include "RenderablePicoSurface.h"
#include "RenderablePicoModel.h"
#include "SkinnedModel.h"

#include "PicoModelNode.h"

const AABB& PicoModelInstance::localAABB() const
{
	return m_picomodel.localAABB();
}

// Cullable implementation
VolumeIntersectionValue PicoModelInstance::intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const
{
	return m_picomodel.intersectVolume(test, localToWorld);
}

PicoModelInstance::PicoModelInstance (const scene::Path& path, scene::Instance* parent, model::RenderablePicoModel& picomodel) :
	Instance(path, parent), m_picomodel(picomodel),
			m_skins(m_picomodel.getSurfaceCount())
{
}
PicoModelInstance::~PicoModelInstance ()
{
}

void PicoModelInstance::render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	const model::SurfaceList& surfaceList = m_picomodel.getSurfaces();
	SurfaceRemaps::const_iterator k = m_skins.begin();
	for (model::SurfaceList::const_iterator i = surfaceList.begin(); i != surfaceList.end(); ++i, ++k) {
		if (i->intersectVolume(volume, localToWorld) != VOLUME_OUTSIDE) {
			i->render(renderer, localToWorld, (*k).second != 0 ? (*k).second : i->getShader());
		}
	}
}

// greebo: Updates the model's surface remaps. Pass the new skin name (can be empty).
void PicoModelInstance::skinChanged (const std::string& newSkinName) {
	if (newSkinName == _skin)
		return;

	// The new skin name is stored locally
	_skin = newSkinName;
	m_picomodel.applySkin(_skin);
}

// Returns the name of the currently active skin
std::string PicoModelInstance::getSkin () const {
	return _skin;
}

void PicoModelInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	render(renderer, volume, Instance::localToWorld());
}
void PicoModelInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	renderSolid(renderer, volume);
}

void PicoModelInstance::testSelect (Selector& selector, SelectionTest& test)
{
	m_picomodel.testSelect(selector, test, Instance::localToWorld());
}

/** Construct a PicoModelNode with the parsed picoModel_t struct and the
 * provided file extension.
 */
PicoModelNode::PicoModelNode (picoModel_t* model, const std::string& ext) :
	m_picomodel(model)
{
}

scene::Node& PicoModelNode::node ()
{
	return *this;
}

scene::Instance* PicoModelNode::create (const scene::Path& path, scene::Instance* parent)
{
	return new PicoModelInstance(path, parent, m_picomodel);
}
void PicoModelNode::forEachInstance (const scene::Instantiable::Visitor& visitor)
{
	m_instances.forEachInstance(visitor);
}
void PicoModelNode::insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
{
	m_instances.insert(observer, path, instance);
}
scene::Instance* PicoModelNode::erase (scene::Instantiable::Observer* observer, const scene::Path& path)
{
	return m_instances.erase(observer, path);
}

void PicoModelNode::skinChanged (const std::string& newSkinName)
{
	m_picomodel.applySkin(newSkinName);
}

inline size_t picoInputStreamReam (void* inputStream, unsigned char* buffer, size_t length)
{
	return reinterpret_cast<InputStream*> (inputStream)->read(buffer, length);
}

/* Use the picomodel library to load the contents of the given file
 * and return a Node containing the model.
 */
scene::Node& loadPicoModel (const picoModule_t* module, ArchiveFile& file)
{

	//Determine the file extension (ASE or LWO) to pass down to the PicoModel
	std::string fName = file.getName();
	std::string fExt = string::toLower(os::getExtension(fName));

	picoModel_t* model = PicoModuleLoadModelStream(module, file.getName().c_str(), &file.getInputStream(), picoInputStreamReam,
			file.size(), 0);
	PicoModelNode* modelNode = new PicoModelNode(model, fExt);
	PicoFreeModel(model);
	return *modelNode;
}

/* Load the provided file as a model object and return as an IModel
 * shared pointer.
 */
model::IModelPtr loadIModel (const picoModule_t* module, ArchiveFile& file)
{
	picoModel_t* model = PicoModuleLoadModelStream(module, file.getName().c_str(), &file.getInputStream(), picoInputStreamReam,
			file.size(), 0);
	model::IModelPtr modelObj(new model::RenderablePicoModel(model));
	PicoFreeModel(model);
	return modelObj;
}
