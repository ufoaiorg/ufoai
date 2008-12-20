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

#include "sidebar.h"

void Sidebar_constructEntities (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new("Entities");
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// entity list
	GtkWidget *pageEntityList = EntityList_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageEntityList);

	// entity inspector
	GtkWidget *pageEntityInspector = EntityInspector_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageEntityInspector);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

void Sidebar_constructSurfaces (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new("Surfaces");
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// surface inspector
	GtkWidget *pageSurfaceInspector = SurfaceInspector_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageSurfaceInspector);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

void Sidebar_constructMapInfo (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new("Map Info");
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// surface inspector
	GtkWidget *pageMapInfo = MapInfo_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageMapInfo);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}
