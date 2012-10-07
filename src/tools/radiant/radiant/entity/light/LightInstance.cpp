#include "LightInstance.h"

LightInstance::LightInstance (const scene::Path& path, scene::Instance* parent, Light& contained) :
		TargetableInstance(path, parent, contained.getEntity(), *this), TransformModifier(
				Light::TransformChangedCaller(contained), ApplyTransformCaller(*this)), m_contained(contained)
{
	m_contained.instanceAttach(Instance::path());

	StaticRenderableConnectionLines::instance().attach(*this);
}

LightInstance::~LightInstance ()
{
	StaticRenderableConnectionLines::instance().detach(*this);

	m_contained.instanceDetach(Instance::path());
}

const AABB& LightInstance::localAABB () const
{
	return m_contained.localAABB();
}

void LightInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderSolid(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
}

void LightInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderWireframe(renderer, volume, Instance::localToWorld(), getSelectable().isSelected());
}

void LightInstance::testSelect (Selector& selector, SelectionTest& test)
{
	m_contained.testSelect(selector, test, Instance::localToWorld());
}

void LightInstance::evaluateTransform ()
{
	if (getType() == TRANSFORM_PRIMITIVE) {
		m_contained.translate(getTranslation());
		m_contained.rotate(getRotation());
	}
}

void LightInstance::applyTransform ()
{
	m_contained.revertTransform();
	evaluateTransform();
	m_contained.freezeTransform();
}
