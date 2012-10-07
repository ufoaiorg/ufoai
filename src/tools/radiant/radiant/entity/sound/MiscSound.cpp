/**
 * @file
 */

/*
 This file is part of UFORadiant.

 UFORadiant is free software; you can redistribute it and/or modify
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

#include "MiscSound.h"

#include "iregistry.h"

#include "math/frustum.h"
#include "selectionlib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"

MiscSound::MiscSound (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_angleKey(
				AngleChangedCaller(*this)), m_angle(ANGLEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_arrow(
				m_ray), m_renderAABBSolid(m_aabb_local), m_renderAABBWire(m_aabb_local), m_renderName(m_named,
				g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(evaluateTransform)
{
	construct();
}

MiscSound::MiscSound (const MiscSound& other, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_angleKey(
				AngleChangedCaller(*this)), m_angle(ANGLEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_arrow(
				m_ray), m_renderAABBSolid(m_aabb_local), m_renderAABBWire(m_aabb_local), m_renderName(m_named,
				g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(evaluateTransform)
{
	construct();
}

void MiscSound::MiscSound::construct ()
{
	m_aabb_local = AABB::createFromMinMax(m_entity.getEntityClass().mins, m_entity.getEntityClass().maxs);
	m_ray.origin = m_aabb_local.origin;
	m_ray.direction[0] = 1;
	m_ray.direction[1] = 0;
	m_ray.direction[2] = 0;

	m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
	m_keyObservers.insert("angle", AngleKey::AngleChangedCaller(m_angleKey));
	m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
}

void MiscSound::updateTransform ()
{
	m_transform.localToParent() = Matrix4::getIdentity();
	m_transform.localToParent().translateBy(m_origin);
	m_ray.direction = matrix4_transformed_direction(matrix4_rotation_for_z(degrees_to_radians(m_angle)),
			Vector3(1, 0, 0));
	m_transformChanged();
}

void MiscSound::originChanged ()
{
	m_origin = m_originKey.m_origin;
	updateTransform();
}

void MiscSound::angleChanged ()
{
	m_angle = m_angleKey.m_angle;
	updateTransform();
}

void MiscSound::instanceAttach (const scene::Path& path)
{
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_entity.attach(m_keyObservers);
	}
}

void MiscSound::instanceDetach (const scene::Path& path)
{
	if (--m_instanceCounter.m_count == 0) {
		m_entity.detach(m_keyObservers);
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

EntityKeyValues& MiscSound::getEntity ()
{
	return m_entity;
}

const EntityKeyValues& MiscSound::getEntity () const
{
	return m_entity;
}

Namespaced& MiscSound::getNamespaced ()
{
	return m_nameKeys;
}

NamedEntity& MiscSound::getNameable ()
{
	return m_named;
}

const NamedEntity& MiscSound::getNameable () const
{
	return m_named;
}

TransformNode& MiscSound::getTransformNode ()
{
	return m_transform;
}

const TransformNode& MiscSound::getTransformNode () const
{
	return m_transform;
}

const AABB& MiscSound::localAABB () const
{
	return m_aabb_local;
}

VolumeIntersectionValue MiscSound::intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
{
	return volume.TestAABB(localAABB(), localToWorld);
}

void MiscSound::renderArrow (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	if (GlobalRegistry().get("user/ui/xyview/showEntityAngles") == "1") {
		renderer.addRenderable(m_arrow, localToWorld);
	}
}

void MiscSound::renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	renderer.SetState(m_entity.getEntityClass().m_state_fill, Renderer::eFullMaterials);
	renderer.addRenderable(m_renderAABBSolid, localToWorld);
	renderArrow(renderer, volume, localToWorld);
}

void MiscSound::renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
	renderer.addRenderable(m_renderAABBWire, localToWorld);
	renderArrow(renderer, volume, localToWorld);
	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		renderer.addRenderable(m_renderName, localToWorld);
	}
}

void MiscSound::testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
{
	test.BeginMesh(localToWorld);

	SelectionIntersection best;
	aabb_testselect(m_aabb_local, test, best);
	if (best.valid()) {
		selector.addIntersection(best);
	}
}

void MiscSound::translate (const Vector3& translation)
{
	m_origin = origin_translated(m_origin, translation);
}

void MiscSound::rotate (const Quaternion& rotation)
{
	m_angle = angle_rotated(m_angle, rotation);
}

void MiscSound::snapto (float snap)
{
	m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
	m_originKey.write(&m_entity);
}

void MiscSound::revertTransform ()
{
	m_origin = m_originKey.m_origin;
	m_angle = m_angleKey.m_angle;
}

void MiscSound::freezeTransform ()
{
	m_originKey.m_origin = m_origin;
	m_originKey.write(&m_entity);
	m_angleKey.m_angle = m_angle;
	m_angleKey.write(&m_entity);
}

void MiscSound::transformChanged ()
{
	revertTransform();
	m_evaluateTransform();
	updateTransform();
}
