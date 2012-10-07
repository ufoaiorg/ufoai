#pragma once

#include "GenericEntityInstance.h"

class GenericEntityNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public Nameable,
		public Snappable,
		public TransformNode,
		public EntityNode,
		public Namespaced {
	InstanceSet m_instances;
	GenericEntity m_contained;

public:
	GenericEntityNode (EntityClass* eclass);
	GenericEntityNode (const GenericEntityNode& other);

	// Snappable implementation
	void snapto (float snap);

	// TransformNode implementation
	const Matrix4& localToParent () const;

	// EntityNode implementation
	Entity& getEntity ();

	// Namespaced implementation
	void setNamespace (INamespace& space);

	scene::Node& clone () const;

	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);
	// Nameable implementation
	virtual std::string name () const;
	virtual void attach (const NameCallback& callback);
	virtual void detach (const NameCallback& callback);
};
