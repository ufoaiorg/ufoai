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

#include "debugging/debugging.h"

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
#include "../../ui/modelselector/ModelSelector.h"
#include "../../ui/common/SoundChooser.h"

#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/container.h"
#include "gtkutil/TreeModel.h"

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

/**
 * @brief Helper method returns the first value in value list for given key for currently selected entity class or an empty string.
 * @param key key to retrieve value for
 * @return first value in value list or empty string
 */
std::string SelectedEntity_getValueForKey (const std::string& key)
{
	ASSERT_MESSAGE(g_current_attributes != 0, "g_current_attributes is zero");
	ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
	if (it != g_selectedKeyValues.end()) {
		KeyValues &possibleValues = (*it).second;
		KeyValues::const_iterator i = possibleValues.find(key);
		if (i != possibleValues.end()) {
			const Values &values = (*i).second;
			ASSERT_MESSAGE(!values.empty(), "Values don't exist");
			return *values.begin();
		}
	}
	return "";
}

/*
 * Entity Inspector Dialog
 */

namespace
{
	enum
	{
		KEYVALLIST_COLUMN_KEY, KEYVALLIST_COLUMN_VALUE, KEYVALLIST_COLUMN_STYLE, /**< pango weigth value used to bold classname rows */
		KEYVALLIST_COLUMN_KEY_EDITABLE, /**< flag indicating whether key should be editable (has no value yet) */
		KEYVALLIST_COLUMN_TOOLTIP,

		KEYVALLIST_MAX_COLUMN
	};

	GtkButton *m_btnRemoveKey;
	GtkButton *m_btnAddKey;
	GtkTreeView *m_viewKeyValues;

	GtkTreeView* g_entityClassList;
	GtkTextView* g_entityClassComment;

	GtkCheckButton* g_entitySpawnflagsCheck[MAX_FLAGS];

	GtkListStore* g_entlist_store;
	GtkTreeStore* g_entprops_store;
	const EntityClass* g_current_flags = 0;
	const EntityClass* g_current_comment = 0;

	// the number of active spawnflags
	int g_spawnflag_count;
	// table: index, match spawnflag item to the spawnflag index (i.e. which bit)
	int spawn_table[MAX_FLAGS];
	// we change the layout depending on how many spawn flags we need to display
	// the table is a 4x4 in which we need to put the comment box g_entityClassComment and the spawn flags..
	GtkTable* g_spawnflagsTable;

	GtkVBox* g_attributeBox = 0;
	typedef std::vector<EntityAttribute*> EntityAttributes;
	EntityAttributes g_entityAttributes;

	int g_numNewKeys = 0;
}

void GlobalEntityAttributes_clear (void)
{
	for (EntityAttributes::iterator i = g_entityAttributes.begin(); i != g_entityAttributes.end(); ++i) {
		delete *i;
	}
	g_entityAttributes.clear();
}

/**
 * @brief visitor implementation used to add entities key/value pairs into a list.
 * @note key "classname" is ignored as this is supposed to be in parent map
 */
class GetKeyValueVisitor: public Entity::Visitor
{
		KeyValues &m_keyvalues;
	public:
		GetKeyValueVisitor (KeyValues& keyvalues) :
			m_keyvalues(keyvalues)
		{
		}

		void visit (const std::string& key, const std::string& value)
		{
			/* don't add classname property, as this will be in parent list */
			if (key == "classname")
				return;
			KeyValues::iterator keyIter = m_keyvalues.find(key);
			if (keyIter == m_keyvalues.end()) {
				m_keyvalues.insert(KeyValues::value_type(key, Values()));
				keyIter = m_keyvalues.find(key);
			}
			(*keyIter).second.push_back(Values::value_type(value));
		}
};

/**
 * @brief Adds all entities class attributes into the given map.
 *
 * @param entity entity to add into map
 * @param keyvalues list of key/values associated to their class names
 */
void Entity_GetKeyValues (const Entity& entity, ClassKeyValues& keyvalues)
{
	std::string classname = entity.getEntityClass().m_name;
	ClassKeyValues::iterator valuesIter = keyvalues.find(classname);
	if (valuesIter == keyvalues.end()) {
		keyvalues.insert(ClassKeyValues::value_type(classname, KeyValues()));
		valuesIter = keyvalues.find(classname);
	}

	GetKeyValueVisitor visitor((*valuesIter).second);

	entity.forEachKeyValue(visitor);
}

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
					Entity_GetKeyValues(*entity, m_keyvalues);
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

void EntityClassList_fill (void)
{
	if (g_entlist_store) {
		EntityClassListStoreAppend append(g_entlist_store);
		GlobalEntityClassManager().forEach(append);
	}
}

void EntityClassList_clear (void)
{
	if (GTK_IS_LIST_STORE(g_entlist_store)) {
		gtk_list_store_clear(g_entlist_store);
	}
}

void SetComment (EntityClass* eclass)
{
	if (eclass == g_current_comment)
		return;

	g_current_comment = eclass;

	GtkTextBuffer* buffer = gtk_text_view_get_buffer(g_entityClassComment);
	gtk_text_buffer_set_text(buffer, eclass->comments().c_str(), -1);
}

/**
 * @brief Updates surface flags buttons to display given entity class spawnflags
 * @param eclass entity class to display
 * @note This method hides all buttons, then updates as much as needed with new label and shows them.
 * @sa EntityInspector_updateSpawnflags
 */
void SurfaceFlags_setEntityClass (EntityClass* eclass)
{
	if (eclass == g_current_flags)
		return;

	g_current_flags = eclass;

	int spawnflag_count = 0;

	{
		// do a first pass to count the spawn flags, don't touch the widgets, we don't know in what state they are
		for (int i = 0; i < MAX_FLAGS; i++) {
			if (eclass->flagnames[i] && eclass->flagnames[i][0] != 0 && strcmp(eclass->flagnames[i], "-")) {
				spawn_table[spawnflag_count] = i;
				spawnflag_count++;
			}
		}
	}

	// disable all remaining boxes
	// NOTE: these boxes might not even be on display
	{
		for (int i = 0; i < g_spawnflag_count; ++i) {
			GtkWidget* widget = GTK_WIDGET(g_entitySpawnflagsCheck[i]);
			gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), " ");
			gtk_widget_hide(widget);
			gtk_widget_ref(widget);
			gtk_container_remove(GTK_CONTAINER(g_spawnflagsTable), widget);
		}
	}

	g_spawnflag_count = spawnflag_count;

	{
		for (int i = 0; i < g_spawnflag_count; ++i) {
			GtkWidget* widget = GTK_WIDGET(g_entitySpawnflagsCheck[i]);
			gtk_widget_show(widget);

			StringOutputStream str(16);
			str << LowerCase(eclass->flagnames[spawn_table[i]]);

			gtk_table_attach(g_spawnflagsTable, widget, i % 4, i % 4 + 1, i / 4, i / 4 + 1,
					(GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
			gtk_widget_unref(widget);

			gtk_label_set_text(GTK_LABEL(GTK_BIN(widget)->child), str.c_str());
		}
	}
}

void EntityClassList_selectEntityClass (EntityClass* eclass)
{
	GtkTreeModel* model = GTK_TREE_MODEL(g_entlist_store);
	GtkTreeIter iter;
	for (gboolean good = gtk_tree_model_get_iter_first(model, &iter); good != FALSE; good = gtk_tree_model_iter_next(
			model, &iter)) {
		char* text;
		gtk_tree_model_get(model, &iter, 0, &text, -1);
		if (eclass->name() == std::string(text)) {
			GtkTreeView* view = g_entityClassList;
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

void EntityInspector_appendAttribute (const std::string& name, EntityAttribute& attribute)
{
	GtkTable* row = DialogRow_new(name, attribute.getWidget());
	DialogVBox_packRow(g_attributeBox, GTK_WIDGET(row));
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
		EntityAttributeFactory (void)
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

void EntityInspector_checkAddNewKeys (void)
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
	g_numNewKeys = count;
}

void EntityInspector_setEntityClass (EntityClass *eclass)
{
	EntityClassList_selectEntityClass(eclass);
	SurfaceFlags_setEntityClass(eclass);

	if (eclass != g_current_attributes) {
		g_current_attributes = eclass;

		container_remove_all(GTK_CONTAINER(g_attributeBox));
		GlobalEntityAttributes_clear();

		for (EntityClassAttributes::const_iterator i = eclass->m_attributes.begin(); i != eclass->m_attributes.end(); ++i) {
			EntityAttribute* attribute = GlobalEntityAttributeFactory::instance().create(i->second.m_type, i->first);
			if (attribute != 0) {
				g_entityAttributes.push_back(attribute);
				EntityInspector_appendAttribute(EntityClassAttributePair_getName(*i), *g_entityAttributes.back());
			}
		}
	}
}

/**
 * @brief This method updates active state of spawnflag buttons based on current selected value
 * @sa SurfaceFlags_setEntityClass
 * @sa EntityInspector_updateGuiElements
 */
void EntityInspector_updateSpawnflags (void)
{
	int i;
	const int f = string::toInt(SelectedEntity_getValueForKey("spawnflags"));
	for (i = 0; i < g_spawnflag_count; ++i) {
		const int v = !!(f & (1 << spawn_table[i]));

		toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(g_entitySpawnflagsCheck[i]), v);
	}

	// take care of the remaining ones
	for (i = g_spawnflag_count; i < MAX_FLAGS; ++i) {
		toggle_button_set_active_no_signal(GTK_TOGGLE_BUTTON(g_entitySpawnflagsCheck[i]), FALSE);
	}
}
#include "string/string.h"

static const char* EntityInspector_getTooltipForKey (const std::string& key)
{
	EntityClassAttribute *attrib = g_current_attributes->getAttribute(key);
	if (attrib != NULL)
		return attrib->m_description.c_str();
	else
		return NULL;
}

/**
 * @brief This method updates key value list for current selected entities.
 * @sa EntityInspector_updateGuiElements
 */
void EntityInspector_updateKeyValueList (void)
{
	GtkTreeStore* store = g_entprops_store;

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
			ASSERT_MESSAGE(!values.empty(), "Values don't exist");
			std::string value = *values.begin();
			gtk_tree_store_set(store, &iter, KEYVALLIST_COLUMN_KEY, key.c_str(), KEYVALLIST_COLUMN_VALUE,
					value.c_str(), KEYVALLIST_COLUMN_STYLE, PANGO_WEIGHT_NORMAL, KEYVALLIST_COLUMN_KEY_EDITABLE, FALSE,
					KEYVALLIST_COLUMN_TOOLTIP, EntityInspector_getTooltipForKey(key), -1);
		}
	}
	//set all elements expanded
	gtk_tree_view_expand_all(m_viewKeyValues);
	EntityInspector_checkAddNewKeys();
}

// Entity Inspector redraw functor
class EntityInspectorDraw
{
	private:
		void updateGuiElements (void)
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
				EntityInspector_setEntityClass(GlobalEntityClassManager().findOrInsert(classname, false));
			}
			EntityInspector_updateSpawnflags();

			EntityInspector_updateKeyValueList();

			for (EntityAttributes::const_iterator i = g_entityAttributes.begin(); i != g_entityAttributes.end(); ++i) {
				(*i)->update();
			}
		}
		IdleDraw m_idleDraw;
	public:
		// Constructor
		EntityInspectorDraw () :
			m_idleDraw(MemberCaller<EntityInspectorDraw, &EntityInspectorDraw::updateGuiElements> (*this))
		{
		}
		void queueDraw (void)
		{
			m_idleDraw.queueDraw();
		}
};

static EntityInspectorDraw g_EntityInspectorDraw;

void EntityInspector_keyValueChanged (void)
{
	g_EntityInspectorDraw.queueDraw();
}

void EntityInspector_selectionChanged (const Selectable&)
{
	EntityInspector_keyValueChanged();
}

/**
 * @brief Creates a new entity based on the currently selected brush and entity type.
 */
void EntityClassList_createEntity (void)
{
	GtkTreeView* view = g_entityClassList;

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

void EntityInspector_addKeyValue (GtkButton *button, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeIter parent;
	GtkTreeView *view = GTK_TREE_VIEW(user_data);
	GtkTreeModel* model = gtk_tree_view_get_model(view);
	GtkTreeStore *store = GTK_TREE_STORE(model);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(view);

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
	gtk_tree_view_expand_all(m_viewKeyValues);

	/* select newly added field and start editing */
	GtkTreeViewColumn* viewColumn = gtk_tree_view_get_column(view, 0);
	GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_set_cursor(view, path, viewColumn, TRUE);
	gtk_tree_path_free(path);
	//disable new key as long as we edit
	g_object_set(m_btnAddKey, "sensitive", FALSE, (const char*) 0);
}

void EntityInspector_clearKeyValue (GtkButton * button, gpointer user_data)
{
	GtkTreeView *view = GTK_TREE_VIEW(user_data);
	GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
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

void EntityClassList_selection_changed (GtkTreeSelection* selection, gpointer data)
{
	EntityClass* eclass = (EntityClass*) gtkutil::TreeModel::getSelectedPointer(selection, 1);
	if (eclass != 0)
		SetComment(eclass);
}

static gint EntityClassList_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	// double clicked with the left mount button
	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
		EntityClassList_createEntity();
		return TRUE;
	}
	return FALSE;
}

static gint EntityClassList_keypress (GtkWidget* widget, GdkEventKey* event, gpointer data)
{
	unsigned int code = gdk_keyval_to_upper(event->keyval);

	if (event->keyval == GDK_Return) {
		EntityClassList_createEntity();
		return TRUE;
	}

	// select the entity that starts with the key pressed
	if (code <= 'Z' && code >= 'A') {
		GtkTreeView* view = g_entityClassList;
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

void SpawnflagCheck_toggled (GtkWidget *widget, gpointer data)
{
	int f, i;

	f = 0;
	for (i = 0; i < g_spawnflag_count; ++i) {
		const int v = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_entitySpawnflagsCheck[i]));
		f |= v << spawn_table[i];
	}

	std::string value = (f == 0) ? "" : string::toString(f);

	{
		std::string command = "entitySetFlags -classname " + g_current_flags->name() + "-flags " + string::toString(f);
		UndoableCommand undo(command);

		Scene_EntitySetKeyValue_Selected(g_current_flags->name(), "spawnflags", value);
	}
}

void entityKeyValueEdited (GtkTreeView *view, int columnIndex, char *newValue)
{
	char *key, *value, *classname, *oldvalue;
	bool isClassname = false;
	GtkTreeModel* model;
	GtkTreeIter iter, parent;
	GtkTreeSelection* selection = gtk_tree_view_get_selection(view);

	if (!newValue)
		return;

	if (gtk_tree_selection_get_selected(selection, &model, &iter) == FALSE) {
		g_warning("No entity parameter selected to change the value for\n");
		return;
	}

	if (columnIndex == 1) {
		gtk_tree_model_get(model, &iter, KEYVALLIST_COLUMN_KEY, &key, KEYVALLIST_COLUMN_VALUE, &oldvalue, -1);
		value = newValue;
	} else {
		gtk_tree_model_get(model, &iter, KEYVALLIST_COLUMN_KEY, &oldvalue, KEYVALLIST_COLUMN_VALUE, &value, -1);
		key = newValue;
	}
	if (std::string(oldvalue) == std::string(newValue)) {
		g_debug("Old and new value are the same, aborting edit.");
		return;
	}
	if (gtk_tree_model_iter_parent(model, &parent, &iter) == FALSE) {
		gtk_tree_model_get(model, &iter, KEYVALLIST_COLUMN_VALUE, &classname, -1);
		isClassname = true;
	} else {
		gtk_tree_model_get(model, &parent, KEYVALLIST_COLUMN_VALUE, &classname, -1);
	}

	// Get current selection text
	StringOutputStream keyConverted(64);
	keyConverted << ConvertUTF8ToLocale(key);
	StringOutputStream valueConverted(64);
	valueConverted << ConvertUTF8ToLocale(value);
	StringOutputStream classnameConverted(64);
	classnameConverted << ConvertUTF8ToLocale(classname);

	// if you change the classname to worldspawn you won't merge back in the structural
	// brushes but create a parasite entity
	if (isClassname && valueConverted.toString() == "worldspawn") {
		gtkutil::errorDialog(_("Cannot change \"classname\" key back to worldspawn."));
		return;
	}

	// we don't want spaces in entity keys
	if (strstr(keyConverted.c_str(), " ")) {
		gtkutil::errorDialog(_("No spaces are allowed in entity keys."));
		return;
	}

	//instead of updating key/values right now, try to get default value and edit it before setting final values
	if (columnIndex == 0 && !keyConverted.empty()) {
		if (valueConverted.empty())
			valueConverted << g_current_attributes->getDefaultForAttribute(keyConverted.toString());
		gtk_tree_store_set(GTK_TREE_STORE(model), &iter, KEYVALLIST_COLUMN_KEY, keyConverted.c_str(),
				KEYVALLIST_COLUMN_VALUE, valueConverted.c_str(), KEYVALLIST_COLUMN_TOOLTIP,
				EntityInspector_getTooltipForKey(keyConverted.toString()), -1);
		g_currentSelectedKey = keyConverted.toString();
		GtkTreePath* currentPath = gtk_tree_model_get_path(model, &iter);
		GtkTreeViewColumn *column = gtk_tree_view_get_column(view, KEYVALLIST_COLUMN_VALUE);
		gtk_tree_view_set_cursor(view, currentPath, column, true);
		gtk_tree_path_free(currentPath);
		return;
	}

	if (isClassname) {
		std::string command = "entitySetClass -class " + classnameConverted.toString() + " -newclass "
				+ valueConverted.toString();
		UndoableCommand undo(command);
		Scene_EntitySetClassname_Selected(classnameConverted.toString(), valueConverted.toString());
	} else {
		std::string command = "entitySetKeyValue -classname \"" + classnameConverted.toString() + "\" -key \""
				+ keyConverted.toString() + "\" -value \"" + valueConverted.toString() + "\"";
		UndoableCommand undo(command);
		Scene_EntitySetKeyValue_Selected(classnameConverted.toString(), keyConverted.toString(), valueConverted.toString());
	}
	gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
}

void entityValueEdited (GtkCellRendererText *renderer, gchar *path, gchar* new_text, GtkTreeView *view)
{
	entityKeyValueEdited(view, 1, new_text);
}

void entityKeyEdited (GtkCellRendererText *renderer, gchar *path, gchar* new_text, GtkTreeView *view)
{
	entityKeyValueEdited(view, 0, new_text);
}

/**
 * @brief callback invoked when key edit was canceled, used to remove newly added empty keys.
 * @param renderer cell renderer used to edit
 * @param view treeview that is edited
 */
void entityKeyEditCanceled (GtkCellRendererText *renderer, GtkTreeView *view)
{
	char *oldKey;

	g_object_get(G_OBJECT(renderer), "text", &oldKey, (char*) 0);
	StringOutputStream keyConverted(64);
	keyConverted << ConvertUTF8ToLocale(oldKey);
	if (keyConverted.empty()) {
		GtkTreeModel* model;
		GtkTreeIter iter;
		/* retrieve current selection and iter*/
		GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
		gtk_tree_selection_get_selected(selection, &model, &iter);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	}
}

/**
 * @brief Callback for selection changed in entity key value list used to enable/disable buttons
 * @param selection current selection
 */
void EntityKeyValueList_selection_changed (GtkTreeSelection* selection)
{
	if (gtk_tree_selection_count_selected_rows(selection) == 0) {
		gtk_widget_set_sensitive(GTK_WIDGET(m_btnRemoveKey), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(m_btnAddKey), FALSE);
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
			EntityInspector_setEntityClass(eclass);
		}

		if (attribKey.length() > 0)
			g_currentSelectedKey = attribKey;

		if (removeAllowed) {
			EntityClassAttribute *attribute = g_current_attributes->getAttribute(attribKey);
			if (attribute && attribute->m_mandatory)
				removeAllowed = false;
		}
		gtk_widget_set_sensitive(GTK_WIDGET(m_btnRemoveKey), removeAllowed);
		gtk_widget_set_sensitive(GTK_WIDGET(m_btnAddKey), (g_numNewKeys > 0));
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
void EntityKeyValueList_keyEditingStarted (GtkCellRenderer *renderer, GtkCellEditable *editable,
		const gchar *path, gpointer *user_data)
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
void EntityKeyValueList_fillValueComboWithClassnames (GtkCellRenderer *renderer)
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
void EntityKeyValueList_valueEditingStarted (GtkCellRenderer *renderer, GtkCellEditable *editable,
		const gchar *path, gpointer *user_data)
{
	// prevent update if already displaying anything
	{
		bool editing;
		g_object_get(G_OBJECT(renderer), "editing", &editing, (const char*) 0);
		if (editing)
			return;
	}

	if (g_currentSelectedKey.empty())
		return;
	if (g_currentSelectedKey == "classname") {
		EntityKeyValueList_fillValueComboWithClassnames(renderer);
		return;
	}
	ClassKeyValues::iterator it = g_selectedKeyValues.find(g_current_attributes->m_name);
	if (it == g_selectedKeyValues.end()) {
		g_warning("leaving updateValueCombo... no class attributes");
		return;
	}
	KeyValues possibleValues = (*it).second;
	KeyValues::const_iterator keyIter = possibleValues.find(g_currentSelectedKey);
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
GtkWidget* EntityInspector_constructNotebookTab (void)
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
				g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(EntityClassList_button_press), 0);
				g_signal_connect(G_OBJECT(view), "key_press_event", G_CALLBACK(EntityClassList_keypress), 0);

				{
					GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
					GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(_("Key"), renderer, "text", 0,
							(char const*) 0);
					gtk_tree_view_append_column(view, column);
				}

				{
					GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
					g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(EntityClassList_selection_changed), 0);
				}

				gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(view));

				g_object_unref(G_OBJECT (store));
				g_entityClassList = view;
				g_entlist_store = store;
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
				g_entityClassComment = text;
			}
		}

		{
			// Spawnflags (4 colums wide max, or window gets too wide.)
			GtkTable* table = GTK_TABLE(gtk_table_new(4, 4, FALSE));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, TRUE, 0);

			g_spawnflagsTable = table;

			for (int i = 0; i < MAX_FLAGS; i++) {
				GtkCheckButton* check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(""));
				gtk_widget_ref(GTK_WIDGET(check));
				g_object_set_data(G_OBJECT(check), "handler", gint_to_pointer(g_signal_connect(G_OBJECT(check),
						"toggled", G_CALLBACK(SpawnflagCheck_toggled), 0)));
				g_entitySpawnflagsCheck[i] = check;
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
				g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(entityKeyEdited), (gpointer) view);
				g_signal_connect(G_OBJECT(renderer), "editing-canceled", G_CALLBACK(entityKeyEditCanceled),
						(gpointer) view);
				g_signal_connect(G_OBJECT(renderer), "editing-started",
						G_CALLBACK(EntityKeyValueList_keyEditingStarted), (gpointer) view);
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
				g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(entityValueEdited), (gpointer) view);
				g_signal_connect(G_OBJECT(renderer), "editing-started", G_CALLBACK(
								EntityKeyValueList_valueEditingStarted), (gpointer) view);
			}

			{
				GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
				g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(EntityKeyValueList_selection_changed), NULL);
			}
			m_viewKeyValues = view;

			gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(view));

			g_object_unref(G_OBJECT (store));

			g_entprops_store = store;

			// entity parameter action buttons
			GtkBox* hbox = GTK_BOX(gtk_hbox_new(TRUE, 4));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

			{
				GtkButton* button = GTK_BUTTON(gtk_button_new_from_stock(GTK_STOCK_REMOVE));
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(EntityInspector_clearKeyValue), gpointer(view));
				gtk_box_pack_start(hbox, GTK_WIDGET(button), TRUE, TRUE, 0);
				m_btnRemoveKey = button;
				gtk_widget_set_sensitive(GTK_WIDGET(m_btnRemoveKey), FALSE);
			}
			{
				GtkButton* button = GTK_BUTTON(gtk_button_new_from_stock(GTK_STOCK_NEW));
				g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(EntityInspector_addKeyValue), gpointer(view));
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(button), TRUE, TRUE, 0);
				m_btnAddKey = button;
				gtk_widget_set_sensitive(GTK_WIDGET(m_btnAddKey), FALSE);
			}
		}
		{
			g_attributeBox = GTK_VBOX(gtk_vbox_new(FALSE, 2));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(g_attributeBox), FALSE, FALSE, 0);
		}
	}

	EntityClassList_fill();

	typedef FreeCaller1<const Selectable&, EntityInspector_selectionChanged> EntityInspectorSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback(EntityInspectorSelectionChangedCaller());
	GlobalEntityCreator().setKeyValueChangedFunc(EntityInspector_keyValueChanged);

	return pageframe;
}

/// todo remove me?
class EntityInspector: public ModuleObserver
{
		std::size_t m_unrealised;
	public:
		EntityInspector () :
			m_unrealised(1)
		{
		}
		void realise (void)
		{
			if (--m_unrealised == 0) {
				EntityClassList_fill();
			}
		}
		void unrealise (void)
		{
			if (++m_unrealised == 1) {
				EntityClassList_clear();
			}
		}
};

EntityInspector g_EntityInspector;

#include "preferencesystem.h"
#include "stringio.h"

void EntityInspector_Construct (void)
{
	GlobalEntityClassManager().attach(g_EntityInspector);
}

void EntityInspector_Destroy (void)
{
	GlobalEntityClassManager().detach(g_EntityInspector);
}
