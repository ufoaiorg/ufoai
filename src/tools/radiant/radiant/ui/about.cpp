/**
 * @file about.cpp
 * @brief Creates the about dialog
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

#include <gtk/gtk.h>
#include "version.h"
#include "aboutmsg.h"
#include "gtkutil/dialog.h"
#include "gtkutil/window.h"
#include "gtkutil/image.h"
#include "igl.h"
#include "../mainframe.h"

void DoAbout (void)
{
	ModalDialog dialog;
	ModalDialogButton ok_button(dialog, eIDOK);

	GtkWindow* window = create_modal_dialog_window(MainFrame_getWindow(), "About UFORadiant", dialog);

	{
		GtkVBox* vbox = create_dialog_vbox(4, 4);
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(vbox));

		{
			GtkHBox* hbox = create_dialog_hbox(4);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

			{
				GtkVBox* vbox2 = create_dialog_vbox(4);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox2), TRUE, FALSE, 0);
				{
					GtkFrame* frame = create_dialog_frame(0, GTK_SHADOW_IN);
					gtk_box_pack_start(GTK_BOX (vbox2), GTK_WIDGET(frame), FALSE, FALSE, 0);
					{
						GtkImage* image = new_local_image("logo.bmp");
						gtk_widget_show(GTK_WIDGET(image));
						gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(image));
					}
				}
			}

			{
				GtkLabel* label = GTK_LABEL(gtk_label_new("UFORadiant " RADIANT_VERSION "\n"
				                            __DATE__ "\n\n"
				                            RADIANT_ABOUTMSG "\n\n"
				                            "This program is free software\n"
				                            "licensed under the GNU GPL.\n"));

				gtk_widget_show(GTK_WIDGET(label));
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
				gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
				gtk_label_set_justify(label, GTK_JUSTIFY_LEFT);
			}

			{
				GtkVBox* vbox2 = create_dialog_vbox(4);
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(vbox2), FALSE, TRUE, 0);
				{
					GtkButton* button = create_modal_dialog_button("OK", ok_button);
					gtk_box_pack_start (GTK_BOX (vbox2), GTK_WIDGET(button), FALSE, FALSE, 0);
				}
			}
		}
		{
			GtkFrame* frame = create_dialog_frame("OpenGL Properties");
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), FALSE, FALSE, 0);
			{
				GtkTable* table = create_dialog_table(3, 2, 4, 4, 4);
				gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(table));
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new("Vendor:"));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 0, 1,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new("Version:"));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 1, 2,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new("Renderer:"));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 2, 3,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 0, 1,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 1, 2,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 2, 3,
					                 (GtkAttachOptions) (GTK_FILL),
					                 (GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
			}
			{
				GtkFrame* frame = create_dialog_frame("OpenGL Extensions");
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), TRUE, TRUE, 0);
				{
					GtkScrolledWindow* sc_extensions = create_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, 4);
					gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET(sc_extensions));
					{
						GtkWidget* text_extensions = gtk_text_view_new();
						gtk_text_view_set_editable(GTK_TEXT_VIEW(text_extensions), FALSE);
						gtk_container_add (GTK_CONTAINER (sc_extensions), text_extensions);
						GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_extensions));
						gtk_text_buffer_set_text(buffer, reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), -1);
						gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_extensions), GTK_WRAP_WORD);
						gtk_widget_show(text_extensions);
					}
				}
			}
		}
	}

	modal_dialog_show(window, dialog);

	gtk_widget_destroy(GTK_WIDGET(window));
}

