/**
 * @file entity.cpp
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

#include "shared.h"

#include "entity.h"

#include "ientity.h"
#include "iselection.h"
#include "imodel.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "editable.h"

#include "eclasslib.h"
#include "scenelib.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "stringio.h"
#include "sidebar/entitylist.h"

#include "gtkutil/filechooser.h"
#include "gtkmisc.h"
#include "select.h"
#include "map.h"
#include "preferences.h"
#include "mainframe.h"
#include "qe3.h"
#include "commands.h"
#include "dialogs/light.h"
#include "dialogs/particle.h"

struct entity_globals_t
{
		Vector3 color_entity;

		entity_globals_t () :
			color_entity(0.0f, 0.0f, 0.0f)
		{
		}
};

entity_globals_t g_entity_globals;

class EntitySetKeyValueSelected: public scene::Graph::Walker
{
		const char* m_key;
		const char* m_value;
	public:
		EntitySetKeyValueSelected (const char* key, const char* value) :
			m_key(key), m_value(value)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && (instance.childSelected() || Instance_getSelectable(instance)->isSelected())) {
				entity->setKeyValue(m_key, m_value);
			}
		}
};

class EntitySetClassnameSelected: public scene::Graph::Walker
{
		const char* m_classname;
	public:
		EntitySetClassnameSelected (const char* classname) :
			m_classname(classname)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && (instance.childSelected() || Instance_getSelectable(instance)->isSelected())) {
				NodeSmartReference node(GlobalEntityCreator().createEntity(GlobalEntityClassManager().findOrInsert(
						m_classname, node_is_group(path.top()))));

				EntityCopyingVisitor visitor(*Node_getEntity(node));

				entity->forEachKeyValue(visitor);

				NodeSmartReference child(path.top().get());
				NodeSmartReference parent(path.parent().get());
				Node_getTraversable(parent)->erase(child);
				if (Node_getTraversable(child) != 0 && Node_getTraversable(node) != 0 && node_is_group(node)) {
					parentBrushes(child, node);
				}
				Node_getTraversable(parent)->insert(node);
			}
		}
};

void Scene_EntitySetKeyValue_Selected (const char* key, const char* value)
{
	GlobalSceneGraph().traverse(EntitySetKeyValueSelected(key, value));
}

void Scene_EntitySetClassname_Selected (const char* classname)
{
	GlobalSceneGraph().traverse(EntitySetClassnameSelected(classname));
}

void Entity_ungroupSelected ()
{
	if (GlobalSelectionSystem().countSelected() < 1)
		return;

	UndoableCommand undo("ungroupSelectedEntities");

	scene::Path world_path(makeReference(GlobalSceneGraph().root()));
	world_path.push(makeReference(Map_FindOrInsertWorldspawn(g_map)));

	scene::Instance &instance = GlobalSelectionSystem().ultimateSelected();
	scene::Path path = instance.path();

	if (!Node_isEntity(path.top()))
		path.pop();

	if (Node_getEntity(path.top()) != 0 && node_is_group(path.top())) {
		if (world_path.top().get_pointer() != path.top().get_pointer()) {
			parentBrushes(path.top(), world_path.top());
			Path_deleteTop(path);
		}
	}
}

class EntityFindSelected : public scene::Graph::Walker
{
public:
	mutable const scene::Path *groupPath;
	mutable scene::Instance *groupInstance;

	EntityFindSelected(): groupPath(0), groupInstance(0)
	{
	}

	bool pre (const scene::Path& path, scene::Instance& instance) const
	{
		return true;
	}

	void post (const scene::Path& path, scene::Instance& instance) const
	{
		Entity* entity = Node_getEntity(path.top());
		if (entity != 0 && Instance_getSelectable(instance)->isSelected()
		 && node_is_group(path.top()) && !groupPath) {
			groupPath = &path;
			groupInstance = &instance;
		}
	}
};

class EntityGroupSelected : public scene::Graph::Walker
{
	NodeSmartReference group;
public:
	EntityGroupSelected (const scene::Path &p): group(p.top().get())
	{
	}

	bool pre (const scene::Path& path, scene::Instance& instance) const
	{
		return true;
	}

	void post (const scene::Path& path, scene::Instance& instance) const
	{
		if (Instance_getSelectable(instance)->isSelected()) {
			Entity* entity = Node_getEntity(path.top());
			if (entity == 0 && Node_isPrimitive(path.top())) {
				NodeSmartReference child(path.top().get());
				NodeSmartReference parent(path.parent().get());

				Node_getTraversable(parent)->erase(child);
				Node_getTraversable(group)->insert(child);

				if (Node_getTraversable(parent)->empty() && path.size() >= 3 && parent != Map_FindOrInsertWorldspawn(g_map)) {
					NodeSmartReference parentparent(path[path.size() - 3].get());
					Node_getTraversable(parentparent)->erase(parent);
				}
			}
		}
	}
};

/**
 * @brief Moves all selected brushes into the selected entity.
 * Usage:
 *
 * - Select brush from entity
 * - Hit Ctrl-Alt-E
 * - Select some other brush
 * - Regroup
 *
 * The other brush will get added to the entity.
 *
 * - Select brush from entity
 * - Regroup
 * The brush will get removed from the entity, and moved to worldspawn.
 */
void Entity_groupSelected ()
{
	if (GlobalSelectionSystem().countSelected() < 1)
		return;

	UndoableCommand undo("groupSelectedEntities");

	scene::Path world_path(makeReference(GlobalSceneGraph().root()));
	world_path.push(makeReference(Map_FindOrInsertWorldspawn(g_map)));

	EntityFindSelected fse;
	GlobalSceneGraph().traverse(fse);
	if (fse.groupPath) {
		GlobalSceneGraph().traverse(EntityGroupSelected(*fse.groupPath));
	} else {
		GlobalSceneGraph().traverse(EntityGroupSelected(world_path));
	}
}

void Entity_connectSelected ()
{
	if (GlobalSelectionSystem().countSelected() == 2) {
		GlobalEntityCreator().connectEntities(GlobalSelectionSystem().penultimateSelected().path(),
				GlobalSelectionSystem().ultimateSelected().path());
	} else {
		g_warning("entityConnectSelected: exactly two instances must be selected\n");
	}
}

static int g_iLastLightIntensity = 100;

void Entity_createFromSelection (const char* name, const Vector3& origin)
{
	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert(name, true);

	const bool isModel = string_equal_nocase(name, "misc_model");
	const bool isSound = string_equal_nocase(name, "misc_sound");
	const bool isParticle = string_equal_nocase(name, "misc_particle");
	const bool isStartingPositionActor = string_equal_nocase(name, "info_player_start") || string_equal_nocase(name,
			"info_human_start") || string_equal_nocase(name, "info_alien_start") || string_equal_nocase(name,
			"info_civilian_start");
	const bool isStartingPositionUGV = string_equal_nocase(name, "info_2x2_start");
	const bool isStartingPosition = isStartingPositionActor || isStartingPositionUGV;

	const bool brushesSelected = Scene_countSelectedBrushes(GlobalSceneGraph()) != 0;

	if (!(entityClass->fixedsize || isModel) && !brushesSelected) {
		g_warning("failed to create a group entity - no brushes are selected\n");
		return;
	}

	AABB workzone(aabb_for_minmax(Select_getWorkZone().d_work_min, Select_getWorkZone().d_work_max));

	NodeSmartReference node(GlobalEntityCreator().createEntity(entityClass));

	Node_getTraversable(GlobalSceneGraph().root())->insert(node);

	scene::Path entitypath(makeReference(GlobalSceneGraph().root()));
	entitypath.push(makeReference(node.get()));
	scene::Instance& instance = findInstance(entitypath);

	if (entityClass->fixedsize || (isModel && !brushesSelected)) {
		Select_Delete();

		Transformable* transform = Instance_getTransformable(instance);
		if (transform != 0) {
			transform->setType(TRANSFORM_PRIMITIVE);
			if (isStartingPosition) {
				const int sizeX = (int) ((entityClass->maxs[0] - entityClass->mins[0]) / 2.0);
				const int sizeY = (int) ((entityClass->maxs[1] - entityClass->mins[1]) / 2.0);
				const int sizeZ = (int) ((entityClass->maxs[2] - entityClass->mins[2]) / 2.0);
				const int x = ((int) origin.x() / UNIT_SIZE) * UNIT_SIZE - sizeX;
				const int y = ((int) origin.y() / UNIT_SIZE) * UNIT_SIZE - sizeY;
				const int z = ((int) origin.z() / UNIT_HEIGHT) * UNIT_HEIGHT + sizeZ;
				const Vector3 vec(x, y, z);
				globalWarningStream() << "original start position: " << origin.x() << " " << origin.y() << " "
						<< origin.z() << "\n";
				globalWarningStream() << "start position: " << x << " " << y << " " << z << "\n";

				transform->setTranslation(vec);
			} else {
				transform->setTranslation(origin);
			}
			transform->freezeTransform();
		}

		GlobalSelectionSystem().setSelectedAll(false);

		Instance_setSelected(instance, true);
	} else {
		Scene_parentSelectedBrushesToEntity(GlobalSceneGraph(), node);
		Scene_forEachChildSelectable(SelectableSetSelected(true), instance.path());
	}

	// TODO tweaking: when right click dropping a light entity, ask for light value in a custom dialog box
	if (string_equal_nocase(name, "light")) {
		int intensity = g_iLastLightIntensity;

		if (DoLightIntensityDlg(&intensity) == eIDOK) {
			g_iLastLightIntensity = intensity;
			char buf[10];
			sprintf(buf, "%d", intensity);
			Node_getEntity(node)->setKeyValue("light", buf);
		}
	}

	if (isModel) {
		const char* model = misc_model_dialog(GTK_WIDGET(MainFrame_getWindow()));
		if (model != 0) {
			Node_getEntity(node)->setKeyValue("model", model);
		}
	} else if (isSound) {
		const char* sound = misc_sound_dialog(GTK_WIDGET(MainFrame_getWindow()));
		if (sound != 0) {
			Node_getEntity(node)->setKeyValue("sound", sound);
		}
	} else if (isParticle) {
		char* particle = misc_particle_dialog(GTK_WIDGET(MainFrame_getWindow()));
		if (particle != 0) {
			Node_getEntity(node)->setKeyValue("particle", particle);
			free(particle);
		}
	}
}

bool DoNormalisedColor (Vector3& color)
{
	if (!color_dialog(GTK_WIDGET(MainFrame_getWindow()), color))
		return false;
	/* scale colors so that at least one component is at 1.0F */

	float largest = 0.0F;

	if (color[0] > largest)
		largest = color[0];
	if (color[1] > largest)
		largest = color[1];
	if (color[2] > largest)
		largest = color[2];

	if (largest == 0.0F) {
		color[0] = 1.0F;
		color[1] = 1.0F;
		color[2] = 1.0F;
	} else {
		const float scaler = 1.0F / largest;

		color[0] *= scaler;
		color[1] *= scaler;
		color[2] *= scaler;
	}

	return true;
}

void Entity_setColour ()
{
	if (GlobalSelectionSystem().countSelected() != 0) {
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();
		Entity* entity = Node_getEntity(path.top());
		if (entity != 0) {
			const char* strColor = entity->getKeyValue("color");
			if (!string_empty(strColor)) {
				Vector3 rgb;
				if (string_parse_vector3(strColor, rgb)) {
					g_entity_globals.color_entity = rgb;
				}
			}
		}
	}
}

/**
 * @todo Support pk3 files
 */
const char* misc_model_dialog (GtkWidget* parent)
{
	StringOutputStream buffer(1024);

	buffer << g_qeglobals.m_userGamePath.c_str() << "models/";

	if (!file_readable(buffer.c_str())) {
		// just go to fsmain
		buffer.clear();
		buffer << g_qeglobals.m_userGamePath.c_str() << "/";
	}

	const char *filename = file_dialog(parent, TRUE, "Choose Model", buffer.c_str(), ModelLoader::Name());
	if (filename != 0) {
		// use VFS to get the correct relative path
		const char* relative = path_make_relative(filename, GlobalFileSystem().findRoot(filename));
		if (relative == filename) {
			g_warning("Could not extract the relative path, using full path instead\n");
		}
		return relative;
	}
	return 0;
}

char* misc_particle_dialog (GtkWidget* parent)
{
	char* particle = NULL;
	DoParticleDlg(&particle);
	return particle;
}

const char* misc_sound_dialog (GtkWidget* parent)
{
	StringOutputStream buffer(1024);

	buffer << g_qeglobals.m_userGamePath.c_str() << "sound/";

	if (!file_readable(buffer.c_str())) {
		// just go to fsmain
		buffer.clear();
		buffer << g_qeglobals.m_userGamePath.c_str() << "/";
	}

	const char* filename = file_dialog(parent, TRUE, "Open Wav File", buffer.c_str(), "sound");
	if (filename != 0) {
		const char* relative = path_make_relative(filename, GlobalFileSystem().findRoot(filename));
		if (relative == filename) {
			g_warning("Could not extract the relative path, using full path instead\n");
		}
		return relative;
	}
	return filename;
}

void Entity_constructMenu (GtkMenu* menu)
{
	create_menu_item_with_mnemonic(menu, "_Regroup", "GroupSelection");
	create_menu_item_with_mnemonic(menu, "_Ungroup", "UngroupSelection");
	create_menu_item_with_mnemonic(menu, "_Connect", "ConnectSelection");
	create_menu_item_with_mnemonic(menu, "_Select Color...", "EntityColor");
}

#include "preferencesystem.h"
#include "stringio.h"

void Entity_Construct ()
{
	GlobalCommands_insert("EntityColor", FreeCaller<Entity_setColour> (), Accelerator('K'));
	GlobalCommands_insert("ConnectSelection", FreeCaller<Entity_connectSelected> (), Accelerator('K',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("GroupSelection", FreeCaller<Entity_groupSelected>());
	GlobalCommands_insert("UngroupSelection", FreeCaller<Entity_ungroupSelected> ());

	GlobalPreferenceSystem().registerPreference("SI_Colors5", Vector3ImportStringCaller(g_entity_globals.color_entity),
			Vector3ExportStringCaller(g_entity_globals.color_entity));
	GlobalPreferenceSystem().registerPreference("LastLightIntensity", IntImportStringCaller(g_iLastLightIntensity),
			IntExportStringCaller(g_iLastLightIntensity));
}

void Entity_Destroy ()
{
}
