#pragma once

#include "EClassModel.h"

class EClassModelNode: public scene::Node,
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
	EclassModel m_contained;

public:
	EClassModelNode (EntityClass* eclass);
	EClassModelNode (const EClassModelNode& other);
	~EClassModelNode ();

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

	void insertChild (scene::Node& child);
	void eraseChild (scene::Node& child);

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
