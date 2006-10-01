/*
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

#include "ufoai_level.h"
#include "ufoai_filters.h"

#include "ifilter.h"
#include "ibrush.h"
#include "ientity.h"

#include "scenelib.h"
#include "entitylib.h"
#include "string/string.h"
#include <list>

class Level;

/**
 * @brief Let us filter some entities given by name
 */
class filter_ufoai_entity_by_classname : public EntityFilter
{
	const char* m_classname;
public:
	filter_ufoai_entity_by_classname(const char* classname) : m_classname(classname)
	{
	}
	bool filter(const Entity& entity) const
	{
		return string_equal(entity.getKeyValue("classname"), m_classname);
	}
};

/**
 * @brief Let us filter the level flags
 */
class filter_ufoai_level : public FaceFilter
{
	int m_level;
public:
	filter_ufoai_level(int level) : m_level(level)
	{
	}
	bool filter(const Face& face) const
	{
//		return string_equal_n(face.getKeyValue("classname"), m_classgroup, m_length);
		return true;
	}
};

filter_ufoai_level g_filter_level_1(1);

/**
 * @brief
 */
void UFOAI_InitFilters()
{
	add_level_filter(g_filter_level_1, 0x100);
	//_QERFaceData
}

/**
 * @brief find entities by class
 * @note from radiant/map.cpp
 */
class EntityFindByClassname : public scene::Graph::Walker
{
	const char* m_name;
	Entity*& m_entity;
public:
	EntityFindByClassname(const char* name, Entity*& entity) : m_name(name), m_entity(entity)
	{
		m_entity = 0;
	}
	bool pre(const scene::Path& path, scene::Instance& instance) const
	{
		if(m_entity == 0)
		{
			Entity* entity = Node_getEntity(path.top());
			if(entity != 0 && string_equal(m_name, entity->getKeyValue("classname")))
			{
				m_entity = entity;
			}
		}
		return true;
	}
};

/**
 * @brief
 */
Entity* Scene_FindEntityByClass(const char* name)
{
	Entity* entity;
	GlobalSceneGraph().traverse(EntityFindByClassname(name, entity));
	return entity;
}
