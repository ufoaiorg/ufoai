/**
 * @file maptools.cpp
 * @brief Creates the maptools dialogs - like checking for errors and compiling
 * a map
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
#include "maptools.h"
#include "cmdlib.h"   // Q_Exec
#include "os/file.h"  // file_exists
#include "scenelib.h" // g_brushCount
#include "gtkutil/messagebox.h"  // gtk_MessageBox
#include "stream/stringstream.h"
#include "../qe3.h" // g_brushCount
#include "../map.h"
#include "../preferences.h"
#include "../mainframe.h"
#ifdef WIN32
#include "../gtkmisc.h"
#endif

// master window widget
static GtkWidget *checkDialog = NULL;
static GtkWidget *text_widget; // slave, text widget from the gtk editor

static gint editor_hide (GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(checkDialog);
	return TRUE;
}

static void CreateCheckDialog (void)
{
	GtkWidget *dlg;
	GtkWidget *vbox, *hbox, *button, *scr, *text;

	dlg = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(dlg), "delete_event", G_CALLBACK(editor_hide), NULL);
	gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 300);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(dlg), GTK_WIDGET(vbox));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	scr = gtk_scrolled_window_new(0, 0);
	gtk_widget_show(scr);
	gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

	text = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scr), text);
	gtk_widget_show(text);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

	button = gtk_button_new_with_label("Close");
	gtk_widget_show(button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(editor_hide), NULL);
	gtk_widget_set_usize(button, 60, -2);

	checkDialog = dlg;
	text_widget = text;
}

void ToolsCheckErrors (void)
{
/*	const char* path = getMapsPath(); */
	const char* mapcompiler = g_pGameDescription->getRequiredKeyValue("mapcompiler");
	const char* fullname = Map_Name(g_map);

	if (!ConfirmModified("Check Map"))
		return;

	/* empty map? */
	if (!g_brushCount.get()) {
		gtk_MessageBox(0, "Nothing to check in this map\n", "Map compiling", eMB_OK, eMB_ICONERROR);
		return;
	}

	StringOutputStream fullpath(256);
	fullpath << CompilerPath_get() << mapcompiler;


	if (file_exists(fullpath.c_str())) {
		char buf[1024];
		const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param_check");

		snprintf(buf, sizeof(buf) - 1, "%s %s", compiler_parameter, fullname);
		buf[sizeof(buf) - 1] = '\0';

		char* output = Q_Exec(mapcompiler, buf, CompilerPath_get(), false);
		if (output) {
			if (!checkDialog)
				CreateCheckDialog();

#if 0
			/** @todo later when fixing is implemented */
			/* reload the map */
			Map_RegionOff();
			Map_Free();
			Map_LoadFile(fullname);
#endif

			gtk_window_set_title(GTK_WINDOW(checkDialog), "Check output");

			/* skip the header */
			char *checkstart = strstr(output, "Starting map checks\n");
			checkstart = strstr(checkstart, "\n");

			GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_widget));
			gtk_text_buffer_set_text(text_buffer, checkstart, strlen(checkstart));

			/* trying to show later */
			gtk_widget_show(checkDialog);

#ifdef WIN32
			process_gui();
			gtk_widget_queue_draw(text_widget);
#endif

			free(output);
		} else
			globalOutputStream() << "No output for checking " << fullname << "\n";
	} else {
		StringOutputStream message(256);
		message << "Could not find the mapcompiler (" << fullpath.c_str() << ") check your path settings\n";
		gtk_MessageBox(0, message.c_str(), "Map compiling", eMB_OK, eMB_ICONERROR);
	}
}

/**
 * @todo Implement me
 */
void ToolsCompile (void)
{
	const char* mapcompiler = g_pGameDescription->getRequiredKeyValue("mapcompiler");

	if (!ConfirmModified("Compile Map"))
		return;

	/* empty map? */
	if (!g_brushCount.get()) {
		gtk_MessageBox(0, "Nothing to compile in this map\n", "Map compiling", eMB_OK, eMB_ICONERROR);
		return;
	}

	if (file_exists(mapcompiler)) {
		char buf[1024];
		const char* fullname = Map_Name(g_map);
		const char* compiler_parameter = g_pGameDescription->getRequiredKeyValue("mapcompiler_param");

		snprintf(buf, sizeof(buf) - 1, "%s %s", compiler_parameter, fullname);
		buf[sizeof(buf) - 1] = '\0';

		/** @todo thread this and update the main window */
		char* output = Q_Exec(mapcompiler, buf, NULL, false);
		if (output) {
			/** @todo parse and display this in a gtk window */
			globalOutputStream() << output;
			free(output);
		}
	}
}
