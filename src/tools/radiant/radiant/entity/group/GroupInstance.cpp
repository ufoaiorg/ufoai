#include "GroupInstance.h"
#include "Group.h"

GroupInstance::GroupInstance (const scene::Path& path, scene::Instance* parent, Group& group) :
		TargetableInstance(path, parent, group.getEntity(), *this), m_contained(group)
{
	m_contained.instanceAttach(Instance::path());
	StaticRenderableConnectionLines::instance().attach(*this);
}

GroupInstance::~GroupInstance ()
{
	StaticRenderableConnectionLines::instance().detach(*this);
	m_contained.instanceDetach(Instance::path());
}

void GroupInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderSolid(renderer, volume, Instance::localToWorld());
}

void GroupInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderWireframe(renderer, volume, Instance::localToWorld(), Instance::childBounds());
}
