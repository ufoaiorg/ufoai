/*
 This file is part of UFORadiant.

 UFORadiant is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 UFORadiant is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with UFORadiant; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#if !defined(INCLUDED_JOBINFO_H)
#define INCLUDED_JOBINFO_H

#include <gtk/gtk.h>
#include "gtkutil/menu/PopupMenu.h"
#include "sidebar.h"

namespace sidebar
{
	class JobInfo : public ui::SidebarComponent
	{
			// Main widget
			GtkWidget* _widget;

			// Main store and view
			GtkListStore* _jobList;
			GtkWidget* _view;

			// Context menu
			gtkutil::PopupMenu _popupMenu;

		private:

			/* GTK CALLBACKS */

			static void stopJobCallback (gpointer data, gpointer userData);
			static void updateJobs (gpointer data, gpointer userData);

		public:

			/** Return the singleton instance.
			 */
			static JobInfo& Instance ()
			{
				static JobInfo _instance;
				return _instance;
			}

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
				return "JobInfo";
			}

			/** Constructor creates GTK widgets.
			 */
			JobInfo ();

			/** Callback that updates the job list
			 */
			void update (void);
	};

	GtkWidget *JobInfo_constructNotebookTab ();
	void JobInfo_Update (void);
}

#endif
