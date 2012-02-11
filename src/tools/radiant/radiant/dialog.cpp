/**
 * @file dialog.cpp
 * @brief Base dialog class, provides a way to run modal dialogs and
 * set/get the widget values in member variables.
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

#include "dialog.h"

#include "debugging/debugging.h"

#include "ui/mainframe/mainframe.h"
#include "ui/common/ShaderChooser.h"
#include "gtkutil/IconTextButton.h"

#include <stdlib.h>

#include <gtk/gtk.h>

#include "stream/stringstream.h"
#include "gtkutil/dialog.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/image.h"
#include "gtkutil/filechooser.h"
#include "textureentry.h"

#include "os/path.h"

GtkEntry* DialogEntry_new ()
{
	GtkEntry* entry = GTK_ENTRY(gtk_entry_new());
	gtk_widget_show(GTK_WIDGET(entry));
	widget_set_size(GTK_WIDGET(entry), 200, 0);
	return entry;
}

class DialogEntryRow
{
	public:
		DialogEntryRow (GtkWidget* row, GtkEntry* entry) :
			m_row(row), m_entry(entry)
		{
		}
		GtkWidget* m_row;
		GtkEntry* m_entry;
};

DialogEntryRow DialogEntryRow_new (const std::string& name)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	GtkEntry* entry = DialogEntry_new();
	gtk_container_add(GTK_CONTAINER(alignment), GTK_WIDGET(entry));

	return DialogEntryRow(GTK_WIDGET(DialogRow_new(name, alignment)), entry);
}

GtkSpinButton* DialogSpinner_new (double value, double lower, double upper, int fraction)
{
	double step = 1.0 / double(fraction);
	unsigned int digits = 0;
	for (; fraction > 1; fraction /= 10) {
		++digits;
	}
	GtkSpinButton
			* spin =
					GTK_SPIN_BUTTON(gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(value, lower, upper, step, 10, 0)), step, digits));
	gtk_widget_show(GTK_WIDGET(spin));
	widget_set_size(GTK_WIDGET(spin), 64, 0);
	return spin;
}

class DialogSpinnerRow
{
	public:
		DialogSpinnerRow (GtkWidget* row, GtkSpinButton* spin) :
			m_row(row), m_spin(spin)
		{
		}
		GtkWidget* m_row;
		GtkSpinButton* m_spin;
};

DialogSpinnerRow DialogSpinnerRow_new (const char* name, double value, double lower, double upper, int fraction)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	GtkSpinButton* spin = DialogSpinner_new(value, lower, upper, fraction);
	gtk_container_add(GTK_CONTAINER(alignment), GTK_WIDGET(spin));

	return DialogSpinnerRow(GTK_WIDGET(DialogRow_new(name, alignment)), spin);
}

Dialog::Dialog () :
	m_window(0), m_parent(0)
{
}

Dialog::~Dialog ()
{
	ASSERT_MESSAGE(m_window == 0, "dialog window not destroyed");
}

void Dialog::ShowDlg ()
{
	ASSERT_MESSAGE(m_window != 0, "dialog was not constructed");
	gtk_widget_show(GTK_WIDGET(m_window));
}

void Dialog::HideDlg ()
{
	ASSERT_MESSAGE(m_window != 0, "dialog was not constructed");
	gtk_widget_hide(GTK_WIDGET(m_window));
}

static gint delete_event_callback (GtkWidget *widget, GdkEvent* event, gpointer data)
{
	reinterpret_cast<Dialog*> (data)->HideDlg();
	reinterpret_cast<Dialog*> (data)->EndModal(eIDCANCEL);
	return TRUE;
}

void Dialog::Create ()
{
	ASSERT_MESSAGE(m_window == 0, "dialog cannot be constructed");

	m_window = BuildDialog();
	g_signal_connect(G_OBJECT(m_window), "delete_event", G_CALLBACK(delete_event_callback), this);
}

void Dialog::Destroy ()
{
	ASSERT_MESSAGE(m_window != 0, "dialog cannot be destroyed");

	gtk_widget_destroy(GTK_WIDGET(m_window));
	m_window = 0;
}

void Dialog::EndModal (EMessageBoxReturn code)
{
	m_modal.loop = 0;
	m_modal.ret = code;
}

EMessageBoxReturn Dialog::DoModal ()
{
	// Import the values from the registry before showing the dialog
	_registryConnector.importValues();

	PreModal();

	EMessageBoxReturn ret = modal_dialog_show(m_window, m_modal);
	ASSERT_NOTNULL(m_window);
	if (ret == eIDOK) {
		_registryConnector.exportValues();
	}

	gtk_widget_hide(GTK_WIDGET(m_window));

	PostModal(m_modal.ret);

	return m_modal.ret;
}

/* greebo: This adds a checkbox to the given VBox and connects an XMLRegistry value to it.
 * The actual data is "imported" and "exported" via those templates.
 */
GtkWidget* Dialog::addCheckBox(GtkWidget* vbox, const std::string& name, const std::string& flag, const std::string& registryKey)
{
	// Create a new checkbox with the given caption and display it
	GtkWidget* check = gtk_check_button_new_with_label(flag.c_str());
	gtk_widget_show(check);

	// Connect the registry key to this toggle button
	_registryConnector.connectGtkObject(GTK_OBJECT(check), registryKey);

	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(DialogRow_new(name, check)));
	return check;
}

/* greebo: This adds a horizontal slider and connects it to the value of the given registryKey
 */
void Dialog::addSlider (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
		gboolean draw_value, double value, double lower, double upper,
		double step_increment, double page_increment, double page_size)
{
	// Create a new adjustment with the boundaries <lower> and <upper> and all the increments
	GtkObject* adj = gtk_adjustment_new(value, lower, upper, step_increment, page_increment, page_size);

	// Connect the registry key to this adjustment
	_registryConnector.connectGtkObject(adj, registryKey);

	// scale
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 1.0, 0.0);
	gtk_widget_show(alignment);

	GtkWidget* scale = gtk_hscale_new(GTK_ADJUSTMENT(adj));
	gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_LEFT);
	gtk_widget_show(scale);
	gtk_container_add(GTK_CONTAINER(alignment), scale);

	gtk_scale_set_draw_value(GTK_SCALE (scale), draw_value);
	int digits = (step_increment < 1.0f) ? 2 : 0;
	gtk_scale_set_digits(GTK_SCALE (scale), digits);

	GtkTable* row = DialogRow_new(name, alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

inline void button_clicked_entry_browse_file (GtkWidget* widget, GtkEntry* entry)
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
		gtk_entry_set_text(entry, file.c_str());
	}
}

inline void button_clicked_entry_browse_directory (GtkWidget* widget, GtkEntry* entry)
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
		gtk_entry_set_text(entry, filename.c_str());
	}
}

GtkWidget* Dialog::addSpinner (GtkWidget* vbox, const std::string& name, const std::string& registryKey, double lower,
		double upper, int fraction)
{
	// Load the initial value (maybe unnecessary, as the value is loaded upon dialog show)
	float value = GlobalRegistry().getFloat(registryKey);

	// Create a new row containing an input field
	DialogSpinnerRow row(DialogSpinnerRow_new(name.c_str(), value, lower, upper, fraction));

	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(row.m_spin), registryKey);

	DialogVBox_packRow(GTK_VBOX(vbox), row.m_row);
	return row.m_row;
}

GtkWidget* Dialog::addTextureEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey)
{
	GtkWidget* hbox = gtk_hbox_new(false, 0);

	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(alignment), hbox);

	GtkEntry* entry = DialogEntry_new();
	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(entry), registryKey);
	GlobalTextureEntryCompletion::instance().connect(entry);

	// Create the icon button to open the ShaderChooser
	GtkWidget* selectShaderButton = gtkutil::IconTextButton("", gtkutil::getLocalPixbuf("folder16.png"), false);
	// Override the size request
	gtk_widget_set_size_request(selectShaderButton, -1, -1);
	g_signal_connect(G_OBJECT(selectShaderButton), "clicked", G_CALLBACK(onTextureSelect), entry);

	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(entry), true, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), selectShaderButton, false, false, 0);

	gtk_widget_show_all(alignment);

	DialogEntryRow row(DialogEntryRow(GTK_WIDGET(DialogRow_new(name, alignment)), entry));
	DialogVBox_packRow(GTK_VBOX(vbox), row.m_row);
	return row.m_row;

}

void Dialog::onTextureSelect (GtkWidget* button, GtkEntry* entry)
{
	// Construct the modal dialog, self-destructs on close
	new ui::ShaderChooser(NULL, GlobalRadiant().getMainWindow(), GTK_WIDGET(entry), true);
}

// greebo: Adds a PathEntry to choose files or directories (depending on the given boolean)
GtkWidget* Dialog::addPathEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
		bool browseDirectories)
{
	PathEntry pathEntry = PathEntry_new();
	g_signal_connect(G_OBJECT(pathEntry.m_button), "clicked", G_CALLBACK(browseDirectories ? button_clicked_entry_browse_directory : button_clicked_entry_browse_file), pathEntry.m_entry);

	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(pathEntry.m_entry), registryKey);

	GtkTable* row = DialogRow_new(name, GTK_WIDGET(pathEntry.m_frame));
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));

	return GTK_WIDGET(row);
}

void Dialog::addCombo (GtkWidget* vbox, const std::string& name, const std::string& registryKey,
		const ComboBoxValueList& valueList)
{
	GtkWidget* alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	gtk_widget_show(alignment);

	{
		// Create a new combo box
		GtkWidget* combo = gtk_combo_box_new_text();

		// Add all the string values to the combo box
		for (ComboBoxValueList::const_iterator i = valueList.begin(); i != valueList.end(); ++i) {
			// Add the current string value to the combo box
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo), i->c_str());
		}

		// Connect the registry key to the newly created combo box
		_registryConnector.connectGtkObject(GTK_OBJECT(combo), registryKey);

		// Add it to the container and make it visible
		gtk_widget_show(combo);
		gtk_container_add(GTK_CONTAINER(alignment), combo);
	}

	// Add the widget to the dialog row
	GtkTable* row = DialogRow_new(name, alignment);
	DialogVBox_packRow(GTK_VBOX(vbox), GTK_WIDGET(row));
}

// greebo: add an entry box connected to the given registryKey
GtkWidget* Dialog::addEntry (GtkWidget* vbox, const std::string& name, const std::string& registryKey)
{
	// Create a new row containing an input field
	DialogEntryRow row(DialogEntryRow_new(name));

	// Connect the registry key to the newly created input field
	_registryConnector.connectGtkObject(GTK_OBJECT(row.m_entry), registryKey);

	DialogVBox_packRow(GTK_VBOX(vbox), row.m_row);
	return row.m_row;
}
