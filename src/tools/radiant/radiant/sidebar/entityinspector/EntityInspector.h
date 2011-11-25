#ifndef ENTITYINSPECTOR_H_
#define ENTITYINSPECTOR_H_

#include "PropertyEditor.h"

#include "iselection.h"
#include "iundo.h"

#include "gtkutil/event/SingleIdleCallback.h"
#include "gtkutil/menu/PopupMenu.h"

#include <gtk/gtkliststore.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtktreeviewcolumn.h>

#include <map>

/* FORWARD DECLS */

class Entity;
class Selectable;
class SidebarComponent;

namespace ui {

namespace {

// Data structure to store the type (vector3, text etc) and the options
// string for a single property.
struct PropertyParms
{
		std::string type;
		std::string options;
};

// Map of property names to PropertyParms
typedef std::map<std::string, PropertyParms> PropertyParmMap;

}

/* The EntityInspector class represents the GTK dialog for editing properties
 * on the selected game entity. The class is implemented as a singleton and
 * contains a method to return the current instance.
 */

class EntityInspector: public SelectionSystem::Observer,
		public gtkutil::SingleIdleCallback,
		public UndoSystem::Observer,
		public SidebarComponent
{
	private:

		// Currently selected entity, this pointer is only non-NULL if the
		// current entity selection includes exactly 1 entity.
		Entity* _selectedEntity;

		// Main EntityInspector widget
		GtkWidget* _widget;

		// Frame to contain the Property Editor
		GtkWidget* _editorFrame;

		// The checkbox for showing the eclass properties
		GtkWidget* _showHelpColumnCheckbox;

		// Key list store and view
		GtkListStore* _listStore;
		GtkWidget* _treeView;

		GtkTreeViewColumn* _helpColumn;

		// Key and value edit boxes. These remain available even for multiple entity selections.
		GtkWidget* _keyEntry;
		GtkWidget* _valEntry;

		// Context menu
		gtkutil::PopupMenu _contextMenu;
		static void _onAddKey (gpointer, gpointer);
		static void _onDeleteKey (gpointer, gpointer);
		static void _onCopyKey (gpointer, gpointer);
		static void _onCutKey (gpointer, gpointer);
		static void _onPasteKey (gpointer, gpointer);

		static bool _testDeleteKey (gpointer userData);
		static bool _testCopyKey (gpointer userData);
		static bool _testCutKey (gpointer userData);
		static bool _testPasteKey (gpointer userData);

		// Currently displayed PropertyEditor
		PropertyEditorPtr _currentPropertyEditor;

		// The last selected key
		std::string _lastKey;

		// The clipboard for spawnargs
		struct ClipBoard
		{
				std::string key;
				std::string value;

				bool empty () const
				{
					return key.empty();
				}
		} _clipBoard;

	private:

		// Utility functions to construct the Gtk components

		GtkWidget* createDialogPane (); // bottom widget pane
		GtkWidget* createTreeViewPane (); // tree view for selecting attributes
		void createContextMenu ();

		// Utility function to retrieve the string selection from the given column in the
		// list store
		std::string getListSelection (int col);

		/* GTK CALLBACKS */

		static void callbackTreeSelectionChanged (GtkWidget* widget, EntityInspector* self);

		static void _onEntryActivate (GtkWidget*, EntityInspector*);
		static void _onSetProperty (GtkWidget*, EntityInspector*);
		static void _onToggleShowHelpIcons (GtkToggleButton*, EntityInspector*);

		static gboolean _onQueryTooltip (GtkWidget* widget, gint x, gint y, gboolean keyboard_mode,
				GtkTooltip* tooltip, EntityInspector* self);

		// Routines to populate the TreeStore with the keyvals attached to the
		// currently-selected object.
		void refreshTreeModel ();

		// Update the GTK components when a new selection is made in the tree view
		void treeSelectionChanged ();

		// Update the currently selected entity pointer. This function returns true
		// if a single Entity is selected, and false if either a non-Entity or more
		// than one object is selected.
		bool updateSelectedEntity ();

		// Set the keyval on all selected entities from the key and value textboxes
		void setPropertyFromEntries ();

		// Applies the given key/value pair to the selection (works with multiple selected entities)
		void applyKeyValueToSelection (const std::string& key, const std::string& value);

		// Static map of property names to PropertyParms objects
		const PropertyParmMap& getPropertyMap ();

	protected:

		// GTK idle callback, used for refreshing display
		void onGtkIdle ();

	public:

		// Constructor
		EntityInspector ();

		// Return or create the singleton instance
		static EntityInspector& Instance ();

		// Get the Gtk Widget for display in the main application
		GtkWidget* getWidget () const;

		const std::string getTitle() const;

		// Callback used by the EntityCreator when a key value changes on an entity
		static void keyValueChanged ();

		/** greebo: Gets called by the RadiantSelectionSystem upon selection change.
		 */
		void selectionChanged (scene::Instance& instance, bool isComponent);

		// Gets called after an undo operation
		void postUndo ();
		// Gets called after a redo operation
		void postRedo ();
};

} // namespace ui

#endif /*ENTITYINSPECTOR_H_*/
