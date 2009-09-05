/**
 * @file texteditor.cpp
 * @brief Creates and handles a text editor editor widget that can be used to
 * control script file editing
 */

/*
 Copyright(C) 1999-2006 Id Software, Inc. and contributors.
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
#include "radiant_i18n.h"
#include "texteditor.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "gtkutil/widget.h"
#include "../exec.h"
#include "os/file.h"
#include "autoptr.h"
#include "ifilesystem.h"
#include "archivelib.h"
#ifdef WIN32
#include "../gtkmisc.h"
#include "../mainframe.h"
#endif

// master window widget
static GtkWidget *text_editor = 0;
static GtkWidget *text_widget; // slave, text widget from the gtk editor

static gint editor_delete(GtkWidget *widget, gpointer data) {
	if (gtk_MessageBox(widget, _("Close the editor?"), _("UFORadiant"),
			eMB_YESNO, eMB_ICONQUESTION) == eIDNO)
		return TRUE;

	gtk_widget_hide(text_editor);

	return TRUE;
}

static void editor_save(GtkWidget *widget, gpointer data) {
	char *filename = (char*) g_object_get_data(G_OBJECT(data), "filename");

	const std::string& enginePath = GlobalRadiant().getEnginePath();
	const std::string& baseGame = GlobalRadiant().getRequiredGameDescriptionKeyValue("basegame");
	std::string fullpath = enginePath + baseGame + "/" + std::string(filename);

	TextFileOutputStream out(fullpath);
	if (out.failed()) {
		g_message("Error saving file to '%s'.", fullpath.c_str());
		gtk_MessageBox(GTK_WIDGET(data), _("Error saving file"));
		return;
	}

	gpointer text = g_object_get_data(G_OBJECT(data), "text");
	GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	GtkTextIter iterEnd, iterStart;
	gtk_text_buffer_get_start_iter(text_buffer, &iterStart);
	gtk_text_buffer_get_end_iter(text_buffer, &iterEnd);
	gchar *str = gtk_text_buffer_get_text(text_buffer, &iterStart, &iterEnd,
			FALSE);
	if (str)
		out << str << '\n';
}

static void editor_close(GtkWidget *widget, gpointer data) {
	if (gtk_MessageBox(text_editor, _("Close the editor?"), _("UFORadiant"),
			eMB_YESNO, eMB_ICONQUESTION) == eIDNO)
		return;

	gtk_widget_hide(text_editor);
}

static void CreateGtkTextEditor(void) {
	GtkWidget *dlg;
	GtkWidget *vbox, *hbox, *button, *scr, *text;

	dlg = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(dlg), "delete_event", G_CALLBACK(editor_delete), dlg);
	gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 300);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(dlg), GTK_WIDGET(vbox));
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

	scr = gtk_scrolled_window_new(0, 0);
	gtk_widget_show(scr);
	gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

	text = gtk_text_view_new();
	gtk_container_add(GTK_CONTAINER(scr), text);
	gtk_widget_show(text);
	g_object_set_data(G_OBJECT(dlg), "text", text);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), TRUE);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox), FALSE, TRUE, 0);

	button = gtk_button_new_with_label(_("Close"));
	gtk_widget_show(button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(editor_close), dlg);

	button = gtk_button_new_with_label(_("Save"));
	gtk_widget_show(button);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(editor_save), dlg);

	text_editor = dlg;
	text_widget = text;
}

/**
 * @brief Text editor dialog
 * @note E.g. used for material editor
 * @param[in] filename Absolute path of the text file to open
 * @param[in] cursorpos Offset to place the cursor to
 * @sa editor_save
 */
void DoTextEditor(const char* filename, int cursorpos,
		const char *appendContent) {
	if (!text_editor)
		CreateGtkTextEditor(); // build it the first time we need it
	else if (widget_is_visible(text_editor)) {
		if (appendContent) {
			GtkTextIter iter;
			GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
					GTK_TEXT_VIEW(text_widget));
			gtk_text_buffer_get_end_iter(text_buffer, &iter);
			gtk_text_buffer_insert(text_buffer, &iter, appendContent, strlen(
					appendContent));
		}
		return;
	} else {
		GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
				GTK_TEXT_VIEW(text_widget));
		gtk_text_buffer_set_text(text_buffer, "", 0);
	}

	void *old_filename;

	gtk_window_set_title(GTK_WINDOW(text_editor), filename);
	GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(text_widget));

	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(filename));
	if (file) {
		const std::size_t size = file->size();
		char *buf = (char *) malloc(size + 1);
		TextInputStream &stream = file->getInputStream();
		const std::size_t realsize = stream.read(buf, size);
		buf[realsize] = '\0';

		gtk_text_buffer_set_text(text_buffer, (char *) buf, size);

		free(buf);
	}

	if (appendContent && strlen(appendContent)) {
		GtkTextIter iter;
		GtkTextTag *tag = gtk_text_buffer_create_tag(text_buffer,
				"newly-added", "weight", PANGO_WEIGHT_BOLD, "foreground",
				"red", NULL);
		gtk_text_buffer_get_end_iter(text_buffer, &iter);
		gtk_text_buffer_insert_with_tags(text_buffer, &iter, appendContent,
				strlen(appendContent), tag, (const char *) 0);
	}

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
}
