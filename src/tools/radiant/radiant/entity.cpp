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
#include "radiant_i18n.h"

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
#include "dialogs/ModelSelector.h"

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
		const char* m_classname;
		const char* m_key;
		const char* m_value;
	public:
		EntitySetKeyValueSelected (const char* classname, const char* key, const char* value) :
			m_classname(classname), m_key(key), m_value(value)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && (!(strcmp(entity->getEntityClass().name(), m_classname))) && (instance.childSelected()
					|| Instance_getSelectable(instance)->isSelected())) {
				entity->setKeyValue(m_key, m_value);
			}
		}
};

class EntityCopyingVisitor: public Entity::Visitor
{
		Entity& m_entity;
	public:
		EntityCopyingVisitor (Entity& entity) :
			m_entity(entity)
		{
		}

		void visit (const char* key, const char* value)
		{
			if (!string_equal(key, "classname") && m_entity.getEntityClass().getAttribute(key) != NULL) {
				m_entity.setKeyValue(key, value);
			} else {
				g_debug("EntityCopyingVisitor: skipping %s\n", key);
			}
		}
};

class EntitySetClassnameSelected: public scene::Graph::Walker
{
		const char* m_oldClassame;
		const char* m_newClassname;
	public:
		EntitySetClassnameSelected (const char* oldClassname, const char* newClassname) :
			m_newClassname(newClassname)
		{
		}
		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}
		void post (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && (!strcmp(entity->getEntityClass().name(), m_oldClassame)) && (instance.childSelected()
					|| Instance_getSelectable(instance)->isSelected())) {
				NodeSmartReference node(GlobalEntityCreator().createEntity(GlobalEntityClassManager().findOrInsert(
						m_newClassname, node_is_group(path.top()))));

				EntityCopyingVisitor visitor(*Node_getEntity(node));
				entity->forEachKeyValue(visitor);

				Node_getEntity(node)->addMandatoryKeyValues();

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

void Scene_EntitySetKeyValue_Selected (const char* classname, const char* key, const char* value)
{
	GlobalSceneGraph().traverse(EntitySetKeyValueSelected(classname, key, value));
}

void Scene_EntitySetClassname_Selected (const char* oldClassname, const char* newClassname)
{
	GlobalSceneGraph().traverse(EntitySetClassnameSelected(oldClassname, newClassname));
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

class EntityFindSelected: public scene::Graph::Walker
{
	public:
		mutable const scene::Path *groupPath;
		mutable scene::Instance *groupInstance;

		EntityFindSelected () :
			groupPath(0), groupInstance(0)
		{
		}

		bool pre (const scene::Path& path, scene::Instance& instance) const
		{
			return true;
		}

		void post (const scene::Path& path, scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(path.top());
			if (entity != 0 && Instance_getSelectable(instance)->isSelected() && node_is_group(path.top())
					&& !groupPath) {
				groupPath = &path;
				groupInstance = &instance;
			}
		}
};

class EntityGroupSelected: public scene::Graph::Walker
{
		NodeSmartReference group;
	public:
		EntityGroupSelected (const scene::Path &p) :
			group(p.top().get())
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

					if (path.size() >= 3 && parent != Map_FindOrInsertWorldspawn(g_map)) {
						NodeSmartReference parentparent(path[path.size() - 3].get());
						Node_getTraversable(parent)->erase(child);
						Node_getTraversable(group)->insert(child);

						if (Node_getTraversable(parent)->empty()) {
							Node_getTraversable(parentparent)->erase(parent);
						}
					} else {
						Node_getTraversable(parent)->erase(child);
						Node_getTraversable(group)->insert(child);
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

static bool Entity_create (const std::string& name, const Vector3& origin)
{
	std::string command = "entityCreate -class " + name;
	UndoableCommand undo(command.c_str());

	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert(name.c_str(), true);

	bool revert = false;
	const bool isModel = (name == "misc_model");
	const bool isSound = (name == "misc_sound");
	const bool isParticle = (name == "misc_particle");
	const bool isStartingPositionActor = (name == "info_player_start") || (name == "info_human_start") || (name
			== "info_alien_start") || (name == "info_civilian_start");
	const bool isStartingPositionUGV = (name == "info_2x2_start");
	const bool isStartingPosition = isStartingPositionActor || isStartingPositionUGV;

	const bool brushesSelected = Scene_countSelectedBrushes(GlobalSceneGraph()) != 0;

	if (!(entityClass->fixedsize || isModel) && !brushesSelected) {
		gtkutil::errorDialog(MainFrame_getWindow(), "Unable to create entity - no brushes selected");
		return revert;
	}

	AABB workzone(aabb_for_minmax(Select_getWorkZone().d_work_min, Select_getWorkZone().d_work_max));

	NodeSmartReference node(GlobalEntityCreator().createEntity(entityClass));

	Node_getTraversable(GlobalSceneGraph().root())->insert(node);

	scene::Path entitypath(makeReference(GlobalSceneGraph().root()));
	entitypath.push(makeReference(node.get()));
	scene::Instance& instance = findInstance(entitypath);

	// set all mandatory fields to default values
	// some will be overwritten later
	Entity *entity = Node_getEntity(node);
	entity->addMandatoryKeyValues();

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
				g_warning("original %s origin: %.0f %.0f %.0f\n", name.c_str(), origin.x(), origin.y(), origin.z());
				g_warning("     new %s origin: %i %i %i\n", name.c_str(), x, y, z);

				transform->setTranslation(vec);
			} else {
				transform->setTranslation(origin);
			}
			transform->freezeTransform();
		}

		GlobalSelectionSystem().setSelectedAll(false);

		Instance_setSelected(instance, true);
	} else {
		// Add selected brushes as children of non-fixed entity
		Scene_parentSelectedBrushesToEntity(GlobalSceneGraph(), node);
		Scene_forEachChildSelectable(SelectableSetSelected(true), instance.path());
	}

	// TODO tweaking: when right click dropping a light entity, ask for light value in a custom dialog box
	if (name == "light") {
		int intensity = g_iLastLightIntensity;

		if (DoLightIntensityDlg(&intensity) == eIDOK) {
			g_iLastLightIntensity = intensity;
			char buf[10];
			sprintf(buf, "%d", intensity);
			entity->setKeyValue("light", buf);
		}
	}

	if (isModel) {
		ui::ModelAndSkin modelAndSkin = ui::ModelSelector::chooseModel();
		if (!modelAndSkin.model.empty()) {
			entity->setKeyValue("model", modelAndSkin.model);
			entity->setKeyValue("skin", modelAndSkin.skin);
		} else
			revert = true;
	} else if (isSound) {
		const char* sound = misc_sound_dialog(GTK_WIDGET(MainFrame_getWindow()));
		if (sound != 0)
			entity->setKeyValue("noise", sound);
		else
			revert = true;
	} else if (isParticle) {
		char* particle = misc_particle_dialog(GTK_WIDGET(MainFrame_getWindow()));
		if (particle != 0) {
			entity->setKeyValue("particle", particle);
			free(particle);
		} else
			revert = true;
	}

	return revert;
}

void Entity_createFromSelection (const std::string& name, const Vector3& origin)
{
	{
		std::string command = "entityCreate -class " + name;
		UndoableCommand undo(command);
	}

	bool revert = Entity_create(name, origin);

	if (revert) {
		GlobalUndoSystem().undo();
		GlobalSceneGraph().sceneChanged();
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
			if (entity->getEntityClass().getAttribute("_color") == 0)
				return;
			const char* strColor = entity->getKeyValue("_color");
			if (!string_empty(strColor)) {
				Vector3 rgb;
				if (string_parse_vector3(strColor, rgb)) {
					g_entity_globals.color_entity = rgb;
				}
			}
			if (DoNormalisedColor(g_entity_globals.color_entity)) {
				char buffer[128];
				sprintf(buffer, "%g %g %g", g_entity_globals.color_entity[0], g_entity_globals.color_entity[1],
						g_entity_globals.color_entity[2]);

				Scene_EntitySetKeyValue_Selected(entity->getEntityClass().name(), "_color", buffer);
			}
		}
	}
}

char* misc_particle_dialog (GtkWidget* parent)
{
	char* particle = NULL;
	DoParticleDlg(&particle);
	return particle;
}

const char* misc_sound_dialog (GtkWidget* parent)
{
	std::string buffer = g_qeglobals.m_userGamePath + "sound/";

	const char* filename = file_dialog(parent, TRUE, _("Open Sound File"), buffer, "sound");
	if (filename != 0) {
		const char* relative = path_make_relative(filename, GlobalFileSystem().findRoot(filename));
		if (relative == filename) {
			g_warning("Could not extract the relative path, using full path instead\n");
		}
		// @todo strip extension
		// const std::size_t length = path_get_filename_base_length(relative);
		// relative[length] = '\0';
		return relative;
	}
	return filename;
}

void LightRadiiImport (EntityCreator& self, bool value)
{
	self.setLightRadii(value);
}
typedef ReferenceCaller1<EntityCreator, bool, LightRadiiImport> LightRadiiImportCaller;

void LightRadiiExport (EntityCreator& self, const BoolImportCallback& importer)
{
	importer(self.getLightRadii());
}
typedef ReferenceCaller1<EntityCreator, const BoolImportCallback&, LightRadiiExport> LightRadiiExportCaller;

void ForceLightRadiiImport (EntityCreator& self, bool value)
{
	self.setForceLightRadii(value);
}
typedef ReferenceCaller1<EntityCreator, bool, ForceLightRadiiImport> ForceLightRadiiImportCaller;

void ForceLightRadiiExport (EntityCreator& self, const BoolImportCallback& importer)
{
	importer(self.getForceLightRadii());
}
typedef ReferenceCaller1<EntityCreator, const BoolImportCallback&, ForceLightRadiiExport> ForceLightRadiiExportCaller;

void Entity_constructPreferences (PreferencesPage& page)
{
	page.appendCheckBox(_("Show"), _("Light Radii"), LightRadiiImportCaller(GlobalEntityCreator()),
			LightRadiiExportCaller(GlobalEntityCreator()));
	page.appendCheckBox(_("Force"), _("Force Light Radii"), ForceLightRadiiImportCaller(GlobalEntityCreator()),
			ForceLightRadiiExportCaller(GlobalEntityCreator()));
}
void Entity_constructPage (PreferenceGroup& group)
{
	PreferencesPage page(group.createPage(_("Entities"), _("Entity Display Preferences")));
	Entity_constructPreferences(page);
}
void Entity_registerPreferencesPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Entity_constructPage> ());
}

namespace
{
	GtkMenuItem *g_menuItemRegroup = 0;
	GtkMenuItem *g_menuItemUngroup = 0;
	GtkMenuItem *g_menuItemSelectColor = 0;
	GtkMenuItem *g_menuItemConnectSelection = 0;
}

void Entity_constructMenu (GtkMenu* menu)
{
	g_menuItemRegroup = create_menu_item_with_mnemonic(menu, _("_Regroup"), "GroupSelection");
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemRegroup), FALSE);
	g_menuItemUngroup = create_menu_item_with_mnemonic(menu, _("_Ungroup"), "UngroupSelection");
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemUngroup), FALSE);
	g_menuItemConnectSelection = create_menu_item_with_mnemonic(menu, _("_Connect"), "ConnectSelection");
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemConnectSelection), FALSE);
	g_menuItemSelectColor = create_menu_item_with_mnemonic(menu, _("_Select Color..."), "EntityColor");
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemSelectColor), FALSE);
}

/**
 * @brief callback evaluates current selection to decide which entity menu items should be sensitive
 */
void Entity_MenuStateReevaluate (void)
{
	const int selected = GlobalSelectionSystem().countSelected();
	bool enableSelectColor = false;
	bool enableConnect = false;
	bool enableGroup = false;
	if (selected > 0) {
		if (selected == 2) {
			enableConnect = true;
		}
		/** @todo should group/ungroup really be available for single selection?*/
		enableGroup = true;
		const scene::Path& path = GlobalSelectionSystem().ultimateSelected().path();
		Entity* entity = Node_getEntity(path.top());
		if (entity != 0) {
			if (entity->getEntityClass().getAttribute("_color")) {
				enableSelectColor = true;
			}
		}
	}
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemSelectColor), enableSelectColor);
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemConnectSelection), enableConnect);
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemRegroup), enableGroup);
	gtk_widget_set_sensitive(GTK_WIDGET(g_menuItemUngroup), enableGroup);
}

/**
 * @brief Class instance for asynchronous menu state update after selection change
 */
class EntityMenuDraw
{
		IdleDraw m_idleDraw;
	public:
		EntityMenuDraw () :
			m_idleDraw(FreeCaller<Entity_MenuStateReevaluate> ())
		{
		}
		void queueDraw (void)
		{
			m_idleDraw.queueDraw();
		}
};
static EntityMenuDraw g_EntityMenuDraw;

/**
 * @brief Selection changed callback used to reevaluate menu state based on current selection.
 * @param selectable unused
 */
void Entity_ColorPickerSelectionChanged (const Selectable& selectable)
{
	g_EntityMenuDraw.queueDraw();
}

#include "preferencesystem.h"
#include "stringio.h"
#include "signal/isignal.h"

void Entity_Construct ()
{
	GlobalCommands_insert("EntityColor", FreeCaller<Entity_setColour> (), Accelerator('K'));
	GlobalCommands_insert("ConnectSelection", FreeCaller<Entity_connectSelected> (), Accelerator('K',
			(GdkModifierType) GDK_CONTROL_MASK));
	GlobalCommands_insert("GroupSelection", FreeCaller<Entity_groupSelected> ());
	GlobalCommands_insert("UngroupSelection", FreeCaller<Entity_ungroupSelected> ());

	GlobalPreferenceSystem().registerPreference("SI_Colors5", Vector3ImportStringCaller(g_entity_globals.color_entity),
			Vector3ExportStringCaller(g_entity_globals.color_entity));
	GlobalPreferenceSystem().registerPreference("LastLightIntensity", IntImportStringCaller(g_iLastLightIntensity),
			IntExportStringCaller(g_iLastLightIntensity));

	typedef FreeCaller1<const Selectable&, Entity_ColorPickerSelectionChanged> EntityColorPickerSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(EntityColorPickerSelectionChangedCaller());

	Entity_registerPreferencesPage();
}

void Entity_Destroy ()
{
}
