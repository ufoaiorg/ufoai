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
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// entity list
	GtkWidget *page = EntityList_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), page);

	// entity inspector

	gtk_widget_show_all(vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
}
