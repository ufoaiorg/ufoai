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

#include "ibrush.h"
#include "ientity.h"
#include "iscenegraph.h"

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
	Entity* entity = NULL;
	GlobalSceneGraph().traverse(EntityFindByClassname(name, entity));
	return entity;
}

/**
 * @brief finds start positions
 */
class EntityFindTeams : public scene::Graph::Walker
{
	const char *m_classname;
	int *m_count;
	int *m_team;

public:
	EntityFindTeams(const char *classname, int *count, int *team) : m_classname(classname), m_count(count), m_team(team)
	{
	}
	bool pre(const scene::Path& path, scene::Instance& instance) const
	{
		const char *str;
		Entity* entity = Node_getEntity(path.top());
		if(entity != 0 && string_equal(m_classname, entity->getKeyValue("classname")))
		{
			if (m_count)
				(*m_count)++;
			// now get the highest teamnum
			if (m_team)
			{
				str = entity->getKeyValue("team");
				if (!string_empty(str))
				{
					if (atoi(str) > *m_team)
						(*m_team) = atoi(str);
				}
			}
		}
		return true;
	}
};

/**
 * @brief
 */
void get_team_count (const char *classname, int *count, int *team)
{
	GlobalSceneGraph().traverse(EntityFindTeams(classname, count, team));
	globalOutputStream() << "UFO:AI: classname: " << classname << ": #" << (*count) << "\n";
}

/**
 * @brief Some default values to worldspawn like maxlevel, maxteams and so on
 */
void assign_default_values_to_worldspawn (bool day)
{
	Entity* worldspawn;
	int teams = 0;
	int count = 0;
	char str[64];

	worldspawn = Scene_FindEntityByClass("worldspawn");
	if (!worldspawn)
	{
		globalOutputStream() << "UFO:AI: Could not find worldspawn.\n";
		return;
	}

	*str = '\0';

	get_team_count("info_player_start", &count, &teams);

	// TODO: Get highest brush - a level has 64 units
	worldspawn->setKeyValue("maxlevel", "5");

	snprintf(str, sizeof(str), "%i", teams);
	worldspawn->setKeyValue("maxteams", str);

	if (day)
	{
		worldspawn->setKeyValue("light", "160");
		worldspawn->setKeyValue("_color", "1 0.8 0.8");
		worldspawn->setKeyValue("angles", "30 210");
		worldspawn->setKeyValue("ambient", "0.4 0.4 0.4");
	}
	else
	{
		worldspawn->setKeyValue("light", "60");
		worldspawn->setKeyValue("_color", "0.8 0.8 1");
		worldspawn->setKeyValue("angles", "15 60");
		worldspawn->setKeyValue("ambient", "0.25 0.25 0.275");
	}
}

/**
 * @brief Will check e.g. the map entities for valid values
 */
void check_map_values (char **returnMsg)
{
	static char message[1024];
	int count = 0;
	int teams = 0;
	Entity* worldspawn;

	worldspawn = Scene_FindEntityByClass("worldspawn");
	if (!worldspawn)
	{
		globalOutputStream() << "UFO:AI: Could not find worldspawn.\n";
		return;
	}

	*message = '\0';

	get_team_count("info_player_start", &count, &teams);
	if (!count)
		strncat(message, "No multiplayer start positions (info_player_start)\n", sizeof(message));
	else if (string_empty(worldspawn->getKeyValue("maxteams")))
		strncat(message, "Worldspawn: No maxteams defined (#info_player_start)\n", sizeof(message));
	else if (teams != atoi(worldspawn->getKeyValue("maxteams")))
		snprintf(message, sizeof(message), "Worldspawn: Settings for maxteams (%s) doesn't match team count (%i)\n", worldspawn->getKeyValue("maxteams"), teams);

	count = 0;
	get_team_count("info_human_start", &count, NULL);
	if (!count)
		strncat(message, "No singleplayer start positions (info_human_start)\n", sizeof(message));

	count = 0;
	get_team_count("info_civilian_start", &count, NULL);
	if (!count)
		strncat(message, "No civilian start positions (info_civilian_start)\n", sizeof(message));

	// no errors - no warnings
	if (!strlen(message))
		return;

	*returnMsg = message;
}
