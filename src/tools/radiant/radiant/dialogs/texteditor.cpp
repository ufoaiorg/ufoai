/**
 * @file texteditor.cpp
 * @brief Creates and handles a text editor editor widget that can be used to
 * control script file editing
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

#include "gtkutil/messagebox.h"
#include <gtk/gtk.h>
#include "texteditor.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "cmdlib.h"
#ifdef WIN32
#include "../gtkmisc.h"
#include "../mainframe.h"
#endif

// master window widget
static GtkWidget *text_editor = 0;
static GtkWidget *text_widget; // slave, text widget from the gtk editor

static gint editor_delete (GtkWidget *widget, gpointer data)
{
	if (gtk_MessageBox(widget, "Close the editor?", "UFORadiant", eMB_YESNO, eMB_ICONQUESTION) == eIDNO)
		return TRUE;

	gtk_widget_hide(text_editor);

	return TRUE;
}

static void editor_save (GtkWidget *widget, gpointer data)
{
	FILE *f = fopen((char*)g_object_get_data(G_OBJECT(data), "filename"), "w");
	gpointer text = g_object_get_data(G_OBJECT(data), "text");

	if (f == NULL) {
		gtk_MessageBox(GTK_WIDGET(data), "Error saving file !");
		return;
	}

	char *str = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);
	fwrite(str, 1, strlen(str), f);
	fclose(f);
}

static void editor_close (GtkWidget *widget, gpointer data)
{
	if (gtk_MessageBox(text_editor, "Close the editor?", "UFORadiant", eMB_YESNO, eMB_ICONQUESTION) == eIDNO)
		return;

	gtk_widget_hide(text_editor);
}

static void CreateGtkTextEditor(void)
{
	GtkWidget *dlg;
	GtkWidget *vbox, *hbox, *button, *scr, *text;

	dlg = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(dlg), "delete_event",
	                 G_CALLBACK(editor_delete), 0);
	gtk_window_set_default_size (GTK_WINDOW (dlg), 600, 300);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_widget_show (vbox);
	gtk_container_add(GTK_CONTAINER(dlg), GTK_WIDGET(vbox));
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	scr = gtk_scrolled_window_new (0, 0);
	gtk_widget_show (scr);
	gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scr), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

	text = gtk_text_view_new();
	gtk_container_add (GTK_CONTAINER (scr), text);
	gtk_widget_show (text);
	g_object_set_data (G_OBJECT (dlg), "text", text);
	gtk_text_view_set_editable (GTK_TEXT_VIEW(text), TRUE);

	hbox = gtk_hbox_new (FALSE, 5);
	gtk_widget_show (hbox);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

	button = gtk_button_new_with_label ("Close");
	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
	                 G_CALLBACK(editor_close), dlg);
	gtk_widget_set_usize (button, 60, -2);

	button = gtk_button_new_with_label ("Save");
	gtk_widget_show (button);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked",
	                 G_CALLBACK(editor_save), dlg);
	gtk_widget_set_usize (button, 60, -2);

	text_editor = dlg;
	text_widget = text;
}

static void DoGtkTextEditor (const char* filename, guint cursorpos)
{
	if (!text_editor)
		CreateGtkTextEditor(); // build it the first time we need it

	// Load file
	FILE *f = fopen(filename, "r");

	if (f == 0) {
		globalOutputStream() << "Unable to load file " << filename << " in shader editor.\n";
		gtk_widget_hide (text_editor);
	} else {
		fseek(f, 0, SEEK_END);
		int len = ftell(f);
		void *buf = malloc(len);
		void *old_filename;

		rewind(f);
		fread(buf, 1, len, f);

		gtk_window_set_title(GTK_WINDOW (text_editor), filename);

		GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_widget));
		gtk_text_buffer_set_text(text_buffer, (char*)buf, len);

		old_filename = g_object_get_data(G_OBJECT(text_editor), "filename");
		if (old_filename)
			free(old_filename);
		g_object_set_data(G_OBJECT(text_editor), "filename", strdup(filename));

		// trying to show later
		gtk_widget_show(text_editor);

#ifdef WIN32
		process_gui();
#endif

		// only move the cursor if it's not exceeding the size..
		// NOTE: this is erroneous, cursorpos is the offset in bytes, not in characters
		// len is the max size in bytes, not in characters either, but the character count is below that limit..
		// thinking .. the difference between character count and byte count would be only because of CR/LF?
		{
			GtkTextIter text_iter;
			// character offset, not byte offset
			gtk_text_buffer_get_iter_at_offset(text_buffer, &text_iter, cursorpos);
			gtk_text_buffer_place_cursor(text_buffer, &text_iter);
		}

#ifdef WIN32
		gtk_widget_queue_draw(text_widget);
#endif

		free(buf);
		fclose(f);
	}
}

#ifdef WIN32
#include <gdk/gdkwin32.h>
// use the file associations to open files instead of builtin Gtk editor
bool g_TextEditor_useWin32Editor = true;
#else
// custom shader editor
bool g_TextEditor_useCustomEditor = false;
CopiedString g_TextEditor_editorCommand("");
#endif

void DoTextEditor (const char* filename, int cursorpos)
{
#ifdef WIN32
	if (g_TextEditor_useWin32Editor) {
		globalOutputStream() << "opening file '" << filename << "' (line " << cursorpos << " info ignored)\n";
		ShellExecute((HWND)GDK_WINDOW_HWND (GTK_WIDGET(MainFrame_getWindow())->window), "open", filename, 0, 0, SW_SHOW);
		return;
	}
#else
	/* check if a custom editor is set */
	if (g_TextEditor_useCustomEditor && !g_TextEditor_editorCommand.empty()) {
		char *output = NULL;
		StringOutputStream strEditCommand(256);
		strEditCommand << g_TextEditor_editorCommand.c_str() << " \"" << filename << "\"";

		globalOutputStream() << "Launching: " << strEditCommand.c_str() << "\n";
		exec_run_cmd(strEditCommand.c_str(), &output);
		if (!output) {
			globalWarningStream() << "Failed to execute " << strEditCommand.c_str() << ", using default\n";
		} else {
			free((void*)output);
			/* the command (appeared) to run successfully, no need to do anything more */
			return;
		}
	}
#endif

	DoGtkTextEditor(filename, cursorpos);
}
