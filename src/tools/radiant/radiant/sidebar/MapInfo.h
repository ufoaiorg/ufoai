/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#if !defined(INCLUDED_MAPINFO_H)
#define INCLUDED_MAPINFO_H

#include <gtk/gtk.h>
#include <map>
#include <string>
#include "sidebar.h"

class Selectable;

namespace sidebar
{
	class MapInfo : public ui::SidebarComponent
	{
		private:

			// Main widget
			GtkWidget* _widget;

			// List store to contain the entity list with name and count
			GtkListStore* _store;
			GtkWidget *_view;

			// List store to contain attributes and values for the map
			GtkListStore* _infoStore;

			GtkWidget *_vboxEntityBreakdown;

		private:

			GtkWidget* createEntityBreakdownTreeView ();
			GtkWidget* createInfoPanel ();

			/* GTK CALLBACKS */

			static void removeEntity (gpointer data, gpointer userData);

		public:

			/** Return the singleton instance.
			 */
			static MapInfo& Instance ();

			/** Return the main widget for packing into
			 * the groupdialog or other parent container.
			 */
			GtkWidget* getWidget () const
			{
				gtk_widget_show_all(_widget);
				return _widget;
			}

			const std::string getTitle() const
			{
				return "MapInfo";
			}

			/** Constructor creates GTK widgets.
			 */
			MapInfo ();

			/** Updates the map info data
			 */
			void update ();
	};
}

#endif
