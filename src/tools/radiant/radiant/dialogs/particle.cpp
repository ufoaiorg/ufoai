/**
 * @file particle.cpp
 * @brief Creates the particle dialog to select a particle id
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

#include "radiant_i18n.h"
#include <gdk/gdkkeysyms.h>
#include "iradiant.h"
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include "gtkutil/window.h"
#include "particles.h"

/**
 * Shows a list with all available particle IDs that were parsed from script files
 * @param[out] particle The selected particle script ID
 * @return EMessageBoxReturn
 */
EMessageBoxReturn DoParticleDlg (char **particle)
{
	ModalDialog dialog;
	ModalDialogButton ok_button(dialog, eIDOK);
	ModalDialogButton cancel_button(dialog, eIDCANCEL);
	GtkListStore *store;
	GtkWidget *treeViewWidget;

	GtkWindow* window = create_modal_dialog_window(GlobalRadiant().getMainWindow(), _("Particle"), dialog, -1, -1);

	gtk_widget_set_size_request(GTK_WIDGET(window), 250, 175);

	GtkAccelGroup *accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(window, accel_group);

	{
		GtkHBox* hbox = create_dialog_hbox(4, 4);
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));
		{
			GtkVBox* vbox = create_dialog_vbox(4);
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
			{
				GtkTreeIter iter;
				GtkCellRenderer *renderer;
				GtkTreeViewColumn * column;
				GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

				treeViewWidget = gtk_tree_view_new();

				renderer = gtk_cell_renderer_text_new();
				column = gtk_tree_view_column_new_with_attributes(_("Particle"), renderer, "text", 0, (const char*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(treeViewWidget), column);

				gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(treeViewWidget));
				gtk_container_add(GTK_CONTAINER(vbox), GTK_WIDGET(scr));

				store = gtk_list_store_new(1, G_TYPE_STRING);

				for (ParticleDefinitionMap::const_iterator i = g_particleDefinitions.begin(); i != g_particleDefinitions.end(); ++i) {
					gtk_list_store_append(store, &iter);
					gtk_list_store_set(store, &iter, 0, (*i).first.c_str(), -1);
				}
				gtk_tree_view_set_model(GTK_TREE_VIEW(treeViewWidget), GTK_TREE_MODEL(store));
				/* unreference the list so that is will be deleted along with the tree view */
				g_object_unref(store);
				gtk_widget_show_all(GTK_WIDGET(scr));
			}
		}
		{
			GtkVBox* vbox = create_dialog_vbox(4);
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), FALSE, FALSE, 0);

			{
				GtkButton* button = create_modal_dialog_button(_("OK"), ok_button);
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				widget_make_default(GTK_WIDGET(button));
				gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, GDK_Return, (GdkModifierType)0, GTK_ACCEL_VISIBLE);
			}
			{
				GtkButton* button = create_modal_dialog_button(_("Cancel"), cancel_button);
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, GDK_Escape, (GdkModifierType)0, GTK_ACCEL_VISIBLE);
			}
		}
	}

	EMessageBoxReturn ret = modal_dialog_show(window, dialog);
	if (ret == eIDOK) {
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeViewWidget));
		GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeViewWidget));
		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			char *ptl = NULL;
			gtk_tree_model_get(model, &iter, 0, &ptl, -1);
			if (ptl)
				*particle = strdup(ptl);
		}
	}

	gtk_widget_destroy(GTK_WIDGET(window));

	return ret;
}
