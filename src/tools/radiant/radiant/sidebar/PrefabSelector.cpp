/**
 * @file PrefabSelector.cpp
 */

/*
 Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "PrefabSelector.h"
#include "radiant_i18n.h"
#include "stream/stringstream.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/image.h"
#include "gtkutil/pointer.h"
#include "gtkutil/TreeModel.h"
#include "../map/map.h"
#include "../select.h"
#include "../environment.h"
#include "os/path.h"
#include "os/dir.h"
#include "os/file.h"
#include "AutoPtr.h"
#include "ifilesystem.h"
#include "iregistry.h"
#include "iundo.h"
#include "archivelib.h"
#include "../camera/GlobalCamera.h"
#include "imaterial.h"
#include "../selection/algorithm/General.h"

namespace sidebar
{
	enum selectionStrategy
	{
		PREFAB_SELECT_EXTEND, PREFAB_SELECT_REPLACE, PREFAB_SELECT_UNSELECT
	};

	enum
	{
		PREFAB_NAME, PREFAB_DESCRIPTION, PREFAB_IMAGE, PREFAB_SHORTNAME, /**< filename or directory name for searching */
		PREFAB_STORE_SIZE
	};

	namespace
	{
		/**
		 * @brief Callback class invoked for adding prefab map files
		 */
		class LoadPrefabFileFunctor
		{
			private:
				std::string _path;
				GtkTreeIter* _parentIter;
				GtkTreeStore* _store;

			public:
				/**
				 * @param path relative to 'prefabs/' into directory where file hits were found
				 * @param parentIter parent iterator to add content to
				 */
				LoadPrefabFileFunctor (const std::string& path, GtkTreeIter* parentIter, GtkTreeStore *store) :
					_path(path), _parentIter(parentIter), _store(store)
				{
				}

				/**
				 * @brief callback function used to add prefab entries to store
				 * @param name filename relative to prefabs directory
				 * @param parentIter parent iterator to add content to, used for tree structure of directories
				 */
				void PrefabAdd (const std::string& name) const
				{
					GtkTreeIter iter;
					const char *description = "";
					char buffer[256];
					StringOutputStream nameContent(256);
					// remove extension from prefab filename
					const std::string fileName = os::getFilenameFromPath(name);
					const std::string fullBaseName = os::stripExtension(name);
					const std::string shortName = os::stripExtension(fileName);
					const std::string fullBaseNamePath = PrefabSelector::GetFullPath(fullBaseName);
					nameContent << "<b>" << shortName << "</b>";
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
						g_message("Could not find image (%s) for prefab %s\n", fileName.c_str(), name.c_str());
					gtk_tree_store_append(_store, &iter, _parentIter);
					gtk_tree_store_set(_store, &iter, PREFAB_NAME, name.c_str(), PREFAB_DESCRIPTION,
							nameContent.c_str(), PREFAB_IMAGE, img, PREFAB_SHORTNAME, shortName.c_str(), -1);
				}

				/**
				 * @brief called for every file matching map prefix
				 * @param name found filename
				 */
				void operator() (const std::string& name) const
				{
					const std::string fullpath = _path + "/" + name;
					PrefabAdd(fullpath);
				}
		};

		/**
		 * @brief Callback class for searching for subdirectories to add prefab map files
		 */
		class LoadPrefabDirectoryFunctor
		{
			private:
				std::string _path;
				std::string _subpath;
				GtkTreeIter* _parentIter;
				GtkTreeStore* _store;
			public:
				/**
				 * @param path base path to prefab directory
				 * @param subpath path relative to prefabs directory which is examined
				 * @param parentIter parent iterator to add found prefab map files to
				 */
				LoadPrefabDirectoryFunctor (const std::string& path, const std::string& subpath, GtkTreeIter* parentIter,
						GtkTreeStore* store) :
					_path(path), _subpath(subpath), _parentIter(parentIter), _store(store)
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
					std::string fullpath = _path + "/";
					if (!_subpath.empty())
						fullpath += _subpath + "/";
					fullpath += name;
					if (file_is_directory(fullpath)) {
						GtkTreeIter subIter;
						std::string sectionDescription = "<b>" + name + "</b>";
						gtk_tree_store_append(_store, &subIter, _parentIter);
						gtk_tree_store_set(_store, &subIter, PREFAB_NAME, name.c_str(), PREFAB_DESCRIPTION,
								sectionDescription.c_str(), PREFAB_SHORTNAME, name.c_str(), -1);
						std::string subPath = _subpath + "/" + name;
						Directory_forEach(fullpath, LoadPrefabDirectoryFunctor(_path, subPath, &subIter, _store));
						Directory_forEach(fullpath, MatchFileExtension<LoadPrefabFileFunctor> ("map", LoadPrefabFileFunctor(subPath,
								&subIter, _store)));
					}
				}
		};
	}

	PrefabSelector::PrefabSelector () :
		_widget(gtk_vbox_new(FALSE, 2)), _store(gtk_tree_store_new(PREFAB_STORE_SIZE, G_TYPE_STRING, G_TYPE_STRING,
				GDK_TYPE_PIXBUF, G_TYPE_STRING)),
				_fileFiltered(gtk_tree_model_filter_new(GTK_TREE_MODEL(_store), NULL)), _fileSorted(
						GTK_TREE_MODEL(gtk_tree_model_sort_new_with_model(_fileFiltered))), _view(
						GTK_TREE_VIEW(gtk_tree_view_new_with_model(_fileSorted)))
	{
		std::string fullpath = GlobalRegistry().get(RKEY_APP_PATH) + "prefabs/";
		_selectedSelectionStrategy = PREFAB_SELECT_EXTEND;

		GtkWidget* scr = gtk_scrolled_window_new(0, 0);
		gtk_box_pack_start(GTK_BOX(_widget), scr, TRUE, TRUE, 0);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
		gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scr), GTK_SHADOW_IN);

		{
			gtk_tree_view_set_enable_search(_view, TRUE);
			gtk_tree_view_set_search_column(_view, PREFAB_SHORTNAME);
			gtk_tree_view_set_headers_visible(_view, TRUE);
			gtk_tree_view_set_headers_clickable(_view, TRUE);
#if GTK_CHECK_VERSION(2,10,0)
			gtk_tree_view_set_grid_lines(_view, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
#endif
			g_signal_connect(G_OBJECT(_view), "button_press_event", G_CALLBACK(callbackButtonPress), this);

			{
				GtkTreeViewColumn* column = gtk_tree_view_column_new();
				gtk_tree_view_column_set_title(column, _("Prefab"));
				gtk_tree_view_column_set_expand(column, FALSE);
				gtk_tree_view_column_set_sort_indicator(column, TRUE);
				gtk_tree_view_column_set_sort_column_id(column, PREFAB_SHORTNAME);
				gtk_tree_view_append_column(_view, column);
				GtkCellRenderer* imageRenderer = gtk_cell_renderer_pixbuf_new();
				gtk_cell_renderer_set_fixed_size(imageRenderer, 128, -1);
				gtk_tree_view_column_pack_start(column, imageRenderer, false);
				gtk_tree_view_column_add_attribute(column, imageRenderer, "pixbuf", PREFAB_IMAGE);
				GtkCellRenderer* nameRenderer = gtk_cell_renderer_text_new();
				gtk_tree_view_column_pack_end(column, nameRenderer, false);
				gtk_tree_view_column_add_attribute(column, nameRenderer, "markup", PREFAB_DESCRIPTION);
			}

			gtk_container_add(GTK_CONTAINER(scr), GTK_WIDGET(_view));
			g_object_unref(G_OBJECT(_store));
		}

		GtkWidget* hboxFooter = gtk_hbox_new(TRUE, 0);
		gtk_box_pack_start(GTK_BOX(_widget), hboxFooter, FALSE, TRUE, 0);
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
							"toggled", G_CALLBACK(callbackSelectionOptionToggleExtend), this)));

			GtkRadioButton
					* radioUnselect =
							GTK_RADIO_BUTTON(gtk_radio_button_new_with_label_from_widget(radioExtendSelection, _("Deselect before insert")));
			gtk_widget_ref(GTK_WIDGET(radioUnselect));
			gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(radioUnselect), TRUE, TRUE, 0);
			g_object_set_data(G_OBJECT(radioUnselect), "handler", gint_to_pointer(
					g_signal_connect(G_OBJECT(radioUnselect),
							"toggled", G_CALLBACK(callbackSelectionOptionToggleUnselect), this)));

			GtkRadioButton
					* radioReplace =
							GTK_RADIO_BUTTON(gtk_radio_button_new_with_label_from_widget(radioUnselect, _("Replace current selection")));
			gtk_widget_ref(GTK_WIDGET(radioReplace));
			gtk_box_pack_start(GTK_BOX(vboxOptions), GTK_WIDGET(radioReplace), TRUE, TRUE, 0);
			g_object_set_data(G_OBJECT(radioReplace), "handler", gint_to_pointer(
					g_signal_connect(G_OBJECT(radioReplace),
							"toggled", G_CALLBACK(callbackSelectionOptionToggleReplace), this)));
		}
		{
			//search entry, connect to file filter
			GtkWidget* vboxSearch = gtk_vbox_new(FALSE, 3);
			gtk_box_pack_start(GTK_BOX(hboxFooter), vboxSearch, FALSE, TRUE, 3);
			GtkWidget *searchEntry = gtk_entry_new();
			gtk_box_pack_start(GTK_BOX(vboxSearch), searchEntry, FALSE, TRUE, 3);
			gtk_widget_show(searchEntry);

			gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(_fileFiltered),
					(GtkTreeModelFilterVisibleFunc) callbackFilterFiles, this, NULL);
			g_signal_connect(G_OBJECT(searchEntry), "changed", G_CALLBACK(callbackRefilter), NULL);
			_filterEntry = GTK_ENTRY(searchEntry);
#if GTK_CHECK_VERSION(2,10,0)
			gtk_tree_view_set_search_entry(_view, _filterEntry);
#endif
		}
		/* fill prefab store with data */
		Directory_forEach(fullpath, LoadPrefabDirectoryFunctor(fullpath, "", NULL, _store));
		Directory_forEach(fullpath, MatchFileExtension<LoadPrefabFileFunctor> ("map", LoadPrefabFileFunctor("", NULL, _store)));
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(_fileFiltered));
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(_fileSorted), PREFAB_SHORTNAME, GTK_SORT_ASCENDING);
	}

	/**
	 * @brief This method decides whether a given prefab entry should be visible or not.
	 * For this it checks whether name contains searched text. If not, it checks whether entry
	 * is a directory and whether sub-entries will be valid after filtering.
	 * @param model tree model to get content from
	 * @param entry entry to decide whether it should be visible
	 * @return true if entry should be visible because it matches the searched pattern or is a directory with content
	 */
	gboolean PrefabSelector::FilterFileOrDirectory (GtkTreeModel *model, GtkTreeIter *entry, PrefabSelector *self)
	{
		std::string searchText = gtk_entry_get_text(self->_filterEntry);
		std::string currentEntry = gtkutil::TreeModel::getString(model, entry, PREFAB_SHORTNAME);
		if (string::contains(currentEntry, searchText))
			return true;
		else
		// check whether there are children in base model (directory)
		if (gtk_tree_model_iter_has_child(model, entry))
			return FilterDirectory(model, entry, self);
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
	gboolean PrefabSelector::FilterDirectory (GtkTreeModel *model, GtkTreeIter *possibleDirectory, PrefabSelector *self)
	{
		int children = gtk_tree_model_iter_n_children(model, possibleDirectory);
		for (int i = 0; i < children; i++) {
			GtkTreeIter child;
			if (gtk_tree_model_iter_nth_child(model, &child, possibleDirectory, i))
				if (FilterFileOrDirectory(model, &child, self))
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
	gboolean PrefabSelector::callbackFilterFiles (GtkTreeModel *model, GtkTreeIter *iter, PrefabSelector *self)
	{
#if GTK_CHECK_VERSION(2,14,0)
		if (gtk_entry_get_text_length(self->_filterEntry) == 0)
			return true;
#else
		if (strlen(gtk_entry_get_text(self->_filterEntry)) == 0)
		return true;
#endif
		return FilterFileOrDirectory(model, iter, self);
	}

	/**
	 * @brief Callback for 'changed' event of search entry used to reinit file filter
	 * @return false
	 */
	gboolean PrefabSelector::callbackRefilter (PrefabSelector *self)
	{
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(self->_fileFiltered));
		return false;
	}

	PrefabSelector& PrefabSelector::Instance ()
	{
		static PrefabSelector _selector;
		return _selector;
	}

	GtkWidget* PrefabSelector::getWidget () const
	{
		return _widget;
	}

	const std::string PrefabSelector::getTitle() const
	{
		return std::string(_("Prefabs"));
	}

	/**
	 * @param baseFileName Filename relative to the prefabs directory without extension
	 * @return The full (absolute) path to the file (but still without extension of course)
	 */
	std::string PrefabSelector::GetFullPath (const std::string& baseFileName)
	{
		return os::standardPath(GlobalRegistry().get(RKEY_APP_PATH) + "prefabs/" + baseFileName);
	}

	/* GTK CALLBACKS */

	/**
	 * @brief callback function for selection buttons to update current selected selection strategy.
	 * @param widget button that was invoked
	 * @param self The prefab selector object
	 */
	void PrefabSelector::callbackSelectionOptionToggleExtend (GtkWidget *widget, PrefabSelector *self)
	{
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			self->_selectedSelectionStrategy = PREFAB_SELECT_EXTEND;
	}

	/**
	 * @brief callback function for selection buttons to update current selected selection strategy.
	 * @param widget button that was invoked
	 * @param self The prefab selector object
	 */
	void PrefabSelector::callbackSelectionOptionToggleReplace (GtkWidget *widget, PrefabSelector *self)
	{
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			self->_selectedSelectionStrategy = PREFAB_SELECT_REPLACE;
	}

	/**
	 * @brief callback function for selection buttons to update current selected selection strategy.
	 * @param widget button that was invoked
	 * @param self The prefab selector object
	 */
	void PrefabSelector::callbackSelectionOptionToggleUnselect (GtkWidget *widget, PrefabSelector *self)
	{
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			self->_selectedSelectionStrategy = PREFAB_SELECT_UNSELECT;
	}

	gint PrefabSelector::callbackButtonPress (GtkWidget *widget, GdkEventButton *event, PrefabSelector *self)
	{
		if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
			GtkTreeModel* model;
			GtkTreeIter iter;
			if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(self->_view), &model, &iter) == FALSE) {
				return FALSE;
			} else {
				UndoableCommand undo("mapImport");

				const std::string text = gtkutil::TreeModel::getString(model, &iter, PREFAB_NAME);

				switch (self->_selectedSelectionStrategy) {
				case PREFAB_SELECT_REPLACE:
					selection::algorithm::deleteSelection();
					break;
				case PREFAB_SELECT_UNSELECT:
					Selection_Deselect();
					break;
				case PREFAB_SELECT_EXTEND:
					//do nothing
					break;
				}

				const std::string fileName = PrefabSelector::GetFullPath(text);
				GlobalMap().importFile(fileName);
				GlobalMaterialSystem()->importMaterialFile(os::stripExtension(fileName) + ".mat");
				return TRUE;
			}
		}
		return FALSE;
	}
}
