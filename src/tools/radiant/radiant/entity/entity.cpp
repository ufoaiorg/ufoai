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
#include "ieventmanager.h"
#include "iradiant.h"

#include "editable.h"
#include "eclasslib.h"
#include "scenelib.h"
#include "os/path.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "stringio.h"
#include "selectionlib.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/dialog.h"

#include "../sidebar/entitylist/EntityList.h"
#include "../gtkmisc.h"
#include "../select.h"
#include "../map/map.h"
#include "../settings/PreferenceDialog.h"
#include "../ui/lightdialog/LightDialog.h"
#include "../ui/particles/ParticleSelector.h"
#include "../ui/modelselector/ModelSelector.h"
#include "../ui/common/SoundChooser.h"
#include "../ui/Icons.h"
#include "../selection/algorithm/General.h"
#include "../selection/algorithm/Entity.h"

class EntitySetKeyValueSelected: public scene::Graph::Walker
{
		const std::string& m_classname;
		const std::string& m_key;
		const std::string& m_value;
	public:
		EntitySetKeyValueSelected (const std::string& classname, const std::string& key, const std::string& value) :
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
			if (entity != 0 && entity->getEntityClass().name() == m_classname && (instance.childSelected()
					|| Instance_getSelectable(instance)->isSelected())) {
				entity->setKeyValue(m_key, m_value);
			}
		}
};

static bool Entity_create (const std::string& name, const Vector3& origin)
{
	EntityClass* entityClass = GlobalEntityClassManager().findOrInsert(name, true);

	bool revert = false;
	const bool isModel = (name == "misc_model");
	const bool isSound = (name == "misc_sound");
	const bool isParticle = (name == "misc_particle");
	const bool isStartingPositionActor = (name == "info_player_start") || (name == "info_human_start") || (name
			== "info_alien_start") || (name == "info_civilian_start");
	const bool isStartingPositionUGV = (name == "info_2x2_start");
	const bool isStartingPosition = isStartingPositionActor || isStartingPositionUGV;

	const bool brushesSelected = map::countSelectedBrushes() != 0;

	if (!(entityClass->fixedsize || isModel) && !brushesSelected) {
		throw EntityCreationException(std::string("Unable to create entity \"") + name + "\", no brushes selected");
	}
	UndoableCommand undo("entityCreate -class " + name);

	AABB workzone(AABB::createFromMinMax(GlobalSelectionSystem().getWorkZone().min, GlobalSelectionSystem().getWorkZone().max));

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
		selection::algorithm::deleteSelection();

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
	if (name == "light" || name == "light_spot") {
		ui::LightDialog dialog;
		dialog.show();
		if (!dialog.isAborted()) {
			std::string intensity = dialog.getIntensity();
			Vector3 color = dialog.getColor();
			entity->setKeyValue("light", intensity);
			entity->setKeyValue("_color", color.toString());
		} else
			revert = true;
	}

	if (isModel) {
		ui::ModelAndSkin modelAndSkin = ui::ModelSelector::chooseModel();
		if (!modelAndSkin.model.empty()) {
			entity->setKeyValue("model", modelAndSkin.model);
			if (modelAndSkin.skin != -1)
				entity->setKeyValue("skin", string::toString(modelAndSkin.skin));
		} else
			revert = true;
	} else if (isSound) {
		// Display the Sound Chooser to get a sound from the user
		ui::SoundChooser sChooser;
		std::string sound = sChooser.chooseSound();
		if (!sound.empty())
			entity->setKeyValue("noise", sound);
		else
			revert = true;
	} else if (isParticle) {
		ui::ParticleSelector pSelector;
		std::string particle = pSelector.chooseParticle();
		if (!particle.empty())
			entity->setKeyValue("particle", particle);
		else
			revert = true;
	}

	return revert;
}

void Entity_createFromSelection (const std::string& name, const Vector3& origin)
{
	try {
		bool revert = Entity_create(name, origin);
		if (revert) {
			GlobalUndoSystem().undo();
			GlobalUndoSystem().clearRedo();
			GlobalSceneGraph().sceneChanged();
		}
	} catch (EntityCreationException e) {
		gtkutil::errorDialog(e.what());
	}

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

void Entity_constructPreferences (PrefPage* page)
{
	page->appendCheckBox(_("Show"), _("Light Radii"), LightRadiiImportCaller(GlobalEntityCreator()),
			LightRadiiExportCaller(GlobalEntityCreator()));
	page->appendCheckBox(_("Force"), _("Force Light Radii"), ForceLightRadiiImportCaller(GlobalEntityCreator()),
			ForceLightRadiiExportCaller(GlobalEntityCreator()));
}
void Entity_constructPage (PreferenceGroup& group)
{
	PreferencesPage* page = group.createPage(_("Entities"), _("Entity Display Preferences"));
	Entity_constructPreferences(reinterpret_cast<PrefPage*>(page));
}
void Entity_registerPrefPage ()
{
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, Entity_constructPage> ());
}

void Entity_Construct ()
{
	GlobalEventManager().addCommand("ConnectSelection", FreeCaller<selection::algorithm::connectSelectedEntities>());
	GlobalEventManager().addCommand("GroupSelection", FreeCaller<selection::algorithm::groupSelected> ());
	GlobalEventManager().addCommand("UngroupSelection", FreeCaller<selection::algorithm::ungroupSelected> ());

	Entity_registerPrefPage();
}
