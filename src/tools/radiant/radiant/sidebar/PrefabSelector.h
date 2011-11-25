/**
 * @file PrefabSelector.h
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

#ifndef PREFABS_H_
#define PREFABS_H_

#include <gtk/gtk.h>
#include <string>
#include "sidebar.h"

namespace sidebar
{

	class PrefabSelector : public ui::SidebarComponent
	{
		private:

			GtkWidget* _widget;

			GtkTreeStore* _store;

			GtkTreeModel* _fileFiltered;

			GtkTreeModel* _fileSorted;

			GtkTreeView* _view;

			GtkEntry* _filterEntry;

			int _selectedSelectionStrategy;

		private:

			// Private constructor, creates GTK widgets
			PrefabSelector ();

			/* GTK CALLBACKS */
			static gboolean callbackFilterFiles (GtkTreeModel *model, GtkTreeIter *iter, PrefabSelector *self);
			static gboolean callbackRefilter (PrefabSelector *self);
			static void callbackSelectionOptionToggleExtend (GtkWidget *widget, PrefabSelector *self);
			static void callbackSelectionOptionToggleReplace (GtkWidget *widget, PrefabSelector *self);
			static void callbackSelectionOptionToggleUnselect (GtkWidget *widget, PrefabSelector *self);
			static gint callbackButtonPress (GtkWidget *widget, GdkEventButton *event, PrefabSelector *self);

			/* GTK CALLBACK HELPER FUNCTIONS */
			static gboolean FilterFileOrDirectory (GtkTreeModel *model, GtkTreeIter *entry, PrefabSelector *self);
			static gboolean FilterDirectory (GtkTreeModel *model, GtkTreeIter *possibleDirectory, PrefabSelector *self);
		public:

			/** greebo: Contains the static instance of this dialog.
			 * Constructs the instance and calls toggle() when invoked.
			 */
			static PrefabSelector& Instance ();

			static std::string GetFullPath (const std::string& file);

			GtkWidget* getWidget () const;

			const std::string getTitle() const;
	};
}

#endif
