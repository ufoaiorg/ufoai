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

#include "jobinfo.h"
#include "../exec.h"
#include "gtkutil/window.h"
#include "gtkutil/dialog.h"

static int jobCount;
static GtkListStore* jobList;
static GtkWidget* view;

enum {
	JOB_TITLE,
	JOB_PROGRESS,
	JOB_ACTION,
	JOB_POINTER,
	JOB_TOOLTIP,
	JOB_ACTION_TOOLTIP,

	JOB_COLUMNS
};

static void stopJobCallback (GtkCellRendererToggle *widget, gchar *path, GtkWidget *ignore)
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

GtkWidget *JobInfo_constructNotebookTab (void)
{
	GtkVBox* vbox = create_dialog_vbox(4, 4);

	{
		GtkScrolledWindow* scr = create_scrolled_window(GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(scr), TRUE, TRUE, 0);

		{
			GtkListStore* store = gtk_list_store_new(JOB_COLUMNS, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_BOOLEAN, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_STRING);

			view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
			gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(view), TRUE);

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes("Job", renderer, "text", JOB_TITLE, (char const*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
				gtk_tree_view_column_set_sort_column_id(column, JOB_TITLE);
				gtk_tree_view_column_set_expand(column, TRUE);
			}

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_progress_new();
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes("Progress", renderer, "value", JOB_PROGRESS, (char const*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
				gtk_tree_view_column_set_sort_column_id(column, JOB_PROGRESS);
				gtk_tree_view_column_set_expand(column, TRUE);
				gtk_tree_view_column_set_max_width(column, 100);
			}

			{
				GtkCellRenderer* renderer = gtk_cell_renderer_toggle_new();
				g_object_set(renderer, "activatable", TRUE, (const char*)0);
				g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(stopJobCallback), NULL);
				GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes("", renderer, "active", JOB_ACTION, (char const*)0);
				gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);
				gtk_tree_view_column_set_sort_column_id(column, JOB_TITLE);
				gtk_tree_view_column_set_alignment(column, 0.5);
				gtk_tree_view_column_set_max_width(column, 20);
			}

			gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(view), JOB_ACTION_TOOLTIP);
			gtk_container_add(GTK_CONTAINER(scr), view);

			jobList = store;
		}
	}

	return GTK_WIDGET(vbox);
}

static void updateJobs (gpointer data, gpointer user_data)
{
	Exec* job = (Exec*) data;
	GtkTreeIter iter;

	jobCount++;

	gtk_list_store_append(GTK_LIST_STORE(jobList), &iter);
	gtk_list_store_set(GTK_LIST_STORE(jobList), &iter, JOB_TITLE, job->process_title, JOB_PROGRESS, 50.0,
		JOB_ACTION, TRUE, JOB_POINTER, job, JOB_TOOLTIP, job->process_description, JOB_ACTION_TOOLTIP, "Click to cancel the job", -1);
}

void JobInfo_Update (void)
{
	GList *jobs = exec_get_cmd_list();

	jobCount = 0;
	gtk_list_store_clear(GTK_LIST_STORE(jobList));

	g_list_foreach(jobs, updateJobs, (void*) 0);
}

/**
 * @sa JobInfo_Destroy
 */
void JobInfo_Construct (void)
{
}

/**
 * @sa JobInfo_Construct
 */
void JobInfo_Destroy (void)
{
}
