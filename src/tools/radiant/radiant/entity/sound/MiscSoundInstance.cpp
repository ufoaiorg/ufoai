#include "MiscSoundInstance.h"

MiscSoundInstance::MiscSoundInstance (const scene::Path& path, scene::Instance* parent, MiscSound& contained) :
		TargetableInstance(path, parent, contained.getEntity(), *this), TransformModifier(
				MiscSound::TransformChangedCaller(contained), ApplyTransformCaller(*this)), m_contained(contained)
{
	m_contained.instanceAttach(Instance::path());

	StaticRenderableConnectionLines::instance().attach(*this);
}

MiscSoundInstance::~MiscSoundInstance ()
{
	StaticRenderableConnectionLines::instance().detach(*this);

	m_contained.instanceDetach(Instance::path());
}

const AABB& MiscSoundInstance::localAABB () const
{
	return m_contained.localAABB();
}

VolumeIntersectionValue MiscSoundInstance::intersectVolume (const VolumeTest& test, const Matrix4& localToWorld) const
{
	return m_contained.intersectVolume(test, localToWorld);
}

void MiscSoundInstance::renderSolid (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderSolid(renderer, volume, Instance::localToWorld());
}

void MiscSoundInstance::renderWireframe (Renderer& renderer, const VolumeTest& volume) const
{
	m_contained.renderWireframe(renderer, volume, Instance::localToWorld());
}

void MiscSoundInstance::testSelect (Selector& selector, SelectionTest& test)
{
	m_contained.testSelect(selector, test, Instance::localToWorld());
}

void MiscSoundInstance::evaluateTransform ()
{
	if (getType() == TRANSFORM_PRIMITIVE) {
		m_contained.translate(getTranslation());
		m_contained.rotate(getRotation());
	}
}

void MiscSoundInstance::applyTransform ()
{
	m_contained.revertTransform();
	evaluateTransform();
	m_contained.freezeTransform();
}
