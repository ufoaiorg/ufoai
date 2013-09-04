#include "EntityClassChooser.h"

#include "iradiant.h"
#include "eclasslib.h"
#include "gtkutil/dialog.h"
#include "gtkutil/RightAlignment.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/TreeModel.h"

#include <gdk/gdkkeysyms.h>

#include "../../entity/entity.h" // Entity_createFromSelection()
namespace ui
{
	namespace
	{
		/* CONSTANTS */
		const char* ECLASS_CHOOSER_TITLE = _("Create entity");
	}

	// Obtain and display the singleton instance
	void EntityClassChooser::displayInstance (const Vector3& point)
	{
		static EntityClassChooser instance;
		instance.show(point);
	}

	// Show the dialog
	void EntityClassChooser::show (const Vector3& point)
	{
		_lastPoint = point;
		gtk_widget_show_all(_widget);
	}

	// Constructor. Creates GTK widgets.
	EntityClassChooser::EntityClassChooser () :
		_widget(gtk_window_new(GTK_WINDOW_TOPLEVEL)), _treeStore(NULL), _selection(NULL), _addButton(NULL)
	{
		gtk_window_set_transient_for(GTK_WINDOW(_widget), GlobalRadiant().getMainWindow());
		gtk_window_set_modal(GTK_WINDOW(_widget), TRUE);
		gtk_window_set_position(GTK_WINDOW(_widget), GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_title(GTK_WINDOW(_widget), ECLASS_CHOOSER_TITLE);

		// Set the default size of the window

		GdkScreen* scr = gtk_window_get_screen(GTK_WINDOW(_widget));
		gint w = gdk_screen_get_width(scr);
		gint h = gdk_screen_get_height(scr);

		gtk_window_set_default_size(GTK_WINDOW(_widget), w / 3, h / 2);

		// Create GUI elements and pack into main VBox

		GtkWidget* vbx = gtk_vbox_new(FALSE, 3);
		gtk_box_pack_start(GTK_BOX(vbx), createTreeView(), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), createUsagePanel(), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), createButtonPanel(), FALSE, FALSE, 0);
		gtk_container_set_border_width(GTK_CONTAINER(_widget), 12);
		gtk_container_add(GTK_CONTAINER(_widget), vbx);

		// Signals
		g_signal_connect(_widget, "delete_event", G_CALLBACK(callbackHide), this);

	}

	// Create the tree view
	GtkWidget* EntityClassChooser::createTreeView ()
	{
		// Set up the TreeModel, and populate it with the list of entity
		// classes by using a local visitor class.

		_treeStore = gtk_tree_store_new(1, G_TYPE_STRING);

		struct TreePopulatingVisitor: public EntityClassVisitor
		{
				// TreeStore to populate
				GtkTreeStore* _store;

				// Constructor
				TreePopulatingVisitor (GtkTreeStore* store) :
					_store(store)
				{
				}

				// Required visit function
				virtual void visit (EntityClass* e)
				{
					GtkTreeIter iter;
					gtk_tree_store_append(_store, &iter, NULL);
					gtk_tree_store_set(_store, &iter, 0, e->name().c_str(), -1);
				}

		} visitor(_treeStore);

		GlobalEntityClassManager().forEach(visitor);

		// Construct the tree view widget with the now-populated model

		GtkWidget* treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_treeStore));
		_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
		gtk_tree_selection_set_mode(_selection, GTK_SELECTION_BROWSE);
		g_signal_connect(G_OBJECT(_selection), "changed", G_CALLBACK(callbackSelectionChanged), this);
		g_signal_connect(G_OBJECT(treeView), "button_press_event", G_CALLBACK(callbackMouseButtonPress), this);
		g_signal_connect(G_OBJECT(treeView), "key_press_event", G_CALLBACK(callbackKeyPress), this);

		GtkCellRenderer* rend = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* col = gtk_tree_view_column_new_with_attributes(_("Entity name"), rend, "text", 0, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(treeView), col);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), FALSE);

		// Pack treeview into a scrolled window, then into a frame

		return gtkutil::ScrolledFrame(treeView);
	}

	// Create the entity usage information panel
	GtkWidget* EntityClassChooser::createUsagePanel ()
	{
		// Create a GtkTextView
		_usageTextView = gtk_text_view_new();
		gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(_usageTextView), GTK_WRAP_WORD);
		gtk_text_view_set_editable(GTK_TEXT_VIEW(_usageTextView), FALSE);

		return gtkutil::ScrolledFrame(_usageTextView);
	}

	// Create the button panel
	GtkWidget* EntityClassChooser::createButtonPanel ()
	{
		GtkWidget* hbx = gtk_hbox_new(FALSE, 3);

		GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
		_addButton = gtk_button_new_from_stock(GTK_STOCK_ADD);

		g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(callbackCancel), this);
		g_signal_connect(G_OBJECT(_addButton), "clicked", G_CALLBACK(callbackAdd), this);

		gtk_box_pack_end(GTK_BOX(hbx), _addButton, TRUE, TRUE, 0);
		gtk_box_pack_end(GTK_BOX(hbx), cancelButton, TRUE, TRUE, 0);
		return gtkutil::RightAlignment(hbx);
	}

	// Update the usage information
	void EntityClassChooser::updateUsageInfo (const std::string& eclass)
	{
		std::string usageText;

		struct EntityClassAttributeFiller: public EntityClassAttributeVisitor
		{
				// TreeStore to populate
				std::string& _string;

				// Constructor
				EntityClassAttributeFiller (std::string& string) :
					_string(string)
				{
				}

				// Required visit function
				void visit (const EntityClassAttribute& attribute)
				{
					_string += attribute.name + ": " + attribute.description + "\n";
				}

		} visitor(usageText);

		// Lookup the IEntityClass instance
		EntityClass* e = GlobalEntityClassManager().findOrInsert(eclass, true);
		e->forEachClassAttribute(visitor);
		// Set the usage panel to the IEntityClass' usage information string
		GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(_usageTextView));
		gtk_text_buffer_set_text(buf, usageText.c_str(), -1);
	}

	/* GTK CALLBACKS */

	void EntityClassChooser::callbackHide (GtkWidget* widget, GdkEvent* ev, EntityClassChooser* self)
	{
		gtk_widget_hide(self->_widget);
	}

	void EntityClassChooser::callbackCancel (GtkWidget* widget, EntityClassChooser* self)
	{
		gtk_widget_hide(self->_widget);
	}

	void EntityClassChooser::addEntity (EntityClassChooser* self)
	{
		// Create the entity and hide the dialog. We might get an EntityCreationException
		// if the wrong number of brushes is selected.
		try {
			std::string name = gtkutil::TreeModel::getSelectedString(self->_selection, 0);
			Entity_createFromSelection(name, self->_lastPoint);
			gtk_widget_hide(self->_widget);
		} catch (EntityCreationException &e) {
			gtkutil::errorDialog(e.what());
		}
	}

	void EntityClassChooser::callbackAdd (GtkWidget* widget, EntityClassChooser* self)
	{
		addEntity(self);
	}

	void EntityClassChooser::callbackSelectionChanged (GtkWidget* widget, EntityClassChooser* self)
	{
		// Check for a selection
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(self->_selection, NULL, &iter)) {

			// There is a selection, so make the Add button active and update the
			// usage information
			gtk_widget_set_sensitive(self->_addButton, TRUE);

			// Get the selected classname
			GValue val;
			val.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(self->_treeStore), &iter, 0, &val);

			// Set the panel text
			self->updateUsageInfo(g_value_get_string(&val));
		} else {
			gtk_widget_set_sensitive(self->_addButton, FALSE);
		}
	}

	gint EntityClassChooser::callbackMouseButtonPress (GtkWidget *widget, GdkEventButton *event, EntityClassChooser* self)
	{
		// double clicked with the left mount button
		if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
			addEntity(self);
			return TRUE;
		}
		return FALSE;
	}

	gint EntityClassChooser::callbackKeyPress (GtkWidget* widget, GdkEventKey* event, EntityClassChooser* self)
	{
		if (event->keyval == GDK_Return) {
			addEntity(self);
			return TRUE;
		}

		return FALSE;
	}

} // namespace ui
