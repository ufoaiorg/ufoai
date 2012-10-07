#include "MiscParticleInstance.h"

MiscParticleInstance::MiscParticleInstance (const scene::Path& path, scene::Instance* parent, MiscParticle& contained) :
		TargetableInstance(path, parent, contained.getEntity(), *this), TransformModifier(
				MiscParticle::TransformChangedCaller(contained), ApplyTransformCaller(*this)), m_contained(contained)
{
	m_contained.instanceAttach(Instance::path());

	StaticRenderableConnectionLines::instance().attach(*this);
}

MiscParticleInstance::~MiscParticleInstance ()
{
	StaticRenderableConnectionLines::instance().detach(*this);

	m_contained.instanceDetach(Instance::path());
}

const AABB& MiscParticleInstance::localAABB () const
{
	return m_contained.localAABB();
}

VolumeIntersectionValue MiscParticleInstance::intersectVolume (const VolumeTest& test,
		const Matrix4& localToWorld) const
{
	return m_contained.intersectVolume(test, localToWorld);
}

void MiscParticleInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderSolid(renderer, volume, Instance::localToWorld());
}

void MiscParticleInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderWireframe(renderer, volume, Instance::localToWorld());
}

void MiscParticleInstance::testSelect (Selector& selector, SelectionTest& test)
{
	m_contained.testSelect(selector, test, Instance::localToWorld());
}

void MiscParticleInstance::evaluateTransform ()
{
	if (getType() == TRANSFORM_PRIMITIVE) {
		m_contained.translate(getTranslation());
		m_contained.rotate(getRotation());
	}
}

void MiscParticleInstance::applyTransform ()
{
	m_contained.revertTransform();
	evaluateTransform();
	m_contained.freezeTransform();
}
