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

#include "entity.h"

#include "ifilter.h"
#include "selectable.h"
#include "namespace.h"

#include "scenelib.h"
#include "entitylib.h"
#include "eclasslib.h"
#include "pivot.h"

#include "targetable.h"
#include "uniquenames.h"
#include "namekeys.h"
#include "stream/stringstream.h"
#include "filters.h"

#include "miscmodel.h"
#include "light.h"
#include "group.h"
#include "eclassmodel.h"
#include "generic.h"
#include "miscparticle.h"
#include "miscsound.h"

inline scene::Node& entity_for_eclass (EntityClass* eclass)
{
	if (classname_equal(eclass->name(), "misc_model")) {
		return New_MiscModel(eclass);
	} else if (classname_equal(eclass->name(), "misc_sound")) {
		return New_MiscSound(eclass);
	} else if (classname_equal(eclass->name(), "misc_particle")) {
		return New_MiscParticle(eclass);
	} else if (classname_equal(eclass->name(), "light")) {
		return New_Light(eclass);
	} else if (!eclass->fixedsize) {
		return New_Group(eclass);
	} else if (!eclass->modelpath().empty()) {
		return New_EclassModel(eclass);
	} else {
		return New_GenericEntity(eclass);
	}
}

void Entity_setName (Entity& entity, const char* name)
{
	entity.setKeyValue("name", name);
}
typedef ReferenceCaller1<Entity, const char*, Entity_setName> EntitySetNameCaller;

inline Namespaced* Node_getNamespaced (scene::Node& node)
{
	return NodeTypeCast<Namespaced>::cast(node);
}

inline scene::Node& node_for_eclass (EntityClass* eclass)
{
	scene::Node& node = entity_for_eclass(eclass);
	Node_getEntity(node)->setKeyValue("classname", eclass->name());

	Namespaced* namespaced = Node_getNamespaced(node);
	if (namespaced != 0) {
		namespaced->setNamespace(GlobalNamespace());
	}

	return node;
}

EntityCreator::KeyValueChangedFunc EntityKeyValues::m_entityKeyValueChanged = 0;
EntityCreator::KeyValueChangedFunc KeyValue::m_entityKeyValueChanged = 0;
Counter* EntityKeyValues::m_counter = 0;

bool g_showNames = true;
bool g_showAngles = true;
bool g_lightRadii = false;
bool g_forceLightRadii = false;

class ConnectEntities
{
	public:
		Entity* m_e1;
		Entity* m_e2;
		ConnectEntities (Entity* e1, Entity* e2) :
			m_e1(e1), m_e2(e2)
		{
		}
		void connect (const std::string& name)
		{
			m_e1->setKeyValue("target", name);
			m_e2->setKeyValue("targetname", name);
		}
		typedef MemberCaller1<ConnectEntities, const std::string&, &ConnectEntities::connect> ConnectCaller;
};

inline Entity* ScenePath_getEntity (const scene::Path& path)
{
	Entity* entity = Node_getEntity(path.top());
	if (entity == 0) {
		entity = Node_getEntity(path.parent());
	}
	return entity;
}

class UFOEntityCreator: public EntityCreator
{
	public:
		scene::Node& createEntity (EntityClass* eclass)
		{
			return node_for_eclass(eclass);
		}
		void setKeyValueChangedFunc (KeyValueChangedFunc func)
		{
			EntityKeyValues::setKeyValueChangedFunc(func);
		}
		void setCounter (Counter* counter)
		{
			EntityKeyValues::setCounter(counter);
		}
		void connectEntities (const scene::Path& path, const scene::Path& targetPath)
		{
			Entity* e1 = ScenePath_getEntity(path);
			Entity* e2 = ScenePath_getEntity(targetPath);

			if (e1 == 0 || e2 == 0) {
				globalErrorStream() << "entityConnectSelected: both of the selected instances must be an entity\n";
				return;
			}

			if (e1 == e2) {
				globalErrorStream()
						<< "entityConnectSelected: the selected instances must not both be from the same entity\n";
				return;
			}

			UndoableCommand undo("entityConnectSelected");

			ConnectEntities connector(e1, e2);
			const char* value = e2->getKeyValue("targetname");
			if (string_empty(value)) {
				value = e1->getKeyValue("target");
			}
			if (!string_empty(value)) {
				connector.connect(value);
			} else {
				const char* type = e2->getKeyValue("classname");
				if (string_empty(type)) {
					type = "t";
				}
				StringOutputStream key(64);
				key << type << "1";
				GlobalNamespace().makeUnique(key.c_str(), ConnectEntities::ConnectCaller(connector));
			}

			SceneChangeNotify();
		}
		void setShowNames (bool showNames)
		{
			g_showNames = showNames;
		}
		bool getShowNames ()
		{
			return g_showNames;
		}
		void setShowAngles (bool showAngles)
		{
			g_showAngles = showAngles;
		}
		bool getShowAngles ()
		{
			return g_showAngles;
		}
		void setLightRadii (bool lightRadii)
		{
			g_lightRadii = lightRadii;
		}
		bool getLightRadii ()
		{
			return g_lightRadii;
		}
		void setForceLightRadii (bool forceLightRadii)
		{
			g_forceLightRadii = forceLightRadii;
		}
		bool getForceLightRadii ()
		{
			return g_forceLightRadii;
		}
};

UFOEntityCreator g_UFOEntityCreator;

EntityCreator& GetEntityCreator ()
{
	return g_UFOEntityCreator;
}

class FilterEntityClassname: public EntityFilter
{
		const char* m_classname;
	public:
		FilterEntityClassname (const char* classname) :
			m_classname(classname)
		{
		}
		bool filter (const Entity& entity) const
		{
			return string_equal(entity.getKeyValue("classname"), m_classname);
		}
};

class FilterEntityClassgroup: public EntityFilter
{
		const char* m_classgroup;
		std::size_t m_length;
	public:
		FilterEntityClassgroup (const char* classgroup) :
			m_classgroup(classgroup), m_length(string_length(m_classgroup))
		{
		}
		bool filter (const Entity& entity) const
		{
			return string_equal_n(entity.getKeyValue("classname"), m_classgroup, m_length);
		}
};

FilterEntityClassname g_filter_entity_world("worldspawn");
FilterEntityClassname g_filter_entity_func_group("func_group");
FilterEntityClassname g_filter_entity_light("light");
FilterEntityClassname g_filter_entity_misc_model("misc_model");
FilterEntityClassname g_filter_entity_misc_particle("misc_particle");
FilterEntityClassgroup g_filter_entity_trigger("trigger_");
FilterEntityClassname g_filter_info_player_start("info_player_start");
FilterEntityClassname g_filter_info_human_start("info_human_start");
FilterEntityClassname g_filter_info_alien_start("info_alien_start");
FilterEntityClassname g_filter_info_2x2_start("info_2x2_start");
FilterEntityClassname g_filter_info_civilian_start("info_civilian_start");
FilterEntityClassgroup g_filter_info("info_");

void Entity_InitFilters ()
{
	add_entity_filter(g_filter_entity_world, EXCLUDE_WORLD);
	add_entity_filter(g_filter_entity_func_group, EXCLUDE_WORLD);
	add_entity_filter(g_filter_entity_world, EXCLUDE_ENT, true);
	add_entity_filter(g_filter_entity_trigger, EXCLUDE_TRIGGERS);
	add_entity_filter(g_filter_entity_misc_model, EXCLUDE_MODELS);
	add_entity_filter(g_filter_entity_light, EXCLUDE_LIGHTS);
	add_entity_filter(g_filter_entity_world, EXCLUDE_NO_FOOTSTEPS, true);
	add_entity_filter(g_filter_entity_world, EXCLUDE_NO_SURFLIGHTS, true);
	add_entity_filter(g_filter_entity_misc_particle, EXCLUDE_PARTICLE);
	add_entity_filter(g_filter_info_player_start, EXCLUDE_INFO_PLAYER_START);
	add_entity_filter(g_filter_info_2x2_start, EXCLUDE_INFO_2x2_START);
	add_entity_filter(g_filter_info_alien_start, EXCLUDE_INFO_ALIEN_START);
	add_entity_filter(g_filter_info_civilian_start, EXCLUDE_INFO_CIVILIAN_START);
	add_entity_filter(g_filter_info_human_start, EXCLUDE_INFO_HUMAN_START);
	add_entity_filter(g_filter_info, EXCLUDE_INFO);
}

#include "preferencesystem.h"

void P_Entity_Construct ()
{
	GlobalPreferenceSystem().registerPreference("SI_ShowNames", BoolImportStringCaller(g_showNames),
			BoolExportStringCaller(g_showNames));
	GlobalPreferenceSystem().registerPreference("SI_ShowAngles", BoolImportStringCaller(g_showAngles),
			BoolExportStringCaller(g_showAngles));
	GlobalPreferenceSystem().registerPreference("LightRadiuses", BoolImportStringCaller(g_lightRadii),
			BoolExportStringCaller(g_lightRadii));
	GlobalPreferenceSystem().registerPreference("ForceLightRadiuses", BoolImportStringCaller(g_forceLightRadii),
			BoolExportStringCaller(g_forceLightRadii));

	Entity_InitFilters();
	MiscModel_construct();
	MiscSound_construct();
	MiscParticle_construct();
	Light_Construct();

	RenderablePivot::StaticShader::instance() = GlobalShaderCache().capture("$PIVOT");

	GlobalShaderCache().attachRenderable(StaticRenderableConnectionLines::instance());
}

void P_Entity_Destroy ()
{
	GlobalShaderCache().detachRenderable(StaticRenderableConnectionLines::instance());

	GlobalShaderCache().release("$PIVOT");

	MiscParticle_destroy();
	MiscSound_destroy();
	MiscModel_destroy();
	Light_Destroy();
}
