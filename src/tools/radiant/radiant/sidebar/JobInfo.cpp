/**
 * @file jobinfo.cpp
 * @brief Job Information sidebar widget (e.g. progress bar for compilation runs)
 */

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

#include "JobInfo.h"
#include "radiant_i18n.h"
#include "../exec.h"
#include "gtkutil/window.h"
#include "gtkutil/dialog.h"
#include "gtkutil/image.h"
#include "gtkutil/IconTextMenuItem.h"
#include "../ui/Icons.h"

namespace sidebar
{
	enum
	{
		JOB_TITLE, JOB_PROGRESS, JOB_POINTER, JOB_TOOLTIP,

		JOB_COLUMNS
	};

	// Constructor
	JobInfo::JobInfo () :
		_widget(gtk_vbox_new(FALSE, 0)), _jobList(gtk_list_store_new(JOB_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE,
				G_TYPE_POINTER, G_TYPE_STRING)), _view(gtk_tree_view_new_with_model(GTK_TREE_MODEL(_jobList))),
				_popupMenu(gtkutil::PopupMenu(_widget))
	{
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(_widget), GTK_WIDGET(scr), TRUE, TRUE, 0);
		gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(_view), TRUE);

		GtkTreeViewColumn* columnJobName = gtk_tree_view_column_new_with_attributes(_("Job"),
				gtk_cell_renderer_text_new(), "text", JOB_TITLE, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnJobName);
		gtk_tree_view_column_set_sort_column_id(columnJobName, JOB_TITLE);
		gtk_tree_view_column_set_expand(columnJobName, TRUE);

		GtkTreeViewColumn* columnProgress = gtk_tree_view_column_new_with_attributes(_("Progress"),
				gtk_cell_renderer_progress_new(), "value", JOB_PROGRESS, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnProgress);
		gtk_tree_view_column_set_sort_column_id(columnProgress, JOB_PROGRESS);
		gtk_tree_view_column_set_expand(columnProgress, TRUE);
		gtk_tree_view_column_set_max_width(columnProgress, 100);

		gtk_container_add(GTK_CONTAINER(scr), _view);

		_popupMenu.addItem(gtkutil::IconTextMenuItem(ui::icons::ICON_FOLDER, _("Abort job")), stopJobCallback, this);
	}

	void JobInfo::stopJobCallback (gpointer data, gpointer userData)
	{
		GtkTreeIter iter;
		JobInfo* jobInfo = (JobInfo*) userData;
		GtkTreeView *view = GTK_TREE_VIEW(jobInfo->_view);
		GtkTreeSelection *selection = gtk_tree_view_get_selection(view);
		GtkTreeModel *model;

		if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
			Exec* job;

			// get the values from the tree view model list
			gtk_tree_model_get(model, &iter, JOB_POINTER, &job, -1);

			exec_stop(job);
		}
	}

	void JobInfo::updateJobs (gpointer data, gpointer userData)
	{
		Exec* job = (Exec*) data;
		GtkListStore* jobList = (GtkListStore*) userData;
		GtkTreeIter iter;

		gtk_list_store_append(GTK_LIST_STORE(jobList), &iter);
		gtk_list_store_set(GTK_LIST_STORE(jobList), &iter, JOB_TITLE, job->process_title, JOB_PROGRESS, job->fraction
				* 100.0, JOB_POINTER, data, JOB_TOOLTIP, job->process_description, -1);
	}

	void JobInfo::update (void)
	{
		GList *jobs = exec_get_cmd_list();

		/** @todo Don't clear and refill but update */
		gtk_list_store_clear(GTK_LIST_STORE(_jobList));
		g_list_foreach(jobs, updateJobs, _jobList);
	}
}
