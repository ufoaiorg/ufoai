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
