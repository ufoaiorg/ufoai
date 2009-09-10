/**
 * @file particlebrowser.cpp
 */

/*
 Copyright (C) 2002-2009 UFO: Alien Invasion.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 */

#include "ParticleBrowser.h"
#include "radiant_i18n.h"

#include "ishaders.h"
#include "generic/callback.h"
#include "gtkutil/image.h"

#include <gtk/gtkvbox.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtktreeselection.h>

#include <iostream>

namespace ui
{
	/* CONSTANTS */
	namespace
	{
		// TreeStore columns
		enum
		{
			PARTICLENAME_COLUMN, PARTICLE_DEF_PTR, N_COLUMNS
		};
	}

	// Constructor
	ParticleBrowser::ParticleBrowser () :
		_widget(gtk_vbox_new(FALSE, 0)), _treeStore(gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER)),
				_treeView(gtk_tree_view_new_with_model(GTK_TREE_MODEL(_treeStore))), _selection(
						gtk_tree_view_get_selection(GTK_TREE_VIEW(_treeView)))
	{
		// Create the treeview
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(_treeView), FALSE);
		g_signal_connect(G_OBJECT(_treeView), "expose-event", G_CALLBACK(_onExpose), this);
		g_signal_connect(G_OBJECT(_treeView), "button-release-event", G_CALLBACK(_onRightClick), this);

		// Single text column with packed icon
		GtkTreeViewColumn* col = gtk_tree_view_column_new();
		gtk_tree_view_column_set_spacing(col, 3);

		GtkCellRenderer* textRenderer = gtk_cell_renderer_text_new();
		gtk_tree_view_column_pack_start(col, textRenderer, FALSE);
		gtk_tree_view_column_set_attributes(col, textRenderer, "text", PARTICLENAME_COLUMN, NULL);

		gtk_tree_view_append_column(GTK_TREE_VIEW(_treeView), col);

		// Pack the treeview into a scrollwindow, frame and then into the vbox
		GtkWidget* scroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(scroll), _treeView);

		GtkWidget* frame = gtk_frame_new(NULL);
		gtk_container_add(GTK_CONTAINER(frame), scroll);
		gtk_box_pack_start(GTK_BOX(_widget), frame, TRUE, TRUE, 0);

		// Connect up the selection changed callback
		g_signal_connect(G_OBJECT(_selection), "changed", G_CALLBACK(_onSelectionChanged), this);

		// Pack in the TexturePreviewCombo widgets
		gtk_box_pack_end(GTK_BOX(_widget), _imagePreview, FALSE, FALSE, 0);

		// Pack in the ModelPreview widgets
		_modelPreview.setSize(100);
		gtk_box_pack_end(GTK_BOX(_widget), _modelPreview, TRUE, TRUE, 0);
	}

	/* Tree query functions */

	ParticleDefinition* ParticleBrowser::getSelectedParticle ()
	{
		// Get the selected value
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(_selection, NULL, &iter)) {
			GValue nameVal;
			nameVal.g_type = 0;
			gtk_tree_model_get_value(GTK_TREE_MODEL(_treeStore), &iter, PARTICLE_DEF_PTR, &nameVal);
			// Return particle pointer
			return (ParticleDefinition*) g_value_get_pointer(&nameVal);
		} else {
			// Error condition if there is no selection
			return (ParticleDefinition*) 0;
		}
	}

	/* GTK CALLBACKS */

	gboolean ParticleBrowser::_onExpose (GtkWidget* widget, GdkEventExpose* ev, ParticleBrowser* self)
	{
		// Populate the tree view if it is not already populated
		static bool _isPopulated = false;
		if (!_isPopulated) {
			GtkTreeIter iter;
			for (ParticleDefinitionMap::const_iterator i = g_particleDefinitions.begin(); i
					!= g_particleDefinitions.end(); ++i) {
				gtk_tree_store_append(self->_treeStore, &iter, (GtkTreeIter*) 0);
				gtk_tree_store_set(self->_treeStore, &iter, PARTICLENAME_COLUMN, (*i).first.c_str(), PARTICLE_DEF_PTR,
						&((*i).second), -1);
			}

			_isPopulated = true;
		}
		return FALSE; // progapagate event
	}

	bool ParticleBrowser::_onRightClick (GtkWidget* widget, GdkEventButton* ev, ParticleBrowser* self)
	{
		// Popup on right-click events only
		if (ev->button == 3) {
		}
		return FALSE;
	}

	void ParticleBrowser::_onSelectionChanged (GtkWidget* widget, ParticleBrowser* self)
	{
		self->_modelPreview.setModel("");
		self->_imagePreview.setTexture("");

		const std::string& image = self->getSelectedParticle()->getImage();
		const std::string& model = self->getSelectedParticle()->getModel();
		if (!image.empty()) {
			// Update the preview if a texture is selected
			self->_imagePreview.setTexture("pics/" + image);
		} else if (!model.empty()) {
			// Update the preview if a model is selected
			self->_modelPreview.initialisePreview();
			self->_modelPreview.setModel("models/" + model);
		}
	}
}
