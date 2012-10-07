/**
 * @file
 * @brief Represents any light entity (e.g. light).
 * This entity displays a special 'light' model.
 * The "origin" key directly controls the position of the light model in local space.
 * The "_color" key controls the colour of the light model.
 * The "light" key is visualised with a sphere representing the approximate coverage of the light
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

#include "light.h"

#include <stdlib.h>

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "iregistry.h"

#include "math/frustum.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "stringio.h"
#include "traverselib.h"

#include "../targetable.h"
#include "../keys/OriginKey.h"
#include "../keys/ColourKey.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

#include "../EntitySettings.h"
#include "../EntityCreator.h"
#include "RenderLightRadiusBox.h"
#include "RenderLightRadiusFill.h"
#include "RenderLightRadiusWire.h"

static void light_vertices (const AABB& aabb_light, Vector3 points[6])
{
	Vector3 max(aabb_light.origin + aabb_light.extents);
	Vector3 min(aabb_light.origin - aabb_light.extents);
	Vector3 mid(aabb_light.origin);

	// top, bottom, tleft, tright, bright, bleft
	points[0] = Vector3(mid[0], mid[1], max[2]);
	points[1] = Vector3(mid[0], mid[1], min[2]);
	points[2] = Vector3(min[0], max[1], mid[2]);
	points[3] = Vector3(max[0], max[1], mid[2]);
	points[4] = Vector3(max[0], min[1], mid[2]);
	points[5] = Vector3(min[0], min[1], mid[2]);
}

static void light_draw (const AABB& aabb_light, RenderStateFlags state)
{
	Vector3 points[6];
	light_vertices(aabb_light, points);

	if (state & RENDER_LIGHTING) {
		const float f = 0.70710678f;
		// North, East, South, West
		const Vector3 normals[8] = { Vector3(0, f, f), Vector3(f, 0, f), Vector3(0, -f, f), Vector3(-f, 0, f), Vector3(
				0, f, -f), Vector3(f, 0, -f), Vector3(0, -f, -f), Vector3(-f, 0, -f), };

		glBegin(GL_TRIANGLES);

		glVertex3fv(points[0]);
		glVertex3fv(points[2]);
		glNormal3fv(normals[0]);
		glVertex3fv(points[3]);

		glVertex3fv(points[0]);
		glVertex3fv(points[3]);
		glNormal3fv(normals[1]);
		glVertex3fv(points[4]);

		glVertex3fv(points[0]);
		glVertex3fv(points[4]);
		glNormal3fv(normals[2]);
		glVertex3fv(points[5]);
		glVertex3fv(points[0]);
		glVertex3fv(points[5]);

		glNormal3fv(normals[3]);
		glVertex3fv(points[2]);
		glEnd();
		glBegin(GL_TRIANGLE_FAN);

		glVertex3fv(points[1]);
		glVertex3fv(points[2]);
		glNormal3fv(normals[7]);
		glVertex3fv(points[5]);

		glVertex3fv(points[1]);
		glVertex3fv(points[5]);
		glNormal3fv(normals[6]);
		glVertex3fv(points[4]);

		glVertex3fv(points[1]);
		glVertex3fv(points[4]);
		glNormal3fv(normals[5]);
		glVertex3fv(points[3]);

		glVertex3fv(points[1]);
		glVertex3fv(points[3]);
		glNormal3fv(normals[4]);
		glVertex3fv(points[2]);

		glEnd();
	} else {
		const unsigned int indices[] = { 0, 2, 3, 0, 3, 4, 0, 4, 5, 0, 5, 2, 1, 2, 5, 1, 5, 4, 1, 4, 3, 1, 3, 2 };
		glVertexPointer(3, GL_FLOAT, 0, points);
		glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(indices[0]), RenderIndexTypeID, indices);
	}
}

class Light: public OpenGLRenderable, public Cullable, public Bounded, public Editable, public Snappable
{
		EntityKeyValues m_entity;
		KeyObserverMap m_keyObservers;
		TraversableNodeSet m_traverse;
		IdentityTransform m_transform;

		OriginKey m_originKey;
		ColourKey m_colour;

		NamedEntity m_named;
		NameKeys m_nameKeys;
		TraversableObserverPairRelay m_traverseObservers;

		RenderLightRadiusWire m_radii_wire;
		RenderLightRadiusFill m_radii_fill;
		RenderLightRadiusBox m_radii_box;
		RenderableNamedEntity m_renderName;

		Vector3 m_lightOrigin;

		Vector3 m_lightTarget;
		bool m_useLightTarget;

		AABB m_aabb_light;

		Callback m_transformChanged;
		Callback m_boundsChanged;
		Callback m_evaluateTransform;

		void construct ()
		{
			m_aabb_light.origin = Vector3(0, 0, 0);
			m_aabb_light.extents = Vector3(8, 8, 8);

			m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
			m_keyObservers.insert("_color", ColourKey::ColourChangedCaller(m_colour));
			m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
			m_keyObservers.insert("light", SphereRenderable::RadiusChangedCaller(m_radii_wire));
			m_keyObservers.insert("light", SphereRenderable::RadiusChangedCaller(m_radii_fill));
		}
		void destroy ()
		{
		}

		void updateOrigin ()
		{
			m_boundsChanged();

			GlobalSelectionSystem().pivotChanged();
		}

		void originChanged ()
		{
			m_aabb_light.origin = m_originKey.m_origin;
			updateOrigin();
		}
		typedef MemberCaller<Light, &Light::originChanged> OriginChangedCaller;

	public:

		Light (EntityClass* eclass, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged,
				const Callback& evaluateTransform) :
			m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_colour(Callback()),
					m_named(m_entity), m_nameKeys(m_entity), m_radii_wire(m_aabb_light.origin), m_radii_fill(
							m_aabb_light.origin), m_radii_box(m_aabb_light.origin), m_renderName(m_named,
							m_aabb_light.origin), m_transformChanged(transformChanged), m_boundsChanged(boundsChanged),
					m_evaluateTransform(evaluateTransform)
		{
			construct();
		}
		Light (const Light& other, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged,
				const Callback& evaluateTransform) :
			m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_colour(Callback()),
					m_named(m_entity), m_nameKeys(m_entity), m_radii_wire(m_aabb_light.origin),
					m_radii_fill(m_aabb_light.origin), m_radii_box(m_aabb_light.origin), m_renderName(m_named,
							m_aabb_light.origin), m_transformChanged(transformChanged), m_boundsChanged(boundsChanged),
					m_evaluateTransform(evaluateTransform)
		{
			construct();
		}
		~Light ()
		{
			destroy();
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

		scene::Traversable& getTraversable ()
		{
			return m_traverse;
		}
		const scene::Traversable& getTraversable () const
		{
			return m_traverse;
		}

		Namespaced& getNamespaced ()
		{
			return m_nameKeys;
		}
		const NamedEntity& getNameable () const
		{
			return m_named;
		}
		NamedEntity& getNameable ()
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
		void attach (scene::Traversable::Observer* observer)
		{
			m_traverseObservers.attach(*observer);
		}
		void detach (scene::Traversable::Observer* observer)
		{
			m_traverseObservers.detach(*observer);
		}

		void render (RenderStateFlags state) const
		{
			light_draw(m_aabb_light, state);
		}

		VolumeIntersectionValue intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			return volume.TestAABB(m_aabb_light, localToWorld);
		}

		// cache
		const AABB& localAABB () const
		{
			return m_aabb_light;
		}

		mutable Matrix4 m_projectionOrientation;

		void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const
		{
			renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
			renderer.SetState(m_colour.state(), Renderer::eFullMaterials);
			renderer.addRenderable(*this, localToWorld);

			if ((entity::EntitySettings::Instance().showAllLightRadii() || (selected
					&& entity::EntitySettings::Instance().showSelectedLightRadii()))
					&& m_entity.getKeyValue("target").empty()) {
				if (renderer.getStyle() == Renderer::eFullMaterials) {
					renderer.SetState(m_radii_fill, Renderer::eFullMaterials);
					renderer.Highlight(Renderer::ePrimitive, false);
					renderer.addRenderable(m_radii_fill, localToWorld);
				} else {
					renderer.addRenderable(m_radii_wire, localToWorld);
				}
			}

			renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eFullMaterials);
		}
		void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const
		{
			renderSolid(renderer, volume, localToWorld, selected);
			if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
				renderer.addRenderable(m_renderName, localToWorld);
			}
		}

		void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
		{
			test.BeginMesh(localToWorld);

			SelectionIntersection best;
			aabb_testselect(m_aabb_light, test, best);
			if (best.valid()) {
				selector.addIntersection(best);
			}
		}

		void translate (const Vector3& translation)
		{
			m_aabb_light.origin = origin_translated(m_aabb_light.origin, translation);
		}
		void rotate (const Quaternion& rotation)
		{
		}
		void snapto (float snap)
		{
			m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
			m_originKey.write(&m_entity);
		}
		void setLightRadius (const AABB& aabb)
		{
			m_aabb_light.origin = aabb.origin;
		}
		void transformLightRadius (const Matrix4& transform)
		{
			matrix4_transform_point(transform, m_aabb_light.origin);
		}
		void revertTransform ()
		{
			m_aabb_light.origin = m_originKey.m_origin;
		}
		void freezeTransform ()
		{
			m_originKey.m_origin = m_aabb_light.origin;
			m_originKey.write(&m_entity);
		}
		void transformChanged ()
		{
			revertTransform();
			m_evaluateTransform();
			updateOrigin();
		}
		typedef MemberCaller<Light, &Light::transformChanged> TransformChangedCaller;

		mutable Matrix4 m_localPivot;
		const Matrix4& getLocalPivot () const
		{
			m_localPivot = Matrix4::getIdentity();
			m_localPivot.t().getVector3() = m_aabb_light.origin;
			return m_localPivot;
		}

		const Vector3& colour () const
		{
			return m_colour.m_colour;
		}

		void projectionChanged ()
		{
			SceneChangeNotify();
		}
		const AABB& aabb () const
		{
			return m_aabb_light;
		}
};

class LightInstance: public TargetableInstance,
		public TransformModifier,
		public Renderable,
		public SelectionTestable,
		public Bounded
{
		Light& m_contained;
	public:
		const AABB& localAABB() const
		{
			return m_contained.localAABB();
		}

		STRING_CONSTANT(Name, "LightInstance");

		LightInstance (const scene::Path& path, scene::Instance* parent, Light& contained) :
			TargetableInstance(path, parent, contained.getEntity(), *this),
					TransformModifier(Light::TransformChangedCaller(contained), ApplyTransformCaller(*this)),
					m_contained(contained)
		{
			m_contained.instanceAttach(Instance::path());

			StaticRenderableConnectionLines::instance().attach(*this);
		}
		~LightInstance ()
		{
			StaticRenderableConnectionLines::instance().detach(*this);

			m_contained.instanceDetach(Instance::path());
		}
		void renderSolid (Renderer& renderer, const VolumeTest& volume) const
		{
			m_contained.renderSolid(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
		}
		void renderWireframe (Renderer& renderer, const VolumeTest& volume) const
		{
			m_contained.renderWireframe(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
		}
		void testSelect (Selector& selector, SelectionTest& test)
		{
			m_contained.testSelect(selector, test, Instance::localToWorld());
		}

		void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
		{
			m_contained.testSelect(selector, test, localToWorld);
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
		typedef MemberCaller<LightInstance, &LightInstance::applyTransform> ApplyTransformCaller;

		const Vector3& colour () const
		{
			return m_contained.colour();
		}
};

class LightNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public scene::Traversable::Observer,
		public Nameable,
		public Snappable,
		public Editable,
		public TransformNode,
		public scene::Traversable,
		public EntityNode,
		public Namespaced
{
		InstanceSet m_instances;
		Light m_contained;

		void construct ()
		{
		}
		void destroy ()
		{
		}
	public:

		// Editable implementation
		const Matrix4& getLocalPivot() const {
			return m_contained.getLocalPivot();
		}

		// Snappable implementation
		void snapto(float snap) {
			m_contained.snapto(snap);
		}

		// TransformNode implementation
		const Matrix4& localToParent() const {
			return m_contained.getTransformNode().localToParent();
		}

		// Traversable implementation
		void insert(Node& node) {
			m_contained.getTraversable().insert(node);
		}

		void erase(Node& node) {
			m_contained.getTraversable().erase(node);
		}

		void traverse(const Walker& walker) {
			m_contained.getTraversable().traverse(walker);
		}

		bool empty() const {
			return m_contained.getTraversable().empty();
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

		LightNode (EntityClass* eclass) :
			m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances),
					InstanceSet::BoundsChangedCaller(m_instances), InstanceSetEvaluateTransform<LightInstance>::Caller(
							m_instances))
		{
			construct();
		}
		LightNode (const LightNode& other) :
			scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), scene::Traversable::Observer(
						other), Nameable(other), Snappable(other), Editable(other), TransformNode(other), scene::Traversable(
						other), EntityNode(other), Namespaced(other), m_contained(other.m_contained, *this,
						InstanceSet::TransformChangedCaller(m_instances), InstanceSet::BoundsChangedCaller(m_instances),
						InstanceSetEvaluateTransform<LightInstance>::Caller(m_instances))
		{
			construct();
		}
		~LightNode ()
		{
			destroy();
		}

		scene::Node& clone () const
		{
			return *(new LightNode(*this));
		}

		void insertChild (scene::Node& child)
		{
			m_instances.insertChild(child);
		}
		void eraseChild (scene::Node& child)
		{
			m_instances.eraseChild(child);
		}

		scene::Instance* create (const scene::Path& path, scene::Instance* parent)
		{
			return new LightInstance(path, parent, m_contained);
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
		// Nameable implementation
		std::string name () const
		{
			return m_contained.getNameable().name();
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

scene::Node& New_Light (EntityClass* eclass)
{
	return *(new LightNode(eclass));
}
