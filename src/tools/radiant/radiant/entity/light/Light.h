#pragma once

#include "cullable.h"
#include "editable.h"
#include "traverselib.h"

#include "RenderLightRadiusBox.h"
#include "RenderLightRadiusFill.h"
#include "RenderLightRadiusWire.h"

#include "../targetable.h"
#include "../keys/OriginKey.h"
#include "../keys/ColourKey.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

class Light: public OpenGLRenderable, public Cullable, public Bounded, public Editable, public Snappable {
private:
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

	AABB m_aabb_light;

	Callback m_transformChanged;
	Callback m_boundsChanged;
	Callback m_evaluateTransform;

	InstanceCounter m_instanceCounter;
	mutable Matrix4 m_localPivot;

	void construct ();

	void updateOrigin ();
	void originChanged ();
	typedef MemberCaller<Light, &Light::originChanged> OriginChangedCaller;

	void light_draw (const AABB& aabb_light, RenderStateFlags state) const;
	void light_vertices (const AABB& aabb_light, Vector3 points[6]) const;

public:
	Light (EntityClass* eclass, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged,
			const Callback& evaluateTransform);
	Light (const Light& other, scene::Node& node, const Callback& transformChanged, const Callback& boundsChanged,
			const Callback& evaluateTransform);
	~Light ();

	void instanceAttach (const scene::Path& path);
	void instanceDetach (const scene::Path& path);

	EntityKeyValues& getEntity ();
	const EntityKeyValues& getEntity () const;

	scene::Traversable& getTraversable ();
	const scene::Traversable& getTraversable () const;

	Namespaced& getNamespaced ();
	const NamedEntity& getNameable () const;
	NamedEntity& getNameable ();
	TransformNode& getTransformNode ();
	const TransformNode& getTransformNode () const;
	void attach (scene::Traversable::Observer* observer);
	void detach (scene::Traversable::Observer* observer);

	void render (RenderStateFlags state) const;

	VolumeIntersectionValue intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const;
	// cache
	const AABB& localAABB () const;

	void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld, bool selected) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
			bool selected) const;

	void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld);

	void translate (const Vector3& translation);
	void rotate (const Quaternion& rotation);
	void snapto (float snap);
	void setLightRadius (const AABB& aabb);
	void transformLightRadius (const Matrix4& transform);
	void revertTransform ();
	void freezeTransform ();
	void transformChanged ();
	typedef MemberCaller<Light, &Light::transformChanged> TransformChangedCaller;

	const Matrix4& getLocalPivot () const;

	const AABB& aabb () const;
};
