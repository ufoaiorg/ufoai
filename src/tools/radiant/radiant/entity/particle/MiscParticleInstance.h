#pragma once

#include "MiscParticle.h"
#include "../targetable.h"

class MiscParticleInstance: public TargetableInstance,
		public TransformModifier,
		public Renderable,
		public SelectionTestable,
		public Bounded,
		public Cullable {
private:
	MiscParticle& m_contained;
	mutable AABB m_bounds;
public:

	// Bounded implementation
	const AABB& localAABB () const;

	// Cullable implementation
	VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const;

	STRING_CONSTANT(Name, "MiscParticleInstance");

	MiscParticleInstance (const scene::Path& path, scene::Instance* parent, MiscParticle& contained);
	~MiscParticleInstance ();
	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;
	void testSelect (Selector& selector, SelectionTest& test);

	void evaluateTransform ();
	void applyTransform ();
	typedef MemberCaller<MiscParticleInstance, &MiscParticleInstance::applyTransform> ApplyTransformCaller;
};
