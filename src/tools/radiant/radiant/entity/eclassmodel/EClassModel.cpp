/**
 * @file
 * @brief Represents any entity which has a fixed size specified in its entity-definition and displays a model (e.g. ammo_bfg).
 * This entity displays the model specified in its entity-definition.
 * The "origin" and "angle" keys directly control the entity's local-to-parent transform.
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

#include "EClassModel.h"

#include "iregistry.h"

#include "cullable.h"
#include "renderable.h"
#include "editable.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "traverselib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "pivot.h"

#include "../targetable.h"
#include "../keys/OriginKey.h"
#include "../keys/AngleKey.h"
#include "../model.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

void EclassModel::construct ()
{
	m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
	//m_keyObservers.insert("skin", ModelSkinKey::SkinChangedCaller(m_skin));
	m_keyObservers.insert("angle", AngleKey::AngleChangedCaller(m_angleKey));
	m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
}

void EclassModel::updateTransform ()
{
	m_transform.localToParent() = Matrix4::getIdentity();
	m_transform.localToParent().translateBy(m_origin);

	matrix4_multiply_by_matrix4(m_transform.localToParent(), matrix4_rotation_for_z_degrees(m_angle));

	m_transformChanged();
}

void EclassModel::originChanged ()
{
	m_origin = m_originKey.m_origin;
	updateTransform();
}

void EclassModel::angleChanged ()
{
	m_angle = m_angleKey.m_angle;
	updateTransform();
}

EclassModel::EclassModel (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_angleKey(
				AngleChangedCaller(*this)), m_angle(ANGLEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_renderName(
				m_named, g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
				evaluateTransform)
{
	construct();
}

EclassModel::EclassModel (const EclassModel& other, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_angleKey(
				AngleChangedCaller(*this)), m_angle(ANGLEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_renderName(
				m_named, g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
				evaluateTransform)
{
	construct();
}

void EclassModel::instanceAttach (const scene::Path& path)
{
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_entity.attach(m_keyObservers);
		m_model.modelChanged(m_entity.getEntityClass().modelpath());
		//m_skin.skinChanged(m_entity.getEntityClass().skin());
	}
}

void EclassModel::instanceDetach (const scene::Path& path)
{
	if (--m_instanceCounter.m_count == 0) {
		//m_skin.skinChanged("");
		m_model.modelChanged("");
		m_entity.detach(m_keyObservers);
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

EntityKeyValues& EclassModel::getEntity ()
{
	return m_entity;
}

const EntityKeyValues& EclassModel::getEntity () const
{
	return m_entity;
}

scene::Traversable& EclassModel::getTraversable ()
{
	return m_model.getTraversable();
}

const scene::Traversable& EclassModel::getTraversable () const
{
	return m_model.getTraversable();
}

Namespaced& EclassModel::getNamespaced ()
{
	return m_nameKeys;
}

NamedEntity& EclassModel::getNameable ()
{
	return m_named;
}

const NamedEntity& EclassModel::getNameable () const
{
	return m_named;
}

TransformNode& EclassModel::getTransformNode ()
{
	return m_transform;
}

const TransformNode& EclassModel::getTransformNode () const
{
	return m_transform;
}

void EclassModel::attach (scene::Traversable::Observer* observer)
{
	m_model.attach(observer);
}

void EclassModel::detach (scene::Traversable::Observer* observer)
{
	m_model.detach(observer);
}

void EclassModel::renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
		bool selected) const
{
	if (selected) {
		m_renderOrigin.render(renderer, volume, localToWorld);
	}

	ASSERT_NOTNULL(m_entity.getEntityClass().m_state_wire);
	renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
}

void EclassModel::renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
		bool selected) const
{
	renderSolid(renderer, volume, localToWorld, selected);
	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		renderer.addRenderable(m_renderName, localToWorld);
	}
}

void EclassModel::translate (const Vector3& translation)
{
	m_origin = origin_translated(m_origin, translation);
}

void EclassModel::rotate (const Quaternion& rotation)
{
	m_angle = angle_rotated(m_angle, rotation);
}

void EclassModel::snapto (float snap)
{
	m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
	m_originKey.write(&m_entity);
}

void EclassModel::revertTransform ()
{
	m_origin = m_originKey.m_origin;
	m_angle = m_angleKey.m_angle;
}

void EclassModel::freezeTransform ()
{
	m_originKey.m_origin = m_origin;
	m_originKey.write(&m_entity);
	m_angleKey.m_angle = m_angle;
	m_angleKey.write(&m_entity);
}

void EclassModel::transformChanged ()
{
	revertTransform();
	m_evaluateTransform();
	updateTransform();
}
