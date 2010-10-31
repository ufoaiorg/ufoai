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
#include "radiant_i18n.h"
#include "../commands.h"
#include "gtkutil/widget.h"
#include "ieventmanager.h"

static void Sidebar_constructPrefabs (GtkWidget *notebook)
{
	// prefabs
	GtkWidget *pagePrefabs = sidebar::PrefabSelector::ConstructNotebookTab();
	if (!pagePrefabs)
		return;

	GtkWidget *label = gtk_label_new_with_mnemonic(_("Prefabs"));
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(vbox), pagePrefabs);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static void Sidebar_constructEntities (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new_with_mnemonic(_("_Entities"));
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
	GtkWidget *label = gtk_label_new_with_mnemonic(_("_Surfaces"));
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
	GtkWidget *label = gtk_label_new(_("Map Info"));
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// map info frame
	GtkWidget *pageMapInfo = sidebar::MapInfo::getInstance().getWidget();
	gtk_container_add(GTK_CONTAINER(vbox), pageMapInfo);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static void Sidebar_constructJobInfo (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new(_("Job Info"));
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	// job info frame
	GtkWidget *pageJobInfo = sidebar::JobInfo::getInstance().getWidget();
	gtk_container_add(GTK_CONTAINER(vbox), pageJobInfo);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static void Sidebar_constructTextureBrowser (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new(_("Textures"));
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	GtkWidget *pageTextureBrowser = TextureBrowser_constructNotebookTab();
	gtk_container_add(GTK_CONTAINER(vbox), pageTextureBrowser);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static void Sidebar_constructParticleBrowser (GtkWidget *notebook)
{
	GtkWidget *label = gtk_label_new(_("Particles"));
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	GtkWidget *pageParticleBrowser = ui::ParticleBrowser::getInstance().getWidget();
	gtk_container_add(GTK_CONTAINER(vbox), pageParticleBrowser);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), swin, label);
}

static GtkWidget *notebook;

void ToggleSidebar (void)
{
	widget_toggle_visible(notebook);
}

void ToggleParticleBrowser (void)
{
	if (!widget_is_visible(GTK_WIDGET(notebook)))
		widget_set_visible(GTK_WIDGET(notebook), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 4);
}

void ToggleTextureBrowser (void)
{
	if (!widget_is_visible(GTK_WIDGET(notebook)))
		widget_set_visible(GTK_WIDGET(notebook), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 3);
}

void TogglePrefabs (void)
{
	if (!widget_is_visible(GTK_WIDGET(notebook)))
		widget_set_visible(GTK_WIDGET(notebook), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 2);
}

void ToggleSurfaceInspector (void)
{
	if (!widget_is_visible(GTK_WIDGET(notebook)))
		widget_set_visible(GTK_WIDGET(notebook), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 1);
}

void ToggleEntityInspector (void)
{
	if (!widget_is_visible(GTK_WIDGET(notebook)))
		widget_set_visible(GTK_WIDGET(notebook), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
}

GtkWidget *Sidebar_construct (void)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(notebook), TRUE, TRUE, 0);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);

	/* if you change the order here - make sure to also change the toggle functions tab page indices */
	Sidebar_constructEntities(notebook);
	Sidebar_constructSurfaces(notebook);
	Sidebar_constructPrefabs(notebook);
	Sidebar_constructTextureBrowser(notebook);
	Sidebar_constructParticleBrowser(notebook);
	Sidebar_constructMapInfo(notebook);
	Sidebar_constructJobInfo(notebook);

	gtk_widget_show_all(vbox);

	GlobalEventManager().addCommand("ToggleSidebar", FreeCaller<ToggleSidebar> ());
	GlobalEventManager().addCommand("ToggleSurfaceInspector", FreeCaller<ToggleSurfaceInspector> ());
	GlobalEventManager().addCommand("ToggleEntityInspector", FreeCaller<ToggleEntityInspector> ());
	GlobalEventManager().addCommand("TogglePrefabs", FreeCaller<TogglePrefabs> ());
	GlobalEventManager().addCommand("ToggleTextureBrowser", FreeCaller<ToggleTextureBrowser> ());
	GlobalEventManager().addCommand("ToggleParticleBrowser", FreeCaller<ToggleTextureBrowser> ());

	return vbox;
}
