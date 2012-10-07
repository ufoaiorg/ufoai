#pragma once

#include "renderable.h"

#include "../targetable.h"

class Group;

class GroupInstance: public TargetableInstance, public Renderable {
	Group& m_contained;
public:
	GroupInstance (const scene::Path& path, scene::Instance* parent, Group& group);

	~GroupInstance ();

	void renderSolid (Renderer& renderer, const VolumeTest& volume) const;

	void renderWireframe (Renderer& renderer, const VolumeTest& volume) const;
};
