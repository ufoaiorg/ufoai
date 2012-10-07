#include "MiscModelNode.h"

MiscModelNode::MiscModelNode (EntityClass* eclass) :
		m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<MiscModelInstance>::Caller(m_instances))
{
	construct();
}
MiscModelNode::MiscModelNode (const MiscModelNode& other) :
		scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), scene::Traversable::Observer(other), Nameable(
				other), Snappable(other), TransformNode(other), scene::Traversable(other), EntityNode(other), Namespaced(
				other), m_contained(other.m_contained, *this, InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<MiscModelInstance>::Caller(m_instances))
{
	construct();
}
MiscModelNode::~MiscModelNode ()
{
	destroy();
}

void MiscModelNode::construct ()
{
	m_contained.attach(this);
}
void MiscModelNode::destroy ()
{
	m_contained.detach(this);
}

void MiscModelNode::snapto (float snap)
{
	m_contained.snapto(snap);
}

const Matrix4& MiscModelNode::localToParent () const
{
	return m_contained.getTransformNode().localToParent();
}

void MiscModelNode::insert (Node& node)
{
	m_contained.getTraversable().insert(node);
}

void MiscModelNode::erase (Node& node)
{
	m_contained.getTraversable().erase(node);
}

void MiscModelNode::traverse (const Walker& walker)
{
	m_contained.getTraversable().traverse(walker);
}

bool MiscModelNode::empty () const
{
	return m_contained.getTraversable().empty();
}

Entity& MiscModelNode::getEntity ()
{
	return m_contained.getEntity();
}

void MiscModelNode::setNamespace (INamespace& space)
{
	m_contained.getNamespaced().setNamespace(space);
}

scene::Node& MiscModelNode::clone () const
{
	return *(new MiscModelNode(*this));
}

void MiscModelNode::insertChild (scene::Node& child)
{
	m_instances.insertChild(child);
}

void MiscModelNode::eraseChild (scene::Node& child)
{
	m_instances.eraseChild(child);
}

scene::Instance* MiscModelNode::create (const scene::Path& path, scene::Instance* parent)
{
	return new MiscModelInstance(path, parent, m_contained);
}

void MiscModelNode::forEachInstance (const scene::Instantiable::Visitor& visitor)
{
	m_instances.forEachInstance(visitor);
}

void MiscModelNode::insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
{
	m_instances.insert(observer, path, instance);
}

scene::Instance* MiscModelNode::erase (scene::Instantiable::Observer* observer, const scene::Path& path)
{
	return m_instances.erase(observer, path);
}

std::string MiscModelNode::name () const
{
	return m_contained.getNameable().name();
}

void MiscModelNode::attach (const NameCallback& callback)
{
	m_contained.getNameable().attach(callback);
}

void MiscModelNode::detach (const NameCallback& callback)
{
	m_contained.getNameable().detach(callback);
}
