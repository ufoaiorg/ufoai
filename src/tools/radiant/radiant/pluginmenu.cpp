/**
 * @file pluginmenu.cpp
 * @brief Generates the plugin menu entries
 */

/*
 Copyright (C) 1999-2006 Id Software, Inc. and contributors.
 For a list of contributors, see the accompanying CONTRIBUTORS file.

 This file is part of GtkRadiant.

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

#include "pluginmenu.h"
#include "radiant_i18n.h"

#include "stream/textstream.h"

#include "gtkutil/pointer.h"
#include "gtkutil/menu.h"

#include "plugin/PluginManager.h"
#include "mainframe.h"
#include "settings/preferences.h"

static int m_nNextPlugInID = 0;

static void plugin_activated (GtkWidget* widget, gpointer data)
{
	const char* str = (const char*) g_object_get_data(G_OBJECT(widget), "command");
	GetPlugInMgr().Dispatch(gpointer_to_int(data), str);
}

#include <stack>
typedef std::stack<GtkWidget*> WidgetStack;

static void PlugInMenu_Add (GtkMenu* plugin_menu, IPlugin* pPlugIn)
{
	GtkWidget *item, *parent, *subMenu;
	WidgetStack menuStack;

	parent = gtk_menu_item_new_with_label(pPlugIn->getMenuName());
	gtk_widget_show(parent);
	gtk_container_add(GTK_CONTAINER (plugin_menu), parent);

	std::size_t nCount = pPlugIn->getCommandCount();
	if (nCount > 0) {
		GtkWidget *menu = gtk_menu_new();
		while (nCount > 0) {
			const char *menuText = pPlugIn->getCommandTitle(--nCount);
			const char *menuCommand = pPlugIn->getCommand(nCount);

			if (menuText != 0 && strlen(menuText) > 0) {
				if (!strcmp(menuText, "-")) {
					item = gtk_menu_item_new();
					gtk_widget_set_sensitive(item, FALSE);
				} else if (!strcmp(menuText, ">")) {
					menuText = pPlugIn->getCommandTitle(--nCount);
					menuCommand = pPlugIn->getCommand(nCount);
					if (!strcmp(menuText, "-") || !strcmp(menuText, ">") || !strcmp(menuText, "<")) {
						globalErrorStream() << pPlugIn->getMenuName() << " Invalid title (" << menuText
								<< ") for submenu.\n";
						continue;
					}

					item = gtk_menu_item_new_with_label(menuText);
					gtk_widget_show(item);
					gtk_container_add(GTK_CONTAINER (menu), item);

					subMenu = gtk_menu_new();
					gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), subMenu);
					menuStack.push(menu);
					menu = subMenu;
					continue;
				} else if (!strcmp(menuText, "<")) {
					if (!menuStack.empty()) {
						menu = menuStack.top();
						menuStack.pop();
					} else {
						globalErrorStream() << pPlugIn->getMenuName()
								<< ": Attempt to end non-existent submenu ignored.\n";
					}
					continue;
				} else {
					item = gtk_menu_item_new_with_label(menuText);
					g_object_set_data(G_OBJECT(item), "command",
							const_cast<gpointer> (static_cast<const void*> (menuCommand)));
					g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(plugin_activated), gint_to_pointer(m_nNextPlugInID));
				}
				gtk_widget_show(item);
				gtk_container_add(GTK_CONTAINER (menu), item);
				pPlugIn->addMenuID(m_nNextPlugInID++);
			}
		}
		if (!menuStack.empty()) {
			std::size_t size = menuStack.size();
			if (size != 0) {
				globalErrorStream() << pPlugIn->getMenuName() << " mismatched > <. " << string::toString(size)
						<< " submenu(s) not closed.\n";
			}
			for (std::size_t i = 0; i < (size - 1); i++) {
				menuStack.pop();
			}
			menu = menuStack.top();
			menuStack.pop();
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM (parent), menu);
	}
}

static GtkMenu* g_plugins_menu = 0;

static void PluginsMenu_populate ()
{
	class PluginsMenuConstructor: public PluginsVisitor
	{
			GtkMenu* m_menu;
		public:
			PluginsMenuConstructor (GtkMenu* menu) :
				m_menu(menu)
			{
			}
			void visit (IPlugin& plugin)
			{
				PlugInMenu_Add(m_menu, &plugin);
			}
	};

	PluginsMenuConstructor constructor(g_plugins_menu);
	GetPlugInMgr().constructMenu(constructor);
}

GtkMenuItem* create_plugins_menu ()
{
	// Plugins menu
	GtkMenuItem* plugins_menu_item = new_sub_menu_item_with_mnemonic(_("_Plugins"));
	GtkMenu* menu = GTK_MENU(gtk_menu_item_get_submenu(plugins_menu_item));

	g_plugins_menu = menu;

	PluginsMenu_populate();

	return plugins_menu_item;
}
