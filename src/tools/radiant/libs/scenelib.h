/**
 * @file scenelib.h
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

#if !defined (INCLUDED_SCENELIB_H)
#define INCLUDED_SCENELIB_H

#include "Bounded.h"
#include "inode.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "ientity.h"
#include "ibrush.h"

#include <cstddef>
#include <string.h>

#include "math/aabb.h"
#include "transformlib.h"
#include "generic/callback.h"
#include "generic/reference.h"
#include "container/stack.h"

class Selector;
class SelectionTest;
class VolumeTest;
template<typename Element> class BasicVector3;
typedef BasicVector3<float> Vector3;
template<typename Element> class BasicVector4;
typedef BasicVector4<float> Vector4;
class Matrix4;
typedef Vector4 Quaternion;
class AABB;

class ComponentSelectionTestable
{
	public:
		STRING_CONSTANT(Name, "ComponentSelectionTestable");

		virtual ~ComponentSelectionTestable ()
		{
		}

		virtual bool isSelectedComponents () const = 0;
		virtual void setSelectedComponents (bool select, SelectionSystem::EComponentMode mode) = 0;
		virtual void testSelectComponents (Selector& selector, SelectionTest& test,
				SelectionSystem::EComponentMode mode) = 0;
};

class ComponentEditable
{
	public:
		STRING_CONSTANT(Name, "ComponentEditable");

		virtual ~ComponentEditable ()
		{
		}
		virtual const AABB& getSelectedComponentsBounds () const = 0;
};

class ComponentSnappable
{
	public:
		STRING_CONSTANT(Name, "ComponentSnappable");

		virtual ~ComponentSnappable ()
		{
		}
		virtual void snapComponents (float snap) = 0;
};

namespace scene
{
	class Node : public INode
	{
		public:
		enum
		{
			eVisible = 0,
			eHidden = 1 << 0, // manually hidden by the user
			eFiltered = 1 << 1, // excluded due to filter settings
			eExcluded = 1 << 2 // excluded due to regioning
		};

		private:

			unsigned int _state;
			std::size_t _refcount;
			bool _isRoot;

		public:

			bool isRoot () const
			{
				return _isRoot;
			}

			void setIsRoot(bool isRoot)
			{
				_isRoot = isRoot;
			}

			Node () :
				_state(eVisible), _refcount(0), _isRoot(false)
			{
			}

			virtual ~Node ()
			{
			}

			virtual void release ()
			{
				// Default implementation: remove this node
				delete this;
			}

			void IncRef ()
			{
				ASSERT_MESSAGE(_refcount < (1 << 24), "Node::decref: uninitialised refcount");
				++_refcount;
			}
			void DecRef ()
			{
				ASSERT_MESSAGE(_refcount < (1 << 24), "Node::decref: uninitialised refcount");
				if (--_refcount == 0) {
					release();
				}
			}
			std::size_t getReferenceCount () const
			{
				return _refcount;
			}

			void enable (unsigned int state)
			{
				_state |= state;
			}
			void disable (unsigned int state)
			{
				_state &= ~state;
			}
			bool visible () const
			{
				return _state == eVisible;
			}
			bool excluded () const
			{
				return (_state & eExcluded) != 0;
			}

			/**
			 * Return the filtered status of this Instance.
			 */
			virtual bool isFiltered() const {
				return (_state & eFiltered) != 0;
			}

			/**
			 * Set the filtered status of this Node. Setting filtered to true will
			 * prevent the node from being rendered.
			 */
			virtual void setFiltered(bool filtered) {
				if (filtered) {
					_state |= eFiltered;
				}
				else {
					_state &= ~eFiltered;
				}
			}
	};
}

inline scene::Instantiable* Node_getInstantiable (scene::Node& node)
{
	return dynamic_cast<scene::Instantiable*>(&node);
}

inline scene::Traversable* Node_getTraversable (scene::Node& node)
{
	return dynamic_cast<scene::Traversable*>(&node);
}

inline void Node_traverseSubgraph (scene::Node& node, const scene::Traversable::Walker& walker)
{
	if (walker.pre(node)) {
		scene::Traversable* traversable = Node_getTraversable(node);
		if (traversable != 0) {
			traversable->traverse(walker);
		}
	}
	walker.post(node);
}

inline TransformNode* Node_getTransformNode (scene::Node& node)
{
	return dynamic_cast<TransformNode*>(&node);
}

inline bool operator< (scene::Node& node, scene::Node& other)
{
	return &node < &other;
}
inline bool operator== (scene::Node& node, scene::Node& other)
{
	return &node == &other;
}
inline bool operator!= (scene::Node& node, scene::Node& other)
{
	return !::operator==(node, other);
}

inline scene::Node& NewNullNode ()
{
	return *(new scene::Node);
}

inline void Path_deleteTop (const scene::Path& path)
{
	Node_getTraversable(path.parent())->erase(path.top());
}

class delete_all: public scene::Traversable::Walker
{
		scene::Node& m_parent;
	public:
		delete_all (scene::Node& parent) :
			m_parent(parent)
		{
		}
		bool pre (scene::Node& node) const
		{
			return false;
		}
		void post (scene::Node& node) const
		{
			Node_getTraversable(m_parent)->erase(node);
		}
};

inline void DeleteSubgraph (scene::Node& subgraph)
{
	Node_getTraversable(subgraph)->traverse(delete_all(subgraph));
}

inline Entity* Node_getEntity(scene::Node& node) {
	EntityNode* entityNode = dynamic_cast<EntityNode*>(&node);
	if (entityNode != NULL) {
		return &entityNode->getEntity();
	}
	return NULL;
}

inline bool Node_isEntity (scene::Node& node)
{
	return dynamic_cast<EntityNode*>(&node) != NULL;
}

template<typename Functor>
class EntityWalker: public scene::Graph::Walker
{
		const Functor& functor;
	public:
		EntityWalker (const Functor& functor) :
			functor(functor)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (Node_isEntity(path.top())) {
				functor(instance);
				return false;
			}
			return true;
		}
};

template<typename Functor>
inline const Functor& Scene_forEachEntity (const Functor& functor)
{
	GlobalSceneGraph().traverse(EntityWalker<Functor> (functor));
	return functor;
}

// checks whether the given node reference is a brush node
inline bool Node_isBrush (scene::Node& node)
{
	return dynamic_cast<IBrushNode*>(&node) != NULL;
}

// checks whether the given node reference is a primitive
inline bool Node_isPrimitive (scene::Node& node)
{
	return Node_isBrush(node);
}

class ParentBrushes: public scene::Traversable::Walker
{
		scene::Node& m_parent;
	public:
		ParentBrushes (scene::Node& parent) :
			m_parent(parent)
		{
		}
		bool pre (scene::Node& node) const
		{
			return false;
		}
		void post (scene::Node& node) const
		{
			if (Node_isPrimitive(node)) {
				Node_getTraversable(m_parent)->insert(node);
			}
		}
};

inline void parentBrushes (scene::Node& subgraph, scene::Node& parent)
{
	Node_getTraversable(subgraph)->traverse(ParentBrushes(parent));
}

class HasBrushes: public scene::Traversable::Walker
{
		bool& m_hasBrushes;
	public:
		HasBrushes (bool& hasBrushes) :
			m_hasBrushes(hasBrushes)
		{
			m_hasBrushes = true;
		}
		bool pre (scene::Node& node) const
		{
			if (!Node_isPrimitive(node)) {
				m_hasBrushes = false;
			}
			return false;
		}
};

inline bool node_is_group (scene::Node& node)
{
	scene::Traversable* traversable = Node_getTraversable(node);
	if (traversable != 0) {
		bool hasBrushes = false;
		traversable->traverse(HasBrushes(hasBrushes));
		return hasBrushes;
	}
	return false;
}

namespace scene
{
	class Instance
	{
		Path m_path;
		Instance* m_parent;

		mutable Matrix4 m_local2world;
		mutable AABB m_bounds;
		mutable AABB m_childBounds;
		mutable bool m_transformChanged;
		mutable bool m_transformMutex;
		mutable bool m_boundsChanged;
		mutable bool m_boundsMutex;
		mutable bool m_childBoundsChanged;
		mutable bool m_childBoundsMutex;
		mutable bool m_isSelected;
		mutable bool m_isSelectedChanged;
		mutable bool m_childSelected;
		mutable bool m_childSelectedChanged;
		mutable bool m_parentSelected;
		mutable bool m_parentSelectedChanged;
		Callback m_childSelectedChangedCallback;
		Callback m_transformChangedCallback;

		// Filtered status
		bool _filtered;

	private:
		void evaluateTransform () const;
		void evaluateChildBounds () const;
		void evaluateBounds () const;

		Instance (const scene::Instance& other);
		Instance& operator= (const scene::Instance& other);
	public:
		Instance (const scene::Path& path, Instance* parent);
		virtual ~Instance ();
		const scene::Path& path () const;
		bool visible () const;
		const Matrix4& localToWorld () const;
		void transformChangedLocal ();
		void transformChanged ();
		void setTransformChangedCallback (const Callback& callback);
		const AABB& worldAABB () const;
		const AABB& childBounds () const;
		void boundsChanged ();
		void childSelectedChanged ();
		bool childSelected () const;
		void setChildSelectedChangedCallback (const Callback& callback);
		void selectedChanged ();
		bool isSelected () const;
		void parentSelectedChanged ();
		bool parentSelected () const;
		Instance* getParent () const;
	};
}

template<typename Functor>
class InstanceWalker: public scene::Graph::Walker
{
		const Functor& m_functor;
	public:
		InstanceWalker (const Functor& functor) :
			m_functor(functor)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			m_functor(instance);
			return true;
		}
};

template<typename Functor>
class ChildInstanceWalker: public scene::Graph::Walker
{
		const Functor& m_functor;
		mutable std::size_t m_depth;
	public:
		ChildInstanceWalker (const Functor& functor) :
			m_functor(functor), m_depth(0)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			if (m_depth == 1) {
				m_functor(instance);
			}
			return ++m_depth != 2;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			--m_depth;
		}
};

template<typename Type, typename Functor>
class InstanceApply: public Functor
{
	public:
		InstanceApply (const Functor& functor) :
			Functor(functor)
		{
		}
		void operator() (scene::Instance& instance) const
		{
			Type* result = dynamic_cast<Type*>(&instance);
			if (result != NULL) {
				Functor::operator()(*result);
			}
		}
};

inline Selectable* Instance_getSelectable (scene::Instance& instance)
{
	return dynamic_cast<Selectable*>(&instance);
}
inline const Selectable* Instance_getSelectable (const scene::Instance& instance)
{
	return dynamic_cast<const Selectable*>(&instance);
}

class AnyInstanceSelected: public scene::Instantiable::Visitor
{
		bool& m_selected;
	public:
		AnyInstanceSelected (bool& selected) :
			m_selected(selected)
		{
			m_selected = false;
		}
		void visit (scene::Instance& instance) const
		{
			Selectable* selectable = Instance_getSelectable(instance);
			if (selectable != 0 && selectable->isSelected()) {
				m_selected = true;
			}
		}
};

inline bool Node_instanceSelected (scene::Node& node)
{
	scene::Instantiable* instantiable = Node_getInstantiable(node);
	bool selected;
	instantiable->forEachInstance(AnyInstanceSelected(selected));
	return selected;
}

class SelectedDescendantWalker: public scene::Traversable::Walker
{
		bool& m_selected;
	public:
		SelectedDescendantWalker (bool& selected) :
			m_selected(selected)
		{
			m_selected = false;
		}

		bool pre (scene::Node& node) const
		{
			if (node.isRoot()) {
				return false;
			}

			if (Node_instanceSelected(node)) {
				m_selected = true;
			}

			return true;
		}
};

inline bool Node_selectedDescendant (scene::Node& node)
{
	bool selected;
	Node_traverseSubgraph(node, SelectedDescendantWalker(selected));
	return selected;
}

template<typename Functor>
inline void Scene_forEachChildSelectable (const Functor& functor, const scene::Path& path)
{
	GlobalSceneGraph().traverse_subgraph(ChildInstanceWalker<InstanceApply<Selectable, Functor> > (functor), path);
}

class SelectableSetSelected
{
		bool m_selected;
	public:
		SelectableSetSelected (bool selected) :
			m_selected(selected)
		{
		}
		void operator() (Selectable& selectable) const
		{
			selectable.setSelected(m_selected);
		}
};

inline Transformable* Instance_getTransformable (scene::Instance& instance)
{
	return dynamic_cast<Transformable*>(&instance);
}
inline const Transformable* Instance_getTransformable (const scene::Instance& instance)
{
	return dynamic_cast<const Transformable*>(&instance);
}

inline ComponentSelectionTestable* Instance_getComponentSelectionTestable (scene::Instance& instance)
{
	return dynamic_cast<ComponentSelectionTestable*>(&instance);
}

inline ComponentEditable* Instance_getComponentEditable (scene::Instance& instance)
{
	return dynamic_cast<ComponentEditable*>(&instance);
}

inline ComponentSnappable* Instance_getComponentSnappable (scene::Instance& instance)
{
	return dynamic_cast<ComponentSnappable*>(&instance);
}

inline void Instance_setSelected (scene::Instance& instance, bool selected)
{
	Selectable* selectable = Instance_getSelectable(instance);
	if (selectable != 0) {
		selectable->setSelected(selected);
	}
}

inline bool Instance_isSelected (scene::Instance& instance)
{
	Selectable* selectable = Instance_getSelectable(instance);
	if (selectable != 0) {
		return selectable->isSelected();
	}
	return false;
}

inline scene::Instance& findInstance (const scene::Path& path)
{
	scene::Instance* instance = GlobalSceneGraph().find(path);
	ASSERT_MESSAGE(instance != 0, "findInstance: path not found in scene-graph");
	return *instance;
}

inline void selectPath (const scene::Path& path, bool selected)
{
	Instance_setSelected(findInstance(path), selected);
}

class SelectChildren: public scene::Traversable::Walker
{
		mutable scene::Path m_path;
	public:
		SelectChildren (const scene::Path& root) :
			m_path(root)
		{
		}
		~SelectChildren ()
		{
		}
		bool pre (scene::Node& node) const
		{
			m_path.push(makeReference(node));
			selectPath(m_path, true);
			return false;
		}
		void post (scene::Node& node) const
		{
			m_path.pop();
		}
};

class InstanceCounter
{
	public:
		unsigned int m_count;
		InstanceCounter () :
			m_count(0)
		{
		}
};

class Counter
{
	public:
		virtual ~Counter ()
		{
		}
		virtual void increment () = 0;
		virtual void decrement () = 0;

		virtual std::size_t get () const = 0;
};

// greebo: These tool methods have been moved from map.cpp, they might come in handy
enum ENodeType
{
	eNodeUnknown, eNodeMap, eNodeEntity, eNodePrimitive
};

inline const char* nodetype_get_name (ENodeType type)
{
	if (type == eNodeMap)
		return "map";
	if (type == eNodeEntity)
		return "entity";
	if (type == eNodePrimitive)
		return "primitive";
	return "unknown";
}

inline ENodeType node_get_nodetype (scene::Node& node)
{
	if (Node_isEntity(node)) {
		return eNodeEntity;
	}
	if (Node_isPrimitive(node)) {
		return eNodePrimitive;
	}
	return eNodeUnknown;
}

#include "generic/callback.h"

template<typename Contained>
class ConstReference;
typedef ConstReference<scene::Path> PathConstReference;

#include "generic/referencecounted.h"
typedef SmartReference<scene::Node, IncRefDecRefCounter<scene::Node> > NodeSmartReference;

#endif
