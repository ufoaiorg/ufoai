/**
 * @file group.cpp
 * @brief Represents any entity which does not have a fixed size specified in its entity-definition (except misc_model).
 * This entity behaves as a group, i.e. it contains brushes.
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

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "iregistry.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"

#include "targetable.h"
#include "keys/OriginKey.h"
#include "keys/AnglesKey.h"
#include "keys/ScaleKey.h"
#include "namedentity.h"
#include "keys/KeyObserverMap.h"
#include "NameKeys.h"
#include "nameable.h"

#include "EntityCreator.h"

class Group
{
		EntityKeyValues m_entity;
		KeyObserverMap m_keyObservers;
		MatrixTransform m_transform;
		TraversableNodeSet m_traverse;

		NamedEntity m_named;
		NameKeys m_nameKeys;

		RenderableNamedEntity m_renderName;
		mutable Vector3 m_name_origin;

		Callback m_transformChanged;

		void construct ()
		{
			m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
		}

	public:
		Group (EntityClass* eclass, scene::Node& node, const Callback& transformChanged) :
			m_entity(eclass), m_named(m_entity), m_nameKeys(m_entity), m_renderName(m_named,
					m_name_origin), m_name_origin(g_vector3_identity), m_transformChanged(transformChanged)
		{
			construct();
		}

		Group (const Group& other, scene::Node& node, const Callback& transformChanged) :
			m_entity(other.m_entity), m_named(m_entity), m_nameKeys(m_entity), m_renderName(
					m_named, g_vector3_identity), m_transformChanged(transformChanged)
		{
			construct();
		}

		InstanceCounter m_instanceCounter;
		void instanceAttach (const scene::Path& path)
		{
			if (++m_instanceCounter.m_count == 1) {
				m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
				m_traverse.instanceAttach(path_find_mapfile(path.begin(), path.end()));
				m_entity.attach(m_keyObservers);
			}
		}

		void instanceDetach (const scene::Path& path)
		{
			if (--m_instanceCounter.m_count == 0) {
				m_entity.detach(m_keyObservers);
				m_traverse.instanceDetach(path_find_mapfile(path.begin(), path.end()));
				m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
			}
		}

		void snapto (float snap) {
			// TODO:mattn
			//m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
			//m_originKey.write(&_entity);
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

		const scene::Traversable& getTraversable() const
		{
			return m_traverse;
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

		const TransformNode& getTransformNode() const {
			return m_transform;
		}

		TransformNode& getTransformNode ()
		{
			return m_transform;
		}

		void attach (scene::Traversable::Observer* observer)
		{
			m_traverse.attach(observer);
		}

		void detach (scene::Traversable::Observer* observer)
		{
			m_traverse.detach(observer);
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
		{
			renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
		}

		void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
				const AABB& childBounds) const
		{
			renderSolid(renderer, volume, localToWorld);

			if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
				// don't draw the name for worldspawn
				if (m_entity.getEntityClass().name() == "worldspawn")
					return;

				// place name in the middle of the "children cloud"
				m_name_origin = childBounds.origin;

				renderer.addRenderable(m_renderName, localToWorld);
			}
		}
};

class GroupInstance: public TargetableInstance, public Renderable
{
		Group& m_contained;
	public:
		GroupInstance (const scene::Path& path, scene::Instance* parent, Group& group) :
			TargetableInstance(path, parent, group.getEntity(), *this),
					m_contained(group)
		{
			m_contained.instanceAttach(Instance::path());
			StaticRenderableConnectionLines::instance().attach(*this);
		}

		~GroupInstance ()
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
			m_contained.renderWireframe(renderer, volume, Instance::localToWorld(), Instance::childBounds());
		}
};

class GroupNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public scene::Traversable::Observer,
		public Nameable,
		public Snappable,
		public TransformNode,
		public scene::Traversable,
		public EntityNode,
		public Namespaced
{
		InstanceSet m_instances;
		Group m_contained;

		void construct ()
		{
			m_contained.attach(this);
		}
		void destroy ()
		{
			m_contained.detach(this);
		}

	public:

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
		void setNamespace(INamespace& space) {
			m_contained.getNamespaced().setNamespace(space);
		}

		GroupNode (EntityClass* eclass) :
			m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances))
		{
			construct();
		}
		GroupNode (const GroupNode& other) :
			scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), scene::Traversable::Observer(
						other), Nameable(other), Snappable(other), TransformNode(other), scene::Traversable(other), EntityNode(
						other), Namespaced(other), m_contained(other.m_contained, *this,
						InstanceSet::TransformChangedCaller(m_instances))
		{
			construct();
		}
		~GroupNode ()
		{
			destroy();
		}

		scene::Node& clone () const
		{
			return *(new GroupNode(*this));
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
			return new GroupInstance(path, parent, m_contained);
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

scene::Node& New_Group (EntityClass* eclass)
{
	return *(new GroupNode(eclass));
}
