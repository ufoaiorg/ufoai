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

#include <gtk/gtk.h>
#include <string>

namespace ui {

class SidebarComponent
{
	protected:

		int _pageIndex;

	public:
		// Get the Gtk Widget for display in the sidebar
		virtual GtkWidget* getWidget () const = 0;

		virtual const std::string getTitle() const = 0;

		virtual ~SidebarComponent () {
		};

		virtual void switchPage (int page)
		{
		}

		virtual void toggleSidebar (bool visible, int page)
		{
			if (!visible) {
				switchPage(-1);
			} else {
				switchPage(page);
			}
		}

		void setIndex(int pageIndex) {
			_pageIndex = pageIndex;
		}

		void show(GtkWidget *notebook) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), _pageIndex);
		}
};

class Sidebar
{
	private:

		// the sidebar widget
		GtkWidget *_widget;

		// the notebook which is part of the sidebar widget
		GtkWidget *_notebook;

		int _currentPageIndex;

		SidebarComponent &_entityInspector;
		SidebarComponent *_entityList;
		SidebarComponent &_textureBrowser;
		SidebarComponent &_surfaceInspector;
		SidebarComponent &_prefabSelector;
		SidebarComponent &_mapInfo;
		SidebarComponent &_jobInfo;

	public:
		// constructs the whole sidebar
		Sidebar ();

		~Sidebar ();

		// returns the sidebar widget
		GtkWidget* getWidget ();

	private:

		int addWidget(const std::string& title, GtkWidget* widget);

		void addComponents (const std::string &title, SidebarComponent &component1, SidebarComponent &component2);
		void addComponent (SidebarComponent &component);

		void toggleSidebar (void);
		void showTextureBrowser (void);
		void showPrefabs (void);
		void showSurfaceInspector (void);
		void showEntityInspector (void);

		void showComponent (SidebarComponent &component);

		/* gtk signal handlers */
		static gboolean onChangePage (GtkNotebook *notebook, gpointer newPage, guint newPageIndex, Sidebar *self);
};

}

#endif
