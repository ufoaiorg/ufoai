/**
 * @file findbrush.cpp
 * @brief Creates the findbrush widget and select the brush (if found)
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

#include "findbrush.h"
#include "radiant_i18n.h"
#include "iradiant.h"
#include "ieventmanager.h"

#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/LeftAlignment.h"
#include "string/string.h"

#include "../../map/map.h"

namespace ui {

namespace {
const int FINDDLG_DEFAULT_SIZE_X = 350;
const int FINDDLG_DEFAULT_SIZE_Y = 100;
}

FindBrushDialog::FindBrushDialog () :
	gtkutil::PersistentTransientWindow(_("Find brush"), GlobalRadiant().getMainWindow())
{
	gtk_window_set_default_size(GTK_WINDOW(getWindow()), FINDDLG_DEFAULT_SIZE_X, FINDDLG_DEFAULT_SIZE_Y);
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
	gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);

	// Create all the widgets
	populateWindow();

	// Propagate shortcuts to the main window
	GlobalEventManager().connectDialogWindow(GTK_WINDOW(getWindow()));

	// Show the window and its children
	show();
}

FindBrushDialog::~FindBrushDialog ()
{
	// Propagate shortcuts to the main window
	GlobalEventManager().disconnectDialogWindow(GTK_WINDOW(getWindow()));
}

void FindBrushDialog::populateWindow() {
	GtkWidget* dialogVBox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(getWindow()), dialogVBox);

	GtkWidget* findHBox = gtk_hbox_new(FALSE, 0);
	GtkWidget* replaceHBox = gtk_hbox_new(FALSE, 0);

	// Pack these hboxes into an alignment so that they are indented
	GtkWidget* alignment = gtkutil::LeftAlignment(GTK_WIDGET(findHBox), 18, 1.0);
	GtkWidget* alignment2 = gtkutil::LeftAlignment(GTK_WIDGET(replaceHBox), 18, 1.0);

	gtk_box_pack_start(GTK_BOX(dialogVBox), GTK_WIDGET(alignment), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(dialogVBox), GTK_WIDGET(alignment2), TRUE, TRUE, 0);

	// Create the labels and pack them in the hbox
	GtkWidget* entityLabel = gtkutil::LeftAlignedLabel(_("Entity number"));
	GtkWidget* brushLabel = gtkutil::LeftAlignedLabel(_("Brush number"));
	gtk_widget_set_size_request(entityLabel, 140, -1);
	gtk_widget_set_size_request(brushLabel, 140, -1);

	gtk_box_pack_start(GTK_BOX(findHBox), entityLabel, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(replaceHBox), brushLabel, FALSE, FALSE, 0);

	_entityEntry = gtk_entry_new();
	_brushEntry = gtk_entry_new();

	gtk_box_pack_start(GTK_BOX(findHBox), _entityEntry, TRUE, TRUE, 6);
	gtk_box_pack_start(GTK_BOX(replaceHBox), _brushEntry, TRUE, TRUE, 6);

	// Finally, add the buttons
	gtk_box_pack_start(GTK_BOX(dialogVBox), createButtons(), FALSE, FALSE, 0);
}

GtkWidget* FindBrushDialog::createButtons() {
	GtkWidget* hbox = gtk_hbox_new(FALSE, 6);

	GtkWidget* findButton = gtk_button_new_from_stock(GTK_STOCK_FIND);
	GtkWidget* closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);

	g_signal_connect(G_OBJECT(findButton), "clicked", G_CALLBACK(onFind), this);
	g_signal_connect(G_OBJECT(closeButton), "clicked", G_CALLBACK(onClose), this);

	gtk_box_pack_end(GTK_BOX(hbox), closeButton, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), findButton, FALSE, FALSE, 0);

	return hbox;
}

void FindBrushDialog::onFind(GtkWidget* widget, FindBrushDialog* self) {
	int entityNum = string::toInt(gtk_entry_get_text(GTK_ENTRY(self->_entityEntry)));
	int brushNum = string::toInt(gtk_entry_get_text(GTK_ENTRY(self->_brushEntry)));
	GlobalMap().SelectBrush(entityNum, brushNum, true);
}

void FindBrushDialog::onClose(GtkWidget* widget, FindBrushDialog* self) {
	// Call the DialogWindow::destroy method and remove self from heap
	self->destroy();
}

}
