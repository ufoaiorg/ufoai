/**
 * @file
 * @brief Represents the misc_particle entity.
 *
 * This entity displays the particle specified in its "particle" key.
 * The "origin" key directly controls the entity's local-to-parent transform.
 */
#include "MiscParticle.h"

#include "iregistry.h"

#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "renderable.h"

MiscParticle::MiscParticle (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_named(m_entity), m_nameKeys(
				m_entity), m_particle(NULL), m_id_origin(g_vector3_identity), m_renderAABBSolid(m_aabb_local), m_renderParticle(
				m_particle), m_renderParticleID(m_particle, m_id_origin), m_renderAABBWire(m_aabb_local), m_renderName(
				m_named, g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
				evaluateTransform)
{
	construct();
}

MiscParticle::MiscParticle (const MiscParticle& other, scene::Node& node, const Callback& transformChanged,
		const Callback& evaluateTransform) :
		m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_origin(ORIGINKEY_IDENTITY), m_named(
				m_entity), m_nameKeys(m_entity), m_particle(NULL), m_id_origin(g_vector3_identity), m_renderAABBSolid(
				m_aabb_local), m_renderParticle(m_particle), m_renderParticleID(m_particle, m_id_origin), m_renderAABBWire(
				m_aabb_local), m_renderName(m_named, g_vector3_identity), m_transformChanged(transformChanged), m_evaluateTransform(
				evaluateTransform)
{
	construct();
}

void MiscParticle::particleChanged (const std::string& value)
{
	m_particle = GlobalParticleSystem().getParticle(value);
}

void MiscParticle::construct ()
{
	m_aabb_local = AABB::createFromMinMax(m_entity.getEntityClass().mins, m_entity.getEntityClass().maxs);
	m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
	m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
	m_keyObservers.insert("particle", ParticleChangedCaller(*this));
}

void MiscParticle::updateTransform ()
{
	m_transform.localToParent() = Matrix4::getIdentity();
	m_transform.localToParent().translateBy(m_origin);
	m_transformChanged();
}

void MiscParticle::originChanged ()
{
	m_origin = m_originKey.m_origin;
	updateTransform();
}

void MiscParticle::instanceAttach (const scene::Path& path)
{
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_entity.attach(m_keyObservers);
	}
}

void MiscParticle::instanceDetach (const scene::Path& path)
{
	if (--m_instanceCounter.m_count == 0) {
		m_entity.detach(m_keyObservers);
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

EntityKeyValues& MiscParticle::getEntity ()
{
	return m_entity;
}

const EntityKeyValues& MiscParticle::getEntity () const
{
	return m_entity;
}

Namespaced& MiscParticle::getNamespaced ()
{
	return m_nameKeys;
}

NamedEntity& MiscParticle::getNameable ()
{
	return m_named;
}

const NamedEntity& MiscParticle::getNameable () const
{
	return m_named;
}

TransformNode& MiscParticle::getTransformNode ()
{
	return m_transform;
}

const TransformNode& MiscParticle::getTransformNode () const
{
	return m_transform;
}

const AABB& MiscParticle::localAABB () const
{
	return m_aabb_local;
}

VolumeIntersectionValue MiscParticle::intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
{
	return volume.TestAABB(localAABB(), localToWorld);
}

void MiscParticle::renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	renderer.SetState(m_entity.getEntityClass().m_state_fill, Renderer::eFullMaterials);
	if (m_particle == NULL || !m_particle->getImage().empty())
		renderer.addRenderable(m_renderParticle, localToWorld);
	else
		renderer.addRenderable(m_renderAABBSolid, localToWorld);
}

void MiscParticle::renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
	renderer.addRenderable(m_renderAABBWire, localToWorld);
	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		renderer.addRenderable(m_renderName, localToWorld);
		m_id_origin = Vector3(-10, -10, -10);
		renderer.addRenderable(m_renderParticleID, localToWorld);
	}
}

void MiscParticle::testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
{
	test.BeginMesh(localToWorld);

	SelectionIntersection best;
	aabb_testselect(m_aabb_local, test, best);
	if (best.valid()) {
		selector.addIntersection(best);
	}
}

void MiscParticle::translate (const Vector3& translation)
{
	m_origin = origin_translated(m_origin, translation);
}

void MiscParticle::rotate (const Quaternion& rotation)
{
}

void MiscParticle::snapto (float snap)
{
	m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
	m_originKey.write(&m_entity);
}

void MiscParticle::revertTransform ()
{
	m_origin = m_originKey.m_origin;
}

void MiscParticle::freezeTransform ()
{
	m_originKey.m_origin = m_origin;
	m_originKey.write(&m_entity);
}

void MiscParticle::transformChanged ()
{
	revertTransform();
	m_evaluateTransform();
	updateTransform();
}
