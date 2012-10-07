#pragma once

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

#include "../targetable.h"
#include "../keys/OriginKey.h"
#include "../keys/AngleKey.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"
#include "../RenderableArrow.h"
#include "../EntitySettings.h"

class GenericEntity: public Cullable, public Bounded, public Snappable {
private:
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;

	OriginKey m_originKey;
	Vector3 m_origin;
	AngleKey m_angleKey;
	float m_angle;

	NamedEntity m_named;
	NameKeys m_nameKeys;

	AABB m_aabb_local;
	Ray m_ray;

	RenderableArrow m_arrow;
	RenderableSolidAABB m_renderAABBSolid;
	RenderableWireframeAABB m_renderAABBWire;
	RenderableNamedEntity m_renderName;

	Callback m_transformChanged;
	Callback m_evaluateTransform;

	void construct ();

	void updateTransform ();
	typedef MemberCaller<GenericEntity, &GenericEntity::updateTransform> UpdateTransformCaller;
	void originChanged ();
	typedef MemberCaller<GenericEntity, &GenericEntity::originChanged> OriginChangedCaller;
	void angleChanged ();
	typedef MemberCaller<GenericEntity, &GenericEntity::angleChanged> AngleChangedCaller;

	InstanceCounter m_instanceCounter;
public:

	GenericEntity (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);
	GenericEntity (const GenericEntity& other, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);

	void instanceAttach (const scene::Path& path);
	void instanceDetach (const scene::Path& path);

	EntityKeyValues& getEntity ();
	const EntityKeyValues& getEntity () const;

	Namespaced& getNamespaced ();
	NamedEntity& getNameable ();
	const NamedEntity& getNameable () const;
	TransformNode& getTransformNode ();
	const TransformNode& getTransformNode () const;
	const AABB& localAABB () const;
	VolumeIntersectionValue intersectVolume (const VolumeTest& volume, const Matrix4& localToWorld) const;
	void renderArrow (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;
	void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;

	void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld);

	void translate (const Vector3& translation);
	void rotate (const Quaternion& rotation);
	void snapto (float snap);
	void revertTransform ();
	void freezeTransform ();
	void transformChanged ();
	typedef MemberCaller<GenericEntity, &GenericEntity::transformChanged> TransformChangedCaller;
};
