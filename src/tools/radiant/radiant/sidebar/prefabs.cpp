/**
 * @file prefabs.cpp
 */

/*
 Copyright (C) 2002-2009 UFO: Alien Invasion.

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
#include "AutoPtr.h"
#include "ifilesystem.h"
#include "archivelib.h"
#include "script/scripttokeniser.h"
#include "../camwindow.h"

static const int TABLE_COLUMS = 3;
namespace
{
	static GtkTreeStore* store;
	static GtkTreeModel *fileFiltered;
	static GtkTreeView* view;
	static GtkEntry *filterEntry;

	enum selectionStrategy
	{
		PREFAB_SELECT_EXTEND, PREFAB_SELECT_REPLACE, PREFAB_SELECT_UNSELECT
	};
}
static int selectedSelectionStrategy = PREFAB_SELECT_EXTEND;

namespace
{
	enum
	{
		PREFAB_NAME, PREFAB_DESCRIPTION, PREFAB_IMAGE, PREFAB_SHORTNAME, /**< filename or directory name for searching */
		PREFAB_STORE_SIZE
	};
}

static const std::string Prefab_GetPath (const std::string& file)
{
	StringOutputStream fullpath(256);
	fullpath << PathCleaned(file.c_str());
	return AppPath_get() + "prefabs/" + fullpath.c_str();
}

static gint PrefabList_button_press (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->type == GDK_2BUTTON_PRESS) {
		GtkTreeModel* model;
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(view), &model, &iter) == FALSE) {
			return FALSE;
		} else {
			char* text;
			UndoableCommand undo("mapImport");

			gtk_tree_model_get(model, &iter, PREFAB_NAME, &text, -1);

			switch (selectedSelectionStrategy) {
			case PREFAB_SELECT_REPLACE:
				Select_Delete();
				break;
			case PREFAB_SELECT_UNSELECT:
				Selection_Deselect();
				break;
			case PREFAB_SELECT_EXTEND:
				//do nothing
				break;
			}

			Map_ImportFile(Prefab_GetPath(text));
			g_free(text);
			gtk_widget_grab_focus(CamWnd_getWidget(*g_pParentWnd->GetCamWnd()));
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * @brief callback function used to add prefab entries to store
 * @param name filename relative to prefabs directory
 * @param parentIter parent iterator to add content to, used for tree structure of directories
 */
void PrefabAdd (const std::string& name, GtkTreeIter* parentIter)
{
	GtkTreeIter iter;
	const char *description = "";
	char buffer[256];
	StringOutputStream nameContent(256);
	// remove extension from prefab filename
	CopiedString baseName = StringRange(name.c_str(), path_get_filename_base_end(name.c_str()));
	CopiedString fileName =
			StringRange(path_get_filename_start(name.c_str()), path_get_filename_base_end(name.c_str()));
	std::string fullBaseNamePath = Prefab_GetPath(baseName.c_str());
	nameContent << "<b>" << fileName.c_str() << "</b>";
	std::string imagePath = fullBaseNamePath + ".jpg";
	std::string descriptionPath = fullBaseNamePath + ".txt";
	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(descriptionPath));
	if (file) {
		TextInputStream &stream = file->getInputStream();
		const std::size_t realsize = stream.read(buffer, file->size());
		buffer[realsize] = '\0';
		description = buffer;
		nameContent << "\n" << description;
	}
	GdkPixbuf *img = pixbuf_new_from_file_with_mask(imagePath);
	if (!img)
		g_warning("Could not find image (%s) for prefab %s\n", baseName.c_str(), name.c_str());
	gtk_tree_store_append(store, &iter, parentIter);
	gtk_tree_store_set(store, &iter, PREFAB_NAME, name.c_str(), PREFAB_DESCRIPTION, nameContent.c_str(), PREFAB_IMAGE,
			img, PREFAB_SHORTNAME, fileName.c_str(), -1);
}

/**
 * @brief callback function for selection buttons to update current selected selection strategy.
 * @param widget button that was invoked
 * @param buttonID buttonID for selected strategy
 */
static void Prefab_SelectionOptions_toggled (GtkWidget *widget, gpointer buttonID)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		selectedSelectionStrategy = gpointer_to_int(buttonID);
}

/* forward declaration */
static gboolean Prefab_FilterDirectory (GtkTreeModel *model, GtkTreeIter *possibleDirectory);

/**
 * @brief This method decides whether a given prefab entry should be visible or not.
 * For this it checks whether name contains searched text. If not, it checks whether entry
 * is a directory and whether sub-entries will be valid after filtering.
 * @param model tree model to get content from
 * @param entry entry to decide whether it should be visible
 * @return true if entry should be visible because it matches the searched pattern or is a directory with content
 */
static gboolean Prefab_FilterFileOrDirectory (GtkTreeModel *model, GtkTreeIter *entry)
{
	const char* searchText = gtk_entry_get_text(filterEntry);
	char* currentEntry;
	gtk_tree_model_get(model, entry, PREFAB_SHORTNAME, &currentEntry, -1);
	if (strstr(currentEntry, searchText) != 0)
		return true;
	else
	// check whether there are children in base model (directory)
	if (gtk_tree_model_iter_has_child(model, entry))
		return Prefab_FilterDirectory(model, entry);
	else
		return false;
}

/**
 * @brief This method decides whether a given prefab directory entry should be visible.
 * It checks whether any of the children should be visible and is itself visible if so.
 * @param model tree model to get content from
 * @param possibleDirectory directory entry which should be checked for visibility
 * @return true if directory will have content after filtering
 */
static gboolean Prefab_FilterDirectory (GtkTreeModel *model, GtkTreeIter *possibleDirectory)
{
	int children = gtk_tree_model_iter_n_children(model, possibleDirectory);
	for (int i = 0; i < children; i++) {
		GtkTreeIter child;
		if (gtk_tree_model_iter_nth_child(model, &child, possibleDirectory, i))
			if (Prefab_FilterFileOrDirectory(model, &child))
				return true;
	}
	return false;

}

/**
 * @brief This method does the filtering work for prefab list
 * @param model current model to filter
 * @param iter iterator to current row which should be checked
 * @return true if entry should be visible (being a directory or matching search content), otherwise false
 */
static gboolean Prefab_FilterFiles (GtkTreeModel *model, GtkTreeIter *iter)
{
#if GTK_CHECK_VERSION(2,14,0)
	if (gtk_entry_get_text_length(filterEntry) == 0)
		return true;
#else
	if (strlen(gtk_entry_get_text(filterEntry)) == 0)
	return true;
#endif
	return Prefab_FilterFileOrDirectory(model, iter);
}

/**
 * @brief Callback for 'changed' event of search entry used to reinit file filter
 * @return false
 */
static gboolean Prefab_Refilter ()
{
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(fileFiltered));
	return false;
}

/**
 * @brief Callback class invoked for adding prefab map files
 */
class CLoadPrefab
{
	private:
		std::string m_path;
		GtkTreeIter* m_parentIter;
	public:
		/**
		 * @param path relative to 'prefabs/' into directory where file hits were found
		 * @param parentIter parent iterator to add content to
		 */
		CLoadPrefab (const std::string& path, GtkTreeIter* parentIter) :
			m_path(path), m_parentIter(parentIter)
		{
		}

		/**
		 * @brief called for every file matching map prefix
		 * @param name found filename
		 */
		void operator() (const std::string& name) const
		{
			std::string fullpath = m_path + "/" + name;
			PrefabAdd(fullpath, m_parentIter);
			g_debug("file: %s\n", fullpath.c_str());
		}
};

/**
 * @brief Callback class for searching for subdirectories to add prefab map files
 */
class CLoadPrefabSubdir
{
	private:
		std::string m_path;
		std::string m_subpath;
		GtkTreeIter* m_parentIter;
	public:
		/**
		 * @param path base path to prefab directory
		 * @param subpath path relative to prefabs directory which is examined
		 * @param parentIter parent iterator to add found prefab map files to
		 */
		CLoadPrefabSubdir (const std::string& path, const std::string& subpath, GtkTreeIter* parentIter) :
			m_path(path), m_subpath(subpath), m_parentIter(parentIter)
		{
		}

		/**
		 * @brief called for every file or directory entry in basepath/subpath directory
		 * @param name found filename
		 * @note for every directory entry a sub iterator is created, afterwards this sub directory is examined
		 */
		void operator() (const std::string& name) const
		{
			if (name.find(".svn") != std::string::npos)
				return;
			std::string fullpath = m_path + "/";
			if (!m_subpath.empty())
				fullpath += m_subpath + "/";
			fullpath += name;
			if (file_is_directory(fullpath)) {
				GtkTreeIter subIter;
				std::string sectionDescription = "<b>" + name + "</b>";
				gtk_tree_store_append(store, &subIter, m_parentIter);
				gtk_tree_store_set(store, &subIter, PREFAB_NAME, name.c_str(), PREFAB_DESCRIPTION, sectionDescription.c_str(),
						PREFAB_SHORTNAME, name.c_str(), -1);
				g_debug("directory: %s\n", name.c_str());
				std::string subPath = m_subpath + "/" + name;
				Directory_forEach(fullpath, CLoadPrefabSubdir(m_path, subPath, &subIter));
				Directory_forEach(fullpath, MatchFileExtension<CLoadPrefab> ("map", CLoadPrefab(subPath, &subIter)));
			}
		}
};

GtkWidget* Prefabs_constructNotebookTab (void)
{
	std::string fullpath = AppPath_get() + "prefabs/";

	GtkWidget* vbox = gtk_vbox_new(FALSE, 2);
	GtkWidget* scr = gtk_scrolled_window_new(0, 0);
	gtk_box_pack_start(GTK_BOX(vbox), scr, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

	// prefab list
	store = gtk_tree_store_new(PREFAB_STORE_SIZE, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING);
	//prepare file filter
	fileFiltered = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
	GtkTreeModel *fileSorted = gtk_tree_model_sort_new_with_model(fileFiltered);
	view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(fileSorted));

	{
		gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), PREFAB_SHORTNAME);
		gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), TRUE);
		gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), TRUE);
#if GTK_CHECK_VERSION(2,10,0)
		gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
#endif
		g_signal_connect(G_OBJECT(view), "button_press_event", G_CALLBACK(PrefabList_button_press), 0);

		{
			GtkTreeViewColumn* column = gtk_tree_view_column_new();
			gtk_tree_view_column_set_title(column, _("Prefab"));
			gtk_tree_view_column_set_expand(column, FALSE);
			gtk_tree_view_column_set_sort_indicator(column, TRUE);
			gtk_tree_view_column_set_sort_column_id(column, PREFAB_SHORTNAME);
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

	GtkWidget* hboxFooter = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hboxFooter, FALSE, TRUE, 0);
	{
		// options
		GtkWidget* vboxOptions = gtk_vbox_new(FALSE, 3);
		gtk_box_pack_start(GTK_BOX(hboxFooter), vboxOptions, FALSE, TRUE, 0);
		GtkRadioButton* radioExtendSelection =
				GTK_RADIO_BUTTON(gtk_radio_button_new_with_label(NULL, _("Extend current selection")));
		gtk_widget_ref(GTK_WIDGET(radioExtendSelection));
		gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(radioExtendSelection), TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(radioExtendSelection), "handler", gint_to_pointer(
				g_signal_connect(radioExtendSelection,
						"toggled", G_CALLBACK(Prefab_SelectionOptions_toggled), gint_to_pointer(PREFAB_SELECT_EXTEND))));

		GtkRadioButton
				* radioUnselect =
						GTK_RADIO_BUTTON(gtk_radio_button_new_with_label_from_widget(radioExtendSelection, _("Deselect before insert")));
		gtk_widget_ref(GTK_WIDGET(radioUnselect));
		gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(radioUnselect), TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(radioUnselect), "handler", gint_to_pointer(g_signal_connect(G_OBJECT(radioUnselect),
				"toggled", G_CALLBACK(Prefab_SelectionOptions_toggled), gint_to_pointer(PREFAB_SELECT_UNSELECT))));

		GtkRadioButton
				* radioReplace =
						GTK_RADIO_BUTTON(gtk_radio_button_new_with_label_from_widget(radioUnselect, _("Replace current selection")));
		gtk_widget_ref(GTK_WIDGET(radioReplace));
		gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(radioReplace), TRUE, TRUE, 0);
		g_object_set_data(G_OBJECT(radioReplace), "handler", gint_to_pointer(g_signal_connect(G_OBJECT(radioReplace),
				"toggled", G_CALLBACK(Prefab_SelectionOptions_toggled), gint_to_pointer(PREFAB_SELECT_REPLACE))));
	}
	{
		//search entry, connect to file filter
		GtkWidget* vboxSearch = gtk_vbox_new(FALSE, 3);
		gtk_box_pack_start(GTK_BOX(hboxFooter), vboxSearch, FALSE, TRUE, 3);
		GtkWidget *searchEntry = gtk_entry_new();
		gtk_box_pack_start(GTK_BOX(vboxSearch), searchEntry, FALSE, TRUE, 3);
		gtk_widget_show(searchEntry);

		gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(fileFiltered),
				(GtkTreeModelFilterVisibleFunc) Prefab_FilterFiles, NULL, NULL);
		g_signal_connect(G_OBJECT(searchEntry), "changed", G_CALLBACK(Prefab_Refilter), NULL );
		filterEntry = GTK_ENTRY(searchEntry);
#if GTK_CHECK_VERSION(2,10,0)
		gtk_tree_view_set_search_entry(view, filterEntry);
#endif

	}
	/* fill prefab store with data */
	Directory_forEach(fullpath.c_str(), CLoadPrefabSubdir(fullpath, "", NULL));
	Directory_forEach(fullpath.c_str(), MatchFileExtension<CLoadPrefab> ("map", CLoadPrefab("", NULL)));
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(fileFiltered));
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(fileSorted), PREFAB_SHORTNAME, GTK_SORT_ASCENDING);
	return vbox;
}
