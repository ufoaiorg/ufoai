#include "ErrorCheckDialog.h"
#include "CompilerObserver.h"

#include "radiant_i18n.h"
#include "imapcompiler.h"
#include "ieventmanager.h"
#include "gtkutil/dialog.h"
#include "gtkutil/TextColumn.h"
#include "gtkutil/TreeModel.h"

#include "imaterial.h"
#include "../../map/map.h"
#include "../../map/MapCompileException.h"
#include "../../mainframe.h"

namespace ui {

namespace {
const int CHECKDLG_DEFAULT_SIZE_X = 600;
const int CHECKDLG_DEFAULT_SIZE_Y = 400;
}

class CheckDialogCompilerIgnoreObserver: public ICompilerObserver
{
		void notify (const std::string &message)
		{
		}
};

ErrorCheckDialog::ErrorCheckDialog () :
	gtkutil::BlockingTransientWindow(_("Error checking"), GlobalRadiant().getMainWindow())
{
	gtk_window_set_default_size(GTK_WINDOW(getWindow()), CHECKDLG_DEFAULT_SIZE_X, CHECKDLG_DEFAULT_SIZE_Y);
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
	gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);

	// Create all the widgets
	populateWindow();

	// Propagate shortcuts to the main window
	GlobalEventManager().connectDialogWindow(GTK_WINDOW(getWindow()));

	try {
		const std::string mapName = GlobalRadiant().getMapName();
		CompilerObserver observer(_listStore);
		GlobalMapCompiler().performCheck(mapName, observer);
		// Show the window and its children
		show();
	} catch (MapCompileException& e) {
		hide();
		gtkutil::errorDialog(e.what());
	}
}

ErrorCheckDialog::~ErrorCheckDialog ()
{
	// Propagate shortcuts to the main window
	GlobalEventManager().disconnectDialogWindow(GTK_WINDOW(getWindow()));
}

void ErrorCheckDialog::showDialog() {
	// Just instantiate a new dialog, this enters a main loop
	ErrorCheckDialog dialog;
}

// Create the buttons panel at bottom of dialog
GtkWidget* ErrorCheckDialog::createButtons ()
{
	GtkWidget* hbx = gtk_hbox_new(FALSE, 3);

	GtkWidget* closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	GtkWidget* fixButton = gtk_button_new_with_label(_("Fix"));

	g_signal_connect(G_OBJECT(fixButton), "clicked", G_CALLBACK(callbackFix), this);
	g_signal_connect(G_OBJECT(closeButton), "clicked", G_CALLBACK(callbackClose), this);

	gtk_box_pack_end(GTK_BOX(hbx), fixButton, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbx), closeButton, FALSE, FALSE, 0);
	return hbx;
}

// Helper function to create the TreeView
GtkWidget* ErrorCheckDialog::createTreeView ()
{
	_listView = gtk_tree_view_new();

	gtk_tree_view_append_column(GTK_TREE_VIEW(_listView), gtkutil::TextColumn(_("Entity"), CHECK_ENTITY));
	gtk_tree_view_append_column(GTK_TREE_VIEW(_listView), gtkutil::TextColumn(_("Brush"), CHECK_BRUSH));
	gtk_tree_view_append_column(GTK_TREE_VIEW(_listView), gtkutil::TextColumn(_("Message"), CHECK_MESSAGE));

	GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(_listView));
	g_signal_connect(G_OBJECT(sel), "changed", G_CALLBACK(callbackSelect), this);

	_listStore = gtk_list_store_new(CHECK_COLUMNS, G_TYPE_INT, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(_listView), GTK_TREE_MODEL(_listStore));
	/* unreference the list so that is will be deleted along with the tree view */
	g_object_unref(_listStore);

	// Pack treeview into a scrolled window and frame, and return

	GtkWidget* scrollWin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollWin), _listView);

	GtkWidget* fr = gtk_frame_new(NULL);
	gtk_container_add(GTK_CONTAINER(fr), scrollWin);

	return fr;
}

void ErrorCheckDialog::populateWindow ()
{
	GtkWidget* dialogVBox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(getWindow()), dialogVBox);

	gtk_box_pack_start(GTK_BOX(dialogVBox), createTreeView(), TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(dialogVBox), createButtons(), FALSE, FALSE, 0);
}

void ErrorCheckDialog::callbackClose (GtkWidget* widget, ErrorCheckDialog* self)
{
	self->hide();
}

void ErrorCheckDialog::callbackFix (GtkWidget* widget, ErrorCheckDialog* self)
{
	// close this dialog
	self->hide();

	try {
		const std::string mapName = GlobalRadiant().getMapName();
		CheckDialogCompilerIgnoreObserver observer;
		GlobalMapCompiler().fixErrors(mapName, observer);
	} catch (MapCompileException& e) {
		gtkutil::errorDialog(e.what());
	}
}

void ErrorCheckDialog::callbackSelect (GtkTreeSelection* sel, ErrorCheckDialog* self)
{
	// Get the selection
	GtkTreeIter selected;
	bool hasSelection = gtk_tree_selection_get_selected(sel, NULL, &selected) ? true : false;
	if (hasSelection) {
		// get the values from the tree view model list
		const int entnum = gtkutil::TreeModel::getInt(GTK_TREE_MODEL(self->_listStore), &selected, CHECK_ENTITY);
		const int brushnum = gtkutil::TreeModel::getInt(GTK_TREE_MODEL(self->_listStore), &selected, CHECK_BRUSH);

		// correct brush and ent values
		if (brushnum < 0 || entnum < 0)
			return;

		// and now do the real selection
		SelectBrush(entnum, brushnum, true);
	}
}

}
