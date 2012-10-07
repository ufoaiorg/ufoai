#include "GroupNode.h"
#include "GroupInstance.h"

GroupNode::GroupNode (EntityClass* eclass) :
		m_contained(eclass, *this, InstanceSet::TransformChangedCaller(m_instances))
{
	construct();
}

GroupNode::GroupNode (const GroupNode& other) :
		scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), scene::Traversable::Observer(other), Nameable(
				other), Snappable(other), TransformNode(other), scene::Traversable(other), EntityNode(other), Namespaced(
				other), m_contained(other.m_contained, *this, InstanceSet::TransformChangedCaller(m_instances))
{
	construct();
}

GroupNode::~GroupNode ()
{
	destroy();
}

void GroupNode::construct ()
{
	m_contained.attach(this);
}

void GroupNode::destroy ()
{
	m_contained.detach(this);
}

void GroupNode::snapto (float snap)
{
	m_contained.snapto(snap);
}

const Matrix4& GroupNode::localToParent () const
{
	return m_contained.getTransformNode().localToParent();
}

void GroupNode::insert (Node& node)
{
	m_contained.getTraversable().insert(node);
}

void GroupNode::erase (Node& node)
{
	m_contained.getTraversable().erase(node);
}

void GroupNode::traverse (const Walker& walker)
{
	m_contained.getTraversable().traverse(walker);
}

bool GroupNode::empty () const
{
	return m_contained.getTraversable().empty();
}

Entity& GroupNode::getEntity ()
{
	return m_contained.getEntity();
}

void GroupNode::setNamespace (INamespace& space)
{
	m_contained.getNamespaced().setNamespace(space);
}

scene::Node& GroupNode::clone () const
{
	return *(new GroupNode(*this));
}

void GroupNode::insertChild (scene::Node& child)
{
	m_instances.insertChild(child);
}

void GroupNode::eraseChild (scene::Node& child)
{
	m_instances.eraseChild(child);
}

scene::Instance* GroupNode::create (const scene::Path& path, scene::Instance* parent)
{
	return new GroupInstance(path, parent, m_contained);
}

void GroupNode::forEachInstance (const scene::Instantiable::Visitor& visitor)
{
	m_instances.forEachInstance(visitor);
}

void GroupNode::insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
{
	m_instances.insert(observer, path, instance);
}

scene::Instance* GroupNode::erase (scene::Instantiable::Observer* observer, const scene::Path& path)
{
	return m_instances.erase(observer, path);
}

std::string GroupNode::name () const
{
	return m_contained.getNameable().name();
}

void GroupNode::attach (const NameCallback& callback)
{
	m_contained.getNameable().attach(callback);
}

void GroupNode::detach (const NameCallback& callback)
{
	m_contained.getNameable().detach(callback);
}
