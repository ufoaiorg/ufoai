#include "../sidebar.h"
#include "EntityInspector.h"
#include "PropertyEditorFactory.h"
#include "AddPropertyDialog.h"

#include "radiant_i18n.h"

#include "ientity.h"
#include "ieclass.h"
#include "iselection.h"
#include "iundo.h"
#include "iregistry.h"

#include "scenelib.h"
#include "selectionlib.h"
#include "gtkutil/dialog.h"
#include "gtkutil/StockIconMenuItem.h"
#include "gtkutil/SeparatorMenuItem.h"
#include "gtkutil/TreeModel.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/image.h"

#include "xmlutil/Document.h"
#include "signal/signal.h"
#include "../../map/map.h"

#include <map>
#include <string>

#include <gtk/gtk.h>

namespace ui {

/* CONSTANTS */

namespace {

const int TREEVIEW_MIN_WIDTH = 220;
const int TREEVIEW_MIN_HEIGHT = 60;
const int PROPERTYEDITORPANE_MIN_HEIGHT = 90;

const char* PROPERTY_NODES_XPATH = "game/entityInspector//property";

const std::string HELP_ICON_NAME = "helpicon.png";

// TreeView column numbers
enum
{
	PROPERTY_NAME_COLUMN,
	PROPERTY_VALUE_COLUMN,
	TEXT_COLOUR_COLUMN,
	PROPERTY_ICON_COLUMN,
	HELP_ICON_COLUMN,
	HAS_HELP_FLAG_COLUMN,
	N_COLUMNS
};

}

// Constructor creates UI components for the EntityInspector dialog

EntityInspector::EntityInspector () :
	_listStore(gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF,
			GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN)), _treeView(gtk_tree_view_new_with_model(GTK_TREE_MODEL(_listStore))),
			_helpColumn(NULL), _contextMenu(gtkutil::PopupMenu(_treeView))
{
	_widget = gtk_vbox_new(FALSE, 0);

	// Pack in GUI components

	GtkWidget* topHBox = gtk_hbox_new(FALSE, 6);

	_showHelpColumnCheckbox = gtk_check_button_new_with_label(_("Show help icons"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(_showHelpColumnCheckbox), FALSE);
	g_signal_connect(G_OBJECT(_showHelpColumnCheckbox), "toggled", G_CALLBACK(_onToggleShowHelpIcons), this);

	gtk_box_pack_start(GTK_BOX(topHBox), _showHelpColumnCheckbox, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(_widget), topHBox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(_widget), createTreeViewPane(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(_widget), createDialogPane(), FALSE, FALSE, 0);

	// Create the context menu
	createContextMenu();

	// Stimulate initial redraw to get the correct status
	requestIdleCallback();

	// Set the function to call when a keyval is changed. This is a requirement
	// of the EntityCreator interface.
	GlobalEntityCreator().setKeyValueChangedFunc(EntityInspector::keyValueChanged);

	// Register self to the SelectionSystem to get notified upon selection changes.
	GlobalSelectionSystem().addObserver(this);

	// Observe the Undo system for undo/redo operations, to refresh the
	// keyvalues when this happens
	GlobalUndoSystem().addObserver(this);
}

void EntityInspector::_onAddKey (gpointer data, gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	// Obtain the entity class to provide to the AddPropertyDialog
	const EntityClass& ec = self->_selectedEntity->getEntityClass();

	// Choose a property, and add to entity with a default value
	std::string property = AddPropertyDialog::chooseProperty(&ec);
	if (!property.empty()) {
		// Save last key, so that it will be automatically selected
		self->_lastKey = property;

		// Add the keyvalue on the entity (triggering the refresh)
		self->_selectedEntity->setKeyValue(property, ec.getDefaultForAttribute(property));
	}
}

void EntityInspector::_onDeleteKey (gpointer data, gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	std::string property = self->getListSelection(PROPERTY_NAME_COLUMN);
	if (!property.empty())
		self->_selectedEntity->setKeyValue(property, "");
}

bool EntityInspector::_testDeleteKey (gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	// Make sure the Delete item is not available for the classname
	const std::string& name = self->getListSelection(PROPERTY_NAME_COLUMN);
	if (name != "classname") {
		// don't allow to delete mandatory properties
		const EntityClassAttribute* attr = self->_selectedEntity->getEntityClass().getAttribute(name);
		if (attr != NULL)
			return !attr->mandatory;
		return true;
	}

	return false;
}

void EntityInspector::_onCopyKey (gpointer data, gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	std::string key = self->getListSelection(PROPERTY_NAME_COLUMN);
	std::string value = self->getListSelection(PROPERTY_VALUE_COLUMN);

	if (!key.empty()) {
		self->_clipBoard.key = key;
		self->_clipBoard.value = value;
	}
}

bool EntityInspector::_testCopyKey (gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	return !self->getListSelection(PROPERTY_NAME_COLUMN).empty();
}

void EntityInspector::_onCutKey (gpointer data, gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	std::string key = self->getListSelection(PROPERTY_NAME_COLUMN);
	std::string value = self->getListSelection(PROPERTY_VALUE_COLUMN);

	if (!key.empty() && self->_selectedEntity != NULL) {
		self->_clipBoard.key = key;
		self->_clipBoard.value = value;

		// Clear the key after copying
		self->_selectedEntity->setKeyValue(key, "");
	}
}

bool EntityInspector::_testCutKey (gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	// Make sure the Cut item is not available for the classname
	const std::string& name = self->getListSelection(PROPERTY_NAME_COLUMN);
	if (name != "classname") {
		// don't allow to cut mandatory properties
		const EntityClassAttribute* attr = self->_selectedEntity->getEntityClass().getAttribute(name);
		if (attr != NULL && attr->mandatory)
			return false;
		// return true only if selection is not empty
		return !self->getListSelection(PROPERTY_NAME_COLUMN).empty();
	}
	return false;
}

void EntityInspector::_onPasteKey (gpointer data, gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	if (!self->_clipBoard.key.empty() && !self->_clipBoard.value.empty()) {
		// Pass the call
		self->applyKeyValueToSelection(self->_clipBoard.key, self->_clipBoard.value);
	}
}

bool EntityInspector::_testPasteKey (gpointer userData)
{
	EntityInspector* self = (EntityInspector*) userData;
	if (GlobalSelectionSystem().getSelectionInfo().entityCount == 0) {
		// No entities selected
		return false;
	}

	// Return true if the clipboard contains data
	return !self->_clipBoard.key.empty() && !self->_clipBoard.value.empty();
}

// Create the context menu
void EntityInspector::createContextMenu ()
{
	// Menu items
	GtkWidget* addKey = gtkutil::StockIconMenuItem(GTK_STOCK_ADD, _("Add property...")
	);
	GtkWidget* delKey = gtkutil::StockIconMenuItem(GTK_STOCK_DELETE, _("Delete property")
	);

	// Add the menu items to the PopupMenu
	_contextMenu.addItem(addKey, _onAddKey, this);
	_contextMenu.addItem(delKey, _onDeleteKey, this, _testDeleteKey);

	// TODO: mattn
	//_contextMenu.addItem(gtkutil::SeparatorMenuItem(), gtkutil::PopupMenu::Callback, NULL);

	_contextMenu.addItem(gtkutil::StockIconMenuItem(GTK_STOCK_COPY, _("Copy Spawnarg")), _onCopyKey, this, _testCopyKey);
	_contextMenu.addItem(gtkutil::StockIconMenuItem(GTK_STOCK_CUT, _("Cut Spawnarg")), _onCutKey, this, _testCutKey);
	_contextMenu.addItem(gtkutil::StockIconMenuItem(GTK_STOCK_PASTE, _("Paste Spawnarg")), _onPasteKey, this,
			_testPasteKey);
}

void EntityInspector::postUndo()
{
	// Now rescan the selection and update the stores
	Instance().requestIdleCallback();
}

void EntityInspector::postRedo()
{
	// Now rescan the selection and update the stores
	Instance().requestIdleCallback();
}

// Return the singleton EntityInspector instance, creating it if it is not yet
// created. Single-threaded design.

EntityInspector& EntityInspector::Instance ()
{
	static EntityInspector _instance;
	return _instance;
}

// Return the Gtk widget for the EntityInspector dialog.

GtkWidget* EntityInspector::getWidget () const
{
	gtk_widget_show_all(_widget);
	return _widget;
}

const std::string EntityInspector::getTitle() const
{
	return _("Entityinspector");
}


// Create the dialog pane

GtkWidget* EntityInspector::createDialogPane ()
{
	GtkWidget* hbx = gtk_hbox_new(FALSE, 0);
	_editorFrame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(_editorFrame), GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(hbx), _editorFrame, TRUE, TRUE, 0);
	gtk_widget_set_size_request(hbx, 0, PROPERTYEDITORPANE_MIN_HEIGHT);
	return hbx;
}

// Create the TreeView pane

GtkWidget* EntityInspector::createTreeViewPane ()
{
	GtkWidget* vbx = gtk_vbox_new(FALSE, 3);

	// Create the Property column
	GtkTreeViewColumn* nameCol = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(nameCol, "Property");
	gtk_tree_view_column_set_sizing(nameCol, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_spacing(nameCol, 3);

	GtkCellRenderer* pixRenderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(nameCol, pixRenderer, FALSE);
	gtk_tree_view_column_set_attributes(nameCol, pixRenderer, "pixbuf", PROPERTY_ICON_COLUMN, NULL);

	GtkCellRenderer* textRenderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(nameCol, textRenderer, FALSE);
	gtk_tree_view_column_set_attributes(nameCol, textRenderer, "text", PROPERTY_NAME_COLUMN, "foreground",
			TEXT_COLOUR_COLUMN, NULL);

	gtk_tree_view_column_set_sort_column_id(nameCol, PROPERTY_NAME_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(_treeView), nameCol);

	// Create the value column
	GtkTreeViewColumn* valCol = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(valCol, "Value");
	gtk_tree_view_column_set_sizing(valCol, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

	GtkCellRenderer* valRenderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(valCol, valRenderer, TRUE);
	gtk_tree_view_column_set_attributes(valCol, valRenderer, "text", PROPERTY_VALUE_COLUMN, "foreground",
			TEXT_COLOUR_COLUMN, NULL);

	gtk_tree_view_column_set_sort_column_id(valCol, PROPERTY_VALUE_COLUMN);
	gtk_tree_view_append_column(GTK_TREE_VIEW(_treeView), valCol);

	// Help column
	_helpColumn = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(_helpColumn, "?");
	gtk_tree_view_column_set_spacing(_helpColumn, 3);
	gtk_tree_view_column_set_visible(_helpColumn, FALSE);

	GdkPixbuf* helpIcon = gtkutil::getLocalPixbuf(HELP_ICON_NAME);
	if (helpIcon != NULL) {
		gtk_tree_view_column_set_fixed_width(_helpColumn, gdk_pixbuf_get_width(helpIcon));
	}

	// Add the help icon
	GtkCellRenderer* pixRend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(_helpColumn, pixRend, FALSE);
	gtk_tree_view_column_set_attributes(_helpColumn, pixRend, "pixbuf", HELP_ICON_COLUMN, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(_treeView), _helpColumn);

	// Connect the tooltip query signal to our custom routine
	g_object_set(G_OBJECT(_treeView), "has-tooltip", TRUE, NULL);
	g_signal_connect(G_OBJECT(_treeView), "query-tooltip", G_CALLBACK(_onQueryTooltip), this);

	// Set up the signals
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(_treeView));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(callbackTreeSelectionChanged), this);

	// Embed the TreeView in a scrolled viewport
	GtkWidget* scrollWin = gtkutil::ScrolledFrame(_treeView);
	gtk_widget_set_size_request(_treeView, TREEVIEW_MIN_WIDTH, TREEVIEW_MIN_HEIGHT);

	gtk_box_pack_start(GTK_BOX(vbx), scrollWin, TRUE, TRUE, 0);

	// Pack in the key and value edit boxes
	_keyEntry = gtk_entry_new();
	_valEntry = gtk_entry_new();

	GtkWidget* setButton = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(setButton), gtk_image_new_from_stock(GTK_STOCK_APPLY, GTK_ICON_SIZE_MENU));
	GtkWidget* setButtonBox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(setButtonBox), _valEntry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(setButtonBox), setButton, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbx), _keyEntry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbx), setButtonBox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbx), gtk_hseparator_new(), FALSE, FALSE, 0);

	// Signals for entry boxes
	g_signal_connect(
			G_OBJECT(setButton), "clicked", G_CALLBACK(_onSetProperty), this);
	g_signal_connect(
			G_OBJECT(_keyEntry), "activate", G_CALLBACK(_onEntryActivate), this);
	g_signal_connect(
			G_OBJECT(_valEntry), "activate", G_CALLBACK(_onEntryActivate), this);

	return vbx;
}

// Retrieve the selected string from the given property in the list store

std::string EntityInspector::getListSelection (int col)
{
	// Prepare to get the selection
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(_treeView));
	GtkTreeIter tmpIter;

	// Return the selected string if available, else a blank string
	if (gtk_tree_selection_get_selected(selection, NULL, &tmpIter)) {
		return gtkutil::TreeModel::getString(GTK_TREE_MODEL(_listStore), &tmpIter, col);
	} else {
		return "";
	}
}

// Redraw the GUI elements
void EntityInspector::onGtkIdle ()
{
	// Entity Inspector can only be used on a single entity. Multiple selections
	// or nothing selected result in a grayed-out dialog, as does the selection
	// of something that is not an Entity (worldspawn).

	if (updateSelectedEntity()) {
		gtk_widget_set_sensitive(_editorFrame, TRUE);
		gtk_widget_set_sensitive(_treeView, TRUE);
		gtk_widget_set_sensitive(_showHelpColumnCheckbox, TRUE);

		refreshTreeModel(); // get values, already have category tree
	} else {

		// Remove the displayed PropertyEditor
		if (_currentPropertyEditor) {
			delete _currentPropertyEditor;
			_currentPropertyEditor = NULL;
		}

		// Disable the dialog and clear the TreeView
		gtk_widget_set_sensitive(_editorFrame, FALSE);
		gtk_widget_set_sensitive(_treeView, FALSE);
		gtk_widget_set_sensitive(_showHelpColumnCheckbox, FALSE);

		gtk_list_store_clear(_listStore);
	}
}

// Entity keyvalue changed callback
void EntityInspector::keyValueChanged ()
{
	// Redraw the entity inspector GUI
	Instance().requestIdleCallback();

	// Set the map modified flag
	if (Instance()._selectedEntity != NULL)
		GlobalMap().setModified(true);
}

// Selection changed callback
void EntityInspector::selectionChanged (scene::Instance& instance, bool isComponent)
{
	Instance().requestIdleCallback();
}

namespace {

// SelectionSystem visitor to set a keyvalue on each entity, checking for
// func_static-style name=model requirements
class EntityKeySetter: public SelectionSystem::Visitor
{
		// Key and value to set on all entities
		std::string _key;
		std::string _value;

	public:

		// Construct with key and value to set
		EntityKeySetter (const std::string& k, const std::string& v) :
			_key(k), _value(v)
		{
		}

		// Required visit function
		void visit (scene::Instance& instance) const
		{
			Entity* entity = Node_getEntity(instance.path().top());
			if (entity) {
				// Set the actual value
				entity->setKeyValue(_key, _value);
			} else if (Node_isPrimitive(instance.path().top())) {
				scene::Instance* parent = instance.getParent();
				// no parent available
				if (parent == NULL)
					return;

				Entity* parentEnt = Node_getEntity(parent->path().top());
				if (parentEnt != NULL) {
					// We have child primitive of an entity selected, the change
					// should go right into that parent entity
					parentEnt->setKeyValue(_key, _value);
				}
			}
		}
};
}

// Set entity property from entry boxes

void EntityInspector::setPropertyFromEntries ()
{
	// Get the key from the entry box
	std::string key = gtk_entry_get_text(GTK_ENTRY(_keyEntry));
	std::string val = gtk_entry_get_text(GTK_ENTRY(_valEntry));

	//  Baal: check key and value strings for newline characters
	if (string::removeNewlines(key))
		gtk_entry_set_text(GTK_ENTRY(_keyEntry ), key.c_str());
	if (string::removeNewlines(val))
		gtk_entry_set_text(GTK_ENTRY(_valEntry ), val.c_str());

	// Pass the call to the specialised routine
	applyKeyValueToSelection(key, val);
}

void EntityInspector::applyKeyValueToSelection (const std::string& key, const std::string& val)
{
	// greebo: Instantiate a scoped object to make this operation undoable
	UndoableCommand command("entitySetProperty");

	if (key.empty() || key == "classname") {
		return;
	}

	// Use EntityKeySetter to set value on all selected entities
	EntityKeySetter setter(key, val);
	GlobalSelectionSystem().foreachSelected(setter);

}

// Construct and return static PropertyMap instance
const PropertyParmMap& EntityInspector::getPropertyMap ()
{
	// Static instance of local class, which queries the XML Registry
	// upon construction and adds the property nodes to the map.

	struct PropertyMapConstructor
	{
			// Map to construct
			PropertyParmMap _map;

			// Constructor queries the XML registry
			PropertyMapConstructor ()
			{
				xml::NodeList pNodes = GlobalRegistry().findXPath(PROPERTY_NODES_XPATH);
				for (xml::NodeList::const_iterator iter = pNodes.begin(); iter != pNodes.end(); ++iter) {
					PropertyParms parms;
					parms.type = iter->getAttributeValue("type");
					parms.options = iter->getAttributeValue("options");
					_map.insert(PropertyParmMap::value_type(iter->getAttributeValue("name"), parms));
				}
			}
	};
	static PropertyMapConstructor _propMap;

	// Return the constructed map
	return _propMap._map;
}

/* GTK CALLBACKS */

// Called when the TreeView selects a different property
void EntityInspector::callbackTreeSelectionChanged (GtkWidget* widget, EntityInspector* self)
{
	self->treeSelectionChanged();
}

void EntityInspector::_onSetProperty (GtkWidget* button, EntityInspector* self)
{
	self->setPropertyFromEntries();
}

// ENTER key in entry boxes
void EntityInspector::_onEntryActivate (GtkWidget* w, EntityInspector* self)
{
	// Set property and move back to key entry
	self->setPropertyFromEntries();
	gtk_widget_grab_focus(self->_keyEntry);
}

void EntityInspector::_onToggleShowHelpIcons (GtkToggleButton* b, EntityInspector* self)
{
	// Set the visibility of the column accordingly
	gtk_tree_view_column_set_visible(self->_helpColumn, gtk_toggle_button_get_active(b));
}

gboolean EntityInspector::_onQueryTooltip (GtkWidget* widget, gint x, gint y, gboolean keyboard_mode,
		GtkTooltip* tooltip, EntityInspector* self)
{
	if (self->_selectedEntity == NULL)
		return FALSE; // no single entity selected

	GtkTreeView* tv = GTK_TREE_VIEW(widget);
	bool showToolTip = false;

	// greebo: Important: convert the widget coordinates to bin coordinates first
	gint binX, binY;
	gtk_tree_view_convert_widget_to_bin_window_coords(tv, x, y, &binX, &binY);

	gint cellx, celly;
	GtkTreeViewColumn* column = NULL;
	GtkTreePath* path = NULL;

	if (gtk_tree_view_get_path_at_pos(tv, binX, binY, &path, &column, &cellx, &celly)) {
		// Get the iter of the row pointed at
		GtkTreeIter iter;
		GtkTreeModel* model = GTK_TREE_MODEL(self->_listStore);
		if (gtk_tree_model_get_iter(model, &iter, path)) {
			bool hasHelp = gtkutil::TreeModel::getBoolean(model, &iter, HAS_HELP_FLAG_COLUMN);

			if (hasHelp) {
				const std::string key = gtkutil::TreeModel::getString(model, &iter, PROPERTY_NAME_COLUMN);

				const EntityClass& eclass = self->_selectedEntity->getEntityClass();
				// Find the attribute on the eclass, that's where the descriptions are defined
				const EntityClassAttribute* attr = eclass.getAttribute(key);
				if (attr != NULL && !attr->description.empty()) {
					// Check the description of the focused item
					gtk_tree_view_set_tooltip_row(tv, tooltip, path);
					gtk_tooltip_set_markup(tooltip, attr->description.c_str());
					return TRUE;
				}
			}

			return FALSE;
		}

		return FALSE;
	}

	if (path != NULL) {
		gtk_tree_path_free(path);
	}

	return showToolTip ? TRUE : FALSE;
}

/* END GTK CALLBACKS */

// Update the PropertyEditor pane, displaying the PropertyEditor if necessary
// and making sure it refers to the currently-selected Entity.
void EntityInspector::treeSelectionChanged ()
{
	// Abort if called without a valid entity selection (may happen during
	// various cleanup operations).
	if (_selectedEntity == NULL)
		return;

	// Get the selected key and value in the tree view
	std::string key = getListSelection(PROPERTY_NAME_COLUMN);
	std::string value = getListSelection(PROPERTY_VALUE_COLUMN);
	if (!key.empty())
		_lastKey = key; // save last key

	// Get the type for this key if it exists, and the options
	PropertyParmMap::const_iterator tIter = getPropertyMap().find(key);
	std::string type = (tIter != getPropertyMap().end() ? tIter->second.type : "");
	std::string options = (tIter != getPropertyMap().end() ? tIter->second.options : "");

	// If the type was not found, also try looking on the entity class
	if (type.empty() && !key.empty()) {
		const EntityClass& eclass = _selectedEntity->getEntityClass();
		const EntityClassAttribute *attr = eclass.getAttribute(key);
		if (attr == NULL)
			type = "";
		else
			type = attr->type;
	}

	// Remove the existing PropertyEditor widget, if there is one
	GtkWidget* existingWidget = gtk_bin_get_child(GTK_BIN(_editorFrame));
	if (existingWidget != NULL)
		gtk_widget_destroy(existingWidget);

	if (_currentPropertyEditor)
		delete _currentPropertyEditor;
	// Construct and add a new PropertyEditor
	_currentPropertyEditor = PropertyEditorFactory::create(type, _selectedEntity, key, options);

	// If the creation was successful (because the PropertyEditor type exists),
	// add its widget to the editor pane
	if (_currentPropertyEditor) {
		gtk_container_add(GTK_CONTAINER(_editorFrame), _currentPropertyEditor->getWidget());
	}

	// Update key and value entry boxes, but only if there is a key value. If
	// there is no selection we do not clear the boxes, to allow keyval copying
	// between entities.
	if (!key.empty()) {
		gtk_entry_set_text(GTK_ENTRY(_keyEntry), key.c_str());
		gtk_entry_set_text(GTK_ENTRY(_valEntry), value.c_str());
	}
}

// Main refresh function.
void EntityInspector::refreshTreeModel ()
{
	// Clear the existing list
	gtk_list_store_clear(_listStore);

	// Local functor to enumerate keyvals on object and add them to the list
	// view.

	class ListPopulateVisitor: public Entity::Visitor
	{
			// List store to populate
			GtkListStore* _store;

			// Property map to look up types
			const PropertyParmMap& _map;

			// Entity class to check for types
			const EntityClass& _eclass;

			// Last selected key to highlight
			std::string _lastKey;

			// TreeIter to select, if we find the last-selected key
			GtkTreeIter* _lastIter;

		public:

			// Constructor
			ListPopulateVisitor (GtkListStore* store, const PropertyParmMap& map, const EntityClass& cls,
					const std::string& lastKey) :
				_store(store), _map(map), _eclass(cls), _lastKey(lastKey), _lastIter(NULL)
			{
			}

			// Required visit function
			virtual void visit (const std::string& key, const std::string& value)
			{
				// Look up type for this key. First check the property parm map,
				// then the entity class itself. If nothing is found, leave blank.
				PropertyParmMap::const_iterator typeIter = _map.find(key);

				const EntityClassAttribute* attr = _eclass.getAttribute(key);
				bool hasDescription = attr ? !attr->description.empty() : false;

				std::string type;
				if (typeIter != _map.end()) {
					type = typeIter->second.type;
				} else {
					if (attr == NULL)
						type = key;
					else
						type = attr->type;
				}

				// Append the details to the treestore
				GtkTreeIter iter;
				gtk_list_store_append(_store, &iter);
				gtk_list_store_set(_store, &iter, PROPERTY_NAME_COLUMN, key.c_str(), PROPERTY_VALUE_COLUMN,
						value.c_str(), TEXT_COLOUR_COLUMN, "black", PROPERTY_ICON_COLUMN,
						PropertyEditorFactory::getPixbufFor(type), HELP_ICON_COLUMN,
						hasDescription ? gtkutil::getLocalPixbuf(HELP_ICON_NAME) : NULL, HAS_HELP_FLAG_COLUMN,
						hasDescription ? TRUE : FALSE, -1);

				// If this was the last selected key, save the Iter so we can
				// select it again
				if (key == _lastKey) {
					_lastIter = gtk_tree_iter_copy(&iter);
				}
			}

			// Get the iter pointing to the last-selected key
			GtkTreeIter* getLastIter ()
			{
				return _lastIter;
			}
	};

	// Populate the list view
	ListPopulateVisitor visitor(_listStore, getPropertyMap(), _selectedEntity->getEntityClass(), _lastKey);
	_selectedEntity->forEachKeyValue(visitor);

	// If we found the last-selected key, select it
	GtkTreeIter* lastIter = visitor.getLastIter();
	if (lastIter != NULL) {
		gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(_treeView)), lastIter);
	}

	// Force an update of widgets
	treeSelectionChanged();
}

// Update the selected Entity pointer

bool EntityInspector::updateSelectedEntity ()
{
	_selectedEntity = NULL;

	// A single entity must be selected
	if (GlobalSelectionSystem().countSelected() != 1)
		return false;

	// The root node must not be selected (this can happen if Invert Selection is activated
	// with an empty scene, or by direct selection in the entity list).
	if (GlobalSelectionSystem().ultimateSelected().path().top().get().isRoot())
		return false;

	// Try both the selected node (if an entity is selected) or the parent node (if a brush is
	// selected. If neither of them convert to entities, return false.
	if ((_selectedEntity = Node_getEntity(GlobalSelectionSystem().ultimateSelected().path().top())) == 0
			&& (_selectedEntity = Node_getEntity(GlobalSelectionSystem().ultimateSelected().path().parent())) == 0) {
		return false;
	} else {
		return true;
	}
}

} // namespace ui
