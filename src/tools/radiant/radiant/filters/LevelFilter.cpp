/**
 * @file LevelFilter.cpp
 */

/*
 Copyright (C) 2002-2011 UFO: Alien Invasion.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "LevelFilter.h"

#include "ieventmanager.h"
#include "iscenegraph.h"
#include "shared.h"

#include "../brush/BrushNode.h"

#include "generic/callback.h"

#include <list>

inline void hide_node (scene::Node& node, bool hide)
{
	hide ? node.enable(scene::Node::eHidden) : node.disable(scene::Node::eHidden);
}

LevelFilter::LevelFilter () :
	currentActiveLevel(0)
{
	// TODO: get this info from entities.ufo
	_classNameList.push_back("func_rotating");
	_classNameList.push_back("func_door");
	_classNameList.push_back("func_door_sliding");
	_classNameList.push_back("func_breakable");
	_classNameList.push_back("misc_item");
	_classNameList.push_back("misc_mission");
	_classNameList.push_back("misc_mission_alien");
	_classNameList.push_back("misc_model");
	_classNameList.push_back("misc_sound");
	_classNameList.push_back("misc_particle");
}

LevelFilter::EntityFindByName::EntityFindByName (const std::string& name, EntityList& entitylist, int flag, bool hide) :
	m_name(name), m_entitylist(entitylist), m_flag(flag), m_hide(hide)
{
}

bool LevelFilter::EntityFindByName::pre (const scene::Path& path, scene::Instance& instance) const
{
	Entity* entity = Node_getEntity(path.top());
	if (entity != 0) {
		if (entity->getKeyValue("classname") == m_name) {
			const std::string spawnflags = entity->getKeyValue("spawnflags");
			if (!spawnflags.empty()) {
				const int spawnflagsInt = string::toInt(spawnflags);
				if (!(spawnflagsInt & m_flag)) {
					hide_node(path.top(), m_hide); // hide/unhide
					m_entitylist.push_back(entity);
				}
			} else {
				globalWarningStream() << "Warning: no spawnflags for bmodel entity " << m_name << ".\n";
			}
		}
	}
	return true;
}

LevelFilter::ForEachFace::ForEachFace (Brush& brush) :
	m_brush(brush)
{
	m_contentFlagsVis = -1;
	m_surfaceFlagsVis = -1;
}

void LevelFilter::ForEachFace::visit (Face& face) const
{
	m_surfaceFlagsVis = face.getShader().m_flags.getSurfaceFlags();
	m_contentFlagsVis = face.getShader().m_flags.getContentFlags();
}

LevelFilter::BrushGetLevel::BrushGetLevel (BrushList& brushlist, int flag, bool hide) :
	m_brushlist(brushlist), m_flag(flag), m_hide(hide)
{
}

bool LevelFilter::BrushGetLevel::pre (const scene::Path& path, scene::Instance& instance) const
{
	Brush* brush = Node_getBrush(path.top());
	if (brush != 0) {
		ForEachFace faces(*brush);
		brush->forEachFace(faces);
		// are any flags set?
		if (faces.m_contentFlagsVis > 0) {
			// flag should not be set
			if (!(faces.m_contentFlagsVis & m_flag)) {
				hide_node(path.top(), m_hide);
				m_brushlist.push_back(brush);
			}
		}
	}
	return true;
}

/**
 * @brief Activates the level filter for the given level
 * @param[in] level Which level to show?
 */
void LevelFilter::filter_level (int flag)
{
	int level;
	BrushList brushes;
	EntityList entities;

	level = (flag >> 8);

	if (currentActiveLevel) {
		GlobalSceneGraph().traverse(BrushGetLevel(brushes, (currentActiveLevel << 8), false));
		for (EntityClassNameList::const_iterator i = _classNameList.begin(); i != _classNameList.end(); ++i) {
			GlobalSceneGraph().traverse(EntityFindByName(*i, entities, currentActiveLevel, false));
		}
		entities.clear();
		brushes.clear();
		if (currentActiveLevel == level) {
			currentActiveLevel = 0;
			// just disable level filter
			SceneChangeNotify();
			return;
		}
	}
	currentActiveLevel = level;

	// first all brushes
	GlobalSceneGraph().traverse(BrushGetLevel(brushes, flag, true));

	// now all entities
	for (EntityClassNameList::const_iterator i = _classNameList.begin(); i != _classNameList.end(); ++i) {
		GlobalSceneGraph().traverse(EntityFindByName(*i, entities, level, true));
	}

	SceneChangeNotify();
}

/**
 * @brief gives the current filter level (levels 1 to 8, 0 for no filtering).
 * @return current filter level
 */
int LevelFilter::getCurrentLevel ()
{
	if (currentActiveLevel > 2)
		return ilogb(currentActiveLevel) + 1;
	return currentActiveLevel;
}

void LevelFilter::filter_level1 ()
{
	filter_level(CONTENTS_LEVEL_1);
}
void LevelFilter::filter_level2 ()
{
	filter_level(CONTENTS_LEVEL_2);
}
void LevelFilter::filter_level3 ()
{
	filter_level(CONTENTS_LEVEL_3);
}
void LevelFilter::filter_level4 ()
{
	filter_level(CONTENTS_LEVEL_4);
}
void LevelFilter::filter_level5 ()
{
	filter_level(CONTENTS_LEVEL_5);
}
void LevelFilter::filter_level6 ()
{
	filter_level(CONTENTS_LEVEL_6);
}
void LevelFilter::filter_level7 ()
{
	filter_level(CONTENTS_LEVEL_7);
}
void LevelFilter::filter_level8 ()
{
	filter_level(CONTENTS_LEVEL_8);
}

/**
 * @brief register commands for level filtering
 */
void LevelFilter::registerCommands ()
{
	GlobalEventManager().addCommand("FilterLevel1", MemberCaller<LevelFilter, &LevelFilter::filter_level1> (*this));
	GlobalEventManager().addCommand("FilterLevel2", MemberCaller<LevelFilter, &LevelFilter::filter_level2> (*this));
	GlobalEventManager().addCommand("FilterLevel3", MemberCaller<LevelFilter, &LevelFilter::filter_level3> (*this));
	GlobalEventManager().addCommand("FilterLevel4", MemberCaller<LevelFilter, &LevelFilter::filter_level4> (*this));
	GlobalEventManager().addCommand("FilterLevel5", MemberCaller<LevelFilter, &LevelFilter::filter_level5> (*this));
	GlobalEventManager().addCommand("FilterLevel6", MemberCaller<LevelFilter, &LevelFilter::filter_level6> (*this));
	GlobalEventManager().addCommand("FilterLevel7", MemberCaller<LevelFilter, &LevelFilter::filter_level7> (*this));
	GlobalEventManager().addCommand("FilterLevel8", MemberCaller<LevelFilter, &LevelFilter::filter_level8> (*this));
}
