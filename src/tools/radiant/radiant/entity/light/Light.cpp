#include "Light.h"
#include "iregistry.h"
#include "../EntitySettings.h"

Light::Light (EntityClass* eclass, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged,
		const Callback& evaluateTransform) :
		m_entity(eclass), m_originKey(OriginChangedCaller(*this)), m_colour(Callback()), m_named(m_entity), m_nameKeys(
				m_entity), m_radii_wire(m_aabb_light.origin), m_radii_fill(m_aabb_light.origin), m_radii_box(
				m_aabb_light.origin), m_renderName(m_named, m_aabb_light.origin), m_transformChanged(transformChanged), m_boundsChanged(
				boundsChanged), m_evaluateTransform(evaluateTransform)
{
	construct();
}

Light::Light (const Light& other, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged,
		const Callback& evaluateTransform) :
		m_entity(other.m_entity), m_originKey(OriginChangedCaller(*this)), m_colour(Callback()), m_named(m_entity), m_nameKeys(
				m_entity), m_radii_wire(m_aabb_light.origin), m_radii_fill(m_aabb_light.origin), m_radii_box(
				m_aabb_light.origin), m_renderName(m_named, m_aabb_light.origin), m_transformChanged(transformChanged), m_boundsChanged(
				boundsChanged), m_evaluateTransform(evaluateTransform)
{
	construct();
}

Light::~Light ()
{
}

void Light::construct ()
{
	m_aabb_light.origin = Vector3(0, 0, 0);
	m_aabb_light.extents = Vector3(8, 8, 8);

	m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
	m_keyObservers.insert("_color", ColourKey::ColourChangedCaller(m_colour));
	m_keyObservers.insert("origin", OriginKey::OriginChangedCaller(m_originKey));
	m_keyObservers.insert("light", SphereRenderable::RadiusChangedCaller(m_radii_wire));
	m_keyObservers.insert("light", SphereRenderable::RadiusChangedCaller(m_radii_fill));
}

void Light::updateOrigin ()
{
	m_boundsChanged();

	GlobalSelectionSystem().pivotChanged();
}

void Light::originChanged ()
{
	m_aabb_light.origin = m_originKey.m_origin;
	updateOrigin();
}

void Light::instanceAttach (const scene::Path& path)
{
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_entity.attach(m_keyObservers);
	}
}

void Light::instanceDetach (const scene::Path& path)
{
	if (--m_instanceCounter.m_count == 0) {
		m_entity.detach(m_keyObservers);
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

EntityKeyValues& Light::getEntity ()
{
	return m_entity;
}

const EntityKeyValues& Light::getEntity () const
{
	return m_entity;
}

scene::Traversable& Light::getTraversable ()
{
	return m_traverse;
}

const scene::Traversable& Light::getTraversable () const
{
	return m_traverse;
}

Namespaced& Light::getNamespaced ()
{
	return m_nameKeys;
}

const NamedEntity& Light::getNameable () const
{
	return m_named;
}

NamedEntity& Light::getNameable ()
{
	return m_named;
}

TransformNode& Light::getTransformNode ()
{
	return m_transform;
}

const TransformNode& Light::getTransformNode () const
{
	return m_transform;
}

void Light::attach (scene::Traversable::Observer* observer)
{
	m_traverseObservers.attach(*observer);
}

void Light::detach (scene::Traversable::Observer* observer)
{
	m_traverseObservers.detach(*observer);
}

void Light::render (RenderStateFlags state) const
{
	light_draw(m_aabb_light, state);
}

VolumeIntersectionValue Light::intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const
{
	return volume.TestAABB(m_aabb_light, localToWorld);
}

const AABB& Light::localAABB () const
{
	return m_aabb_light;
}

void Light::renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const
{
	renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
	renderer.SetState(m_colour.state(), Renderer::eFullMaterials);
	renderer.addRenderable(*this, localToWorld);

	if ((entity::EntitySettings::Instance().showAllLightRadii()
			|| (selected && entity::EntitySettings::Instance().showSelectedLightRadii()))
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

void Light::renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
		bool selected) const
{
	renderSolid(renderer, volume, localToWorld, selected);
	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		renderer.addRenderable(m_renderName, localToWorld);
	}
}

void Light::testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld)
{
	test.BeginMesh(localToWorld);

	SelectionIntersection best;
	aabb_testselect(m_aabb_light, test, best);
	if (best.valid()) {
		selector.addIntersection(best);
	}
}

void Light::translate (const Vector3& translation)
{
	m_aabb_light.origin = origin_translated(m_aabb_light.origin, translation);
}

void Light::rotate (const Quaternion& rotation)
{
}

void Light::snapto (float snap)
{
	m_originKey.m_origin = origin_snapped(m_originKey.m_origin, snap);
	m_originKey.write(&m_entity);
}

void Light::setLightRadius (const AABB& aabb)
{
	m_aabb_light.origin = aabb.origin;
}

void Light::transformLightRadius (const Matrix4& transform)
{
	matrix4_transform_point(transform, m_aabb_light.origin);
}

void Light::revertTransform ()
{
	m_aabb_light.origin = m_originKey.m_origin;
}

void Light::freezeTransform ()
{
	m_originKey.m_origin = m_aabb_light.origin;
	m_originKey.write(&m_entity);
}

void Light::transformChanged ()
{
	revertTransform();
	m_evaluateTransform();
	updateOrigin();
}

const Matrix4& Light::getLocalPivot () const
{
	m_localPivot = Matrix4::getIdentity();
	m_localPivot.t().getVector3() = m_aabb_light.origin;
	return m_localPivot;
}

const AABB& Light::aabb () const
{
	return m_aabb_light;
}

void Light::light_vertices (const AABB& aabb_light, Vector3 points[6]) const
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

void Light::light_draw (const AABB& aabb_light, RenderStateFlags state) const
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

