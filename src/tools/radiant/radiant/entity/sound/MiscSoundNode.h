#pragma once

#include "MiscSoundInstance.h"

class MiscSoundNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public Nameable,
		public Snappable,
		public TransformNode,
		public EntityNode,
		public Namespaced {
	InstanceSet m_instances;
	MiscSound m_contained;

public:

	// Nameable implementation
	std::string name () const;

	// Snappable implementation
	void snapto (float snap);

	// TransformNode implementation
	const Matrix4& localToParent () const;

	// EntityNode implementation
	Entity& getEntity ();

	// Namespaced implementation
	void setNamespace (INamespace& space);

	MiscSoundNode (EntityClass* eclass);
	MiscSoundNode (const MiscSoundNode& other);

	scene::Node& clone () const;

	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);

	void attach (const NameCallback& callback);
	void detach (const NameCallback& callback);
};

