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

#include "radiant_i18n.h"
#include "version.h"
#include "aboutmsg.h"
#include "gtkutil/dialog.h"
#include "gtkutil/window.h"
#include "gtkutil/image.h"
#include "igl.h"
#include "sound/OpenAL.h"
#include "iradiant.h"
#include <gtk/gtkgl.h>
#include "../ui/Icons.h"

#ifdef DEBUG
# define ABOUT_DEBUG "Debug build"
#else
# define ABOUT_DEBUG "Release build"
#endif

void DoAbout (void)
{
	ModalDialog dialog;
	ModalDialogButton ok_button(dialog, eIDOK);

	GtkWindow* window = create_modal_dialog_window(GlobalRadiant().getMainWindow(), _("About UFORadiant"), dialog);

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
					GtkFrame* frame = create_dialog_frame("", GTK_SHADOW_IN);
					gtk_box_pack_start(GTK_BOX (vbox2), GTK_WIDGET(frame), FALSE, FALSE, 0);
					{
						GtkWidget* image = gtkutil::getImage(ui::icons::ICON_LOGO);
						gtk_widget_show(image);
						gtk_container_add(GTK_CONTAINER(frame), image);
					}
				}
			}

			{
				GtkLabel* label = GTK_LABEL(gtk_label_new(NULL));
				gtk_label_set_markup(label, "<b>UFORadiant " RADIANT_VERSION "</b>\n"
					__DATE__ " " ABOUT_DEBUG "\n\n"
					RADIANT_ABOUTMSG "\n\n"
					"This program is free software\n"
					"licensed under the GNU GPL.\n\n"
					"<b>UFO:AI</b> http://ufoai.sourceforge.net\n");

				gtk_widget_show(GTK_WIDGET(label));
				gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
				gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
				gtk_label_set_justify(label, GTK_JUSTIFY_LEFT);
			}
		}
		{
			GtkFrame* frame = create_dialog_frame(_("<b>GTK+ Properties</b>"));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), FALSE, FALSE, 0);
			gtk_widget_show(GTK_WIDGET(frame));
			{
				GtkTable* table = create_dialog_table(4, 2, 4, 4, 4);
				gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(table));
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("GTK version:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 0, 1,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("GTKGL version:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 1, 2,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("GLib version:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 2, 3,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("Image formats:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 3, 4,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					char versionString[16];
					g_snprintf(versionString, sizeof(versionString), "%i.%i.%i", gtk_major_version, gtk_minor_version, gtk_micro_version);
					GtkLabel* label = GTK_LABEL(gtk_label_new(versionString));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 0, 1,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					char versionString[16];
					g_snprintf(versionString, sizeof(versionString), "%i.%i.%i", gtkglext_major_version, gtkglext_minor_version, gtkglext_micro_version);
					GtkLabel* label = GTK_LABEL(gtk_label_new(versionString));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 1, 2,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					char versionString[16];
					g_snprintf(versionString, sizeof(versionString), "%i.%i.%i", glib_major_version, glib_minor_version, glib_micro_version);
					GtkLabel* label = GTK_LABEL(gtk_label_new(versionString));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 2, 3,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
				}
				{
					/** @todo somehow query this info from the registered image types */
					GSList *formats = gdk_pixbuf_get_formats();
					GSList *format;
					gchar *list = (gchar *)0;
					for (format = formats; format; format = g_slist_next(format)) {
						GdkPixbufFormat *pixbuf_format = (GdkPixbufFormat*)format->data;
						gchar *name = gdk_pixbuf_format_get_name(pixbuf_format);
						if (!list) {
#if GLIB_CHECK_VERSION(2,16,0)
							if (!g_strcmp0(name, "jpeg") || !g_strcmp0(name, "tga") || !g_strcmp0(name, "png")) {
#endif
								list = g_strconcat("<b>", name, "</b> ", (void *)0);
#if GLIB_CHECK_VERSION(2,16,0)
							} else {
								list = g_strconcat(name, " ", (void *)0);
							}
#endif
						} else {
							gchar *tmp;
#if GLIB_CHECK_VERSION (2,16,0)
							if (!g_strcmp0(name, "jpeg") || !g_strcmp0(name, "tga") || !g_strcmp0(name, "png")) {
#endif
								tmp = g_strconcat(list, "<b>", name, "</b> ", (void *)0);
#if GLIB_CHECK_VERSION (2,16,0)
							} else {
								tmp = g_strconcat(list, name, " ", (void *)0);
							}
#endif
							g_free(list);
							list = tmp;
						}
					}
					if (list && !strstr(list, "jpeg")) {
						gchar *tmp =  g_strconcat(list, "\n<b>No JPEG support</b>\n", (void *)0);
						g_free(list);
						list = tmp;
					}
					if (list && !strstr(list, "png")) {
						gchar *tmp =  g_strconcat(list, "\n<b>No PNG support</b>\n", (void *)0);
						g_free(list);
						list = tmp;
					}
					if (list && !strstr(list, "tga")) {
						gchar *tmp =  g_strconcat(list, "\n<b>No TGA support</b>\n", (void *)0);
						g_free(list);
						list = tmp;
					}
					g_slist_free(formats);

					GtkLabel* label = GTK_LABEL(gtk_label_new((const gchar*) 0));
					gtk_label_set_markup(label, list ? list : "");
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 3, 4,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
			}
		}
		{
			GtkFrame* frame = create_dialog_frame(_("<b>OpenGL Properties</b>"));
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), FALSE, FALSE, 0);
			{
				GtkTable* table = create_dialog_table(3, 2, 4, 4, 4);
				gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(table));
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("Vendor:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 0, 1,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("Version:")));
					gtk_widget_show(GTK_WIDGET(label));
					gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 1, 2,
									(GtkAttachOptions) (GTK_FILL),
									(GtkAttachOptions) (0), 0, 0);
					gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
				}
				{
					GtkLabel* label = GTK_LABEL(gtk_label_new(_("Renderer:")));
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
				GtkFrame* frame = create_dialog_frame(_("<b>OpenGL Extensions</b>"));
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
			{
				GtkFrame* frame = create_dialog_frame(_("<b>OpenAL Properties</b>"));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), FALSE, FALSE, 0);
				{
					GtkTable* table = create_dialog_table(3, 2, 4, 4, 4);
					gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(table));
					{
						GtkLabel* label = GTK_LABEL(gtk_label_new(_("Vendor:")));
						gtk_widget_show(GTK_WIDGET(label));
						gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 0, 1,
										(GtkAttachOptions) (GTK_FILL),
										(GtkAttachOptions) (0), 0, 0);
						gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
					}
					{
						GtkLabel* label = GTK_LABEL(gtk_label_new(_("Version:")));
						gtk_widget_show(GTK_WIDGET(label));
						gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 1, 2,
										(GtkAttachOptions) (GTK_FILL),
										(GtkAttachOptions) (0), 0, 0);
						gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
					}
					{
						GtkLabel* label = GTK_LABEL(gtk_label_new(_("Renderer:")));
						gtk_widget_show(GTK_WIDGET(label));
						gtk_table_attach(table, GTK_WIDGET(label), 0, 1, 2, 3,
										(GtkAttachOptions) (GTK_FILL),
										(GtkAttachOptions) (0), 0, 0);
						gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
					}
					{
						GtkLabel* label = GTK_LABEL(gtk_label_new(reinterpret_cast<const char*>(alGetString(AL_VENDOR))));
						gtk_widget_show(GTK_WIDGET(label));
						gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 0, 1,
										(GtkAttachOptions) (GTK_FILL),
										(GtkAttachOptions) (0), 0, 0);
						gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
					}
					{
						GtkLabel* label = GTK_LABEL(gtk_label_new(reinterpret_cast<const char*>(alGetString(AL_VERSION))));
						gtk_widget_show(GTK_WIDGET(label));
						gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 1, 2,
										(GtkAttachOptions) (GTK_FILL),
										(GtkAttachOptions) (0), 0, 0);
						gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
					}
					{
						GtkLabel* label = GTK_LABEL(gtk_label_new(reinterpret_cast<const char*>(alGetString(AL_RENDERER))));
						gtk_widget_show(GTK_WIDGET(label));
						gtk_table_attach(table, GTK_WIDGET(label), 1, 2, 2, 3,
										(GtkAttachOptions) (GTK_FILL),
										(GtkAttachOptions) (0), 0, 0);
						gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
					}
				}
			}
			{
				GtkFrame* frame = create_dialog_frame(_("<b>OpenAL Extensions</b>"));
				gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(frame), TRUE, TRUE, 0);
				{
					GtkScrolledWindow* sc_extensions = create_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, 4);
					gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET(sc_extensions));
					{
						GtkWidget* text_extensions = gtk_text_view_new();
						gtk_text_view_set_editable(GTK_TEXT_VIEW(text_extensions), FALSE);
						gtk_container_add (GTK_CONTAINER (sc_extensions), text_extensions);
						GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_extensions));
						gtk_text_buffer_set_text(buffer, reinterpret_cast<const char*>(alGetString(AL_EXTENSIONS)), -1);
						gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_extensions), GTK_WRAP_WORD);
						gtk_widget_show(text_extensions);
					}
				}
			}
		}
		{
			GtkHBox* hbox = create_dialog_hbox(4);
			GtkButton* button = create_modal_dialog_button("OK", ok_button);
			gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(button), FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, FALSE, 0);
		}
	}

	modal_dialog_show(window, dialog);

	gtk_widget_destroy(GTK_WIDGET(window));
}
