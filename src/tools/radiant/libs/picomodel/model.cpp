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
#include "VectorLightList.h"

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

class PicoModel: public Cullable, public Bounded
{
		typedef std::vector<model::RenderablePicoSurface*> surfaces_t;
		surfaces_t m_surfaces;

		AABB m_aabb_local;
	public:
		Callback m_lightsChanged;

		PicoModel ()
		{
			constructNull();
		}

		/** Construct a PicoModel object from the provided picoModel_t struct and
		* the given file extension.
		*/
		PicoModel(picoModel_t* model, const std::string& ext) {
			CopyPicoModel(model, ext);
		}

		~PicoModel ()
		{
			for (surfaces_t::iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i)
				delete *i;
		}

		typedef surfaces_t::const_iterator const_iterator;

		const_iterator begin () const
		{
			return m_surfaces.begin();
		}
		const_iterator end () const
		{
			return m_surfaces.end();
		}
		std::size_t size () const
		{
			return m_surfaces.size();
		}

		VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const
		{
			return test.TestAABB(m_aabb_local, localToWorld);
		}

		virtual const AABB& localAABB () const
		{
			return m_aabb_local;
		}

		void render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
				std::vector<Shader*> states) const
		{
			for (surfaces_t::const_iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i) {
				if ((*i)->intersectVolume(volume, localToWorld) != VOLUME_OUTSIDE) {
					(*i)->render(renderer, localToWorld, states[i - m_surfaces.begin()]);
				}
			}
		}

		void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
		{
			for (surfaces_t::iterator i = m_surfaces.begin(); i != m_surfaces.end(); ++i) {
				if ((*i)->intersectVolume(test.getVolume(), localToWorld) != VOLUME_OUTSIDE) {
					(*i)->testSelect(selector, test, localToWorld);
				}
			}
		}

	private:

		/** Copy a picoModel_t struct returned from the picomodel library into
		 * the required C++ objects for a PicoModel object. The file extension
		 * is used to determine whether the Doom 3 shader is chosen from the
		 * material name (LWO objects) or the bitmap patch (ASE objects).
		 */

		void CopyPicoModel (picoModel_t* model, const std::string& fileExt)
		{
			m_aabb_local = AABB();

			/* each surface on the model will become a new map drawsurface */
			int numSurfaces = PicoGetModelNumSurfaces(model);
			for (int s = 0; s < numSurfaces; ++s) {
				/* get surface */
				picoSurface_t* surface = PicoGetModelSurface(model, s);
				if (surface == 0)
					continue;

				/* only handle triangle surfaces initially (fixme: support patches) */
				if (PicoGetSurfaceType(surface) != PICO_TRIANGLES)
					continue;

				/* fix the surface's normals */
				PicoFixSurfaceNormals(surface);

				model::RenderablePicoSurface* picosurface = new model::RenderablePicoSurface(surface);
				m_aabb_local.includeAABB(picosurface->localAABB());
				m_surfaces.push_back(picosurface);
			}
		}
		void constructNull ()
		{
			model::RenderablePicoSurface* picosurface = new model::RenderablePicoSurface(NULL);
			m_aabb_local = picosurface->localAABB();
			m_surfaces.push_back(picosurface);
		}
};

class PicoModelInstance: public scene::Instance, public Renderable, public SelectionTestable, public LightCullable
{
		class TypeCasts
		{
				InstanceTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
					InstanceContainedCast<PicoModelInstance, Bounded>::install(m_casts);
					InstanceContainedCast<PicoModelInstance, Cullable>::install(m_casts);
					InstanceStaticCast<PicoModelInstance, Renderable>::install(m_casts);
					InstanceStaticCast<PicoModelInstance, SelectionTestable>::install(m_casts);
					//InstanceStaticCast<PicoModelInstance, SkinnedModel>::install(m_casts);
				}
				InstanceTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		PicoModel& m_picomodel;

		const LightList* m_lightList;
		typedef Array<VectorLightList> SurfaceLightLists;
		SurfaceLightLists m_surfaceLightLists;

		class Remap
		{
			public:
				std::string first;
				Shader* second;
				Remap () :
					second(0)
				{
				}
		};
		typedef Array<Remap> SurfaceRemaps;
		SurfaceRemaps m_skins;

		PicoModelInstance (const PicoModelInstance&);
		PicoModelInstance operator= (const PicoModelInstance&);
	public:
		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		void* m_test;

		Bounded& get (NullType<Bounded> )
		{
			return m_picomodel;
		}
		Cullable& get (NullType<Cullable> )
		{
			return m_picomodel;
		}

		void lightsChanged ()
		{
			m_lightList->lightsChanged();
		}
		typedef MemberCaller<PicoModelInstance, &PicoModelInstance::lightsChanged> LightsChangedCaller;

		PicoModelInstance (const scene::Path& path, scene::Instance* parent, PicoModel& picomodel) :
			Instance(path, parent, this, StaticTypeCasts::instance().get()), m_picomodel(picomodel),
					m_surfaceLightLists(m_picomodel.size()), m_skins(m_picomodel.size())
		{
			m_lightList = &GlobalShaderCache().attach(*this);
			m_picomodel.m_lightsChanged = LightsChangedCaller(*this);

			Instance::setTransformChangedCallback(LightsChangedCaller(*this));
		}
		~PicoModelInstance ()
		{
			Instance::setTransformChangedCallback(Callback());

			m_picomodel.m_lightsChanged = Callback();
			GlobalShaderCache().detach(*this);
		}

		void render (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			SurfaceLightLists::const_iterator j = m_surfaceLightLists.begin();
			SurfaceRemaps::const_iterator k = m_skins.begin();
			for (PicoModel::const_iterator i = m_picomodel.begin(); i != m_picomodel.end(); ++i, ++j, ++k) {
				if ((*i)->intersectVolume(volume, localToWorld) != VOLUME_OUTSIDE) {
					renderer.setLights(*j);
					(*i)->render(renderer, localToWorld, (*k).second != 0 ? (*k).second : (*i)->getShader());
				}
			}
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume) const
		{
			m_lightList->evaluateLights();

			render(renderer, volume, Instance::localToWorld());
		}
		void renderWireframe (Renderer& renderer, const VolumeTest& volume) const
		{
			renderSolid(renderer, volume);
		}

		void testSelect (Selector& selector, SelectionTest& test)
		{
			m_picomodel.testSelect(selector, test, Instance::localToWorld());
		}

		void insertLight (const RendererLight& light)
		{
		}
		void clearLights ()
		{
		}
};

class PicoModelNode: public scene::Node, public scene::Instantiable
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
				}
				NodeTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		InstanceSet m_instances;
		PicoModel m_picomodel;

	public:
		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		/** Construct a PicoModelNode with the parsed picoModel_t struct and the
		 * provided file extension.
		 */
		PicoModelNode (picoModel_t* model, const std::string& ext) :
			scene::Node(this, StaticTypeCasts::instance().get()),
			m_picomodel(model,ext) //pass extension down to the PicoModel
		{
		}

		scene::Node& node ()
		{
			return *this;
		}

		scene::Instance* create (const scene::Path& path, scene::Instance* parent)
		{
			return new PicoModelInstance(path, parent, m_picomodel);
		}
		void forEachInstance (const scene::Instantiable::Visitor& visitor)
		{
			m_instances.forEachInstance(visitor);
		}
		void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
		{
			m_instances.insert(observer, path, instance);
		}
		scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path)
		{
			return m_instances.erase(observer, path);
		}
};

size_t picoInputStreamReam (void* inputStream, unsigned char* buffer, size_t length)
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
	return modelNode->node();
}

#include "RenderablePicoModel.h"

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
