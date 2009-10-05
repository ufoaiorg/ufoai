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

namespace sidebar
{
	class JobInfo
	{
			// Main widget
			GtkWidget* _widget;

			// Main store and view
			GtkListStore* _jobList;
			GtkWidget* _view;

		private:

			/* GTK CALLBACKS */

			static void stopJobCallback (GtkCellRendererToggle *widget, gchar *path, GtkWidget *ignore);
			static void updateJobs (gpointer data, gpointer userData);

		public:

			/** Return the singleton instance.
			 */
			static JobInfo& getInstance ()
			{
				static JobInfo _instance;
				return _instance;
			}

			/** Return the main widget for packing into
			 * the groupdialog or other parent container.
			 */
			GtkWidget* getWidget ()
			{
				gtk_widget_show_all(_widget);
				return _widget;
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
