#pragma once

#include "traverselib.h"

#include "../namedentity.h"
#include "../keys/KeyObserverMap.h"
#include "../NameKeys.h"

class Group {
	EntityKeyValues m_entity;
	KeyObserverMap m_keyObservers;
	MatrixTransform m_transform;
	TraversableNodeSet m_traverse;

	NamedEntity m_named;
	NameKeys m_nameKeys;

	RenderableNamedEntity m_renderName;
	mutable Vector3 m_name_origin;

	Callback m_transformChanged;

	void construct ();

	InstanceCounter m_instanceCounter;

public:
	Group (EntityClass* eclass, scene::Node& node, const Callback& transformChanged);

	Group (const Group& other, scene::Node& node, const Callback& transformChanged);

	void instanceAttach (const scene::Path& path);

	void instanceDetach (const scene::Path& path);

	void snapto (float snap);

	EntityKeyValues& getEntity ();

	const EntityKeyValues& getEntity () const;

	scene::Traversable& getTraversable ();

	const scene::Traversable& getTraversable () const;

	Namespaced& getNamespaced ();

	NamedEntity& getNameable ();

	const NamedEntity& getNameable () const;

	const TransformNode& getTransformNode () const;

	TransformNode& getTransformNode ();

	void attach (scene::Traversable::Observer* observer);

	void detach (scene::Traversable::Observer* observer);

	void renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const;
	void renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
			const AABB& childBounds) const;
};

