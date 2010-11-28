/**
 * @file eclass.cpp
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

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

#include "debugging/debugging.h"

#include <map>

#include "ifilesystem.h"

#include "string/string.h"
#include "eclasslib.h"
#include "os/path.h"
#include "os/dir.h"
#include "stream/stringstream.h"
#include "moduleobservers.h"

#include "../mainframe.h"

namespace
{
	typedef std::map<std::string, EntityClass*, RawStringLessNoCase> EntityClasses;
	EntityClasses g_entityClasses;
	EntityClass *eclass_bad = 0;
	typedef std::map<std::string, ListAttributeType> ListAttributeTypes;
	ListAttributeTypes g_listTypes;
}

EClassModules& EntityClassManager_getEClassModules ();

/*!
 implementation of the EClass manager API
 */

void CleanEntityList (EntityClasses& entityClasses)
{
	for (EntityClasses::iterator i = entityClasses.begin(); i != entityClasses.end(); ++i) {
		(*i).second->free((*i).second);
	}
	entityClasses.clear();
}

void Eclass_Clear ()
{
	CleanEntityList(g_entityClasses);
	g_listTypes.clear();
}

EntityClass* EClass_InsertSortedList (EntityClasses& entityClasses, EntityClass *entityClass)
{
	std::pair<EntityClasses::iterator, bool> result = entityClasses.insert(EntityClasses::value_type(
			entityClass->name(), entityClass));
	if (!result.second) {
		entityClass->free(entityClass);
	}
	return (*result.first).second;
}

EntityClass* Eclass_InsertAlphabetized (EntityClass *e)
{
	return EClass_InsertSortedList(g_entityClasses, e);
}

void Eclass_forEach (EntityClassVisitor& visitor)
{
	for (EntityClasses::iterator i = g_entityClasses.begin(); i != g_entityClasses.end(); ++i) {
		visitor.visit((*i).second);
	}
}

class RadiantEclassCollector: public EntityClassCollector
{
	public:
		void insert (EntityClass* eclass)
		{
			Eclass_InsertAlphabetized(eclass);
		}
		void insert (const char* name, const ListAttributeType& list)
		{
			g_listTypes.insert(ListAttributeTypes::value_type(name, list));
		}
};

RadiantEclassCollector g_collector;

const ListAttributeType* EntityClass_findListType (const std::string& name)
{
	ListAttributeTypes::iterator i = g_listTypes.find(name);
	if (i != g_listTypes.end()) {
		return &(*i).second;
	}
	return 0;
}

void EntityClassUFO_Construct ()
{
	/** @todo remove this visitor - we only have one entity def parser */
	class LoadEntityDefinitionsVisitor: public EClassModules::Visitor
	{
		public:
			LoadEntityDefinitionsVisitor ()
			{
			}

			void visit (const std::string& name, const EntityClassScanner& table) const
			{
				g_message("Load entity definition file from '%s'\n", table.getFilename().c_str());
				table.scanFile(g_collector, table.getFilename());
			}
	};

	EntityClassManager_getEClassModules().foreachModule(LoadEntityDefinitionsVisitor());
}

EntityClass* Eclass_ForName (const std::string& name, bool has_brushes)
{
	if (name.empty())
		return eclass_bad;

	EntityClasses::iterator i = g_entityClasses.find(name);
	if (i != g_entityClasses.end() && i->first == name)
		return i->second;

	EntityClass* e = EntityClass_Create_Default(name, has_brushes);
	return Eclass_InsertAlphabetized(e);
}

class EntityClassUFO: public ModuleObserver
{
		std::size_t m_unrealised;
		ModuleObservers m_observers;
	public:
		EntityClassUFO () :
			m_unrealised(3)
		{
		}
		void realise ()
		{
			if (--m_unrealised == 0) {
				EntityClassUFO_Construct();
				m_observers.realise();
			}
		}
		void unrealise ()
		{
			if (++m_unrealised == 1) {
				m_observers.unrealise();
				Eclass_Clear();
			}
		}
		void attach (ModuleObserver& observer)
		{
			m_observers.attach(observer);
		}
		void detach (ModuleObserver& observer)
		{
			m_observers.detach(observer);
		}
};

EntityClassUFO g_EntityClassUFO;

void EntityClass_attach (ModuleObserver& observer)
{
	g_EntityClassUFO.attach(observer);
}
void EntityClass_detach (ModuleObserver& observer)
{
	g_EntityClassUFO.detach(observer);
}

void EntityClass_realise ()
{
	g_EntityClassUFO.realise();
}
void EntityClass_unrealise ()
{
	g_EntityClassUFO.unrealise();
}

void EntityClassUFO_construct ()
{
	// start by creating the default unknown eclass
	eclass_bad = EClass_Create("UNKNOWN_CLASS", Vector3(0.0f, 0.5f, 0.0f), "");

	EntityClass_realise();
}

void EntityClassUFO_destroy ()
{
	EntityClass_unrealise();

	eclass_bad->free(eclass_bad);
}

#include "modulesystem/modulesmap.h"

class EntityClassUFODependencies: public GlobalRadiantModuleRef,
		public GlobalFileSystemModuleRef,
		public GlobalShaderCacheModuleRef
{
		EClassModulesRef m_eclass_modules;
	public:
		EntityClassUFODependencies () :
			m_eclass_modules("def")
		{
		}
		EClassModules& getEClassModules ()
		{
			return m_eclass_modules.get();
		}
};

class EclassManagerAPI
{
		EntityClassManager m_eclassmanager;
	public:
		typedef EntityClassManager Type;
		STRING_CONSTANT(Name, "*");

		EclassManagerAPI ()
		{
			EntityClassUFO_construct();

			m_eclassmanager.findOrInsert = &Eclass_ForName;
			m_eclassmanager.findListType = &EntityClass_findListType;
			m_eclassmanager.forEach = &Eclass_forEach;
			m_eclassmanager.attach = &EntityClass_attach;
			m_eclassmanager.detach = &EntityClass_detach;
			m_eclassmanager.realise = &EntityClass_realise;
			m_eclassmanager.unrealise = &EntityClass_unrealise;

			GlobalRadiant().attachGameToolsPathObserver(g_EntityClassUFO);
			GlobalRadiant().attachGameModeObserver(g_EntityClassUFO);
		}
		~EclassManagerAPI ()
		{
			GlobalRadiant().detachGameModeObserver(g_EntityClassUFO);
			GlobalRadiant().detachGameToolsPathObserver(g_EntityClassUFO);

			EntityClassUFO_destroy();
		}
		EntityClassManager* getTable ()
		{
			return &m_eclassmanager;
		}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<EclassManagerAPI, EntityClassUFODependencies> EclassManagerModule;
typedef Static<EclassManagerModule> StaticEclassManagerModule;
StaticRegisterModule staticRegisterEclassManager(StaticEclassManagerModule::instance());

EClassModules& EntityClassManager_getEClassModules ()
{
	return StaticEclassManagerModule::instance().getDependencies().getEClassModules();
}
