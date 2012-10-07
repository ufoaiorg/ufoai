#pragma once

#include "MiscModel.h"

#include "renderable.h"

#include "../targetable.h"

class MiscModelInstance: public TargetableInstance, public TransformModifier, public Renderable {
	MiscModel& m_contained;
public:

	STRING_CONSTANT(Name, "MiscModelInstance");

	MiscModelInstance (const scene::Path& path, scene::Instance* parent, MiscModel& miscmodel);
	~MiscModelInstance ();
	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;
	void evaluateTransform ();
	void applyTransform ();
	typedef MemberCaller<MiscModelInstance, &MiscModelInstance::applyTransform> ApplyTransformCaller;
};
