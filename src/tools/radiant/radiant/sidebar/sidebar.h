/*
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

#ifndef INCLUDED_SIDEBAR_H
#define INCLUDED_SIDEBAR_H

#include "entitylist.h"
#include "entityinspector/entityinspector.h"
#include "MapInfo.h"
#include "surfaceinspector/surfaceinspector.h"
#include "PrefabSelector.h"
#include "JobInfo.h"
#include "texturebrowser.h"

namespace ui {

class Sidebar
{
	private:

		// the sidebar widget
		GtkWidget *_widget;

		// the notebook which is part of the sidebar widget
		GtkWidget *_notebook;

	public:
		// constructs the whole sidebar
		Sidebar ();

		~Sidebar ();

		// returns the sidebar widget
		GtkWidget* getWidget ();

	private:

		// single notebook tabs
		void constructTextureBrowser ();
		void constructEntityInspector ();
		void constructSurfaceInspector ();
		void constructJobInfo ();
		void constructMapInfo ();
		void constructPrefabBrowser ();

		void toggleSidebar (void);
		void toggleTextureBrowser (void);
		void togglePrefabs (void);
		void toggleSurfaceInspector (void);
		void toggleEntityInspector (void);
};

}

#endif
