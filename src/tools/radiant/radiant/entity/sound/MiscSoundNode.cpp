#include "MiscSoundNode.h"
#include "MiscSoundInstance.h"

MiscSoundNode::MiscSoundNode (EntityClass* eclass) :
		m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<MiscSoundInstance>::Caller(m_instances))
{
}
MiscSoundNode::MiscSoundNode (const MiscSoundNode& other) :
		scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), Nameable(other), Snappable(other), TransformNode(
				other), EntityNode(other), Namespaced(other), m_contained(other.m_contained, *this,
				InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<MiscSoundInstance>::Caller(m_instances))
{
}

std::string MiscSoundNode::name () const
{
	return m_contained.getNameable().name();
}

void MiscSoundNode::snapto (float snap)
{
	m_contained.snapto(snap);
}

const Matrix4& MiscSoundNode::localToParent () const
{
	return m_contained.getTransformNode().localToParent();
}

Entity& MiscSoundNode::getEntity ()
{
	return m_contained.getEntity();
}

void MiscSoundNode::setNamespace (INamespace& space)
{
	m_contained.getNamespaced().setNamespace(space);
}

scene::Node& MiscSoundNode::clone () const
{
	return *(new MiscSoundNode(*this));
}

scene::Instance* MiscSoundNode::create (const scene::Path& path, scene::Instance* parent)
{
	return new MiscSoundInstance(path, parent, m_contained);
}

void MiscSoundNode::forEachInstance (const scene::Instantiable::Visitor& visitor)
{
	m_instances.forEachInstance(visitor);
}

void MiscSoundNode::insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
{
	m_instances.insert(observer, path, instance);
}

scene::Instance* MiscSoundNode::erase (scene::Instantiable::Observer* observer, const scene::Path& path)
{
	return m_instances.erase(observer, path);
}

void MiscSoundNode::attach (const NameCallback& callback)
{
	m_contained.getNameable().attach(callback);
}

void MiscSoundNode::detach (const NameCallback& callback)
{
	m_contained.getNameable().detach(callback);
}
