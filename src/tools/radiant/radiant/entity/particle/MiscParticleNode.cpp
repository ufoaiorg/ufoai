#include "MiscParticleNode.h"
#include "MiscParticleInstance.h"

MiscParticleNode::MiscParticleNode (EntityClass* eclass) :
		m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<MiscParticleInstance>::Caller(m_instances))
{
}

MiscParticleNode::MiscParticleNode (const MiscParticleNode& other) :
		scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), Nameable(other), Snappable(other), TransformNode(
				other), EntityNode(other), Namespaced(other), m_contained(other.m_contained, *this,
				InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<MiscParticleInstance>::Caller(m_instances))
{
}

std::string MiscParticleNode::name () const
{
	return m_contained.getNameable().name();
}

void MiscParticleNode::snapto (float snap)
{
	m_contained.snapto(snap);
}

const Matrix4& MiscParticleNode::localToParent () const
{
	return m_contained.getTransformNode().localToParent();
}

Entity& MiscParticleNode::getEntity ()
{
	return m_contained.getEntity();
}

void MiscParticleNode::setNamespace (INamespace& space)
{
	m_contained.getNamespaced().setNamespace(space);
}

scene::Node& MiscParticleNode::clone () const
{
	return *(new MiscParticleNode(*this));
}

scene::Instance* MiscParticleNode::create (const scene::Path& path, scene::Instance* parent)
{
	return new MiscParticleInstance(path, parent, m_contained);
}

void MiscParticleNode::forEachInstance (const scene::Instantiable::Visitor& visitor)
{
	m_instances.forEachInstance(visitor);
}

void MiscParticleNode::insert (scene::Instantiable::Observer* observer, const scene::Path& path,
		scene::Instance* instance)
{
	m_instances.insert(observer, path, instance);
}

scene::Instance* MiscParticleNode::erase (scene::Instantiable::Observer* observer, const scene::Path& path)
{
	return m_instances.erase(observer, path);
}

void MiscParticleNode::attach (const NameCallback& callback)
{
	m_contained.getNameable().attach(callback);
}

void MiscParticleNode::detach (const NameCallback& callback)
{
	m_contained.getNameable().detach(callback);
}
