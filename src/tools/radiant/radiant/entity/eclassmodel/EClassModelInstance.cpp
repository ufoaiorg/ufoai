#include "EClassModelInstance.h"

EClassModelInstance::EClassModelInstance (const scene::Path& path, scene::Instance* parent, EclassModel& contained) :
		TargetableInstance(path, parent, contained.getEntity(), *this), TransformModifier(
				EclassModel::TransformChangedCaller(contained), ApplyTransformCaller(*this)), m_contained(contained)
{
	m_contained.instanceAttach(Instance::path());

	StaticRenderableConnectionLines::instance().attach(*this);
}

EClassModelInstance::~EClassModelInstance ()
{
	StaticRenderableConnectionLines::instance().detach(*this);

	m_contained.instanceDetach(Instance::path());
}

void EClassModelInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderSolid(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
}

void EClassModelInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderWireframe(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
}

void EClassModelInstance::evaluateTransform ()
{
	if (getType() == TRANSFORM_PRIMITIVE) {
		m_contained.translate(getTranslation());
		m_contained.rotate(getRotation());
	}
}

void EClassModelInstance::applyTransform ()
{
	m_contained.revertTransform();
	evaluateTransform();
	m_contained.freezeTransform();
}
