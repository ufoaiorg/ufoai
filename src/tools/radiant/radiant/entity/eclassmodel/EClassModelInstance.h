#pragma once

#include "EClassModel.h"

class EClassModelInstance: public TargetableInstance, public TransformModifier, public Renderable {
private:
	EclassModel& m_contained;

public:
	STRING_CONSTANT(Name, "EClassModelInstance");

	EClassModelInstance (const scene::Path& path, scene::Instance* parent, EclassModel& contained);
	~EClassModelInstance ();
	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;

	void evaluateTransform ();
	void applyTransform ();
	typedef MemberCaller<EClassModelInstance, &EClassModelInstance::applyTransform> ApplyTransformCaller;
};
