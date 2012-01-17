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

Sidebar::Sidebar () :
		_entityInspector(EntityInspector::Instance()), _entityList(new EntityList()), _textureBrowser(
				GlobalTextureBrowser()), _surfaceInspector(ui::SurfaceInspector::Instance()), _prefabSelector(
				sidebar::PrefabSelector::Instance()), _mapInfo(sidebar::MapInfo::Instance()), _jobInfo(sidebar::JobInfo::Instance())
{
	_widget = gtk_vbox_new(FALSE, 0);

	_notebook = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(_notebook), TRUE);

	/* if you change the order here - make sure to also change the toggle functions tab page indices */
	addComponent(_textureBrowser);
	addComponent(_surfaceInspector);
	addComponents(_("_Entities"), *_entityList, _entityInspector);
	addComponent(_prefabSelector);
	addComponents(_("Info"), _mapInfo, _jobInfo);

	// console
	GtkWidget* consoleWidget = Console_constructWindow();
	addWidget(_("Console"), consoleWidget);

	gtk_box_pack_start(GTK_BOX(_widget), GTK_WIDGET(_notebook), TRUE, TRUE, 0);
	gtk_widget_show_all(_widget);

	GlobalEventManager().addCommand("ToggleSidebar", MemberCaller<Sidebar, &Sidebar::toggleSidebar> (*this));
	GlobalEventManager().addCommand("ToggleSurfaceInspector", MemberCaller<Sidebar, &Sidebar::showSurfaceInspector> (
			*this));
	GlobalEventManager().addCommand("ToggleEntityInspector", MemberCaller<Sidebar, &Sidebar::showEntityInspector> (
			*this));
	GlobalEventManager().addCommand("TogglePrefabs", MemberCaller<Sidebar, &Sidebar::showPrefabs> (*this));
	GlobalEventManager().addCommand("ToggleTextureBrowser", MemberCaller<Sidebar, &Sidebar::showTextureBrowser> (
			*this));

	g_signal_connect(G_OBJECT(_notebook), "switch-page", G_CALLBACK(onChangePage), this);
}

Sidebar::~Sidebar ()
{
	ui::SurfaceInspector::Instance().shutdown();
	TextureBrowser_Destroy();
	delete _entityList;
}

int Sidebar::addWidget(const std::string& title, GtkWidget* widget)
{
	GtkWidget *label = gtk_label_new_with_mnemonic(title.c_str());
	GtkWidget *swin = gtk_scrolled_window_new(0, 0);
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	// scrollable window settings
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(vbox), widget);

	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(vbox));

	gtk_widget_show_all(swin);
	return gtk_notebook_append_page(GTK_NOTEBOOK(_notebook), swin, label);
}

void Sidebar::addComponent (SidebarComponent &component)
{
	GtkWidget *widget = component.getWidget();
	int index = addWidget(component.getTitle(), widget);
	component.setIndex(index);
}

void Sidebar::addComponents (const std::string &title, SidebarComponent &component1, SidebarComponent &component2)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

	GtkWidget *widget1 = component1.getWidget();
	GtkWidget *widget2 = component2.getWidget();

	gtk_container_add(GTK_CONTAINER(vbox), widget1);
	gtk_container_add(GTK_CONTAINER(vbox), widget2);

	int index = addWidget(title, vbox);
	component1.setIndex(index);
	component2.setIndex(index);
}

void Sidebar::toggleSidebar ()
{
	bool visible = widget_toggle_visible(_widget);
	_entityInspector.toggleSidebar(visible, _currentPageIndex);
	_entityList->toggleSidebar(visible, _currentPageIndex);
	_textureBrowser.toggleSidebar(visible, _currentPageIndex);
	_surfaceInspector.toggleSidebar(visible, _currentPageIndex);
	_prefabSelector.toggleSidebar(visible, _currentPageIndex);
	_mapInfo.toggleSidebar(visible, _currentPageIndex);
	_jobInfo.toggleSidebar(visible, _currentPageIndex);
}

void Sidebar::showComponent (SidebarComponent &component)
{
	if (!widget_is_visible(GTK_WIDGET(_widget)))
		widget_set_visible(GTK_WIDGET(_widget), TRUE);
	component.show(_notebook);
}

void Sidebar::showTextureBrowser ()
{
	showComponent(_textureBrowser);
}

void Sidebar::showPrefabs ()
{
	showComponent(_prefabSelector);
}

void Sidebar::showSurfaceInspector ()
{
	showComponent(_surfaceInspector);
}

void Sidebar::showEntityInspector ()
{
	showComponent(_entityInspector);
}

gboolean Sidebar::onChangePage (GtkNotebook *notebook, gpointer newPage, guint newPageIndex, Sidebar *self)
{
	self->_entityInspector.switchPage(newPageIndex);
	self->_entityList->switchPage(newPageIndex);
	self->_textureBrowser.switchPage(newPageIndex);
	self->_surfaceInspector.switchPage(newPageIndex);
	self->_prefabSelector.switchPage(newPageIndex);
	self->_mapInfo.switchPage(newPageIndex);
	self->_jobInfo.switchPage(newPageIndex);

	self->_currentPageIndex = newPageIndex;

	return FALSE;
}

GtkWidget* Sidebar::getWidget ()
{
	return _widget;
}
}
