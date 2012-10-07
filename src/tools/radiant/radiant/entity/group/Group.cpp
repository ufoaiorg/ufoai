/**
 * @file
 * @brief Represents any entity which does not have a fixed size specified in its entity-definition (except misc_model).
 * This entity behaves as a group, i.e. it contains brushes.
 */

/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

 This file is part of GtkRadiant.

 GtkRadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 GtkRadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with GtkRadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "Group.h"
#include "iregistry.h"

#include "renderable.h"
#include "render.h"

void Group::construct ()
{
	m_keyObservers.insert("targetname", NamedEntity::IdentifierChangedCaller(m_named));
}

Group::Group (EntityClass* eclass, scene::Node& node, const Callback& transformChanged) :
		m_entity(eclass), m_named(m_entity), m_nameKeys(m_entity), m_renderName(m_named, m_name_origin), m_name_origin(
				g_vector3_identity), m_transformChanged(transformChanged)
{
	construct();
}

Group::Group (const Group& other, scene::Node& node, const Callback& transformChanged) :
		m_entity(other.m_entity), m_named(m_entity), m_nameKeys(m_entity), m_renderName(m_named, g_vector3_identity), m_transformChanged(
				transformChanged)
{
	construct();
}

void Group::instanceAttach (const scene::Path& path)
{
	if (++m_instanceCounter.m_count == 1) {
		m_entity.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_traverse.instanceAttach(path_find_mapfile(path.begin(), path.end()));
		m_entity.attach(m_keyObservers);
	}
}

void Group::instanceDetach (const scene::Path& path)
{
	if (--m_instanceCounter.m_count == 0) {
		m_entity.detach(m_keyObservers);
		m_traverse.instanceDetach(path_find_mapfile(path.begin(), path.end()));
		m_entity.instanceDetach(path_find_mapfile(path.begin(), path.end()));
	}
}

void Group::snapto (float snap)
{
}

EntityKeyValues& Group::getEntity ()
{
	return m_entity;
}

const EntityKeyValues& Group::getEntity () const
{
	return m_entity;
}

scene::Traversable& Group::getTraversable ()
{
	return m_traverse;
}

const scene::Traversable& Group::getTraversable () const
{
	return m_traverse;
}

Namespaced& Group::getNamespaced ()
{
	return m_nameKeys;
}

NamedEntity& Group::getNameable ()
{
	return m_named;
}

const NamedEntity& Group::getNameable () const
{
	return m_named;
}

const TransformNode& Group::getTransformNode () const
{
	return m_transform;
}

TransformNode& Group::getTransformNode ()
{
	return m_transform;
}

void Group::attach (scene::Traversable::Observer* observer)
{
	m_traverse.attach(observer);
}

void Group::detach (scene::Traversable::Observer* observer)
{
	m_traverse.detach(observer);
}

void Group::renderSolid (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld) const
{
	renderer.SetState(m_entity.getEntityClass().m_state_wire, Renderer::eWireframeOnly);
}

void Group::renderWireframe (Renderer& renderer, const VolumeTest& volume, const Matrix4& localToWorld,
		const AABB& childBounds) const
{
	renderSolid(renderer, volume, localToWorld);

	if (GlobalRegistry().get("user/ui/xyview/showEntityNames") == "1") {
		// don't draw the name for worldspawn
		if (m_entity.getEntityClass().name() == "worldspawn")
			return;

		// place name in the middle of the "children cloud"
		m_name_origin = childBounds.origin;

		renderer.addRenderable(m_renderName, localToWorld);
	}
}
