#pragma once

#include "iparticles.h"

#include "cullable.h"
#include "Bounded.h"
#include "editable.h"

#include "RenderableParticleID.h"
#include "RenderableParticle.h"

#include "../keys/OriginKey.h"
#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

class MiscParticle: public Cullable, public Bounded, public Snappable {
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;

	OriginKey m_originKey;
	Vector3 m_origin;

	NamedEntity m_named;
	NameKeys m_nameKeys;
	IParticleDefinition* m_particle;
	mutable Vector3 m_id_origin;

	// bounding box
	AABB m_aabb_local;

	RenderableSolidAABB m_renderAABBSolid;
	RenderableParticle m_renderParticle;
	RenderableParticleID m_renderParticleID;
	RenderableWireframeAABB m_renderAABBWire;
	RenderableNamedEntity m_renderName;

	Callback m_transformChanged;
	Callback m_evaluateTransform;

	void particleChanged (const std::string& value);
	typedef MemberCaller1<MiscParticle, const std::string&, &MiscParticle::particleChanged> ParticleChangedCaller;

	void construct ();

	void updateTransform ();
	typedef MemberCaller<MiscParticle, &MiscParticle::updateTransform> UpdateTransformCaller;
	void originChanged ();
	typedef MemberCaller<MiscParticle, &MiscParticle::originChanged> OriginChangedCaller;

	InstanceCounter m_instanceCounter;

public:

	MiscParticle (EntityClass* eclass, scene::Node& node, const Callback& transformChanged,
			const Callback& evaluateTransform);
	MiscParticle (const MiscParticle& other, scene::Node& node, const Callback& transformChanged,
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

	void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;

	void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;

	void testSelect (Selector& selector, SelectionTest& test, const Matrix4& localToWorld);

	void translate (const Vector3& translation);

	void rotate (const Quaternion& rotation);

	void snapto (float snap);

	void revertTransform ();

	void freezeTransform ();

	void transformChanged ();
	typedef MemberCaller<MiscParticle, &MiscParticle::transformChanged> TransformChangedCaller;
};
