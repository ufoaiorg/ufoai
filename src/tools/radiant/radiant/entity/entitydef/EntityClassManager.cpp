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

#include "EntityClassManager.h"

/*!
 implementation of the EClass manager API
 */

void EntityClassManager::CleanEntityList (EntityClasses& entityClasses)
{
	for (EntityClasses::iterator i = entityClasses.begin(); i != entityClasses.end(); ++i) {
		(*i).second->free((*i).second);
	}
	entityClasses.clear();
}

void EntityClassManager::Clear ()
{
	CleanEntityList(g_entityClasses);
	g_listTypes.clear();
}

EntityClass* EntityClassManager::InsertSortedList (EntityClasses& entityClasses, EntityClass *entityClass)
{
	std::pair<EntityClasses::iterator, bool> result = entityClasses.insert(EntityClasses::value_type(
			entityClass->name(), entityClass));
	if (!result.second) {
		entityClass->free(entityClass);
	}
	return (*result.first).second;
}

EntityClass* EntityClassManager::InsertAlphabetized (EntityClass *e)
{
	return InsertSortedList(g_entityClasses, e);
}

void EntityClassManager::forEach (EntityClassVisitor& visitor)
{
	for (EntityClasses::iterator i = g_entityClasses.begin(); i != g_entityClasses.end(); ++i) {
		visitor.visit((*i).second);
	}
}

const ListAttributeType* EntityClassManager::findListType (const std::string& name)
{
	ListAttributeTypes::iterator i = g_listTypes.find(name);
	if (i != g_listTypes.end()) {
		return &(*i).second;
	}
	return 0;
}

void EntityClassManager::Construct ()
{
	GlobalEntityClassScanner()->scanFile(g_collector);
}

EntityClass* EntityClassManager::findOrInsert (const std::string& name, bool has_brushes)
{
	if (name.empty())
		return eclass_bad;

	EntityClasses::iterator i = g_entityClasses.find(name);
	if (i != g_entityClasses.end() && i->first == name)
		return i->second;

	EntityClass* e = EntityClass_Create_Default(name, has_brushes);
	return InsertAlphabetized(e);
}

void EntityClassManager::realise ()
{
	if (--m_unrealised == 0) {
		Construct();
		m_observers.realise();
	}
}
void EntityClassManager::unrealise ()
{
	if (++m_unrealised == 1) {
		m_observers.unrealise();
		Clear();
	}
}
void EntityClassManager::attach (ModuleObserver& observer)
{
	m_observers.attach(observer);
}
void EntityClassManager::detach (ModuleObserver& observer)
{
	m_observers.detach(observer);
}

EntityClassManager::EntityClassManager () :
	g_collector(this), eclass_bad(0), m_unrealised(3)
{
	// start by creating the default unknown eclass
	eclass_bad = EClass_Create("UNKNOWN_CLASS", Vector3(0.0f, 0.5f, 0.0f), "");

	realise();
}

EntityClassManager::~EntityClassManager ()
{
	unrealise();

	eclass_bad->free(eclass_bad);
}
