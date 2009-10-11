/**
 * @file prism.cpp
 * @brief Creates the prism arbitrary sides dialog
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
#include "gtkutil/dialog.h"
#include "scenelib.h"
#include "../mainframe.h"
#include "../brush/brushmanip.h"
#include "../sidebar/sidebar.h"

void DoSides (int type, int axis)
{
	ModalDialog dialog;
	GtkEntry* sides_entry;

	GtkWindow* window = create_dialog_window(MainFrame_getWindow(), _("Arbitrary sides"), G_CALLBACK(dialog_delete_callback), &dialog);

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group(window, accel);

	{
		GtkHBox* hbox = create_dialog_hbox(4, 4);
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));
		{
			GtkLabel* label = GTK_LABEL(gtk_label_new(_("Sides:")));
			gtk_widget_show(GTK_WIDGET(label));
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
		}
		{
			GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
			gtk_widget_show(GTK_WIDGET(entry));
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry), FALSE, FALSE, 0);
			sides_entry = entry;
			gtk_widget_grab_focus(GTK_WIDGET(entry));
		}
		{
			GtkVBox* vbox = create_dialog_vbox(4);
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
			{
				GtkButton* button = create_dialog_button(_("OK"), G_CALLBACK(dialog_button_ok), &dialog);
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				widget_make_default(GTK_WIDGET(button));
				gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Return, (GdkModifierType)0, (GtkAccelFlags)0);
			}
			{
				GtkButton* button = create_dialog_button(_("Cancel"), G_CALLBACK(dialog_button_cancel), &dialog);
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);
				gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel, GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);
			}
		}
	}

	if (modal_dialog_show(window, dialog) == eIDOK) {
		const char *str = gtk_entry_get_text(sides_entry);

		Scene_BrushConstructPrefab(GlobalSceneGraph(), (EBrushPrefab)type, atoi(str), TextureBrowser_GetSelectedShader(GlobalTextureBrowser()));
	}

	gtk_widget_destroy(GTK_WIDGET(window));
}
