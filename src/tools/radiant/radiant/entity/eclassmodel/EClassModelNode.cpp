#include "EClassModelNode.h"
#include "EClassModelInstance.h"

EClassModelNode::EClassModelNode (EntityClass* eclass) :
		m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<EClassModelInstance>::Caller(m_instances))
{
	m_contained.attach(this);
}

EClassModelNode::EClassModelNode (const EClassModelNode& other) :
		scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), scene::Traversable::Observer(other), Nameable(
				other), Snappable(other), TransformNode(other), scene::Traversable(other), EntityNode(other), Namespaced(
				other), m_contained(other.m_contained, *this, InstanceSet::TransformChangedCaller(m_instances),
				InstanceSetEvaluateTransform<EClassModelInstance>::Caller(m_instances))
{
	m_contained.attach(this);
}

EClassModelNode::~EClassModelNode ()
{
	m_contained.detach(this);
}

void EClassModelNode::snapto (float snap)
{
	m_contained.snapto(snap);
}

const Matrix4& EClassModelNode::localToParent () const
{
	return m_contained.getTransformNode().localToParent();
}

void EClassModelNode::insert (Node& node)
{
	m_contained.getTraversable().insert(node);
}

void EClassModelNode::erase (Node& node)
{
	m_contained.getTraversable().erase(node);
}

void EClassModelNode::traverse (const Walker& walker)
{
	m_contained.getTraversable().traverse(walker);
}

bool EClassModelNode::empty () const
{
	return m_contained.getTraversable().empty();
}

Entity& EClassModelNode::getEntity ()
{
	return m_contained.getEntity();
}

void EClassModelNode::setNamespace (INamespace& space)
{
	m_contained.getNamespaced().setNamespace(space);
}

void EClassModelNode::insertChild (scene::Node& child)
{
	m_instances.insertChild(child);
}

void EClassModelNode::eraseChild (scene::Node& child)
{
	m_instances.eraseChild(child);
}

scene::Node& EClassModelNode::clone () const
{
	return *(new EClassModelNode(*this));
}

scene::Instance* EClassModelNode::create (const scene::Path& path, scene::Instance* parent)
{
	return new EClassModelInstance(path, parent, m_contained);
}

void EClassModelNode::forEachInstance (const scene::Instantiable::Visitor& visitor)
{
	m_instances.forEachInstance(visitor);
}

void EClassModelNode::insert (scene::Instantiable::Observer* observer, const scene::Path& path,
		scene::Instance* instance)
{
	m_instances.insert(observer, path, instance);
}

scene::Instance* EClassModelNode::erase (scene::Instantiable::Observer* observer, const scene::Path& path)
{
	return m_instances.erase(observer, path);
}

std::string EClassModelNode::name () const
{
	return m_contained.getNameable().name();
}

void EClassModelNode::attach (const NameCallback& callback)
{
	m_contained.getNameable().attach(callback);
}

void EClassModelNode::detach (const NameCallback& callback)
{
	m_contained.getNameable().detach(callback);
}
