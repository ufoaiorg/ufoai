#pragma once

#include "MiscSound.h"

#include "transformlib.h"
#include "instancelib.h"

#include "../targetable.h"

class MiscSoundInstance: public TargetableInstance,
		public TransformModifier,
		public Renderable,
		public SelectionTestable,
		public Bounded,
		public Cullable {
	MiscSound& m_contained;
	mutable AABB m_bounds;
public:

	// Bounded implementation
	const AABB& localAABB () const;

	// Cullable implementation
	VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const;

	STRING_CONSTANT(Name, "MiscSoundInstance");

	MiscSoundInstance (const scene::Path& path, scene::Instance* parent, MiscSound& contained);
	~MiscSoundInstance ();

	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;

	void testSelect (Selector& selector, SelectionTest& test);

	void evaluateTransform ();
	void applyTransform ();
	typedef MemberCaller<MiscSoundInstance, &MiscSoundInstance::applyTransform> ApplyTransformCaller;
};
