/**
 * @file findbrush.cpp
 * @brief Creates the findbrush widget and select the brush (if found)
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
#include "iscenegraph.h"
#include "iradiant.h"
#include "scenelib.h"
#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"
#include <gdk/gdkkeysyms.h>
#include "../../map/map.h"

void FindBrushOrEntity (void)
{
	ModalDialog dialog;
	GtkEntry* entity;
	GtkEntry* brush;

	GtkWindow* window = create_dialog_window(GlobalRadiant().getMainWindow(), _("Find Brush"), G_CALLBACK(dialog_delete_callback), &dialog);

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group(window, accel);

	{
		GtkVBox* vbox = create_dialog_vbox(4, 4);
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox));
		{
			GtkTable* table = create_dialog_table(2, 2, 4, 4);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), TRUE, TRUE, 0);
			{
				GtkWidget* label = gtk_label_new(_("Entity number"));
				gtk_widget_show (label);
				gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
								(GtkAttachOptions) (0),
								(GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkWidget* label = gtk_label_new(_("Brush number"));
				gtk_widget_show (label);
				gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
								(GtkAttachOptions) (0),
								(GtkAttachOptions) (0), 0, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(table, GTK_WIDGET(entry), 1, 2, 0, 1,
								(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
								(GtkAttachOptions) (0), 0, 0);
				gtk_widget_grab_focus(GTK_WIDGET(entry));
				entity = entry;
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_table_attach(table, GTK_WIDGET(entry), 1, 2, 1, 2,
								(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
								(GtkAttachOptions) (0), 0, 0);

				brush = entry;
			}
		}
		{
			GtkHBox* hbox = create_dialog_hbox(4);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), TRUE, TRUE, 0);
			{
				GtkButton* button = create_dialog_button(_("Find"), G_CALLBACK(dialog_button_ok), &dialog);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				widget_make_default(GTK_WIDGET(button));
				gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0);
			}
			{
				GtkButton* button = create_dialog_button(_("Close"), G_CALLBACK(dialog_button_cancel), &dialog);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);
			}
		}
	}

	if (modal_dialog_show(window, dialog) == eIDOK) {
		const char *entstr = gtk_entry_get_text(entity);
		const char *brushstr = gtk_entry_get_text(brush);
		SelectBrush(atoi(entstr), atoi(brushstr), true);
	}

	gtk_widget_destroy(GTK_WIDGET(window));
}
