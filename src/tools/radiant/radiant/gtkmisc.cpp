/**
 * @file gtkmisc.cpp
 * @brief Small functions to help with GTK
 */

/*
 Copyright (c) 2001, Loki software, inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list
 of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 Neither the name of Loki software nor the names of its contributors may be used
 to endorse or promote products derived from this software without specific prior
 written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "gtkmisc.h"
#include "radiant_i18n.h"
#include "ieventmanager.h"

#include "os/path.h"

#include <gtk/gtkmenuitem.h>
#include <gtk/gtkmenu.h>

#include "gtkutil/dialog.h"
#include "gtkutil/image.h"
#include "gtkutil/menu.h"
#include "gtkutil/MenuItemAccelerator.h"
#include "gtkutil/SeparatorMenuItem.h"

// =============================================================================
// Misc stuff

/* greebo: Create a menu item under the given <menu> and connect it to the given <command> name
 */
GtkMenuItem* createMenuItemWithMnemonic (GtkMenu* menu, const std::string& caption, const std::string& commandName, const std::string& iconName)
{
	GtkWidget* menuItem = NULL;

	IEvent* event = GlobalEventManager().findEvent(commandName);

	if (event != NULL) {
		// Retrieve an acclerator string formatted for a menu
		const std::string accelText = GlobalEventManager().getAcceleratorStr(event, true);

		// Create a new menuitem
		menuItem = gtkutil::TextMenuItemAccelerator(caption, accelText, iconName, false);

		gtk_widget_show_all(GTK_WIDGET(menuItem));

		// Add the menu item to the container
		gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(menuItem));

		event->connectWidget(GTK_WIDGET(menuItem));
	} else {
		globalErrorStream() << "gtkutil::createMenuItem failed to lookup command " << commandName << "\n";
	}

	return GTK_MENU_ITEM(menuItem);
}

/* greebo: Create a check menuitem under the given <menu> and connect it to the given <command> name
 */
GtkMenuItem* createCheckMenuItemWithMnemonic (GtkMenu* menu, const std::string& caption, const std::string& commandName, const std::string& iconName)
{
	GtkWidget* menuItem = NULL;

	IEvent* event = GlobalEventManager().findEvent(commandName);

	if (event != NULL) {
		// Retrieve an acclerator string formatted for a menu
		const std::string accelText = GlobalEventManager().getAcceleratorStr(event, true);

		menuItem = gtkutil::TextMenuItemAccelerator(caption, accelText, iconName, true);

		gtk_widget_show_all(GTK_WIDGET(menuItem));

		// Add the menu item to the container
		gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(menuItem));

		event->connectWidget(GTK_WIDGET(menuItem));
	} else {
		globalErrorStream() << "gtkutil::createMenuItem failed to lookup command " << commandName << "\n";
	}

	return GTK_MENU_ITEM(menuItem);
}

/* Adds a separator to the given menu
 */
GtkMenuItem* createSeparatorMenuItem (GtkMenu* menu)
{
	GtkWidget* separator = gtkutil::SeparatorMenuItem();
	gtk_widget_show(separator);

	// Add the menu item to the container
	gtk_container_add(GTK_CONTAINER(menu), GTK_WIDGET(separator));

	return GTK_MENU_ITEM(separator);
}
