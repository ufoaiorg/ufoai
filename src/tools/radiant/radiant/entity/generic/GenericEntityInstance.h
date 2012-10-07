#pragma once

#include "GenericEntity.h"

class GenericEntityInstance: public TargetableInstance,
		public TransformModifier,
		public Renderable,
		public SelectionTestable,
		public Bounded,
		public Cullable {
	GenericEntity& m_contained;
	mutable AABB m_bounds;
public:
	GenericEntityInstance (const scene::Path& path, scene::Instance* parent, GenericEntity& contained);
	~GenericEntityInstance ();

	STRING_CONSTANT(Name, "GenericEntityInstance");

	// Bounded implementation
	const AABB& localAABB () const;

	// Cullable implementation
	VolumeIntersectionValue intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const;

	// Renderable implementation
	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;

	// SelectionTestable implementation
	void testSelect (Selector& selector, SelectionTest& test);

	// Instance implementation
	void evaluateTransform ();

	void applyTransform ();
	typedef MemberCaller<GenericEntityInstance, &GenericEntityInstance::applyTransform> ApplyTransformCaller;
};
