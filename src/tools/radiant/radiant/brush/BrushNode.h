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
#include "TexDef.h"
#include "ibrush.h"
#include "editable.h"

#include "BrushTokens.h"
#include "Brush.h"
#include "BrushInstance.h"

class BrushNode: public scene::Node,
		public scene::Instantiable,
		public scene::Cloneable,
		public Nameable,
		public Snappable,
		public TransformNode,
		public MapImporter,
		public MapExporter,
		public IBrushNode
{

		// The instances of this node
		InstanceSet m_instances;
		// The actual contained brush (NO reference)
		Brush m_brush;
		// The map importer/exporters
		BrushTokenImporter m_mapImporter;
		BrushTokenExporter m_mapExporter;

	public:

		// Nameable implemenation
		std::string name () const
		{
			return "Brush";
		}

		// TransformNode implementation
		const Matrix4& localToParent () const
		{
			return m_brush.localToParent();
		}

		// MapImporter implementation
		bool importTokens (Tokeniser& tokeniser)
		{
			return m_mapImporter.importTokens(tokeniser);
		}

		// MapExporter implementation
		void exportTokens (TokenWriter& writer) const
		{
			m_mapExporter.exportTokens(writer);
		}

		// IBrushNode implementation
		Brush& getBrush ()
		{
			return m_brush;
		}

		// Constructor
		BrushNode () :
			m_brush(*this, InstanceSetEvaluateTransform<BrushInstance>::Caller(m_instances),
					InstanceSet::BoundsChangedCaller(m_instances)), m_mapImporter(m_brush), m_mapExporter(m_brush)
		{
		}
		// Copy Constructor
		BrushNode (const BrushNode& other) :
				scene::Node(other), scene::Instantiable(other), scene::Cloneable(other), Nameable(other), Snappable(
						other), TransformNode(other), MapImporter(other), MapExporter(other), IBrushNode(other), m_brush(
						other.m_brush, *this, InstanceSetEvaluateTransform<BrushInstance>::Caller(m_instances),
						InstanceSet::BoundsChangedCaller(m_instances)), m_mapImporter(m_brush), m_mapExporter(m_brush)
		{
		}

		// Snappable implementation
		void snapto (float snap)
		{
			m_brush.snapto(snap);
		}

		scene::Node& clone () const
		{
			return *(new BrushNode(*this));
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

// Casts the node onto a BrushNode and returns the Brush pointer
inline Brush* Node_getBrush (scene::Node& node)
{
	IBrushNode* brushNode = dynamic_cast<IBrushNode*> (&node);
	if (brushNode != NULL) {
		return &brushNode->getBrush();
	}
	return NULL;
}

#endif
