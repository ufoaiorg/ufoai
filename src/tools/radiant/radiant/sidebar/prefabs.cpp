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
#include "os/file.h"
#include "autoptr.h"
#include "ifilesystem.h"
#include "archivelib.h"
#include "script/scripttokeniser.h"

static const int TABLE_COLUMS = 3;
static GtkTreeStore* store;
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

/**
 * @brief callback function used to add prefab entries to store
 * @param name filename relativ to prefabs directory
 * @param parentIter parent iterator to add content to, used for tree structure of directories
 */
void PrefabAdd(const char *name, GtkTreeIter* parentIter) {
	StringOutputStream fullBaseNamePath(256);
	StringOutputStream imagePath(256);
	StringOutputStream descriptionPath(256);
	GtkTreeIter iter;
	const char *description = "";
	char buffer[256];
	StringOutputStream nameContent(256);
	nameContent << "<b>" << name << "</b>";
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
		nameContent << "\n" << description;
	}
	GdkPixbuf *img = pixbuf_new_from_file_with_mask(imagePath.c_str());
	if (!img)
		g_warning("Could not find image (%s) for prefab %s\n", baseName.c_str(), name);
	gtk_tree_store_append(store, &iter, parentIter);
	gtk_tree_store_set(store, &iter, PREFAB_NAME, name, PREFAB_DESCRIPTION, nameContent.c_str(), PREFAB_IMAGE, img, -1);
}

static void PrefabList_selection_changed (GtkTreeSelection* selection, gpointer data)
{
	GtkTreeModel* model;
	GtkTreeIter selected;
	if (gtk_tree_selection_get_selected(selection, &model, &selected)) {
		char *description;
		gtk_tree_model_get(model, &selected, PREFAB_DESCRIPTION, &description, -1);
		GtkTextBuffer* buffer = gtk_text_view_get_buffer(prefabDescription);
		if (description) {
			gtk_text_buffer_set_text(buffer, description, -1);
		} else {
			gtk_text_buffer_set_text(buffer, "", -1);
		}
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

/**
 * @brief Callback class invoked for adding prefab map files
 */
class CLoadPrefab
{
	private:
		CopiedString m_path;
		GtkTreeIter* m_parentIter;
	public:
		/**
		 * @param path relative to 'prefabs/' into directory where file hits were found
		 * @param parentIter parent iterator to add content to
		 */
		CLoadPrefab (const char* path, GtkTreeIter* parentIter) :
			m_path(path), m_parentIter(parentIter)
		{
		}

		/**
		 * @brief called for every file matching map prefix
		 * @param name found filename
		 */
		void operator() (const char* name) const
		{
			StringOutputStream fullpath(256);
			fullpath << m_path.c_str() << "/" << name;
			PrefabAdd(fullpath.c_str(), m_parentIter);
			g_debug("file: %s\n", fullpath.c_str());
		}
};

/**
 * @brief Callback class for searching for subdirectories to add prefab map files
 */
class CLoadPrefabSubdir
{
	private:
		CopiedString m_path;
		CopiedString m_subpath;
		GtkTreeIter* m_parentIter;
	public:
		/**
		 * @param path base path to prefab directory
		 * @param subpath path relative to prefabs directory which is examined
		 * @param parentIter parent iterator to add found prefab map files to
		 */
		CLoadPrefabSubdir (const char* path, const char* subpath, GtkTreeIter* parentIter): m_path(path), m_subpath(subpath), m_parentIter(parentIter)
		{
		}

		/**
		 * @brief called for every file or directory entry in basepath/subpath directory
		 * @param name found filename
		 * @note for every directory entry a sub iterator is created, afterwards this sub directory is examined
		 */
		void operator() (const char* name) const
		{
			if (strstr(name, ".svn") != 0)
				return;
			StringOutputStream fullpath(256);
			fullpath << m_path.c_str() << "/";
			if (!m_subpath.empty())
				fullpath << m_subpath.c_str() << "/";
			fullpath << name;
			if (file_is_directory(fullpath.c_str())) {
				GtkTreeIter subIter;
				gtk_tree_store_append(store, &subIter, m_parentIter);
				gtk_tree_store_set(store, &subIter, PREFAB_NAME, name, PREFAB_DESCRIPTION, name, -1);
				g_debug("directory: %s\n", name);
				StringOutputStream subPath(128);
				subPath << m_subpath.c_str() << "/" << name;
				Directory_forEach(fullpath.c_str(), CLoadPrefabSubdir(m_path.c_str(), subPath.c_str(), &subIter));
				Directory_forEach(fullpath.c_str(), MatchFileExtension<CLoadPrefab> ("map", CLoadPrefab(
						subPath.c_str(), &subIter)));
			} else {
				g_debug("ignoring %s as directory\n", name);
			}
		}
};

GtkWidget* Prefabs_constructNotebookTab(void) {
	StringOutputStream fullpath(256);
	fullpath << AppPath_get() << "prefabs/";

	GtkWidget* vbox = gtk_vbox_new(FALSE, 2);
	GtkWidget* scr = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr),
			GTK_SHADOW_IN);

	{
		// prefab list
		store = gtk_tree_store_new(PREFAB_STORE_SIZE, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF);
		view
				= GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
		gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), PREFAB_NAME);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
		g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(PrefabList_button_press), 0);

		{
			GtkTreeViewColumn* column =
					gtk_tree_view_column_new();
			gtk_tree_view_column_set_title(column,
							_("Prefab"));
			gtk_tree_view_column_set_expand(column, FALSE);
			gtk_tree_view_column_set_sort_indicator(column, TRUE);
			gtk_tree_view_column_set_sort_column_id(column, PREFAB_NAME);
			gtk_tree_view_append_column(view, column);
			GtkCellRenderer* imageRenderer = gtk_cell_renderer_pixbuf_new();
			gtk_cell_renderer_set_fixed_size(imageRenderer, 128, -1);
			gtk_tree_view_column_pack_start(column, imageRenderer, false);
			gtk_tree_view_column_add_attribute(column, imageRenderer, "pixbuf", PREFAB_IMAGE);
			GtkCellRenderer* nameRenderer = gtk_cell_renderer_text_new();
			gtk_tree_view_column_pack_end(column, nameRenderer, false);
			gtk_tree_view_column_add_attribute(column, nameRenderer, "markup", PREFAB_DESCRIPTION);
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
	/* fill prefab store with data */
	Directory_forEach(fullpath.c_str(), CLoadPrefabSubdir(fullpath.c_str(),"", NULL));
	Directory_forEach(fullpath.c_str(), MatchFileExtension<CLoadPrefab> ("map", CLoadPrefab("",NULL)));
	return vbox;
}
