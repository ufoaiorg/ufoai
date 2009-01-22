/**
 * @file light.cpp
 * @brief Creates the light dialog to set the intensity
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
#include "../mainframe.h"

EMessageBoxReturn DoLightIntensityDlg (int *intensity) {
	ModalDialog dialog;
	GtkEntry* intensity_entry;
	ModalDialogButton ok_button(dialog, eIDOK);
	ModalDialogButton cancel_button(dialog, eIDCANCEL);

	GtkWindow* window = create_modal_dialog_window(MainFrame_getWindow(), "Light intensity", dialog, -1, -1);

	GtkAccelGroup *accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(window, accel_group);

	{
		GtkHBox* hbox = create_dialog_hbox(4, 4);
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(hbox));
		{
			GtkVBox* vbox = create_dialog_vbox(4);
			gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox), TRUE, TRUE, 0);
			{
				GtkLabel* label = GTK_LABEL(gtk_label_new(_("ESC for default, ENTER to validate")));
				gtk_widget_show(GTK_WIDGET(label));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(label), FALSE, FALSE, 0);
			}
			{
				GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
				gtk_widget_show(GTK_WIDGET(entry));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(entry), TRUE, TRUE, 0);

				gtk_widget_grab_focus(GTK_WIDGET(entry));

				intensity_entry = entry;
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

	char buf[16];
	sprintf (buf, "%d", *intensity);
	gtk_entry_set_text(intensity_entry, buf);

	EMessageBoxReturn ret = modal_dialog_show(window, dialog);
	if (ret == eIDOK)
		*intensity = atoi (gtk_entry_get_text(intensity_entry));

	gtk_widget_destroy(GTK_WIDGET(window));

	return ret;
}
