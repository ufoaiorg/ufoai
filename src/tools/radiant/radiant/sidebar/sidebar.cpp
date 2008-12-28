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
#include "../commands.h"
#include "gtkutil/widget.h"

#include <gtk/gtk.h>

static void Sidebar_constructEntities (GtkWidget *notebook)
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

static void Sidebar_constructSurfaces (GtkWidget *notebook)
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

static void Sidebar_constructMapInfo (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new("Map Info");
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// map info frame
	GtkWidget *pageMapInfo = MapInfo_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageMapInfo);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static void Sidebar_constructJobInfo (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new("Job Info");
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// job info frame
	GtkWidget *pageJobInfo = JobInfo_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageJobInfo);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static GtkWidget *notebook;

void ToggleSidebar (void)
{
	widget_toggle_visible(notebook);
}

static void SidebarButtonToggle (GtkWidget* widget, gpointer data)
{
	ToggleSidebar();
}

GtkWidget *Sidebar_construct (void)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	GtkWidget *button = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(button), image);
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(SidebarButtonToggle), 0);
	widget_set_size(GTK_WIDGET(button), 20, 20);

	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(button), FALSE, FALSE, 0);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(notebook), TRUE, TRUE, 0);

	Sidebar_constructEntities(notebook);
	Sidebar_constructSurfaces(notebook);
	Sidebar_constructMapInfo(notebook);
	Sidebar_constructJobInfo(notebook);

	gtk_widget_show_all(vbox);

	GlobalCommands_insert("ToggleSidebar", FreeCaller<ToggleSidebar> (), Accelerator('S'));

	return vbox;
}
