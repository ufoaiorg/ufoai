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

#if !defined(INCLUDED_BRUSHNODE_H)
#define INCLUDED_BRUSHNODE_H

#include "instancelib.h"
#include "brush.h"
#include "brushtokens.h"

class BrushNode: public scene::Node, public scene::Instantiable, public scene::Cloneable
{
		// The typecast class (needed to cast this node onto other types)
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
					NodeContainedCast<BrushNode, Snappable>::install(m_casts);
					NodeContainedCast<BrushNode, TransformNode>::install(m_casts);
					NodeContainedCast<BrushNode, Brush>::install(m_casts);
					NodeContainedCast<BrushNode, MapImporter>::install(m_casts);
					NodeContainedCast<BrushNode, MapExporter>::install(m_casts);
				}
				NodeTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		// The instances of this node
		InstanceSet m_instances;
		// The actual contained brush (NO reference)
		Brush m_brush;
		// The map importer/exporters
		BrushTokenImporter m_mapImporter;
		BrushTokenExporter m_mapExporter;

	public:

		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		// greebo: Returns the casted types of this node
		Snappable& get (NullType<Snappable> )
		{
			return m_brush;
		}
		TransformNode& get (NullType<TransformNode> )
		{
			return m_brush;
		}
		Brush& get (NullType<Brush> )
		{
			return m_brush;
		}
		MapImporter& get (NullType<MapImporter> )
		{
			return m_mapImporter;
		}
		MapExporter& get (NullType<MapExporter> )
		{
			return m_mapExporter;
		}
		Nameable& get (NullType<Nameable> )
		{
			return m_brush;
		}
		// Constructor
		BrushNode () :
			scene::Node(this, StaticTypeCasts::instance().get()), m_brush(*this, InstanceSetEvaluateTransform<
					BrushInstance>::Caller(m_instances), InstanceSet::BoundsChangedCaller(m_instances)), m_mapImporter(
					m_brush), m_mapExporter(m_brush)
		{
		}
		// Copy Constructor
		BrushNode (const BrushNode& other) :
			scene::Node(this, StaticTypeCasts::instance().get()), scene::Instantiable(other), scene::Cloneable(other),
					m_brush(other.m_brush, *this, InstanceSetEvaluateTransform<BrushInstance>::Caller(m_instances),
							InstanceSet::BoundsChangedCaller(m_instances)), m_mapImporter(m_brush), m_mapExporter(
							m_brush)
		{
		}

		// Returns the actual scene node
		scene::Node& node ()
		{
			return *this;
		}

		scene::Node& clone () const
		{
			return (new BrushNode(*this))->node();
		}
		// Creates a new instance on the heap
		scene::Instance* create (const scene::Path& path, scene::Instance* parent)
		{
			return new BrushInstance(path, parent, m_brush);
		}
		// Loops through all instances with the given visitor class
		void forEachInstance (const scene::Instantiable::Visitor& visitor)
		{
			m_instances.forEachInstance(visitor);
		}
		// Inserts / erases an instance
		void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
		{
			m_instances.insert(observer, path, instance);
		}
		scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path)
		{
			return m_instances.erase(observer, path);
		}
};

// Casts the node onto a Brush and returns the pointer to it
inline Brush* Node_getBrush (scene::Node& node)
{
	return NodeTypeCast<Brush>::cast(node);
}

#endif
