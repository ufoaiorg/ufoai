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

///\file
///\brief Represents the misc_particle entity.
///
/// This entity displays the particle specified in its "particle" key.
/// The "origin" key directly controls the entity's local-to-parent transform.

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "iregistry.h"
#include "iparticles.h"

#include "math/frustum.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "math/line.h"

#include "ifilesystem.h"

#include "targetable.h"
#include "keys/OriginKey.h"
#include "namedentity.h"
#include "keys/KeyObserverMap.h"
#include "NameKeys.h"
#include "model.h"
#include "EntityCreator.h"
#include "miscparticle.h"

/**
 * @brief Render class for the grid windows
 */
class RenderableParticleID: public OpenGLRenderable
{
		const IParticleDefinition* m_particle;
		const Vector3 &m_origin;
	public:
		RenderableParticleID (const IParticleDefinition* particle, const Vector3 &origin) :
			m_particle(particle), m_origin(origin)
		{
		}

		void render (RenderStateFlags state) const
		{
			if (m_particle != 0) {
				glRasterPos3fv(m_origin);
				GlobalOpenGL().drawString(m_particle->getName());
			}
		}
};

/**
 * @brief Render class for the 3d view
 */
class RenderableParticle: public OpenGLRenderable
{
		const IParticleDefinition* m_particle;
	public:
		RenderableParticle (const IParticleDefinition* particle) :
			m_particle(particle)
		{
		}

		void render (RenderStateFlags state) const
		{
			/** @todo render the image and/or model */
		}
};

inline void read_aabb (AABB& aabb, const EntityClass& eclass)
{
	aabb = AABB::createFromMinMax(eclass.mins, eclass.maxs);
}

class MiscParticle: public Cullable, public Bounded, public Snappable
{
		EntityKeyValues m_entity;
		KeyObserverMap m_keyObservers;
		MatrixTransform m_transform;

		OriginKey m_originKey;
		Vector3 m_origin;

		NamedEntity m_named;
		NameKeys m_nameKeys;
		IParticleDefinition* m_particle;
		mutable Vector3 m_id_origin;

		// bounding box
		AABB m_aabb_local;

		RenderableSolidAABB m_renderAABBSolid;
		RenderableParticle m_renderParticle;
		RenderableParticleID m_renderParticleID;
		RenderableWireframeAABB m_renderAABBWire;
		RenderableNamedEntity m_renderName;

		Callback m_transformChanged;
		Callback m_evaluateTransform;

		void particleChanged (const std::string& value)
		{
			m_particle = GlobalParticleSystem().getParticle(value);
		}
		typedef MemberCaller1<MiscParticle, const std::string&, &MiscParticle::particleChanged> ParticleChangedCaller;

		void construct ()
		{
			read_aabb(m_aabb_local, m_entity.getEntityClass());

			m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
			m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
			m_keyObservers.insert("particle", ParticleChangedCaller(*this));
		}

		void updateTransform ()
		{
			m_transform.localToParent() = Matrix4::getIdentity();
			m_transform.localToParent().translateBy(m_origin);
			m_transformChanged();
		}
		typedef MemberCaller<MiscParticle, &MiscParticle::updateTransform> UpdateTransformCaller;
		void originChanged ()
		{
			m_origin = m_originKey.m_origin;
			updateTransform();
		}
		typedef MemberCaller<MiscParticle, &MiscParticle::originChanged> OriginChangedCaller;

	public:

		MiscParticle (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
				const Callback& evaluateTransform) :
			m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY),
					m_named(m_entity), m_nameKeys(m_entity), m_particle(NULL),
					m_id_origin(g_vector3_identity), m_renderAABBSolid(m_aabb_local), m_renderParticle(m_particle),
					m_renderParticleID(m_particle, m_id_origin), m_renderAABBWire(m_aabb_local), m_renderName(m_named,
							g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
							evaluateTransform)
		{
			construct();
		}
		MiscParticle (const MiscParticle& other, scene::Node& node, const Callback& transformChanged,
				const Callback& evaluateTransform) :
			m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY),
					m_named(m_entity), m_nameKeys(m_entity), m_particle(NULL), m_id_origin(
					g_vector3_identity), m_renderAABBSolid(m_aabb_local), m_renderParticle(m_particle),
					m_renderParticleID(m_particle, m_id_origin), m_renderAABBWire(m_aabb_local), m_renderName(m_named,
							g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
							evaluateTransform)
		{
			construct();
		}

		InstanceCounter m_instanceCounter;
		void instanceAttach (const scene::Path& path)
		{
			if (++m_instanceCounter.m_count == 1) {
				m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
				m_entity.attach(m_keyObservers);
			}
		}
		void instanceDetach (const scene::Path& path)
		{
			if (--m_instanceCounter.m_count == 0) {
				m_entity.detach(m_keyObservers);
				m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
			}
		}

		EntityKeyValues& getEntity ()
		{
			return m_entity;
		}
		const EntityKeyValues& getEntity () const
		{
			return m_entity;
		}

		Namespaced& getNamespaced ()
		{
			return m_nameKeys;
		}
		NamedEntity& getNameable ()
		{
			return m_named;
		}
		const NamedEntity& getNameable () const
		{
			return m_named;
		}
		TransformNode& getTransformNode ()
		{
			return m_transform;
		}
		const TransformNode& getTransformNode () const
		{
			return m_transform;
		}

		const AABB& localAABB () const
		{
			return m_aabb_local;
		}

		VolumeIntersectionValue intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			return volume.TestAABB(localAABB(), localToWorld);
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			renderer.SetState(m_entity.getEntityClass().m_state_fill, Renderer::eFullMaterials);
			if (m_particle == NULL || !m_particle->getImage().empty())
				renderer.addRenderable(m_renderParticle, localToWorld);
			else
				renderer.addRenderable(m_renderAABBSolid, localToWorld);
		}
		void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
			renderer.addRenderable(m_renderAABBWire, localToWorld);
			if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
				renderer.addRenderable(m_renderName, localToWorld);
				m_id_origin = Vector3(-10, -10, -10);
				renderer.addRenderable(m_renderParticleID, localToWorld);
			}
		}

		void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
		{
			test.BeginMesh(localToWorld);

			SelectionIntersection best;
			aabb_testselect(m_aabb_local, test, best);
			if (best.valid()) {
				selector.addIntersection(best);
			}
		}

		void translate (const Vector3& translation)
		{
			m_origin = origin_translated(m_origin, translation);
		}
		void rotate (const Quaternion& rotation)
		{
		}
		void snapto (float snap)
		{
			m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
			m_originKey.write(&m_entity);
		}
		void revertTransform ()
		{
			m_origin = m_originKey.m_origin;
		}
		void freezeTransform ()
		{
			m_originKey.m_origin = m_origin;
			m_originKey.write(&m_entity);
		}
		void transformChanged ()
		{
			revertTransform();
			m_evaluateTransform();
			updateTransform();
		}
		typedef MemberCaller<MiscParticle, &MiscParticle::transformChanged> TransformChangedCaller;
};

class MiscParticleInstance: public TargetableInstance,
		public TransformModifier,
		public Renderable,
		public SelectionTestable,
		public Bounded,
		public Cullable
{
		MiscParticle& m_contained;
		mutable AABB m_bounds;
	public:

		// Bounded implementation
		const AABB& localAABB() const
		{
			return m_contained.localAABB();
		}

		// Cullable implementation
		VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const
		{
			return m_contained.intersectVolume(test, localToWorld);
		}

		STRING_CONSTANT(Name, "MiscParticleInstance");

		MiscParticleInstance (const scene::Path& path, scene::Instance* parent, MiscParticle& contained) :
			TargetableInstance(path, parent, contained.getEntity(), *this),
					TransformModifier(MiscParticle::TransformChangedCaller(contained), ApplyTransformCaller(*this)),
					m_contained(contained)
		{
			m_contained.instanceAttach(Instance::path());

			StaticRenderableConnectionLines::instance().attach(*this);
		}
		~MiscParticleInstance ()
		{
			StaticRenderableConnectionLines::instance().detach(*this);

			m_contained.instanceDetach(Instance::path());
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume) const
		{
			m_contained.renderSolid(renderer, volume, Instance::localToWorld());
		}
		void renderWireframe (Renderer& renderer, const VolumeTest& volume) const
		{
			m_contained.renderWireframe(renderer, volume, Instance::localToWorld());
		}

		void testSelect (Selector& selector, SelectionTest& test)
		{
			m_contained.testSelect(selector, test, Instance::localToWorld());
		}

		void evaluateTransform ()
		{
			if (getType() == TRANSFORM_PRIMITIVE) {
				m_contained.translate(getTranslation());
				m_contained.rotate(getRotation());
			}
		}
		void applyTransform ()
		{
			m_contained.revertTransform();
			evaluateTransform();
			m_contained.freezeTransform();
		}
		typedef MemberCaller<MiscParticleInstance, &MiscParticleInstance::applyTransform> ApplyTransformCaller;
};

class MiscParticleNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public Nameable,
		public Snappable,
		public TransformNode,
		public EntityNode,
		public Namespaced
{
		InstanceSet m_instances;
		MiscParticle m_contained;

	public:

		// Nameable implementation
		std::string name () const
		{
			return m_contained.getNameable().name();
		}

		// Snappable implementation
		void snapto(float snap) {
			m_contained.snapto(snap);
		}

		// TransformNode implementation
		const Matrix4& localToParent() const {
			return m_contained.getTransformNode().localToParent();
		}

		// EntityNode implementation
		Entity& getEntity() {
			return m_contained.getEntity();
		}

		// Namespaced implementation
		void setNamespace(INamespace& space)
		{
			m_contained.getNamespaced().setNamespace(space);
		}

		MiscParticleNode (EntityClass* eclass) :
			m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances),
						InstanceSetEvaluateTransform<MiscParticleInstance>::Caller(m_instances))
		{
		}
		MiscParticleNode (const MiscParticleNode& other) :
			scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), Nameable(other), Snappable(
						other), TransformNode(other), EntityNode(other), Namespaced(other), m_contained(
						other.m_contained, *this, InstanceSet::TransformChangedCaller(m_instances),
						InstanceSetEvaluateTransform<MiscParticleInstance>::Caller(m_instances))
		{
		}

		scene::Node& clone () const
		{
			return *(new MiscParticleNode(*this));
		}

		scene::Instance* create (const scene::Path& path, scene::Instance* parent)
		{
			return new MiscParticleInstance(path, parent, m_contained);
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

		void attach (const NameCallback& callback)
		{
			m_contained.getNameable().attach(callback);
		}

		void detach (const NameCallback& callback)
		{
			m_contained.getNameable().detach(callback);
		}
};

scene::Node& New_MiscParticle (EntityClass* eclass)
{
	return *(new MiscParticleNode(eclass));
}
