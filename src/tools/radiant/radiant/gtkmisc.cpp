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
#include "gtkutil/filechooser.h"
#include "gtkutil/image.h"
#include "gtkutil/menu.h"
#include "gtkutil/MenuItemAccelerator.h"
#include "gtkutil/SeparatorMenuItem.h"
#include "gtkutil/IConv.h"

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

// =============================================================================
// File dialog

bool color_dialog (GtkWidget *parent, Vector3& color, const std::string& title)
{
	GtkWidget* dlg;
	double clr[3];
	ModalDialog dialog;

	clr[0] = color[0];
	clr[1] = color[1];
	clr[2] = color[2];

	dlg = gtk_color_selection_dialog_new(title.c_str());
	gtk_color_selection_set_color(GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (dlg)->colorsel), clr);
	g_signal_connect(G_OBJECT(dlg), "delete_event", G_CALLBACK(dialog_delete_callback), &dialog);
	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(dlg)->ok_button), "clicked", G_CALLBACK(dialog_button_ok), &dialog);
	g_signal_connect(G_OBJECT(GTK_COLOR_SELECTION_DIALOG(dlg)->cancel_button), "clicked", G_CALLBACK(dialog_button_cancel), &dialog);

	if (parent != 0)
		gtk_window_set_transient_for(GTK_WINDOW (dlg), GTK_WINDOW (parent));

	bool ok = modal_dialog_show(GTK_WINDOW(dlg), dialog) == eIDOK;
	if (ok) {
		GdkColor gdkcolor;
		gtk_color_selection_get_current_color(GTK_COLOR_SELECTION (GTK_COLOR_SELECTION_DIALOG (dlg)->colorsel),
				&gdkcolor);
		clr[0] = gdkcolor.red / 65535.0;
		clr[1] = gdkcolor.green / 65535.0;
		clr[2] = gdkcolor.blue / 65535.0;

		color[0] = (float) clr[0];
		color[1] = (float) clr[1];
		color[2] = (float) clr[2];
	}

	gtk_widget_destroy(dlg);

	return ok;
}

void button_clicked_entry_browse_file (GtkWidget* widget, GtkEntry* entry)
{
	std::string filename = gtk_entry_get_text(entry);

	gtkutil::FileChooser fileChooser(gtk_widget_get_toplevel(widget), _("Choose File"), true, false);
	if (!filename.empty()) {
		fileChooser.setCurrentPath(os::stripFilename(filename));
		fileChooser.setCurrentFile(filename);
	}

	std::string file = fileChooser.display();

	if (GTK_IS_WINDOW(gtk_widget_get_toplevel(widget))) {
		gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	}

	if (!file.empty()) {
		file = gtkutil::IConv::filenameToUTF8(file);
		gtk_entry_set_text(entry, file.c_str());
	}
}

void button_clicked_entry_browse_directory (GtkWidget* widget, GtkEntry* entry)
{
	gtkutil::FileChooser dirChooser(gtk_widget_get_toplevel(widget), _("Choose Directory"), true, true);
	std::string curEntry = gtk_entry_get_text(entry);

	if (g_path_is_absolute(curEntry.c_str()))
		curEntry.clear();
	dirChooser.setCurrentPath(curEntry);

	std::string filename = dirChooser.display();

	if (GTK_IS_WINDOW(gtk_widget_get_toplevel(widget))) {
		gtk_window_present(GTK_WINDOW(gtk_widget_get_toplevel(widget)));
	}

	if (!filename.empty()) {
		filename = gtkutil::IConv::filenameToUTF8(filename);
		gtk_entry_set_text(entry, filename.c_str());
	}
}
