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

#if !defined(INCLUDED_IENTITY_H)
#define INCLUDED_IENTITY_H

#include <string>
#include "generic/constant.h"

#include "string/string.h"
#include "generic/callbackfwd.h"
#include <string>

class EntityClass;

typedef Callback1<const std::string&> KeyObserver;

class EntityKeyValue
{
	public:
		virtual ~EntityKeyValue ()
		{
		}
		virtual const std::string& get () const = 0;
		virtual void assign (const std::string& other) = 0;
		virtual void attach (const KeyObserver& observer) = 0;
		virtual void detach (const KeyObserver& observer) = 0;
};

class Entity
{
	public:
		STRING_CONSTANT(Name, "Entity");

		class Observer
		{
			public:
				virtual ~Observer ()
				{
				}
				virtual void onKeyInsert (const std::string& key, EntityKeyValue& value) = 0;
				virtual void onKeyErase (const std::string& key, EntityKeyValue& value) = 0;
		};

		class Visitor
		{
			public:
				virtual ~Visitor ()
				{
				}
				virtual void visit (const std::string& key, const std::string& value) = 0;
		};

		virtual ~Entity ()
		{
		}

		virtual const EntityClass& getEntityClass () const = 0;
		virtual void forEachKeyValue (Visitor& visitor) const = 0;

		/** Set a key value on this entity. Setting the value to "" will
		 * remove the key.
		 */

		virtual void setKeyValue (const std::string& key, const std::string& value) = 0;

		virtual std::string getKeyValue (const std::string& key) const = 0;

		virtual void addMandatoryKeyValues () = 0;
		virtual bool isContainer () const = 0;
		virtual void attach (Observer& observer) = 0;
		virtual void detach (Observer& observer) = 0;
};

class EntityNode
{
public:
	virtual ~EntityNode() {}

	/** greebo: Temporary workaround for entity-containing nodes.
	 * 			This is only used by Node_getEntity to retrieve the
	 * 			contained entity from a node.
	 */
	virtual Entity& getEntity() = 0;
};

template<typename value_type>
class Stack;
template<typename Contained>
class Reference;

namespace scene
{
	class Node;
}

typedef Reference<scene::Node> NodeReference;

namespace scene
{
	typedef Stack<NodeReference> Path;
}

class EntityCreator
{
	public:
		INTEGER_CONSTANT(Version, 2);
		STRING_CONSTANT(Name, "entity");

		virtual ~EntityCreator ()
		{
		}

		virtual scene::Node& createEntity (EntityClass* eclass) = 0;

		typedef void (*KeyValueChangedFunc)();
		virtual void setKeyValueChangedFunc(KeyValueChangedFunc func) = 0;

		virtual void connectEntities (const scene::Path& e1, const scene::Path& e2) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<EntityCreator> GlobalEntityModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<EntityCreator> GlobalEntityModuleRef;

inline EntityCreator& GlobalEntityCreator ()
{
	return GlobalEntityModule::getTable();
}

#endif
