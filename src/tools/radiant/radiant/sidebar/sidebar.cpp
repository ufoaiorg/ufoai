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
#include "gtkutil/widget.h"
#include "ieventmanager.h"
#include "iradiant.h"
#include "../log/console.h"

#include "entitylist/EntityList.h"
#include "entityinspector/EntityInspector.h"
#include "PrefabSelector.h"
#include "surfaceinspector/surfaceinspector.h"
#include "texturebrowser.h"
#include "MapInfo.h"
#include "JobInfo.h"

namespace ui {

Sidebar::Sidebar ()
{
	_widget = gtk_vbox_new(FALSE, 0);

	_notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(_notebook), TRUE);

	/* if you change the order here - make sure to also change the toggle functions tab page indices */
	constructTextureBrowser();
	constructSurfaceInspector();
	constructEntityInspector();
	constructPrefabBrowser();
	constructInfo();

	gtk_box_pack_start(GTK_BOX(_widget), GTK_WIDGET(_notebook), TRUE, TRUE, 0);
	gtk_widget_show_all(_widget);

	// console
	GtkWidget* console_window = Console_constructWindow(GlobalRadiant().getMainWindow());
	gtk_box_pack_end(GTK_BOX(_widget), GTK_WIDGET(console_window), FALSE, FALSE, 0);

	GlobalEventManager().addCommand("ToggleSidebar", MemberCaller<Sidebar, &Sidebar::toggleSidebar> (*this));
	GlobalEventManager().addCommand("ToggleSurfaceInspector", MemberCaller<Sidebar, &Sidebar::toggleSurfaceInspector> (
			*this));
	GlobalEventManager().addCommand("ToggleEntityInspector", MemberCaller<Sidebar, &Sidebar::toggleEntityInspector> (
			*this));
	GlobalEventManager().addCommand("TogglePrefabs", MemberCaller<Sidebar, &Sidebar::togglePrefabs> (*this));
	GlobalEventManager().addCommand("ToggleTextureBrowser", MemberCaller<Sidebar, &Sidebar::toggleTextureBrowser> (
			*this));
}

Sidebar::~Sidebar ()
{
	ui::SurfaceInspector::Instance().shutdown();
	//EntityList::Instance().shutdown();
	//EntityInspector::getInstance().shutdown();
	//sidebar::MapInfo::getInstance().shutdown();
	//sidebar::JobInfo::getInstance().shutdown();
	TextureBrowser_Destroy();
}

void Sidebar::addWidget(const std::string& title, GtkWidget* widget)
{
	if (!widget)
		return;

	GtkWidget *label = gtk_label_new_with_mnemonic(title.c_str());
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(vbox), widget);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	gtk_notebook_append_page(GTK_NOTEBOOK(_notebook), swin, label);
}

void Sidebar::constructPrefabBrowser ()
{
	// prefabs
	GtkWidget *pagePrefabs = sidebar::PrefabSelector::ConstructNotebookTab();
	if (!pagePrefabs)
		return;

	addWidget(_("Prefabs"), pagePrefabs);
}

void Sidebar::constructEntityInspector ()
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	GtkWidget *pageEntityList = EntityList::Instance().getWidget();
	GtkWidget *pageEntityInspector = EntityInspector::getInstance().getWidget();

	gtk_container_add(GTK_CONTAINER(vbox), pageEntityList);
	gtk_container_add(GTK_CONTAINER(vbox), pageEntityInspector);

	addWidget(_("_Entities"), vbox);
}

void Sidebar::constructSurfaceInspector ()
{
	GtkWidget *pageSurfaceInspector = ui::SurfaceInspector::Instance().getWidget();
	addWidget(_("_Surfaces"), pageSurfaceInspector);
}

void Sidebar::constructInfo ()
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	GtkWidget *pageMapInfo = sidebar::MapInfo::getInstance().getWidget();
	GtkWidget *pageJobInfo = sidebar::JobInfo::getInstance().getWidget();

	gtk_container_add(GTK_CONTAINER(vbox), pageMapInfo);
	gtk_container_add(GTK_CONTAINER(vbox), pageJobInfo);

	addWidget(_("Info"), vbox);
}

void Sidebar::constructTextureBrowser ()
{
	GtkWidget *pageTextureBrowser = GlobalTextureBrowser().getWidget();
	addWidget(_("Textures"), pageTextureBrowser);
}

void Sidebar::toggleSidebar ()
{
	widget_toggle_visible(_widget);
}

void Sidebar::toggleTextureBrowser ()
{
	if (!widget_is_visible(GTK_WIDGET(_widget)))
		widget_set_visible(GTK_WIDGET(_widget), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(_notebook), 3);
}

void Sidebar::togglePrefabs ()
{
	if (!widget_is_visible(GTK_WIDGET(_widget)))
		widget_set_visible(GTK_WIDGET(_widget), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(_notebook), 2);
}

void Sidebar::toggleSurfaceInspector ()
{
	if (!widget_is_visible(GTK_WIDGET(_widget)))
		widget_set_visible(GTK_WIDGET(_widget), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(_notebook), 1);
}

void Sidebar::toggleEntityInspector ()
{
	if (!widget_is_visible(GTK_WIDGET(_widget)))
		widget_set_visible(GTK_WIDGET(_widget), TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(_notebook), 0);
}

GtkWidget* Sidebar::getWidget ()
{
	return _widget;
}
}
