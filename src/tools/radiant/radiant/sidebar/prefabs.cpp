/**
 * @file prefabs.cpp
 */

/*
 Copyright (C) 1997-2008 UFO:AI Team

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

#include "prefabs.h"
#include "radiant_i18n.h"
#include "stream/stringstream.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/image.h"
#include "gtkutil/pointer.h"
#include "../mainframe.h"
#include "../map.h"
#include "../select.h"
#include "os/path.h"
#include "os/dir.h"
#include "autoptr.h"
#include "ifilesystem.h"
#include "archivelib.h"
#include "script/scripttokeniser.h"

static const int TABLE_COLUMS = 3;
static GtkListStore* store;
static GtkTreeView* view;
static GtkTextView* prefabDescription;
static int unselectAfterInsert = 0;
static int replaceSelection = 0;

namespace {
	enum {
		PREFAB_NAME,
		PREFAB_DESCRIPTION,
		PREFAB_IMAGE,
		PREFAB_STORE_SIZE
	};
}

static const char *Prefab_GetPath(StringOutputStream &fullpath, const char *file) {
	fullpath << AppPath_get() << "prefabs/" << PathCleaned(file);
	return fullpath.c_str();
}

static gint PrefabList_button_press(GtkWidget *widget, GdkEventButton *event,
		gpointer data) {
	if (event->type == GDK_2BUTTON_PRESS) {
		GtkTreeModel* model;
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(view),
				&model, &iter) == FALSE) {
			return FALSE;
		} else {
			char* text;
			StringOutputStream fullpath(256);
			UndoableCommand undo("mapImport");

			gtk_tree_model_get(model, &iter, PREFAB_NAME, &text, -1);

			if (replaceSelection)
				Select_Delete();
			if (unselectAfterInsert)
				Selection_Deselect();

			Map_ImportFile(Prefab_GetPath(fullpath, text));
			g_free(text);

			return TRUE;
		}
	}
	return FALSE;
}

static void PrefabAdd(const char *name) {
	StringOutputStream fullBaseNamePath(256);
	StringOutputStream imagePath(256);
	StringOutputStream descriptionPath(256);
	GtkTreeIter iter;
	const char *description = "";
	char buffer[256];

	// remove extension from prefab filename
	CopiedString baseName = StringRange(name, path_get_filename_base_end(name));
	Prefab_GetPath(fullBaseNamePath, baseName.c_str());
	imagePath << fullBaseNamePath.c_str() << ".jpg";
	descriptionPath << fullBaseNamePath.c_str() << ".txt";
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(
			descriptionPath.c_str()));
	if (file) {
		TextInputStream &stream = file->getInputStream();
		stream.read(buffer, file->size());
		buffer[file->size()] = '\0';
		description = buffer;
	}
	GdkPixbuf *img = pixbuf_new_from_file_with_mask(imagePath.c_str());
	if (!img)
		g_warning("Could not find image (%s) for prefab %s\n", baseName.c_str(), name);
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, PREFAB_NAME, name, PREFAB_DESCRIPTION, description, PREFAB_IMAGE, img, -1);
}

static void PrefabList_selection_changed (GtkTreeSelection* selection, gpointer data)
{
	GtkTreeModel* model;
	GtkTreeIter selected;
	if (gtk_tree_selection_get_selected(selection, &model, &selected)) {
		char *description;
		gtk_tree_model_get(model, &selected, PREFAB_DESCRIPTION, &description, -1);
		GtkTextBuffer* buffer = gtk_text_view_get_buffer(prefabDescription);
		gtk_text_buffer_set_text(buffer, description, -1);
	}
}

static void Unselect_toggled (GtkWidget *widget, gpointer data)
{
	unselectAfterInsert = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void Replace_toggled (GtkWidget *widget, gpointer data)
{
	replaceSelection = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

class CLoadPrefab
{
	public:
		CLoadPrefab (const char* path)
		{
		}
		void operator() (const char* name) const
		{
			PrefabAdd(name);
		}
};

GtkWidget* Prefabs_constructNotebookTab(void) {
	StringOutputStream fullpath(256);
	fullpath << AppPath_get() << "prefabs/";

	GtkWidget* vbox = gtk_vbox_new(FALSE, 2);
	GtkWidget* scr = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
			GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr),
			GTK_SHADOW_IN);

	{
		// prefab list
		store = gtk_list_store_new(PREFAB_STORE_SIZE, G_TYPE_STRING, G_TYPE_STRING,
				GDK_TYPE_PIXBUF);
		view
				= GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
		gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), PREFAB_NAME);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), TRUE);
		g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(PrefabList_button_press), 0);

		{
			GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
			GtkTreeViewColumn* column =
					gtk_tree_view_column_new_with_attributes(
							_("Prefab"), renderer, "text", PREFAB_NAME,
							(char const*) 0);
			gtk_tree_view_column_set_expand(column, TRUE);
			gtk_tree_view_column_set_sort_indicator(column, TRUE);
			gtk_tree_view_column_set_sort_column_id(column, PREFAB_NAME);
			gtk_tree_view_append_column(view, column);
		}

		{
			GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
			GtkTreeViewColumn* column =
					gtk_tree_view_column_new_with_attributes(
							_("Image"), renderer, "pixbuf", PREFAB_IMAGE,
							(char const*) 0);
			gtk_tree_view_column_set_fixed_width(column, 128);
			gtk_tree_view_append_column(view, column);
		}

		gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(view));
		g_object_unref(G_OBJECT(store));
	}

	{
		GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
		g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(PrefabList_selection_changed),
				0);
	}

	{
		// prefab description
		GtkWidget* scr = gtk_scrolled_window_new(0, 0);
		gtk_box_pack_start(GTK_BOX(vbox), scr, FALSE, TRUE, 0);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

		{
			prefabDescription = GTK_TEXT_VIEW(gtk_text_view_new());
			widget_set_size(GTK_WIDGET(prefabDescription), 0, 0); // as small as possible
			gtk_text_view_set_wrap_mode(prefabDescription, GTK_WRAP_WORD);
			gtk_text_view_set_editable(prefabDescription, FALSE);
			gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(prefabDescription));
		}
	}

	{
		// options
		GtkWidget* vboxOptions = gtk_vbox_new(FALSE, 2);
		gtk_box_pack_start(GTK_BOX(vbox), vboxOptions, FALSE, TRUE, 0);
		GtkCheckButton* checkUnselect = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(_("Deselect before insert")));
		gtk_widget_ref(GTK_WIDGET(checkUnselect));
		gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(checkUnselect), TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(checkUnselect), "handler", gint_to_pointer(g_signal_connect(G_OBJECT(checkUnselect),
				"toggled", G_CALLBACK(Unselect_toggled), 0)));

		GtkCheckButton* checkReplace = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(_("Replace current selection")));
		gtk_widget_ref(GTK_WIDGET(checkReplace));
		gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(checkReplace), TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(checkReplace), "handler", gint_to_pointer(g_signal_connect(G_OBJECT(checkReplace),
				"toggled", G_CALLBACK(Replace_toggled), 0)));
	}

	Directory_forEach(fullpath.c_str(), MatchFileExtension<CLoadPrefab> ("map", CLoadPrefab(fullpath.c_str())));

	return vbox;
}
