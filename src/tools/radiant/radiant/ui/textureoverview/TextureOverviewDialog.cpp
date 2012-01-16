#include "TextureOverviewDialog.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "ieventmanager.h"

#include "gtkutil/ScrolledFrame.h"

#include <gtk/gtkhbox.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>

namespace ui
{
	// TODO: ShaderSelector

	TextureOverviewDialog::TextureOverviewDialog () :
		gtkutil::BlockingTransientWindow(_("Texture overview"), GlobalRadiant().getMainWindow()), _widget(gtk_hbox_new(FALSE, 0))
	{
	}

	TextureOverviewDialog::~TextureOverviewDialog ()
	{
		// Propagate shortcuts to the main window
		GlobalEventManager().disconnectDialogWindow(GTK_WINDOW(getWindow()));
	}

	void TextureOverviewDialog::populateWindow ()
	{
		// Set up the info table
		GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_infoStore));
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

		GtkCellRenderer* rend;
		GtkTreeViewColumn* col;

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Name"), rend, "text", 0, NULL);
		g_object_set(G_OBJECT(rend), "weight", 700, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		rend = gtk_cell_renderer_text_new();
		col = gtk_tree_view_column_new_with_attributes(_("Count"), rend, "text", 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

		GtkWidget *scroll = gtkutil::ScrolledFrame(view);

		gtk_box_pack_start(GTK_BOX(_widget), scroll, TRUE, TRUE, 0);

	}

	GtkWidget* TextureOverviewDialog::createButtons ()
	{
		GtkWidget* hbox = gtk_hbox_new(FALSE, 6);

		GtkWidget* findButton = gtk_button_new_from_stock(GTK_STOCK_FIND);
		GtkWidget* closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);

		g_signal_connect(G_OBJECT(closeButton), "clicked", G_CALLBACK(onClose), this);

		gtk_box_pack_end(GTK_BOX(hbox), closeButton, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(hbox), findButton, FALSE, FALSE, 0);

		return hbox;
	}

	void TextureOverviewDialog::onClose (GtkWidget* widget, TextureOverviewDialog* self)
	{
		// Call the DialogWindow::destroy method and remove self from heap
		self->destroy();
	}
}
