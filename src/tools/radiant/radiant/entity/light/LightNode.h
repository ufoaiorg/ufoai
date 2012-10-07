#pragma once

#include "Light.h"
#include "renderable.h"

#include "math/frustum.h"
#include "selectionlib.h"
#include "instancelib.h"
#include "transformlib.h"
#include "entitylib.h"
#include "render.h"
#include "eclasslib.h"
#include "stringio.h"
#include "traverselib.h"

class LightNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public scene::Traversable::Observer,
		public Nameable,
		public Snappable,
		public Editable,
		public TransformNode,
		public scene::Traversable,
		public EntityNode,
		public Namespaced {
private:
	InstanceSet m_instances;
	Light m_contained;

public:
	LightNode (EntityClass* eclass);
	LightNode (const LightNode& other);
	~LightNode ();

	// Editable implementation
	const Matrix4& getLocalPivot () const;

	// Snappable implementation
	void snapto (float snap);

	// TransformNode implementation
	const Matrix4& localToParent () const;

	// Traversable implementation
	void insert (Node& node);
	void erase (Node& node);
	void traverse (const Walker& walker);
	bool empty () const;

	// Traversable observer implementation
	void insertChild (scene::Node& child);
	void eraseChild (scene::Node& child);

	// EntityNode implementation
	Entity& getEntity ();

	// Namespaced implementation
	void setNamespace (INamespace& space);

	// Cloneable implementation
	scene::Node& clone () const;

	// Instantiable implementation
	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);

	// Nameable implementation
	std::string name () const;
	void attach (const NameCallback& callback);
	void detach (const NameCallback& callback);
};
