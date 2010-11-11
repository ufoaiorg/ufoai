#include "ParticleSelector.h"

#include "iradiant.h"
#include "iparticles.h"
#include "gtkutil/IconTextColumn.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/RightAlignment.h"
#include "gtkutil/TreeModel.h"
#include "gtkutil/image.h"
#include "gtkutil/MultiMonitor.h"
#include "generic/callback.h"
#include "../Icons.h"
#include <gtk/gtk.h>
#include "radiant_i18n.h"
#include "os/path.h"

namespace ui
{
	namespace
	{
		// Treestore enum
		enum
		{
			DISPLAYNAME_COLUMN, FILENAME_COLUMN, ICON_COLUMN, N_COLUMNS
		};
	}

	// Constructor
	ParticleSelector::ParticleSelector () :
		_widget(gtk_window_new(GTK_WINDOW_TOPLEVEL)), _treeStore(gtk_tree_store_new(N_COLUMNS,
				G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF))
	{
		// Set up the window
		gtk_window_set_transient_for(GTK_WINDOW(_widget), GlobalRadiant().getMainWindow());
		gtk_window_set_modal(GTK_WINDOW(_widget), TRUE);
		gtk_window_set_title(GTK_WINDOW(_widget), _("Choose particle"));
		gtk_window_set_position(GTK_WINDOW(_widget), GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_type_hint(GTK_WINDOW(_widget), GDK_WINDOW_TYPE_HINT_DIALOG);

		// Set the default size of the window
		GdkRectangle rect = gtkutil::MultiMonitor::getMonitorForWindow(GlobalRadiant().getMainWindow());
		gtk_window_set_default_size(GTK_WINDOW(_widget), rect.width / 2, rect.height / 2);

		// Delete event
		g_signal_connect(
				G_OBJECT(_widget), "delete-event", G_CALLBACK(_onDelete), this
		);

		// Main vbox
		GtkWidget* vbx = gtk_vbox_new(FALSE, 12);
		gtk_box_pack_start(GTK_BOX(vbx), createTreeView(), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), _preview, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbx), createButtons(), FALSE, FALSE, 0);

		gtk_container_set_border_width(GTK_CONTAINER(_widget), 12);
		gtk_container_add(GTK_CONTAINER(_widget), vbx);
	}

	namespace
	{
		/**
		 * Visitor class to enumerate sound shaders and add them to the tree store.
		 */
		class ParticlePopulator : public IParticleSystem::Visitor
		{
			private:

				GtkTreeStore* _treeStore;

			public:

				ParticlePopulator(GtkTreeStore* treeStore) : _treeStore(treeStore) {
				}

				// Required visit function
				void visit (IParticleDefinition* particle) const
				{
					// Get the display name by stripping off everything before the last
					// slash
					std::string displayName = particle->getName();
					std::string fileName = displayName; // TODO:

					// Pixbuf depends on model type
					GdkPixbuf* pixBuf = gtkutil::getLocalPixbuf(ui::icons::ICON_PARTICLE_MAP);

					// Append a node to the tree view for this child,
					GtkTreeIter iter;
					gtk_tree_store_append(_treeStore, &iter, NULL);

					// Fill in the column values
					gtk_tree_store_set(_treeStore, &iter, DISPLAYNAME_COLUMN, displayName.c_str(), FILENAME_COLUMN,
							fileName.c_str(), ICON_COLUMN, pixBuf, -1);
				}
		};
	} // namespace

	// Create the tree view
	GtkWidget* ParticleSelector::createTreeView ()
	{
		// Tree view with single text icon column
		GtkWidget* tv = gtk_tree_view_new_with_model(GTK_TREE_MODEL(_treeStore));
		gtk_tree_view_append_column(GTK_TREE_VIEW(tv), gtkutil::IconTextColumn(_("Particle"), DISPLAYNAME_COLUMN,
				ICON_COLUMN, false));

		_treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
		g_signal_connect(G_OBJECT(_treeSelection), "changed",
				G_CALLBACK(_onSelectionChange), this);

		// Visit all sound sound files and collect them for later insertion
		ParticlePopulator functor(_treeStore);
		GlobalParticleSystem().foreachParticle(functor);

		return gtkutil::ScrolledFrame(tv);
	}

	// Create buttons panel
	GtkWidget* ParticleSelector::createButtons ()
	{
		GtkWidget* hbx = gtk_hbox_new(TRUE, 6);

		GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
		GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

		g_signal_connect(G_OBJECT(okButton), "clicked",
				G_CALLBACK(_onOK), this);
		g_signal_connect(G_OBJECT(cancelButton), "clicked",
				G_CALLBACK(_onCancel), this);

		gtk_box_pack_end(GTK_BOX(hbx), okButton, TRUE, TRUE, 0);
		gtk_box_pack_end(GTK_BOX(hbx), cancelButton, TRUE, TRUE, 0);

		return gtkutil::RightAlignment(hbx);
	}

	// Show and block
	std::string ParticleSelector::chooseParticle ()
	{
		gtk_widget_show_all(_widget);
		gtk_main();
		return os::stripExtension(_selectedSound);
	}

	/* GTK CALLBACKS */

	// Delete dialog
	gboolean ParticleSelector::_onDelete (GtkWidget* w, GdkEvent* e, ParticleSelector* self)
	{
		self->_selectedSound = "";
		gtk_main_quit();
		return false; // propagate event
	}

	// OK button
	void ParticleSelector::_onOK (GtkWidget* w, ParticleSelector* self)
	{
		// Get the selection if valid, otherwise ""
		self->_selectedSound = gtkutil::TreeModel::getSelectedString(self->_treeSelection, FILENAME_COLUMN);

		gtk_widget_destroy(self->_widget);
		gtk_main_quit();
	}

	// Cancel button
	void ParticleSelector::_onCancel (GtkWidget* w, ParticleSelector* self)
	{
		self->_selectedSound = "";
		gtk_widget_destroy(self->_widget);
		gtk_main_quit();
	}

	// Tree Selection Change
	void ParticleSelector::_onSelectionChange (GtkTreeSelection* selection, ParticleSelector* self)
	{
		std::string selectedParticle = gtkutil::TreeModel::getSelectedString(self->_treeSelection, FILENAME_COLUMN);
		IParticleDefinition* pDef = GlobalParticleSystem().getParticle(selectedParticle);
		self->_preview.setParticle(pDef);
	}
}
