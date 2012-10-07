#pragma once

#include "Group.h"

#include "editable.h"
#include "instancelib.h"

class GroupNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public scene::Traversable::Observer,
		public Nameable,
		public Snappable,
		public TransformNode,
		public scene::Traversable,
		public EntityNode,
		public Namespaced {
private:
	InstanceSet m_instances;
	Group m_contained;

	void construct ();
	void destroy ();

public:
	GroupNode (EntityClass* eclass);
	GroupNode (const GroupNode& other);
	~GroupNode ();

	// Snappable implementation
	void snapto (float snap);

	// TransformNode implementation
	const Matrix4& localToParent () const;

	// Traversable implementation
	void insert (Node& node);
	void erase (Node& node);
	void traverse (const Walker& walker);
	bool empty () const;

	void insertChild (scene::Node& child);
	void eraseChild (scene::Node& child);

	// EntityNode implementation
	Entity& getEntity ();

	// Namespaced implementation
	void setNamespace (INamespace& space);

	// Cloneable
	scene::Node& clone () const;

	// Instantiable
	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);

	// Nameable
	std::string name () const;
	void attach (const NameCallback& callback);
	void detach (const NameCallback& callback);
};
