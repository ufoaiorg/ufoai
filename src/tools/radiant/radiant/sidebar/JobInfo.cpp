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

namespace sidebar
{
	enum
	{
		JOB_TITLE, JOB_PROGRESS, JOB_ACTION, JOB_POINTER, JOB_TOOLTIP, JOB_ACTION_TOOLTIP,

		JOB_COLUMNS
	};

	// Constructor
	JobInfo::JobInfo () :
		_widget(gtk_vbox_new(FALSE, 0)), _jobList(gtk_list_store_new(JOB_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE,
				G_TYPE_BOOLEAN, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING)), _view(gtk_tree_view_new_with_model(
				GTK_TREE_MODEL(_jobList)))
	{
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(_widget), GTK_WIDGET(scr), TRUE, TRUE, 0);
		gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(_view), TRUE);

		GtkTreeViewColumn* columnJobName = gtk_tree_view_column_new_with_attributes(_("Job"),
				gtk_cell_renderer_text_new(), "text", JOB_TITLE, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnJobName);
		gtk_tree_view_column_set_sort_column_id(columnJobName, JOB_TITLE);
		gtk_tree_view_column_set_expand(columnJobName, TRUE);

		GtkCellRenderer* rendererProgress = gtk_cell_renderer_progress_new();
		GtkTreeViewColumn* columnProgress = gtk_tree_view_column_new_with_attributes(_("Progress"),
				gtk_cell_renderer_progress_new(), "value", JOB_PROGRESS, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnProgress);
		gtk_tree_view_column_set_sort_column_id(columnProgress, JOB_PROGRESS);
		gtk_tree_view_column_set_expand(columnProgress, TRUE);
		gtk_tree_view_column_set_max_width(columnProgress, 100);
		g_object_set(rendererProgress, "activatable", TRUE, (const char*) 0);
		g_signal_connect(G_OBJECT(rendererProgress), "toggled", G_CALLBACK(stopJobCallback), _view);

		GtkTreeViewColumn* columnToggle = gtk_tree_view_column_new_with_attributes("", gtk_cell_renderer_toggle_new(),
				"active", JOB_ACTION, (char const*) 0);
		gtk_tree_view_append_column(GTK_TREE_VIEW(_view), columnToggle);
		gtk_tree_view_column_set_sort_column_id(columnToggle, JOB_TITLE);
		gtk_tree_view_column_set_alignment(columnToggle, 0.5);
		gtk_tree_view_column_set_max_width(columnToggle, 20);

#if GTK_CHECK_VERSION(2, 12, 0)
		gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(_view), JOB_ACTION_TOOLTIP);
#endif
		gtk_container_add(GTK_CONTAINER(scr), _view);
	}

	void JobInfo::stopJobCallback (GtkCellRendererToggle *widget, gchar *path, GtkWidget *view)
	{
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(view));
		Exec* job;

		if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
			return;

		// get the values from the tree view model list
		gtk_tree_model_get(model, &iter, JOB_POINTER, &job, -1);

		exec_stop(job);
	}

	void JobInfo::updateJobs (gpointer data, gpointer userData)
	{
		Exec* job = (Exec*) data;
		GtkListStore* jobList = (GtkListStore*) userData;
		GtkTreeIter iter;

		gtk_list_store_append(GTK_LIST_STORE(jobList), &iter);
		gtk_list_store_set(GTK_LIST_STORE(jobList), &iter, JOB_TITLE, job->process_title, JOB_PROGRESS, job->fraction
				* 100.0, JOB_ACTION, TRUE, JOB_POINTER, job, JOB_TOOLTIP, job->process_description, JOB_ACTION_TOOLTIP,
				_("Click to cancel the job"), -1);
	}

	void JobInfo::update (void)
	{
		GList *jobs = exec_get_cmd_list();

		gtk_list_store_clear(GTK_LIST_STORE(_jobList));

		g_list_foreach(jobs, updateJobs, _jobList);
	}
}

