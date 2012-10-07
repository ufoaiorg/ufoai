#pragma once

#include "MiscModelInstance.h"

class MiscModelNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public scene::Traversable::Observer,
		public Nameable,
		public Snappable,
		public TransformNode,
		public scene::Traversable,
		public EntityNode,
		public Namespaced {
	InstanceSet m_instances;
	MiscModel m_contained;

	void construct ();
	void destroy ();

public:
	// Snappable implementation
	void snapto (float snap);

	// TransformNode implementation
	const Matrix4& localToParent () const;

	// Traversable implementation
	void insert (Node& node);

	void erase (Node& node);

	void traverse (const Walker& walker);

	bool empty () const;

	// EntityNode implementation
	Entity& getEntity ();

	// Namespaced implementation
	void setNamespace (INamespace& space);

	MiscModelNode (EntityClass* eclass);
	MiscModelNode (const MiscModelNode& other);
	~MiscModelNode ();

	scene::Node& clone () const;

	void insertChild (scene::Node& child);
	void eraseChild (scene::Node& child);

	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);

	// Nameable implementation
	std::string name () const;

	void attach (const NameCallback& callback);

	void detach (const NameCallback& callback);
};

