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
#include "scenelib.h"

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
				virtual void insert (const std::string& key, EntityKeyValue& value) = 0;
				virtual void erase (const std::string& key, EntityKeyValue& value) = 0;
				virtual void clear ()
				{
				}
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

inline Entity* Node_getEntity (scene::Node& node)
{
	return NodeTypeCast<Entity>::cast(node);
}

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

class Counter;

class EntityCreator
{
	public:
		INTEGER_CONSTANT(Version, 2);
		STRING_CONSTANT(Name, "entity");

		virtual ~EntityCreator ()
		{
		}

		virtual scene::Node& createEntity (EntityClass* eclass) = 0;

		typedef void (*KeyValueChangedFunc) ();
		virtual void setKeyValueChangedFunc (KeyValueChangedFunc func) = 0;

		virtual void setCounter (Counter* counter) = 0;

		virtual void connectEntities (const scene::Path& e1, const scene::Path& e2) = 0;

		virtual void setLightRadii (bool lightRadii) = 0;
		virtual bool getLightRadii () = 0;
		virtual void setForceLightRadii (bool forceLightRadii) = 0;
		virtual bool getForceLightRadii () = 0;
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
