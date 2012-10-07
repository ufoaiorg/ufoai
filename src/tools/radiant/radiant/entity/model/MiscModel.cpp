/**
 * @file
 * @brief Represents the misc_model entity.
 * This entity displays the model specified in its "model" key.
 * The "origin", "angles" and "modelscale_vec" keys directly control the entity's local-to-parent transform.
 */

#include "MiscModel.h"

MiscModel::MiscModel (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_anglesKey(
				AnglesChangedCaller(*this)), m_angles(ANGLESKEY_IDENTITY), m_scaleKey(ScaleChangedCaller(*this)), m_scale(
				SCALEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_renderName(m_named, g_vector3_identity), m_transformChanged(
				transformChanged), m_evaluateTransform(evaluateTransform)
{
	construct();
}

MiscModel::MiscModel (const MiscModel& other, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_anglesKey(
				AnglesChangedCaller(*this)), m_angles(ANGLESKEY_IDENTITY), m_scaleKey(ScaleChangedCaller(*this)), m_scale(
				SCALEKEY_IDENTITY), m_named(m_entity), m_nameKeys(m_entity), m_renderName(m_named, g_vector3_identity), m_transformChanged(
				transformChanged), m_evaluateTransform(evaluateTransform)
{
	construct();
}

void MiscModel::construct ()
{
	m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
	m_keyObservers.insert("model", SingletonModel::ModelChangedCaller(m_model));
	m_keyObservers.insert("skin", SingletonModel::SkinChangedCaller(m_model));
	m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
	m_keyObservers.insert("angle", AnglesKey::AngleChangedCaller(m_anglesKey));
	m_keyObservers.insert("angles", AnglesKey::AnglesChangedCaller(m_anglesKey));
	m_keyObservers.insert("modelscale_vec", ScaleKey::ScaleChangedCaller(m_scaleKey));
}

void MiscModel::updateTransform ()
{
	m_transform.localToParent() = Matrix4::getIdentity();
	matrix4_transform_by_euler_xyz_degrees(m_transform.localToParent(), m_origin, m_angles, m_scale);
	m_transformChanged();
}

void MiscModel::originChanged ()
{
	m_origin = m_originKey.m_origin;
	updateTransform();
}

void MiscModel::anglesChanged ()
{
	m_angles = m_anglesKey.m_angles;
	updateTransform();
}

void MiscModel::scaleChanged ()
{
	m_scale = m_scaleKey.m_scale;
	updateTransform();
}

void MiscModel::instanceAttach (const scene::Path& path)
{
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_entity.attach(m_keyObservers);
	}
}

void MiscModel::instanceDetach (const scene::Path& path)
{
	if (--m_instanceCounter.m_count == 0) {
		m_entity.detach(m_keyObservers);
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

EntityKeyValues& MiscModel::getEntity ()
{
	return m_entity;
}

const EntityKeyValues& MiscModel::getEntity () const
{
	return m_entity;
}

scene::Traversable& MiscModel::getTraversable ()
{
	return m_model.getTraversable();
}

const scene::Traversable& MiscModel::getTraversable () const
{
	return m_model.getTraversable();
}

Namespaced& MiscModel::getNamespaced ()
{
	return m_nameKeys;
}

NamedEntity& MiscModel::getNameable ()
{
	return m_named;
}

const NamedEntity& MiscModel::getNameable () const
{
	return m_named;
}

TransformNode& MiscModel::getTransformNode ()
{
	return m_transform;
}

const TransformNode& MiscModel::getTransformNode () const
{
	return m_transform;
}

void MiscModel::attach (scene::Traversable::Observer* observer)
{
	m_model.attach(observer);
}

void MiscModel::detach (scene::Traversable::Observer* observer)
{
	m_model.detach(observer);
}

void MiscModel::renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
		bool selected) const
{
	if (selected) {
		m_renderOrigin.render(renderer, volume, localToWorld);
	}

	ASSERT_NOTNULL(m_entity.getEntityClass().m_state_wire);
	renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
}

void MiscModel::renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
		bool selected) const
{
	renderSolid(renderer, volume, localToWorld, selected);
	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		renderer.addRenderable(m_renderName, localToWorld);
	}
}

void MiscModel::translate (const Vector3& translation)
{
	m_origin = origin_translated(m_origin, translation);
}

void MiscModel::rotate (const Quaternion& rotation)
{
	m_angles = angles_rotated(m_angles, rotation);
}

void MiscModel::scale (const Vector3& scaling)
{
	m_scale = scale_scaled(m_scale, scaling);
}

void MiscModel::snapto (float snap)
{
	m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
	m_originKey.write(&m_entity);
}

void MiscModel::revertTransform ()
{
	m_origin = m_originKey.m_origin;
	m_angles = m_anglesKey.m_angles;
	m_scale = m_scaleKey.m_scale;
}

void MiscModel::freezeTransform ()
{
	m_originKey.m_origin = m_origin;
	m_originKey.write(&m_entity);
	m_anglesKey.m_angles = m_angles;
	m_anglesKey.write(&m_entity);
	m_scaleKey.m_scale = m_scale;
	m_scaleKey.write(&m_entity);
}

void MiscModel::transformChanged ()
{
	revertTransform();
	m_evaluateTransform();
	updateTransform();
}
