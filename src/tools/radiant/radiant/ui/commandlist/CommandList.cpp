#include "CommandList.h"

#include "gtk/gtk.h"

#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/TextButton.h"
#include "gtkutil/TextColumn.h"

#include "../../mainframe.h"
#include "CommandListPopulator.h"
#include "radiant_i18n.h"

namespace ui {

namespace {
const int CMDLISTDLG_DEFAULT_SIZE_X = 350;
const int CMDLISTDLG_DEFAULT_SIZE_Y = 400;

const std::string CMDLISTDLG_WINDOW_TITLE = _("Shortcut List");
}

CommandListDialog::CommandListDialog () :
		gtkutil::BlockingTransientWindow(CMDLISTDLG_WINDOW_TITLE, MainFrame_getWindow())
{
	// Set the default border width in accordance to the HIG
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);

	gtk_window_set_default_size(GTK_WINDOW(getWindow()),
			CMDLISTDLG_DEFAULT_SIZE_X, CMDLISTDLG_DEFAULT_SIZE_Y);

	// Create all the widgets
	populateWindow();

	// Show the window and its children
	gtk_widget_show_all(getWindow());
}

void CommandListDialog::populateWindow ()
{
	GtkHBox* hbox = GTK_HBOX(gtk_hbox_new(FALSE, 12));
	gtk_widget_show(GTK_WIDGET(hbox));
	gtk_container_add(GTK_CONTAINER(getWindow()), GTK_WIDGET(hbox));

	{
		// Create a new liststore item and define its columns
		_listStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

		GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_listStore));

		GtkTreeViewColumn* cmdCol = gtkutil::TextColumn(_("Command"), 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), cmdCol);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), gtkutil::TextColumn(_("Key"), 1));

		gtk_widget_show(view);

		// Instantiate the visitor class with the target list store
		CommandListPopulator populator(_listStore);

		// Cycle through all the events and create the according list items
		GlobalEventManager().foreachEvent(&populator);

		g_object_unref(G_OBJECT(_listStore));

		// Pack this treeview into a scrolled window and show it
		GtkWidget* scrolled = gtkutil::ScrolledFrame(view);
		gtk_widget_show_all(scrolled);

		// Set the sorting column
		gtk_tree_view_column_set_sort_column_id(cmdCol, 0);

		// Pack the scrolled window into the hbox
		gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);
	}

	GtkVBox* vbox = GTK_VBOX(gtk_vbox_new(FALSE, 6));
	gtk_widget_show(GTK_WIDGET(vbox));
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), FALSE, FALSE, 0);

	// Create the close button
	GtkWidget* closeButton = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(vbox), closeButton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(closeButton), "clicked", G_CALLBACK(callbackClose), this);

	gtk_widget_set_size_request(closeButton, 80, -1);
}

void CommandListDialog::callbackClose (GtkWidget* widget, CommandListDialog* self)
{
	// Call the DialogWindow::destroy method and remove self from heap
	self->destroy();
}

} // namespace ui

void ShowCommandListDialog ()
{
	new ui::CommandListDialog(); // self-destructs in GTK callback
}
