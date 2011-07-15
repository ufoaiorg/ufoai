/**
 * @file miscmodel.cpp
 * @brief Represents the misc_model entity.
 * This entity displays the model specified in its "model" key.
 * The "origin", "angles" and "modelscale_vec" keys directly control the entity's local-to-parent transform.
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
#include "eclasslib.h"
#include "render.h"
#include "pivot.h"

#include "targetable.h"
#include "keys/OriginKey.h"
#include "keys/AnglesKey.h"
#include "keys/ScaleKey.h"
#include "model.h"
#include "namedentity.h"
#include "keys/KeyObserverMap.h"
#include "NameKeys.h"

#include "EntityCreator.h"
#include "miscmodel.h"

class MiscModel: public Snappable
{
		EntityKeyValues m_entity;
		KeyObserverMap m_keyObservers;
		MatrixTransform m_transform;

		OriginKey m_originKey;
		Vector3 m_origin;
		AnglesKey m_anglesKey;
		Vector3 m_angles;
		ScaleKey m_scaleKey;
		Vector3 m_scale;

		SingletonModel m_model;

		NamedEntity m_named;
		NameKeys m_nameKeys;
		RenderablePivot m_renderOrigin;
		RenderableNamedEntity m_renderName;

		Callback m_transformChanged;
		Callback m_evaluateTransform;

		void construct ()
		{
			m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
			m_keyObservers.insert("model", SingletonModel::ModelChangedCaller(m_model));
			m_keyObservers.insert("skin", SingletonModel::SkinChangedCaller(m_model));
			m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
			m_keyObservers.insert("angle", AnglesKey::AngleChangedCaller(m_anglesKey));
			m_keyObservers.insert("angles", AnglesKey::AnglesChangedCaller(m_anglesKey));
			m_keyObservers.insert("modelscale_vec", ScaleKey::ScaleChangedCaller(m_scaleKey));
		}

		void updateTransform ()
		{
			m_transform.localToParent() = Matrix4::getIdentity();
			matrix4_transform_by_euler_xyz_degrees(m_transform.localToParent(), m_origin, m_angles, m_scale);
			m_transformChanged();
		}

		void originChanged ()
		{
			m_origin = m_originKey.m_origin;
			updateTransform();
		}
		typedef MemberCaller<MiscModel, &MiscModel::originChanged> OriginChangedCaller;

		void anglesChanged ()
		{
			m_angles = m_anglesKey.m_angles;
			updateTransform();
		}
		typedef MemberCaller<MiscModel, &MiscModel::anglesChanged> AnglesChangedCaller;

		void scaleChanged ()
		{
			m_scale = m_scaleKey.m_scale;
			updateTransform();
		}
		typedef MemberCaller<MiscModel, &MiscModel::scaleChanged> ScaleChangedCaller;

	public:

		MiscModel (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
				const Callback& evaluateTransform) :
			m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_anglesKey(
					AnglesChangedCaller(*this)), m_angles(ANGLESKEY_IDENTITY), m_scaleKey(ScaleChangedCaller(*this)),
					m_scale(SCALEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_renderName(m_named,
							g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
							evaluateTransform)
		{
			construct();
		}
		MiscModel (const MiscModel& other, scene::Node& node, const Callback& transformChanged,
				const Callback& evaluateTransform) :
			m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY),
					m_anglesKey(AnglesChangedCaller(*this)), m_angles(ANGLESKEY_IDENTITY), m_scaleKey(
							ScaleChangedCaller(*this)), m_scale(SCALEKEY_IDENTITY), m_named(m_entity), m_nameKeys(
							m_entity), m_renderName(m_named, g_vector3_identity), m_transformChanged(transformChanged),
					m_evaluateTransform(evaluateTransform)
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

		scene::Traversable& getTraversable ()
		{
			return m_model.getTraversable();
		}
		const scene::Traversable& getTraversable () const
		{
			return m_model.getTraversable();
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

		void attach (scene::Traversable::Observer* observer)
		{
			m_model.attach(observer);
		}
		void detach (scene::Traversable::Observer* observer)
		{
			m_model.detach(observer);
		}

		void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const
		{
			if (selected) {
				m_renderOrigin.render(renderer, volume, localToWorld);
			}

			ASSERT_NOTNULL(m_entity.getEntityClass().m_state_wire);
			renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
		}
		void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const
		{
			renderSolid(renderer, volume, localToWorld, selected);
			if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
				renderer.addRenderable(m_renderName, localToWorld);
			}
		}

		void translate (const Vector3& translation)
		{
			m_origin = origin_translated(m_origin, translation);
		}
		void rotate (const Quaternion& rotation)
		{
			m_angles = angles_rotated(m_angles, rotation);
		}
		void scale (const Vector3& scaling)
		{
			m_scale = scale_scaled(m_scale, scaling);
		}
		void snapto (float snap)
		{
			m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
			m_originKey.write(&m_entity);
		}
		void revertTransform ()
		{
			m_origin = m_originKey.m_origin;
			m_angles = m_anglesKey.m_angles;
			m_scale = m_scaleKey.m_scale;
		}
		void freezeTransform ()
		{
			m_originKey.m_origin = m_origin;
			m_originKey.write(&m_entity);
			m_anglesKey.m_angles = m_angles;
			m_anglesKey.write(&m_entity);
			m_scaleKey.m_scale = m_scale;
			m_scaleKey.write(&m_entity);
		}
		void transformChanged ()
		{
			revertTransform();
			m_evaluateTransform();
			updateTransform();
		}
		typedef MemberCaller<MiscModel, &MiscModel::transformChanged> TransformChangedCaller;
};

class MiscModelInstance: public TargetableInstance, public TransformModifier, public Renderable
{
		MiscModel& m_contained;
	public:

		STRING_CONSTANT(Name, "MiscModelInstance");

		MiscModelInstance (const scene::Path& path, scene::Instance* parent, MiscModel& miscmodel) :
			TargetableInstance(path, parent, miscmodel.getEntity(), *this), TransformModifier(
					MiscModel::TransformChangedCaller(miscmodel), ApplyTransformCaller(*this)), m_contained(miscmodel)
		{
			m_contained.instanceAttach(Instance::path());
			StaticRenderableConnectionLines::instance().attach(*this);
		}
		~MiscModelInstance ()
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
		void evaluateTransform ()
		{
			if (getType() == TRANSFORM_PRIMITIVE) {
				m_contained.translate(getTranslation());
				m_contained.rotate(getRotation());
				m_contained.scale(getScale());
			}
		}
		void applyTransform ()
		{
			m_contained.revertTransform();
			evaluateTransform();
			m_contained.freezeTransform();
		}
		typedef MemberCaller<MiscModelInstance, &MiscModelInstance::applyTransform> ApplyTransformCaller;
};

class MiscModelNode: public scene::Node,
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
		MiscModel m_contained;

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
		void snapto (float snap)
		{
			m_contained.snapto(snap);
		}

		// TransformNode implementation
		const Matrix4& localToParent () const
		{
			return m_contained.getTransformNode().localToParent();
		}

		// Traversable implementation
		void insert (Node& node)
		{
			m_contained.getTraversable().insert(node);
		}

		void erase (Node& node)
		{
			m_contained.getTraversable().erase(node);
		}

		void traverse (const Walker& walker)
		{
			m_contained.getTraversable().traverse(walker);
		}

		bool empty () const
		{
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

		MiscModelNode (EntityClass* eclass) :
			m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances), InstanceSetEvaluateTransform<
					MiscModelInstance>::Caller(m_instances))
		{
			construct();
		}
		MiscModelNode (const MiscModelNode& other) :
			scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), scene::Traversable::Observer(
						other), Nameable(other), Snappable(other), TransformNode(other), scene::Traversable(other), EntityNode(
						other), Namespaced(other), m_contained(other.m_contained, *this,
						InstanceSet::TransformChangedCaller(m_instances),
						InstanceSetEvaluateTransform<MiscModelInstance>::Caller(m_instances))
		{
			construct();
		}
		~MiscModelNode ()
		{
			destroy();
		}

		scene::Node& clone () const
		{
			return *(new MiscModelNode(*this));
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
			return new MiscModelInstance(path, parent, m_contained);
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

scene::Node& New_MiscModel (EntityClass* eclass)
{
	return *(new MiscModelNode(eclass));
}
