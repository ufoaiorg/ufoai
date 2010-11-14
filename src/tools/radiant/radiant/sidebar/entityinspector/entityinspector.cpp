/**
 * @file entityinspector.cpp
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

#include "entityinspector.h"
#include "radiant_i18n.h"

#include "ientity.h"
#include "ifilesystem.h"
#include "imodel.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"

#include <map>
#include <set>

#include "os/path.h"
#include "eclasslib.h"
#include "scenelib.h"
#include "generic/callback.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "moduleobserver.h"
#include "convert.h"
#include "stringio.h"
#include "string/string.h"

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/container.h"
#include "gtkutil/TreeModel.h"
#include "gtkutil/IConv.h"

#include "../../qe3.h"
#include "../../gtkmisc.h"
#include "../../entity.h"
#include "../../mainframe.h"
#include "../../textureentry.h"

#include "EntityAttribute.h"
#include "AngleAttribute.h"
#include "AnglesAttribute.h"
#include "BooleanAttribute.h"
#include "DirectionAttribute.h"
#include "ListAttribute.h"
#include "ModelAttribute.h"
#include "ParticleAttribute.h"
#include "SoundAttribute.h"
#include "StringAttribute.h"
#include "Vector3Attribute.h"

namespace {
enum
{
	KEYVALLIST_COLUMN_KEY, KEYVALLIST_COLUMN_VALUE, KEYVALLIST_COLUMN_STYLE, /**< pango weigth value used to bold classname rows */
	KEYVALLIST_COLUMN_KEY_EDITABLE, /**< flag indicating whether key should be editable (has no value yet) */
	KEYVALLIST_COLUMN_TOOLTIP,

	KEYVALLIST_MAX_COLUMN
};
}

/**
 * @brief Helper method returns the first value in value list for given key for currently selected entity class or an empty string.
 * @param key key to retrieve value for
 * @return first value in value list or empty string
 */
std::string EntityInspector::SelectedEntity_getValueForKey (const std::string& key)
{
	ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
	if (it != g_selectedKeyValues.end()) {
		KeyValues &possibleValues = (*it).second;
		KeyValues::const_iterator i = possibleValues.find(key);
		if (i != possibleValues.end()) {
			const Values &values = (*i).second;
			return *values.begin();
		}
	}
	return "";
}

void EntityInspector::GlobalEntityAttributes_clear ()
{
	for (EntityAttributes::iterator i = _entityAttributes.begin(); i != _entityAttributes.end(); ++i) {
		delete *i;
	}
	_entityAttributes.clear();
}

/**
 * @brief visitor implementation used to add entities key/value pairs into a list.
 * @note key "classname" is ignored as this is supposed to be in parent map
 */
class GetKeyValueVisitor: public Entity::Visitor
{
		KeyValues &_keyvalues;
	public:
		GetKeyValueVisitor (KeyValues& keyvalues) :
			_keyvalues(keyvalues)
		{
		}

		void visit (const std::string& key, const std::string& value)
		{
			/* don't add classname property, as this will be in parent list */
			if (key == "classname")
				return;
			KeyValues::iterator keyIter = _keyvalues.find(key);
			if (keyIter == _keyvalues.end()) {
				_keyvalues.insert(KeyValues::value_type(key, Values()));
				keyIter = _keyvalues.find(key);
			}
			(*keyIter).second.push_back(Values::value_type(value));
		}
};

/**
 * @brief Adds all currently selected entities attribute values to the given map.
 *
 * @param keyvalues map of entitites key value pairs associated to their classnames
 */
void Entity_GetKeyValues_Selected (ClassKeyValues& keyvalues)
{
	/**
	 * @brief visitor implementation used to add all selected entities key/value pairs
	 */
	class EntityGetKeyValues: public SelectionSystem::Visitor
	{
			ClassKeyValues& m_keyvalues;
			mutable std::set<Entity*> m_visited;

			/**
			 * @brief Adds all entities class attributes into the given map.
			 *
			 * @param entity entity to add into map
			 * @param keyvalues list of key/values associated to their class names
			 */
			void Entity_GetKeyValues (const Entity& entity) const
			{
				std::string classname = entity.getEntityClass().m_name;
				ClassKeyValues::iterator valuesIter = m_keyvalues.find(classname);
				if (valuesIter == m_keyvalues.end()) {
					m_keyvalues.insert(ClassKeyValues::value_type(classname, KeyValues()));
					valuesIter = m_keyvalues.find(classname);
				}

				GetKeyValueVisitor visitor((*valuesIter).second);

				entity.forEachKeyValue(visitor);
			}

		public:
			EntityGetKeyValues (ClassKeyValues& keyvalues) :
				m_keyvalues(keyvalues)
			{
			}
			void visit (scene::Instance& instance) const
			{
				Entity* entity = Node_getEntity(instance.path().top());
				if (entity == 0 && instance.path().size() != 1) {
					entity = Node_getEntity(instance.path().parent());
				}
				if (entity != 0 && m_visited.insert(entity).second) {
					Entity_GetKeyValues(*entity);
				}
			}
	} visitor(keyvalues);
	GlobalSelectionSystem().foreachSelected(visitor);
}

class EntityClassListStoreAppend: public EntityClassVisitor
{
		GtkListStore* store;
	public:
		EntityClassListStoreAppend (GtkListStore* store_) :
			store(store_)
		{
		}
		void visit (EntityClass* e)
		{
			GtkTreeIter iter;
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, e->name().c_str(), 1, e, -1);
		}
};

void EntityInspector::EntityClassList_fill ()
{
	if (_entityListStore) {
		EntityClassListStoreAppend append(_entityListStore);
		GlobalEntityClassManager().forEach(append);
	}
}

void EntityInspector::EntityClassList_clear ()
{
	if (GTK_IS_LIST_STORE(_entityListStore)) {
		gtk_list_store_clear(_entityListStore);
	}
}

void EntityInspector::SetComment (EntityClass* eclass)
{
	if (eclass == _currentComment)
		return;

	_currentComment = eclass;

	GtkTextBuffer* buffer = gtk_text_view_get_buffer(_entityClassComment);
	gtk_text_buffer_set_text(buffer, eclass->comments().c_str(), -1);
}

/**
 * @brief Updates surface flags buttons to display given entity class spawnflags
 * @param eclass entity class to display
 * @note This method hides all buttons, then updates as much as needed with new label and shows them.
 * @sa EntityInspector::updateSpawnflags
 */
void EntityInspector::SurfaceFlags_setEntityClass (EntityClass* eclass)
{
	if (eclass == _currentFlags)
		return;

	_currentFlags = eclass;

	int spawnflag_count = 0;

	{
		// do a first pass to count the spawn flags, don't touch the widgets, we don't know in what state they are
		for (int i = 0; i < MAX_FLAGS; i++) {
			if (eclass->flagnames[i] && eclass->flagnames[i][0] != 0 && strcmp(eclass->flagnames[i], "-")) {
				_spawnTable[spawnflag_count] = i;
				spawnflag_count++;
			}
		}
	}

	// disable all remaining boxes
	// NOTE: these boxes might not even be on display
	{
		for (int i = 0; i < _spawnflagCount; ++i) {
			GtkWidget* widget = GTK_WIDGET(_entitySpawnflagsCheck[i]);
			gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), " ");
			gtk_widget_hide(widget);
			gtk_widget_ref(widget);
			gtk_container_remove(GTK_CONTAINER(_spawnflagsTable), widget);
		}
	}

	_spawnflagCount = spawnflag_count;

	{
		for (int i = 0; i < _spawnflagCount; ++i) {
			GtkWidget* widget = GTK_WIDGET(_entitySpawnflagsCheck[i]);
			gtk_widget_show(widget);

			StringOutputStream str(16);
			str << LowerCase(eclass->flagnames[_spawnTable[i]]);

			gtk_table_attach(_spawnflagsTable, widget, i % 4, i % 4 + 1, i / 4, i / 4 + 1,
					(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
			gtk_widget_unref(widget);

			gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), str.c_str());
		}
	}
}

void EntityInspector::EntityClassList_selectEntityClass (EntityClass* eclass)
{
	GtkTreeModel* model = GTK_TREE_MODEL(_entityListStore);
	GtkTreeIter iter;
	for (gboolean good = gtk_tree_model_get_iter_first(model, &iter); good != FALSE; good = gtk_tree_model_iter_next(
			model, &iter)) {
		char* text;
		gtk_tree_model_get(model, &iter, 0, &text, -1);
		if (eclass->name() == std::string(text)) {
			GtkTreeView* view = _entityClassTreeView;
			GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
			gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
			if (GTK_WIDGET_REALIZED(view)) {
				gtk_tree_view_scroll_to_cell(view, path, 0, FALSE, 0, 0);
			}
			gtk_tree_path_free(path);
			good = FALSE;
		}
		g_free(text);
	}
}

void EntityInspector::appendAttribute (const std::string& name, EntityAttribute& attribute)
{
	GtkTable* row = DialogRow_new(name, attribute.getWidget());
	DialogVBox_packRow(_attributeBox, GTK_WIDGET(row));
}

/**
 * Set of utility classes which are used to create an EntityAttribute of the
 * type specified in the template parameter. For use in the
 * EntityAttributeFactory class.
 *
 * TODO: Perhaps cleaner to use a namespaced, templated function here rather
 * than a class with single static method?
 */
template<typename Attribute>
class StatelessAttributeCreator
{
	public:
		// Create an Entity Attribute of the given type.
		static EntityAttribute* create (const std::string& classname, const std::string& name)
		{
			return new Attribute(classname, name);
		}
};

/**
 * This class allows the creation of various types of EntityAttribute objects
 * identified at runtime by their string name.
 */
class EntityAttributeFactory
{
		typedef EntityAttribute* (*CreateFunc) (const std::string& classname, const std::string& name);
		typedef std::map<std::string, CreateFunc, RawStringLess> Creators;
		Creators m_creators;
	public:
		// Constructor. Populate the Creators map with the string types of all
		// available EntityAttribute classes, along with a pointer to the create()
		// function from the respective StatelessAttributeCreator object.
		EntityAttributeFactory ()
		{
			m_creators.insert(Creators::value_type("string", &StatelessAttributeCreator<StringAttribute>::create));
			m_creators.insert(Creators::value_type("color", &StatelessAttributeCreator<StringAttribute>::create));
			m_creators.insert(Creators::value_type("integer", &StatelessAttributeCreator<StringAttribute>::create));
			m_creators.insert(Creators::value_type("real", &StatelessAttributeCreator<StringAttribute>::create));
			m_creators.insert(Creators::value_type("boolean", &StatelessAttributeCreator<BooleanAttribute>::create));
			m_creators.insert(Creators::value_type("angle", &StatelessAttributeCreator<AngleAttribute>::create));
			m_creators.insert(Creators::value_type("direction", &StatelessAttributeCreator<DirectionAttribute>::create));
			m_creators.insert(Creators::value_type("angles", &StatelessAttributeCreator<AnglesAttribute>::create));
			m_creators.insert(Creators::value_type("particle", &StatelessAttributeCreator<ParticleAttribute>::create));
			m_creators.insert(Creators::value_type("model", &StatelessAttributeCreator<ModelAttribute>::create));
			m_creators.insert(Creators::value_type("noise", &StatelessAttributeCreator<SoundAttribute>::create));
			m_creators.insert(Creators::value_type("vector3", &StatelessAttributeCreator<Vector3Attribute>::create));
		}

		// Create an EntityAttribute from the given string classtype, with the given name
		EntityAttribute* create (const std::string& type, const std::string& name)
		{
			std::string classname = g_current_attributes->name();
			Creators::iterator i = m_creators.find(type);
			// If the StatelessAttributeCreator::create function is found for the
			// type, invoke it to create a new EntityAttribute with the given name
			if (i != m_creators.end())
				return i->second(classname, name);

			const ListAttributeType* listType = GlobalEntityClassManager().findListType(type);
			if (listType != 0)
				return new ListAttribute(classname, name, *listType);

			return 0;
		}
};

typedef Static<EntityAttributeFactory> GlobalEntityAttributeFactory;

void EntityInspector::checkAddNewKeys ()
{
	int count = 0;
	const EntityClassAttributes validAttrib = g_current_attributes->m_attributes;
	for (EntityClassAttributes::const_iterator i = validAttrib.begin(); i != validAttrib.end(); ++i) {
		EntityClassAttribute *attrib = const_cast<EntityClassAttribute*> (&(i->second));
		ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
		if (it == g_selectedKeyValues.end())
			return;

		KeyValues possibleValues = it->second;
		KeyValues::const_iterator keyIter = possibleValues.find(attrib->m_type);
		/* end means we don't have it actually in this map, so this is a valid new key*/
		if (keyIter == possibleValues.end())
			count++;
	}
	_numNewKeys = count;
}

void EntityInspector::setEntityClass (EntityClass *eclass)
{
	EntityClassList_selectEntityClass(eclass);
	SurfaceFlags_setEntityClass(eclass);

	if (eclass != g_current_attributes) {
		g_current_attributes = eclass;

		container_remove_all(GTK_CONTAINER(_attributeBox));
		GlobalEntityAttributes_clear();

		for (EntityClassAttributes::const_iterator i = eclass->m_attributes.begin(); i != eclass->m_attributes.end(); ++i) {
			EntityAttribute* attribute = GlobalEntityAttributeFactory::instance().create(i->second.m_type, i->first);
			if (attribute != 0) {
				_entityAttributes.push_back(attribute);
				appendAttribute(EntityClassAttributePair_getName(*i), *_entityAttributes.back());
			}
		}
	}
}

/**
 * @brief This method updates active state of spawnflag buttons based on current selected value
 */
void EntityInspector::updateSpawnflags ()
{
	int i;
	const int f = string::toInt(SelectedEntity_getValueForKey("spawnflags"));
	for (i = 0; i < _spawnflagCount; ++i) {
		const int v = !!(f & (1 << _spawnTable[i]));

		toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(_entitySpawnflagsCheck[i]), v);
	}

	// take care of the remaining ones
	for (i = _spawnflagCount; i < MAX_FLAGS; ++i) {
		toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(_entitySpawnflagsCheck[i]), FALSE);
	}
}

std::string EntityInspector::getTooltipForKey (const std::string& key)
{
	EntityClassAttribute *attrib = g_current_attributes->getAttribute(key);
	if (attrib != NULL)
		return attrib->m_description;
	else
		return "";
}

/**
 * @brief This method updates key value list for current selected entities.
 */
void EntityInspector::updateKeyValueList ()
{
	GtkTreeStore* store = _entityPropertiesTreeStore;

	gtk_tree_store_clear(store);
	// Walk through list and add pairs
	for (ClassKeyValues::iterator classIter = g_selectedKeyValues.begin(); classIter != g_selectedKeyValues.end(); ++classIter) {
		std::string classname = classIter->first;
		GtkTreeIter classTreeIter;
		gtk_tree_store_append(store, &classTreeIter, NULL);
		gtk_tree_store_set(store, &classTreeIter, KEYVALLIST_COLUMN_KEY, "classname", KEYVALLIST_COLUMN_VALUE,
				classname.c_str(), KEYVALLIST_COLUMN_STYLE, PANGO_WEIGHT_BOLD, KEYVALLIST_COLUMN_KEY_EDITABLE, FALSE,
				KEYVALLIST_COLUMN_TOOLTIP, g_current_attributes->comments().c_str(), -1);
		KeyValues possibleValues = classIter->second;
		for (KeyValues::iterator i = possibleValues.begin(); i != possibleValues.end(); ++i) {
			GtkTreeIter iter;
			gtk_tree_store_append(store, &iter, &classTreeIter);
			std::string key = i->first;
			const Values &values = (*i).second;
			std::string value = *values.begin();
			gtk_tree_store_set(store, &iter, KEYVALLIST_COLUMN_KEY, key.c_str(), KEYVALLIST_COLUMN_VALUE,
					value.c_str(), KEYVALLIST_COLUMN_STYLE, PANGO_WEIGHT_NORMAL, KEYVALLIST_COLUMN_KEY_EDITABLE, FALSE,
					KEYVALLIST_COLUMN_TOOLTIP, getTooltipForKey(key).c_str(), -1);
		}
	}
	//set all elements expanded
	gtk_tree_view_expand_all(_keyValuesTreeView);
	checkAddNewKeys();
}

// Entity Inspector redraw functor
class EntityInspectorDraw
{
	private:

		IdleDraw m_idleDraw;
		EntityInspector& _entityInspector;

	private:

		void updateGuiElements ()
		{
			g_selectedKeyValues.clear();
			Entity_GetKeyValues_Selected(g_selectedKeyValues);

			{
				std::string classname;
				ClassKeyValues::iterator classFirstIter = g_selectedKeyValues.begin();
				if (classFirstIter != g_selectedKeyValues.end())
					classname = classFirstIter->first;
				else
					classname = "";
				_entityInspector.setEntityClass(GlobalEntityClassManager().findOrInsert(classname, false));
			}
			_entityInspector.updateSpawnflags();

			_entityInspector.updateKeyValueList();

			for (EntityAttributes::const_iterator i = _entityInspector._entityAttributes.begin(); i
					!= _entityInspector._entityAttributes.end(); ++i) {
				(*i)->update();
			}
		}

	public:

		// Constructor
		EntityInspectorDraw (EntityInspector& entityInspector) :
			m_idleDraw(MemberCaller<EntityInspectorDraw, &EntityInspectorDraw::updateGuiElements> (*this)),
					_entityInspector(entityInspector)
		{
		}
		void queueDraw ()
		{
			m_idleDraw.queueDraw();
		}
};

/**
 * @brief Creates a new entity based on the currently selected brush and entity type.
 */
void EntityInspector::EntityClassList_createEntity ()
{
	GtkTreeView* view = _entityClassTreeView;

	// find out what type of entity we are trying to create
	GtkTreeModel* model;
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(view), &model, &iter) == FALSE) {
		gtkutil::errorDialog(_("You must have a selected class to create an entity"));
		return;
	}

	char* text;
	gtk_tree_model_get(model, &iter, 0, &text, -1);
	Entity_createFromSelection(text, g_vector3_identity);
	g_free(text);
}

void EntityInspector::addKeyValue (GtkButton *button, EntityInspector* entityInspector)
{
	GtkTreeIter iter;
	GtkTreeIter parent;
	GtkTreeModel* model = gtk_tree_view_get_model(entityInspector->_keyValuesTreeView);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(entityInspector->_keyValuesTreeView);

	gtk_tree_selection_get_selected(selection, &model, &iter);
	if (gtk_tree_model_iter_parent(model, &parent, &iter) == FALSE) {
		// parent node selected, just add child
		parent = iter;
		gtk_tree_store_append(store, &iter, &parent);
	} else {
		gtk_tree_store_append(store, &iter, &parent);
	}

	gtk_tree_store_set(store, &iter, KEYVALLIST_COLUMN_KEY, "", KEYVALLIST_COLUMN_VALUE, "", KEYVALLIST_COLUMN_STYLE,
			PANGO_WEIGHT_NORMAL, KEYVALLIST_COLUMN_KEY_EDITABLE, TRUE, KEYVALLIST_COLUMN_TOOLTIP, NULL, -1);
	/* expand to have added line visible (for the case parent was not yet expanded because it had no children */
	gtk_tree_view_expand_all(entityInspector->_keyValuesTreeView);

	/* select newly added field and start editing */
	GtkTreeViewColumn* viewColumn = gtk_tree_view_get_column(entityInspector->_keyValuesTreeView, 0);
	GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_set_cursor(entityInspector->_keyValuesTreeView, path, viewColumn, TRUE);
	gtk_tree_path_free(path);
	//disable new key as long as we edit
	g_object_set(entityInspector->_addKeyButton, "sensitive", FALSE, (const char*) 0);
}

void EntityInspector::clearKeyValue (GtkButton * button, EntityInspector* entityInspector)
{
	GtkTreeSelection* selection = gtk_tree_view_get_selection(entityInspector->_keyValuesTreeView);
	std::string key = gtkutil::TreeModel::getSelectedString(selection, KEYVALLIST_COLUMN_KEY);
	if (key.empty())
		return;

	std::string classname = gtkutil::TreeModel::getSelectedParentString(selection, KEYVALLIST_COLUMN_VALUE);
	if (classname.empty())
		classname = gtkutil::TreeModel::getSelectedString(selection, KEYVALLIST_COLUMN_VALUE);

	if (key != "classname") {
		std::string command = "entityDeleteKey -classname \"" + classname + "\" -key \"" + key + "\"";
		UndoableCommand undo(command);
		Scene_EntitySetKeyValue_Selected(classname, key, "");
	}
}

// =============================================================================
// callbacks

void EntityInspector::EntityClassList_selection_changed (GtkTreeSelection* selection, EntityInspector* entityInspector)
{
	EntityClass* eclass = (EntityClass*) gtkutil::TreeModel::getSelectedPointer(selection, 1);
	if (eclass != 0)
		entityInspector->SetComment(eclass);
}

gint EntityInspector::EntityClassList_button_press (GtkWidget *widget, GdkEventButton *event,
		EntityInspector* entityInspector)
{
	// double clicked with the left mount button
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		entityInspector->EntityClassList_createEntity();
		return TRUE;
	}
	return FALSE;
}

gint EntityInspector::EntityClassList_keypress (GtkWidget* widget, GdkEventKey* event, EntityInspector* entityInspector)
{
	unsigned int code = gdk_keyval_to_upper(event->keyval);

	if (event->keyval == GDK_Return) {
		entityInspector->EntityClassList_createEntity();
		return TRUE;
	}

	// select the entity that starts with the key pressed
	if (code <= 'Z' && code >= 'A') {
		GtkTreeView* view = entityInspector->_entityClassTreeView;
		GtkTreeModel* model;
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(view), &model, &iter) == FALSE
				|| gtk_tree_model_iter_next(model, &iter) == FALSE) {
			gtk_tree_model_get_iter_first(model, &iter);
		}

		for (std::size_t count = gtk_tree_model_iter_n_children(model, 0); count > 0; --count) {
			char* text;
			gtk_tree_model_get(model, &iter, 0, &text, -1);

			if (toupper(text[0]) == (int) code) {
				GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
				gtk_tree_selection_select_path(gtk_tree_view_get_selection(view), path);
				if (GTK_WIDGET_REALIZED(view)) {
					gtk_tree_view_scroll_to_cell(view, path, 0, FALSE, 0, 0);
				}
				gtk_tree_path_free(path);
				count = 1;
			}

			g_free(text);

			if (gtk_tree_model_iter_next(model, &iter) == FALSE)
				gtk_tree_model_get_iter_first(model, &iter);
		}

		return TRUE;
	}
	return FALSE;
}

void EntityInspector::SpawnflagCheck_toggled (GtkWidget *widget, EntityInspector* entityInspector)
{
	int f, i;

	f = 0;
	for (i = 0; i < entityInspector->_spawnflagCount; ++i) {
		const int v = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entityInspector->_entitySpawnflagsCheck[i]));
		f |= v << entityInspector->_spawnTable[i];
	}

	std::string value = (f == 0) ? "" : string::toString(f);

	{
		std::string command = "entitySetFlags -classname " + entityInspector->_currentFlags->name() + "-flags "
				+ string::toString(f);
		UndoableCommand undo(command);

		Scene_EntitySetKeyValue_Selected(entityInspector->_currentFlags->name(), "spawnflags", value);
	}
}

void EntityInspector::entityKeyValueEdited (bool isValueEdited, const std::string& newValue)
{
	std::string key, value;
	std::string classname, oldvalue;
	bool isClassname = false;
	GtkTreeModel* model;
	GtkTreeIter iter, parent;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(_keyValuesTreeView);

	if (gtk_tree_selection_get_selected(selection, &model, &iter) == FALSE)
		return;

	if (isValueEdited) {
		key = gtkutil::TreeModel::getString(model, &iter, KEYVALLIST_COLUMN_KEY);
		oldvalue = gtkutil::TreeModel::getString(model, &iter, KEYVALLIST_COLUMN_VALUE);
		value = newValue;
	} else {
		oldvalue = gtkutil::TreeModel::getString(model, &iter, KEYVALLIST_COLUMN_KEY);
		value = gtkutil::TreeModel::getString(model, &iter, KEYVALLIST_COLUMN_VALUE);
		key = newValue;
	}

	if (oldvalue == newValue)
		return;

	if (gtk_tree_model_iter_parent(model, &parent, &iter) == FALSE) {
		classname = gtkutil::TreeModel::getString(model, &iter, KEYVALLIST_COLUMN_VALUE);
		isClassname = true;
	} else {
		classname = gtkutil::TreeModel::getString(model, &parent, KEYVALLIST_COLUMN_VALUE);
	}

	// Get current selection text
	std::string keyConverted = gtkutil::IConv::localeFromUTF8(key);
	std::string valueConverted = gtkutil::IConv::localeFromUTF8(value);
	std::string classnameConverted = gtkutil::IConv::localeFromUTF8(classname);

	// if you change the classname to worldspawn you won't merge back in the structural
	// brushes but create a parasite entity
	if (isClassname && valueConverted == "worldspawn") {
		gtkutil::errorDialog(_("Cannot change \"classname\" key back to worldspawn."));
		return;
	}

	// we don't want spaces in entity keys
	if (strstr(keyConverted.c_str(), " ")) {
		gtkutil::errorDialog(_("No spaces are allowed in entity keys."));
		return;
	}

	//instead of updating key/values right now, try to get default value and edit it before setting final values
	if (!isValueEdited && !keyConverted.empty()) {
		if (valueConverted.empty())
			valueConverted = g_current_attributes->getDefaultForAttribute(keyConverted);
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter, KEYVALLIST_COLUMN_KEY, keyConverted.c_str(),
				KEYVALLIST_COLUMN_VALUE, valueConverted.c_str(), KEYVALLIST_COLUMN_TOOLTIP, getTooltipForKey(
						keyConverted).c_str(), -1);
		g_currentSelectedKey = keyConverted;
		GtkTreePath* currentPath = gtk_tree_model_get_path(model, &iter);
		GtkTreeViewColumn *column = gtk_tree_view_get_column(_keyValuesTreeView, KEYVALLIST_COLUMN_VALUE);
		gtk_tree_view_set_cursor(_keyValuesTreeView, currentPath, column, TRUE);
		gtk_tree_path_free(currentPath);
		return;
	}

	if (isClassname) {
		std::string command = "entitySetClass -class " + classnameConverted + " -newclass " + valueConverted;
		UndoableCommand undo(command);
		Scene_EntitySetClassname_Selected(classnameConverted, valueConverted);
	} else {
		std::string command = "entitySetKeyValue -classname \"" + classnameConverted + "\" -key \"" + keyConverted
				+ "\" -value \"" + valueConverted + "\"";
		UndoableCommand undo(command);
		Scene_EntitySetKeyValue_Selected(classnameConverted, keyConverted, valueConverted);
	}
	gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
}

void EntityInspector::entityValueEdited (GtkCellRendererText *renderer, gchar *path, gchar* new_text,
		EntityInspector *entityInspector)
{
	entityInspector->entityKeyValueEdited(true, new_text);
}

void EntityInspector::entityKeyEdited (GtkCellRendererText *renderer, gchar *path, gchar* new_text,
		EntityInspector *entityInspector)
{
	entityInspector->entityKeyValueEdited(false, new_text);
}

/**
 * @brief callback invoked when key edit was canceled, used to remove newly added empty keys.
 * @param renderer cell renderer used to edit
 * @param view treeview that is edited
 */
void EntityInspector::entityKeyEditCanceled (GtkCellRendererText *renderer, EntityInspector *entityInspector)
{
	char *oldKey;

	g_object_get(G_OBJECT(renderer), "text", &oldKey, (char*) 0);
	std::string keyConverted = gtkutil::IConv::localeFromUTF8(oldKey);
	if (keyConverted.empty()) {
		GtkTreeModel* model;
		GtkTreeIter iter;
		/* retrieve current selection and iter*/
		GtkTreeSelection *selection = gtk_tree_view_get_selection(entityInspector->_keyValuesTreeView);
		gtk_tree_selection_get_selected(selection, &model, &iter);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	}
}

/**
 * @brief Callback for selection changed in entity key value list used to enable/disable buttons
 * @param selection current selection
 */
void EntityInspector::EntityKeyValueList_selection_changed (GtkTreeSelection* selection,
		EntityInspector* entityInspector)
{
	if (gtk_tree_selection_count_selected_rows(selection) == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(entityInspector->_removeKeyButton), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(entityInspector->_addKeyButton), FALSE);
	} else {
		bool removeAllowed = true;
		std::string classname;
		std::string attribKey = gtkutil::TreeModel::getSelectedString(selection, KEYVALLIST_COLUMN_KEY);
		if (attribKey.empty()) {
			/* new attribute, don't check for remove yet */
			removeAllowed = false;
		}

		classname = gtkutil::TreeModel::getSelectedParentString(selection, KEYVALLIST_COLUMN_VALUE);
		if (classname.empty()) {
			// no parent -> top level = classname selected, may not be removed
			removeAllowed = false;
			classname = gtkutil::TreeModel::getSelectedString(selection, KEYVALLIST_COLUMN_VALUE);
		}
		/* check whether attributes reflect current keyvalue list selected class */
		if (classname != g_current_attributes->name()) {
			/* entity class does not fit the selection from keyvaluelist, update as needed */
			EntityClass *eclass = GlobalEntityClassManager().findOrInsert(classname, false);
			entityInspector->setEntityClass(eclass);
		}

		if (attribKey.length() > 0)
			entityInspector->g_currentSelectedKey = attribKey;

		if (removeAllowed) {
			EntityClassAttribute *attribute = g_current_attributes->getAttribute(attribKey);
			if (attribute && attribute->m_mandatory)
				removeAllowed = false;
		}
		gtk_widget_set_sensitive(GTK_WIDGET(entityInspector->_removeKeyButton), removeAllowed);
		gtk_widget_set_sensitive(GTK_WIDGET(entityInspector->_addKeyButton), (entityInspector->_numNewKeys > 0));
	}
}

/**
 * @brief Callback for "editing-started" signal for key column used to update combo renderer with appropriate values.
 *
 * @param renderer combo renderer used for editing
 * @param editable unused
 * @param path unused
 * @param user_data unused
 */
void EntityInspector::EntityKeyValueList_keyEditingStarted (GtkCellRenderer *renderer, GtkCellEditable *editable,
		const gchar *path, EntityInspector* entityInspector)
{
	if (!g_current_attributes)
		return;
	/* determine available key values and set them to combo box */
	GtkListStore* store;
	g_object_get(renderer, "model", &store, (char*) 0);
	gtk_list_store_clear(store);

	GtkTreeIter storeIter;
	const EntityClassAttributes validAttrib = g_current_attributes->m_attributes;
	for (EntityClassAttributes::const_iterator i = validAttrib.begin(); i != validAttrib.end(); ++i) {
		EntityClassAttribute *attrib = const_cast<EntityClassAttribute*> (&(*i).second);
		ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
		if (it == g_selectedKeyValues.end()) {
			return;
		}
		KeyValues possibleValues = (*it).second;
		KeyValues::const_iterator keyIter = possibleValues.find(attrib->m_type);
		/* end means we don't have it actually in this map, so this is a valid new key*/
		if (keyIter == possibleValues.end()) {
			gtk_list_store_append(store, &storeIter);
			gtk_list_store_set(store, &storeIter, 0, attrib->m_type.c_str(), -1);
		}
	}
}

/**
 * @brief visitor class used to add entity classes into given store for value combobox.
 */
class EntityKeyValueComboListStoreAppend: public EntityClassVisitor
{
		GtkListStore* store;
		bool onlyFixedsize;
	public:
		EntityKeyValueComboListStoreAppend (GtkListStore* store_, bool onlyFixedsize_) :
			store(store_), onlyFixedsize(onlyFixedsize_)
		{
		}
		void visit (EntityClass* e)
		{
			if (e->name() == "worldspawn")
				return; //don't add worldspawn
			if (onlyFixedsize && !e->fixedsize)
				return; //don't add non-fixedsize if we have no brush actually
			GtkTreeIter iter;
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, e->name().c_str(), -1);
		}
};

/**
 * @brief Helper method fills all classnames into given renderer combo model.
 * @param renderer value combo renderer
 */
void EntityInspector::EntityKeyValueList_fillValueComboWithClassnames (GtkCellRenderer *renderer)
{
	GtkListStore* store;
	g_object_get(renderer, "model", &store, (char*) 0);
	gtk_list_store_clear(store);
	EntityKeyValueComboListStoreAppend append(store, g_current_attributes->fixedsize);
	GlobalEntityClassManager().forEach(append);
}

/**
 * @brief Callback for "editing-started" signal for value column used to update combo renderer with appropriate values.
 *
 * @param renderer combo renderer used for editing
 * @param editable unused
 * @param path unused
 * @param user_data unused
 */
void EntityInspector::EntityKeyValueList_valueEditingStarted (GtkCellRenderer *renderer, GtkCellEditable *editable,
		const gchar *path, EntityInspector* entityInspector)
{
	// prevent update if already displaying anything
	{
		bool editing;
		g_object_get(G_OBJECT(renderer), "editing", &editing, (const char*) 0);
		if (editing)
			return;
	}

	if (entityInspector->g_currentSelectedKey.empty())
		return;

	if (entityInspector->g_currentSelectedKey == "classname") {
		entityInspector->EntityKeyValueList_fillValueComboWithClassnames(renderer);
		return;
	}
	ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
	if (it == g_selectedKeyValues.end()) {
		return;
	}
	KeyValues possibleValues = (*it).second;
	KeyValues::const_iterator keyIter = possibleValues.find(entityInspector->g_currentSelectedKey);
	GtkListStore* store;
	g_object_get(renderer, "model", &store, (char*) 0);
	gtk_list_store_clear(store);
	GtkTreeIter storeIter;
	// end means we don't have it actually in this map, so this is a valid new key
	if (keyIter != possibleValues.end()) {
		Values values = (*keyIter).second;
		for (Values::const_iterator valueIter = values.begin(); valueIter != values.end(); valueIter++) {
			gtk_list_store_append(store, &storeIter);
			gtk_list_store_set(store, &storeIter, 0, (*valueIter).c_str(), -1);
		}
	} else {
		gtk_list_store_append(store, &storeIter);
		gtk_list_store_set(store, &storeIter, 0, "", -1);
	}
}

/**
 * Function to construct the GTK components of the entity inspector.
 */
GtkWidget* EntityInspector::constructNotebookTab ()
{
	GtkWidget* pageframe = gtk_frame_new(_("Entity Inspector"));
	GtkWidget* vbox = gtk_vbox_new(FALSE, 2);

	gtk_container_set_border_width(GTK_CONTAINER(pageframe), 2);

	{
		gtk_container_add(GTK_CONTAINER(pageframe), vbox);

		{
			// entity class list
			GtkWidget* scr = gtk_scrolled_window_new(0, 0);
			gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
			gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

			{
				GtkListStore* store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);

				GtkTreeView * view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
				gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), FALSE);
				gtk_tree_view_set_headers_visible(view, FALSE);
				g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(EntityClassList_button_press), this);
				g_signal_connect(G_OBJECT(view), "key_press_event", G_CALLBACK(EntityClassList_keypress), this);

				{
					GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
					GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Key"), renderer, "text", 0,
							(char const*) 0);
					gtk_tree_view_append_column(view, column);
				}

				{
					GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
					g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(EntityClassList_selection_changed), this);
				}

				gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(view));

				g_object_unref(G_OBJECT (store));
				_entityClassTreeView = view;
				_entityListStore = store;
			}
		}

		{
			// entity class comments
			GtkWidget* scr = gtk_scrolled_window_new(0, 0);
			gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
			gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

			{
				GtkTextView* text = GTK_TEXT_VIEW(gtk_text_view_new());
				widget_set_size(GTK_WIDGET(text), 0, 0); // as small as possible
				gtk_text_view_set_wrap_mode(text, GTK_WRAP_WORD);
				gtk_text_view_set_editable(text, FALSE);
				gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(text));
				_entityClassComment = text;
			}
		}

		{
			// Spawnflags (4 colums wide max, or window gets too wide.)
			GtkTable* table = GTK_TABLE(gtk_table_new(4, 4, FALSE));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, TRUE, 0);

			_spawnflagsTable = table;

			for (int i = 0; i < MAX_FLAGS; i++) {
				GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(""));
				gtk_widget_ref(GTK_WIDGET(check));
				g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(g_signal_connect(G_OBJECT(check),
						"toggled", G_CALLBACK(SpawnflagCheck_toggled), this)));
				_entitySpawnflagsCheck[i] = check;
			}
		}

		{
			// entity key/value list
			GtkWidget* scr = gtk_scrolled_window_new(0, 0);
			gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
			gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
			gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

			GtkTreeStore* store = gtk_tree_store_new(KEYVALLIST_MAX_COLUMN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
					G_TYPE_BOOLEAN, G_TYPE_STRING);

			GtkTreeView * view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
			_keyValuesTreeView = view;
			gtk_tree_view_set_enable_search(view, FALSE);
#if GTK_CHECK_VERSION(2,12,0)
			gtk_tree_view_set_show_expanders(view, FALSE);
			gtk_tree_view_set_level_indentation(view, 10);
			gtk_tree_view_set_tooltip_column(view, KEYVALLIST_COLUMN_TOOLTIP);
#endif
			/* expand all rows after the treeview widget has been realized */
			g_signal_connect(view, "realize", G_CALLBACK(gtk_tree_view_expand_all), NULL);
			{
				GtkCellRenderer* renderer = gtk_cell_renderer_combo_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Key"), renderer, "text",
						KEYVALLIST_COLUMN_KEY, (char const*) 0);
				gtk_tree_view_column_add_attribute(column, renderer, "weight", KEYVALLIST_COLUMN_STYLE);
				gtk_tree_view_column_add_attribute(column, renderer, "editable", KEYVALLIST_COLUMN_KEY_EDITABLE);
				{
					GtkListStore* rendererStore = gtk_list_store_new(1, G_TYPE_STRING);
					g_object_set(renderer, "model", GTK_TREE_MODEL(rendererStore), "text-column", 0, (const char*) 0);
					g_object_unref(G_OBJECT (rendererStore));
				}
				gtk_tree_view_append_column(view, column);
				/** @todo there seems to be a display problem with has-entries for worldspawn attributes using windows , check that */
				g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, "has-entry", FALSE, (char const*) 0);
				g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(entityKeyEdited), this);
				g_signal_connect(G_OBJECT(renderer), "editing-canceled", G_CALLBACK(entityKeyEditCanceled),
						this);
				g_signal_connect(G_OBJECT(renderer), "editing-started",
						G_CALLBACK(EntityKeyValueList_keyEditingStarted), this);
			}

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_combo_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Value"), renderer, "text",
						KEYVALLIST_COLUMN_VALUE, (char const*) 0);
				gtk_tree_view_column_add_attribute(column, renderer, "weight", KEYVALLIST_COLUMN_STYLE);
				gtk_tree_view_append_column(view, column);
				{
					GtkListStore* rendererStore = gtk_list_store_new(1, G_TYPE_STRING);
					g_object_set(renderer, "model", GTK_TREE_MODEL(rendererStore), "text-column", 0, (const char*) 0);
					g_object_unref(G_OBJECT (rendererStore));
				}
				g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, (char const*) 0);
				g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(entityValueEdited), this);
				g_signal_connect(G_OBJECT(renderer), "editing-started", G_CALLBACK(
								EntityKeyValueList_valueEditingStarted), this);
			}

			{
				GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
				g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(EntityKeyValueList_selection_changed), this);
			}

			gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(view));

			g_object_unref(G_OBJECT (store));

			_entityPropertiesTreeStore = store;

			// entity parameter action buttons
			GtkBox* hbox = GTK_BOX(gtk_hbox_new(TRUE, 4));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

			{
				GtkButton* button = GTK_BUTTON(gtk_button_new_from_stock(GTK_STOCK_REMOVE));
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(clearKeyValue), this);
				gtk_box_pack_start(hbox, GTK_WIDGET(button), TRUE, TRUE, 0);
				_removeKeyButton = button;
				gtk_widget_set_sensitive(GTK_WIDGET(_removeKeyButton), FALSE);
			}
			{
				GtkButton* button = GTK_BUTTON(gtk_button_new_from_stock(GTK_STOCK_NEW));
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(addKeyValue), this);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(button), TRUE, TRUE, 0);
				_addKeyButton = button;
				gtk_widget_set_sensitive(GTK_WIDGET(_addKeyButton), FALSE);
			}
		}
		{
			_attributeBox = GTK_VBOX(gtk_vbox_new(FALSE, 2));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(_attributeBox), FALSE, FALSE, 0);
		}
	}

	EntityClassList_fill();

	GlobalSelectionSystem().addSelectionChangeCallback(MemberCaller1<EntityInspector, const Selectable&,
			&EntityInspector::selectionChanged> (*this));

	return pageframe;
}

EntityInspector::EntityInspector () :
	_currentFlags(0), _currentComment(0), _attributeBox(0), _numNewKeys(0), m_unrealised(1)
{
}
void EntityInspector::realise ()
{
	if (--m_unrealised == 0) {
		EntityClassList_fill();
	}
}
void EntityInspector::unrealise ()
{
	if (++m_unrealised == 1) {
		EntityClassList_clear();
	}
}

EntityInspector& GlobalEntityInspector ()
{
	static EntityInspector _entityInspector;
	return _entityInspector;
}

void EntityInspector_Construct ()
{
	GlobalEntityClassManager().attach(GlobalEntityInspector());
}

void EntityInspector_Destroy ()
{
	GlobalEntityClassManager().detach(GlobalEntityInspector());
}

EntityInspectorDraw g_EntityInspectorDraw(GlobalEntityInspector());

void EntityInspector::selectionChanged (const Selectable&)
{
	g_EntityInspectorDraw.queueDraw();
}
