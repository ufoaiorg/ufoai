#pragma once

#include "MiscParticle.h"
#include "../model.h"

class MiscParticleNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public Nameable,
		public Snappable,
		public TransformNode,
		public EntityNode,
		public Namespaced {
private:
	InstanceSet m_instances;
	MiscParticle m_contained;
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

	MiscParticleNode (EntityClass* eclass);
	MiscParticleNode (const MiscParticleNode& other);

	scene::Node& clone () const;

	scene::Instance* create (const scene::Path& path, scene::Instance* parent);
	void forEachInstance (const scene::Instantiable::Visitor& visitor);
	void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance);
	scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path);

	void attach (const NameCallback& callback);

	void detach (const NameCallback& callback);
};
