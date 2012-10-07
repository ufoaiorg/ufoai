#pragma once

#include "Light.h"

class LightInstance: public TargetableInstance,
		public TransformModifier,
		public Renderable,
		public SelectionTestable,
		public Bounded {
private:
	Light& m_contained;
public:
	STRING_CONSTANT(Name, "LightInstance");

	LightInstance (const scene::Path& path, scene::Instance* parent, Light& contained);
	~LightInstance ();

	// Bounded implementation
	const AABB& localAABB () const;

	// Renderable implementation
	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;
	void testSelect (Selector& selector, SelectionTest& test);

	// scene::Instance
	void evaluateTransform ();

	void applyTransform ();
	typedef MemberCaller<LightInstance, &LightInstance::applyTransform> ApplyTransformCaller;
};
