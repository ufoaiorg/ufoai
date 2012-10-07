#include "MiscModelInstance.h"

MiscModelInstance::MiscModelInstance (const scene::Path& path, scene::Instance* parent, MiscModel& miscmodel) :
		TargetableInstance(path, parent, miscmodel.getEntity(), *this), TransformModifier(
				MiscModel::TransformChangedCaller(miscmodel), ApplyTransformCaller(*this)), m_contained(miscmodel)
{
	m_contained.instanceAttach(Instance::path());
	StaticRenderableConnectionLines::instance().attach(*this);
}

MiscModelInstance::~MiscModelInstance ()
{
	StaticRenderableConnectionLines::instance().detach(*this);
	m_contained.instanceDetach(Instance::path());
}

void MiscModelInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderSolid(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
}

void MiscModelInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderWireframe(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
}

void MiscModelInstance::evaluateTransform ()
{
	if (getType() == TRANSFORM_PRIMITIVE) {
		m_contained.translate(getTranslation());
		m_contained.rotate(getRotation());
		m_contained.scale(getScale());
	}
}

void MiscModelInstance::applyTransform ()
{
	m_contained.revertTransform();
	evaluateTransform();
	m_contained.freezeTransform();
}
