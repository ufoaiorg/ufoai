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

#if !defined (INCLUDED_MAPLIB_H)
#define INCLUDED_MAPLIB_H

#include "nameable.h"
#include "mapfile.h"

#include "traverselib.h"
#include "transformlib.h"
#include "scenelib.h"
#include "string/string.h"
#include "instancelib.h"
#include "selectionlib.h"
#include "generic/callback.h"

class NameableString: public Nameable
{
		std::string m_name;
	public:
		NameableString (const std::string& name) :
			m_name(name)
		{
		}

		std::string name () const
		{
			return m_name;
		}
		void attach (const NameCallback& callback)
		{
		}
		void detach (const NameCallback& callback)
		{
		}
};

const std::size_t MAPFILE_MAX_CHANGES = std::numeric_limits<std::size_t>::max();

class UndoFileChangeTracker: public UndoTracker, public MapFile
{
		std::size_t m_size;
		std::size_t m_saved;
		typedef void (UndoFileChangeTracker::*Pending) ();
		Pending m_pending;
		Callback m_changed;

	public:
		UndoFileChangeTracker () :
			m_size(0), m_saved(MAPFILE_MAX_CHANGES), m_pending(0)
		{
		}
		void print ()
		{
			globalOutputStream() << "saved: " << string::toString(m_saved) << " size: " << string::toString(m_size) << "\n";
		}

		void push ()
		{
			++m_size;
			m_changed();
			//print();
		}
		void pop ()
		{
			--m_size;
			m_changed();
			//print();
		}
		void pushOperation ()
		{
			if (m_size < m_saved) {
				// redo queue has been flushed.. it is now impossible to get back to the saved state via undo/redo
				m_saved = MAPFILE_MAX_CHANGES;
			}
			push();
		}
		void clear ()
		{
			m_size = 0;
			m_changed();
			//print();
		}
		void clearRedo (void)
		{
			// do nothing special
		}
		void begin ()
		{
			m_pending = Pending(&UndoFileChangeTracker::pushOperation);
		}
		void undo ()
		{
			m_pending = Pending(&UndoFileChangeTracker::pop);
		}
		void redo ()
		{
			m_pending = Pending(&UndoFileChangeTracker::push);
		}

		void changed ()
		{
			if (m_pending != 0) {
				((*this).*m_pending)();
				m_pending = 0;
			}
		}

		void save ()
		{
			m_saved = m_size;
			m_changed();
		}
		bool saved () const
		{
			return m_saved == m_size;
		}

		void setChangedCallback (const Callback& changed)
		{
			m_changed = changed;
			m_changed();
		}

		std::size_t changes () const
		{
			return m_size;
		}
};

class MapRoot: public scene::Node,
		public scene::Instantiable,
		public scene::Traversable::Observer,
		public Nameable,
		public TransformNode,
		public MapFile
{
		class TypeCasts
		{
				NodeTypeCastTable m_casts;
			public:
				TypeCasts ()
				{
					NodeContainedCast<MapRoot, scene::Traversable>::install(m_casts);
				}
				NodeTypeCastTable& get ()
				{
					return m_casts;
				}
		};

		IdentityTransform m_transform;
		TraversableNodeSet m_traverse;
		InstanceSet m_instances;
		typedef SelectableInstance Instance;
		NameableString m_name;
		UndoFileChangeTracker m_changeTracker;
	public:
		typedef LazyStatic<TypeCasts> StaticTypeCasts;

		scene::Traversable& get (NullType<scene::Traversable> )
		{
			return m_traverse;
		}

		// TransformNode implementation
		const Matrix4& localToParent() const {
			return m_transform.localToParent();
		}

		// Nameable implementation
		std::string name() const {
			return m_name.name();
		}

		// MapFile implementation
		virtual void save() {
			m_changeTracker.save();
		}
		virtual bool saved() const {
			return m_changeTracker.saved();
		}
		virtual void changed() {
			m_changeTracker.changed();
		}
		virtual void setChangedCallback(const Callback& changed) {
			m_changeTracker.setChangedCallback(changed);
		}
		virtual std::size_t changes() const {
			return m_changeTracker.changes();
		}

		MapRoot (const std::string& name) :
			scene::Node(this, StaticTypeCasts::instance().get()), m_name(name)
		{
			m_isRoot = true;

			m_traverse.attach(this);

			GlobalUndoSystem().trackerAttach(m_changeTracker);
		}
		~MapRoot ()
		{
			GlobalUndoSystem().trackerDetach(m_changeTracker);
			m_traverse.detach(this);
		}
		scene::Node& node ()
		{
			return *this;
		}

		InstanceCounter m_instanceCounter;
		void instanceAttach (const scene::Path& path)
		{
			if (++m_instanceCounter.m_count == 1) {
				m_traverse.instanceAttach(path_find_mapfile(path.begin(), path.end()));
			}
		}
		void instanceDetach (const scene::Path& path)
		{
			if (--m_instanceCounter.m_count == 0) {
				m_traverse.instanceDetach(path_find_mapfile(path.begin(), path.end()));
			}
		}

		void insert (scene::Node& child)
		{
			m_instances.insert(child);
		}
		void erase (scene::Node& child)
		{
			m_instances.erase(child);
		}

		scene::Node& clone () const
		{
			return (new MapRoot(*this))->node();
		}

		scene::Instance* create (const scene::Path& path, scene::Instance* parent)
		{
			return new Instance(path, parent);
		}
		void forEachInstance (const scene::Instantiable::Visitor& visitor)
		{
			m_instances.forEachInstance(visitor);
		}
		void insert (scene::Instantiable::Observer* observer, const scene::Path& path, scene::Instance* instance)
		{
			m_instances.insert(observer, path, instance);
			instanceAttach(path);
		}
		scene::Instance* erase (scene::Instantiable::Observer* observer, const scene::Path& path)
		{
			instanceDetach(path);
			return m_instances.erase(observer, path);
		}
};

inline void MapRoot_Construct ()
{
}

inline void MapRoot_Destroy ()
{
}

inline NodeSmartReference NewMapRoot (const std::string& name)
{
	return NodeSmartReference((new MapRoot(name))->node());
}

#endif
