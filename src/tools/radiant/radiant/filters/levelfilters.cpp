/**
 * @file levelfilters.cpp
 * @brief toolbar buttons and behaviour for filtering visible content via (ufoai)levelflags.
 */

/*
 Copyright (C) 2002-2010 UFO: Alien Invasion.

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
#include "levelfilters.h"

#include "../brush/TexDef.h"
#include "ibrush.h"
#include "ientity.h"
#include "ieventmanager.h"
#include "iscenegraph.h"

// believe me, i'm sorry
#include "../brush/brush.h"
#include "../brush/BrushNode.h"

#include "generic/callback.h"

#include <list>

static int currentActiveLevel = 0;

static inline void hide_node (scene::Node& node, bool hide)
{
	hide ? node.enable(scene::Node::eHidden) : node.disable(scene::Node::eHidden);
}

typedef std::list<Entity*> entitylist_t;

class EntityFindByName: public scene::Graph::Walker
{
		const char* m_name;
		entitylist_t& m_entitylist;
		/* this starts at 1 << level */
		int m_flag;
		int m_hide;
	public:
		EntityFindByName (const char* name, entitylist_t& entitylist, int flag, bool hide) :
			m_name(name), m_entitylist(entitylist), m_flag(flag), m_hide(hide)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0) {
				if (string_equal(m_name, entity->getKeyValue("classname"))) {
					const std::string spawnflags = entity->getKeyValue("spawnflags");
					if (!spawnflags.empty()) {
						const int spawnflagsInt = string::toInt(spawnflags);
						if (!(spawnflagsInt & m_flag)) {
							hide_node(path.top(), m_hide); // hide/unhide
							m_entitylist.push_back(entity);
						}
					} else {
						globalOutputStream() << "UFO:AI: Warning: no spawnflags for " << m_name << ".\n";
					}
				}
			}
			return true;
		}
};

class ForEachFace: public BrushVisitor
{
		Brush &m_brush;
	public:
		mutable int m_contentFlagsVis;
		mutable int m_surfaceFlagsVis;

		ForEachFace (Brush& brush) :
			m_brush(brush)
		{
			m_contentFlagsVis = -1;
			m_surfaceFlagsVis = -1;
		}

		void visit (Face& face) const
		{
			m_surfaceFlagsVis = face.getShader().m_flags.m_surfaceFlags;
			m_contentFlagsVis = face.getShader().m_flags.m_contentFlags;
		}
};

typedef std::list<Brush*> brushlist_t;

class BrushGetLevel: public scene::Graph::Walker
{
		brushlist_t& m_brushlist;
		int m_flag;
		bool m_content; // if true - use m_contentFlags - otherwise m_surfaceFlags
		mutable bool m_notset;
		mutable bool m_hide;
	public:
		BrushGetLevel (brushlist_t& brushlist, int flag, bool content, bool notset, bool hide) :
			m_brushlist(brushlist), m_flag(flag), m_content(content), m_notset(notset), m_hide(hide)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			Brush* brush = Node_getBrush(path.top());
			if (brush != 0) {
				ForEachFace faces(*brush);
				brush->forEachFace(faces);
				// contentflags?
				if (m_content) {
					// are any flags set?
					if (faces.m_contentFlagsVis > 0) {
						// flag should not be set
						if (m_notset && (!(faces.m_contentFlagsVis & m_flag))) {
							hide_node(path.top(), m_hide);
							m_brushlist.push_back(brush);
						}
						// check whether flag is set
						else if (!m_notset && ((faces.m_contentFlagsVis & m_flag))) {
							hide_node(path.top(), m_hide);
							m_brushlist.push_back(brush);
						}
					}
				}
				// surfaceflags?
				else {
					// are any flags set?
					if (faces.m_surfaceFlagsVis > 0) {
						// flag should not be set
						if (m_notset && (!(faces.m_surfaceFlagsVis & m_flag))) {
							hide_node(path.top(), m_hide);
							m_brushlist.push_back(brush);
						}
						// check whether flag is set
						else if (!m_notset && ((faces.m_surfaceFlagsVis & m_flag))) {
							hide_node(path.top(), m_hide);
							m_brushlist.push_back(brush);
						}
					}
				}

			}
			return true;
		}
};

/**
 * @brief Activates the level filter for the given level
 * @param[in] level Which level to show?
 * @todo Entities
 */
void filter_level (int flag)
{
	int level;
	brushlist_t brushes;
	entitylist_t entities;

	level = (flag >> 8);

	if (currentActiveLevel) {
		GlobalSceneGraph().traverse(BrushGetLevel(brushes, (currentActiveLevel << 8), true, true, false));
		// TODO: get this info from entities.ufo
		GlobalSceneGraph().traverse(EntityFindByName("func_rotating", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("func_door", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("func_breakable", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("misc_item", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("misc_mission", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("misc_mission_alien", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("misc_model", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("misc_sound", entities, currentActiveLevel, false));
		GlobalSceneGraph().traverse(EntityFindByName("misc_particle", entities, currentActiveLevel, false));
		entities.erase(entities.begin(), entities.end());
		brushes.erase(brushes.begin(), brushes.end());
		if (currentActiveLevel == level) {
			currentActiveLevel = 0;
			// just disable level filter
			SceneChangeNotify();
			return;
		}
	}
	currentActiveLevel = level;
	globalOutputStream() << "UFO:AI: level_active: " << currentActiveLevel << ", flag: " << flag << ".\n";

	// first all brushes
	GlobalSceneGraph().traverse(BrushGetLevel(brushes, flag, true, true, true));

	// now all entities
	GlobalSceneGraph().traverse(EntityFindByName("func_door", entities, level, true));
	GlobalSceneGraph().traverse(EntityFindByName("func_breakable", entities, level, true));
	GlobalSceneGraph().traverse(EntityFindByName("misc_model", entities, level, true));
	GlobalSceneGraph().traverse(EntityFindByName("misc_particle", entities, level, true));

	SceneChangeNotify();
}

/**
 * @brief gives the current filter level (levels 1 to 8, 0 for no filtering).
 * @return current filter level
 */
int filter_getCurrentLevel (void)
{
	if (currentActiveLevel > 2)
		return ilogb(currentActiveLevel) + 1;
	return currentActiveLevel;
}

/**@todo find a better way than these functions */
void filter_level1 (void)
{
	filter_level(CONTENTS_LEVEL_1);
}
void filter_level2 (void)
{
	filter_level(CONTENTS_LEVEL_2);
}
void filter_level3 (void)
{
	filter_level(CONTENTS_LEVEL_3);
}
void filter_level4 (void)
{
	filter_level(CONTENTS_LEVEL_4);
}
void filter_level5 (void)
{
	filter_level(CONTENTS_LEVEL_5);
}
void filter_level6 (void)
{
	filter_level(CONTENTS_LEVEL_6);
}
void filter_level7 (void)
{
	filter_level(CONTENTS_LEVEL_7);
}
void filter_level8 (void)
{
	filter_level(CONTENTS_LEVEL_8);
}

/**
 * @brief register commands for level filtering
 */
void LevelFilters_registerCommands (void)
{
	GlobalEventManager().addCommand("FilterLevel1", FreeCaller<filter_level1> ());
	GlobalEventManager().addCommand("FilterLevel2", FreeCaller<filter_level2> ());
	GlobalEventManager().addCommand("FilterLevel3", FreeCaller<filter_level3> ());
	GlobalEventManager().addCommand("FilterLevel4", FreeCaller<filter_level4> ());
	GlobalEventManager().addCommand("FilterLevel5", FreeCaller<filter_level5> ());
	GlobalEventManager().addCommand("FilterLevel6", FreeCaller<filter_level6> ());
	GlobalEventManager().addCommand("FilterLevel7", FreeCaller<filter_level7> ());
	GlobalEventManager().addCommand("FilterLevel8", FreeCaller<filter_level8> ());
}
